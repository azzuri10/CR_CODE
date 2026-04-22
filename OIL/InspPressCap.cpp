#include "HeaderDefine.h"
#include "InspPressCap.h"
#include "InspPressCapStruct.h"
#include "ModelManager.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <locale>
#include "InferenceWorker.h"
#include "Data.h"
#include "AnalyseMat.h"
#include "../XML/write_json.h"
#include <atomic>
#include <shared_mutex>

// 初始化静态成员
std::shared_mutex InspPressCap::modelLoadMutex;
std::map<std::string, std::string> InspPressCap::capDetectionModelMap;
std::map<std::string, std::string> InspPressCap::capDefectModelMap;
std::map<std::string, std::string> InspPressCap::capClassifyModelMap;
std::map<int, InspPressCapIn> InspPressCap::cameraConfigMap;

// 构造函数
InspPressCap::InspPressCap(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
	bool isLoadConfig, int timeOut, InspPressCapOut& outInfo)
	: ANA(std::make_unique<AnalyseMat>()),
	COM(std::make_unique<Common>()),
	TXT(std::make_unique<TxtOperater>())
{
	outInfo.system.startTime = COM->time_t2string_with_ms();
	m_img = img.clone();
	m_params.timeOut = timeOut;
	m_timeoutFlagRef = &outInfo.system.timeoutFlag;
	COM->CreateDir(outInfo.paths.logDirectory);
	Log::WriteAsyncLog("********** Start Inspction JobID = ", INFO, outInfo.paths.logFile, true, outInfo.system.jobId, " ***********");

	bool shouldLoadConfig = isLoadConfig ||
		jobId == 0 ||
		cameraConfigMap.find(cameraId) == cameraConfigMap.end();

	//读取config
	if (shouldLoadConfig)
	{
		bool rv_loadConfig = readParams(img, outInfo.paths.configFile, m_params, outInfo, outInfo.paths.logFile);
		if (!rv_loadConfig) {
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = outInfo.status.errorMessage;
			Log::WriteAsyncLog(outInfo.status.errorMessage, ERR, outInfo.paths.logFile, true);
			return;
		}
		else
		{
			Log::WriteAsyncLog("读取config成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}


		//检测roi
		if (!ANA->JudgeRectIn(cv::Rect(0, 0, img.cols, img.rows), m_params.roiRect)) {
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "roi 设置超出图像范围!";
			Log::WriteAsyncLog("roi 设置超出图像范围", ERR, outInfo.paths.logFile, true);
			return;
		}

		//读取定位配置文件
		if (LoadConfigYOLO(
			m_params.locateThreshConfig,
			m_params.locatePara,
			m_params.locateClassName,
			outInfo.paths.logFile) != 1)
		{
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("定位阈值文件-参数设置错误！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "定位阈值文件-参数设置错误!";
			return;
		}
		else
		{
			Log::WriteAsyncLog("定位参数读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}

		//读取缺陷配置文件
		if (LoadConfigYOLO(
			m_params.defectThreshConfig,
			m_params.defectPara,
			m_params.defectClassName,
			outInfo.paths.logFile) != 1)
		{
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("缺陷参数设置错误！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "缺陷参数设置错误!";
			return;
		}
		else
		{
			Log::WriteAsyncLog("缺陷参数读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}


		//读取分类类型名称
		std::ifstream ifs1(m_params.classifyNameFile.c_str());
		if (!ifs1.is_open()) {
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "分类类型文件缺失!";
			Log::WriteAsyncLog(m_params.classifyNameFile, ERR, outInfo.paths.logFile, true, "---分类类型文件缺失!");
			return;
		}
		else
		{
			m_params.classifyClassName.clear();
			std::string line;
			while (getline(ifs1, line)) m_params.classifyClassName.push_back(line);
			Log::WriteAsyncLog("分类类型文件读取成功！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

			bool isTopType = (std::find(m_params.classifyClassName.begin(),
				m_params.classifyClassName.end(),
				m_params.topType) != m_params.classifyClassName.end());
			bool isBottomType = (std::find(m_params.classifyClassName.begin(),
				m_params.classifyClassName.end(),
				m_params.bottomType) != m_params.classifyClassName.end());

			if (m_params.topType != "0" && m_params.topType != "不检测")
			{
				if (!isTopType)
				{
					outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "当前选择的盖帽类型不在分类类型文件内!";
					Log::WriteAsyncLog("当前选择的盖帽类型不在分类类型文件内!", ERR, outInfo.paths.logFile, true);
					return;
				}
			}
			if (m_params.bottomType != "0" && m_params.bottomType != "不检测")
			{
				if (!isBottomType)
				{
					outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "当前选择的盖底类型不在分类类型文件内!";
					Log::WriteAsyncLog("当前选择的盖底类型不在分类类型文件内!", ERR, outInfo.paths.logFile, true);
					return;
				}
			}
		}

		bool loadModel = loadAllModels(outInfo, true);
		if (!loadModel) {
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "深度学习模型加载异常!";
			Log::WriteAsyncLog(m_params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "---深度学习模型加载异常!");
			return;
		}

		if (!validateCameraModels(outInfo.system.cameraId)) {
			Log::WriteAsyncLog("相机ID配置错误/模型文件缺失!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "相机ID配置错误/模型文件缺失!";
			throw std::invalid_argument("相机ID配置错误/模型文件缺失!");
		}

		cameraConfigMap[cameraId] = m_params;
	}
	else
	{
		m_params = cameraConfigMap[cameraId];
	}



	if (outInfo.status.statusCode = PRESSCAP_RETURN_OK)
	{
		loadCapConfigSuccess[outInfo.system.cameraId] = true;
		Log::WriteAsyncLog("参数初始化完成!", INFO, outInfo.paths.logFile, true);
	}
	else
	{
		loadCapConfigSuccess[outInfo.system.cameraId] = false;
		Log::WriteAsyncLog("参数初始化失败!", ERR, outInfo.paths.logFile, true);
	}

	if (m_params.saveDebugImage) {
		COM->CreateDir(outInfo.paths.intermediateImagesDir);
	}
	if (m_params.saveResultImage) {
		COM->CreateDir(outInfo.paths.resultsOKDir);
		COM->CreateDir(outInfo.paths.resultsNGDir);
	}
}

InspPressCap::~InspPressCap() {
	if (m_timeoutFlagRef) *m_timeoutFlagRef = true;
}


// 验证摄像头ID对应的模型配置是否存在
bool InspPressCap::validateCameraModels(int cameraId) {
	std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
	return capDetectionModelMap.count("capDetection_" + std::to_string(cameraId)) &&
		capDefectModelMap.count("capDefect_" + std::to_string(cameraId)) &&
		capClassifyModelMap.count("capClassify_" + std::to_string(cameraId));
}

// 加载所有模型到ModelManager
bool InspPressCap::loadAllModels(InspPressCapOut& outInfo, bool ini) {
	if (!ini) {
		Log::WriteAsyncLog("跳过模型加载!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return true;
	}

	const int cameraId = outInfo.system.cameraId;
	const cv::String key = std::to_string(cameraId);

	// 获取当前相机专用模型路径
	std::vector<std::string> cameraModelPaths;

	// 1. 添加检测模型
	std::string detectionKey = "capDetection_" + std::to_string(cameraId);
	if (auto it = capDetectionModelMap.find(detectionKey); it != capDetectionModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	// 2. 添加缺陷模型
	std::string defectKey = "capDefect_" + std::to_string(cameraId);
	if (auto it = capDefectModelMap.find(defectKey); it != capDefectModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	// 3. 添加分类模型
	std::string classifyKey = "capClassify_" + std::to_string(cameraId);
	if (auto it = capClassifyModelMap.find(classifyKey); it != capClassifyModelMap.end()) {
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

		Mat iniImg = Mat::zeros(cv::Size(2500, 2000), CV_8UC3);
		outInfo.locate.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.locateWeightsFile, m_params.locateClassName, iniImg, 0.5, 0.3);
		outInfo.defects.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.defectWeightsFile, m_params.defectClassName, iniImg, 0.1, 0.5);
		outInfo.classification.topType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.classifyWeightsFile, m_params.classifyClassName, iniImg);
		outInfo.classification.topType.className = "";
		Log::WriteAsyncLog("模型初始化完成！", INFO, outInfo.paths.logFile, true);

		return true;
	}
	catch (const std::exception& e) {
		Log::WriteAsyncLog("相机" + std::to_string(cameraId) + "模型加载异常: " + std::string(e.what()), ERR, outInfo.paths.logFile, true);
		return false;
	}
}

// 读取参数的函数
bool InspPressCap::readParams(cv::Mat img, const std::string& filePath, InspPressCapIn& params, InspPressCapOut& outInfo, const std::string& fileName) {
	std::ifstream ifs(filePath.c_str());
	if (!ifs.is_open()) {
		outInfo.status.errorMessage = "config文件丢失!";
		Log::WriteAsyncLog("config文件丢失！", WARNING, outInfo.paths.logFile, true);
		return false;
	}
	std::string line;
	while (!ifs.eof()) {
		//读取行字符串
		//发现"##"为注释，跳过；空行跳过
		//发现“:”，提取关键字；未发现则config异常
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
			outInfo.status.errorMessage = "参数缺失!";
			Log::WriteAsyncLog(keyWord, WARNING, outInfo.paths.logFile, true, " 参数缺失！");
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

		value.erase(0, value.find_first_not_of(" "));  // 去除前后空格
		value.erase(value.find_last_not_of(" ") + 1);
		value.erase(0, value.find_first_not_of(" "));
		value.erase(value.find_last_not_of(" ") + 1);

		std::string keyStr(value.begin(), value.end());

		//是否存储中间图像(0:否  1:是)
		if (keyWord == "PRESS_CAP_SAVE_DEBUG_IMAGE") {
			params.saveDebugImage = std::stoi(value);
		}
		else if (keyWord == "PRESS_CAP_SAVE_RESULT_IMAGE") {
			params.saveResultImage = std::stoi(value);
		}
		else if (keyWord == "PRESS_CAP_SAVE_LOG_TXT") {
			params.saveLogTxt = std::stoi(value);
		}
		else if (keyWord == "PRESS_CAP_DRAW_RESULT") {
			params.drawResult = std::stoi(value);
		}
		else if (keyWord == "PRESS_CAP_SAVE_TRAIN") {
			params.saveTrain = std::stoi(value);
		}
		else if (keyWord == "PRESS_CAP_ROI_X") {
			params.roiRect.x = std::stoi(value);
			if (params.roiRect.x < 0 || params.roiRect.x > img.cols)
			{
				outInfo.status.errorMessage = "ROI_X: 超出图像范围!";
				Log::WriteAsyncLog("ROI_X: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_ROI_Y") {
			params.roiRect.y = std::stoi(value);
			if (params.roiRect.y < 0 || params.roiRect.y > img.rows)
			{
				outInfo.status.errorMessage = "ROI_Y: 超出图像范围!";
				Log::WriteAsyncLog("ROI_Y: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_ROI_W") {
			params.roiRect.width = std::stoi(value);
			if (params.roiRect.x + params.roiRect.width > img.cols)
			{
				outInfo.status.errorMessage = "ROI_X+ROI_W: 超出图像范围!";
				Log::WriteAsyncLog("ROI_X+ROI_W: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
			else if (params.roiRect.width < 10)
			{
				outInfo.status.errorMessage = "ROI_W < 10!";
				Log::WriteAsyncLog("ROI_W < 10!", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_ROI_H") {
			params.roiRect.height = std::stoi(value);
			if (params.roiRect.y + params.roiRect.height > img.rows)
			{
				outInfo.status.errorMessage = "ROI_Y+ROI_H: 超出图像范围!";
				Log::WriteAsyncLog("ROI_Y+ROI_H: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
			else if (params.roiRect.height < 10)
			{
				outInfo.status.errorMessage = "ROI_H < 10!";
				Log::WriteAsyncLog("ROI_H < 10!", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_ROTATE_ANGLE") {
			params.rotateAngle = std::stod(value);
			if (params.rotateAngle < -90 || params.rotateAngle > 90)
			{
				outInfo.status.errorMessage = "矫正角度: 超出范围(-90,90)!";
				Log::WriteAsyncLog("矫正角度: 超出范围(-90,90)！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_TYPE") {
			params.capType = std::stoi(value);
		}
		else if (keyWord == "PRESS_CAP_OUT") {
			params.capOut = std::stoi(value);
		}
		else if (keyWord == "PRESS_CAP_HEIGHT") {
			params.capHeight = std::stoi(value);
			if (params.capHeight < 10 || params.capHeight > 1000)
			{
				outInfo.status.errorMessage = "瓶盖高度: 超出范围(10,1000)!";
				Log::WriteAsyncLog("瓶盖高度: 超出范围(10,1000)！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_TOP_Y") {
			params.capTopY = std::stoi(value);
			if (params.capTopY < 10 || params.capTopY > 10000)
			{
				outInfo.status.errorMessage = "瓶盖盖顶Y坐标: 超出范围(10,10000)!";
				Log::WriteAsyncLog("瓶盖盖顶Y坐标: 超出范围(10,10000)！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_HEIGHT_ERR") {
			params.capHeightErr = std::stoi(value); 
			if (params.capHeightErr < 0 || params.capHeight > 1000)
			{
				outInfo.status.errorMessage = "瓶盖高度偏差阈值: 超出范围(0,1000)!";
				Log::WriteAsyncLog("瓶盖高度偏差阈值: 超出范围(0,1000)！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_TOP_HEIGHT") {
			params.capTopHeight = std::stoi(value);
			if (params.capTopHeight < 10 || params.capTopHeight > 1000)
			{
				outInfo.status.errorMessage = "盖帽高度: 超出范围(10,1000)!";
				Log::WriteAsyncLog("盖帽高度: 超出范围(10,1000)！", ERR, outInfo.paths.logFile, true);
				return false;
			}
			else if (params.capTopHeight > params.capHeight)
			{
				outInfo.status.errorMessage = "盖帽高度不应该大于瓶盖高度!";
				Log::WriteAsyncLog("盖帽高度不应该大于瓶盖高度！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_BOTTOM_HEIGHT") {
			params.capBottomHeight = std::stoi(value);
			if (params.capBottomHeight < 0 || params.capBottomHeight > 1000)
			{
				outInfo.status.errorMessage = "盖底高度: 超出范围(0,1000)!";
				Log::WriteAsyncLog("盖底高度: 超出范围(0,1000)！", ERR, outInfo.paths.logFile, true);
				return false;
			}
			else if (params.capBottomHeight > params.capHeight)
			{
				outInfo.status.errorMessage = "盖底高度不应该大于瓶盖高度!";
				Log::WriteAsyncLog("盖底高度不应该大于瓶盖高度！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_HARDWARE_TYPE") {
			params.hardwareType = std::stoi(value);
		}
		else if (keyWord == "PRESS_CAP_MODEL_TYPE") {
			params.modelType = std::stoi(value);
		}
		else if (keyWord == "PRESS_CAP_LOCATE_WEIGHTS_FILE") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
			std::string camera = std::to_string(outInfo.system.cameraId);
			capDetectionModelMap["capDetection_" + camera] = value;
			params.locateWeightsFile = value;
			if (!COM->FileExistsModern(params.locateWeightsFile))
			{
				outInfo.status.errorMessage = "定位模型文件缺失!";
				Log::WriteAsyncLog(params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "--定位模型文件文件缺失！");
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_LOCATE_CONFIG") {
			params.locateThreshConfig = value;
			if (!COM->FileExistsModern(params.locateThreshConfig))
			{
				outInfo.status.errorMessage = "定位阈值文件缺失!";
				Log::WriteAsyncLog(params.locateThreshConfig, ERR, outInfo.paths.logFile, true, "--定位阈值文件缺失！");
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_DEFECT_WEIGHTS_FILE") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex); // 加锁
			std::string camera = std::to_string(outInfo.system.cameraId);
			capDefectModelMap["capDefect_" + camera] = value;
			params.defectWeightsFile = value;
			if (!COM->FileExistsModern(params.defectWeightsFile))
			{
				outInfo.status.errorMessage = "缺陷模型文件缺失!";
				Log::WriteAsyncLog(params.defectWeightsFile, ERR, outInfo.paths.logFile, true, "--缺陷模型文件缺失！");
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_DEFECT_CONFIG") {
			params.defectThreshConfig = value;
			if (!COM->FileExistsModern(params.defectThreshConfig))
			{
				outInfo.status.errorMessage = "缺陷阈值文件缺失!";
				Log::WriteAsyncLog(params.defectThreshConfig, ERR, outInfo.paths.logFile, true, "--缺陷阈值文件缺失！");
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_CLASSFY_WEIGHTS_FILE") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);  // 加锁
			std::string camera = std::to_string(outInfo.system.cameraId);
			capClassifyModelMap["capClassify_" + camera] = value;
			params.classifyWeightsFile = value;
			if (!COM->FileExistsModern(params.classifyWeightsFile))
			{
				outInfo.status.errorMessage = "分类模型文件缺失!";
				Log::WriteAsyncLog(params.classifyWeightsFile, ERR, outInfo.paths.logFile, true, "--分类模型文件缺失！");
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_CLASSES_FILE") {
			params.classifyNameFile = value;
			if (!COM->FileExistsModern(params.classifyNameFile))
			{
				outInfo.status.errorMessage = "分类类型文件缺失!";
				Log::WriteAsyncLog(params.classifyNameFile, ERR, outInfo.paths.logFile, true, "--分类类型文件缺失！");
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_TOP_TYPE") {
			params.topType = value;
		}
		else if (keyWord == "PRESS_CAP_BOTTOM_TYPE") {
			params.bottomType = value;
		}
		else if (keyWord == "PRESS_CAP_BW_THRESH") {
			params.bwThresh = std::stoi(value);
			if (params.bwThresh < 0 || params.bwThresh > 255)
			{
				outInfo.status.errorMessage = "二值化阈值: 超出范围(0,255)!";
				Log::WriteAsyncLog("二值化阈值: 超出范围(0,255)！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_TOP_LINE_ANGLE") {
			params.topLineAngle = std::stod(value);
			if (params.topLineAngle < -10 || params.topLineAngle > 10)
			{
				outInfo.status.errorMessage = "盖顶角度阈值: 超出范围(-10,10)!";
				Log::WriteAsyncLog("盖顶角度阈值: 超出范围(-10,10)！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_BOTTOM_LINE_ANGLE") {
			params.bottomLineAngle = std::stod(value);
			if (params.bottomLineAngle < -10 || params.bottomLineAngle > 10)
			{
				outInfo.status.errorMessage = "盖底角度阈值: 超出范围(-10,10)!";
				Log::WriteAsyncLog("盖底角度阈值: 超出范围(-10,10)！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_TOP_BOTTOM_ANGLE_DIF") {
			params.topBottomAngleDif = std::stod(value);
			if (params.topBottomAngleDif < -10 || params.topBottomAngleDif > 10)
			{
				outInfo.status.errorMessage = "盖顶与支撑环角度差阈值: 超出范围(-10,10)!";
				Log::WriteAsyncLog("盖顶与支撑环角度差阈值: 超出范围(-10,10)！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "PRESS_CAP_LEAK") {
			params.leakThresh = std::stod(value);
			if (params.leakThresh < 0 || params.leakThresh > 1000)
			{
				outInfo.status.errorMessage = "压盖不严阈值: 超出范围(0,1000)!";
				Log::WriteAsyncLog("压盖不严阈值: 超出范围(0,1000)！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
	}

	ifs.close();
	return true;
}

//void processImageOptimized(cv::Mat& imgRoiBwClose) {
//#pragma omp parallel for
//	for (int y = 0; y < imgRoiBwClose.rows; ++y) {
//		uchar* row = imgRoiBwClose.ptr<uchar>(y);
//		int count = 0;
//		int start_x = 0;
//
//		for (int x = 0; x < imgRoiBwClose.cols; ++x) {
//			if (row[x] == 255) {
//				if (count == 0) start_x = x;
//				count++;
//			}
//			else {
//				if (count > 0 && count < 100) {
//					std::fill(row + start_x, row + x, 0);
//				}
//				count = 0;
//			}
//		}
//
//		if (count > 0 && count < 100) {
//			std::fill(row + start_x, row + imgRoiBwClose.cols, 0);
//		}
//	}
//}

void processImageOptimized(const cv::Mat& imgRoiBwClose) {
	cv::Mat processed = imgRoiBwClose.clone();
#pragma omp parallel for
	for (int y = 0; y < processed.rows; ++y) {
		uchar* row = processed.ptr<uchar>(y);
		int count = 0;
		int start_x = 0;

		for (int x = 0; x < processed.cols; ++x) {
			if (row[x] == 255) {
				if (count == 0) {
					start_x = x; // 记录连续段的起始位置
				}
				count++;
			}
			else {
				if (count > 0 && count < 100) {
					memset(row + start_x, 0, count); // 批量清除
				}
				count = 0;
			}
		}

		// 处理行末尾的连续段
		if (count > 0 && count < 100) {
			memset(row + start_x, 0, count);
		}
	}
}

void processImage(cv::Mat& imgRoiBwClose) {
	// 遍历每一行
	for (int y = 0; y < imgRoiBwClose.rows; ++y) {
		int count = 0; // 计数连续的255点
		for (int x = 0; x < imgRoiBwClose.cols; ++x) {
			if (imgRoiBwClose.at<uchar>(y, x) == 255) {
				count++; // 找到一个连续的点
			}
			else {
				// 如果遇到0，检查连续点数
				if (count > 0 && count < 100) {
					// 将连续点数小于100的点设置为0
					for (int i = x - count; i < x; ++i) {
						imgRoiBwClose.at<uchar>(y, i) = 0; // 设置为黑色
					}
				}
				count = 0; // 重置计数
			}
		}
		// 处理行末尾的连续点
		if (count > 0 && count < 100) {
			for (int i = imgRoiBwClose.cols - count; i < imgRoiBwClose.cols; ++i) {
				imgRoiBwClose.at<uchar>(y, i) = 0; // 设置为黑色
			}
		}
	}
}

void InspPressCap::PressCap_RotateImg(InspPressCapOut& outInfo) {
	if (m_params.rotateAngle == 0)
	{
		return;
	}

	Point rotateCenter;
	rotateCenter.x = m_img.cols * 0.5;
	rotateCenter.y = m_img.rows * 0.5;

	ANA->RotateImg(m_img, m_img, rotateCenter, -m_params.rotateAngle);
	DAS->DAS_Img(m_imgRotate, outInfo.paths.intermediateImagesDir + "0. m_imgRotate.jpg ", m_params.saveDebugImage);
}

void InspPressCap::PressCap_SetROI(InspPressCapOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != PRESSCAP_RETURN_OK) {
		Log::WriteAsyncLog("跳过ROI区域获取!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始ROI区域获取!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	outInfo.images.roi.data = std::make_shared<cv::Mat>(m_img(m_params.roiRect).clone());
	outInfo.images.roi.stageName = "PressCap_Main";
	outInfo.images.roi.description = "ROI区域获取";
	outInfo.images.roi.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

	outInfo.images.roiLog.data = std::make_shared<cv::Mat>(m_img.clone());
	outInfo.images.roiLog.stageName = "PressCap_Main";
	outInfo.images.roiLog.description = "ROI_LOG绘制: " + std::to_string(m_params.saveDebugImage);
	outInfo.images.roiLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
	DAS->DAS_Rect(outInfo.images.roiLog.mat(), m_params.roiRect, outInfo.paths.intermediateImagesDir + "1.0.0.roiRect.jpg", m_params.saveDebugImage);

}

void InspPressCap::PressCap_LocateCap(InspPressCapOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != PRESSCAP_RETURN_OK) {
		Log::WriteAsyncLog("跳过瓶盖定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始瓶盖定位!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (m_params.locateWeightsFile.find(".onnx") != std::string::npos)
	{
		outInfo.locate.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.locateWeightsFile, m_params.locateClassName, outInfo.images.roi.mat(), 0.1, 0.3);
	}
	else
	{
		outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}
	if (m_params.saveDebugImage)
	{
		outInfo.images.capRegionDetectLog.data = std::make_shared<cv::Mat>(outInfo.images.roi.mat().clone());
		outInfo.images.capRegionDetectLog.stageName = "PressCap_LocateCap";
		outInfo.images.capRegionDetectLog.description = "Locate绘制: " + std::to_string(m_params.saveDebugImage);
		outInfo.images.capRegionDetectLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
		DAS->DAS_FinsObject(outInfo.images.capRegionDetectLog.mat(), outInfo.locate.details, outInfo.paths.intermediateImagesDir + "2.1.1.detections.jpg", m_params.saveDebugImage);
	}

	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{
		outInfo.locate.details[i].box = ANA->AdjustROI(outInfo.locate.details[i].box, outInfo.images.roi.mat());
		outInfo.locate.details[i].box.x += m_params.roiRect.x;
		outInfo.locate.details[i].box.y += m_params.roiRect.y;
	}

	Log::WriteAsyncLog("开始分析定位结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	for (int i = outInfo.locate.details.size() - 1; i >= 0; --i)
	{
		auto& locate = outInfo.locate.details[i];
		int paramIndex = -1; // 根据缺陷类别设置对应参数索引

		bool valid = true;
		if (locate.className == "瓶盖")paramIndex = 1;
		else if (locate.className == "无上盖")   paramIndex = 2;
		else if (locate.className == "缺陷盖")  paramIndex = 3; 
		else if (locate.className == "无盖")	paramIndex = 0; 

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

	std::vector<FinsObject> detectionsFil;  // 检测结果
	bool findNC = false;
	bool findNT = false;
	bool findBC = false;
	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{

		if (outInfo.locate.details[i].className == "无盖")
		{
			findNC = true;
		}
		else if (outInfo.locate.details[i].className == "瓶盖")
		{
			detectionsFil.push_back(outInfo.locate.details[i]);
		}
		else if (outInfo.locate.details[i].className == "无上盖")
		{
			findNT = true;
		}
		else if (outInfo.locate.details[i].className == "缺陷盖")
		{
			findBC = true;
		}
	}

	if (findNC)
	{
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/NC");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/NC/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/NC/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg", m_img);
		}

		outInfo.status.errorMessage = "定位-无盖!";
		Log::WriteAsyncLog("定位-无盖!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = PRESSCAP_RETURN_NO_CAP;
		return;
	}
	if (findNT)
	{
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/NCT");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/NCT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/NCT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg", m_img);
		}
		outInfo.status.errorMessage = "定位-无上盖!";
		Log::WriteAsyncLog("定位-无上盖!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = PRESSCAP_RETURN_NO_CAP_TOP;
		return;
	}
	else
		if (findBC)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/BC");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg",
					m_img.rows,
					m_img.cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/BC/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/BC/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg", m_img);
			}
			outInfo.status.errorMessage = "定位-缺陷盖!";
			Log::WriteAsyncLog("定位-缺陷盖!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_CAP_SCRAP;
			return;
		}

	if (detectionsFil.empty()) {

		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/OUT");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/OUT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/OUT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg", m_img);
		}
		outInfo.status.errorMessage = "定位-无目标!";
		Log::WriteAsyncLog("定位-无目标!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = PRESSCAP_RETURN_CAP_CLOSE_LR_BOUNDARY;
		return;
	}
	else if (detectionsFil.size() > 1)
	{
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/MULTY");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg", m_img);
		}
		outInfo.status.errorMessage = "多个瓶盖!";
		Log::WriteAsyncLog("多个瓶盖!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = PRESSCAP_RETURN_OTHER;
		return;
	}
	else
	{
		if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
		{
			
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/OK");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE.jpg", m_img);
		}
		outInfo.geometry.capRect = detectionsFil[0].box;
		Log::WriteAsyncLog("定位瓶盖成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	if (m_params.saveDebugImage)
	{
		outInfo.images.capRegionDetectFilLog.data = std::make_shared<cv::Mat>(outInfo.images.roi.mat().clone());
		outInfo.images.capRegionDetectFilLog.stageName = "PressCap_LocateCap";
		outInfo.images.capRegionDetectFilLog.description = "Locate_Fil绘制: " + std::to_string(m_params.saveDebugImage);
		outInfo.images.capRegionDetectFilLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
		DAS->DAS_FinsObject(outInfo.images.capRegionDetectFilLog.mat(), outInfo.locate.details, outInfo.paths.intermediateImagesDir + "2.1.2.detectionsFil.jpg", m_params.saveDebugImage);
	}


	outInfo.geometry.capRect = ANA->AdjustROI(outInfo.geometry.capRect, m_img);

	outInfo.images.capRegion.data = std::make_shared<cv::Mat>(m_img(outInfo.geometry.capRect).clone());
	outInfo.images.capRegion.stageName = "PressCap_LocateCap";
	outInfo.images.capRegion.description = "capRegion定位";
	outInfo.images.capRegion.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
	DAS->DAS_Img(outInfo.images.capRegion.mat(), outInfo.paths.intermediateImagesDir + "2.2.1.capRegion.jpg", m_params.saveDebugImage);


}

void InspPressCap::PressCap_CheckDefect(InspPressCapOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != PRESSCAP_RETURN_OK) {
		Log::WriteAsyncLog("跳过缺陷检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else if (m_params.capType == 2) {
		Log::WriteAsyncLog("跳过缺陷检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始缺陷检测!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	if (m_params.defectWeightsFile.find(".onnx") != std::string::npos)
	{
		outInfo.defects.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.defectWeightsFile, m_params.defectClassName, outInfo.images.capRegion.mat(), 0.1, 0.5);
	}
	else
	{
		outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}

	if (m_params.saveDebugImage)
	{
		outInfo.images.capRegionDefectLog.data = std::make_shared<cv::Mat>(outInfo.images.capRegion.mat().clone());
		outInfo.images.capRegionDefectLog.stageName = "PressCap_CheckDefect";
		outInfo.images.capRegionDefectLog.description = "缺陷定位";
		outInfo.images.capRegionDefectLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
		DAS->DAS_FinsObject(outInfo.images.capRegionDefectLog.mat(), outInfo.locate.details, outInfo.paths.intermediateImagesDir + "3.1.1.detections.jpg", m_params.saveDebugImage);
	}

	//
	/*  0 word_LRP
		1 word_BARERR
		2 word_BARB
		3 word_BARD
		4 word_LEAK
		5 word_CRIMP*/
	Log::WriteAsyncLog("开始分析缺陷检测结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	std::vector<cv::Rect> lrpRects;
	for (int i = outInfo.defects.details.size() - 1; i >= 0; --i)
	{
		auto& defect = outInfo.defects.details[i];
		int paramIndex = -1; // 根据缺陷类别设置对应参数索引

		bool valid = true;
		if (defect.className == "瓶盖破损") paramIndex = 5;  // 瓶盖破损
		else if (defect.className == "防盗环缺陷")paramIndex = 1;  // 防盗环缺陷
		else if (defect.className == "防盗环断桥")   paramIndex = 2;  // 防盗环断桥
		else if (defect.className == "上下盖分离")  paramIndex = 3;  // 上下盖分离
		else if (defect.className == "压盖不严") paramIndex = 4;  // 压盖不严
		else if (defect.className == "支撑环端点")	paramIndex = 0;  // 支撑环端点

		if (paramIndex != -1)
		{
			auto& para = m_params.defectPara[paramIndex];
			if (defect.box.width < para.widthRange[0] ||
				defect.box.width > para.widthRange[1] ||
				defect.box.height < para.heightRange[0] ||
				defect.box.height > para.heightRange[1] ||
				defect.confidence < para.confidenceThresh)
			{
				valid = false;
			}
		}

		if (!valid) {
			// 移除不符合条件的缺陷
			outInfo.defects.details.erase(outInfo.defects.details.begin() + i);

		}
	}

	for (int i = 0; i < outInfo.defects.details.size(); i++)
	{
		if (outInfo.defects.details[i].className == "瓶盖破损")
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				
				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/瓶盖破损/");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.defects.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg",
					outInfo.images.capRegion.mat().rows,
					outInfo.images.capRegion.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/瓶盖破损/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.json");
				cv::imwrite(outInfo.paths.trainDir + "DEFECT/瓶盖破损/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg", outInfo.images.capRegion.mat());

			}
			outInfo.defects.findCRIMP = true;
			outInfo.status.errorMessage = "缺陷-瓶盖破损!";
			Log::WriteAsyncLog("缺陷-瓶盖破损!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_CAP_CRIMP;
			return;
		}
		else if (outInfo.defects.details[i].className == "防盗环缺陷")
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				
				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/防盗环缺陷/");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.defects.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg",
					outInfo.images.capRegion.mat().rows,
					outInfo.images.capRegion.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/防盗环缺陷/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.json");
				cv::imwrite(outInfo.paths.trainDir + "DEFECT/防盗环缺陷/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg", outInfo.images.capRegion.mat());

			}
			outInfo.defects.findBARERR = true;
			outInfo.status.errorMessage = "缺陷-防盗环缺陷!";
			Log::WriteAsyncLog("缺陷-防盗环缺陷!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_BAR_BREAK;
			return;
		}
		else if (outInfo.defects.details[i].className == "防盗环断桥")
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				
				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/防盗环断桥/");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.defects.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg",
					outInfo.images.capRegion.mat().rows,
					outInfo.images.capRegion.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/防盗环断桥/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.json");
				cv::imwrite(outInfo.paths.trainDir + "DEFECT/防盗环断桥/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg", outInfo.images.capRegion.mat());

			}
			outInfo.defects.findBARB = true;
			outInfo.status.errorMessage = "缺陷-防盗环断桥!";
			Log::WriteAsyncLog("缺陷-防盗环断桥!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_BAR_BRIDGE_BREAK;
			return;
		}
		else if (outInfo.defects.details[i].className == "上下盖分离")
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				
				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/上下盖分离/");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.defects.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg",
					outInfo.images.capRegion.mat().rows,
					outInfo.images.capRegion.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/上下盖分离/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.json");
				cv::imwrite(outInfo.paths.trainDir + "DEFECT/上下盖分离/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg", outInfo.images.capRegion.mat());

			}
			outInfo.defects.findBARD = true;
			outInfo.status.errorMessage = "缺陷-上下盖分离!";
			Log::WriteAsyncLog("缺陷-上下盖分离!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_BAR_CAP_SEP;
			return;
		}
		else if (outInfo.defects.details[i].className == "压盖不严")
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				
				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/压盖不严/");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.defects.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg",
					outInfo.images.capRegion.mat().rows,
					outInfo.images.capRegion.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/压盖不严/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.json");
				cv::imwrite(outInfo.paths.trainDir + "DEFECT/压盖不严/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg", outInfo.images.capRegion.mat());

			}
			outInfo.defects.findLEAK = true;
			outInfo.status.errorMessage = "缺陷-压盖不严!";
			Log::WriteAsyncLog("缺陷-压盖不严!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_LEAK;
			return;
		}
		else if (outInfo.defects.details[i].className == "支撑环端点")
		{
			lrpRects.push_back(outInfo.defects.details[i].box);
		}
	}

	if (lrpRects.size() < 2) {
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			
			COM->CreateDir(outInfo.paths.trainDir + "DEFECT/OUT/");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.defects.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg",
				outInfo.images.capRegion.mat().rows,
				outInfo.images.capRegion.mat().cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/OUT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.json");
			cv::imwrite(outInfo.paths.trainDir + "DEFECT/OUT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg", outInfo.images.capRegion.mat());


		}
		outInfo.status.errorMessage = "缺陷-支撑环端点定位异常!";
		Log::WriteAsyncLog("缺陷-支撑环端点定位异常!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = PRESSCAP_RETURN_LR_FAILED;
		return;
	}
	else if (lrpRects.size() > 2) {
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			
			COM->CreateDir(outInfo.paths.trainDir + "DEFECT/MULTY/");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.defects.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg",
				outInfo.images.capRegion.mat().rows,
				outInfo.images.capRegion.mat().cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.json");
			cv::imwrite(outInfo.paths.trainDir + "DEFECT/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg", outInfo.images.capRegion.mat());

		}
		outInfo.status.errorMessage = "缺陷-支撑环端点定位异常!";
		Log::WriteAsyncLog("缺陷-支撑环端点定位异常!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = PRESSCAP_RETURN_LR_FAILED;
		return;
	}
	else {

		if (abs(lrpRects[0].x - lrpRects[1].x) < outInfo.images.capRegion.mat().cols / 3)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				
				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/OUT/");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.defects.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg",
					outInfo.images.capRegion.mat().rows,
					outInfo.images.capRegion.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/OUT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.json");
				cv::imwrite(outInfo.paths.trainDir + "DEFECT/OUT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg", outInfo.images.capRegion.mat());

			}
			outInfo.status.errorMessage = "缺陷-支撑环端点定位异常!";
			Log::WriteAsyncLog("缺陷-支撑环端点定位异常!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_LR_FAILED;
			return;
		}
		else
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
			{
				
				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/OK/");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.defects.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg",
					outInfo.images.capRegion.mat().rows,
					outInfo.images.capRegion.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.json");
				cv::imwrite(outInfo.paths.trainDir + "DEFECT/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_DEFECT.jpg", outInfo.images.capRegion.mat());


			}
			Log::WriteAsyncLog("正常定位到支撑环端点!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}
		if (lrpRects[0].x < lrpRects[1].x)
		{
			outInfo.geometry.capNeckLeftRect = lrpRects[0];
			outInfo.geometry.capNeckRightRect = lrpRects[1];
		}
		else
		{
			outInfo.geometry.capNeckLeftRect = lrpRects[1];
			outInfo.geometry.capNeckRightRect = lrpRects[0];
		}
		outInfo.geometry.leftBottomPoint.x = outInfo.geometry.capNeckLeftRect.x;
		outInfo.geometry.leftBottomPoint.y = outInfo.geometry.capNeckLeftRect.y;
		outInfo.geometry.rightBottomPoint.x = outInfo.geometry.capNeckRightRect.x + outInfo.geometry.capNeckRightRect.width;
		outInfo.geometry.rightBottomPoint.y = outInfo.geometry.capNeckRightRect.y;


		CALC_LinePara bottomLine;
		CAL->CALC_Line(outInfo.geometry.leftBottomPoint, outInfo.geometry.rightBottomPoint, bottomLine);
		if (bottomLine.angle > 90) {
			outInfo.geometry.bottomAngle = bottomLine.angle - 180;
		}
		else {
			outInfo.geometry.bottomAngle = bottomLine.angle;
		}

	}
}

void InspPressCap::PressCap_LocateTopBottom(InspPressCapOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != PRESSCAP_RETURN_OK) {
		Log::WriteAsyncLog("跳过盖帽盖底定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始盖帽盖底定位！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (!m_params.capOut)
	{
		Log::WriteAsyncLog("转换灰度图像！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		cv::Mat imgGray;
		cv::cvtColor(outInfo.images.capRegion.mat(), imgGray, cv::COLOR_BGR2GRAY);
		outInfo.images.capRegionGrayConverted.data = std::make_shared<cv::Mat>(imgGray);
		outInfo.images.capRegionGrayConverted.stageName = "PressCap_LocateTop";
		outInfo.images.capRegionGrayConverted.description = "capRegion灰度转换";
		outInfo.images.capRegionGrayConverted.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
		DAS->DAS_Img(outInfo.images.capRegionGrayConverted.mat(), outInfo.paths.intermediateImagesDir + "4.1.1.capRegionGrayConverted.jpg", m_params.saveDebugImage);

		Log::WriteAsyncLog("二值化！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		cv::Mat imgBw;
		threshold(outInfo.images.capRegionGrayConverted.mat(), imgBw, m_params.bwThresh, 255, cv::THRESH_BINARY_INV);
		outInfo.images.capRegionBinarized.data = std::make_shared<cv::Mat>(imgBw);
		outInfo.images.capRegionBinarized.stageName = "PressCap_LocateTop";
		outInfo.images.capRegionBinarized.description = "capRegion二值化";
		outInfo.images.capRegionBinarized.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
		DAS->DAS_Img(outInfo.images.capRegionBinarized.mat(), outInfo.paths.intermediateImagesDir + "4.1.2.capRegionBinarized.jpg", m_params.saveDebugImage);

		// 处理图像
		Log::WriteAsyncLog("预处理！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

		// 处理图像
		cv::Mat imgRoiBwFil = imgBw.clone();
		std::vector<std::vector<cv::Point>> contours;
		findContours(imgRoiBwFil, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
		if (contours.empty()) {
			Log::WriteAsyncLog("二值化阈值设置错误!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "二值化阈值设置错误!";
			return;
		}
		processImage(imgRoiBwFil);
		DAS->DAS_Counters(imgRoiBwFil, contours, outInfo.paths.intermediateImagesDir + "4.1.3.imgRoiBwFil.jpg", m_params.saveDebugImage);


		cv::Mat nonZero;
		cv::findNonZero(imgRoiBwFil, nonZero);

		int cntY = 0;
		for (int x = 0; x < imgBw.cols; ++x) {
			if (imgBw.at<uchar>(0, x) == 255) { // 检查灰度值
				cntY++; // 找到至少一个255像素			
			}
		}
		if (cntY > imgBw.cols * 0.4)
		{
			Log::WriteAsyncLog("靠近上边界!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_CAP_CLOSE_TOP_BOUNDARY;
			outInfo.status.errorMessage = "靠近上边界!";
			return;
		}




		Log::WriteAsyncLog("定位盖顶边缘！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		std::vector<cv::Point> topPoints;
		bool rv = ANA->FindPointUpToDownOne(imgRoiBwFil, 0, 0, imgRoiBwFil.cols - 1, imgRoiBwFil.rows / 2, topPoints);
		if (rv == false)
		{
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "二值化阈值设置错误!";
			Log::WriteAsyncLog("二值化阈值设置错误!", ERR, outInfo.paths.logFile, true);
			return;
		}
		DAS->DAS_Points(outInfo.images.capRegionGrayConverted.mat(), topPoints, outInfo.paths.intermediateImagesDir + "4.3.2.topPoints.jpg", m_params.saveDebugImage);
		CAL->CALC_RemovePointsByDir(outInfo.images.capRegionGrayConverted.mat(), 45, topPoints);
		DAS->DAS_Points(outInfo.images.capRegionGrayConverted.mat(), topPoints, outInfo.paths.intermediateImagesDir + "4.3.3.topPointsFilterByDir.jpg", m_params.saveDebugImage);
		CALC_LinePara upLine;
		std::vector<cv::Point> innerUpPts;
		CAL->CALC_FitLineByPts(topPoints, 0.7, MAX(m_params.capTopHeight >> 4, 50), upLine, innerUpPts);
		DAS->DAS_Points(outInfo.images.capRegionGrayConverted.mat(), innerUpPts, outInfo.paths.intermediateImagesDir + "4.3.4.topPointsFilterByLine.jpg", m_params.saveDebugImage);
		ANA->DeleteDiscretePts(innerUpPts, outInfo.geometry.contourTop);
		DAS->DAS_Points(outInfo.images.capRegionGrayConverted.mat(), outInfo.geometry.contourTop, outInfo.paths.intermediateImagesDir + "4.3.5.topPoints.jpg", m_params.saveDebugImage);

		// check cap top pos
		int minX = 100000;
		cv::Point minXPoint;
		int minY = 100000;
		cv::Point minYPoint;
		int maxX = 0;
		cv::Point maxXPoint;
		int maxY = 0;
		cv::Point maxYPoint;
		ANA->FindPointsXY(outInfo.geometry.contourTop, minX, minXPoint, minY, minYPoint, maxX, maxXPoint, maxY, maxYPoint);
		outInfo.geometry.leftTopPoint = minXPoint;
		outInfo.geometry.rightTopPoint = maxXPoint;
		outInfo.geometry.topPoint = minYPoint;

		//top angle
		if (upLine.angle > 90) {
			outInfo.geometry.topAngle = upLine.angle - 180;
		}
		else {
			outInfo.geometry.topAngle = upLine.angle;
		}

		//calculate
		outInfo.geometry.topBottomAngleDif = fabs(outInfo.geometry.bottomAngle - outInfo.geometry.topAngle);
		outInfo.geometry.capHeight = (outInfo.geometry.leftBottomPoint.y + outInfo.geometry.rightBottomPoint.y) / 2 - outInfo.geometry.topPoint.y;
		outInfo.geometry.capHeighttDeviation = outInfo.geometry.capHeight - m_params.capHeight;
		Log::WriteAsyncLog("瓶盖高度:", INFO, outInfo.paths.logFile, m_params.saveLogTxt, outInfo.geometry.capHeight);
		Log::WriteAsyncLog("瓶盖高度偏差:", INFO, outInfo.paths.logFile, m_params.saveLogTxt, outInfo.geometry.capHeighttDeviation);

		outInfo.geometry.capTopRect.x = outInfo.geometry.leftTopPoint.x;
		outInfo.geometry.capTopRect.y = outInfo.geometry.topPoint.y;
		outInfo.geometry.capTopRect.width = outInfo.geometry.rightTopPoint.x - outInfo.geometry.leftTopPoint.x;
		outInfo.geometry.capTopRect.height = m_params.capTopHeight;

		outInfo.geometry.capBottomRect.x = outInfo.geometry.leftBottomPoint.x;
		outInfo.geometry.capBottomRect.y = (outInfo.geometry.leftBottomPoint.y + outInfo.geometry.rightBottomPoint.y) / 2 - m_params.capBottomHeight;
		outInfo.geometry.capBottomRect.width = outInfo.geometry.rightBottomPoint.x - outInfo.geometry.leftBottomPoint.x;
		outInfo.geometry.capBottomRect.height = m_params.capBottomHeight;

		if (m_params.saveDebugImage)
		{
			outInfo.images.capTopBottomRectLog.data = std::make_shared<cv::Mat>(outInfo.images.capRegion.mat().clone());
			outInfo.images.capTopBottomRectLog.stageName = "PressCap_LocateTopBottom";
			outInfo.images.capTopBottomRectLog.description = "盖帽盖底定位";
			outInfo.images.capTopBottomRectLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
			DAS->DAS_Rect(outInfo.images.capTopBottomRectLog.mat(), m_params.saveDebugImage, outInfo.paths.intermediateImagesDir + "4.4.1.topPoints.jpg", outInfo.geometry.capTopRect, outInfo.geometry.capBottomRect);
		}
		if (m_params.capHeightErr == 0)
		{
			Log::WriteAsyncLog("瓶盖高度偏差阈值设置为0，跳过瓶盖高度检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		}
		else
		{
			if (outInfo.geometry.capHeighttDeviation > m_params.capHeightErr)
			{
				outInfo.status.statusCode = PRESSCAP_RETURN_FIND_HIGH_CAP;
				outInfo.status.errorMessage = "瓶盖高度偏差-高盖!";
				Log::WriteAsyncLog("瓶盖高度偏差-高盖!!", ERR, outInfo.paths.logFile, true);
				return;
			}
			else if (outInfo.geometry.capHeighttDeviation < -m_params.capHeightErr)
			{
				outInfo.status.statusCode = PRESSCAP_RETURN_FIND_LOW_CAP;
				outInfo.status.errorMessage = "瓶盖高度偏差-矮盖！";
				Log::WriteAsyncLog("瓶盖高度偏差-矮盖!!", ERR, outInfo.paths.logFile, true);
				return;
			}
		}
	}
	else
	{
		Log::WriteAsyncLog("设定瓶盖会超出背光源，不能检测瓶盖高度和盖顶角度!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		Log::WriteAsyncLog("不检测瓶盖高度和盖顶角度!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);

		if (m_params.capBottomHeight != 0)
		{
			outInfo.geometry.capBottomRect.x = outInfo.geometry.leftBottomPoint.x;
			outInfo.geometry.capBottomRect.y = (outInfo.geometry.leftBottomPoint.y + outInfo.geometry.rightBottomPoint.y) / 2 - m_params.capBottomHeight;
			outInfo.geometry.capBottomRect.width = outInfo.geometry.rightBottomPoint.x - outInfo.geometry.leftBottomPoint.x;
			outInfo.geometry.capBottomRect.height = m_params.capBottomHeight;
			outInfo.geometry.capBottomRect = ANA->AdjustROI(outInfo.geometry.capBottomRect, outInfo.images.capRegion.mat());

			if (outInfo.geometry.capBottomRect.x < 0 ||
				outInfo.geometry.capBottomRect.y < 0 ||
				outInfo.geometry.capBottomRect.width <= 0 ||
				outInfo.geometry.capBottomRect.height <= 0)
			{
				outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "瓶盖高度设置错误或者ROI设置错误！";
				Log::WriteAsyncLog("瓶盖高度设置错误或者ROI设置错误！", ERR, outInfo.paths.logFile, true);
				return;
			}
		}


		outInfo.geometry.capTopRect.x = outInfo.geometry.capNeckLeftRect.x + outInfo.geometry.capNeckLeftRect.width;
		outInfo.geometry.capTopRect.y = outInfo.geometry.capNeckLeftRect.y + -m_params.capHeight;
		outInfo.geometry.capTopRect.width = outInfo.geometry.capNeckRightRect.x - outInfo.geometry.capTopRect.x;
		outInfo.geometry.capTopRect.height = m_params.capTopHeight;
		outInfo.geometry.capTopRect = ANA->AdjustROI(outInfo.geometry.capTopRect, outInfo.images.capRegion.mat());


		if (outInfo.geometry.capTopRect.x < 0 ||
			outInfo.geometry.capTopRect.y < 0 ||
			outInfo.geometry.capTopRect.width <= 0 ||
			outInfo.geometry.capTopRect.height <= 0)
		{
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "瓶盖高度设置错误或者ROI设置错误！";
			Log::WriteAsyncLog("瓶盖高度设置错误或者ROI设置错误！", ERR, outInfo.paths.logFile, true);
			return;
		}

	}


}

void InspPressCap::PressCap_LocateTopBottom_CapTpye2(InspPressCapOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != PRESSCAP_RETURN_OK) {
		Log::WriteAsyncLog("跳过盖帽盖底定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始盖帽盖底定位！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (!m_params.capOut)
	{
		Log::WriteAsyncLog("转换灰度图像！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		cv::Mat imgGray;
		cv::cvtColor(outInfo.images.capRegion.mat(), imgGray, cv::COLOR_BGR2GRAY);
		outInfo.images.capRegionGrayConverted.data = std::make_shared<cv::Mat>(imgGray);
		outInfo.images.capRegionGrayConverted.stageName = "PressCap_LocateTop";
		outInfo.images.capRegionGrayConverted.description = "capRegion灰度转换";
		outInfo.images.capRegionGrayConverted.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
		DAS->DAS_Img(outInfo.images.capRegionGrayConverted.mat(), outInfo.paths.intermediateImagesDir + "4.1.1.capRegionGrayConverted.jpg", m_params.saveDebugImage);

		Log::WriteAsyncLog("二值化！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		cv::Mat imgBw;
		threshold(outInfo.images.capRegionGrayConverted.mat(), imgBw, m_params.bwThresh, 255, cv::THRESH_BINARY_INV);
		outInfo.images.capRegionBinarized.data = std::make_shared<cv::Mat>(imgBw);
		outInfo.images.capRegionBinarized.stageName = "PressCap_LocateTop";
		outInfo.images.capRegionBinarized.description = "capRegion二值化";
		outInfo.images.capRegionBinarized.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
		DAS->DAS_Img(outInfo.images.capRegionBinarized.mat(), outInfo.paths.intermediateImagesDir + "4.1.2.capRegionBinarized.jpg", m_params.saveDebugImage);

		// 处理图像
		Log::WriteAsyncLog("预处理！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

		// 处理图像
		cv::Mat imgRoiBwFil = imgBw.clone();
		std::vector<std::vector<cv::Point>> contours;
		findContours(imgRoiBwFil, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
		if (contours.empty()) {
			Log::WriteAsyncLog("二值化阈值设置错误!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "二值化阈值设置错误!";
			return;
		}
		processImage(imgRoiBwFil);
		DAS->DAS_Counters(imgRoiBwFil, contours, outInfo.paths.intermediateImagesDir + "4.1.3.imgRoiBwFil.jpg", m_params.saveDebugImage);


		cv::Mat nonZero;
		cv::findNonZero(imgRoiBwFil, nonZero);

		int cntY = 0;
		for (int x = 0; x < imgBw.cols; ++x) {
			if (imgBw.at<uchar>(0, x) == 255) { // 检查灰度值
				cntY++; // 找到至少一个255像素			
			}
		}
		if (cntY > imgBw.cols * 0.4)
		{
			Log::WriteAsyncLog("靠近上边界!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_CAP_CLOSE_TOP_BOUNDARY;
			outInfo.status.errorMessage = "靠近上边界!";
			return;
		}




		Log::WriteAsyncLog("定位盖顶边缘！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		std::vector<cv::Point> topPoints;
		bool rv = ANA->FindPointUpToDownOne(imgRoiBwFil, 0, 0, imgRoiBwFil.cols - 1, imgRoiBwFil.rows / 2, topPoints);
		if (rv == false)
		{
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "二值化阈值设置错误!";
			Log::WriteAsyncLog("二值化阈值设置错误!", ERR, outInfo.paths.logFile, true);
			return;
		}
		DAS->DAS_Points(outInfo.images.capRegionGrayConverted.mat(), topPoints, outInfo.paths.intermediateImagesDir + "4.3.2.topPoints.jpg", m_params.saveDebugImage);
		CAL->CALC_RemovePointsByDir(outInfo.images.capRegionGrayConverted.mat(), 45, topPoints);
		DAS->DAS_Points(outInfo.images.capRegionGrayConverted.mat(), topPoints, outInfo.paths.intermediateImagesDir + "4.3.3.topPointsFilterByDir.jpg", m_params.saveDebugImage);
		CALC_LinePara upLine;
		std::vector<cv::Point> innerUpPts;
		CAL->CALC_FitLineByPts(topPoints, 0.7, MAX(m_params.capTopHeight >> 4, 50), upLine, innerUpPts);
		DAS->DAS_Points(outInfo.images.capRegionGrayConverted.mat(), innerUpPts, outInfo.paths.intermediateImagesDir + "4.3.4.topPointsFilterByLine.jpg", m_params.saveDebugImage);
		ANA->DeleteDiscretePts(innerUpPts, outInfo.geometry.contourTop);
		DAS->DAS_Points(outInfo.images.capRegionGrayConverted.mat(), outInfo.geometry.contourTop, outInfo.paths.intermediateImagesDir + "4.3.5.topPoints.jpg", m_params.saveDebugImage);

		// check cap top pos
		int minX = 100000;
		cv::Point minXPoint;
		int minY = 100000;
		cv::Point minYPoint;
		int maxX = 0;
		cv::Point maxXPoint;
		int maxY = 0;
		cv::Point maxYPoint;
		ANA->FindPointsXY(outInfo.geometry.contourTop, minX, minXPoint, minY, minYPoint, maxX, maxXPoint, maxY, maxYPoint);
		outInfo.geometry.leftTopPoint = minXPoint;
		outInfo.geometry.rightTopPoint = maxXPoint;
		outInfo.geometry.topPoint = minYPoint;

		//top angle
		if (upLine.angle > 90) {
			outInfo.geometry.topAngle = upLine.angle - 180;
		}
		else {
			outInfo.geometry.topAngle = upLine.angle;
		}
		 
		//calculate
		outInfo.geometry.topBottomAngleDif = fabs(outInfo.geometry.bottomAngle - outInfo.geometry.topAngle);
		outInfo.geometry.capTopY = m_params.roiRect.y + outInfo.geometry.capRect.y + outInfo.geometry.topPoint.y;
		outInfo.geometry.capHeighttDeviation = outInfo.geometry.capTopY - m_params.capTopY;
		Log::WriteAsyncLog("瓶盖盖顶Y坐标:", INFO, outInfo.paths.logFile, m_params.saveLogTxt, outInfo.geometry.capTopY);
		Log::WriteAsyncLog("瓶盖高度偏差:", INFO, outInfo.paths.logFile, m_params.saveLogTxt, outInfo.geometry.capHeighttDeviation);

		outInfo.geometry.capTopRect.x = outInfo.geometry.leftTopPoint.x;
		outInfo.geometry.capTopRect.y = outInfo.geometry.topPoint.y;
		outInfo.geometry.capTopRect.width = outInfo.geometry.rightTopPoint.x - outInfo.geometry.leftTopPoint.x;
		outInfo.geometry.capTopRect.height = m_params.capTopHeight;


		if (m_params.saveDebugImage)
		{
			outInfo.images.capTopBottomRectLog.data = std::make_shared<cv::Mat>(outInfo.images.capRegion.mat().clone());
			outInfo.images.capTopBottomRectLog.stageName = "PressCap_LocateTop";
			outInfo.images.capTopBottomRectLog.description = "盖帽定位";
			outInfo.images.capTopBottomRectLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
			DAS->DAS_Rect(outInfo.images.capTopBottomRectLog.mat(), m_params.saveDebugImage, outInfo.paths.intermediateImagesDir + "4.4.1.topPoints.jpg", outInfo.geometry.capTopRect, outInfo.geometry.capBottomRect);
		}
		if (m_params.capHeightErr == 0)
		{
			Log::WriteAsyncLog("瓶盖高度偏差阈值设置为0，跳过瓶盖高度检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		}
		else
		{
			if (outInfo.geometry.capHeighttDeviation > m_params.capHeightErr)
			{
				outInfo.status.statusCode = PRESSCAP_RETURN_FIND_HIGH_CAP;
				outInfo.status.errorMessage = "瓶盖高度偏差-高盖!";
				Log::WriteAsyncLog("瓶盖高度偏差-高盖!!", ERR, outInfo.paths.logFile, true);
				return;
			}
			else if (outInfo.geometry.capHeighttDeviation < -m_params.capHeightErr)
			{
				outInfo.status.statusCode = PRESSCAP_RETURN_FIND_LOW_CAP;
				outInfo.status.errorMessage = "瓶盖高度偏差-矮盖！";
				Log::WriteAsyncLog("瓶盖高度偏差-矮盖!!", ERR, outInfo.paths.logFile, true);
				return;
			}
		}
	}
	else
	{
		Log::WriteAsyncLog("设定瓶盖会超出背光源，不能检测瓶盖高度和盖顶角度!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		Log::WriteAsyncLog("不检测瓶盖高度和盖顶角度!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}


}

void InspPressCap::PressCap_CheckAngle(InspPressCapOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != PRESSCAP_RETURN_OK) {
		Log::WriteAsyncLog("跳过歪斜检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始歪斜检测!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	Log::WriteAsyncLog("盖顶倾斜角:", INFO, outInfo.paths.logFile, m_params.saveLogTxt, outInfo.geometry.topAngle);
	Log::WriteAsyncLog("支撑环倾斜角:", INFO, outInfo.paths.logFile, m_params.saveLogTxt, outInfo.geometry.bottomAngle);
	Log::WriteAsyncLog("盖顶与支撑环角度差值:", INFO, outInfo.paths.logFile, m_params.saveLogTxt, outInfo.geometry.topBottomAngleDif);
	if (m_params.topBottomAngleDif == 0)
	{
		Log::WriteAsyncLog("盖顶与支撑环角度差阈值设置为0，跳过盖顶与支撑环角度差检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	else if (m_params.capOut)
	{
		Log::WriteAsyncLog("瓶盖是否会超出背光源设置为0，跳过盖顶与支撑环角度差检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	else
	{
		if (outInfo.geometry.topBottomAngleDif > m_params.topBottomAngleDif)
		{
			outInfo.status.statusCode = PRESSCAP_RETURN_FIND_ANGLE_ERR;
			outInfo.status.errorMessage = "瓶盖歪斜!";
			Log::WriteAsyncLog("瓶盖歪斜!", ERR, outInfo.paths.logFile, true);
			return;
		}
	}

	if (m_params.topLineAngle == 0)
	{
		Log::WriteAsyncLog("盖顶角度阈值设置为0，跳过盖顶角度检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	else if (m_params.capOut)
	{
		Log::WriteAsyncLog("瓶盖是否会超出背光源设置为0，跳过盖顶角度检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	else
	{
		if (fabs(outInfo.geometry.topAngle) > m_params.topLineAngle)
		{
			outInfo.status.statusCode = PRESSCAP_RETURN_FIND_TOP_ANGLE_ERR;
			outInfo.status.errorMessage = "盖顶歪斜!";
			Log::WriteAsyncLog("盖顶歪斜!", ERR, outInfo.paths.logFile, true);
			return;
		}
	}


	if (m_params.bottomLineAngle == 0)
	{
		Log::WriteAsyncLog("盖底角度阈值设置为0，跳过盖底角度检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	else
	{
		if (fabs(outInfo.geometry.bottomAngle) > m_params.bottomLineAngle)
		{
			outInfo.status.statusCode = PRESSCAP_RETURN_FIND_BOTTOM_ANGLE_ERR;
			outInfo.status.errorMessage = "支撑环歪斜!";
			Log::WriteAsyncLog("支撑环歪斜!", ERR, outInfo.paths.logFile, true);
			return;
		}
	}



}

void InspPressCap::PressCap_CheckTopType(InspPressCapOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != PRESSCAP_RETURN_OK) {
		Log::WriteAsyncLog("跳过盖帽类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}

	if (m_params.topType == "0" || m_params.topType == "不检测")
	{
		outInfo.classification.topType.className = "不检测";
		Log::WriteAsyncLog("盖帽类型设置为0，跳过盖帽类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始盖帽类型检测!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	outInfo.images.topRegion.data = std::make_shared<cv::Mat>(outInfo.images.capRegion.mat()(outInfo.geometry.capTopRect).clone());
	outInfo.images.topRegion.stageName = "PressCap_CheckTopType";
	outInfo.images.topRegion.description = "提取盖帽区域做分类";
	outInfo.images.topRegion.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

	cv::Mat img = outInfo.images.topRegion.mat().clone();
	resize(img, img, cv::Size(img.cols, img.cols));
	if (m_params.classifyWeightsFile.find(".onnx") != std::string::npos)
	{
		outInfo.classification.topType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.classifyWeightsFile, m_params.classifyClassName, img);
	}
	else
	{
		outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}
	DAS->DAS_String(outInfo.images.topRegion.mat(), outInfo.classification.topType.className, outInfo.paths.intermediateImagesDir + "5.1.1.topType.jpg", m_params.saveDebugImage);


	Log::WriteAsyncLog("盖帽类型： ", ERR, outInfo.paths.logFile, true, outInfo.classification.topType.className);
	if (outInfo.classification.topType.className != m_params.topType)
	{
		outInfo.status.statusCode = PRESSCAP_RETURN_CAP_TOP_TYPE_FAILED;
		outInfo.status.errorMessage = "盖帽类型错误!";
		Log::WriteAsyncLog("盖帽类型错误!", ERR, outInfo.paths.logFile, true);
	}

	if ((m_params.saveTrain == 1 || m_params.saveTrain == 2) && outInfo.status.statusCode == PRESSCAP_RETURN_OK)
	{
		
		cv::Mat imgSave;
		cv::resize(outInfo.images.topRegion.mat(), imgSave, cv::Size(64, 64));
		
		COM->CreateDir(outInfo.paths.trainDir + "CAPTOP/" + outInfo.classification.topType.className);
		cv::imwrite(outInfo.paths.trainDir + "CAPTOP/" + outInfo.classification.topType.className + "/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_CLASSFY.jpg", imgSave);
	}
	else if (m_params.saveTrain == 1 || m_params.saveTrain == 3 && outInfo.status.statusCode != PRESSCAP_RETURN_OK)
	{
		
		cv::Mat imgSave;
		cv::resize(outInfo.images.topRegion.mat(), imgSave, cv::Size(64, 64));
		
		COM->CreateDir(outInfo.paths.trainDir + "CAPTOP/" + outInfo.classification.topType.className);
		cv::imwrite(outInfo.paths.trainDir + "CAPTOP/" + outInfo.classification.topType.className + "/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_CLASSFY.jpg", imgSave);
	}



}

void InspPressCap::PressCap_CheckBottomType(InspPressCapOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != PRESSCAP_RETURN_OK) {
		Log::WriteAsyncLog("跳过盖底类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	if (m_params.capBottomHeight == 0)
	{
		outInfo.classification.bottomType.className = "不检测";
		Log::WriteAsyncLog("盖底高度设置为0，跳过盖底类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else if (m_params.bottomType == "0" || m_params.bottomType == "不检测")
	{
		outInfo.classification.bottomType.className = "不检测";
		Log::WriteAsyncLog("盖底类型设置为0，跳过盖底类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始盖底类型检测!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	outInfo.images.bottomRegion.data = std::make_shared<cv::Mat>(outInfo.images.capRegion.mat()(outInfo.geometry.capBottomRect).clone());
	outInfo.images.bottomRegion.stageName = "PressCap_CheckBottomType";
	outInfo.images.bottomRegion.description = "提取盖底区域做分类";
	outInfo.images.bottomRegion.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

	cv::Mat img = outInfo.images.bottomRegion.mat().clone();
	resize(img, img, cv::Size(img.cols, img.cols));
	if (m_params.classifyWeightsFile.find(".onnx") != std::string::npos)
	{
		outInfo.classification.bottomType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.classifyWeightsFile, m_params.classifyClassName, img);
	}
	else
	{
		outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}
	DAS->DAS_String(outInfo.images.bottomRegion.mat(), outInfo.classification.bottomType.className, outInfo.paths.intermediateImagesDir + "6.1.1.bottomType.jpg", m_params.saveDebugImage);


	Log::WriteAsyncLog("盖底类型: ", ERR, outInfo.paths.logFile, true, outInfo.classification.bottomType.className);

	if (outInfo.classification.bottomType.className != m_params.bottomType)
	{
		outInfo.status.statusCode = PRESSCAP_RETURN_CAP_BOTTOM_TYPE_FAILED;
		outInfo.status.errorMessage = "盖底类型错误!";
		Log::WriteAsyncLog("盖底类型错误!", ERR, outInfo.paths.logFile, true);
	}

	if ((m_params.saveTrain == 1 || m_params.saveTrain == 2) && outInfo.status.statusCode == PRESSCAP_RETURN_OK)
	{
		
		cv::Mat imgSave;
		cv::resize(outInfo.images.bottomRegion.mat(), imgSave, cv::Size(64, 64));
		
		COM->CreateDir(outInfo.paths.trainDir + "CAPBOTTOM/" + outInfo.classification.bottomType.className);
		cv::imwrite(outInfo.paths.trainDir + "CAPBOTTOM/" + outInfo.classification.bottomType.className + "/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_CLASSFY.jpg", imgSave);
	}
	else if (m_params.saveTrain == 1 || m_params.saveTrain == 3 && outInfo.status.statusCode != PRESSCAP_RETURN_OK)
	{
		
		cv::Mat imgSave;
		cv::resize(outInfo.images.bottomRegion.mat(), imgSave, cv::Size(64, 64));
		COM->CreateDir(outInfo.paths.trainDir + "CAPBOTTOM/" + outInfo.classification.bottomType.className);
		cv::imwrite(outInfo.paths.trainDir + "CAPBOTTOM/" + outInfo.classification.bottomType.className + "/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_CLASSFY.jpg", imgSave);
	}


}

void InspPressCap::PressCap_Rotate(InspPressCapOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;

	// 使用const引用避免重复计算
	const auto& config = m_params;
	const cv::Rect& roiRect = config.roiRect;

	// 旋转中心计算优化
	const cv::Rect& bl = outInfo.geometry.capNeckLeftRect;
	const cv::Rect& br = outInfo.geometry.capNeckRightRect;

	outInfo.geometry.rotateCenter = (bl.x > (roiRect.width - br.x)) ?
		cv::Point(bl.x, bl.y) :
		cv::Point(br.x + br.width, br.y);

	// 合并重复的矩阵运算
	const float angle = -outInfo.geometry.bottomAngle;
	const cv::Mat affineMat = cv::getRotationMatrix2D(outInfo.geometry.rotateCenter, angle, 1.0);

	// 使用智能指针避免不必要的拷贝
	outInfo.images.affineMat.data = std::make_shared<cv::Mat>(affineMat);

	// 图像处理流水线优化
	if (!outInfo.images.capRegionGrayConverted.mat().empty()) {
		cv::Mat rotated;
		cv::warpAffine(outInfo.images.capRegionGrayConverted.mat(), rotated,
			affineMat, outInfo.images.capRegionGrayConverted.mat().size(),
			cv::INTER_LINEAR, cv::BORDER_REFLECT);
		outInfo.images.rotateCap.data = std::make_shared<cv::Mat>(std::move(rotated));
	}

	// 调试输出优化（使用条件判断前置）
	if (config.saveDebugImage) {
		DAS->DAS_Img(outInfo.images.rotateCap.mat(), outInfo.paths.intermediateImagesDir + "7.1.2.rotateCap.jpg", m_params.saveDebugImage);
	}

	// 坐标变换优化
	std::vector<cv::Point> srcPtList = {
		outInfo.geometry.leftTopPoint,
		outInfo.geometry.rightTopPoint,
		cv::Point(bl.x, bl.y),
		cv::Point(br.x + br.width, br.y),
		outInfo.geometry.topPoint
	};

	std::vector<cv::Point> dstPtList;
	cv::transform(srcPtList, dstPtList, affineMat);


	//// 正确绑定geometry子对象的成员
	//auto& geometry = outInfo.geometry;
	//auto& [leftTop, rightTop, bottomLeftRect, bottomRightRect, topPoint] = geometry;

	//// 直接赋值到geometry成员
	//geometry.leftTopPoint = dstPtList[0];
	//geometry.rightTopPoint = dstPtList[1];
	//geometry.topPoint = dstPtList[4];
}

void InspPressCap::PressCap_CheckLeak(InspPressCapOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != PRESSCAP_RETURN_OK) {
		Log::WriteAsyncLog("跳过压盖不严检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}

	if (m_params.leakThresh == 0)
	{
		Log::WriteAsyncLog("压盖不严阈值设置为0，跳过压盖不严检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	else if (m_params.capOut)
	{
		Log::WriteAsyncLog("瓶盖是否会超出背光源设置为0，跳过压盖不严检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	else if (m_params.capType == 2)
	{
		Log::WriteAsyncLog("瓶盖类型设置为0，跳过压盖不严检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	else
	{

		cv::Rect checkLeakRectL;
		cv::Rect checkLeakRectR; //泄漏检测区域
		if (m_params.capBottomHeight == 0)
		{
			checkLeakRectL.x = outInfo.geometry.capNeckLeftRect.x;
			checkLeakRectL.y = outInfo.geometry.capNeckLeftRect.y - m_params.capHeight * 0.5;
			checkLeakRectL.width = outInfo.geometry.capNeckRightRect.x - outInfo.geometry.capNeckLeftRect.x + outInfo.geometry.capNeckRightRect.width;
			checkLeakRectL.height = m_params.capHeight * 0.5 + outInfo.geometry.capNeckLeftRect.height;

			checkLeakRectR.x = outInfo.geometry.capNeckLeftRect.x;
			checkLeakRectR.y = outInfo.geometry.capNeckRightRect.y - m_params.capHeight * 0.5;
			checkLeakRectR.width = outInfo.geometry.capNeckRightRect.x - outInfo.geometry.capNeckLeftRect.x + outInfo.geometry.capNeckRightRect.width;
			checkLeakRectR.height = m_params.capHeight * 0.5 + outInfo.geometry.capNeckRightRect.height;
		}
		else
		{
			checkLeakRectL.x = outInfo.geometry.capNeckLeftRect.x;
			checkLeakRectL.y = outInfo.geometry.capNeckLeftRect.y - m_params.capBottomHeight * 0.5;
			checkLeakRectL.width = outInfo.geometry.capNeckRightRect.x - outInfo.geometry.capNeckLeftRect.x + outInfo.geometry.capNeckRightRect.width;
			checkLeakRectL.height = m_params.capBottomHeight * 0.5 + outInfo.geometry.capNeckLeftRect.height;

			checkLeakRectR.x = outInfo.geometry.capNeckLeftRect.x;
			checkLeakRectR.y = outInfo.geometry.capNeckRightRect.y - m_params.capBottomHeight * 0.5;
			checkLeakRectR.width = outInfo.geometry.capNeckRightRect.x - outInfo.geometry.capNeckLeftRect.x + outInfo.geometry.capNeckRightRect.width;
			checkLeakRectR.height = m_params.capBottomHeight * 0.5 + outInfo.geometry.capNeckRightRect.height;

		}

		//left and right points
		cv::Mat checkLeakImgL = outInfo.images.capRegionBinarized.mat()(checkLeakRectL).clone();
		cv::Mat checkLeakImgR = outInfo.images.capRegionBinarized.mat()(checkLeakRectR).clone();
		std::vector<cv::Point> leftPoints;
		std::vector<cv::Point> rightPoints;
		ANA->FindPointLeftToRightOne(checkLeakImgL, leftPoints);
		ANA->FindPointRightToLeftOne(checkLeakImgR, rightPoints);
		DAS->DAS_Points(checkLeakImgL, leftPoints, outInfo.paths.intermediateImagesDir + "7.2.1.leftPoints.jpg", m_params.saveDebugImage);
		DAS->DAS_Points(checkLeakImgR, rightPoints, outInfo.paths.intermediateImagesDir + "7.2.2.rightPoints.jpg", m_params.saveDebugImage);


		std::vector<cv::Point> leftLeakPoints;
		std::vector<cv::Point> rightLeakPoints;
		outInfo.geometry.maxErrL = MIN(MAX(5, outInfo.geometry.capNeckLeftRect.width * 0.2), outInfo.geometry.capNeckLeftRect.width * 0.30);//支撑环端点到瓶盖底部垂直边缘距离最大误差
		outInfo.geometry.maxErrR = MIN(MAX(5, outInfo.geometry.capNeckRightRect.width * 0.2), outInfo.geometry.capNeckRightRect.width * 0.30);//支撑环端点到瓶盖底部垂直边缘距离最大误差

		int gapWidthLeft = outInfo.geometry.capNeckLeftRect.width - outInfo.geometry.maxErrL;
		int gapWidthRight = outInfo.geometry.capNeckRightRect.width - outInfo.geometry.maxErrR;
		int cnt = 0;
		for (int i = 1; i < leftPoints.size() - outInfo.geometry.capNeckLeftRect.height * 0.7; i++) { //减去0.7的高度是避免某些支撑环是翘起的状态
			if (leftPoints[i].x > leftPoints[i - 1].x + 1 || leftPoints[i].x > gapWidthLeft) {
				cnt++;
				leftLeakPoints.push_back(leftPoints[i]);
				if (cnt >= 3) {
					outInfo.geometry.contourLeftLeak = leftLeakPoints;
				}
			}
			else {
				cnt = 0;
				leftLeakPoints.clear();
				continue;
			}
		}
		for (int i = 1; i < rightPoints.size() - outInfo.geometry.capNeckRightRect.height * 0.7; i++) {
			if (rightPoints[i].x < rightPoints[i - 1].x - 1 || rightPoints[i].x < checkLeakImgR.cols - gapWidthRight) {
				cnt++;
				rightLeakPoints.push_back(rightPoints[i]);
				if (cnt >= 3) {
					outInfo.geometry.contourRightLeak = rightLeakPoints;
				}
			}
			else {
				cnt = 0;
				rightLeakPoints.clear();
				continue;
			}
		}

		for (int i = 0; i < outInfo.geometry.contourLeftLeak.size(); i++)
		{
			outInfo.geometry.contourLeftLeak[i].x += (outInfo.geometry.capRect.x + checkLeakRectL.x);
			outInfo.geometry.contourLeftLeak[i].y += (outInfo.geometry.capRect.y + checkLeakRectL.y);
		}
		for (int i = 0; i < outInfo.geometry.contourRightLeak.size(); i++)
		{
			outInfo.geometry.contourRightLeak[i].x += (outInfo.geometry.capRect.x + checkLeakRectR.x);
			outInfo.geometry.contourRightLeak[i].y += (outInfo.geometry.capRect.y + checkLeakRectR.y);
		}

		DAS->DAS_Points(outInfo.images.capRegionBinarized.mat(), outInfo.geometry.contourLeftLeak, outInfo.paths.intermediateImagesDir + "7.3.1.leftLeakPoints.jpg", m_params.saveDebugImage);
		DAS->DAS_Points(outInfo.images.capRegionBinarized.mat(), outInfo.geometry.contourRightLeak, outInfo.paths.intermediateImagesDir + "7.3.2.rightLeakPoints.jpg", m_params.saveDebugImage);

		Log::WriteAsyncLog("LeftLeak =  ", INFO, outInfo.paths.logFile, m_params.saveLogTxt, outInfo.geometry.contourLeftLeak.size());
		if (outInfo.geometry.contourLeftLeak.size() > m_params.leakThresh)
		{
			outInfo.status.statusCode = PRESSCAP_RETURN_LEAK;
			outInfo.status.errorMessage = "盖底左侧不严!";
			Log::WriteAsyncLog("盖底左侧不严!", ERR, outInfo.paths.logFile, true);
			return;
		}

		Log::WriteAsyncLog("RightLeak =  ", INFO, outInfo.paths.logFile, m_params.saveLogTxt, outInfo.geometry.contourRightLeak.size());
		if (outInfo.geometry.contourRightLeak.size() > m_params.leakThresh)
		{
			outInfo.status.statusCode = PRESSCAP_RETURN_LEAK;
			outInfo.status.errorMessage = "盖底右侧不严!";
			Log::WriteAsyncLog("盖底右侧不严!", ERR, outInfo.paths.logFile, true);
			return;
		}
	}
}


void InspPressCap::PressCap_DrawResult(InspPressCapOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	Log::WriteAsyncLog("开始绘制结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

	outInfo.images.outputImg.data = std::make_shared<cv::Mat>(m_img.clone());
	outInfo.images.outputImg.stageName = "PressCap_DrawResult";
	outInfo.images.outputImg.description = "绘制全部结果: " + std::to_string(m_params.saveDebugImage);
	outInfo.images.outputImg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
	auto format = [](float conf) {
		return (std::ostringstream() << std::fixed << std::setprecision(2) << conf).str();
	};
	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{
		if (outInfo.locate.details[i].className == "瓶盖")
		{
			rectangle(outInfo.images.outputImg.mat(), outInfo.locate.details[i].box, Colors::GREEN, 1, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg.mat(),
				(outInfo.locate.details[i].className + "," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x , outInfo.locate.details[i].box.y + outInfo.locate.details[i].box.height + 10),
				Colors::GREEN, 45, FW_BOLD);
		}
		else if (outInfo.locate.details[i].className == "无盖")
		{
			rectangle(outInfo.images.outputImg.mat(), outInfo.locate.details[i].box, Colors::RED, 1, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg.mat(),
				(outInfo.locate.details[i].className + "," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x, outInfo.locate.details[i].box.y + outInfo.locate.details[i].box.height + 10),
				Colors::RED, 45, FW_BOLD);
		}
		else if (outInfo.locate.details[i].className == "无上盖")
		{
			rectangle(outInfo.images.outputImg.mat(), outInfo.locate.details[i].box, Colors::RED, 1, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg.mat(),
				(outInfo.locate.details[i].className + "," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x, outInfo.locate.details[i].box.y + outInfo.locate.details[i].box.height + 10),
				Colors::RED, 45, FW_BOLD);
		}
		else if (outInfo.locate.details[i].className == "缺陷盖")
		{
			rectangle(outInfo.images.outputImg.mat(), outInfo.locate.details[i].box, Colors::RED, 1, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg.mat(),
				(outInfo.locate.details[i].className + "," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x, outInfo.locate.details[i].box.y + outInfo.locate.details[i].box.height + 10),
				Colors::RED, 45, FW_BOLD);
		}
	}

	rectangle(outInfo.images.outputImg.mat(), m_params.roiRect, Colors::YELLOW, 1, cv::LINE_AA);

	rectangle(outInfo.images.outputImg.mat(), outInfo.geometry.capRect, Colors::GREEN, 1, cv::LINE_AA);

	outInfo.geometry.capTopRect.x += outInfo.geometry.capRect.x;
	outInfo.geometry.capTopRect.y += outInfo.geometry.capRect.y;
	rectangle(outInfo.images.outputImg.mat(), outInfo.geometry.capTopRect, Colors::GREEN, 1, cv::LINE_AA);

	outInfo.geometry.capBottomRect.x += outInfo.geometry.capRect.x;
	outInfo.geometry.capBottomRect.y += outInfo.geometry.capRect.y;
	rectangle(outInfo.images.outputImg.mat(), outInfo.geometry.capBottomRect, Colors::GREEN, 1, cv::LINE_AA);




	for (int i = 0; i < outInfo.defects.details.size(); i++)
	{
		if (outInfo.defects.details[i].className == "支撑环端点")
		{
			outInfo.defects.details[i].box.x += outInfo.geometry.capRect.x;
			outInfo.defects.details[i].box.y += outInfo.geometry.capRect.y;
			rectangle(outInfo.images.outputImg.mat(), outInfo.defects.details[i].box, Colors::GREEN, 1, cv::LINE_AA);
		}
		else
		{
			outInfo.defects.details[i].box.x += outInfo.geometry.capRect.x;
			outInfo.defects.details[i].box.y += outInfo.geometry.capRect.y;
			if (outInfo.defects.details[i].className == "防盗环缺陷")
			{
				putTextZH(outInfo.images.outputImg.mat(),
					outInfo.defects.details[i].className.c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y - 30),
					Colors::RED, 35, FW_BOLD);
				putTextZH(outInfo.images.outputImg.mat(),
					(std::to_string(outInfo.defects.details[i].box.width) + "," + std::to_string(outInfo.defects.details[i].box.height)).c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y),
					Colors::RED, 35, FW_BOLD);
				putTextZH(outInfo.images.outputImg.mat(),
					format(outInfo.defects.details[i].confidence).c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y + 30),
					Colors::RED, 35, FW_BOLD);
			}
			else if (outInfo.defects.details[i].className == "防盗环断桥")
			{
				putTextZH(outInfo.images.outputImg.mat(),
					outInfo.defects.details[i].className.c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y - 30),
					Colors::RED, 35, FW_BOLD);
				putTextZH(outInfo.images.outputImg.mat(),
					(std::to_string(outInfo.defects.details[i].box.width) + "," + std::to_string(outInfo.defects.details[i].box.height)).c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y),
					Colors::RED, 35, FW_BOLD);
				putTextZH(outInfo.images.outputImg.mat(),
					format(outInfo.defects.details[i].confidence).c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y + 30),
					Colors::RED, 35, FW_BOLD);
			}
			else if (outInfo.defects.details[i].className == "上下盖分离")
			{
				putTextZH(outInfo.images.outputImg.mat(),
					outInfo.defects.details[i].className.c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y - 30),
					Colors::RED, 35, FW_BOLD);
				putTextZH(outInfo.images.outputImg.mat(),
					(std::to_string(outInfo.defects.details[i].box.width) + "," + std::to_string(outInfo.defects.details[i].box.height)).c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y),
					Colors::RED, 35, FW_BOLD);
				putTextZH(outInfo.images.outputImg.mat(),
					format(outInfo.defects.details[i].confidence).c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y + 30),
					Colors::RED, 35, FW_BOLD);
			}
			else if (outInfo.defects.details[i].className == "压盖不严")
			{
				putTextZH(outInfo.images.outputImg.mat(),
					outInfo.defects.details[i].className.c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y - 30),
					Colors::RED, 35, FW_BOLD);
				putTextZH(outInfo.images.outputImg.mat(),
					(std::to_string(outInfo.defects.details[i].box.width) + "," + std::to_string(outInfo.defects.details[i].box.height)).c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y),
					Colors::RED, 35, FW_BOLD);
				putTextZH(outInfo.images.outputImg.mat(),
					format(outInfo.defects.details[i].confidence).c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y + 30),
					Colors::RED, 35, FW_BOLD);
			}
			else if (outInfo.defects.details[i].className == "瓶盖破损")
			{
				putTextZH(outInfo.images.outputImg.mat(),
					outInfo.defects.details[i].className.c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y - 30),
					Colors::RED, 35, FW_BOLD);
				putTextZH(outInfo.images.outputImg.mat(),
					(std::to_string(outInfo.defects.details[i].box.width) + "," + std::to_string(outInfo.defects.details[i].box.height)).c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y),
					Colors::RED, 35, FW_BOLD);
				putTextZH(outInfo.images.outputImg.mat(),
					format(outInfo.defects.details[i].confidence).c_str(),
					cv::Point(outInfo.defects.details[i].box.x  + 5 + outInfo.defects.details[i].box.width, outInfo.defects.details[i].box.y + 30),
					Colors::RED, 35, FW_BOLD);
			}
			rectangle(outInfo.images.outputImg.mat(), outInfo.defects.details[i].box, Colors::RED, 2, cv::LINE_AA);
		}
	}


	for (int i = 0; i < outInfo.geometry.contourLeftLeak.size(); i++) {
		circle(outInfo.images.outputImg.mat(), outInfo.geometry.contourLeftLeak[i], 2, Colors::RED, 3, cv::LINE_AA);
	}

	for (int i = 0; i < outInfo.geometry.contourRightLeak.size(); i++) {
		circle(outInfo.images.outputImg.mat(), outInfo.geometry.contourRightLeak[i], 2, Colors::RED, 3, cv::LINE_AA);
	}

	std::string rv = "ID = " + std::to_string(outInfo.system.jobId) + ", " + "RV = " + std::to_string(outInfo.status.statusCode) + ", " + outInfo.status.errorMessage;
	if (outInfo.status.statusCode == PRESSCAP_RETURN_OK) {
		putTextZH(outInfo.images.outputImg.mat(), rv.c_str(), cv::Point(15, 30), Colors::GREEN, 50, FW_BOLD);
	}
	else {
		putTextZH(outInfo.images.outputImg.mat(), rv.c_str(), cv::Point(15, 30), Colors::RED, 50, FW_BOLD);
	}
	Log::WriteAsyncLog(rv, INFO, outInfo.paths.logFile, true);

	if (m_params.capType == 0 || m_params.capType == 1)
	{
		if (outInfo.status.statusCode == PRESSCAP_RETURN_FIND_LOW_CAP || outInfo.status.statusCode == PRESSCAP_RETURN_FIND_HIGH_CAP)
		{
			putTextZH(outInfo.images.outputImg.mat(), ("瓶盖高度 = " + std::to_string(outInfo.geometry.capHeight)).c_str(), cv::Point(15, 120), Colors::RED, 35, FW_BOLD);
			putTextZH(outInfo.images.outputImg.mat(), ("高度偏差 = " + std::to_string(outInfo.geometry.capHeighttDeviation)).c_str(), cv::Point(15, 180), Colors::RED, 35, FW_BOLD);
		}
		else
		{
			putTextZH(outInfo.images.outputImg.mat(), ("瓶盖高度 = " + std::to_string(outInfo.geometry.capHeight)).c_str(), cv::Point(15, 120), Colors::GREEN, 35, FW_BOLD);
			putTextZH(outInfo.images.outputImg.mat(), ("高度偏差 = " + std::to_string(outInfo.geometry.capHeighttDeviation)).c_str(), cv::Point(15, 180), Colors::GREEN, 35, FW_BOLD);
		}
	}
	else if (m_params.capType == 2)
	{
		if (outInfo.status.statusCode == PRESSCAP_RETURN_FIND_LOW_CAP || outInfo.status.statusCode == PRESSCAP_RETURN_FIND_HIGH_CAP)
		{
			putTextZH(outInfo.images.outputImg.mat(), ("盖顶Y坐标 = " + std::to_string(outInfo.geometry.capTopY)).c_str(), cv::Point(15, 120), Colors::RED, 35, FW_BOLD);
			putTextZH(outInfo.images.outputImg.mat(), ("高度偏差 = " + std::to_string(outInfo.geometry.capHeighttDeviation)).c_str(), cv::Point(15, 180), Colors::RED, 35, FW_BOLD);
		}
		else
		{
			putTextZH(outInfo.images.outputImg.mat(), ("盖顶Y坐标 = " + std::to_string(outInfo.geometry.capTopY)).c_str(), cv::Point(15, 120), Colors::GREEN, 35, FW_BOLD);
			putTextZH(outInfo.images.outputImg.mat(), ("高度偏差 = " + std::to_string(outInfo.geometry.capHeighttDeviation)).c_str(), cv::Point(15, 180), Colors::GREEN, 35, FW_BOLD);
		}
	}

	

	if (outInfo.status.statusCode == PRESSCAP_RETURN_FIND_TOP_ANGLE_ERR)
	{
		putTextZH(outInfo.images.outputImg.mat(), ("盖顶角度 = " + format_float(outInfo.geometry.topAngle)).c_str(), cv::Point(15, 240), Colors::RED, 35, FW_BOLD);
	}
	else
	{
		putTextZH(outInfo.images.outputImg.mat(), ("盖顶角度 = " + format_float(outInfo.geometry.topAngle)).c_str(), cv::Point(15, 240), Colors::GREEN, 35, FW_BOLD);
	}

	if (outInfo.status.statusCode == PRESSCAP_RETURN_FIND_BOTTOM_ANGLE_ERR)
	{
		putTextZH(outInfo.images.outputImg.mat(), ("支撑环角度 = " + format_float(outInfo.geometry.bottomAngle)).c_str(), cv::Point(15, 300), Colors::RED, 35, FW_BOLD);
	}
	else
	{
		putTextZH(outInfo.images.outputImg.mat(), ("支撑环角度 = " + format_float(outInfo.geometry.bottomAngle)).c_str(), cv::Point(15, 300), Colors::GREEN, 35, FW_BOLD);
	}

	if (outInfo.status.statusCode == PRESSCAP_RETURN_FIND_ANGLE_ERR)
	{
		putTextZH(outInfo.images.outputImg.mat(), ("角度差 = " + format_float(outInfo.geometry.topBottomAngleDif)).c_str(), cv::Point(15, 360), Colors::RED, 35, FW_BOLD);
	}
	else
	{
		putTextZH(outInfo.images.outputImg.mat(), ("角度差 = " + format_float(outInfo.geometry.topBottomAngleDif)).c_str(), cv::Point(15, 360), Colors::GREEN, 35, FW_BOLD);
	}

	if (outInfo.status.statusCode == PRESSCAP_RETURN_LEAK)
	{
		putTextZH(outInfo.images.outputImg.mat(), ("左侧压盖不严 =  " + std::to_string(outInfo.geometry.contourLeftLeak.size())).c_str(), cv::Point(15, 420), Colors::RED, 35, FW_BOLD);
		putTextZH(outInfo.images.outputImg.mat(), ("右侧压盖不严 =  " + std::to_string(outInfo.geometry.contourRightLeak.size())).c_str(), cv::Point(15, 480), Colors::RED, 35, FW_BOLD);
	}
	else
	{
		putTextZH(outInfo.images.outputImg.mat(), ("左侧压盖不严 =  " + std::to_string(outInfo.geometry.contourLeftLeak.size())).c_str(), cv::Point(15, 420), Colors::GREEN, 35, FW_BOLD);
		putTextZH(outInfo.images.outputImg.mat(), ("右侧压盖不严 =  " + std::to_string(outInfo.geometry.contourRightLeak.size())).c_str(), cv::Point(15, 480), Colors::GREEN, 35, FW_BOLD);
	}

	if (outInfo.status.statusCode == PRESSCAP_RETURN_CAP_TOP_TYPE_FAILED)
	{
		putTextZH(outInfo.images.outputImg.mat(), ("盖帽类型: " + outInfo.classification.topType.className).c_str(), cv::Point(15, 540), Colors::RED, 35, FW_BOLD);
	}
	else
	{
		putTextZH(outInfo.images.outputImg.mat(), ("盖帽类型: " + outInfo.classification.topType.className).c_str(), cv::Point(15, 540), Colors::GREEN, 35, FW_BOLD);
	}

	if (outInfo.status.statusCode == PRESSCAP_RETURN_CAP_BOTTOM_TYPE_FAILED)
	{
		putTextZH(outInfo.images.outputImg.mat(), ("盖底类型: " + outInfo.classification.bottomType.className).c_str(), cv::Point(15, 600), Colors::RED, 35, FW_BOLD);
	}
	else
	{
		putTextZH(outInfo.images.outputImg.mat(), ("盖底类型: " + outInfo.classification.bottomType.className).c_str(), cv::Point(15, 600), Colors::GREEN, 35, FW_BOLD);
	}

	DAS->DAS_Img(outInfo.images.outputImg.mat(), outInfo.paths.intermediateImagesDir + "10.outputImg.jpg", m_params.saveDebugImage);




}

std::future<int> InspPressCap::RunInspectionAsync(InspPressCapOut& outInfo) {
	return std::async(std::launch::async, [this, &outInfo] {
		// 设置超时检查起点
		m_startTime = std::chrono::high_resolution_clock::now();

		// 执行主检测逻辑
		return PressCap_Main(outInfo, true);
		});
}

int InspPressCap::PressCap_Main(InspPressCapOut& outInfo, bool checkTimeout) {	
	try {
		double time0 = static_cast<double>(cv::getTickCount());
		if (outInfo.status.statusCode == PRESSCAP_RETURN_OK)
		{
			Log::WriteAsyncLog("PressCap_Main!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			if (checkTimeout && CheckTimeout(m_params.timeOut)) {
				Log::WriteAsyncLog("超时!", WARNING, outInfo.paths.logFile, true);
				*m_timeoutFlagRef = true;
				outInfo.status.statusCode = PRESSCAP_RETURN_TIMEOUT;
				return PRESSCAP_RETURN_TIMEOUT;
			}

			if (outInfo.status.statusCode != PRESSCAP_RETURN_OK) {
				Log::WriteAsyncLog("跳过获取模型!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
			}
			else
			{
				std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
				std::string cameraIdStr = std::to_string(outInfo.system.cameraId);
				m_params.locateWeightsFile = capDetectionModelMap["capDetection_" + cameraIdStr];
				m_params.defectWeightsFile = capDefectModelMap["capDefect_" + cameraIdStr];
				m_params.classifyWeightsFile = capClassifyModelMap["capClassify_" + cameraIdStr];
			}

			PressCap_RotateImg(outInfo);
			outInfo.images.outputImg.data = std::make_shared<cv::Mat>(m_img.clone());

			// 第1步:定位瓶盖
			PressCap_SetROI(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return PRESSCAP_RETURN_TIMEOUT;
			}

			// 第2步:定位瓶盖
			PressCap_LocateCap(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return PRESSCAP_RETURN_TIMEOUT;
			}

			// 第3步:缺陷检测
			PressCap_CheckDefect(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return PRESSCAP_RETURN_TIMEOUT;
			}
			

			// 第4步:定位瓶盖各区域
			if (m_params.capType == 0 || m_params.capType == 1)
			{
				PressCap_LocateTopBottom(outInfo);
			}
			else if(m_params.capType == 2)
			{
				PressCap_LocateTopBottom_CapTpye2(outInfo);
			}
			
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return PRESSCAP_RETURN_TIMEOUT;
			}

			// 第5步:歪斜检测
			PressCap_CheckAngle(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return PRESSCAP_RETURN_TIMEOUT;
			}

			// 第6步:压盖不严检测
			PressCap_CheckLeak(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return PRESSCAP_RETURN_TIMEOUT;
			}

			// 第7步:盖帽类型检测
			PressCap_CheckTopType(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return PRESSCAP_RETURN_TIMEOUT;
			}

			// 第8步:盖底类型检测
			PressCap_CheckBottomType(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return PRESSCAP_RETURN_TIMEOUT;
			}
		}
		

		// 第9步:绘制结果
		PressCap_DrawResult(outInfo);
		if (CheckTimeout(m_params.timeOut))
		{
			Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			return PRESSCAP_RETURN_TIMEOUT;
		}

		if (outInfo.status.statusCode == PRESSCAP_RETURN_OK) {
			DAS->DAS_Img(outInfo.images.outputImg.mat(),
				outInfo.paths.resultsOKDir + std::to_string(outInfo.system.jobId) + ".jpg",
				m_params.saveResultImage);
		}
		else {
			DAS->DAS_Img(outInfo.images.outputImg.mat(),
				outInfo.paths.resultsNGDir + std::to_string(outInfo.system.jobId) + ".jpg",
				m_params.saveResultImage);
		}

		Log::WriteAsyncLog("END!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

		time0 = ((double)cv::getTickCount() - time0) / cv::getTickFrequency() * 1000;
		Log::WriteAsyncLog("算法耗时：", INFO, outInfo.paths.logFile, m_params.saveLogTxt, time0);
	}
	catch (const std::exception& e) {
		std::cerr << "[ERROR] Inference failed: " << e.what() << std::endl;
		return PRESSCAP_RETURN_ALGO_ERR;
	}

	return outInfo.status.statusCode;
}