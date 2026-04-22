#include "HeaderDefine.h"
#include "InspBoxBag.h"
#include "InspBoxBagStruct.h"
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
std::shared_mutex InspBoxBag::modelLoadMutex;
std::map<std::string, std::string> InspBoxBag::sewDetectionModelMap;
std::map<std::string, std::string> InspBoxBag::sewClassifyModelMap;
std::map<int, InspBoxBagIn> InspBoxBag::cameraConfigMap;

// 构造函数
InspBoxBag::InspBoxBag(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
	bool isLoadConfig, int timeOut, InspBoxBagOut& outInfo)
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

	if (img.empty()) {
		outInfo.status.statusCode = BOXBAG_RETURN_ALGO_ERR;
		outInfo.status.errorMessage = "输入图像为空!";
		Log::WriteAsyncLog("输入图像为空!", INFO, outInfo.paths.logFile, true);
		return;
	}

	if (img.channels() == 1) {
		cv::cvtColor(img, m_img, cv::COLOR_GRAY2BGR);
		outInfo.images.outputImg = m_img.clone();
	}
	else if (img.channels() == 3) {
		m_img = img.clone();
		outInfo.images.outputImg = m_img.clone();
	}

	m_params.timeOut = timeOut;
	m_timeoutFlagRef = &outInfo.system.timeoutFlag;
	COM->CreateDir(outInfo.paths.logDirectory);
	Log::WriteAsyncLog("********** Start Inspction JobID = ", INFO, outInfo.paths.logFile, true, outInfo.system.jobId, " ***********");

	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << outInfo.system.cameraId << "  jobId_" << outInfo.system.jobId << std::endl;

	char bufLog[100];
	sprintf(bufLog, "BoxBag/camera_%d/", outInfo.system.cameraId);
	char bufConfig[100];
	sprintf(bufConfig, "/InspBoxBagConfig_%d.txt", outInfo.system.cameraId);
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
			outInfo.status.statusCode = BOXBAG_RETURN_CONFIG_ERR;
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
			outInfo.status.statusCode = BOXBAG_RETURN_CONFIG_ERR;
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
			outInfo.status.statusCode = BOXBAG_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("定位阈值文件-参数设置错误！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "定位阈值文件-参数设置错误!";
			return;
		}
		else
		{
			Log::WriteAsyncLog("定位参数读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}




		bool loadModel = loadAllModels(outInfo, true);
		if (!loadModel) {
			outInfo.status.statusCode = BOXBAG_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "深度学习模型加载异常!";
			Log::WriteAsyncLog(m_params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "---深度学习模型加载异常!");
			return;
		}

		if (!validateCameraModels(outInfo.system.cameraId)) {
			Log::WriteAsyncLog("相机ID配置错误/模型文件缺失!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = BOXBAG_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "相机ID配置错误/模型文件缺失!";
			throw std::invalid_argument("相机ID配置错误/模型文件缺失!");
		}

		cameraConfigMap[cameraId] = m_params;
	}
	else
	{
		m_params = cameraConfigMap[cameraId];
	}



	if (outInfo.status.statusCode = BOXBAG_RETURN_OK)
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
bool InspBoxBag::validateCameraModels(int cameraId) {
	std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
	return sewDetectionModelMap.count("sewDetection_" + std::to_string(cameraId)) &&
		sewClassifyModelMap.count("sewClassify_" + std::to_string(cameraId));
}

// 加载所有模型到ModelManager
bool InspBoxBag::loadAllModels(InspBoxBagOut& outInfo, bool ini) {
	if (!ini) {
		Log::WriteAsyncLog("跳过模型加载!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return true;
	}

	const int cameraId = outInfo.system.cameraId;
	const cv::String key = std::to_string(cameraId);

	// 获取当前相机专用模型路径
	std::vector<std::string> cameraModelPaths;

	// 1. 添加检测模型
	std::string detectionKey = "sewDetection_" + std::to_string(cameraId);
	if (auto it = sewDetectionModelMap.find(detectionKey); it != sewDetectionModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	// 2. 添加分类模型
	std::string classifyKeyCap = "sewClassify_" + std::to_string(cameraId);
	if (auto it = sewClassifyModelMap.find(classifyKeyCap); it != sewClassifyModelMap.end()) {
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
			FinsClassification sewType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.sewClassifyWeightsFile, m_params.sewClassifyClassName, iniImg);
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
bool InspBoxBag::readParams(cv::Mat img, const std::string& filePath, InspBoxBagIn& params, InspBoxBagOut& outInfo, const std::string& fileName) {
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
		if (keyWord == "BOXBAG_SAVE_DEBUG_IMAGE") {
			params.saveDebugImage = std::stoi(value);
		}
		else if (keyWord == "BOXBAG_SAVE_RESULT_IMAGE") {
			params.saveResultImage = std::stoi(value);
		}
		else if (keyWord == "BOXBAG_SAVE_LOG_TXT") {
			params.saveLogTxt = std::stoi(value);
		}
		else if (keyWord == "BOXBAG_DRAW_RESULT") {
			params.drawResult = std::stoi(value);
		}
		else if (keyWord == "BOXBAG_SAVE_TRAIN") {
			params.saveTrain = std::stoi(value);
		}
		else if (keyWord == "BOXBAG_ROI_X") {
			params.roiRect.x = std::stoi(value);
			if (params.roiRect.x < 0 || params.roiRect.x > img.cols)
			{
				outInfo.status.errorMessage = "ROI_X: 超出图像范围!";
				Log::WriteAsyncLog("ROI_X: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOXBAG_ROI_Y") {
			params.roiRect.y = std::stoi(value);
			if (params.roiRect.y < 0 || params.roiRect.y > img.rows)
			{
				outInfo.status.errorMessage = "ROI_Y: 超出图像范围!";
				Log::WriteAsyncLog("ROI_Y: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "BOXBAG_ROI_W") {
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
		else if (keyWord == "BOXBAG_ROI_H") {
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

		else if (keyWord == "BOXBAG_HARDWARE_TYPE") {
			params.hardwareType = std::stoi(value);
		}
		else if (keyWord == "BOXBAG_MODEL_TYPE") {
			params.modelType = std::stoi(value);
		}
		else if (keyWord == "BOXBAG_LOCATE_MODEL_WEIGHTS_FLIE") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
			std::string camera = std::to_string(outInfo.system.cameraId);
			sewDetectionModelMap["sewDetection_" + camera] = value;
			params.locateWeightsFile = value;
			if (!COM->FileExistsModern(params.locateWeightsFile))
			{
				outInfo.status.errorMessage = "定位模型文件缺失!";
				Log::WriteAsyncLog(params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "--定位模型文件文件缺失！");
				return false;
			}
		}
		else if (keyWord == "BOXBAG_LOCATE_MODEL_CONFIGURATIO_FLIE") {
			params.locateThreshConfig = value;
			if (!COM->FileExistsModern(params.locateThreshConfig))
			{
				outInfo.status.errorMessage = "定位阈值文件缺失!";
				Log::WriteAsyncLog(params.locateThreshConfig, ERR, outInfo.paths.logFile, true, "--定位阈值文件缺失！");
				return false;
			}
		}
		else if (keyWord == "BOXBAG_CLASSFY_MODEL_WEIGHTS_FLIE") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);  // 加锁
			std::string camera = std::to_string(outInfo.system.cameraId);
			sewClassifyModelMap["sewClassify_" + camera] = value;
			params.sewClassifyWeightsFile = value;
			if (params.isCheckColor)
			{
				if (!COM->FileExistsModern(params.sewClassifyWeightsFile))
				{
					outInfo.status.errorMessage = "缝线分类模型文件缺失!";
					Log::WriteAsyncLog(params.sewClassifyWeightsFile, ERR, outInfo.paths.logFile, true, "--缝线分类模型文件缺失！");
					return false;
				}
			}
		}
	}

	ifs.close();
	return true;
}


void InspBoxBag::BoxBag_SetROI(InspBoxBagOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != BOXBAG_RETURN_OK) {
		Log::WriteAsyncLog("跳过ROI区域获取!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始ROI区域获取!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	outInfo.images.roiImg = m_img(m_params.roiRect).clone();


}

void InspBoxBag::BoxBag_LocateBoxBag(InspBoxBagOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != BOXBAG_RETURN_OK) {
		Log::WriteAsyncLog("跳过定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始定位!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (m_params.locateWeightsFile.find(".onnx") != std::string::npos)
	{
		outInfo.locate.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.locateWeightsFile, m_params.locateClassName, outInfo.images.roiImg, 0.1, 0.5);
	}
	else
	{
		outInfo.status.statusCode = BOXBAG_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}
	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{
		outInfo.locate.details[i].box = ANA->AdjustROI(outInfo.locate.details[i].box, outInfo.images.roiImg);
		outInfo.locate.details[i].box.x += m_params.roiRect.x;
		outInfo.locate.details[i].box.y += m_params.roiRect.y;
	}

	Log::WriteAsyncLog("开始分析定位结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	for (int i = outInfo.locate.details.size() - 1; i >= 0; --i)
	{
		auto& locate = outInfo.locate.details[i];
		int paramIndex = -1; // 根据缺陷类别设置对应参数索引

		bool valid = true;
		if (locate.className == "OK")paramIndex = 1;
		else if (locate.className == "NG")	paramIndex = 0;

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


	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{

		if (outInfo.locate.details[i].className == "NG")
		{
			outInfo.status.errorMessage = "露袋!";
			Log::WriteAsyncLog("露袋!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = BOXBAG_RETURN_BAG;
			break;
		}
	}


	if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
	{
		if (outInfo.status.statusCode == BOXBAG_RETURN_BAG)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/NG/");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/NG/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/NG/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);

		}
	}

	if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
	{
		if (outInfo.status.statusCode == BOXBAG_RETURN_OK)
		{
			for (int i = 0; i < outInfo.locate.details.size(); i++)
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
			}
		}

	}

}


void InspBoxBag::BoxBag_CheckBoxBagType(InspBoxBagOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != BOXBAG_RETURN_OK) {
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
			if (m_params.sewClassifyWeightsFile.find(".onnx") != std::string::npos || m_params.sewClassifyWeightsFile.find(".engine") != std::string::npos)
			{
				classfyName = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.sewClassifyWeightsFile, m_params.sewClassifyClassName, m_img(outInfo.locate.details[i].box));
			}
			else
			{
				outInfo.status.statusCode = BOXBAG_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx或engine!";
				Log::WriteAsyncLog("模型文件异常，目前仅支持onnx或engine!", ERR, outInfo.paths.logFile, true);

				return;
			}
		}

		outInfo.locate.details[i].className = classfyName.className;
		//outInfo.locate.details[i].confidence = classfyName.confidence;
	}

}



void InspBoxBag::BoxBag_DrawResult(InspBoxBagOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	Log::WriteAsyncLog("开始绘制结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);


	int fontSize = 25;

	auto format = [](float conf) {
		return (std::ostringstream() << std::fixed << std::setprecision(2) << conf).str();
	};

	rectangle(outInfo.images.outputImg, m_params.roiRect, Colors::YELLOW, 3, cv::LINE_AA);

	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{
		if (outInfo.locate.details[i].className == "OK")
		{

			rectangle(outInfo.images.outputImg, outInfo.locate.details[i].box, Colors::GREEN, 3, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg,
				("OK," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x, outInfo.locate.details[i].box.y + outInfo.locate.details[i].box.height + 10),
				Colors::GREEN, 45, FW_BOLD);
		}
		else if (outInfo.locate.details[i].className == "NG")
		{
			rectangle(outInfo.images.outputImg, outInfo.locate.details[i].box, Colors::RED, 3, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg,
				(outInfo.locate.details[i].className + "," + std::to_string(outInfo.locate.details[i].box.width) + "," + std::to_string(outInfo.locate.details[i].box.height) + "," + format(outInfo.locate.details[i].confidence)).c_str(),
				cv::Point(outInfo.locate.details[i].box.x, outInfo.locate.details[i].box.y + outInfo.locate.details[i].box.height + 10),
				Colors::RED, 45, FW_BOLD);
		}
	}


	// 4. 绘制总体状态
	std::string rv = "ID = " + std::to_string(outInfo.system.jobId) +
		", RV = " + std::to_string(outInfo.status.statusCode) +
		", " + outInfo.status.errorMessage;

	cv::Scalar statusColor = (outInfo.status.statusCode == BOXBAG_RETURN_OK) ?
		Colors::GREEN : Colors::RED;

	putTextZH(outInfo.images.outputImg, rv.c_str(), cv::Point(15, 30), statusColor, 50, FW_BOLD);



	// 6. 保存图像
	DAS->DAS_Img(outInfo.images.outputImg, outInfo.paths.intermediateImagesDir + "10.outputImg.jpg", m_params.saveDebugImage);
}

std::future<int> InspBoxBag::RunInspectionAsync(InspBoxBagOut& outInfo) {
	return std::async(std::launch::async, [this, &outInfo] {
		// 设置超时检查起点
		m_startTime = std::chrono::high_resolution_clock::now();

		// 执行主检测逻辑
		return BoxBag_Main(outInfo, true);
		});
}

int InspBoxBag::BoxBag_Main(InspBoxBagOut& outInfo, bool checkTimeout) {
	try {
		double time0 = static_cast<double>(cv::getTickCount());
		if (outInfo.status.statusCode == BOXBAG_RETURN_OK)
		{
			Log::WriteAsyncLog("BoxBag_Main!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			if (checkTimeout && CheckTimeout(m_params.timeOut)) {
				if (m_safeTimeoutFlag) {
					m_safeTimeoutFlag->store(true);
				}
				return BOXBAG_RETURN_TIMEOUT;
			}

			if (outInfo.status.statusCode != BOXBAG_RETURN_OK) {
				Log::WriteAsyncLog("跳过获取模型!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
			}
			else
			{
				std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
				std::string cameraIdStr = std::to_string(outInfo.system.cameraId);
				m_params.locateWeightsFile = sewDetectionModelMap["sewDetection_" + cameraIdStr];
				m_params.sewClassifyWeightsFile = sewClassifyModelMap["sewClassify_" + cameraIdStr];
			}


			// 第1步:设置ROI
			BoxBag_SetROI(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return BOXBAG_RETURN_TIMEOUT;
			}

			// 第2步:定位
			BoxBag_LocateBoxBag(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return BOXBAG_RETURN_TIMEOUT;
			}

			//// 第3步:分类
			//BoxBag_CheckBoxBagType(outInfo);
			//if (CheckTimeout(m_params.timeOut))
			//{
			//	Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			//	return BOXBAG_RETURN_TIMEOUT;
			//}


		}


		// 第9步:绘制结果
		BoxBag_DrawResult(outInfo);
		if (CheckTimeout(m_params.timeOut))
		{
			Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			return BOXBAG_RETURN_TIMEOUT;
		}

		if (outInfo.status.statusCode == BOXBAG_RETURN_OK) {
			DAS->DAS_Img(outInfo.images.outputImg,
				outInfo.paths.resultsOKDir + std::to_string(outInfo.system.jobId) + ".jpg",
				m_params.saveResultImage);
		}
		else {
			DAS->DAS_Img(outInfo.images.outputImg,
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
		return BOXBAG_RETURN_ALGO_ERR;
	}


	return outInfo.status.statusCode;
}