#include "InspBottleNeck.h"
#include "InspBottleNeckStruct.h"
#include "ModelManager.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <locale>
#include "InferenceWorker.h"
#include "Data.h"
#include "AnalyseMat.h"
#include <atomic>
#include <shared_mutex>
#include <numeric>

#include <json.hpp>
using json = nlohmann::json;

// 静态成员初始化
std::shared_mutex InspBottleNeck::modelLoadMutex;
std::map<std::string, std::string> InspBottleNeck::bottleNeckDetectionModelMap;
std::map<std::string, std::string> InspBottleNeck::bottleNeckClassifyModelMap;
std::map<int, InspBottleNeckIn> InspBottleNeck::cameraConfigMap;

// 静态统计变量初始化
std::mutex InspBottleNeck::m_statsMutex;
int InspBottleNeck::m_totalFittedCount = 0;
double InspBottleNeck::m_sumEllipticity = 0.0;
double InspBottleNeck::m_sumIrregularity = 0.0;
double InspBottleNeck::m_sumDiameter = 0.0;
bool InspBottleNeck::m_statsInitialized = false;

// 构造函数
InspBottleNeck::InspBottleNeck(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
	bool isLoadConfig, int timeOut, InspBottleNeckOut& outInfo)
	: LOG(std::make_unique<Log>()),
	ANA(std::make_unique<AnalyseMat>()),
	COM(std::make_unique<Common>()),
	TXT(std::make_unique<TxtOperater>()),
	DAS(std::make_unique<DrawAndShowImg>())
{
	m_safeTimeoutFlag = std::make_shared<std::atomic<bool>>(false);
	m_params.timeOut = timeOut;
	m_timeoutFlagRef = &outInfo.system.timeoutFlag;

	outInfo.status.errorMessage = "OK";
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.status.statusCode = BOTTLENECK_RETURN_OK;
	outInfo.status.logs.reserve(100);

	// 只在jobId==1时初始化统计变量
	if (jobId == 1) {
		InitializeStatistics(jobId);
	}

	m_img = img.clone();
	cv::cvtColor(img, outInfo.images.outputImg, cv::COLOR_GRAY2BGR);

	bool shouldLoadConfig = isLoadConfig ||
		jobId == 0 ||
		cameraConfigMap.find(cameraId) == cameraConfigMap.end();

	// 获取config
	if (shouldLoadConfig)
	{
		bool rv_loadConfig = readParams(m_img, outInfo.paths.configFile, m_params, outInfo, outInfo.paths.logFile);
		if (!rv_loadConfig) {
			outInfo.status.statusCode = BOTTLENECK_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "获取算法config失败!";
			Log::WriteAsyncLog(outInfo.status.errorMessage, ERR, outInfo.paths.logFile, true);
			return;
		}
		else
		{
			Log::WriteAsyncLog("获取config成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}

		// 检查roi
		if (!ANA->JudgeRectIn(cv::Rect(0, 0, img.cols, img.rows), m_params.roiRect)) {
			outInfo.status.statusCode = BOTTLENECK_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "roi区域超出图像范围!";
			Log::WriteAsyncLog(outInfo.status.errorMessage, ERR, outInfo.paths.logFile, true);
			return;
		}

		if (m_params.isClassfy)
		{
			m_params.neckClassifyClassName = { "NG", "OK" };
			bool loadModel = loadAllModels(outInfo, true);
			if (!loadModel) {
				outInfo.status.statusCode = BOTTLENECK_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "深度学习模型加载异常!";
				Log::WriteAsyncLog("---深度学习模型加载异常!", ERR, outInfo.paths.logFile, true);
				return;
			}

			if (!validateCameraModels(outInfo.system.cameraId)) {
				outInfo.status.statusCode = BOTTLENECK_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "相机ID非法/模型文件缺失!";
				Log::WriteAsyncLog(outInfo.status.errorMessage, ERR, outInfo.paths.logFile, true);
				throw std::invalid_argument("相机ID非法/模型文件缺失!");
			}
		}

		if (m_params.isLocate)
		{

			//读取定位配置文件
			if (LoadConfigYOLO(
				m_params.locateThreshConfig,
				m_params.locatePara,
				m_params.locateClassName,
				outInfo.paths.logFile) != 1)
			{
				outInfo.status.statusCode = BOTTLENECK_RETURN_CONFIG_ERR;
				Log::WriteAsyncLog("缺陷检模型配置文件-参数设置错误！", ERR, outInfo.paths.logFile, true);
				outInfo.status.errorMessage = "缺陷检模型配置文件-参数设置错误!";
				return;
			}
			else
			{
				Log::WriteAsyncLog("缺陷检模型读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			}
		}
	}
	else
	{
		m_params = cameraConfigMap[cameraId];
	}

	if (outInfo.status.statusCode == BOTTLENECK_RETURN_OK)
	{
		Log::WriteAsyncLog(outInfo.status.errorMessage, INFO, outInfo.paths.logFile, true, "初始化成功!");
	}
	else
	{
		Log::WriteAsyncLog(outInfo.status.errorMessage, ERR, outInfo.paths.logFile, true, "初始化失败!");
	}

	if (m_params.saveDebugImage) {
		COM->CreateDir(outInfo.paths.intermediateImagesDir);
	}
	if (m_params.saveResultImage) {
		COM->CreateDir(outInfo.paths.resultsOKDir);
		COM->CreateDir(outInfo.paths.resultsNGDir);
	}
}
InspBottleNeck::~InspBottleNeck() {
}

// 初始化统计变量
void InspBottleNeck::InitializeStatistics(int jobId) {
	std::lock_guard<std::mutex> lock(m_statsMutex);

	// 只在jobId==1时初始化统计变量
	if (jobId == 1 && !m_statsInitialized) {
		m_totalFittedCount = 0;
		m_sumEllipticity = 0.0;
		m_sumIrregularity = 0.0;
		m_sumDiameter = 0.0;
		m_statsInitialized = true;

		LOG->WriteAsyncLog("统计变量已初始化 (jobId=" + std::to_string(jobId) + ")",
			INFO, "", m_params.saveLogTxt);
	}
}

// 验证相机ID对应的模型是否存在
bool InspBottleNeck::validateCameraModels(int cameraId) {
	std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
	return bottleNeckDetectionModelMap.count("bottleNeckDetection_" + std::to_string(cameraId)) &&
		bottleNeckClassifyModelMap.count("bottleNeckClassfy_" + std::to_string(cameraId));
}

// 加载所有模型到ModelManager
bool InspBottleNeck::loadAllModels(InspBottleNeckOut& outInfo, bool ini) {
	if (!ini) {
		Log::WriteAsyncLog(outInfo.status.errorMessage, WARNING, outInfo.paths.logFile, m_params.saveLogTxt, "模型加载跳过!");
		return true;
	}

	const int cameraId = outInfo.system.cameraId;

	// 获取当前专用模型路径
	std::vector<std::string> cameraModelPaths;


	// 添加分类模型
	std::string classfyKey = "bottleNeckClassfy_" + std::to_string(cameraId);
	if (auto it = bottleNeckClassifyModelMap.find(classfyKey); it != bottleNeckClassifyModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	// 添加定位模型
	std::string detectionKey = "bottleNeckDetection_" + std::to_string(cameraId);
	if (auto it = bottleNeckDetectionModelMap.find(detectionKey); it != bottleNeckDetectionModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}
	if (cameraModelPaths.empty()) {
		Log::WriteAsyncLog("相机" + std::to_string(cameraId) + "未找到有效模型路径!", ERR, outInfo.paths.logFile, true);
		return false;
	}

	try {
		ModelManager& mgr = ModelManager::Instance(cameraId);

		for (const auto& modelPath : cameraModelPaths) {
			// 避免重复加载
			if (!mgr.IsModelLoaded(modelPath)) {
				mgr.LoadModel(modelPath, m_params.hardwareType);
				Log::WriteAsyncLog("相机" + std::to_string(cameraId) + "加载模型: " + modelPath, INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			}
		}

		//// 预热模型
		//cv::Mat iniImg = cv::Mat::zeros(cv::Size(500, 500), CV_8UC3);
		//std::vector<FinsObject> dummyResults = InferenceWorker::Run(
		//	cameraId, m_params.locateWeightsFile, m_params.locateClassName, iniImg, 0.1, 0.5);
		//outInfo.classification.neckType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.neckClassifyWeightsFile, m_params.neckClassifyClassName, iniImg);

		//Log::WriteAsyncLog(outInfo.status.errorMessage, INFO, outInfo.paths.logFile, true, "模型加载初始化成功");
		return true;
	}
	catch (const std::exception& e) {
		Log::WriteAsyncLog("相机" + std::to_string(cameraId) + "模型加载异常: " + std::string(e.what()), ERR, outInfo.paths.logFile, true);
		return false;
	}
}

bool InspBottleNeck::readParams(cv::Mat img, const std::string& filePath, InspBottleNeckIn& params,
	InspBottleNeckOut& outInfo, const std::string& fileName) {
	std::ifstream ifs(filePath.c_str());
	if (!ifs.is_open()) {
		outInfo.status.errorMessage = "config文件打开失败!";
		Log::WriteAsyncLog("config文件打开失败", WARNING, outInfo.paths.logFile, true);
		return false;
	}

	std::string line;
	while (!ifs.eof()) {
		getline(ifs, line);
		size_t findPos = line.find("##");

		if (findPos != std::string::npos || line.empty()) {
			continue;
		}

		int findCommon = line.find_first_of(":");
		std::string keyWord;
		if (findCommon > 0) {
			keyWord = line.substr(0, findCommon);
		}
		else {
			outInfo.status.errorMessage = "分隔符缺失!";
			Log::WriteAsyncLog(keyWord, WARNING, outInfo.paths.logFile, true, " 分隔符缺失");
			return false;
		}

		std::string cutName, value;
		std::string tmp = line.substr(findCommon + 1);
		int stringSize = tmp.size();
		if (stringSize > 1) {
			cutName = tmp.substr(stringSize - 1, stringSize - 1);
			if (cutName == "\r") {
				value = tmp.substr(0, stringSize - 1);
			}
			else {
				value = tmp;
			}
		}
		else {
			value = tmp;
		}

		value.erase(0, value.find_first_not_of(" "));  // 去除前导空格
		value.erase(value.find_last_not_of(" ") + 1);
		value.erase(0, value.find_first_not_of(" "));
		value.erase(value.find_last_not_of(" ") + 1);

		std::string keyStr(value.begin(), value.end());

		// 是否存储中间图片(0:不存储 1:存储)
		if (keyWord == "BOTTLENECK_SAVE_DEBUG_IMAGE") {
			params.saveDebugImage = std::stoi(value);
		}
		else if (keyWord == "BOTTLENECK_SAVE_RESULT_IMAGE") {
			params.saveResultImage = std::stoi(value);
		}
		else if (keyWord == "BOTTLENECK_SAVE_LOG_TXT") {
			params.saveLogTxt = std::stoi(value);
		}
		else if (keyWord == "BOTTLENECK_DRAW_RESULT") {
			params.drawResult = std::stoi(value);
		}
		else if (keyWord == "BOTTLENECK_SAVE_TRAIN") {
			params.saveTrain = std::stoi(value);
		}
		else if (keyWord == "BOTTLENECK_TIMEOUT") {
			params.timeOut = std::stoi(value);
		}
		else if (keyWord == "BOTTLENECK_ROI_X") {
			params.roiRect.x = std::stoi(value);
			if (params.roiRect.x < 0 || params.roiRect.x > img.cols)
			{
				outInfo.status.errorMessage = "ROI_X: 超出图像范围!";
				Log::WriteAsyncLog("ROI_X: 超出图像范围", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_ROI_Y") {
			params.roiRect.y = std::stoi(value);
			if (params.roiRect.y < 0 || params.roiRect.y > img.rows)
			{
				outInfo.status.errorMessage = "ROI_Y: 超出图像范围!";
				Log::WriteAsyncLog("ROI_Y: 超出图像范围", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_ROI_W") {
			params.roiRect.width = std::stoi(value);
			if (params.roiRect.x + params.roiRect.width > img.cols)
			{
				outInfo.status.errorMessage = "ROI_X+ROI_W: 超出图像范围!";
				Log::WriteAsyncLog("ROI_X+ROI_W: 超出图像范围", ERR, outInfo.paths.logFile, true);
				return false;
			}
			else if (params.roiRect.width < 10)
			{
				outInfo.status.errorMessage = "ROI_W < 10!";
				Log::WriteAsyncLog("ROI_W < 10!", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_ROI_H") {
			params.roiRect.height = std::stoi(value);
			if (params.roiRect.y + params.roiRect.height > img.rows)
			{
				outInfo.status.errorMessage = "ROI_Y+ROI_H: 超出图像范围!";
				Log::WriteAsyncLog("ROI_Y+ROI_H: 超出图像范围", ERR, outInfo.paths.logFile, true);
				return false;
			}
			else if (params.roiRect.height < 10)
			{
				outInfo.status.errorMessage = "ROI_H < 10!";
				Log::WriteAsyncLog("ROI_H < 10!", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_OUT_FILTER_THRESH") {
			params.filterThresh = std::stof(value);
			if (params.filterThresh > 10000 ||
				params.outCircleDiameter < 0)
			{
				outInfo.status.errorMessage = "BOTTLENECK_OUT_FILTER_THRESH 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_OUT_FILTER_THRESH 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_OUT_CIRCLE_BW_THRESH") {
			params.outCircleBwThresh = std::stoi(value);
			if (params.outCircleBwThresh > 255 || params.outCircleBwThresh < 0)
			{
				outInfo.status.errorMessage = "BOTTLENECK_OUT_CIRCLE_BW_THRESH 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_OUT_CIRCLE_BW_THRESH 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_OUT_CIRCLE_DIAMETER") {
			params.outCircleDiameter = std::stof(value);
			if (params.outCircleDiameter > img.cols ||
				params.outCircleDiameter > img.rows ||
				params.outCircleDiameter < 100)
			{
				outInfo.status.errorMessage = "BOTTLENECK_OUT_CIRCLE_DIAMETER 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_OUT_CIRCLE_DIAMETER 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_OUT_DIAMETER_RANGE") {
			params.outCircleDiameterRange = std::stof(value);
			if (params.outCircleDiameterRange > 100 || params.outCircleDiameterRange < 1)
			{
				outInfo.status.errorMessage = "BOTTLENECK_OUT_DIAMETER_RANGE 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_OUT_DIAMETER_RANGE 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_OUT_CIRCLE_DP") {
			params.outCircleDp = std::stof(value);
			if (params.outCircleDp > 20 || params.outCircleDp < 1)
			{
				outInfo.status.errorMessage = "BOTTLENECK_OUT_CIRCLE_DP 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_OUT_CIRCLE_DP 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_OUT_CIRCLE_EDGE_THRESH") {
			params.outCircleEdgeThresh = std::stoi(value);
			if (params.outCircleEdgeThresh > 300 || params.outCircleEdgeThresh < 1)
			{
				outInfo.status.errorMessage = "BOTTLENECK_OUT_CIRCLE_EDGE_THRESH 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_OUT_CIRCLE_EDGE_THRESH 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_NECK_EXT") {
			params.neckExt = std::stoi(value);
			if (params.neckExt > 1000 || params.neckExt < 0)
			{
				outInfo.status.errorMessage = "BOTTLENECK_NECK_EXT 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_NECK_EXT 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_OUT_FINE") {
			params.fineOut = std::stoi(value);
			if (params.fineOut < 0)
			{
				outInfo.status.errorMessage = "BOTTLENECK_OUT_FINE 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_OUT_FINE 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_OUT_CIRCLE_POINT_DEV_THRESH") {
			params.circlePointDevThresh = std::stoi(value);
			if (params.circlePointDevThresh > 1000 || params.circlePointDevThresh < 0)
			{
				outInfo.status.errorMessage = "BOTTLENECK_OUT_CIRCLE_POINT_DEV_THRESH 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_OUT_CIRCLE_POINT_DEV_THRESH 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_OUT_BREAK_THRESH") {
			params.outBreakThresh = std::stoi(value);
			if (params.outBreakThresh > 1000 || params.outBreakThresh < 0)
			{
				outInfo.status.errorMessage = "BOTTLENECK_OUT_BREAK_THRESH 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_OUT_BREAK_THRESH 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_OUT_NOT_CIRCLE_THRESH") {
			params.outNotCircleThresh = std::stof(value);
			if (params.outNotCircleThresh > 1000 || params.outNotCircleThresh < 0)
			{
				outInfo.status.errorMessage = "BOTTLENECK_OUT_NOT_CIRCLE_THRESH 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_OUT_NOT_CIRCLE_THRESH 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_OUT_IRREGULARITY_THRESH") {
			params.irregularity = std::stof(value);
			if (params.irregularity > 100 || params.irregularity < 0)
			{
				outInfo.status.errorMessage = "BOTTLENECK_OUT_IRREGULARITY_THRESH 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_OUT_NOT_CIRCLE_THRESH 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_HARDWARE_TYPE") {
			params.hardwareType = std::stoi(value);
			if (params.hardwareType > 1 || params.hardwareType < 0)
			{
				outInfo.status.errorMessage = "BOTTLENECK_HARDWARE_TYPE 范围错误!";
				Log::WriteAsyncLog("BOTTLENECK_HARDWARE_TYPE 范围错误", ERR, outInfo.paths.logFile, true);
				return false;
			}

		}
		else if (keyWord == "BOTTLENECK_MODEL_TYPE") {
			params.netType = std::stoi(value);
		}
		else if (keyWord == "BOTTLENECK_CLASSFY_TYPE") {
			params.isClassfy = std::stoi(value);
		}
		else if (keyWord == "BOTTLENECK_MODEL_CLASSFY_WEIGHTS_FLIE") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
			std::string camera = std::to_string(outInfo.system.cameraId);
			bottleNeckClassifyModelMap["bottleNeckClassfy_" + camera] = value;
			params.neckClassifyWeightsFile = value;
			if (!COM->FileExistsModern(params.neckClassifyWeightsFile))
			{
				outInfo.status.errorMessage = "分类模型文件缺失!";
				Log::WriteAsyncLog(params.neckClassifyWeightsFile, ERR, outInfo.paths.logFile, true, "--分类模型文件缺失");
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_DEFECT_TYPE") {
			params.isLocate = std::stoi(value);
		}
		else if (keyWord == "BOTTLENECK_MODEL_DEFECT_CONFIG_FLIE") {
			params.locateThreshConfig = value;
			if (!COM->FileExistsModern(params.locateThreshConfig))
			{
				outInfo.status.errorMessage = "缺陷检模型配置文件缺失!";
				Log::WriteAsyncLog(params.locateThreshConfig, ERR, outInfo.paths.logFile, true, "--缺陷检模型配置文件缺失");
				return false;
			}
		}
		else if (keyWord == "BOTTLENECK_MODEL_DEFECT_WEIGHTS_FLIE") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
			std::string camera = std::to_string(outInfo.system.cameraId);
			bottleNeckDetectionModelMap["bottleNeckDetection_" + camera] = value;
			params.locateWeightsFile = value;
			if (!COM->FileExistsModern(params.locateWeightsFile))
			{
				outInfo.status.errorMessage = "定位模型文件缺失!";
				Log::WriteAsyncLog(params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "--定位模型文件缺失");
				return false;
			}
		}
	}

	ifs.close();
	return true;
}

/**
 * 不规则度计算 - 实现椭圆拟合之后的不规则度
 * 通过计算每个边缘点到拟合椭圆的平均距离
 */
float calculateIrregularity(const std::vector<cv::Point>& edgePoints,
	const cv::RotatedRect& fittedEllipse) {
	if (edgePoints.empty()) {
		return 0.0f;
	}

	float totalDeviation = 0.0f;
	float maxDeviation = 0.0f;

	// 1. 获取椭圆参数
	cv::Point2f center = fittedEllipse.center;
	float a = fittedEllipse.size.width * 0.5f;  // 长半轴
	float b = fittedEllipse.size.height * 0.5f; // 短半轴
	float theta = fittedEllipse.angle * CV_PI / 180.0f; // 角度转换

	// 2. 计算每个点到拟合椭圆的距离
	for (const auto& point : edgePoints) {
		// 转换到椭圆坐标系
		float dx = point.x - center.x;
		float dy = point.y - center.y;

		// 旋转到椭圆轴对齐坐标系
		float rotatedX = dx * cos(theta) + dy * sin(theta);
		float rotatedY = -dx * sin(theta) + dy * cos(theta);

		// 计算点到椭圆的最短距离
		float angle = atan2(rotatedY, rotatedX);
		float ellipseX = a * cos(angle);
		float ellipseY = b * sin(angle);

		// 计算实际点到拟合椭圆上对应点的距离
		float distance = sqrt(pow(rotatedX - ellipseX, 2) +
			pow(rotatedY - ellipseY, 2));

		totalDeviation += distance;
		maxDeviation = std::max(maxDeviation, distance);
	}

	// 3. 计算平均偏差
	float avgDeviation = totalDeviation / edgePoints.size();

	return avgDeviation;
}

// 定位函数
void InspBottleNeck::BottleNeck_Locate(InspBottleNeckOut& outInfo) {
	if (outInfo.status.statusCode != BOTTLENECK_RETURN_OK) {
		return;
	}

	// 1. 提取ROI区域
	outInfo.images.roiImg = m_img(m_params.roiRect).clone();
	DAS->DAS_Rect(m_img, m_params.roiRect,
		outInfo.paths.intermediateImagesDir + "1.1_roiRect.jpg",
		m_params.saveDebugImage);

	Mat resizeImg;
	cv::resize(outInfo.images.roiImg, resizeImg, cv::Size(outInfo.images.roiImg.cols / 2, outInfo.images.roiImg.rows / 2));

	// 2. 霍夫圆检测
	std::vector<cv::Vec4f> circles;
	int minRadius = (m_params.outCircleDiameter - m_params.outCircleDiameterRange) * 0.25;
	int maxRadius = (m_params.outCircleDiameter + m_params.outCircleDiameterRange) * 0.25;
	float param2 = (m_params.outCircleDiameter * 0.1) / m_params.outCircleDp * 0.8;

	cv::HoughCircles(resizeImg, circles, cv::HOUGH_GRADIENT,
		m_params.outCircleDp,
		outInfo.images.roiImg.cols, // 最小圆间距
		m_params.outCircleEdgeThresh, // 圆边缘特征
		param2, // 累加器阈值
		minRadius, maxRadius);

	DAS->DAS_Circles(resizeImg, circles, outInfo.paths.intermediateImagesDir + "1.2.1_circles.jpg", m_params.saveDebugImage);

	if (circles.empty()) {
		outInfo.status.statusCode = BOTTLENECK_RETURN_LOCATE_OUT_CIRCLE_FAILED;
		outInfo.status.errorMessage = "支撑环定位失败!";
		LOG->WriteAsyncLog("支撑环定位失败", ERR, outInfo.paths.logFile, true);
		return;
	}
	else {
		outInfo.geometry.outCircle = circles[0] * 2;
		outInfo.geometry.outCenter.x = circles[0][0] * 2;
		outInfo.geometry.outCenter.y = circles[0][1] * 2;
	}

	// 5. 边界检查
	int bnd = 10;
	if (outInfo.geometry.outCircle[0] - outInfo.geometry.outCircle[2] <= bnd ||
		outInfo.geometry.outCircle[1] - outInfo.geometry.outCircle[2] < bnd ||
		outInfo.geometry.outCircle[0] + outInfo.geometry.outCircle[2] >= outInfo.images.roiImg.cols - bnd ||
		outInfo.geometry.outCircle[1] + outInfo.geometry.outCircle[2] >= outInfo.images.roiImg.rows - bnd) {
		outInfo.status.statusCode = BOTTLENECK_RETURN_CLOSE_BOUNDARY;
		outInfo.status.errorMessage = "靠近边界!";
		LOG->WriteAsyncLog("靠近边界!", ERR, outInfo.paths.logFile, true);
		return;
	}

	//精确定位
	if (m_params.fineOut)
	{
		// 6. 创建掩膜
		cv::Mat mask = cv::Mat::zeros(outInfo.images.roiImg.size(), CV_8UC1);
		cv::circle(mask, cv::Point2f(outInfo.geometry.outCircle[0], outInfo.geometry.outCircle[1]),
			outInfo.geometry.outCircle[2] + m_params.outCircleDiameterRange,
			cv::Scalar(255), -1, 8, 0);
		cv::circle(mask, cv::Point2f(outInfo.geometry.outCircle[0], outInfo.geometry.outCircle[1]),
			m_params.outCircleDiameter * 0.5 - m_params.outCircleDiameterRange,
			cv::Scalar(0), -1, 8, 0);

		cv::Mat imgBw;
		cv::threshold(outInfo.images.roiImg, imgBw, m_params.outCircleBwThresh, 255, cv::THRESH_BINARY);
		DAS->DAS_Img(imgBw, outInfo.paths.intermediateImagesDir + "1.3.1_imgBw.jpg", m_params.saveDebugImage);
		cv::Mat imgCanny;
		cv::Canny(outInfo.images.roiImg, imgCanny, m_params.outCircleEdgeThresh, m_params.outCircleEdgeThresh * 3);
		DAS->DAS_Img(imgCanny, outInfo.paths.intermediateImagesDir + "1.3.2_imgCanny.jpg", m_params.saveDebugImage);
		int kernelSize = 3;
		cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
			cv::Size(kernelSize, kernelSize));
		cv::morphologyEx(imgCanny, imgCanny, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 2);
		DAS->DAS_Img(imgCanny, outInfo.paths.intermediateImagesDir + "1.3.3_imgMorph.jpg",
			m_params.saveDebugImage);

		cv::Mat imgCircle = imgCanny | imgBw;
		imgCircle = mask & imgCircle;
		DAS->DAS_Img(imgCircle, outInfo.paths.intermediateImagesDir + "1.3.4_imgOutCircle.jpg", m_params.saveDebugImage);

		// 7. 去除小连通域干扰
		cv::Mat imgCircleFil = cv::Mat::zeros(outInfo.images.roiImg.size(), CV_8U);
		std::vector<std::vector<cv::Point>> contours;
		std::vector<cv::Vec4i> hierarchy;
		cv::findContours(imgCircle, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

		for (int i = 0; i < contours.size(); i++) {
			cv::Rect tmpRect = cv::boundingRect(contours[i]);
			int area = cv::contourArea(contours[i]);

			if (MAX(tmpRect.width, tmpRect.height) > m_params.filterThresh &&
				MAX(tmpRect.width, tmpRect.height) > m_params.filterThresh &&
				area > m_params.filterThresh * 5) {
				if (hierarchy[i][3] == -1) {
					cv::drawContours(imgCircleFil, contours, i, cv::Scalar(255, 255, 255),
						cv::FILLED, cv::LINE_8, hierarchy);
				}
			}
		}
		DAS->DAS_Img(imgCircleFil, outInfo.paths.intermediateImagesDir + "1.4.1_imgBwFil.jpg",
			m_params.saveDebugImage);

		// 8. 定位外环边缘
		cv::Mat outEdge = cv::Mat::zeros(imgCircleFil.size(), CV_8UC1);
		ContinuousEdgeDetector detector(outInfo.geometry.outCenter, outInfo.geometry.outCircle[2] - m_params.outCircleDiameterRange, outInfo.geometry.outCircle[2] + m_params.outCircleDiameterRange);
		auto [goodPts, badPts] = detector.findContinuousEdge(imgCircleFil, outEdge, false);
		outInfo.geometry.outEdgePoints = std::move(goodPts);
		outInfo.geometry.outEdgePointsBad = std::move(badPts);
		DAS->DAS_Points(outInfo.images.roiImg, outInfo.geometry.outEdgePoints, outInfo.paths.intermediateImagesDir + "1.5.1 outEdgePoints.jpg",
			m_params.saveDebugImage);
		DAS->DAS_Points(outInfo.images.roiImg, outInfo.geometry.outEdgePointsBad, outInfo.paths.intermediateImagesDir + "1.5.2 outEdgePointsBad.jpg",
			m_params.saveDebugImage);

		if (outInfo.geometry.outEdgePoints.size() < 2 * outInfo.geometry.outCircle[2])
		{
			outInfo.status.statusCode = BOTTLENECK_RETURN_LOCATE_OUT_CIRCLE_FAILED;
			outInfo.status.errorMessage = "定位失败，若支撑环正常则调小二值化阈值!";
			LOG->WriteAsyncLog("定位失败，若支撑环正常则调小二值化阈值!", ERR, outInfo.paths.logFile, true);
			return;
		}

		// 9. 去除偏离点并重新拟合椭圆
		FitEllipseWithOutlierRemoval(outInfo);


		// 11. 只有成功拟合的圆才进行统计
		if (outInfo.status.statusCode == BOTTLENECK_RETURN_OK) {
			UpdateStatistics(outInfo);

			LogStatistics(outInfo);
		}

		LOG->WriteAsyncLog("外圆定位完成!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	else
	{
		Log::WriteAsyncLog("精确定位圆未启用!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);

		outInfo.geometry.rectNeck.x = outInfo.geometry.outCircle[0] - m_params.roiRect.x -
			m_params.outCircleDiameter * 0.5 - m_params.neckExt;
		outInfo.geometry.rectNeck.y = outInfo.geometry.outCircle[1] - m_params.roiRect.y -
			m_params.outCircleDiameter * 0.5 - m_params.neckExt;
		outInfo.geometry.rectNeck.width = m_params.outCircleDiameter + m_params.neckExt * 2;
		outInfo.geometry.rectNeck.height = m_params.outCircleDiameter + m_params.neckExt * 2;
		ANA->RestrainRect(m_params.roiRect, outInfo.geometry.rectNeck, outInfo.geometry.rectNeck);

	}

	// 10. 提取瓶颈图像
	outInfo.images.imgNeck = outInfo.images.roiImg(outInfo.geometry.rectNeck).clone();
	cv::cvtColor(outInfo.images.imgNeck, outInfo.images.imgNeck, cv::COLOR_GRAY2BGR);
	DAS->DAS_Img(outInfo.images.imgNeck,
		outInfo.paths.intermediateImagesDir + "1.7_imgNeck.jpg",
		m_params.saveDebugImage);
}

// 辅助函数：计算两点间距离
double calculateDistance(const cv::Point& p1, const cv::Point& p2) {
	double dx = p1.x - p2.x;
	double dy = p1.y - p2.y;
	return std::sqrt(dx * dx + dy * dy);
}

// 查找连续点集（基于相邻距离阈值）
std::vector<std::vector<cv::Point>> findContinuousClusters(
	const std::vector<cv::Point>& points,
	double distanceThreshold = 5.0) {

	std::vector<std::vector<cv::Point>> clusters;

	if (points.empty()) {
		return clusters;
	}

	// 如果点集已经按某种顺序排列（比如沿圆周），使用简单聚类
	std::vector<bool> visited(points.size(), false);

	for (size_t i = 0; i < points.size(); i++) {
		if (visited[i]) continue;

		std::vector<cv::Point> currentCluster;
		std::queue<size_t> toVisit;

		toVisit.push(i);
		visited[i] = true;

		while (!toVisit.empty()) {
			size_t currentIdx = toVisit.front();
			toVisit.pop();

			currentCluster.push_back(points[currentIdx]);

			// 查找当前点的邻近点
			for (size_t j = 0; j < points.size(); j++) {
				if (!visited[j] && calculateDistance(points[currentIdx], points[j]) <= distanceThreshold) {
					toVisit.push(j);
					visited[j] = true;
				}
			}
		}

		if (!currentCluster.empty()) {
			clusters.push_back(currentCluster);
		}
	}

	return clusters;
}

// 分析缺陷并添加到输出信息
void InspBottleNeck::AnalyzeAndMarkDefects(InspBottleNeckOut& outInfo) {
	if (outInfo.status.statusCode != BOTTLENECK_RETURN_OK) {
		return;
	}

	// 清空现有缺陷信息
	outInfo.geometry.outEdgeDefects.clear();
	outInfo.geometry.outlierDefects.clear();

	// 1. 分析outEdgePointsBad缺陷
	if (!outInfo.geometry.outEdgePointsBad.empty() && m_params.outBreakThresh > 0) {
		auto edgeClusters = findContinuousClusters(outInfo.geometry.outEdgePointsBad, 5.0);

		for (const auto& cluster : edgeClusters) {
			if (cluster.size() > m_params.outBreakThresh) {
				InspBottleNeckOut::Geometry::DefectInfo defect;
				defect.points = cluster;
				defect.pointCount = static_cast<int>(cluster.size());

				// 计算边界框
				int minX = INT_MAX, minY = INT_MAX;
				int maxX = INT_MIN, maxY = INT_MIN;
				int sumX = 0, sumY = 0;

				for (const auto& pt : cluster) {
					minX = std::min(minX, pt.x);
					minY = std::min(minY, pt.y);
					maxX = std::max(maxX, pt.x);
					maxY = std::max(maxY, pt.y);
					sumX += pt.x;
					sumY += pt.y;
				}

				defect.boundingBox = cv::Rect(minX, minY, maxX - minX, maxY - minY);
				defect.centroid = cv::Point(sumX / cluster.size(), sumY / cluster.size());

				// 计算缺陷严重程度（基于点集大小）
				float maxPossibleDefect = 100.0f; // 可配置
				defect.severity = std::min(1.0f, static_cast<float>(cluster.size()) / maxPossibleDefect);

				outInfo.geometry.outEdgeDefects.push_back(defect);

				// 记录日志
				LOG->WriteAsyncLog("发现外边缘缺陷: 点数=" + std::to_string(cluster.size()) +
					", 位置=(" + std::to_string(defect.centroid.x) + "," +
					std::to_string(defect.centroid.y) + ")",
					INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			}
		}
	}

	// 2. 分析outlierPoints缺陷
	if (!outInfo.geometry.outlierPoints.empty() && m_params.outBreakThresh > 0) {
		auto outlierClusters = findContinuousClusters(outInfo.geometry.outlierPoints, 5.0);

		for (const auto& cluster : outlierClusters) {
			if (cluster.size() > m_params.outBreakThresh) {
				InspBottleNeckOut::Geometry::DefectInfo defect;
				defect.points = cluster;
				defect.pointCount = static_cast<int>(cluster.size());

				// 计算边界框和质心
				int minX = INT_MAX, minY = INT_MAX;
				int maxX = INT_MIN, maxY = INT_MIN;
				int sumX = 0, sumY = 0;

				for (const auto& pt : cluster) {
					minX = std::min(minX, pt.x);
					minY = std::min(minY, pt.y);
					maxX = std::max(maxX, pt.x);
					maxY = std::max(maxY, pt.y);
					sumX += pt.x;
					sumY += pt.y;
				}

				defect.boundingBox = cv::Rect(minX, minY, maxX - minX, maxY - minY);
				defect.centroid = cv::Point(sumX / cluster.size(), sumY / cluster.size());

				// 计算缺陷严重程度
				float maxPossibleDefect = 100.0f;
				defect.severity = std::min(1.0f, static_cast<float>(cluster.size()) / maxPossibleDefect);

				outInfo.geometry.outlierDefects.push_back(defect);

				// 记录日志
				LOG->WriteAsyncLog("发现离群点缺陷: 点数=" + std::to_string(cluster.size()) +
					", 位置=(" + std::to_string(defect.centroid.x) + "," +
					std::to_string(defect.centroid.y) + ")",
					INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			}
		}
	}

	// 3. 如果有缺陷，更新状态码
	if (!outInfo.geometry.outEdgeDefects.empty()) {
		outInfo.status.statusCode = BOTTLENECK_RETURN_OUT_BREAK0;
		outInfo.status.errorMessage = "支撑环边缘缺陷-缺失";
	}

	if (!outInfo.geometry.outlierDefects.empty()) {
		if (outInfo.status.statusCode == BOTTLENECK_RETURN_OK) {
			outInfo.status.statusCode = BOTTLENECK_RETURN_OUT_BREAK1;
			outInfo.status.errorMessage = "支撑环边缘缺陷-凹凸";
		}
	}
}

void InspBottleNeck::FitEllipseWithOutlierRemoval(InspBottleNeckOut& outInfo) {
	// 初始拟合椭圆
	cv::RotatedRect initialEllipse = cv::fitEllipseAMS(outInfo.geometry.outEdgePoints);
	DAS->DAS_RotateRect(outInfo.images.roiImg, initialEllipse,
		outInfo.paths.intermediateImagesDir + "1.6.1_initialEllipse.jpg",
		m_params.saveDebugImage);

	// 计算每个点到初始椭圆的距离
	std::vector<double> distances;
	for (const auto& pt : outInfo.geometry.outEdgePoints) {
		double dist = DistanceToEllipse(pt, initialEllipse);
		distances.push_back(dist);
	}

	// 设置距离阈值：均值 + 2倍标准差
	double distThreshold = m_params.circlePointDevThresh;

	// 找到连续的偏离点序列
	std::vector<cv::Point> filteredPoints;
	int continuousOutlierCount = 0;
	int maxContinuousOutliers = 0;
	bool inOutlierSegment = false;
	int segmentStartIdx = -1;

	for (size_t i = 0; i < outInfo.geometry.outEdgePoints.size(); i++) {
		if (distances[i] > distThreshold) {
			// 偏离点
			outInfo.geometry.outlierPoints.push_back(outInfo.geometry.outEdgePoints[i]);

			if (!inOutlierSegment) {
				inOutlierSegment = true;
				segmentStartIdx = i;
			}
			continuousOutlierCount++;
		}
		else {
			// 正常点
			if (inOutlierSegment) {
				// 结束一个偏离段
				if (continuousOutlierCount > maxContinuousOutliers) {
					maxContinuousOutliers = continuousOutlierCount;
				}

				// 如果连续偏离点数小于阈值，保留这些点
				if (continuousOutlierCount <= m_params.outBreakThresh) {
					for (int j = segmentStartIdx; j < i; j++) {
						filteredPoints.push_back(outInfo.geometry.outEdgePoints[j]);
					}
				}
				inOutlierSegment = false;
				continuousOutlierCount = 0;
			}
			filteredPoints.push_back(outInfo.geometry.outEdgePoints[i]);
		}
	}

	// 处理最后一个可能的偏离段
	if (inOutlierSegment) {
		if (continuousOutlierCount > maxContinuousOutliers) {
			maxContinuousOutliers = continuousOutlierCount;
		}

		if (continuousOutlierCount <= m_params.outBreakThresh) {
			for (int j = segmentStartIdx; j < outInfo.geometry.outEdgePoints.size(); j++) {
				filteredPoints.push_back(outInfo.geometry.outEdgePoints[j]);
			}
		}
	}

	// 绘制偏离点
	DAS->DAS_Points(outInfo.images.roiImg, outInfo.geometry.outlierPoints,
		outInfo.paths.intermediateImagesDir + "1.6.2_outlierPoints.jpg",
		m_params.saveDebugImage);

	// 检查过滤后的点数是否足够
	if (filteredPoints.size() < 5) {
		outInfo.status.statusCode = BOTTLENECK_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "配置错误-外环边缘点偏离阈值设置过小!";
		LOG->WriteAsyncLog("配置错误-外环边缘点偏离阈值设置过小!", ERR, outInfo.paths.logFile, true);
		return;
	}

	// 用过滤后的点重新拟合椭圆
	outInfo.geometry.outCircleBox = cv::fitEllipseAMS(filteredPoints);
	outInfo.geometry.outCircleRadius =
		(outInfo.geometry.outCircleBox.size.width +
			outInfo.geometry.outCircleBox.size.height) * 0.25;

	// 计算椭圆度
	outInfo.geometry.ellipticity = fabs(outInfo.geometry.outCircleBox.size.width -
		outInfo.geometry.outCircleBox.size.height);

	// 用原始点集计算不规则度
	outInfo.geometry.irregularity = calculateIrregularity(
		outInfo.geometry.outEdgePoints,   // 使用原始点集
		outInfo.geometry.outCircleBox
	);

	// 记录统计信息
	outInfo.statistics.initialPointCount = outInfo.geometry.outEdgePoints.size();
	outInfo.statistics.filteredPointCount = filteredPoints.size();
	outInfo.statistics.outlierCount = outInfo.geometry.outlierPoints.size();
	outInfo.statistics.maxContinuousOutliers = maxContinuousOutliers;
	outInfo.statistics.distanceThreshold = distThreshold;

	// 将椭圆中心转换到原图坐标系
	outInfo.geometry.outCircleBox.center.x += m_params.roiRect.x;
	outInfo.geometry.outCircleBox.center.y += m_params.roiRect.y;

	// 设置瓶颈区域
	outInfo.geometry.rectNeck.x = outInfo.geometry.outCircleBox.center.x - m_params.roiRect.x -
		m_params.outCircleDiameter * 0.5 - m_params.neckExt;
	outInfo.geometry.rectNeck.y = outInfo.geometry.outCircleBox.center.y - m_params.roiRect.y -
		m_params.outCircleDiameter * 0.5 - m_params.neckExt;
	outInfo.geometry.rectNeck.width = m_params.outCircleDiameter + m_params.neckExt * 2;
	outInfo.geometry.rectNeck.height = m_params.outCircleDiameter + m_params.neckExt * 2;
	ANA->RestrainRect(m_params.roiRect, outInfo.geometry.rectNeck, outInfo.geometry.rectNeck);

	// 椭圆度检查
	if (outInfo.geometry.ellipticity > m_params.outNotCircleThresh) {
		outInfo.status.statusCode = BOTTLENECK_RETURN_OUT_NOT_CIRCLE;
		outInfo.status.errorMessage = "椭圆!";
		LOG->WriteAsyncLog("椭圆!", ERR, outInfo.paths.logFile, true);
	}

	if (outInfo.geometry.irregularity > m_params.irregularity) {
		outInfo.status.statusCode = BOTTLENECK_RETURN_OUT_IRREGULARITY;
		outInfo.status.errorMessage = "不规则圆!";
		LOG->WriteAsyncLog("不规则圆!", ERR, outInfo.paths.logFile, true);
	}

	DAS->DAS_Points(outInfo.images.roiImg, outInfo.geometry.outEdgePoints, outInfo.paths.intermediateImagesDir + "1.6.3_finalEllipse.jpg", m_params.saveDebugImage);

	LOG->WriteAsyncLog("椭圆拟合完成! 原始点数: " + std::to_string(outInfo.statistics.initialPointCount) +
		", 过滤后点数: " + std::to_string(outInfo.statistics.filteredPointCount) +
		", 偏离点数: " + std::to_string(outInfo.statistics.outlierCount) +
		", 最大连续偏离点数: " + std::to_string(outInfo.statistics.maxContinuousOutliers) +
		", 距离阈值: " + std::to_string(outInfo.statistics.distanceThreshold),
		INFO, outInfo.paths.logFile, m_params.saveLogTxt);
}

double InspBottleNeck::DistanceToEllipse(const cv::Point2f& pt, const cv::RotatedRect& ellipse) {
	// 获取椭圆参数
	cv::Point2f center = ellipse.center;
	float a = ellipse.size.width / 2.0f;  // 长半轴
	float b = ellipse.size.height / 2.0f; // 短半轴
	float angle = ellipse.angle * CV_PI / 180.0f;  // 转换为弧度

	// 将点转换到椭圆坐标系
	float cos_angle = cos(angle);
	float sin_angle = sin(angle);

	float dx = pt.x - center.x;
	float dy = pt.y - center.y;

	// 旋转到椭圆轴对齐坐标系
	float x_rot = dx * cos_angle + dy * sin_angle;
	float y_rot = -dx * sin_angle + dy * cos_angle;

	// 计算点到椭圆的最短距离（近似解）
	// 使用梯度下降法求解最近点
	float t = atan2(a * y_rot, b * x_rot);
	float x_e, y_e;

	// 迭代求解（通常3-4次迭代即可收敛）
	for (int i = 0; i < 5; i++) {
		x_e = a * cos(t);
		y_e = b * sin(t);

		float dx = x_rot - x_e;
		float dy = y_rot - y_e;

		// 计算梯度
		float grad_x = -dx * a * sin(t) + dy * b * cos(t);
		t -= 0.1 * grad_x / (a * b);  // 学习率
	}

	// 计算最终点
	x_e = a * cos(t);
	y_e = b * sin(t);

	// 计算欧氏距离
	float distance = sqrt(pow(x_rot - x_e, 2) + pow(y_rot - y_e, 2));

	return distance;
}

// 统计相关函数实现
void InspBottleNeck::UpdateStatistics(InspBottleNeckOut& outInfo) {
	std::lock_guard<std::mutex> lock(m_statsMutex);

	// 如果统计未初始化，先初始化
	if (!m_statsInitialized) {
		m_totalFittedCount = 0;
		m_sumEllipticity = 0.0;
		m_sumIrregularity = 0.0;
		m_sumDiameter = 0.0;
		m_statsInitialized = true;
	}

	// 计算当前检测的直径（长轴+短轴的平均值）
	double currentDiameter = (outInfo.geometry.outCircleBox.size.width +
		outInfo.geometry.outCircleBox.size.height) * 0.5;

	// 更新统计
	m_totalFittedCount++;
	m_sumEllipticity += outInfo.geometry.ellipticity;
	m_sumIrregularity += outInfo.geometry.irregularity;
	m_sumDiameter += currentDiameter;

	// 在日志中记录本次检测结果
	LOG->WriteAsyncLog("检测#" + std::to_string(m_totalFittedCount) +
		": 椭圆度=" + std::to_string(outInfo.geometry.ellipticity) +
		", 不规则度=" + std::to_string(outInfo.geometry.irregularity) +
		", 直径=" + std::to_string(currentDiameter),
		INFO, outInfo.paths.logFile, m_params.saveLogTxt);
}


void InspBottleNeck::LogStatistics(InspBottleNeckOut& outInfo) {
	std::lock_guard<std::mutex> lock(m_statsMutex);

	if (m_totalFittedCount > 0) {
		double avgEllipticity = m_sumEllipticity / m_totalFittedCount;
		double avgIrregularity = m_sumIrregularity / m_totalFittedCount;
		double avgDiameter = m_sumDiameter / m_totalFittedCount;

		LOG->WriteAsyncLog("=== 统计信息（累计" + std::to_string(m_totalFittedCount) + "次）===",
			INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		LOG->WriteAsyncLog("平均椭圆度: " + std::to_string(avgEllipticity),
			INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		LOG->WriteAsyncLog("平均不规则度: " + std::to_string(avgIrregularity),
			INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		LOG->WriteAsyncLog("平均直径: " + std::to_string(avgDiameter),
			INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}
}

InspBottleNeck::AverageStats InspBottleNeck::GetAverageStats() const {
	std::lock_guard<std::mutex> lock(m_statsMutex);

	AverageStats stats;
	stats.fittedCount = m_totalFittedCount;

	if (m_totalFittedCount > 0) {
		stats.avgEllipticity = m_sumEllipticity / m_totalFittedCount;
		stats.avgIrregularity = m_sumIrregularity / m_totalFittedCount;
		stats.avgDiameter = m_sumDiameter / m_totalFittedCount;
	}

	return stats;
}

void InspBottleNeck::ResetStatistics() {
	std::lock_guard<std::mutex> lock(m_statsMutex);

	m_totalFittedCount = 0;
	m_sumEllipticity = 0.0;
	m_sumIrregularity = 0.0;
	m_sumDiameter = 0.0;

	LOG->WriteAsyncLog("统计已重置", INFO, "", false);
}

void InspBottleNeck::BottleNeck_Classfy(InspBottleNeckOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;

	if (outInfo.status.statusCode != BOTTLENECK_RETURN_OK) {
		Log::WriteAsyncLog("跳过瓶口分类!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	if (!m_params.isClassfy)
	{
		Log::WriteAsyncLog("瓶口分类未启用!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始瓶口分类!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (m_params.neckClassifyWeightsFile.find(".onnx") != std::string::npos || m_params.neckClassifyWeightsFile.find(".engine") != std::string::npos)
	{
		outInfo.classification.neckType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.neckClassifyWeightsFile, m_params.neckClassifyClassName, outInfo.images.imgNeck);
	}
	else
	{
		outInfo.status.statusCode = BOTTLENECK_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "瓶口分类模型文件异常!";
		Log::WriteAsyncLog("瓶口分类模型文件异常:目前只支持onnx或engine!", ERR, outInfo.paths.logFile, true);
		return;
	}

	if (outInfo.classification.neckType.className == "NG")
	{
		outInfo.status.statusCode = BOTTLENECK_RETURN_CLASSFY_DEFECT0;
		outInfo.status.errorMessage = "分类-瓶口严重缺陷!";
		Log::WriteAsyncLog("分类-瓶口严重缺陷!", ERR, outInfo.paths.logFile, true);
	}

	if ((m_params.saveTrain == 1 || m_params.saveTrain == 2) && outInfo.status.statusCode == BOTTLENECK_RETURN_OK)
	{
		COM->CreateDir(outInfo.paths.trainDir + "NECK/CLASSFY/OK");
		cv::imwrite(outInfo.paths.trainDir + "NECK/CLASSFY/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_CLASSFY.jpg", outInfo.images.imgNeck);
	}
	else if (m_params.saveTrain == 1 || m_params.saveTrain == 3 && outInfo.status.statusCode != BOTTLENECK_RETURN_OK)
	{
		COM->CreateDir(outInfo.paths.trainDir + "NECK/CLASSFY/NG");
		cv::imwrite(outInfo.paths.trainDir + "NECK/CLASSFY/NG/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_CLASSFY.jpg", outInfo.images.imgNeck);
	}
}


void InspBottleNeck::BottleNeck_Defect(InspBottleNeckOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	Log::WriteAsyncLog("开始瓶口缺陷定位!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);


	if (outInfo.status.statusCode != BOTTLENECK_RETURN_OK) {
		Log::WriteAsyncLog("跳过瓶口缺陷定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	if (!m_params.isLocate)
	{
		Log::WriteAsyncLog("缺陷定位未启用!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始缺陷定位!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	if (m_params.locateWeightsFile.find(".onnx") != std::string::npos)
	{
		outInfo.locate.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.locateWeightsFile, m_params.locateClassName, outInfo.images.imgNeck, 0.1, 0.3);
	}
	else
	{
		outInfo.status.statusCode = BOTTLENECK_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "瓶口缺陷定位模型文件异常!";
		Log::WriteAsyncLog("瓶口缺陷定位模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}

	DAS->DAS_FinsObject(outInfo.images.roiImg, outInfo.locate.details, outInfo.paths.intermediateImagesDir + "2.1.1.details.jpg", m_params.saveDebugImage);


	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{
		outInfo.locate.details[i].box = ANA->AdjustROI(outInfo.locate.details[i].box, outInfo.images.roiImg);
		outInfo.locate.details[i].box.x += outInfo.geometry.rectNeck.x;
		outInfo.locate.details[i].box.y += outInfo.geometry.rectNeck.y;
	}

	Log::WriteAsyncLog("开始分析定位结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	for (int i = outInfo.locate.details.size() - 1; i >= 0; --i)
	{
		auto& locate = outInfo.locate.details[i];
		int paramIndex = -1; // 根据缺陷类别设置对应参数索引

		bool valid = true;
		if (locate.className == "瓶口缺陷1")paramIndex = 1;
		else if (locate.className == "瓶口缺陷0")	paramIndex = 0;

		if (paramIndex != -1)
		{
			auto& para = m_params.locatePara[paramIndex];
			if (locate.box.width < para.widthRange[0] ||
				locate.box.width > para.widthRange[1] ||
				locate.box.height < para.heightRange[0] ||
				locate.box.height > para.heightRange[1] ||
				locate.confidence < para.confidenceThresh)
			{
				valid = false;
			}
		}

		if (!valid) {
			outInfo.locate.details.erase(outInfo.locate.details.begin() + i);

		}
	}

	bool findTop0 = false;
	bool findTop1 = false;

	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{

		if (outInfo.locate.details[i].className == "瓶口缺陷0")
		{
			findTop0 = true;
			break;
		}
		else if (outInfo.locate.details[i].className == "瓶口缺陷1")
		{
			findTop1 = true;
		}
	}

	if (findTop0)
	{
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			COM->CreateDir(outInfo.paths.trainDir + "DEFECT/瓶口缺陷0");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_瓶口缺陷0" + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "/NECK/DEFECT/瓶口缺陷0" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_瓶口缺陷0" + ".json");
			cv::imwrite(outInfo.paths.trainDir + "/NECKDEFECT/瓶口缺陷0" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_瓶口缺陷0" + ".jpg", outInfo.images.imgNeck);
		}

		outInfo.status.errorMessage = "定位-瓶口缺陷0!";
		Log::WriteAsyncLog("定位-瓶口缺陷0!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = BOTTLENECK_RETURN_LOCATE_DEFECT_IN0;
		return;
	}
	else if (findTop1)
	{
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			COM->CreateDir(outInfo.paths.trainDir + "DEFECT/瓶口缺陷1");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_瓶口缺陷1" + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "/NECK/DEFECT/瓶口缺陷1" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_瓶口缺陷1" + ".json");
			cv::imwrite(outInfo.paths.trainDir + "/NECK/DEFECT/瓶口缺陷1" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_瓶口缺陷1" + ".jpg", outInfo.images.imgNeck);
		}

		outInfo.status.errorMessage = "定位-瓶口缺陷1!";
		Log::WriteAsyncLog("定位-瓶口缺陷1!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = BOTTLENECK_RETURN_LOCATE_DEFECT_IN0;
		return;
	}





}


// 继续完成 BottleNeck_DrawResult 函数
void InspBottleNeck::BottleNeck_DrawResult(InspBottleNeckOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	Log::WriteAsyncLog("开始缺陷绘制!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

	// 1. 绘制ROI
	cv::rectangle(outInfo.images.outputImg, m_params.roiRect, Colors::YELLOW, 1, cv::LINE_AA);

	if (outInfo.status.statusCode == BOTTLENECK_RETURN_CLOSE_BOUNDARY || !m_params.fineOut)
	{
		circle(outInfo.images.outputImg,
			cv::Point2f(outInfo.geometry.outCircle[0] + m_params.roiRect.x, outInfo.geometry.outCircle[1] + m_params.roiRect.y),
			outInfo.geometry.outCircle[2], Colors::GREEN, 2, 8, 0);
	}

	outInfo.geometry.rectNeck.x += m_params.roiRect.x;
	outInfo.geometry.rectNeck.y += m_params.roiRect.y;
	cv::rectangle(outInfo.images.outputImg, outInfo.geometry.rectNeck, Colors::GREEN, 1, cv::LINE_AA);

	// 2. 绘制外圆椭圆
	for (const auto& pt : outInfo.geometry.outEdgePoints) {
		cv::Point globalPt = cv::Point(
			pt.x + m_params.roiRect.x,
			pt.y + m_params.roiRect.y
		);
		cv::circle(outInfo.images.outputImg, globalPt, 1, Colors::GREEN, -1);
	}

	// 5. 绘制统计信息和状态
	auto stats = GetAverageStats();

	// 状态信息
	std::string rv = "ID = " + std::to_string(outInfo.system.jobId) +
		", RV = " + std::to_string(outInfo.status.statusCode) +
		", " + outInfo.status.errorMessage;

	cv::Scalar statusColor = (outInfo.status.statusCode == BOTTLENECK_RETURN_OK) ?
		Colors::GREEN : Colors::RED;

	putTextZH(outInfo.images.outputImg, rv.c_str(), cv::Point(15, 30), statusColor, 40, FW_BOLD);

	// 6. 缺陷统计信息
	int y_offset = 120;

	// 7. 原有几何参数显示
	if (outInfo.geometry.ellipticity > m_params.outNotCircleThresh) {
		putTextZH(outInfo.images.outputImg,
			("椭圆度 = " + std::to_string(outInfo.geometry.ellipticity)).c_str(),
			cv::Point(15, y_offset), Colors::RED, 35, FW_BOLD);
	}
	else {
		putTextZH(outInfo.images.outputImg,
			("椭圆度 = " + std::to_string(outInfo.geometry.ellipticity)).c_str(),
			cv::Point(15, y_offset), Colors::GREEN, 35, FW_BOLD);
	}
	y_offset += 60;

	if (outInfo.geometry.irregularity > m_params.irregularity) {
		putTextZH(outInfo.images.outputImg,
			("不规则度 = " + std::to_string(outInfo.geometry.irregularity)).c_str(),
			cv::Point(15, y_offset), Colors::RED, 35, FW_BOLD);
	}
	else {
		putTextZH(outInfo.images.outputImg,
			("不规则度 = " + std::to_string(outInfo.geometry.irregularity)).c_str(),
			cv::Point(15, y_offset), Colors::GREEN, 35, FW_BOLD);
	}
	y_offset += 60;

	// 8. 统计信息
	if (stats.fittedCount > 0) {
		putTextZH(outInfo.images.outputImg,
			("平均椭圆度 = " + std::to_string(stats.avgEllipticity)).c_str(),
			cv::Point(15, y_offset), Colors::GREEN, 35, FW_BOLD);
		y_offset += 60;

		putTextZH(outInfo.images.outputImg,
			("平均不规则度 = " + std::to_string(stats.avgIrregularity)).c_str(),
			cv::Point(15, y_offset), Colors::GREEN, 35, FW_BOLD);
		y_offset += 60;

		putTextZH(outInfo.images.outputImg,
			("平均直径 = " + std::to_string(stats.avgDiameter)).c_str(),
			cv::Point(15, y_offset), Colors::GREEN, 35, FW_BOLD);
		y_offset += 60;
	}


	// 外边缘缺陷信息
	if (!outInfo.geometry.outEdgeDefects.empty()) {
		std::string edgeDefectInfo = "支撑环缺失: " +
			std::to_string(outInfo.geometry.outEdgeDefects.size()) + "处";
		putTextZH(outInfo.images.outputImg, edgeDefectInfo.c_str(),
			cv::Point(15, y_offset), Colors::RED, 35, FW_BOLD);
		y_offset += 40;

		for (size_t i = 0; i < outInfo.geometry.outEdgeDefects.size(); i++) {
			// 绘制缺陷点（橙色圆点）
			for (const auto& pt : outInfo.geometry.outEdgeDefects[i].points) {
				cv::Point globalPt = cv::Point(
					pt.x + m_params.roiRect.x,
					pt.y + m_params.roiRect.y
				);
				cv::circle(outInfo.images.outputImg, globalPt, 1, Colors::RED, -1);
			}
			cv::Point globalCentroid = cv::Point(
				outInfo.geometry.outEdgeDefects[i].centroid.x + m_params.roiRect.x,
				outInfo.geometry.outEdgeDefects[i].centroid.y + m_params.roiRect.y
			);
			// 添加缺陷标签
			std::string label = "缺失点集: " + std::to_string(i + 1);
			putTextZH(outInfo.images.outputImg, label.c_str(),
				globalCentroid + cv::Point(10, 0), Colors::RED, 20, FW_BOLD);

			const auto& defect = outInfo.geometry.outEdgeDefects[i];
			std::string detail = "缺失点集" + std::to_string(i + 1) + ": " +
				std::to_string(defect.pointCount) + "点";
			putTextZH(outInfo.images.outputImg, detail.c_str(),
				cv::Point(30, y_offset), Colors::RED, 25, FW_NORMAL);
			y_offset += 30;
		}
		y_offset += 10;
	}
	//y_offset += 60;
	// 离群点缺陷信息
	if (!outInfo.geometry.outlierDefects.empty()) {
		std::string outlierDefectInfo = "支撑环凹凸: " +
			std::to_string(outInfo.geometry.outlierDefects.size()) + "处";
		putTextZH(outInfo.images.outputImg, outlierDefectInfo.c_str(),
			cv::Point(15, y_offset), Colors::RED, 35, FW_BOLD);
		y_offset += 40;

		for (size_t i = 0; i < outInfo.geometry.outlierDefects.size(); i++) {
			// 绘制缺陷点（橙色圆点）
			for (const auto& pt : outInfo.geometry.outlierDefects[i].points) {
				cv::Point globalPt = cv::Point(
					pt.x + m_params.roiRect.x,
					pt.y + m_params.roiRect.y
				);
				cv::circle(outInfo.images.outputImg, globalPt, 1, Colors::RED, -1);
			}
			cv::Point globalCentroid = cv::Point(
				outInfo.geometry.outlierDefects[i].centroid.x + m_params.roiRect.x,
				outInfo.geometry.outlierDefects[i].centroid.y + m_params.roiRect.y
			);
			// 添加缺陷标签
			std::string label = "凹凸点集: " + std::to_string(i + 1);
			putTextZH(outInfo.images.outputImg, label.c_str(),
				globalCentroid + cv::Point(10, 0), Colors::RED, 20, FW_BOLD);

			const auto& defect = outInfo.geometry.outlierDefects[i];
			std::string detail = "凹凸点集" + std::to_string(i + 1) + ": " +
				std::to_string(defect.pointCount);
			putTextZH(outInfo.images.outputImg, detail.c_str(),
				cv::Point(30, y_offset), Colors::RED, 25, FW_NORMAL);
			y_offset += 30;
		}
		y_offset += 10;
	}

	auto format = [](float conf) {
		return (std::ostringstream() << std::fixed << std::setprecision(2) << conf).str();
	};
	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{
		cv::Rect boxTmp = outInfo.locate.details[i].box;
		boxTmp.x += m_params.roiRect.x;
		boxTmp.y += m_params.roiRect.y;
		if (outInfo.locate.details[i].className == "瓶口缺陷0")
		{
			rectangle(outInfo.images.outputImg, boxTmp, Colors::GOLDEN, 3, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg,
				(std::to_string(boxTmp.width) + "," + std::to_string(boxTmp.height) + "," + format(outInfo.locate.details[i].confidence) + ",瓶口缺陷0").c_str(),
				cv::Point(boxTmp.x, boxTmp.y + boxTmp.height + 10),
				Colors::GOLDEN, 25, FW_BOLD);
		}
		else if (outInfo.locate.details[i].className == "瓶口缺陷1")
		{
			rectangle(outInfo.images.outputImg, boxTmp, Colors::GOLDEN, 3, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg,
				(std::to_string(boxTmp.width) + "," + std::to_string(boxTmp.height) + "," + format(outInfo.locate.details[i].confidence) + ",瓶口缺陷1").c_str(),
				cv::Point(boxTmp.x, boxTmp.y + boxTmp.height + 10),
				Colors::GOLDEN, 25, FW_BOLD);
		}
	}

	// 9. 保存调试图像
	DAS->DAS_Img(outInfo.images.outputImg,
		outInfo.paths.intermediateImagesDir + "10.outputImg.jpg",
		m_params.saveDebugImage);
}

int InspBottleNeck::BottleNeck_Main(InspBottleNeckOut& outInfo) {
	// 清空上一轮的耗时记录
	outInfo.status.stageDurations.clear();
	auto startTotal = std::chrono::high_resolution_clock::now();

	try {
		double time0 = static_cast<double>(cv::getTickCount());

		if (outInfo.status.statusCode == BOTTLENECK_RETURN_OK) {
			LOG->WriteAsyncLog("BottleNeck_Main开始执行！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

			// --- 阶段 1: 定位 ---
			auto startStage = std::chrono::high_resolution_clock::now();
			BottleNeck_Locate(outInfo);
			auto endStage = std::chrono::high_resolution_clock::now();
			double stageDuration = std::chrono::duration<double, std::milli>(endStage - startStage).count();
			outInfo.status.stageDurations.emplace_back("BottleNeck_Locate", stageDuration);
			if (CheckTimeout(m_params.timeOut)) {
				LOG->WriteAsyncLog("定位阶段超时！", ERR, outInfo.paths.logFile, true);
				return BOTTLENECK_RETURN_TIMEOUT;
			}
			// --- 阶段 1 结束 ---

			// --- 阶段 2: 缺陷标记 ---
			startStage = std::chrono::high_resolution_clock::now();
			AnalyzeAndMarkDefects(outInfo);
			endStage = std::chrono::high_resolution_clock::now();
			stageDuration = std::chrono::duration<double, std::milli>(endStage - startStage).count();
			outInfo.status.stageDurations.emplace_back("AnalyzeAndMarkDefects", stageDuration);
			if (CheckTimeout(m_params.timeOut)) {
				LOG->WriteAsyncLog("缺陷标记超时！", ERR, outInfo.paths.logFile, true);
				return BOTTLENECK_RETURN_TIMEOUT;
			}
			// --- 阶段 2 结束 ---

			// --- 阶段 3: 分类 ---
			startStage = std::chrono::high_resolution_clock::now();
			BottleNeck_Classfy(outInfo);
			endStage = std::chrono::high_resolution_clock::now();
			stageDuration = std::chrono::duration<double, std::milli>(endStage - startStage).count();
			outInfo.status.stageDurations.emplace_back("BottleNeck_Classfy", stageDuration);
			if (CheckTimeout(m_params.timeOut)) {
				LOG->WriteAsyncLog("分类超时！", ERR, outInfo.paths.logFile, true);
				return BOTTLENECK_RETURN_TIMEOUT;
			}
			// --- 阶段 3 结束 ---

			// --- 阶段 4: 缺陷定位 ---
			startStage = std::chrono::high_resolution_clock::now();
			BottleNeck_Defect(outInfo);
			endStage = std::chrono::high_resolution_clock::now();
			stageDuration = std::chrono::duration<double, std::milli>(endStage - startStage).count();
			outInfo.status.stageDurations.emplace_back("BottleNeck_Defect", stageDuration);
			if (CheckTimeout(m_params.timeOut)) {
				LOG->WriteAsyncLog("缺陷定位超时！", ERR, outInfo.paths.logFile, true);
				return BOTTLENECK_RETURN_TIMEOUT;
			}
			// --- 阶段 4 结束 ---
		}

		// --- 阶段 5: 绘制结果 ---
		auto startStage = std::chrono::high_resolution_clock::now();
		BottleNeck_DrawResult(outInfo);
		auto endStage = std::chrono::high_resolution_clock::now();
		double stageDuration = std::chrono::duration<double, std::milli>(endStage - startStage).count();
		outInfo.status.stageDurations.emplace_back("BottleNeck_DrawResult", stageDuration);
		if (CheckTimeout(m_params.timeOut)) {
			LOG->WriteAsyncLog("绘制结果超时！", ERR, outInfo.paths.logFile, true);
			return BOTTLENECK_RETURN_TIMEOUT;
		}
		// --- 阶段 5 结束 ---

		// 输出结果图片
		if (outInfo.status.statusCode == BOTTLENECK_RETURN_OK) {
			DAS->DAS_Img(outInfo.images.outputImg,
				outInfo.paths.resultsOKDir + std::to_string(outInfo.system.jobId) + ".jpg",
				m_params.saveDebugImage);
		}
		else {
			DAS->DAS_Img(outInfo.images.outputImg,
				outInfo.paths.resultsNGDir + std::to_string(outInfo.system.jobId) + ".jpg",
				m_params.saveDebugImage);
		}

		// 记录总耗时
		auto endTotal = std::chrono::high_resolution_clock::now();
		double totalDuration = std::chrono::duration<double, std::milli>(endTotal - startTotal).count();
		outInfo.status.stageDurations.emplace_back("Total_Main", totalDuration);

		// 在日志中输出各阶段耗时（按实际执行顺序）
		std::stringstream ss;
		ss << "\n================ 阶段耗时分析 ================\n";
		for (const auto& [stage, duration] : outInfo.status.stageDurations) {
			ss << stage << ": " << std::fixed << std::setprecision(2) << duration << " ms\n";
		}
		LOG->WriteAsyncLog(ss.str(), INFO, outInfo.paths.logFile, m_params.saveLogTxt);

		outInfo.returnVal = outInfo.status.statusCode;
	}
	catch (const std::exception& e) {
		if (m_safeTimeoutFlag) {
			m_safeTimeoutFlag->store(false);
		}
		LOG->WriteAsyncLog("算法执行异常: " + std::string(e.what()), ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = BOTTLENECK_RETURN_OTHER;
		outInfo.status.errorMessage = "算法执行异常: " + std::string(e.what());
		outInfo.returnVal = BOTTLENECK_RETURN_OTHER;
	}

	return outInfo.status.statusCode;
}
