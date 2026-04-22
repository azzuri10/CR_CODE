#include "HeaderDefine.h"
#include "InspHandle.h"
#include "ModelManager.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <locale>
#include "InferenceWorker.h"
#include "Data.h"
#include "AnalyseMat.h"

std::shared_mutex InspHandle::modelLoadMutex;
std::map<std::string, std::string> InspHandle::handleLocationModelMap;
std::map<std::string, std::string> InspHandle::handleClassfyModelMap;
std::map<std::string, std::string> InspHandle::filmClassfyModelMap;
std::map<int, InspHandleIn> InspHandle::cameraConfigMap;

// 构造函数:初始化时加载所有相关模型
InspHandle::InspHandle(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
	bool isLoadConfig, int timeOut, InspHandleOut& outInfo)
	: ANA(std::make_unique<AnalyseMat>()),
	COM(std::make_unique<Common>())
{

	m_timeOut = timeOut;
	m_timeoutFlagRef = &outInfo.system.timeoutFlag;
	outInfo.system.startTime = COM->time_t2string_with_ms();
	//输入参数初始化
	if (img.channels() == 1)
	{		
		cv::cvtColor(img, m_img, cv::COLOR_GRAY2BGR);
		outInfo.images.outputImg.data = std::make_shared<cv::Mat>(m_img.clone());
		outInfo.images.outputImg.stageName = "初始化";
		outInfo.images.outputImg.description = "初始化";
		outInfo.images.outputImg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
	}
	else if (img.channels() == 3)
	{
		m_img = img.clone();
		outInfo.images.outputImg.data = std::make_shared<cv::Mat>(m_img.clone());
	}

	

	COM->CreateDir(outInfo.paths.logDirectory);
	Log::WriteAsyncLog("********** Start Inspction JobID = ", INFO, outInfo.paths.logFile, true, outInfo.system.jobId, " ***********");

	bool shouldLoadConfig = isLoadConfig ||
		jobId == 0 ||
		cameraConfigMap.find(cameraId) == cameraConfigMap.end();

	//读取config
	if (shouldLoadConfig)
	{
		bool rv_loadConfig = readParams(m_img, outInfo.paths.configFile, m_params, outInfo, outInfo.paths.logFile);
		if (!rv_loadConfig) {
			outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = outInfo.status.errorMessage;
			Log::WriteAsyncLog(outInfo.status.errorMessage, ERR, outInfo.paths.logFile, true);
			return;
		}
		else
		{
			Log::WriteAsyncLog("读取config成功!", INFO, outInfo.paths.logFile, true);
		}


		//检测roi
		if (!ANA->JudgeRectIn(cv::Rect(0, 0, img.cols, img.rows), m_params.roiRect)) {
			outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
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
			outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("定位参数设置错误！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "定位参数设置错误!";
			return;
		}
		else
		{
			Log::WriteAsyncLog("定位参数读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}

		if (m_params.checkHandleType == 1)
		{
			Log::WriteAsyncLog("提手检测有无，目标类型按有无设置，不再检出提手挂反、不到位!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			for (int i = 0; i < m_params.locateClassName.size(); i++)
			{
				if (m_params.locateClassName[i] == "提手挂反" || m_params.locateClassName[i] == "不到位")
				{
					m_params.locateClassName[i] == "提手";
				}
			}
		}
		if (m_params.checkFilmType == 1)
		{
			Log::WriteAsyncLog("塑膜检测有无，目标类型按有无塑膜设置，不再检出坏膜!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			for (int i = 0; i < m_params.locateClassName.size(); i++)
			{
				if (m_params.locateClassName[i] == "坏膜")
				{
					m_params.locateClassName[i] = "有塑膜";
				}
			}
		}

		//读取提手分类类型名称
		if (m_params.checkHandleType)
		{
			std::ifstream ifsHandle(m_params.handleClassfyFile.c_str());
			if (!ifsHandle.is_open()) {
				outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "提手分类类型文件缺失!";
				Log::WriteAsyncLog(m_params.handleClassfyFile, ERR, outInfo.paths.logFile, true, "---提手分类类型文件缺失!");
				return;
			}
			else
			{
				m_params.handleClassfyName.clear();
				std::string line;
				while (getline(ifsHandle, line)) m_params.handleClassfyName.push_back(line);
				Log::WriteAsyncLog("提手类型文件读取成功！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			
				bool isHandleType = (std::find(m_params.handleClassfyName.begin(),
					m_params.handleClassfyName.end(),
					m_params.handleType) != m_params.handleClassfyName.end());
				
				if (m_params.handleType != "0" && m_params.handleType != "不检测")
				{
					if (!isHandleType)
					{
						outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
						outInfo.status.errorMessage = "当前选择的提手类型不在分类类型文件内!";
						Log::WriteAsyncLog("当前选择的提手类型不在分类类型文件内!", ERR, outInfo.paths.logFile, true);
						return;
					}
				}			
			}
		}
		else
		{
			Log::WriteAsyncLog("未开启提手类型检测！", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		}


		//读取塑膜分类类型名称
		if (m_params.checkFilmType)
		{
			std::ifstream ifsFilm(m_params.filmClassfyFile.c_str());
			if (!ifsFilm.is_open()) {
				outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "塑膜分类类型文件缺失!";
				Log::WriteAsyncLog(m_params.filmClassfyFile, ERR, outInfo.paths.logFile, true, "---塑膜分类类型文件缺失!");
				return;
			}
			else
			{
				m_params.filmClassfyName.clear();
				std::string line;
				while (getline(ifsFilm, line)) m_params.filmClassfyName.push_back(line);
				Log::WriteAsyncLog("塑膜类型文件读取成功！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

				bool isFilmType = (std::find(m_params.filmClassfyName.begin(),
					m_params.filmClassfyName.end(),
					m_params.filmType) != m_params.filmClassfyName.end());

				if (m_params.filmType != "0" && m_params.filmType != "不检测")
				{
					if (!isFilmType)
					{
						outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
						outInfo.status.errorMessage = "当前选择的塑膜类型不在分类类型文件内!";
						Log::WriteAsyncLog("当前选择的塑膜类型不在分类类型文件内!", ERR, outInfo.paths.logFile, true);
						return;
					}
				}
			}
		}
		else
		{
			Log::WriteAsyncLog("未开启塑膜类型检测！", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		}


		if (isLoadConfig || outInfo.system.jobId == 0 || !loadHandleConfigSuccess[outInfo.system.cameraId])
		{
			bool loadModel = loadAllModels(outInfo, true);
			if (!loadModel) {
				outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "深度学习模型加载异常!";
				Log::WriteAsyncLog(m_params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "---深度学习模型加载异常!");
				return;
			}
		}
		else
		{
			Log::WriteAsyncLog("跳过模型加载！", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		}



		if (!validateCameraModels(outInfo.system.cameraId)) {
			Log::WriteAsyncLog("相机ID配置错误/模型文件缺失!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "相机ID配置错误/模型文件缺失!";
			throw std::invalid_argument("相机ID配置错误/模型文件缺失!");
		}

		cameraConfigMap[cameraId] = m_params;
	}
	else
	{
		m_params = cameraConfigMap[cameraId];
	}

	if (outInfo.status.statusCode = HANDLE_RETURN_OK)
	{
		loadHandleConfigSuccess[outInfo.system.cameraId] = true;
	}
	else
	{
		loadHandleConfigSuccess[outInfo.system.cameraId] = false;
	}
	if (m_params.saveDebugImage) {
		COM->CreateDir(outInfo.paths.intermediateImagesDir);
	}

	if (m_params.saveResultImage) {
		COM->CreateDir(outInfo.paths.resultsOKDir);
		COM->CreateDir(outInfo.paths.resultsNGDir);
	}
}

InspHandle::~InspHandle() {
	// 可在此处添加资源释放逻辑（如有需要）
}

// 验证摄像头ID对应的模型配置是否存在
bool InspHandle::validateCameraModels(int cameraId) {
	std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
	return handleLocationModelMap.count("handleLocation_" + std::to_string(cameraId));
	return handleClassfyModelMap.count("handleClassfy_" + std::to_string(cameraId));
	return handleClassfyModelMap.count("filmClassfy_" + std::to_string(cameraId));
}

// 加载所有模型到ModelManager
bool InspHandle::loadAllModels(InspHandleOut& outInfo, bool ini) {
	if (!ini) {
		Log::WriteAsyncLog("跳过模型加载!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return true;
	}

	const int cameraId = outInfo.system.cameraId;
	const cv::String key = std::to_string(cameraId);

	// 获取当前相机专用模型路径
	std::vector<std::string> cameraModelPaths;

	// 1. 添加检测模型
	std::string detectionKey = "handleLocation_" + std::to_string(cameraId);
	if (auto it = handleLocationModelMap.find(detectionKey); it != handleLocationModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	// 2. 添加缺陷模型
	std::string handleKey = "handleClassfy_" + std::to_string(cameraId);
	if (auto it = handleClassfyModelMap.find(handleKey); it != handleClassfyModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	// 3. 添加分类模型
	std::string filmKey = "filmClassfy_" + std::to_string(cameraId);
	if (auto it = filmClassfyModelMap.find(filmKey); it != filmClassfyModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	if (cameraModelPaths.empty()) {
		Log::WriteAsyncLog("相机" + std::to_string(cameraId) + "未找到有效模型路径!",
			ERR, outInfo.paths.logFile, true);
		return false;
	}

	try {
		ModelManager& mgr = ModelManager::Instance(cameraId);

		for (const auto& modelPath : cameraModelPaths) {
			// 避免重复加载
			if (!mgr.IsModelLoaded(modelPath)) {
				mgr.LoadModel(modelPath, m_params.hardwareType);
				Log::WriteAsyncLog("相机" + std::to_string(cameraId) +
					"加载模型: " + modelPath, INFO,
					outInfo.paths.logFile,
					m_params.saveLogTxt);
			}
		}
		Mat iniImg = Mat::zeros(cv::Size(2500, 2000), CV_8UC3);
		outInfo.locate.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.locateWeightsFile, m_params.locateClassName, iniImg, 0.5, 0.3);
		if (m_params.handleType != "0" && m_params.handleType != "不检测")
		{
			outInfo.classification.filmType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.filmClassfyFile, m_params.filmClassfyName, iniImg);
		}
		if (m_params.filmType != "0" && m_params.filmType != "不检测")
		{
			outInfo.classification.handleType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.handleClassfyFile, m_params.handleClassfyName, iniImg);
		}
		outInfo.classification.handleType.className = "";
		outInfo.classification.filmType.className = "";
		Log::WriteAsyncLog("模型初始化完成！", INFO, outInfo.paths.logFile, true);


		return true;
	}
	catch (const std::exception& e) {
		Log::WriteAsyncLog("相机" + std::to_string(cameraId) +
			"模型加载异常: " + std::string(e.what()), ERR,
			outInfo.paths.logFile, true);
		return false;
	}
}


// 读取参数的函数
bool InspHandle::readParams(cv::Mat img, const std::string& filePath, InspHandleIn& params, InspHandleOut& outInfo, const std::string& fileName) {
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
		if (keyWord == "HANDLE_SAVE_DEBUG_IMAGE") {
			params.saveDebugImage = std::stoi(value);
		}
		else if (keyWord == "HANDLE_SAVE_RESULT_IMAGE") {
			params.saveResultImage = std::stoi(value);
		}
		else if (keyWord == "HANDLE_SAVE_LOG_TXT") {
			params.saveLogTxt = std::stoi(value);
		}
		else if (keyWord == "HANDLE_DRAW_RESULT") {
			params.drawResult = std::stoi(value);
		}
		else if (keyWord == "HANDLE_SAVE_TRAIN") {
			params.saveTrain = std::stoi(value);
		}
		else if (keyWord == "HANDLE_ROI_X") {
			params.roiRect.x = std::stoi(value);
			if (params.roiRect.x < 0 || params.roiRect.x > img.cols)
			{
				outInfo.status.errorMessage = "ROI_X: 超出图像范围!";
				Log::WriteAsyncLog("ROI_X: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "HANDLE_ROI_Y") {
			params.roiRect.y = std::stoi(value);
			if (params.roiRect.y < 0 || params.roiRect.y > img.rows)
			{
				outInfo.status.errorMessage = "ROI_Y: 超出图像范围!";
				Log::WriteAsyncLog("ROI_Y: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "HANDLE_ROI_W") {
			params.roiRect.width = std::stoi(value);
			if (params.roiRect.x + params.roiRect.width > img.cols)
			{
				outInfo.status.errorMessage = "ROI_X+ROI_W: 超出图像范围!";
				Log::WriteAsyncLog("ROI_X+ROI_W: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "HANDLE_ROI_H") {
			params.roiRect.height = std::stoi(value);
			if (params.roiRect.y + params.roiRect.height > img.rows)
			{
				outInfo.status.errorMessage = "ROI_Y+ROI_H: 超出图像范围!";
				Log::WriteAsyncLog("ROI_Y+ROI_H: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "HANDLE_CHECK_HANLDE") {
			params.checkHandleType = std::stoi(value);
			if (params.checkHandleType > 2 || params.checkHandleType < 0)
			{
				outInfo.status.errorMessage = "是否检测提环: 超出范围!";
				Log::WriteAsyncLog("是否检测提环: 超出范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "HANDLE_CHECK_FILM") {
			params.checkFilmType = std::stoi(value);
			if (params.checkFilmType > 2 || params.checkFilmType < 0)
			{
				outInfo.status.errorMessage = "是否检测塑膜: 超出范围!";
				Log::WriteAsyncLog("是否检测塑膜: 超出范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "HANDLE_HARDWARE_TYPE") {
			params.hardwareType = std::stoi(value);
			if (params.hardwareType > 2 || params.hardwareType < 0)
			{
				outInfo.status.errorMessage = "检测使用硬件: 超出范围!";
				Log::WriteAsyncLog("检测使用硬件: 超出范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "HANDLE_MODEL_TYPE") {
			params.modelType = std::stoi(value);
			if (params.modelType > 3 || params.modelType < 0)
			{
				outInfo.status.errorMessage = "模型类型: 超出范围!";
				Log::WriteAsyncLog("模型类型: 超出范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "HANDLE_LOCATE_WEIGHTS_FLIE") {
			params.locateWeightsFile = value;
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);  // 加锁
			std::string camera = std::to_string(outInfo.system.cameraId);
			handleLocationModelMap["handleLocation_" + camera] = value;
			if (!COM->FileExistsModern(params.locateWeightsFile))
			{
				outInfo.status.errorMessage = "定位类型文件缺失!";
				Log::WriteAsyncLog(params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "--定位类型文件缺失！");
				return false;
			}
		}
		else if (keyWord == "HANDLE_LOCATE_CONFIG") {
			params.locateThreshConfig = value;
			if (!COM->FileExistsModern(params.locateThreshConfig))
			{
				outInfo.status.errorMessage = "定位阈值文件缺失!";
				Log::WriteAsyncLog(params.locateThreshConfig, ERR, outInfo.paths.logFile, true, "--定位阈值文件缺失！");
				return false;
			}
		}
		else if (keyWord == "HANDLE_TYPE") {
		params.handleType = value;
		}
		else if (keyWord == "HANDLE_CLASSFY_WEIGHTS_FILE") {
			params.handleClassfyFile = value;
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);  // 加锁
			std::string camera = std::to_string(outInfo.system.cameraId);
			handleClassfyModelMap["handleClassfy_" + camera] = value;
			if (params.handleType != "0")
			{
				if (!COM->FileExistsModern(params.handleClassfyFile))
				{
					outInfo.status.errorMessage = "提手分类模型文件缺失!";
					Log::WriteAsyncLog(params.handleClassfyFile, ERR, outInfo.paths.logFile, true, "--提手分类模型文件缺失！");
					return false;
				}
			}			
		}
		else if (keyWord == "HANDLE_CLASSES_FILE") {
			params.handleClassfyNameFile = value;
			if (params.handleType != "0")
			{
				if (!COM->FileExistsModern(params.handleClassfyNameFile))
				{
					outInfo.status.errorMessage = "提手分类类型文件缺失!";
					Log::WriteAsyncLog(params.handleClassfyNameFile, ERR, outInfo.paths.logFile, true, "--提手分类类型文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "HANDLE_FILM_TYPE") {
		params.filmType = value;
		}
		else if (keyWord == "HANDLE_FILM_CLASSFY_WEIGHTS_FILE") {
			params.filmClassfyFile = value;
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);  // 加锁
			std::string camera = std::to_string(outInfo.system.cameraId);
			filmClassfyModelMap["filmClassfy_" + camera] = value;
			if (params.filmType != "0")
			{
				if (!COM->FileExistsModern(params.filmClassfyFile))
				{
					outInfo.status.errorMessage = "塑膜分类模型文件缺失!";
					Log::WriteAsyncLog(params.filmClassfyFile, ERR, outInfo.paths.logFile, true, "--塑膜分类模型文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "HANDLE_FILM_CLASSES_FILE") {
			params.filmClassfyNameFile = value;
			if (params.filmType != "0")
			{
				if (!COM->FileExistsModern(params.filmClassfyNameFile))
				{
					outInfo.status.errorMessage = "塑膜分类类型文件缺失!";
					Log::WriteAsyncLog(params.filmClassfyNameFile, ERR, outInfo.paths.logFile, true, "--塑膜分类类型文件缺失！");
					return false;
				}
			}
		}      
	}

	ifs.close();
	return true;
}

void InspHandle::Handle_SetROI(InspHandleOut& outInfo) {
	if (outInfo.status.statusCode != HANDLE_RETURN_OK) {
		Log::WriteAsyncLog("跳过ROI区域获取!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始ROI区域获取!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	outInfo.images.roi.data = std::make_shared<cv::Mat>(m_img(m_params.roiRect).clone());
	outInfo.images.roi.stageName = "Handle_Main";
	outInfo.images.roi.description = "ROI区域获取";
	outInfo.images.roi.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

	outInfo.images.roiLog.data = std::make_shared<cv::Mat>(outInfo.images.outputImg.mat());
	outInfo.images.roiLog.stageName = "Handle_Main";
	outInfo.images.roiLog.description = "ROI_LOG绘制: " + std::to_string(m_params.saveDebugImage);
	outInfo.images.roiLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
	DAS->DAS_Rect(outInfo.images.roiLog.mat(), m_params.roiRect, outInfo.paths.intermediateImagesDir + "1.0.0.roiRect.jpg", m_params.saveDebugImage);

}

void InspHandle::Handle_LocateHandle(InspHandleOut& outInfo) {
	if (outInfo.status.statusCode != HANDLE_RETURN_OK) {
		Log::WriteAsyncLog("跳过提手定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始提手定位!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (m_params.locateWeightsFile.find(".onnx") != std::string::npos)
	{
		outInfo.locate.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.locateWeightsFile, m_params.locateClassName, outInfo.images.roi.mat());
	}
	else
	{
		outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}
	if (m_params.saveDebugImage)
	{
		outInfo.images.locationLog.data = std::make_shared<cv::Mat>(outInfo.images.roi.mat().clone());
		outInfo.images.locationLog.stageName = "Handle_LocateHandle";
		outInfo.images.locationLog.description = "Locate绘制: " + std::to_string(m_params.saveDebugImage);
		outInfo.images.locationLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
		DAS->DAS_FinsObject(outInfo.images.locationLog.mat(), outInfo.locate.details, outInfo.paths.intermediateImagesDir + "2.1.1.location.jpg", m_params.saveDebugImage);
	}



	Log::WriteAsyncLog("开始分析定位结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	for (int i = outInfo.locate.details.size() - 1; i >= 0; --i)
	{
		auto& locate = outInfo.locate.details[i];
		int paramIndex = -1; // 根据缺陷类别设置对应参数索引

		bool valid = true;
		if (locate.className == "无塑膜") paramIndex = 5;
		else if (locate.className == "提手挂反")paramIndex = 1; 
		else if (locate.className == "无提手")   paramIndex = 2; 
		else if (locate.className == "不到位")  paramIndex = 3; 
		else if (locate.className == "有塑膜") paramIndex = 4;  
		else if (locate.className == "提手")	paramIndex = 0;

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
	bool findNH = false;
	bool findHandle = false;
	bool findBad = false;
	bool findRev = false;
	bool findNF = false;
	bool findBF = false;
	bool findFilm = false;
	int cntHandle = 0;
	int cntFilm = 0;
	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{
		outInfo.locate.details[i].box.x += m_params.roiRect.x;
		outInfo.locate.details[i].box.y += m_params.roiRect.y;

		if (outInfo.locate.details[i].className == "无提手")
		{
			findNH = true;
		}
		else if (outInfo.locate.details[i].className == "不到位" && m_params.checkHandleType == 2)
		{
			findBad = true;
		}
		else if (outInfo.locate.details[i].className == "提手挂反" && m_params.checkHandleType == 2)
		{
			findRev = true;
		}
		else if (outInfo.locate.details[i].className == "无塑膜" && m_params.checkFilmType == 1)
		{
			findNF = true;
		}
		else if (outInfo.locate.details[i].className == "坏膜" && m_params.checkFilmType == 2)
		{
			findBF = true;
			outInfo.geometry.filmRect = outInfo.locate.details[i].box;
		}
		else if (outInfo.locate.details[i].className == "提手")
		{
			findHandle = true;
			cntHandle++;
			outInfo.geometry.handleRect = outInfo.locate.details[i].box;
		}
		else if (outInfo.locate.details[i].className == "有塑膜")
		{
			findFilm = true;
			cntFilm++;
			outInfo.geometry.filmRect = outInfo.locate.details[i].box;
		}
	}

	if (outInfo.locate.details.empty())
	{
		if (m_params.saveTrain==1 || m_params.saveTrain == 3)
		{
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/NONE");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				outInfo.images.outputImg.mat().rows,
				outInfo.images.outputImg.mat().cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/NONE/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/NONE/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

		}
		outInfo.status.errorMessage = "定位-无目标!";
		Log::WriteAsyncLog("定位-无目标!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = HANDLE_RETURN_NO_TARGET;
		return;
	}
		

	if (m_params.checkHandleType == 0)
	{
		Log::WriteAsyncLog("未开启提环检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	else if (m_params.checkHandleType == 1)
	{
		if (cntHandle == 0)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/无提手");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/无提手/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/无提手/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}
			outInfo.status.errorMessage = "定位-无提手!";
			Log::WriteAsyncLog("定位-无提手!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = HANDLE_RETURN_HANDLE_MISS;
			return;
		}
		/*if (cntHandle > 1)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/MULTY");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}
			outInfo.status.errorMessage = "定位-多个目标!";
			Log::WriteAsyncLog("定位-多个目标!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = HANDLE_RETURN_MULT_TARGET;
			return;
		}*/
		else
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/OK");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}
			Log::WriteAsyncLog("定位提手成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);


			outInfo.geometry.handleRect = ANA->AdjustROI(outInfo.geometry.handleRect, outInfo.images.roi.mat());
			outInfo.images.handleRegion.data = std::make_shared<cv::Mat>(outInfo.images.roi.mat()(outInfo.geometry.handleRect).clone());
			outInfo.images.handleRegion.stageName = "Handle_LocateCap";
			outInfo.images.handleRegion.description = "handleRegion定位";
			outInfo.images.handleRegion.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
			DAS->DAS_Img(outInfo.images.handleRegion.mat(), outInfo.paths.intermediateImagesDir + "2.2.1.handleRegion.jpg", m_params.saveDebugImage);

		}
	}
	else if(m_params.checkHandleType == 2)
	{
		if (findNH)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/无提手");

				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/无提手/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/无提手/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}

			outInfo.status.errorMessage = "定位-无提手!";
			Log::WriteAsyncLog("定位-无提手!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = HANDLE_RETURN_HANDLE_MISS;
			return;
		}
		if (findBad)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/不到位");

				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/不到位/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/不到位/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}
			outInfo.status.errorMessage = "定位-不到位!";
			Log::WriteAsyncLog("定位-不到位!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = HANDLE_RETURN_HANDLE_POS_ERR;
			return;
		}
		if (findRev)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/提手挂反");

				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/提手挂反/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/提手挂反/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}

			outInfo.status.errorMessage = "定位-提手挂反!";
			Log::WriteAsyncLog("定位-提手挂反!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = HANDLE_RETURN_HANDLE_POS_REV;
			return;
		}

		if (cntHandle == 0)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/无提手");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/无提手/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/无提手/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}
			outInfo.status.errorMessage = "定位-无目标!";
			Log::WriteAsyncLog("定位-无目标!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = HANDLE_RETURN_NO_TARGET;
			return;
		}
		/*if (cntHandle > 1)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/MULTY");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}
			outInfo.status.errorMessage = "定位-多个目标!";
			Log::WriteAsyncLog("定位-多个目标!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = HANDLE_RETURN_MULT_TARGET;
			return;
		}*/
		else
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/OK");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}
			Log::WriteAsyncLog("定位提手成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);


			outInfo.geometry.handleRect = ANA->AdjustROI(outInfo.geometry.handleRect, outInfo.images.roi.mat());
			outInfo.images.handleRegion.data = std::make_shared<cv::Mat>(outInfo.images.roi.mat()(outInfo.geometry.handleRect).clone());
			outInfo.images.handleRegion.stageName = "Handle_LocateCap";
			outInfo.images.handleRegion.description = "handleRegion定位";
			outInfo.images.handleRegion.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
			DAS->DAS_Img(outInfo.images.handleRegion.mat(), outInfo.paths.intermediateImagesDir + "2.2.1.handleRegion.jpg", m_params.saveDebugImage);

		}

	}
	
	if (m_params.checkFilmType > 0)
	{
		if (findBF)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/BF");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/BF/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/BF/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}
			outInfo.status.errorMessage = "定位-坏膜!";
			Log::WriteAsyncLog("定位-坏膜!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = HANDLE_RETURN_BAD_FILM;
			return;
		}
		else if (cntFilm == 0)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/NF");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/NF/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/NF/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}
			outInfo.status.errorMessage = "定位-无塑膜!";
			Log::WriteAsyncLog("定位-无塑膜!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = HANDLE_RETURN_NO_FILM;
			return;
		}
		/*else if (cntFilm > 1)
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/MULTY_FM");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/MULTY_FM/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/MULTY_FM/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}
			outInfo.status.errorMessage = "定位-多个目标!";
			Log::WriteAsyncLog("定位-多个目标!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = HANDLE_RETURN_MULT_TARGET;
			return;
		}*/
		else
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/OK");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.details,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
					outInfo.images.outputImg.mat().rows,
					outInfo.images.outputImg.mat().cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

			}
			Log::WriteAsyncLog("定位塑膜成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

		}
	}
	else
	{
		Log::WriteAsyncLog("未开启塑膜检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	
}

void InspHandle::Handle_CheckHandle(InspHandleOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != HANDLE_RETURN_OK) {
		Log::WriteAsyncLog("跳过提手类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}

	if (m_params.handleType == "0" || m_params.handleType == "不检测")
	{
		outInfo.classification.handleType.className = "不检测";
		Log::WriteAsyncLog("提手类型设置为0，跳过提手类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始提手类型检测!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	outInfo.images.handleRegion.data = std::make_shared<cv::Mat>(outInfo.images.roi.mat()(outInfo.geometry.handleRect).clone());
	outInfo.images.handleRegion.stageName = "PressCap_CheckTopType";
	outInfo.images.handleRegion.description = "提取提手区域做分类";
	outInfo.images.handleRegion.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

	cv::Mat img = outInfo.images.handleRegion.mat().clone();
	resize(img, img, cv::Size(img.cols, img.cols));
	if (m_params.handleClassfyFile.find(".onnx") != std::string::npos)
	{
		outInfo.classification.handleType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.handleClassfyFile, m_params.handleClassfyName, img);
	}
	else
	{
		outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}
	DAS->DAS_String(outInfo.images.handleRegion.mat(), outInfo.classification.handleType.className, outInfo.paths.intermediateImagesDir + "5.1.1.handleType.jpg", m_params.saveDebugImage);


	Log::WriteAsyncLog("提手类型： ", ERR, outInfo.paths.logFile, true, outInfo.classification.handleType.className);
	if (outInfo.classification.handleType.className != m_params.handleType)
	{
		outInfo.status.statusCode = HANDLE_RETURN_HANDLE_PTYPE_ERR;
		outInfo.status.errorMessage = "提手类型错误!";
		Log::WriteAsyncLog("提手类型错误!", ERR, outInfo.paths.logFile, true);
	}

	if ((m_params.saveTrain == 1 || m_params.saveTrain == 2) && outInfo.status.statusCode == HANDLE_RETURN_OK)
	{
		outInfo.system.startTime = COM->time_t2string_with_ms();
		cv::Mat imgSave;
		cv::resize(outInfo.images.handleRegion.mat(), imgSave, cv::Size(64, 64));
		outInfo.system.startTime = COM->time_t2string_with_ms();
		COM->CreateDir(outInfo.paths.trainDir + "HANDLE/" + outInfo.classification.handleType.className);
		cv::imwrite(outInfo.paths.trainDir + "HANDLE/" + outInfo.classification.handleType.className + "/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", imgSave);
	}
	else if (m_params.saveTrain == 1 || m_params.saveTrain == 3 && outInfo.status.statusCode != HANDLE_RETURN_OK)
	{
		outInfo.system.startTime = COM->time_t2string_with_ms();
		cv::Mat imgSave;
		cv::resize(outInfo.images.handleRegion.mat(), imgSave, cv::Size(64, 64));
		outInfo.system.startTime = COM->time_t2string_with_ms();
		COM->CreateDir(outInfo.paths.trainDir + "HANDLE/" + outInfo.classification.handleType.className);
		cv::imwrite(outInfo.paths.trainDir + "HANDLE/" + outInfo.classification.handleType.className + "/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", imgSave);
	}
}


void InspHandle::Handle_CheckFilm(InspHandleOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != HANDLE_RETURN_OK) {
		Log::WriteAsyncLog("跳过塑膜类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}

	if (m_params.filmType == "0" || m_params.filmType == "不检测")
	{
		outInfo.classification.filmType.className = "不检测";
		Log::WriteAsyncLog("塑膜类型设置为0，跳过塑膜类型检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始塑膜类型检测!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	outInfo.images.filmRegion.data = std::make_shared<cv::Mat>(outInfo.images.roi.mat()(outInfo.geometry.filmRect).clone());
	outInfo.images.filmRegion.stageName = "PressCap_CheckTopType";
	outInfo.images.filmRegion.description = "提取塑膜区域做分类";
	outInfo.images.filmRegion.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

	cv::Mat img = outInfo.images.filmRegion.mat().clone();
	resize(img, img, cv::Size(img.cols, img.cols));
	if (m_params.filmClassfyFile.find(".onnx") != std::string::npos)
	{
		outInfo.classification.filmType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.filmClassfyFile, m_params.filmClassfyName, img);
	}
	else
	{
		outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}
	DAS->DAS_String(outInfo.images.filmRegion.mat(), outInfo.classification.filmType.className, outInfo.paths.intermediateImagesDir + "5.1.1.filmType.jpg", m_params.saveDebugImage);


	Log::WriteAsyncLog("塑膜类型： ", ERR, outInfo.paths.logFile, true, outInfo.classification.filmType.className);
	if (outInfo.classification.filmType.className != m_params.filmType)
	{
		outInfo.status.statusCode = HANDLE_RETURN_FILM_TYPE_ERR;
		outInfo.status.errorMessage = "塑膜类型错误!";
		Log::WriteAsyncLog("塑膜类型错误!", ERR, outInfo.paths.logFile, true);
	}

	if ((m_params.saveTrain == 1 || m_params.saveTrain == 2) && outInfo.status.statusCode == HANDLE_RETURN_OK)
	{
		outInfo.system.startTime = COM->time_t2string_with_ms();
		cv::Mat imgSave;
		cv::resize(outInfo.images.filmRegion.mat(), imgSave, cv::Size(64, 64));
		outInfo.system.startTime = COM->time_t2string_with_ms();
		COM->CreateDir(outInfo.paths.trainDir + "FILM/" + outInfo.classification.filmType.className);
		cv::imwrite(outInfo.paths.trainDir + "FILM/" + outInfo.classification.filmType.className + "/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", imgSave);
	}
	else if (m_params.saveTrain == 1 || m_params.saveTrain == 3 && outInfo.status.statusCode != HANDLE_RETURN_OK)
	{
		outInfo.system.startTime = COM->time_t2string_with_ms();
		cv::Mat imgSave;
		cv::resize(outInfo.images.filmRegion.mat(), imgSave, cv::Size(64, 64));
		outInfo.system.startTime = COM->time_t2string_with_ms();
		COM->CreateDir(outInfo.paths.trainDir + "FILM/" + outInfo.classification.filmType.className);
		cv::imwrite(outInfo.paths.trainDir + "FILM/" + outInfo.classification.filmType.className + "/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", imgSave);
	}
}


void InspHandle::Handle_DrawResult(InspHandleOut& outInfo) {
	Log::WriteAsyncLog("开始绘制结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

	outInfo.images.outputImg.stageName = "Handle_DrawResult";
	outInfo.images.outputImg.description = "绘制全部结果: " + std::to_string(m_params.saveDebugImage);
	outInfo.images.outputImg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();


	rectangle(outInfo.images.outputImg.mat(), m_params.roiRect, Colors::YELLOW, 1, cv::LINE_AA);

	auto format = [](float conf) {
		return (std::ostringstream() << std::fixed << std::setprecision(2) << conf).str();
	};
	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{
		if (outInfo.locate.details[i].className == "提手")
		{
			rectangle(outInfo.images.outputImg.mat(), outInfo.locate.details[i].box, Colors::GREEN, 1, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg.mat(),
				(outInfo.locate.details[i].className + "," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x, outInfo.locate.details[i].box.y + outInfo.locate.details[i].box.height - 50),
				Colors::GREEN, 45, FW_BOLD);
		}
		else if (outInfo.locate.details[i].className == "提手挂反")
		{
			rectangle(outInfo.images.outputImg.mat(), outInfo.locate.details[i].box, Colors::RED, 1, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg.mat(),
				(outInfo.locate.details[i].className + "," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x, outInfo.locate.details[i].box.y + outInfo.locate.details[i].box.height - 50),
				Colors::RED, 45, FW_BOLD);
		}
		else if (outInfo.locate.details[i].className == "无提手")
		{
			rectangle(outInfo.images.outputImg.mat(), outInfo.locate.details[i].box, Colors::RED, 1, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg.mat(),
				(outInfo.locate.details[i].className + "," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x, outInfo.locate.details[i].box.y + outInfo.locate.details[i].box.height + 10),
				Colors::RED, 45, FW_BOLD);
		}
		else if (outInfo.locate.details[i].className == "不到位")
		{
			rectangle(outInfo.images.outputImg.mat(), outInfo.locate.details[i].box, Colors::RED, 1, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg.mat(),
				(outInfo.locate.details[i].className + "," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x, outInfo.locate.details[i].box.y + outInfo.locate.details[i].box.height - 50),
				Colors::RED, 45, FW_BOLD);
		}
		else if (outInfo.locate.details[i].className == "有塑膜")
		{
			rectangle(outInfo.images.outputImg.mat(), outInfo.locate.details[i].box, Colors::GREEN, 1, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg.mat(),
				(outInfo.locate.details[i].className + "," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x, outInfo.locate.details[i].box.y - 50),
				Colors::GREEN, 45, FW_BOLD);
		}
		else if (outInfo.locate.details[i].className == "无塑膜" && m_params.checkFilmType)
		{
			rectangle(outInfo.images.outputImg.mat(), outInfo.locate.details[i].box, Colors::RED, 1, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg.mat(),
				(outInfo.locate.details[i].className + "," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x, outInfo.locate.details[i].box.y - 50),
				Colors::RED, 45, FW_BOLD);
		}
	}


	std::string rv = "ID = " + std::to_string(outInfo.system.jobId) + ", " + "RV = " + std::to_string(outInfo.status.statusCode) + ", " + outInfo.status.errorMessage;
	if (outInfo.status.statusCode == HANDLE_RETURN_OK) {
		putTextZH(outInfo.images.outputImg.mat(), rv.c_str(), cv::Point(15, 30), Colors::GREEN, 55, FW_BOLD);
	}
	else {
		putTextZH(outInfo.images.outputImg.mat(), rv.c_str(), cv::Point(15, 30), Colors::RED, 55, FW_BOLD);
	}

	if (m_params.handleType != "0" && m_params.handleType != "不检测")
	{
		if (m_params.handleType == outInfo.classification.handleType.className)
		{
			putTextZH(outInfo.images.outputImg.mat(), ("提手类型: " + outInfo.classification.handleType.className).c_str(), cv::Point(15, 140), Colors::GREEN, 35, FW_BOLD);
		}
		else
		{
			putTextZH(outInfo.images.outputImg.mat(), ("提手类型: " + outInfo.classification.handleType.className).c_str(), cv::Point(15, 140), Colors::RED, 35, FW_BOLD);
		}
		
	}
	if (m_params.filmType != "0" && m_params.handleType != "filmType")
	{
		if (m_params.filmType == outInfo.classification.filmType.className)
		{
			putTextZH(outInfo.images.outputImg.mat(), ("塑膜类型: " + outInfo.classification.filmType.className).c_str(), cv::Point(15, 200), Colors::GREEN, 35, FW_BOLD);
		}
		else
		{
			putTextZH(outInfo.images.outputImg.mat(), ("塑膜类型: " + outInfo.classification.filmType.className).c_str(), cv::Point(15, 200), Colors::RED, 35, FW_BOLD);
		}	
	}
	

	DAS->DAS_Img(outInfo.images.outputImg.mat(), outInfo.paths.intermediateImagesDir + "10.outputImg.jpg", m_params.saveDebugImage);

	Log::WriteAsyncLog(rv, INFO, outInfo.paths.logFile, true);
}

int InspHandle::Handle_Main(InspHandleOut& outInfo) {
	try {
		double time0 = static_cast<double>(cv::getTickCount());
		if (outInfo.status.statusCode == HANDLE_RETURN_OK)
		{
			Log::WriteAsyncLog("Handle_Main!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			if (m_params.checkHandleType != 0 || m_params.checkFilmType != 0)
			{
				// 第1步:定位塑膜
				Handle_SetROI(outInfo);

				// 第2步:定位塑膜
				Handle_LocateHandle(outInfo);

				// 第3步:提手类型检测
				Handle_CheckHandle(outInfo);

				// 第4步:塑膜类型检测
				Handle_CheckFilm(outInfo);
			}
			else
			{
				outInfo.status.statusCode = HANDLE_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "提手、塑膜检测全部关闭！！！";
				Log::WriteAsyncLog("提手、塑膜检测全部关闭!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
			}
		}
			 
		// 第5步:绘制结果
		Handle_DrawResult(outInfo);

		if (outInfo.status.statusCode == HANDLE_RETURN_OK) {
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
		return HANDLE_RETURN_ALGO_ERR;
	}

	return outInfo.status.statusCode;
}