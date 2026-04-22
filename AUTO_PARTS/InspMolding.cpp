#include "HeaderDefine.h"
#include "InspMolding.h"
#include "InspMoldingStruct.h"
#include "ModelManager.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <locale>
#include "InferenceWorker.h"
#include "Data.h"
#include "AnalyseMat.h"
#include "../XML/write_json.h"

std::mutex InspMolding::modelLoadMutex;
std::map<std::string, std::string> InspMolding::moldingDetectionModelMap;
std::map<std::string, std::string> InspMolding::moldingDefectModelMap;


// 构造函数:初始化时加载所有相关模型
InspMolding::InspMolding(std::string configPath, const cv::Mat& img, int cameraId, int jobId, bool isLoadConfig, InspMoldingOut& inspMoldingOutInfo)
	: LOG(std::make_unique<Log>()),
	ANA(std::make_unique<AnalyseMat>()),
	COM(std::make_unique<Common>()),
	TXT(std::make_unique<TxtOperater>()),
	MF(std::make_unique<MatchFun>())
{
	inspMoldingOutInfo.system.jobId = jobId;
	inspMoldingOutInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << inspMoldingOutInfo.system.cameraId << "  m_jobId_" << inspMoldingOutInfo.system.jobId << std::endl;

	char bufLog[100];
	sprintf(bufLog, "Molding/camera_%d/", inspMoldingOutInfo.system.cameraId);
	char bufConfig[100];
	sprintf(bufConfig, "/InspMoldingConfig_%d.txt", inspMoldingOutInfo.system.cameraId);
	inspMoldingOutInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
	inspMoldingOutInfo.paths.intermediateImagesDir =
		ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(inspMoldingOutInfo.system.jobId) + "/";
	inspMoldingOutInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
	inspMoldingOutInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
	inspMoldingOutInfo.paths.configFile = configPath + std::string(bufConfig);
	inspMoldingOutInfo.paths.logFile = inspMoldingOutInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

	inspMoldingOutInfo.status.errorMessage = "OK";
	inspMoldingOutInfo.system.startTime = COM->time_t2string_with_ms();
	inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_OK;
	inspMoldingOutInfo.status.logs.reserve(100);
	m_params.locateClassName = { "BAD","OK", "OTHER" };// 异物、注塑件、坏料
	inspMoldingOutInfo.images.moldingImgs.clear();
	//输入参数初始化
	if (img.empty())
	{
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_INPUT_PARA_ERR;
		inspMoldingOutInfo.status.errorMessage = "输入图像为空!";
		LOG->WriteLog("输入图像为空!", ERR, inspMoldingOutInfo.paths.logFile, true);
		return;
	}
	if (img.channels() == 1)
	{
		cv::cvtColor(img, m_img, cv::COLOR_GRAY2BGR);
		inspMoldingOutInfo.images.outputImg.data = std::make_shared<cv::Mat>(m_img.clone());
		inspMoldingOutInfo.images.outputImg.stageName = "初始化";
		inspMoldingOutInfo.images.outputImg.description = "初始化";
		inspMoldingOutInfo.images.outputImg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
	}
	else if (img.channels() == 3)
	{
		m_img = img.clone();
		inspMoldingOutInfo.images.outputImg.data = std::make_shared<cv::Mat>(m_img.clone());
	}



	COM->CreateDir(inspMoldingOutInfo.paths.logDirectory);
	LOG->WriteLog("********** Start Inspction JobID = ", INFO, inspMoldingOutInfo.paths.logFile, true, inspMoldingOutInfo.system.jobId, " ***********");

	//读取config
	bool rv_loadConfig = readParams(m_img, inspMoldingOutInfo.paths.configFile, m_params, inspMoldingOutInfo, inspMoldingOutInfo.paths.logFile);
	if (!rv_loadConfig) {
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_CONFIG_ERR;
		inspMoldingOutInfo.status.errorMessage = "读取算法config失败!";
		LOG->WriteLog("读取算法config失败!", ERR, inspMoldingOutInfo.paths.logFile, true);
		return;
	}
	else
	{
		LOG->WriteLog("读取config成功!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
	}


	//检测roi
	if (!ANA->JudgeRectIn(cv::Rect(0, 0, img.cols, img.rows), m_params.roiRect)) {
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_CONFIG_ERR;
		inspMoldingOutInfo.status.errorMessage = "roi 设置超出图像范围!";
		LOG->WriteLog("roi 设置超出图像范围", ERR, inspMoldingOutInfo.paths.logFile, true);
		return;
	}

	//读取缺陷配置文件
	if (!TXT->LoadTypeConfigInTxt(m_params.defectThreshConfig, m_params.defectPara, inspMoldingOutInfo.paths.logFile))
	{
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_CONFIG_ERR;
		LOG->WriteLog("工件缺陷参数设置错误！", ERR, inspMoldingOutInfo.paths.logFile, true);
		inspMoldingOutInfo.status.errorMessage = "工件缺陷参数设置错误!";
		return;
	}
	else
	{
		LOG->WriteLog("工件缺陷参数读取成功!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
	}

	//读取缺陷类型名称
	std::ifstream ifs(m_params.defectNameFile.c_str());
	if (!ifs.is_open()) {
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_CONFIG_ERR;
		inspMoldingOutInfo.status.errorMessage = "缺陷类型文件缺失!";
		LOG->WriteLog(m_params.defectNameFile, ERR, inspMoldingOutInfo.paths.logFile, true, "---缺陷类型文件缺失!");
		return;
	}
	else
	{
		m_params.defectClassName.clear();
		std::string line;
		while (getline(ifs, line)) m_params.defectClassName.push_back(line);
		LOG->WriteLog("缺陷类型文件读取成功！", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (isLoadConfig || inspMoldingOutInfo.system.jobId == 0 || !loadMoldingConfigSuccess[inspMoldingOutInfo.system.cameraId])
	{
		bool loadModel = loadAllModels(inspMoldingOutInfo, true);
		if (!loadModel) {
			inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_CONFIG_ERR;
			inspMoldingOutInfo.status.errorMessage = "深度学习模型加载异常!";
			LOG->WriteLog(m_params.defectNameFile, ERR, inspMoldingOutInfo.paths.logFile, true, "---深度学习模型加载异常!");
			return;
		}
	}
	else
	{
		LOG->WriteLog("跳过模型加载！", WARNING, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
	}



	if (!validateCameraModels(inspMoldingOutInfo.system.cameraId)) {
		LOG->WriteLog("相机ID配置错误/模型文件缺失!", ERR, inspMoldingOutInfo.paths.logFile, true);
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_CONFIG_ERR;
		inspMoldingOutInfo.status.errorMessage = "相机ID配置错误/模型文件缺失!";
		throw std::invalid_argument("相机ID配置错误/模型文件缺失!");
	}

	if (inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_OK)
	{
		loadMoldingConfigSuccess[inspMoldingOutInfo.system.cameraId] = true;
	}
	else
	{
		loadMoldingConfigSuccess[inspMoldingOutInfo.system.cameraId] = false;
	}

	if (m_params.saveDebugImage) {
		COM->CreateDir(inspMoldingOutInfo.paths.intermediateImagesDir);
	}
	if (m_params.saveResultImage) {
		COM->CreateDir(inspMoldingOutInfo.paths.resultsOKDir);
		COM->CreateDir(inspMoldingOutInfo.paths.resultsNGDir);
	}
}

InspMolding::~InspMolding() {
	// 可在此处添加资源释放逻辑（如有需要）
}

// 验证摄像头ID对应的模型配置是否存在
bool InspMolding::validateCameraModels(int cameraId) {
	std::lock_guard<std::mutex> lock(modelLoadMutex);
	return moldingDetectionModelMap.count("moldingDetection_" + std::to_string(cameraId)) &&
		moldingDefectModelMap.count("moldingDefect_" + std::to_string(cameraId));
}

// 加载所有模型到ModelManager
bool InspMolding::loadAllModels(InspMoldingOut& inspMoldingOutInfo, bool ini) {
	if (!ini)
	{
		LOG->WriteLog("跳过模型加载!", WARNING, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
		return true;
	}
	std::vector<std::string> modelPaths;

	// 收集所有模型路径
	for (const auto& pair : moldingDetectionModelMap) {
		modelPaths.push_back(pair.second);
	}
	for (const auto& pair : moldingDefectModelMap) {
		modelPaths.push_back(pair.second);
	}

	// 去重
	std::sort(modelPaths.begin(), modelPaths.end());
	auto last = std::unique(modelPaths.begin(), modelPaths.end());
	modelPaths.erase(last, modelPaths.end());

	// 加载模型
	try {
		ModelManager::Instance().LoadModels(modelPaths, ini, m_params.hardwareType);
		return true;
	}
	catch (const std::exception& e) {
		LOG->WriteLog("注塑件模型加载异常!", WARNING, inspMoldingOutInfo.paths.logFile, true);
		inspMoldingOutInfo.status.errorMessage = "注塑件模型加载异常!";
		std::cerr << "[ERROR] Molding failed to load models: " << e.what() << std::endl;
		throw;
	}
}

// 读取参数的函数
bool InspMolding::readParams(cv::Mat img, const std::string& filePath, InspMoldingIn& params, InspMoldingOut& inspMoldingOutInfo, const std::string& fileName) {
	std::ifstream ifs(filePath.c_str());
	if (!ifs.is_open()) {
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
			inspMoldingOutInfo.status.errorMessage = "参数缺失!";
			LOG->WriteLog(keyWord, WARNING, inspMoldingOutInfo.paths.logFile, true, " 参数缺失！");
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
		if (keyWord == "MOLDING_SAVE_DEBUG_IMAGE") {
			params.saveDebugImage = std::stoi(value);
		}
		else if (keyWord == "MOLDING_SAVE_RESULT_IMAGE") {
			params.saveResultImage = std::stoi(value);
		}
		else if (keyWord == "MOLDING_SAVE_LOG_TXT") {
			params.saveLogTxt = std::stoi(value);
		}
		else if (keyWord == "MOLDING_DRAW_RESULT") {
			params.drawResult = std::stoi(value);
		}
		else if (keyWord == "MOLDING_SAVE_TRAIN") {
			params.saveTrain = std::stoi(value);
		}
		else if (keyWord == "MOLDING_ROI_X") {
			params.roiRect.x = std::stoi(value);
			if (params.roiRect.x < 0 || params.roiRect.x > img.cols)
			{
				inspMoldingOutInfo.status.errorMessage = "MOLDING_ROI_X 设置超出图像范围!";
				LOG->WriteLog("MOLDING_ROI_X 设置超出图像范围！", ERR, inspMoldingOutInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "MOLDING_ROI_Y") {
			params.roiRect.y = std::stoi(value);
			if (params.roiRect.y < 0 || params.roiRect.y > img.rows)
			{
				inspMoldingOutInfo.status.errorMessage = "MOLDING_ROI_Y 设置超出图像范围!";
				LOG->WriteLog("MOLDING_ROI_Y 设置超出图像范围！", ERR, inspMoldingOutInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "MOLDING_ROI_W") {
			params.roiRect.width = std::stoi(value);
			if (params.roiRect.x + params.roiRect.width > img.cols)
			{
				inspMoldingOutInfo.status.errorMessage = "MOLDING_ROI_X+MOLDING_ROI_W 设置超出图像范围!";
				LOG->WriteLog("MOLDING_ROI_X+MOLDING_ROI_W 设置超出图像范围！", ERR, inspMoldingOutInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "MOLDING_ROI_H") {
			params.roiRect.height = std::stoi(value);
			if (params.roiRect.y + params.roiRect.height > img.rows)
			{
				inspMoldingOutInfo.status.errorMessage = "MOLDING_ROI_Y+MOLDING_ROI_H 设置超出图像范围!";
				LOG->WriteLog("MOLDING_ROI_Y+MOLDING_ROI_H 设置超出图像范围！", ERR, inspMoldingOutInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "MOLDING_HARDWARE_TYPE") {
			params.hardwareType = std::stoi(value);
		}
		else if (keyWord == "MOLDING_MODEL_TYPE") {
			params.modelType = std::stoi(value);
		}
		if (keyWord == "MOLDING_LOCATE_WEIGHTS_FILE") {
			std::lock_guard<std::mutex> lock(modelLoadMutex);  // 加锁
			std::string camera = std::to_string(inspMoldingOutInfo.system.cameraId);
			moldingDetectionModelMap["moldingDetection_" + camera] = value;
			params.locateWeightsFile = value;
			if (!COM->FileExistsModern(params.locateWeightsFile))
			{
				inspMoldingOutInfo.status.errorMessage = params.locateWeightsFile + "--定位模型文件缺失!";
				LOG->WriteLog(params.locateWeightsFile, ERR, inspMoldingOutInfo.paths.logFile, true, "--定位模型文件文件缺失！");
				return false;
			}
		}
		else if (keyWord == "MOLDING_DEFECT_WEIGHTS_FILE") {
			std::lock_guard<std::mutex> lock(modelLoadMutex);  // 加锁
			std::string camera = std::to_string(inspMoldingOutInfo.system.cameraId);
			moldingDefectModelMap["moldingDefect_" + camera] = value;
			params.defectWeightsFile = value;
			if (!COM->FileExistsModern(params.defectWeightsFile))
			{
				inspMoldingOutInfo.status.errorMessage = params.defectWeightsFile + "--缺陷模型文件缺失!";
				LOG->WriteLog(params.defectWeightsFile, ERR, inspMoldingOutInfo.paths.logFile, true, "--缺陷模型文件缺失！");
				return false;
			}
		}
		else if (keyWord == "MOLDING_DEFECT_NAME_FILE") {
			params.defectNameFile = value;
			if (!COM->FileExistsModern(params.defectNameFile))
			{
				inspMoldingOutInfo.status.errorMessage = params.defectNameFile + "--缺陷类型文件缺失!";
				LOG->WriteLog(params.defectNameFile, ERR, inspMoldingOutInfo.paths.logFile, true, "--缺陷类型文件缺失！");
				return false;
			}
		}
		else if (keyWord == "MOLDING_DEFECT_CONFIG") {
			params.defectThreshConfig = value;
			if (!COM->FileExistsModern(params.defectThreshConfig))
			{
				inspMoldingOutInfo.status.errorMessage = params.defectThreshConfig + "--缺陷阈值文件缺失!";
				LOG->WriteLog(params.defectThreshConfig, ERR, inspMoldingOutInfo.paths.logFile, true, "--缺陷阈值文件缺失！");
				return false;
			}
		}
	}

	ifs.close();
	return true;
}

void InspMolding::Molding_SetROI(InspMoldingOut& inspMoldingOutInfo) {
	if (inspMoldingOutInfo.status.statusCode != MOLDING_RETURN_OK) {
		LOG->WriteLog("跳过ROI区域获取!", WARNING, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		LOG->WriteLog("开始ROI区域获取!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
	}


	inspMoldingOutInfo.images.roi.data = std::make_shared<cv::Mat>(m_img(m_params.roiRect).clone());
	inspMoldingOutInfo.images.roi.stageName = "Molding_Main";
	inspMoldingOutInfo.images.roi.description = "ROI区域获取";
	inspMoldingOutInfo.images.roi.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

	inspMoldingOutInfo.images.roiLog.data = std::make_shared<cv::Mat>(m_img.clone());
	inspMoldingOutInfo.images.roiLog.stageName = "Molding_Main";
	inspMoldingOutInfo.images.roiLog.description = "ROI_LOG绘制: " + std::to_string(m_params.saveDebugImage);
	inspMoldingOutInfo.images.roiLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
	DAS->DAS_Rect(inspMoldingOutInfo.images.roiLog.mat(), m_params.roiRect, inspMoldingOutInfo.paths.intermediateImagesDir + "1.0.0.roiRect.jpg", m_params.saveDebugImage);

}

void InspMolding::Molding_Locate(InspMoldingOut& inspMoldingOutInfo) {
	if (inspMoldingOutInfo.status.statusCode != MOLDING_RETURN_OK) {
		LOG->WriteLog("跳过工件定位!", WARNING, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		LOG->WriteLog("开始工件定位!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (m_params.locateWeightsFile.find(".xml") != std::string::npos)
	{
		//inspMoldingOutInfo.locate.details = InferenceWorker::RunOpenVINO(m_params.locateWeightsFile, m_params.locateClassName, inspMoldingOutInfo.images.roi.mat(), 0.3, 0.4);
	}
	else if (m_params.locateWeightsFile.find(".onnx") != std::string::npos)
	{
		inspMoldingOutInfo.locate.details = InferenceWorker::RunObb(m_params.locateWeightsFile, m_params.locateClassName, inspMoldingOutInfo.images.roi.mat(), 0.3, 0.4);
	}
	else
	{
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_CONFIG_ERR;
		inspMoldingOutInfo.status.errorMessage = "模型文件异常，目前仅支持xml、onnx!";
		LOG->WriteLog("模型文件异常，目前仅支持xml、onnx!", ERR, inspMoldingOutInfo.paths.logFile, true);

		return;
	}
	if (m_params.saveDebugImage)
	{
		inspMoldingOutInfo.images.moldingRegionDetectLog.data = std::make_shared<cv::Mat>(inspMoldingOutInfo.images.roi.mat().clone());
		inspMoldingOutInfo.images.moldingRegionDetectLog.stageName = "Molding_LocateMolding";
		inspMoldingOutInfo.images.moldingRegionDetectLog.description = "Locate绘制: " + std::to_string(m_params.saveDebugImage);
		inspMoldingOutInfo.images.moldingRegionDetectLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
		DAS->DAS_FinsObjectObb(inspMoldingOutInfo.images.moldingRegionDetectLog.mat(), inspMoldingOutInfo.locate.details, inspMoldingOutInfo.paths.intermediateImagesDir + "2.1.1.detections.jpg", m_params.saveDebugImage);
	}

	for (int i = 0; i < inspMoldingOutInfo.locate.details.size(); i++)
	{
		inspMoldingOutInfo.locate.details[i].box = ANA->AdjustRotateROI(inspMoldingOutInfo.locate.details[i].box, inspMoldingOutInfo.images.roi.mat());
		inspMoldingOutInfo.locate.details[i].box.center.x += m_params.roiRect.x;
		inspMoldingOutInfo.locate.details[i].box.center.y += m_params.roiRect.y;
	}

	LOG->WriteLog("开始分析定位结果!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
	std::vector<FinsObjectRotate> detectionsFil;  // 检测结果
	std::vector<FinsObjectRotate> detectionsOK;  // 检测结果
	std::vector<FinsObjectRotate> detectionsBad;  // 检测结果
	bool findFM = false;
	bool findOK = false;
	bool findBAD = false;
	int bnd = 5; //靠近上下边界的直接过滤
	int minH = 1200; //点位矩形框高度小的直接过滤
	for (int i = 0; i < inspMoldingOutInfo.locate.details.size(); i++)
	{
		bool outPart = false;
		cv::Point2f vertices[4];
		inspMoldingOutInfo.locate.details[i].box.points(vertices);

	
		
		if (inspMoldingOutInfo.locate.details[i].className == "BAD")
		{
			detectionsFil.push_back(inspMoldingOutInfo.locate.details[i]);
			detectionsBad.push_back(inspMoldingOutInfo.locate.details[i]);
			findBAD = true;
		}
		else if (inspMoldingOutInfo.locate.details[i].className == "OK")
		{
			for (int j = 0; j < 4; j++)
			{
				if (vertices[j].x < bnd || vertices[j].x > inspMoldingOutInfo.images.roi.mat().cols - bnd)
				{
					outPart = true;
				}
				else if (vertices[j].y < bnd || vertices[j].y > inspMoldingOutInfo.images.roi.mat().rows - bnd)
				{
					outPart = true;
				}
			}
			if (!outPart)
			{
				detectionsOK.push_back(inspMoldingOutInfo.locate.details[i]);
				detectionsFil.push_back(inspMoldingOutInfo.locate.details[i]);
			}
			
			//inspMoldingOutInfo.images.moldingImgs.push_back(ANA->ExtractRotatedROI(inspMoldingOutInfo.locate.details[i].box, inspMoldingOutInfo.images.roi.mat()));
		}
		else if (inspMoldingOutInfo.locate.details[i].className == "OTHER")
		{
			detectionsBad.push_back(inspMoldingOutInfo.locate.details[i]);
			detectionsFil.push_back(inspMoldingOutInfo.locate.details[i]);
			findFM = true;
		}

		

		
	}
	inspMoldingOutInfo.locate.details = detectionsFil;
	inspMoldingOutInfo.locate.okDetails = detectionsOK;
	inspMoldingOutInfo.locate.badDetails = detectionsBad;

	if (findFM)
	{
		if (m_params.saveTrain)
		{
			COM->CreateDir("D:/TRAIN_DATA_2/MOLDING/LOCATE/OTHER");
			auto jsonData = generateXAnyLabelingJSON(
				inspMoldingOutInfo.locate.details,
				inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg",
				inspMoldingOutInfo.images.outputImg.mat().rows,
				inspMoldingOutInfo.images.outputImg.mat().cols
			);
			saveJSONToFile(jsonData, "D:/TRAIN_DATA_2/MOLDING/LOCATE/OTHER/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".json");
			cv::imwrite("D:/TRAIN_DATA_2/MOLDING/LOCATE/OTHER/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg", inspMoldingOutInfo.images.outputImg.mat());
		}

		inspMoldingOutInfo.status.errorMessage = "异物!";
		LOG->WriteLog("异物!", ERR, inspMoldingOutInfo.paths.logFile, true);
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_FIND_FM;
		return;
	}
	if (findBAD)
	{
		if (m_params.saveTrain)
		{
			COM->CreateDir("D:/TRAIN_DATA_2/MOLDING/LOCATE/BAD");
			auto jsonData = generateXAnyLabelingJSON(
				inspMoldingOutInfo.locate.details,
				inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg",
				inspMoldingOutInfo.images.outputImg.mat().rows,
				inspMoldingOutInfo.images.outputImg.mat().cols
			);
			saveJSONToFile(jsonData, "D:/TRAIN_DATA_2/MOLDING/LOCATE/BAD/"+ inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".json");
			cv::imwrite("D:/TRAIN_DATA_2/MOLDING/LOCATE/BAD/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg", inspMoldingOutInfo.images.outputImg.mat());
		}

		inspMoldingOutInfo.status.errorMessage = "坏料!";
		LOG->WriteLog("坏料!", ERR, inspMoldingOutInfo.paths.logFile, true);
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_FIND_BAD;
		return;
	}

	if (inspMoldingOutInfo.locate.okDetails.empty()) {
		if (m_params.saveTrain)
		{
			COM->CreateDir("D:/TRAIN_DATA_2/MOLDING/LOCATE/NONE");
			auto jsonData = generateXAnyLabelingJSON(
				inspMoldingOutInfo.locate.details,
				inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg",
				inspMoldingOutInfo.images.outputImg.mat().rows,
				inspMoldingOutInfo.images.outputImg.mat().cols
			);
			saveJSONToFile(jsonData, "D:/TRAIN_DATA_2/MOLDING/LOCATE/NONE/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".json");
			cv::imwrite("D:/TRAIN_DATA_2/MOLDING/LOCATE/NONE/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg", inspMoldingOutInfo.images.outputImg.mat());
		}

		inspMoldingOutInfo.status.errorMessage = "无目标!";
		LOG->WriteLog("无目标!", ERR, inspMoldingOutInfo.paths.logFile, true);
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_FIND_NONE;
		return;
	}
	else if (inspMoldingOutInfo.locate.okDetails.size() > 2) {
		if (m_params.saveTrain)
		{
			COM->CreateDir("D:/TRAIN_DATA_2/MOLDING/LOCATE/MULTY");
			auto jsonData = generateXAnyLabelingJSON(
				inspMoldingOutInfo.locate.details,
				inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg",
				inspMoldingOutInfo.images.outputImg.mat().rows,
				inspMoldingOutInfo.images.outputImg.mat().cols
			);
			saveJSONToFile(jsonData, "D:/TRAIN_DATA_2/MOLDING/LOCATE/MULTY/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".json");
			cv::imwrite("D:/TRAIN_DATA_2/MOLDING/LOCATE/MULTY/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg", inspMoldingOutInfo.images.outputImg.mat());
		}

		inspMoldingOutInfo.status.errorMessage = "多目标!";
		LOG->WriteLog("无目标!", ERR, inspMoldingOutInfo.paths.logFile, true);
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_FIND_MULTY;
		return;
	}
	else if (inspMoldingOutInfo.locate.okDetails.size() < 2) {
		if (m_params.saveTrain)
		{
			COM->CreateDir("D:/TRAIN_DATA_2/MOLDING/LOCATE/MISS");
			auto jsonData = generateXAnyLabelingJSON(
				inspMoldingOutInfo.locate.details,
				inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg",
				inspMoldingOutInfo.images.outputImg.mat().rows,
				inspMoldingOutInfo.images.outputImg.mat().cols
			);
			saveJSONToFile(jsonData, "D:/TRAIN_DATA_2/MOLDING/LOCATE/MISS/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".json");
			cv::imwrite("D:/TRAIN_DATA_2/MOLDING/LOCATE/MISS/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg", inspMoldingOutInfo.images.outputImg.mat());
		}

		inspMoldingOutInfo.status.errorMessage = "少目标!";
		LOG->WriteLog("少目标!", ERR, inspMoldingOutInfo.paths.logFile, true);
		inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_FIND_MISS;
		return;
	}
	else
	{
		if (m_params.saveTrain == 2)
		{
			COM->CreateDir("D:/TRAIN_DATA_2/MOLDING/LOCATE/OK");
			auto jsonData = generateXAnyLabelingJSON(
				inspMoldingOutInfo.locate.details,
				inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg",
				inspMoldingOutInfo.images.outputImg.mat().rows,
				inspMoldingOutInfo.images.outputImg.mat().cols
			);
			saveJSONToFile(jsonData, "D:/TRAIN_DATA_2/MOLDING/LOCATE/OK/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".json");
			cv::imwrite("D:/TRAIN_DATA_2/MOLDING/LOCATE/OK/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg", inspMoldingOutInfo.images.outputImg.mat());
		}
		LOG->WriteLog("定位工件成功!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
	}

	for (int i = 0; i < inspMoldingOutInfo.locate.details.size(); i++)
	{
		float angle = CAL->CALC_GetAbsoluteAngle(inspMoldingOutInfo.locate.details[i].box) - 90;
		cv::Mat rotationMatrix = cv::getRotationMatrix2D(inspMoldingOutInfo.locate.details[i].box.center, angle, 1.0);
		cv::Mat rotatedImage;
		warpAffine(inspMoldingOutInfo.images.roi.mat(), rotatedImage, rotationMatrix, inspMoldingOutInfo.images.roi.mat().size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));

		// 裁剪旋转区域
		cv::Rect boundingRect = inspMoldingOutInfo.locate.details[i].box.boundingRect();
		cv::Mat croppedRegion = rotatedImage(boundingRect);
		cv::transpose(croppedRegion, croppedRegion);
		inspMoldingOutInfo.images.moldingImgs.push_back(croppedRegion);

		//cv::namedWindow("croppedRegion", cv::WINDOW_NORMAL);
		//cv::imshow("croppedRegion", croppedRegion);
		//cv::waitKey(0);
	}
	
}

void InspMolding::HandleDefect(const char* dirName, MOLDING_RETURN_VAL  statusCode, InspMoldingOut& inspMoldingOutInfo, const char* errMsg)
{
	/*if (m_params.saveTrain)
	{
		std::string path = std::string("D:/TRAIN_DATA_2/MOLDING/DEFECT/") + dirName;
		COM->CreateDir(path.c_str());
		writeImageXml(path, inspMoldingOutInfo.system.startTime, m_img, inspMoldingOutInfo.system.cameraId, inspMoldingOutInfo.system.jobId, inspMoldingOutInfo.locate.details);
	}*/

	inspMoldingOutInfo.status.statusCode = statusCode;
	LOG->WriteLog(errMsg, ERR, inspMoldingOutInfo.paths.logFile, true);
}

void InspMolding::Molding_CheckDefect(InspMoldingOut& inspMoldingOutInfo) {
	if (inspMoldingOutInfo.status.statusCode != MOLDING_RETURN_OK) {
		LOG->WriteLog("跳过缺陷检测!", WARNING, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		LOG->WriteLog("开始缺陷检测!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
	}

	

	for (int mm = 0; mm < inspMoldingOutInfo.images.moldingImgs.size(); mm++)
	{
		if (m_params.defectWeightsFile.find(".xml") != std::string::npos)
		{
			inspMoldingOutInfo.defects.details = InferenceWorker::RunOpenVINO(m_params.defectWeightsFile, m_params.defectClassName, inspMoldingOutInfo.images.moldingImgs[mm], 0.1, 0.5);
		}
		else if (m_params.defectWeightsFile.find(".onnx") != std::string::npos)
		{
			inspMoldingOutInfo.defects.details = InferenceWorker::Run(m_params.defectWeightsFile, m_params.defectClassName, inspMoldingOutInfo.images.moldingImgs[mm], 0.1, 0.5);
		}
		else
		{
			inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_CONFIG_ERR;
			inspMoldingOutInfo.status.errorMessage = "模型文件异常，目前仅支持xml、onnx!";
			LOG->WriteLog("模型文件异常，目前仅支持xml、onnx!", ERR, inspMoldingOutInfo.paths.logFile, true);

			return;
		}

		/*if (m_params.saveDebugImage)
		{
			inspMoldingOutInfo.images.moldingRegionDefectLog.data = std::make_shared<cv::Mat>(inspMoldingOutInfo.images.moldingRegion.mat().clone());
			inspMoldingOutInfo.images.moldingRegionDefectLog.stageName = "Molding_CheckDefect";
			inspMoldingOutInfo.images.moldingRegionDefectLog.description = "缺陷定位";
			inspMoldingOutInfo.images.moldingRegionDefectLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
			DAS->DAS_FinsObject(inspMoldingOutInfo.images.moldingRegionDefectLog.mat(), inspMoldingOutInfo.locate.details, inspMoldingOutInfo.paths.intermediateImagesDir + "3.1.1.detections.jpg", m_params.saveDebugImage);
		}*/


		/*  0 word_LRP
			1 word_BARERR
			2 word_BARB
			3 word_BARD
			4 word_LEAKM
			5 word_CRIMP
			6 word_SCRAP
			7 word_TOPB
			8 word_NC
			9 word_NCT*/
		LOG->WriteLog("开始分析缺陷检测结果!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
		std::vector<cv::Rect> lrpRects;
		for (int i = inspMoldingOutInfo.defects.details.size() - 1; i >= 0; --i)
		{
			auto& defect = inspMoldingOutInfo.defects.details[i];
			int paramIndex = -1; // 根据缺陷类别设置对应参数索引

			bool valid = true;
			if (defect.className == "BREAK")         paramIndex = 0;  // 破损、划痕
			else if (defect.className == "BREAK_L")   paramIndex = 1;  // 轻微粘合异常
			else if (defect.className == "OVERFLOW") paramIndex = 2;  // 溢胶
			else if (defect.className == "OFFCUT") paramIndex = 3;  // 碎料

			if (paramIndex != -1)
			{
				auto& para = m_params.defectPara[paramIndex];
				if (defect.box.width < para.length.minimum ||
					defect.box.width > para.length.maximum ||
					defect.box.height < para.length.minimum ||
					defect.box.height > para.length.maximum ||
					defect.confidence < para.confidence_threshold)
				{
					valid = false;
				}
			}

			if (!valid) {
				// 移除不符合条件的缺陷
				inspMoldingOutInfo.defects.details.erase(inspMoldingOutInfo.defects.details.begin() + i);

			}
		}

		if (inspMoldingOutInfo.defects.details.empty())
		{
			if (m_params.saveTrain == 2)
			{
				COM->CreateDir("D:/TRAIN_DATA_2/MOLDING/DEFECT/OK");
				auto jsonData = generateXAnyLabelingJSON(
					inspMoldingOutInfo.defects.details,
					inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg",
					inspMoldingOutInfo.images.moldingImgs[mm].rows,
					inspMoldingOutInfo.images.moldingImgs[mm].cols
				);
				saveJSONToFile(jsonData, "D:/TRAIN_DATA_2/MOLDING/DEFECT/OK/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) +  std::to_string(mm) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".json");
				cv::imwrite("D:/TRAIN_DATA_2/MOLDING/DEFECT/OK/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(mm) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg", inspMoldingOutInfo.images.moldingImgs[mm]);
			}
		}

		for (int i = 0; i < inspMoldingOutInfo.defects.details.size(); i++)
		{
			if (inspMoldingOutInfo.defects.details[i].className == "BREAK")
			{
				if (inspMoldingOutInfo.defects.details[i].box.width < m_params.defectPara[i].length.minimum ||
					inspMoldingOutInfo.defects.details[i].box.width > m_params.defectPara[i].length.maximum ||
					inspMoldingOutInfo.defects.details[i].confidence < m_params.defectPara[i].confidence_threshold)
				{

				}
				if (m_params.saveTrain)
				{
					COM->CreateDir("D:/TRAIN_DATA_2/MOLDING/DEFECT/BREAK");
					auto jsonData = generateXAnyLabelingJSON(
						inspMoldingOutInfo.defects.details,
						inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg",
						inspMoldingOutInfo.images.moldingImgs[mm].rows,
						inspMoldingOutInfo.images.moldingImgs[mm].cols
					);
					saveJSONToFile(jsonData, "D:/TRAIN_DATA_2/MOLDING/DEFECT/BREAK/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(mm) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".json");
					cv::imwrite("D:/TRAIN_DATA_2/MOLDING/DEFECT/BREAK/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(mm) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg", inspMoldingOutInfo.images.moldingImgs[mm]);
				}

				inspMoldingOutInfo.defects.findHJ = true;
				inspMoldingOutInfo.status.errorMessage = "缺胶、断裂、划痕!";
				LOG->WriteLog("缺胶、断裂、划痕!", ERR, inspMoldingOutInfo.paths.logFile, true);
				inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_FIND_HJ;
			}
			else if (inspMoldingOutInfo.defects.details[i].className == "BREAK_L")
			{
				if (inspMoldingOutInfo.defects.details[i].box.width < m_params.defectPara[i].length.minimum ||
					inspMoldingOutInfo.defects.details[i].box.width > m_params.defectPara[i].length.maximum ||
					inspMoldingOutInfo.defects.details[i].confidence < m_params.defectPara[i].confidence_threshold)
				{

				}
				if (m_params.saveTrain)
				{
					COM->CreateDir("D:/TRAIN_DATA_2/MOLDING/DEFECT/BREAK_L");
					auto jsonData = generateXAnyLabelingJSON(
						inspMoldingOutInfo.defects.details,
						inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg",
						inspMoldingOutInfo.images.moldingImgs[mm].rows,
						inspMoldingOutInfo.images.moldingImgs[mm].cols
					);
					saveJSONToFile(jsonData, "D:/TRAIN_DATA_2/MOLDING/DEFECT/BREAK_L/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(mm) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".json");
					cv::imwrite("D:/TRAIN_DATA_2/MOLDING/DEFECT/BREAK_L/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(mm) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg", inspMoldingOutInfo.images.moldingImgs[mm]);
					
				}
				inspMoldingOutInfo.defects.findHJ = true;
				inspMoldingOutInfo.status.errorMessage = "轻微粘合异常!";
				LOG->WriteLog("轻微粘合异常!", ERR, inspMoldingOutInfo.paths.logFile, true);
				inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_FIND_HJL;
			}
			else if (inspMoldingOutInfo.defects.details[i].className == "OVERFLOW")
			{
				if (m_params.saveTrain)
				{
					COM->CreateDir("D:/TRAIN_DATA_2/MOLDING/DEFECT/OVERFLOW");
					auto jsonData = generateXAnyLabelingJSON(
						inspMoldingOutInfo.defects.details,
						inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg",
						inspMoldingOutInfo.images.moldingImgs[mm].rows,
						inspMoldingOutInfo.images.moldingImgs[mm].cols
					);
					saveJSONToFile(jsonData, "D:/TRAIN_DATA_2/MOLDING/DEFECT/OVERFLOW/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(mm) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".json");
					cv::imwrite("D:/TRAIN_DATA_2/MOLDING/DEFECT/OVERFLOW/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(mm) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg", inspMoldingOutInfo.images.moldingImgs[mm]);

				}
				inspMoldingOutInfo.defects.findEE = true;
				inspMoldingOutInfo.status.errorMessage = "多胶/飞边!";
				LOG->WriteLog("多胶/飞边!", ERR, inspMoldingOutInfo.paths.logFile, true);
				inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_FIND_EE;
			}
			else if (inspMoldingOutInfo.defects.details[i].className == "OFFCUT")
			{
				if (m_params.saveTrain)
				{
					COM->CreateDir("D:/TRAIN_DATA_2/MOLDING/DEFECT/OFFCUT");
					auto jsonData = generateXAnyLabelingJSON(
						inspMoldingOutInfo.defects.details,
						inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg",
						inspMoldingOutInfo.images.moldingImgs[mm].rows,
						inspMoldingOutInfo.images.moldingImgs[mm].cols
					);
					saveJSONToFile(jsonData, "D:/TRAIN_DATA_2/MOLDING/DEFECT/OFFCUT/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(mm) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".json");
					cv::imwrite("D:/TRAIN_DATA_2/MOLDING/DEFECT/OFFCUT/" + inspMoldingOutInfo.system.startTime + std::to_string(inspMoldingOutInfo.system.jobId) + std::to_string(mm) + std::to_string(inspMoldingOutInfo.system.cameraId) + ".jpg", inspMoldingOutInfo.images.moldingImgs[mm]);

					
				}
				inspMoldingOutInfo.defects.findFR = true;
				inspMoldingOutInfo.status.errorMessage = "碎料!";
				LOG->WriteLog("碎料!", ERR, inspMoldingOutInfo.paths.logFile, true);
				inspMoldingOutInfo.status.statusCode = MOLDING_RETURN_FIND_OC;
			}		
		}

		for (int kk = 0; kk < inspMoldingOutInfo.defects.details.size(); kk++)
		{
			cv::rectangle(inspMoldingOutInfo.images.moldingImgs[mm], inspMoldingOutInfo.defects.details[kk].box, Colors::RED, 5, cv::LINE_AA);
		}
	}
	
	
}


void InspMolding::Molding_DrawResult(InspMoldingOut& inspMoldingOutInfo) {
	LOG->WriteLog("开始绘制结果!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);

	inspMoldingOutInfo.images.outputImg.data = std::make_shared<cv::Mat>(m_img.clone());
	inspMoldingOutInfo.images.outputImg.stageName = "Molding_DrawResult";
	inspMoldingOutInfo.images.outputImg.description = "绘制全部结果: " + std::to_string(m_params.saveDebugImage);
	inspMoldingOutInfo.images.outputImg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();


	rectangle(inspMoldingOutInfo.images.outputImg.mat(), m_params.roiRect, Colors::YELLOW, 1, cv::LINE_AA);

	for (int i = 0; i < inspMoldingOutInfo.locate.details.size(); i++)
	{
		cv::Point2f vertices[4];
		inspMoldingOutInfo.locate.details[i].box.points(vertices);
		for (int j = 0; j < 4; j++) {
			cv::line(inspMoldingOutInfo.images.outputImg.mat(), vertices[j], vertices[(j + 1) % 4], Colors::GREEN, 8, cv::LINE_AA);
		}
	}
	for (int i = 0; i < inspMoldingOutInfo.locate.badDetails.size(); i++)
	{
		cv::Point2f vertices[4];
		inspMoldingOutInfo.locate.badDetails[i].box.points(vertices);
		for (int j = 0; j < 4; j++) {
			cv::line(inspMoldingOutInfo.images.outputImg.mat(), vertices[j], vertices[(j + 1) % 4], Colors::RED, 8, cv::LINE_AA);
		}
	}

	if (inspMoldingOutInfo.status.statusCode == MOLDING_RETURN_FIND_HJ ||
		inspMoldingOutInfo.status.statusCode == MOLDING_RETURN_FIND_EE ||
		inspMoldingOutInfo.status.statusCode == MOLDING_RETURN_FIND_HJL ||
		inspMoldingOutInfo.status.statusCode == MOLDING_RETURN_FIND_OC )
	{
		for (int i = 0; i < inspMoldingOutInfo.locate.details.size(); i++) {
			// 1. 重新计算变换参数（需与原始操作一致）
			float angle = CAL->CALC_GetAbsoluteAngle(inspMoldingOutInfo.locate.details[i].box) - 90;
			cv::Point2f center = inspMoldingOutInfo.locate.details[i].box.center;
			cv::Rect boundingRect = inspMoldingOutInfo.locate.details[i].box.boundingRect();

			// 2. 恢复方向并重建旋转图像
			cv::Mat processedImgCorrected;
			cv::transpose(inspMoldingOutInfo.images.moldingImgs[i], processedImgCorrected);

			cv::Mat newRotatedImage = cv::Mat::zeros(inspMoldingOutInfo.images.roi.mat().size(),
				inspMoldingOutInfo.images.roi.mat().type());
			processedImgCorrected.copyTo(newRotatedImage(boundingRect));

			// 3. 逆旋转
			cv::Mat inverseRotationMatrix = cv::getRotationMatrix2D(center, -angle, 1.0);
			cv::Mat inverseRotatedImage;
			cv::warpAffine(newRotatedImage, inverseRotatedImage, inverseRotationMatrix,
				newRotatedImage.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));

			// 4. 掩码覆盖到原图
			cv::Mat mask;
			cv::cvtColor(inverseRotatedImage, mask, cv::COLOR_BGR2GRAY);
			cv::threshold(mask, mask, 1, 255, cv::THRESH_BINARY);
			inverseRotatedImage.copyTo(inspMoldingOutInfo.images.roi.mat(), mask);
		}
		inspMoldingOutInfo.images.roi.mat().copyTo(inspMoldingOutInfo.images.outputImg.mat()(m_params.roiRect));
	}
	

	std::string rv = "ID = " + std::to_string(inspMoldingOutInfo.system.jobId) + ", " + "RV = " + std::to_string(inspMoldingOutInfo.status.statusCode) + ", " + inspMoldingOutInfo.status.errorMessage;
	if (inspMoldingOutInfo.status.statusCode == MOLDING_RETURN_OK) {
		putTextZH(inspMoldingOutInfo.images.outputImg.mat(), rv.c_str(), cv::Point(15, 30), Colors::GREEN, 300, FW_BOLD);
	}
	else {
		putTextZH(inspMoldingOutInfo.images.outputImg.mat(), rv.c_str(), cv::Point(15, 30), Colors::RED, 300, FW_BOLD);
	}
	LOG->WriteLog(rv, INFO, inspMoldingOutInfo.paths.logFile, true);



	
	DAS->DAS_Img(inspMoldingOutInfo.images.outputImg.mat(), inspMoldingOutInfo.paths.intermediateImagesDir + "10.outputImg.jpg", m_params.saveDebugImage);


	
}

int InspMolding::Molding_Main(InspMoldingOut& inspMoldingOutInfo) {
	double time0 = static_cast<double>(cv::getTickCount());
	LOG->WriteLog("Molding_Main!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);

	if (inspMoldingOutInfo.status.statusCode != MOLDING_RETURN_OK) {
		LOG->WriteLog("跳过获取模型!", WARNING, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
	}
	else
	{
		LOG->WriteLog("获取模型!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);
		std::lock_guard<std::mutex> lock(modelLoadMutex);
		m_params.locateWeightsFile = moldingDetectionModelMap.at("moldingDetection_" + std::to_string(inspMoldingOutInfo.system.cameraId));
		m_params.defectWeightsFile = moldingDefectModelMap.at("moldingDefect_" + std::to_string(inspMoldingOutInfo.system.cameraId));
	}

	try {

		// 第1步:定位工件
		Molding_SetROI(inspMoldingOutInfo);

		// 第2步:定位工件
		Molding_Locate(inspMoldingOutInfo);

		// 第3步:缺陷检测
		Molding_CheckDefect(inspMoldingOutInfo);


		// 第9步:绘制结果
		Molding_DrawResult(inspMoldingOutInfo);

		if (inspMoldingOutInfo.status.statusCode == MOLDING_RETURN_OK) {
			DAS->DAS_Img(inspMoldingOutInfo.images.outputImg.mat(),
				inspMoldingOutInfo.paths.resultsOKDir + std::to_string(inspMoldingOutInfo.system.jobId) + ".jpg",
				m_params.saveResultImage);
		}
		else {
			DAS->DAS_Img(inspMoldingOutInfo.images.outputImg.mat(),
				inspMoldingOutInfo.paths.resultsNGDir + std::to_string(inspMoldingOutInfo.system.jobId) + ".jpg",
				m_params.saveResultImage);
		}
		LOG->WriteLog("END!", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt);

		time0 = ((double)cv::getTickCount() - time0) / cv::getTickFrequency() * 1000;
		LOG->WriteLog("算法耗时：", INFO, inspMoldingOutInfo.paths.logFile, m_params.saveLogTxt, time0);
	}
	catch (const std::exception& e) {
		std::cerr << "[ERROR] Inference failed: " << e.what() << std::endl;
		return MOLDING_RETURN_ALGO_ERR;
	}

	return inspMoldingOutInfo.status.statusCode;
}