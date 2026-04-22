#include "HeaderDefine.h"
#include "InspBottleNum.h"
#include "InspBottleNumStruct.h"
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
std::shared_mutex InspBottleNum::modelLoadMutex;
std::map<std::string, std::string> InspBottleNum::bottleNumDetectionModelMap;
std::map<std::string, std::string> InspBottleNum::capClassifyModelMap;
std::map<std::string, std::string> InspBottleNum::handleClassifyModelMap;
std::map<int, InspBottleNumIn> InspBottleNum::cameraConfigMap;

// 构造函数
InspBottleNum::InspBottleNum(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
	bool isLoadConfig, int timeOut, InspBottleNumOut& outInfo)
	: ANA(std::make_unique<AnalyseMat>()),
	LOG(std::make_unique<Log>()),
	COM(std::make_unique<Common>()),
	TXT(std::make_unique<TxtOperater>())
{
	m_safeTimeoutFlag = std::make_shared<std::atomic<bool>>(false);

	// 将外部引用指向这个安全标志
	if (outInfo.system.timeoutFlag) {
		*m_timeoutFlagRef = m_safeTimeoutFlag->load();
	}
	else {
		m_timeoutFlagRef = nullptr;
	}

	m_img = img.clone();
	m_params.timeOut = timeOut;
	m_timeoutFlagRef = &outInfo.system.timeoutFlag;
	COM->CreateDir(outInfo.paths.logDirectory);
	Log::WriteAsyncLog("********** Start Inspction JobID = ", INFO, outInfo.paths.logFile, true, outInfo.system.jobId, " ***********");

	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << outInfo.system.cameraId << "  jobId_" << outInfo.system.jobId << std::endl;

	char bufLog[100];
	sprintf(bufLog, "BottleNum/camera_%d/", outInfo.system.cameraId);
	char bufConfig[100];
	sprintf(bufConfig, "/InspBottleNumConfig_%d.txt", outInfo.system.cameraId);
	outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + string(bufLog);
	outInfo.paths.intermediateImagesDir =
		ProjectConstants::LOG_PATH + string(bufLog) + "IMG/" + to_string(outInfo.system.jobId) + "/";
	outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + string(bufLog) + "OK/";
	outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + string(bufLog) + "NG/";
	outInfo.paths.configFile = configPath + string(bufConfig);
	outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

	COM->CreateDir(outInfo.paths.logDirectory);
	LOG->WriteLog("********** Start Inspection JobID = ", INFO, outInfo.paths.logFile, true, outInfo.system.jobId, " ***********");

	bool shouldLoadConfig = isLoadConfig ||
		jobId == 0 ||
		cameraConfigMap.find(cameraId) == cameraConfigMap.end();

	//读取config
	if (shouldLoadConfig)
	{
		bool rv_loadConfig = readParams(img, outInfo.paths.configFile, m_params, outInfo, outInfo.paths.logFile);
		if (!rv_loadConfig) {
			outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
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
			outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
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
			outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("定位阈值文件-参数设置错误！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "定位阈值文件-参数设置错误!";
			return;
		}
		else
		{
			Log::WriteAsyncLog("定位参数读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}

		if (LoadConfigBottleType(m_params.targetConfigFile,
			m_params.targetType,
			outInfo.paths.logFile) != 1)
		{
			outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("检测品种配置文件-参数设置错误！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "检测品种配置文件-参数设置错误!";
			return;
		}
		else
		{
			Log::WriteAsyncLog("检测品种配置文件读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

			// 检查是否有重复的 bottleType 名称
			std::set<std::string> uniqueBottleTypes;
			bool hasDuplicate = false;
			std::string duplicateType;

			for (const auto& type : m_params.targetType) {
				// 确保 bottleType 不为空
				if (type.bottleType.empty()) {
					outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
					Log::WriteAsyncLog("检测品种配置文件中存在空瓶型名称!", ERR, outInfo.paths.logFile, true);
					outInfo.status.errorMessage = "检测品种配置文件中存在空瓶型名称!";
					return;
				}

				// 检查是否重复
				if (uniqueBottleTypes.find(type.bottleType) != uniqueBottleTypes.end()) {
					hasDuplicate = true;
					duplicateType = type.bottleType;
					break;
				}
				uniqueBottleTypes.insert(type.bottleType);
			}

			if (hasDuplicate) {
				outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
				std::string errorMsg = "配置存在重复瓶型: " + duplicateType;
				Log::WriteAsyncLog(errorMsg, ERR, outInfo.paths.logFile, true);
				outInfo.status.errorMessage = errorMsg;
				return;
			}

		}

		for (int i = 0; i < m_params.targetType.size(); i++)
		{
			m_params.bottleNum += m_params.targetType[i].num;
		}

		//读取分类类型名称
		if (m_params.isCheckColor)
		{
			std::ifstream ifs1(m_params.capClassifyNameFile.c_str());
			if (!ifs1.is_open()) {
				outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "瓶盖分类类型文件缺失!";
				Log::WriteAsyncLog(m_params.capClassifyNameFile, ERR, outInfo.paths.logFile, true, "---瓶盖分类类型文件缺失!");
				return;
			}
			else
			{
				m_params.capClassifyClassName.clear();
				std::string line;
				while (getline(ifs1, line)) m_params.capClassifyClassName.push_back(line);

				m_params.capClassifyClassName.push_back("坏盖");
				Log::WriteAsyncLog("瓶盖分类类型文件读取成功！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			}

			std::ifstream ifs2(m_params.handleClassifyNameFile.c_str());
			if (!ifs2.is_open()) {
				outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "提手分类类型文件缺失!";
				Log::WriteAsyncLog(m_params.handleClassifyNameFile, ERR, outInfo.paths.logFile, true, "---提手分类类型文件缺失!");
				return;
			}
			else
			{
				m_params.handleClassifyNameFile.clear();
				std::string line;
				while (getline(ifs2, line)) m_params.handleClassifyClassName.push_back(line);

				m_params.handleClassifyClassName.push_back("坏提手");
				Log::WriteAsyncLog("提手分类类型文件读取成功！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);


			}

			for (int i = 0; i < m_params.targetType.size(); i++)
			{
				bool findCapType = (std::find(m_params.capClassifyClassName.begin(),
					m_params.capClassifyClassName.end(),
					m_params.targetType[i].capType) != m_params.capClassifyClassName.end());
				if (!findCapType)
				{
					outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "当前选择的瓶盖类型不存在!";
					Log::WriteAsyncLog("当前选择的瓶盖类型不存在!", ERR, outInfo.paths.logFile, true);
					return;
				}


				if (m_params.targetType[i].handleType != "无提手")
				{
					bool findHandleType = (std::find(m_params.handleClassifyClassName.begin(),
						m_params.handleClassifyClassName.end(),
						m_params.targetType[i].handleType) != m_params.handleClassifyClassName.end());
					if (!findHandleType)
					{
						outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
						outInfo.status.errorMessage = "当前选择的提手类型不存在!";
						Log::WriteAsyncLog("当前选择的提手类型不存在!", ERR, outInfo.paths.logFile, true);
						return;
					}

				}

			}
		}
		else
		{
			for (int i = 0; i < m_params.targetType.size(); i++)
			{
				if (COM->Contains(m_params.targetType[i].capType, "盖"))
				{
					m_params.targetType[i].capType = "瓶盖";
				}
				if (COM->Contains(m_params.targetType[i].handleType, "提手"))
				{
					if (m_params.targetType[i].handleType != "无提手")
					{
						m_params.targetType[i].handleType = "提手";
					}
				}
				m_params.targetType[i].bottleType = m_params.targetType[i].capType + m_params.targetType[i].handleType;
			}
		}

		bool loadModel = loadAllModels(outInfo, true);
		if (!loadModel) {
			outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "深度学习模型加载异常!";
			Log::WriteAsyncLog(m_params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "---深度学习模型加载异常!");
			return;
		}
		if (!validateCameraModels(outInfo.system.cameraId)) {
			Log::WriteAsyncLog("相机ID配置错误/模型文件缺失!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "相机ID配置错误/模型文件缺失!";
			throw std::invalid_argument("相机ID配置错误/模型文件缺失!");
		}

		cameraConfigMap[cameraId] = m_params;
	}
	else
	{
		m_params = cameraConfigMap[cameraId];
	}



	if (outInfo.status.statusCode = BOTTLENUM_RETURN_OK)
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




// 验证摄像头ID对应的模型配置是否存在
bool InspBottleNum::validateCameraModels(int cameraId) {
	std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
	return bottleNumDetectionModelMap.count("bottleNumDetection_" + std::to_string(cameraId)) &&
		capClassifyModelMap.count("capClassify_" + std::to_string(cameraId)) &&
		handleClassifyModelMap.count("handleClassify_" + std::to_string(cameraId));
}

// 加载所有模型到ModelManager
bool InspBottleNum::loadAllModels(InspBottleNumOut& outInfo, bool ini) {
	if (!ini) {
		Log::WriteAsyncLog("跳过模型加载!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return true;
	}

	const int cameraId = outInfo.system.cameraId;
	const cv::String key = std::to_string(cameraId);

	// 获取当前相机专用模型路径
	std::vector<std::string> cameraModelPaths;

	// 1. 添加检测模型
	std::string detectionKey = "bottleNumDetection_" + std::to_string(cameraId);
	if (auto it = bottleNumDetectionModelMap.find(detectionKey); it != bottleNumDetectionModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	// 2. 添加分类模型
	std::string classifyKeyCap = "capClassify_" + std::to_string(cameraId);
	if (auto it = capClassifyModelMap.find(classifyKeyCap); it != capClassifyModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	// 3. 添加分类模型
	std::string classifyKeyHandle = "handleClassify_" + std::to_string(cameraId);
	if (auto it = handleClassifyModelMap.find(classifyKeyHandle); it != handleClassifyModelMap.end()) {
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
		if (m_params.isCheckColor)
		{
			FinsClassification capType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.capClassifyWeightsFile, m_params.capClassifyClassName, iniImg);
			FinsClassification handleType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.handleClassifyWeightsFile, m_params.handleClassifyClassName, iniImg);
		}

		Log::WriteAsyncLog("模型初始化完成！", INFO, outInfo.paths.logFile, true);

		return true;
	}
	catch (const std::exception& e) {
		Log::WriteAsyncLog("相机" + std::to_string(cameraId) + "模型加载异常: " + std::string(e.what()), ERR, outInfo.paths.logFile, true);
		return false;
	}
}

// 读取参数的函数
bool InspBottleNum::readParams(cv::Mat img, const std::string& filePath, InspBottleNumIn& params, InspBottleNumOut& outInfo, const std::string& fileName) {
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
		if (keyWord == "BOTTLENUM_SAVE_DEBUG_IMAGE") {
			params.saveDebugImage = std::stoi(value);
		}
		else if (keyWord == "BOTTLENUM_SAVE_RESULT_IMAGE") {
			params.saveResultImage = std::stoi(value);
		}
		else if (keyWord == "BOTTLENUM_SAVE_LOG_TXT") {
			params.saveLogTxt = std::stoi(value);
		}
		else if (keyWord == "BOTTLENUM_DRAW_RESULT") {
			params.drawResult = std::stoi(value);
		}
		else if (keyWord == "BOTTLENUM_SAVE_TRAIN") {
			params.saveTrain = std::stoi(value);
		}
		else if (keyWord == "BOTTLENUM_ROI_X") {
			params.roiRect.x = std::stoi(value);
			if (params.roiRect.x < 0 || params.roiRect.x > img.cols)
			{
				outInfo.status.errorMessage = "ROI_X: 超出图像范围!";
				Log::WriteAsyncLog("ROI_X: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENUM_ROI_Y") {
			params.roiRect.y = std::stoi(value);
			if (params.roiRect.y < 0 || params.roiRect.y > img.rows)
			{
				outInfo.status.errorMessage = "ROI_Y: 超出图像范围!";
				Log::WriteAsyncLog("ROI_Y: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOTTLENUM_ROI_W") {
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
		else if (keyWord == "BOTTLENUM_ROI_H") {
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

		else if (keyWord == "BOTTLENUM_HARDWARE_TYPE") {
			params.hardwareType = std::stoi(value);
		}
		else if (keyWord == "BOTTLENUM_MODEL_TYPE") {
			params.modelType = std::stoi(value);
		}
		else if (keyWord == "BOTTLENUM_CHECK_COLOR") {
			params.isCheckColor = std::stoi(value);
		}
		else if (keyWord == "BOTTLENUM_LOCATE_WEIGHTS_FLIE") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
			std::string camera = std::to_string(outInfo.system.cameraId);
			bottleNumDetectionModelMap["bottleNumDetection_" + camera] = value;
			params.locateWeightsFile = value;
			if (!COM->FileExistsModern(params.locateWeightsFile))
			{
				outInfo.status.errorMessage = "定位模型文件缺失!";
				Log::WriteAsyncLog(params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "--定位模型文件文件缺失！");
				return false;
			}
		}
		else if (keyWord == "BOTTLENUM_LOCATE_CONFIG_FLIE") {
			params.locateThreshConfig = value;
			if (!COM->FileExistsModern(params.locateThreshConfig))
			{
				outInfo.status.errorMessage = "定位阈值文件缺失!";
				Log::WriteAsyncLog(params.locateThreshConfig, ERR, outInfo.paths.logFile, true, "--定位阈值文件缺失！");
				return false;
			}
		}
		else if (keyWord == "BOTTLENUM_CAP_CLASSFY_WEIGHTS_FILE") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);  // 加锁
			std::string camera = std::to_string(outInfo.system.cameraId);
			capClassifyModelMap["capClassify_" + camera] = value;
			params.capClassifyWeightsFile = value;
			if (params.isCheckColor)
			{
				if (!COM->FileExistsModern(params.capClassifyWeightsFile))
				{
					outInfo.status.errorMessage = "瓶盖分类模型文件缺失!";
					Log::WriteAsyncLog(params.capClassifyWeightsFile, ERR, outInfo.paths.logFile, true, "--瓶盖分类模型文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "BOTTLENUM_CAP_CLASSFY_CLASSES_FILE") {
			params.capClassifyNameFile = value;
			if (params.isCheckColor)
			{
				if (!COM->FileExistsModern(params.capClassifyNameFile))
				{
					outInfo.status.errorMessage = "瓶盖分类类型文件缺失!";
					Log::WriteAsyncLog(params.capClassifyNameFile, ERR, outInfo.paths.logFile, true, "--瓶盖分类类型文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "BOTTLENUM_HANDLE_CLASSFY_WEIGHTS_FILE") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);  // 加锁
			std::string camera = std::to_string(outInfo.system.cameraId);
			handleClassifyModelMap["handleClassify_" + camera] = value;
			params.handleClassifyWeightsFile = value;
			if (params.isCheckColor)
			{
				if (!COM->FileExistsModern(params.handleClassifyWeightsFile))
				{
					outInfo.status.errorMessage = "提手分类模型文件缺失!";
					Log::WriteAsyncLog(params.handleClassifyWeightsFile, ERR, outInfo.paths.logFile, true, "--提手分类模型文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "BOTTLENUM_HANDLE_CLASSFY_CLASSES_FILE") {
			params.handleClassifyNameFile = value;
			if (params.isCheckColor)
			{
				if (!COM->FileExistsModern(params.handleClassifyNameFile))
				{
					outInfo.status.errorMessage = "提手分类类型文件缺失!";
					Log::WriteAsyncLog(params.handleClassifyNameFile, ERR, outInfo.paths.logFile, true, "--提手分类类型文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "BOTTLENUM_TYPE_CONFIG") {
			params.targetConfigFile = value;
			if (params.isCheckColor)
			{
				if (!COM->FileExistsModern(params.targetConfigFile))
				{
					outInfo.status.errorMessage = "目标类型配置文件缺失!";
					Log::WriteAsyncLog(params.targetConfigFile, ERR, outInfo.paths.logFile, true, "--目标类型配置文件缺失！");
					return false;
				}
			}
		}
	}

	ifs.close();
	return true;
}


void InspBottleNum::BottleNum_SetROI(InspBottleNumOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != BOTTLENUM_RETURN_OK) {
		Log::WriteAsyncLog("跳过ROI区域获取!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始ROI区域获取!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	outInfo.images.roi.data = std::make_shared<cv::Mat>(m_img(m_params.roiRect).clone());
	outInfo.images.roi.stageName = "BottleNum_Main";
	outInfo.images.roi.description = "ROI区域获取";
	outInfo.images.roi.timestamp = std::chrono::system_clock::now().time_since_epoch().count();


}

void InspBottleNum::BottleNum_LocateBottle(InspBottleNumOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != BOTTLENUM_RETURN_OK) {
		Log::WriteAsyncLog("跳过定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始定位!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (m_params.locateWeightsFile.find(".onnx") != std::string::npos)
	{
		outInfo.locate.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.locateWeightsFile, m_params.locateClassName, outInfo.images.roi.mat(), 0.1, 0.5);
	}
	else
	{
		outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
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
		if (locate.className == "提手")paramIndex = 1;
		else if (locate.className == "瓶盖")	paramIndex = 0;

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
			outInfo.locate.badDetails.push_back(outInfo.locate.details[i]);
			outInfo.locate.details.erase(outInfo.locate.details.begin() + i);
		}
	}


}


void InspBottleNum::BottleNum_CheckBottleType(InspBottleNumOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != BOTTLENUM_RETURN_OK) {
		Log::WriteAsyncLog("跳过类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}

	if (!m_params.isCheckColor) {
		Log::WriteAsyncLog("跳过类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{
		FinsClassification classfyName;
		if (COM->Contains(outInfo.locate.details[i].className, "盖"))
		{
			if (m_params.capClassifyWeightsFile.find(".onnx") != std::string::npos)
			{
				classfyName = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.capClassifyWeightsFile, m_params.capClassifyClassName, m_img(outInfo.locate.details[i].box));
			}
			else
			{
				outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
				Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

				return;
			}
		}
		else if (COM->Contains(outInfo.locate.details[i].className, "提手"))
		{
			if (m_params.handleClassifyWeightsFile.find(".onnx") != std::string::npos)
			{
				classfyName = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.handleClassifyWeightsFile, m_params.handleClassifyClassName, m_img(outInfo.locate.details[i].box));
			}
			else
			{
				outInfo.status.statusCode = BOTTLENUM_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
				Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

				return;
			}
		}
		outInfo.locate.details[i].className = classfyName.className;
		//outInfo.locate.details[i].confidence = classfyName.confidence;
	}

}

void InspBottleNum::BottleNum_MatchBottles(InspBottleNumOut& outInfo) {
	std::vector<FinsObject> caps;
	std::vector<FinsObject> handles;

	// 分离瓶盖和提手
	for (const auto& obj : outInfo.locate.details) {
		if (COM->Contains(obj.className, "盖")) {
			caps.push_back(obj);
		}
		else if (COM->Contains(obj.className, "提手")) {
			handles.push_back(obj);
		}
	}

	std::vector<bool> capMatched(caps.size(), false);
	std::vector<bool> handleMatched(handles.size(), false);
	int bottleCount = outInfo.classification.bottleResult.size();

	// 1. 匹配瓶盖和提手
	for (int i = 0; i < caps.size(); ++i) {
		const auto& cap = caps[i];
		int bestMatchIdx = -1;
		float maxIoU = 0.0f;

		for (int j = 0; j < handles.size(); ++j) {
			if (handleMatched[j]) continue;

			const auto& handle = handles[j];
			cv::Rect intersection = cap.box & handle.box;

			if (intersection.area() > 0) {
				float iou = static_cast<float>(intersection.area()) / cap.box.area();

				if (iou > 0.5f && iou > maxIoU) {
					maxIoU = iou;
					bestMatchIdx = j;
				}
			}
		}

		if (bestMatchIdx != -1) {
			BottleResult bottle;
			bottle.capType = cap.className;
			bottle.capRect = cap.box;
			bottle.handleType = handles[bestMatchIdx].className;
			bottle.handleRect = handles[bestMatchIdx].box;
			bottle.num = ++bottleCount;
			bottle.bottleType = bottle.capType + bottle.handleType;
			bottle.capScore = caps[i].confidence;
			bottle.handleScore = handles[bestMatchIdx].confidence;
			outInfo.classification.bottleResult.push_back(bottle);
			capMatched[i] = true;
			handleMatched[bestMatchIdx] = true;
		}
	}

	// 2. 处理未匹配的瓶盖（无提手）
	for (int i = 0; i < caps.size(); ++i) {
		if (!capMatched[i]) {
			BottleResult bottle;
			bottle.capType = caps[i].className;
			bottle.capRect = caps[i].box;
			bottle.handleType = "无提手";
			bottle.handleRect = cv::Rect(); // 空矩形
			bottle.num = ++bottleCount;
			bottle.bottleType = bottle.capType + bottle.handleType;
			bottle.capScore = caps[i].confidence;

			outInfo.classification.bottleResult.push_back(bottle);
		}
	}

	// 3. 处理未匹配的提手（无盖）
	for (int j = 0; j < handles.size(); ++j) {
		if (!handleMatched[j]) {
			BottleResult bottle;
			bottle.capType = "*****";
			bottle.capRect = cv::Rect(); // 空矩形
			bottle.handleType = handles[j].className;
			bottle.handleRect = handles[j].box;
			bottle.num = ++bottleCount;
			bottle.bottleType = bottle.capType + bottle.handleType;
			bottle.handleScore = handles[j].confidence;

			outInfo.classification.bottleResult.push_back(bottle);
		}
	}
}

void InspBottleNum::BottleNum_CheckResult(InspBottleNumOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != BOTTLENUM_RETURN_OK) {
		Log::WriteAsyncLog("跳过识别结果检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}

	// 1. 检查总数是否匹配
	const int actualCount = outInfo.classification.bottleResult.size();
	if (actualCount > m_params.bottleNum) {
		outInfo.status.statusCode = BOTTLENUM_RETURN_BOTTLE_MORE;
		outInfo.status.errorMessage = "装箱数量多!";
		Log::WriteAsyncLog("装箱数量多!", ERR, outInfo.paths.logFile, true);
		//return;
	}
	else if (actualCount < m_params.bottleNum) {
		outInfo.status.statusCode = BOTTLENUM_RETURN_BOTTLE_LESS;
		outInfo.status.errorMessage = "装箱数量少!";
		Log::WriteAsyncLog("装箱数量少!", ERR, outInfo.paths.logFile, true);
		//return;
	}


	// 3. 统计目标类型
	std::map<std::string, int> targetCountMap;
	for (const auto& target : m_params.targetType) {
		std::string key = target.capType + "|" + target.handleType;
		targetCountMap[key] = target.num;
		outInfo.classification.checkResult.totalTarget += target.num;
	}

	// 4. 统计实际结果
	std::map<std::string, int> resultCountMap;
	for (auto& result : outInfo.classification.bottleResult) {
		std::string key = result.capType + "|" + result.handleType;
		resultCountMap[key]++;
		outInfo.classification.checkResult.totalActual++;

		if (targetCountMap.find(key) != targetCountMap.end() &&
			resultCountMap[key] <= targetCountMap[key]) {
			result.isMatched = true; 
		}
		if (result.capType == "坏盖")
		{
			outInfo.classification.checkResult.findBadCap = true;
		}
		if (result.handleType == "坏提手")
		{
			outInfo.classification.checkResult.findBadHandle = true;
		}
	}

	// 5. 比较每个类型
	for (const auto& target : targetCountMap) {
		TypeComparison comp;
		comp.typeKey = target.first;
		comp.targetNum = target.second;

		auto it = resultCountMap.find(target.first);
		if (it == resultCountMap.end()) {
			comp.actualNum = 0;
			comp.isMatch = false;
			comp.status = "未检测到";
			outInfo.classification.checkResult.allMatch = false;
		}
		else {
			comp.actualNum = it->second;
			comp.isMatch = (it->second == target.second);
			comp.status = comp.isMatch ? "匹配" : "数量错误";
			if (!comp.isMatch) outInfo.classification.checkResult.allMatch = false;

			// 从结果映射中移除已处理项
			resultCountMap.erase(it);
		}
		outInfo.classification.checkResult.typeComparisons.push_back(comp);
	}

	// 6. 处理错误类型
	for (const auto& result : resultCountMap) {
		outInfo.classification.checkResult.undefinedTypes.push_back(
			result.first + " (数量: " + std::to_string(result.second) + ")"
		);
		outInfo.classification.checkResult.allMatch = false;
	}

	// 7. 生成整体消息
	std::ostringstream message;
	if (outInfo.classification.checkResult.allMatch && outInfo.classification.checkResult.undefinedTypes.empty()) {
		message << "所有瓶子类型和数量匹配!";
	}
	else {
		// 添加类型不匹配信息
		for (const auto& comp : outInfo.classification.checkResult.typeComparisons) {
			if (!comp.isMatch) {
				message << "类型 '" << comp.typeKey << "': " << comp.status
					<< " (目标=" << comp.targetNum
					<< ", 实际=" << comp.actualNum << ")\n";
			}
		}

		// 添加错误类型信息
		for (const auto& undef : outInfo.classification.checkResult.undefinedTypes) {
			message << "检测错误类型: " << undef << "\n";
		}
	}
	outInfo.classification.checkResult.overallMessage = message.str();

	// 8. 存储详细结果到outInfo

	// 9. 设置状态
	if (outInfo.status.statusCode == BOTTLENUM_RETURN_OK)
	{
		if (outInfo.classification.checkResult.findBadCap)
		{
			outInfo.status.statusCode = BOTTLENUM_RETURN_BOTTLE_CAP_ERR;
			outInfo.status.errorMessage = "发现异常瓶盖!";
			Log::WriteAsyncLog("发现异常瓶盖!", ERR, outInfo.paths.logFile, true);
		}
		else if (outInfo.classification.checkResult.findBadHandle)
		{
			outInfo.status.statusCode = BOTTLENUM_RETURN_BOTTLE_HANDLE_ERR;
			outInfo.status.errorMessage = "发现异常提手!";
			Log::WriteAsyncLog("发现异常提手!", ERR, outInfo.paths.logFile, true);
		}
		else if (!outInfo.classification.checkResult.allMatch || !outInfo.classification.checkResult.undefinedTypes.empty()) {
			outInfo.status.statusCode = BOTTLENUM_RETURN_BOTTLE_TYPE_ERR;
			outInfo.status.errorMessage = "品种错误!";
			Log::WriteAsyncLog("品种错误!", ERR, outInfo.paths.logFile, true);
		}
		else {
			Log::WriteAsyncLog("所有瓶子匹配成功!", INFO, outInfo.paths.logFile, true);
		}
	}
	
}

void InspBottleNum::BottleNum_SaveForTrain(InspBottleNumOut& outInfo) {
	if (m_params.saveTrain == 0) return;

	if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
	{
		if (outInfo.status.statusCode == BOTTLENUM_RETURN_BOTTLE_MORE)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/MORE/");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/MORE/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/MORE/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

		}
		else if (outInfo.status.statusCode == BOTTLENUM_RETURN_BOTTLE_LESS)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/LESS/");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/LESS/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/LESS/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

		}
		else if (outInfo.status.statusCode == BOTTLENUM_RETURN_BOTTLE_TYPE_ERR)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/TYPE_ERR/");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/TYPE_ERR/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/TYPE_ERR/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

		}

		if (outInfo.status.statusCode != BOTTLENUM_RETURN_OK)
		{
			if (m_params.isCheckColor)
			{
				for (int i = 0; i < outInfo.locate.details.size(); i++)
				{
					outInfo.system.startTime = COM->time_t2string_with_ms();
					COM->CreateDir(outInfo.paths.trainDir + "CLASSFY/NG/" + outInfo.locate.details[i].className);
					cv::imwrite(outInfo.paths.trainDir + "CLASSFY/NG/" + outInfo.locate.details[i].className + "/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img(outInfo.locate.details[i].box));
				}
			}
		}

	}
	if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
	{
		if (outInfo.status.statusCode == BOTTLENUM_RETURN_OK)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/OK/");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);


			if (outInfo.status.statusCode == BOTTLENUM_RETURN_OK)
			{
				if (m_params.isCheckColor)
				{
					for (int i = 0; i < outInfo.locate.details.size(); i++)
					{
						outInfo.system.startTime = COM->time_t2string_with_ms();
						COM->CreateDir(outInfo.paths.trainDir + "CLASSFY/OK/" + outInfo.locate.details[i].className);
						cv::imwrite(outInfo.paths.trainDir + "CLASSFY/OK/" + outInfo.locate.details[i].className + "/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img(outInfo.locate.details[i].box));
					}
				}
			}
		}
		

		
	}



}

void InspBottleNum::BottleNum_DrawResult(InspBottleNumOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	Log::WriteAsyncLog("开始绘制结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);


	int fontSize = 25;
	// 创建输出图像
	outInfo.images.outputImg.data = std::make_shared<cv::Mat>(m_img.clone());
	outInfo.images.outputImg.stageName = "BottleNum_DrawResult";
	outInfo.images.outputImg.description = "绘制全部结果: " + std::to_string(m_params.saveDebugImage);
	outInfo.images.outputImg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

	cv::Mat outputImg = outInfo.images.outputImg.mat();

	// 辅助函数
	auto format = [](float conf) {
		return (std::ostringstream() << std::fixed << std::setprecision(2) << conf).str();
	};

	auto getColor = [](const std::string& status) {
		if (status == "匹配") return Colors::GREEN;
		if (status == "未检测到") return Colors::YELLOW;
		if (status == "数量错误") return Colors::YELLOW;
		return Colors::WHITE;
	};

	// 1. 绘制ROI区域
	cv::rectangle(outputImg, m_params.roiRect, Colors::YELLOW, 1, cv::LINE_AA);

	// 2. 绘制匹配结果
	for (const auto& bottle : outInfo.locate.badDetails) {
		// 绘制瓶盖
		float lr = bottle.box.width * 1.0 / 100;
		int fontSizeCur = fontSize * lr;
		std::string capText = bottle.className + "  " + format(bottle.confidence).c_str();
		putTextZH(outputImg, capText.c_str(),
			cv::Point(bottle.box.x, bottle.box.y + 5),
			Colors::LIGHT_BLUE, fontSize, FW_BOLD);
		std::string capText1 = (std::to_string(bottle.box.width) + "," + std::to_string(bottle.box.height)).c_str();
		putTextZH(outputImg, capText1.c_str(),
			cv::Point(bottle.box.x, bottle.box.y + 30 * lr),
			Colors::LIGHT_BLUE, fontSize, FW_BOLD);
		cv::rectangle(outputImg, bottle.box, Colors::LIGHT_BLUE, 2, cv::LINE_AA);
	}

	auto getColor1 = [](bool isMatched) {
		return isMatched ? Colors::GREEN : Colors::RED;
	};
	for (const auto& bottle : outInfo.classification.bottleResult) {
		cv::Scalar drawColor = getColor1(bottle.isMatched);

		// 瓶盖绘制
		if (!bottle.capType.empty() && bottle.capType != "*****") {
			float lr = bottle.capRect.width * 1.0f / 100;

			cv::rectangle(outputImg, bottle.capRect, drawColor, 2, cv::LINE_AA);

			std::string capText = bottle.capType + "  " + format(bottle.capScore);
			putTextZH(outputImg, capText.c_str(),
				cv::Point(bottle.capRect.x, bottle.capRect.y + 5),
				drawColor, fontSize, FW_BOLD);

			std::string capText1 = std::to_string(bottle.capRect.width) + "," + std::to_string(bottle.capRect.height);
			putTextZH(outputImg, capText1.c_str(),
				cv::Point(bottle.capRect.x, bottle.capRect.y + 30 * lr),
				drawColor, fontSize, FW_BOLD);
		}

		// 提手绘制
		if (!bottle.handleType.empty() && !bottle.handleRect.empty()) {
			float lrHandle = bottle.handleRect.width * 1.0f / 200;

			cv::rectangle(outputImg, bottle.handleRect, drawColor, 2, cv::LINE_AA);

			std::string handleText = bottle.handleType + "  " + format(bottle.handleScore);
			putTextZH(outputImg, handleText.c_str(),
				cv::Point(bottle.handleRect.x, bottle.handleRect.y + bottle.handleRect.height + 5),
				drawColor, fontSize, FW_BOLD);

			std::string handleText1 = std::to_string(bottle.handleRect.width) + "," + std::to_string(bottle.handleRect.height);
			putTextZH(outputImg, handleText1.c_str(),
				cv::Point(bottle.handleRect.x, bottle.handleRect.y + bottle.handleRect.height + 30 * lrHandle),
				drawColor, fontSize, FW_BOLD);
		}
	}

	// 3. 绘制比较结果（如果存在）
	if (outInfo.classification.checkResult.typeComparisons.size() > 0) {
		int yPos = 110;
		int lineHeight = 45;

		yPos += lineHeight;

		// 绘制每个类型的比较结果
		for (TypeComparison& comp : outInfo.classification.checkResult.typeComparisons) {
			cv::Scalar color = getColor(comp.status);

			std::string text = comp.typeKey +
				" (目标=" + std::to_string(comp.targetNum) +
				", 实际=" + std::to_string(comp.actualNum) + ")";

			putTextZH(outputImg, text.c_str(), cv::Point(15, yPos), color, 35, FW_BOLD);
			yPos += lineHeight;
		}

		// 绘制错误类型
		if (!outInfo.classification.checkResult.undefinedTypes.empty()) {
			//putTextZH(outputImg, "错误类型:", cv::Point(15, yPos), Colors::RED, 40, FW_BOLD);
			//yPos += lineHeight;

			for (const auto& undef : outInfo.classification.checkResult.undefinedTypes) {
				putTextZH(outputImg, undef.c_str(), cv::Point(15, yPos), Colors::RED, 35, FW_BOLD);
				yPos += lineHeight;
			}
		}
	}

	// 4. 绘制总体状态
	std::string rv = "ID = " + std::to_string(outInfo.system.jobId) +
		", RV = " + std::to_string(outInfo.status.statusCode) +
		", " + outInfo.status.errorMessage;

	cv::Scalar statusColor = (outInfo.status.statusCode == BOTTLENUM_RETURN_OK) ?
		Colors::GREEN : Colors::RED;

	putTextZH(outputImg, rv.c_str(), cv::Point(15, 30), statusColor, 50, FW_BOLD);

	// 5. 绘制总数信息
	std::string countInfo = "目标总数: " + std::to_string(m_params.bottleNum) +
		", 检测总数: " + std::to_string(outInfo.classification.bottleResult.size());
	putTextZH(outputImg, countInfo.c_str(), cv::Point(10, 100), Colors::GREEN, 40, FW_BOLD);

	// 6. 保存图像
	DAS->DAS_Img(outputImg, outInfo.paths.intermediateImagesDir + "10.outputImg.jpg", m_params.saveDebugImage);
}

std::future<int> InspBottleNum::RunInspectionAsync(InspBottleNumOut& outInfo) {
	return std::async(std::launch::async, [this, &outInfo] {
		// 设置超时检查起点
		m_startTime = std::chrono::high_resolution_clock::now();

		// 执行主检测逻辑
		return BottleNum_Main(outInfo, true);
		});
}

int InspBottleNum::BottleNum_Main(InspBottleNumOut& outInfo, bool checkTimeout) {
	try {
		double time0 = static_cast<double>(cv::getTickCount());
		if (outInfo.status.statusCode == BOTTLENUM_RETURN_OK)
		{
			Log::WriteAsyncLog("BottleNum_Main!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			if (checkTimeout && CheckTimeout(m_params.timeOut)) {
				if (m_safeTimeoutFlag) {
					m_safeTimeoutFlag->store(true);
				}
				return BOTTLENUM_RETURN_TIMEOUT;
			}

			if (outInfo.status.statusCode != BOTTLENUM_RETURN_OK) {
				Log::WriteAsyncLog("跳过获取模型!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
			}
			else
			{
				std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
				std::string cameraIdStr = std::to_string(outInfo.system.cameraId);
				m_params.locateWeightsFile = bottleNumDetectionModelMap["bottleNumDetection_" + cameraIdStr];
				m_params.capClassifyWeightsFile = capClassifyModelMap["capClassify_" + cameraIdStr];
			}


			// 第1步:设置ROI
			BottleNum_SetROI(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return BOTTLENUM_RETURN_TIMEOUT;
			}

			// 第2步:定位
			BottleNum_LocateBottle(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return BOTTLENUM_RETURN_TIMEOUT;
			}

			// 第3步:分类
			BottleNum_CheckBottleType(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return BOTTLENUM_RETURN_TIMEOUT;
			}

			// 第4步:匹配
			BottleNum_MatchBottles(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return BOTTLENUM_RETURN_TIMEOUT;
			}


			// 第5步:结果
			BottleNum_CheckResult(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return BOTTLENUM_RETURN_TIMEOUT;
			}


			// 第5步:结果
			BottleNum_SaveForTrain(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return BOTTLENUM_RETURN_TIMEOUT;
			}
		}


		// 第9步:绘制结果
		BottleNum_DrawResult(outInfo);
		if (CheckTimeout(m_params.timeOut))
		{
			Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			return BOTTLENUM_RETURN_TIMEOUT;
		}

		if (outInfo.status.statusCode == BOTTLENUM_RETURN_OK) {
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
		if (m_safeTimeoutFlag) {
			m_safeTimeoutFlag->store(false); // 重置状态
		}
		std::cerr << "[ERROR] Inference failed: " << e.what() << std::endl;
		return BOTTLENUM_RETURN_ALGO_ERR;
	}


	return outInfo.status.statusCode;
}