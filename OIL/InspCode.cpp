#include "HeaderDefine.h"
#include "InspCode.h"
#include "InspCodeStruct.h"
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
#include <fstream>

namespace fs = std::filesystem;
std::unordered_map<char, std::string> symbol_map = {
	{'/', "slash"}, {'\\', "backslash"}, {':', "colon"}, {'*', "asterisk"},
	{'?', "question"}, {'"', "quote"}, {'<', "less"}, {'>', "greater"},
	{'|', "pipe"}, {'#', "hash"}, {'(', "paren_open"}, {')', "paren_close"},
	{'[', "bracket_open"}, {']', "bracket_close"}, {'{', "brace_open"},
	{'}', "brace_close"}, {'&', "ampersand"}, {'%', "percent"}, {'$', "dollar"},
	{'@', "at"}, {'!', "exclamation"}, {';', "semicolon"}, {',', "comma"},
	{'.', "dot"}, {' ', "space"}, {'-', "hyphen"}
};

// 初始化静态成员
std::shared_mutex InspCode::modelLoadMutex;
std::map<std::string, std::string> InspCode::codeYWModelMap;
std::map<std::string, std::string> InspCode::codeLocateModelMap;
std::map<std::string, std::string> InspCode::codeClassifyModelMap;
std::map<std::string, std::string> InspCode::codeDefectModelMap;
std::map<int, InspCodeIn> InspCode::cameraConfigMap;

static void DebugLogCodeAssist(const char* runId, const char* hypothesisId, const char* location,
	int jobId, int cameraId, int statusCode, int valid, int elapsedMs, float score)
{
	std::ofstream ofs("debug-db9b0f.log", std::ios::app);
	if (!ofs.is_open()) return;
	const auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	ofs << "{\"sessionId\":\"db9b0f\",\"runId\":\"" << runId
		<< "\",\"hypothesisId\":\"" << hypothesisId
		<< "\",\"location\":\"" << location
		<< "\",\"message\":\"code-assist\",\"data\":{\"jobId\":" << jobId
		<< ",\"cameraId\":" << cameraId
		<< ",\"statusCode\":" << statusCode
		<< ",\"valid\":" << valid
		<< ",\"elapsedMs\":" << elapsedMs
		<< ",\"score\":" << score
		<< "},\"timestamp\":" << ts << "}\n";
}

static bool ApplyCodeAssistMatchStatus(InspCodeOut& outInfo, int rv, Log* logger) {
	const int valid = outInfo.geometry.matchLocateResult.valid;
	const auto setError = [&](CODE_RETURN_VAL code, const std::string& msg) {
		outInfo.status.errorMessage = msg;
		outInfo.status.statusCode = code;
		logger->WriteLog("模板", ERR, outInfo.paths.logFile, true, msg);
	};

	switch (valid) {
	case 0:
		setError(CODE_RETURN_MATCH_ERR0, "辅助定位-匹配失败!");
		return false;
	case 1:
		logger->WriteLog("模板", INFO, outInfo.paths.logFile, true, "匹配成功!");
		return true;
	case 2:
		setError(CODE_RETURN_MATCH_ERR5, "辅助定位-得分过低!");
		return false;
	case 3:
		setError(CODE_RETURN_MATCH_ERR1, "辅助定位-歪斜!");
		return false;
	case 5:
		setError(CODE_RETURN_MATCH_ERR2, "辅助定位-水平偏移!");
		return false;
	case 6:
		setError(CODE_RETURN_MATCH_ERR3, "辅助定位-垂直偏移!");
		return false;
	case 7:
		setError(CODE_RETURN_MATCH_ERR4, "辅助定位-距离偏移!");
		return false;
	default:
		break;
	}

	if (rv == 8) {
		setError(CODE_RETURN_TIMEOUT, "算法超时!");
		return false;
	}
	return true;
}
std::map<int, std::deque<std::string>> InspCode::recentCodeContents;
std::map<int, std::string> InspCode::lastSerialNumber;
std::mutex InspCode::staticMutex;

// 构造函数
InspCode::InspCode(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
	bool isLoadConfig, int timeOut, InspCodeOut& outInfo)
	: LOG(std::make_unique<Log>()),
	ANA(std::make_unique<AnalyseMat>()),
	COM(std::make_unique<Common>()),
	MF(std::make_unique<MatchFun>()),
	BAQ(std::make_unique<BarAndQR>()),
	TXT(std::make_unique<TxtOperater>())
{
	outInfo.system.startTime = COM->time_t2string_with_ms();
	if (jobId <= 0)
	{
		lastSerialNumber.clear();
	}
	if (img.empty()) {
		outInfo.status.statusCode = CODE_RETURN_INPUT_PARA_ERR;
		outInfo.status.errorMessage = "输入图像为空!";
		Log::WriteAsyncLog("输入图像为空!", INFO, outInfo.paths.logFile, true);
		return;
	}

	if (img.channels() == 1) {
		m_imgGray = img.clone();
		cv::cvtColor(img, m_img, cv::COLOR_GRAY2BGR);
		outInfo.images.outputImg = m_img.clone();
	}
	else if (img.channels() == 3) {
		m_img = img.clone();
		cv::cvtColor(img, m_imgGray, cv::COLOR_BGR2GRAY);
		outInfo.images.outputImg = m_img.clone();
	}

	m_params.timeOut = timeOut;
	m_timeoutFlagRef = &outInfo.system.timeoutFlag;
	COM->CreateDir(outInfo.paths.logDirectory);
	Log::WriteAsyncLog("超时时间阈值 = ", INFO, outInfo.paths.logFile, true,  timeOut);

	bool shouldLoadConfig = isLoadConfig ||
		jobId == 0 ||
		cameraConfigMap.find(cameraId) == cameraConfigMap.end();

	//读取config
	if (shouldLoadConfig)
	{
		bool rv_loadConfig = readParams(img, outInfo.paths.configFile, m_params, outInfo, outInfo.paths.logFile);
		if (!rv_loadConfig) {
			outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = outInfo.status.errorMessage;
			Log::WriteAsyncLog(outInfo.status.errorMessage, ERR, outInfo.paths.logFile, true);
			return;
		}
		else
		{
			Log::WriteAsyncLog("读取config成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}

		if (m_params.isAssist)
		{
			outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("辅助定位功能暂时无法使用，请重新选择！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "辅助定位功能暂时无法使用，请重新选择！";
			return;
		}

		if (m_params.isAssist && m_params.isYW)
		{
			outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("辅助定位和有无检测不能同时开启！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "辅助定位和有无检测不能同时开启!";
			return;
		}


		//读取喷码基础参数
		if (LoadConfigCodeBasic(
			m_params.basicConfig,
			m_params.basicInfo,
			outInfo.paths.logFile) != 1)
		{
			outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("喷码基础参数设置错误！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "喷码基础参数设置错误!";
			return;
		}
		else
		{
			Log::WriteAsyncLog("喷码基础参数读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}

		//检测roi
		if (!ANA->JudgeRectIn(cv::Rect(0, 0, img.cols, img.rows), m_params.basicInfo.roi)) {
			outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "roi 设置超出图像范围!";
			Log::WriteAsyncLog("roi 设置超出图像范围", ERR, outInfo.paths.logFile, true);
			return;
		}

		if (m_params.isAssist)
		{
			int load_rv = LoadConfigMatchLocate(
				m_params.assistConfig,
				m_params.assistPara,
				outInfo.paths.logFile);
			if (load_rv == -1) {
				LOG->WriteLog(m_params.assistConfig, INFO, outInfo.paths.logFile, true, " -- 模板配置文件丢失!");
				outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "模板配置文件丢失!";
				loadCodeConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}
			else if (load_rv == -2) {
				LOG->WriteLog(m_params.assistConfig, INFO, outInfo.paths.logFile, true, " -- 模板配置文件格式错误!");
				outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "模板配置文件格式错误!";
				loadCodeConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}
			else if (load_rv == -3) {
				LOG->WriteLog(m_params.assistConfig, INFO, outInfo.paths.logFile, true, " -- 模板配置为空!");
				outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "模板配置为空!";
				loadCodeConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}



			// 模板图像加载和特征提取
			if (isLoadConfig || !loadCodeConfigSuccess[outInfo.system.cameraId] || jobId == 0 || jobId == 1) {
				LOG->WriteLog("开始读取匹配模板!", INFO, outInfo.paths.logFile, true);
				MatchLocateConfig& matchCfg = m_params.assistPara;

				vector<string> fn;
				fn = COM->getImageFilesInDirectory(matchCfg.templatePath);
				cv::glob(matchCfg.templatePath, fn, false);

				if (matchCfg.matchType == 0 || matchCfg.matchType == 1) {
					// Halcon辅助定位 - 创建形状模型
					for (int j = 0; j < fn.size(); j++) {
						try {
							// 读取图像
							HObject ho_Image;

							Mat templ = imread(fn[j], 1);
							Rect roiCur = matchCfg.templatePose;
							roiCur.x -= matchCfg.extW;
							roiCur.y -= matchCfg.extH;
							roiCur.width += 2 * matchCfg.extW;
							roiCur.height += 2 * matchCfg.extH;

							if (ANA->IsRectOutOfBounds(roiCur, m_img))
							{
								LOG->WriteLog(m_params.assistConfig, INFO, outInfo.paths.logFile, true, " -- 模版roi超出图像范围!");
								outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
								outInfo.status.errorMessage = "模版roi超出图像范围!";
								loadCodeConfigSuccess[outInfo.system.cameraId] = false;
								return;
							}
							ANA->Mat2HObject(templ, ho_Image);

							matchCfg.templateMats.push_back(templ);

							// 创建形状模型
							HTuple hv_ModelID;
							//// 创建匹配模型
							switch (matchCfg.matchType) {
							case 0: // 形状匹配
								HalconCpp::CreateShapeModel(
									ho_Image,
									matchCfg.numLevels,
									matchCfg.angleRange[0],
									matchCfg.angleRange[1] - matchCfg.angleRange[0],
									matchCfg.angleStep,   // 角度步长
									HalconCpp::HTuple(MF->GetOptimizationString(matchCfg.optimization)),
									HalconCpp::HTuple(MF->GetMetricString(matchCfg.metric)),
									matchCfg.contrast,
									matchCfg.minContrast,
									&hv_ModelID);
								break;
							case 1: // 缩放匹配（修正参数）
								HalconCpp::CreateScaledShapeModel(
									ho_Image,
									matchCfg.numLevels,
									matchCfg.angleRange[0],
									matchCfg.angleRange[1] - matchCfg.angleRange[0],
									matchCfg.angleStep,
									matchCfg.scaleRange[0],
									matchCfg.scaleRange[1],
									matchCfg.scaleStep,
									HalconCpp::HTuple(MF->GetOptimizationString(matchCfg.optimization)),
									HalconCpp::HTuple(MF->GetMetricString(matchCfg.metric)),
									matchCfg.contrast,
									matchCfg.minContrast,
									&hv_ModelID);
								break;

							default:
								LOG->WriteLog(m_params.assistConfig, INFO, outInfo.paths.logFile, true, " -- 模板配置文件内容为空!");
								outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
								outInfo.status.errorMessage = "模板配置文件内容为空!";
								loadCodeConfigSuccess[outInfo.system.cameraId] = false;
								return;
							}

							// 获取模型轮廓
							HObject ho_ModelContours;
							GetShapeModelContours(&ho_ModelContours, hv_ModelID, 1);
							matchCfg.labelAllTemplateHObjects.push_back(ho_ModelContours);
							matchCfg.labelAllTemplateHTuple.push_back(hv_ModelID);
							//matchCfg.templateMats = COM->ReadImages(matchCfg.templatePaths[j], 0);

						}
						catch (HException& e) {
							LOG->WriteLog("Halcon模型创建失败: " + string(e.ErrorMessage().Text()),
								ERR, outInfo.paths.logFile, true);
							outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
							outInfo.status.errorMessage = "模板模型创建失败!";
							return;
						}
					}
				}
			}
		}
		//读取有无配置文件
		if (LoadConfigYOLO(
			m_params.ywConfig,
			m_params.ywPara,
			m_params.ywClassName,
			outInfo.paths.logFile) != 1)
		{
			outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("有无检测模型-参数设置错误！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "有无检测模型-参数设置错误!";
			return;
		}
		else
		{
			Log::WriteAsyncLog("有无检测模型读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}


		//读取定位配置文件
		if (LoadConfigYOLO(
			m_params.locateConfig,
			m_params.locatePara,
			m_params.locateClassName,
			outInfo.paths.logFile) != 1)
		{
			outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("定位检测模型-参数设置错误！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "定位检测模型-参数设置错误!";
			return;
		}
		else
		{
			Log::WriteAsyncLog("定位检测模型读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}


		//读取字符识别文件
		if (LoadConfigCodeClassfy(
			m_params.classfyConfig,
			m_params.classfyPara,
			m_params.classfyClassName,
			outInfo.paths.logFile) != 1)
		{
			outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
			Log::WriteAsyncLog("字符识别模型-参数设置错误！", ERR, outInfo.paths.logFile, true);
			outInfo.status.errorMessage = "字符识别模型-参数设置错误!";
			return;
		}
		else
		{
			Log::WriteAsyncLog("字符识别型读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}

		// 创建反向映射表
		std::unordered_map<std::string, std::string> reverse_map;
		for (const auto& entry : symbol_map) {
			reverse_map[entry.second] = std::string(1, entry.first);
		}

		// 遍历 ywClassName 并转换英文单词为标点符号
		for (auto& str : m_params.classfyClassName) {
			auto it = reverse_map.find(str);
			if (it != reverse_map.end()) {
				str = it->second;
			}
		}

		m_params.inputInfo = readConfig(m_params.infoConfig);

		////读取缺陷配置文件
		//if (LoadConfigYOLO(
		//	m_params.defectThreshConfig,
		//	m_params.defectPara,
		//	m_params.defectClassName,
		//	outInfo.paths.logFile) != 1)
		//{
		//	outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
		//	Log::WriteAsyncLog("缺陷参数设置错误！", ERR, outInfo.paths.logFile, true);
		//	outInfo.status.errorMessage = "缺陷参数设置错误!";
		//	return;
		//}
		//else
		//{
		//	Log::WriteAsyncLog("缺陷参数读取成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		//}


		////读取分类类型名称
		//std::ifstream ifs1(m_params.classifyNameFile.c_str());
		//if (!ifs1.is_open()) {
		//	outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
		//	outInfo.status.errorMessage = "分类类型文件缺失!";
		//	Log::WriteAsyncLog(m_params.classifyNameFile, ERR, outInfo.paths.logFile, true, "---分类类型文件缺失!");
		//	return;
		//}
		//else
		//{
		//	m_params.classifyClassName.clear();
		//	std::string line;
		//	while (getline(ifs1, line)) m_params.classifyClassName.push_back(line);
		//	Log::WriteAsyncLog("分类类型文件读取成功！", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

		//	bool isTopType = (std::find(m_params.classifyClassName.begin(),
		//		m_params.classifyClassName.end(),
		//		m_params.topType) != m_params.classifyClassName.end());
		//	bool isBottomType = (std::find(m_params.classifyClassName.begin(),
		//		m_params.classifyClassName.end(),
		//		m_params.bottomType) != m_params.classifyClassName.end());

		//	if (m_params.topType != "0" && m_params.topType != "不检测")
		//	{
		//		if (!isTopType)
		//		{
		//			outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
		//			outInfo.status.errorMessage = "当前选择的盖帽类型不在分类类型文件内!";
		//			Log::WriteAsyncLog("当前选择的盖帽类型不在分类类型文件内!", ERR, outInfo.paths.logFile, true);
		//			return;
		//		}
		//	}
		//	if (m_params.bottomType != "0" && m_params.bottomType != "不检测")
		//	{
		//		if (!isBottomType)
		//		{
		//			outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
		//			outInfo.status.errorMessage = "当前选择的盖底类型不在分类类型文件内!";
		//			Log::WriteAsyncLog("当前选择的盖底类型不在分类类型文件内!", ERR, outInfo.paths.logFile, true);
		//			return;
		//		}
		//	}
		//}

		bool loadModel = loadAllModels(outInfo, true);
		if (!loadModel) {
			outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "深度学习模型加载异常!";
			Log::WriteAsyncLog("深度学习模型加载异常!", ERR, outInfo.paths.logFile, true);
			return;
		}

		//if (!validateCameraModels(outInfo.system.cameraId)) {
		//	Log::WriteAsyncLog("相机ID配置错误/模型文件缺失!", ERR, outInfo.paths.logFile, true);
		//	outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
		//	outInfo.status.errorMessage = "相机ID配置错误/模型文件缺失!";
		//	throw std::invalid_argument("相机ID配置错误/模型文件缺失!");
		//}

		// 一维码配置加载
		if (m_params.isCheckBar) {
			int load_rv = LoadConfigBar(m_params.barConfigFile, m_params.barConfigs, outInfo.paths.logFile);
			if (load_rv == -1) {
				LOG->WriteLog(m_params.barConfigFile, INFO, outInfo.paths.logFile, true, " -- 一维码配置文件丢失!");
				outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "一维码配置文件丢失!";
				loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}
			else if (load_rv == -2) {
				LOG->WriteLog(m_params.barConfigFile, INFO, outInfo.paths.logFile, true, " -- 一维码配置文件格式错误!");
				outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "一维码配置文件格式错误!";
				loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}
			else if (load_rv == -3) {
				LOG->WriteLog(m_params.barConfigFile, INFO, outInfo.paths.logFile, true, " -- 一维码配置文件内容为空!");
				outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "一维码配置文件内容为空!";
				loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}


			const std::vector<std::string> barTypes = {
				"EAN-8","EAN-13","EAN-13 Add-On 5",
				"Code 39","Code 128","Codabar",
				"UPC-A","UPC-E",
				"2/5 Interleaved","2/5 Industrial",
				"GS1-128","GS1 DataBar Truncated","GS1 DataBar Stacked Omnidir",
				"GS1 DataBar Limited","GS1 DataBar Expanded Stacked","GS1 DataBar Expanded" };

			const std::vector<std::string> qrTypes = {
				"QR Code", "Aztec Code", "Data Matrix ECC 200",
				"GS1 Aztec Code", "GS1 DataMatrix", "GS1 QR Code",
				"Micro QR Code", "PDF417"
			};


			for (int i = 0; i < m_params.barConfigs.size(); i++) {
				if (m_params.barConfigs[i].checkType == "一维码" || m_params.barConfigs[i].checkType == "1D")
				{

					bool findBarType = false;
					for (int j = 0; j < barTypes.size(); j++)
					{
						if (barTypes[j] == m_params.barConfigs[i].barType)
						{
							findBarType = true;
							std::cout << "当前一维码类型:  " << m_params.barConfigs[i].barType << std::endl;
							break;
						}
						else if (m_params.barConfigs[i].barType == "auto")
						{
							findBarType = true;
							std::cout << "当前一维码类型:  " << m_params.barConfigs[i].barType << std::endl;
							break;
						}
					}
					if (!findBarType)
					{
						std::cout << "一维码类型选择错误，请参考一维码类型选项，注意大小写！ " << std::endl;
						std::cout << "当前支持的一维码类型如下: " << std::endl;
						std::cout << "auto" << std::endl;
						std::cout << "EAN-8，EAN-13，EAN-13 Add-On 5" << std::endl;
						std::cout << "Code 39，Code 128，Codabar" << std::endl;
						std::cout << "UPC_A，UPC_E，UPC_EAN_EXTENSION" << std::endl;
						std::cout << " 2/5 Interleaved，2/5 Industrial" << std::endl;
						std::cout << "GS1-128，GS1 DataBar Truncated，GS1 DataBar Stacked Omnidir，GS1 DataBar Limited，GS1 DataBar Expanded Stacked，GS1 DataBar Expanded" << std::endl;

						LOG->WriteLog(m_params.barConfigs[i].barType, INFO, outInfo.paths.logFile, true, " -- 一维码类型选择错误，请参考一维码类型选项!");
						outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
						outInfo.status.errorMessage = "一维码类型选择错误，请参考一维码类型选项!";
						return;
					}

					m_params.barConfigs[i].targetTypes =
						(m_params.barConfigs[i].barType == "auto") ? barTypes : std::vector<std::string>{ m_params.barConfigs[i].barType };

					//HalconCpp::CreateBarCodeModel(HTuple(), HTuple(), &m_params.barConfigs[i].barCodeHandle);
					//HalconCpp::SetBarCodeParam(m_params.barConfigs[i].barCodeHandle, "stop_after_result_num", 0);
					//HalconCpp::SetBarCodeParam(m_params.barConfigs[i].barCodeHandle, "meas_param_estimation", "false");
					//HalconCpp::SetBarCodeParam(m_params.barConfigs[i].barCodeHandle, "slanted", "false");
					//HalconCpp::SetBarCodeParam(m_params.barConfigs[i].barCodeHandle, "code_types", m_params.barConfigs[i].barType.c_str());

				}
				else if (m_params.barConfigs[i].checkType == "二维码" || m_params.barConfigs[i].checkType == "2D")
				{

					bool findQrType = false;
					for (int j = 0; j < qrTypes.size(); j++)
					{
						if (qrTypes[j] == m_params.barConfigs[i].barType)
						{
							findQrType = true;
							std::cout << "当前二维码类型:  " << m_params.barConfigs[i].barType << std::endl;
							break;
						}
						else if (m_params.barConfigs[i].barType == "auto")
						{
							findQrType = true;
							std::cout << "当前二维码类型:  " << m_params.barConfigs[i].barType << std::endl;
							break;
						}
					}
					if (!findQrType)
					{
						std::cout << "二维码类型选择错误，请参考二维码类型选项，注意大小写！ " << std::endl;
						std::cout << "当前支持的二维码类型如下: " << std::endl;
						std::cout << "auto" << std::endl;
						std::cout << "QR Code, Aztec Code, Data Matrix ECC 200, GS1 Aztec Code, GS1 DataMatrix, GS1 QR Code, Micro QR Code, PDF417" << std::endl;

						LOG->WriteLog(m_params.barConfigs[i].barType, INFO, outInfo.paths.logFile, true, " -- 二维码类型选择错误，请参考二维码类型选项!");
						outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
						outInfo.status.errorMessage = "二维码类型选择错误，请参考二维码类型选项!";
						return;
					}

					m_params.barConfigs[i].targetTypes =
						(m_params.barConfigs[i].barType == "auto") ? qrTypes : std::vector<std::string>{ m_params.barConfigs[i].barType };
				}

				if (!ANA->JudgeRectIn(cv::Rect(0, 0, img.cols, img.rows), m_params.barConfigs[i].roi)) {
					outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "一维码/二维码检测roi设置超出图像范围!";
					LOG->WriteLog("一维码/二维码检测roi设置超出图像范围", ERR, outInfo.paths.logFile, true);
					return;
				}
				if (m_params.barConfigs[i].barType.empty())
				{
					outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "一维码/二维码检测种类选择错误!";
					LOG->WriteLog("一维码/二维码检测种类选择错误", ERR, outInfo.paths.logFile, true);
					return;
				}
				if (m_params.barConfigs[i].checkModel < 0 || m_params.barConfigs[i].checkModel > 1)
				{
					outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "一维码/二维码解析模式选择错误!";
					LOG->WriteLog("一维码/二维码解析模式选择错误", ERR, outInfo.paths.logFile, true);
					return;
				}
				if (m_params.barConfigs[i].info.empty())
				{
					outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "一维码/二维码信息输入空值！";
					LOG->WriteLog("一维码/二维码信息输入空值", ERR, outInfo.paths.logFile, true);
					return;
				}
				if (m_params.barConfigs[i].countRange[0] < m_params.barConfigs[i].countRange[1])
				{
					outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "一维码/二维码数量范围设置错误！";
					LOG->WriteLog("一维码/二维码数量范围设置错误", ERR, outInfo.paths.logFile, true);
					return;
				}
				/*if (m_params.barConfigs[i].modelPath.empty())
				{
					outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "一维码/二维码深度学习模型路径输入空值！";
					LOG->WriteLog("一维码/二维码深度学习模型路径输入空值", ERR, outInfo.paths.logFile, true);
					return;
				}*/
			}
		}

		cameraConfigMap[cameraId] = m_params;
	}
	else
	{
		m_params = cameraConfigMap[cameraId];
	}



	if (outInfo.status.statusCode == CODE_RETURN_OK)
	{
		loadCodeConfigSuccess[outInfo.system.cameraId] = true;
		Log::WriteAsyncLog("参数初始化完成!", INFO, outInfo.paths.logFile, true);
	}
	else
	{
		loadCodeConfigSuccess[outInfo.system.cameraId] = false;
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

InspCode::~InspCode() {
	if (m_timeoutFlagRef) *m_timeoutFlagRef = true;
}


// 验证摄像头ID对应的模型配置是否存在
bool InspCode::validateCameraModels(int cameraId) {
	std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
	return codeYWModelMap.count("codeYW_" + std::to_string(cameraId)) &&
		codeLocateModelMap.count("codeLocate_" + std::to_string(cameraId)) &&
		codeDefectModelMap.count("codeDefect_" + std::to_string(cameraId)) &&
		codeClassifyModelMap.count("codeClassify_" + std::to_string(cameraId));
}

// 加载所有模型到ModelManager
bool InspCode::loadAllModels(InspCodeOut& outInfo, bool ini) {
	if (!ini) {
		Log::WriteAsyncLog("跳过模型加载!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return true;
	}

	const int cameraId = outInfo.system.cameraId;
	const cv::String key = std::to_string(cameraId);

	// 获取当前相机专用模型路径
	std::vector<std::string> cameraModelPaths;

	// 1. 添加有无模型
	std::string ywKey = "codeYW_" + std::to_string(cameraId);
	if (auto it = codeYWModelMap.find(ywKey); it != codeYWModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	// 2. 添加检测模型
	std::string detectionKey = "codeLocate_" + std::to_string(cameraId);
	if (auto it = codeLocateModelMap.find(detectionKey); it != codeLocateModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	// 3. 添加分类模型
	std::string classifyKey = "codeClassify_" + std::to_string(cameraId);
	if (auto it = codeClassifyModelMap.find(classifyKey); it != codeClassifyModelMap.end()) {
		if (COM->FileExistsModern(it->second)) {
			cameraModelPaths.push_back(it->second);
		}
	}

	// 4. 添加缺陷模型
	std::string defectKey = "codeDefect_" + std::to_string(cameraId);
	if (auto it = codeDefectModelMap.find(defectKey); it != codeDefectModelMap.end()) {
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

		/*Mat iniImg = Mat::zeros(cv::Size(2500, 2000), CV_8UC3);
		outInfo.locate.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.locateWeightsFile, m_params.locateClassName, iniImg, 0.5, 0.3);
		outInfo.defects.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.defectWeightsFile, m_params.defectClassName, iniImg, 0.1, 0.5);
		outInfo.classification.topType = InferenceWorker::RunClassification(outInfo.system.cameraId, m_params.classifyWeightsFile, m_params.classifyClassName, iniImg);
		outInfo.classification.topType.className = "";
		Log::WriteAsyncLog("模型初始化完成！", INFO, outInfo.paths.logFile, true);*/

		return true;
	}
	catch (const std::exception& e) {
		Log::WriteAsyncLog("相机" + std::to_string(cameraId) + "模型加载异常: " + std::string(e.what()), ERR, outInfo.paths.logFile, true);
		return false;
	}
}

// 读取参数的函数
bool InspCode::readParams(cv::Mat img, const std::string& filePath, InspCodeIn& params, InspCodeOut& outInfo, const std::string& fileName) {
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
		if (keyWord == "CODE_SAVE_DEBUG_IMAGE") {
			params.saveDebugImage = std::stoi(value);
		}
		else if (keyWord == "CODE_SAVE_RESULT_IMAGE") {
			params.saveResultImage = std::stoi(value);
		}
		else if (keyWord == "CODE_SAVE_LOG_TXT") {
			params.saveLogTxt = std::stoi(value);
		}
		else if (keyWord == "CODE_DRAW_RESULT") {
			params.drawResult = std::stoi(value);
		}
		else if (keyWord == "CODE_SAVE_TRAIN") {
			params.saveTrain = std::stoi(value);
		}
		else if (keyWord == "CODE_BASIC_CONFIG") {
			params.basicConfig = value;
			if (!COM->FileExistsModern(params.basicConfig))
			{
				outInfo.status.errorMessage = "喷码基础参数文件缺失!";
				Log::WriteAsyncLog(params.basicConfig, ERR, outInfo.paths.logFile, true, "--喷码基础参数文件缺失！");
				return false;
			}
		}
		else if (keyWord == "CODE_ASSIST") {
			params.isAssist = std::stoi(value);
		}
		else if (keyWord == "CODE_ASSIST_CONFIG") {
			params.assistConfig = value;
			if (params.isAssist)
			{
				if (!COM->FileExistsModern(params.assistConfig))
				{
					outInfo.status.errorMessage = "辅助定位配置参数文件缺失!";
					Log::WriteAsyncLog(params.assistConfig, ERR, outInfo.paths.logFile, true, "--辅助定位配置参数文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "CODE_HARDWARE_TYPE") {
			params.hardwareType = std::stoi(value);
			if (params.hardwareType < 0 || params.hardwareType > 2) {
				outInfo.status.errorMessage = "硬件类型选择错误";
				Log::WriteAsyncLog("硬件类型选择错误！ hardwareType = ", ERR, outInfo.paths.logFile, true, params.hardwareType);
				return false;
			}
		}
		else if (keyWord == "CODE_MODEL_TYPE") {
			params.modelType = std::stoi(value);
			if (params.modelType < 0 || params.modelType > 1) {
				outInfo.status.errorMessage = "模型类型选择错误";
				Log::WriteAsyncLog("模型类型选择错误！ modelType = ", ERR, outInfo.paths.logFile, true, params.hardwareType);
				return false;
			}
		}
		else if (keyWord == "CODE_CHECK_YW") {
			params.isYW = std::stoi(value);
		}
		else if (keyWord == "CODE_CHECK_YW_CONFIG") {
			params.ywConfig = value;
			if (params.isYW)
			{
				if (!COM->FileExistsModern(params.ywConfig))
				{
					outInfo.status.errorMessage = "有无检测模型配置参数文件缺失!";
					Log::WriteAsyncLog(params.ywConfig, ERR, outInfo.paths.logFile, true, "--有无检测模型配置参数文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "CODE_CHECK_YW_MODEL") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex); // 加锁
			std::string camera = std::to_string(outInfo.system.cameraId);
			codeYWModelMap["codeYW_" + camera] = value;
			params.ywModel = value;
			if (params.isYW)
			{
				if (!COM->FileExistsModern(params.ywModel))
				{
					outInfo.status.errorMessage = "有无检测模型文件缺失!";
					Log::WriteAsyncLog(params.ywModel, ERR, outInfo.paths.logFile, true, "--有无检测模型文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "CODE_CHAR_LOCATE") {
			params.isLocate = std::stoi(value);
		}
		else if (keyWord == "CODE_CHAR_LOCATE_CONFIG") {
			params.locateConfig = value;
			if (params.isLocate)
			{
				if (!COM->FileExistsModern(params.locateConfig))
				{
					outInfo.status.errorMessage = "字符定位模型配置参数文件缺失!";
					Log::WriteAsyncLog(params.locateConfig, ERR, outInfo.paths.logFile, true, "--字符定位模型配置参数文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "CODE_CHAR_LOCATE_MODEL") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex); // 加锁
			std::string camera = std::to_string(outInfo.system.cameraId);
			codeLocateModelMap["codeLocate_" + camera] = value;
			params.locateModel = value;
			if (params.isLocate)
			{
				if (!COM->FileExistsModern(params.locateModel))
				{
					outInfo.status.errorMessage = "字符定位模型文件缺失!";
					Log::WriteAsyncLog(params.locateModel, ERR, outInfo.paths.logFile, true, "--字符定位模型文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "CODE_CHAR_CLASSFY") {
			params.isClassfy = std::stoi(value);
		}
		else if (keyWord == "CODE_CHAR_DEFECT") {
			params.isDefect = std::stoi(value);
			if (params.isClassfy)
			{
				if (!params.isClassfy && params.isDefect) {
					outInfo.status.errorMessage = "未开启字符分类";
					Log::WriteAsyncLog("未开启字符分类", ERR, outInfo.paths.logFile, true);
					return false;
				}
			}
		}
		else if (keyWord == "CODE_CHAR_CLASSFY_CONFIG") {
			params.classfyConfig = value;
			if (params.isClassfy)
			{
				if (!COM->FileExistsModern(params.classfyConfig))
				{
					outInfo.status.errorMessage = "字符分类模型配置参数文件缺失!";
					Log::WriteAsyncLog(params.classfyConfig, ERR, outInfo.paths.logFile, true, "--字符分类模型配置参数文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "CODE_CHAR_CLASSFY_MODEL") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex); // 加锁
			std::string camera = std::to_string(outInfo.system.cameraId);
			codeClassifyModelMap["codeClassify_" + camera] = value;
			params.classfyModel = value;

			if (params.isClassfy)
			{
				if (!COM->FileExistsModern(params.classfyModel))
				{
					outInfo.status.errorMessage = "字符分类模型文件缺失!";
					Log::WriteAsyncLog(params.classfyConfig, ERR, outInfo.paths.logFile, true, "--字符分类模型文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "CODE_INFO_CHECK") {
			params.infoConfig = value;
			if (params.isClassfy)
			{
				if (!COM->FileExistsModern(params.infoConfig))
				{
					outInfo.status.errorMessage = "喷码内容匹配参数文件缺失!";
					Log::WriteAsyncLog(params.infoConfig, ERR, outInfo.paths.logFile, true, "--喷码内容匹配参数文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "CODE_IS_CHECK_BAR") {
			params.isCheckBar = std::stoi(value);
		}
		else if (keyWord == "CODE_BAR_CONFIG_FILE") {
			params.barConfigFile = value;
			if (m_params.isCheckBar)
			{
				if (!COM->FileExistsModern(params.barConfigFile)) {
					outInfo.status.errorMessage = "一维码/二维码检测配置文件缺失!";
					LOG->WriteLog(params.barConfigFile, ERR, outInfo.paths.logFile, true, "--一维码/二维码检测配置文件缺失！");
					return false;
				}
			}
		}
	}

	ifs.close();
	return true;
}

void InspCode::Code_RotateImg(InspCodeOut& outInfo) {
	if (m_params.basicInfo.rotateAngle == 0)
	{
		return;
	}

	if (m_params.basicInfo.rotateCenter.x == 0 && m_params.basicInfo.rotateCenter.y == 0)
	{
		m_params.basicInfo.rotateCenter.x = m_img.cols * 0.5;
		m_params.basicInfo.rotateCenter.y = m_img.rows * 0.5;
	}


	ANA->RotateImg(m_img, m_img, m_params.basicInfo.rotateCenter, -m_params.basicInfo.rotateAngle);
	DAS->DAS_Img(m_imgRotate, outInfo.paths.intermediateImagesDir + "0. m_imgRotate.jpg ", m_params.saveDebugImage);
}


void InspCode::Code_WarpImg(InspCodeOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != CODE_RETURN_OK) {
		Log::WriteAsyncLog("跳过喷码矫正!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else if (m_params.basicInfo.warp == 0)
	{
		Log::WriteAsyncLog("喷码矫正未启用!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始喷码矫正!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	Mat input = m_img(outInfo.geometry.codeRect).clone();
	Point2f startPoint = Point2f(0, input.rows * m_params.basicInfo.warpL);
	Point2f midPoint = Point2f(input.cols / 2, input.rows * m_params.basicInfo.warpM);
	Point2f endPoint = Point2f(input.cols - 1, input.rows * m_params.basicInfo.warpR);

	// 1. 使用三点拟合抛物线 (y = ax² + bx + c)
	vector<Point2f> points = { startPoint, midPoint, endPoint };

	// 构建方程组矩阵
	Mat A = (Mat_<float>(3, 3) <<
		points[0].x * points[0].x, points[0].x, 1,
		points[1].x * points[1].x, points[1].x, 1,
		points[2].x * points[2].x, points[2].x, 1);

	Mat B = (Mat_<float>(3, 1) << points[0].y, points[1].y, points[2].y);

	Mat coeffs;
	solve(A, B, coeffs, DECOMP_SVD);

	float a = coeffs.at<float>(0);
	float b = coeffs.at<float>(1);
	float c = coeffs.at<float>(2);

	// 2. 计算曲线上的点
	int minX = min(startPoint.x, endPoint.x);
	int maxX = max(startPoint.x, endPoint.x);
	int curveWidth = maxX - minX + 1;

	vector<Point2f> curvePoints(curveWidth);
	vector<float> derivatives(curveWidth);
	vector<float> curveLengths(curveWidth);

	curvePoints[0] = Point2f(minX, a * minX * minX + b * minX + c);
	curveLengths[0] = 0.0f;

	for (int i = 1; i < curveWidth; i++) {
		int x = minX + i;
		float y = a * x * x + b * x + c;
		curvePoints[i] = Point2f(x, y);
		derivatives[i] = 2 * a * x + b;
		float segmentLength = norm(curvePoints[i] - curvePoints[i - 1]);
		curveLengths[i] = curveLengths[i - 1] + segmentLength;
	}

	double curveLength = curveLengths[curveWidth - 1];

	// 3. 创建输出图像（与原图相同尺寸）
	Mat output = Mat::zeros(input.size(), input.type());

	// 4. 创建映射表（使用原图尺寸）
	Mat mapX(input.size(), CV_32F);
	Mat mapY(input.size(), CV_32F);

	// 5. 初始化映射表为恒等映射（保持非ROI区域不变）
	for (int i = 0; i < input.rows; i++) {
		for (int j = 0; j < input.cols; j++) {
			mapX.at<float>(i, j) = j;
			mapY.at<float>(i, j) = i;
		}
	}

	// 6. 计算曲线区域的高度（基于图像高度）
	int samplingHeight = static_cast<int>(input.rows);
	int centerY = (startPoint.y + endPoint.y) / 2;
	int roiY = centerY - samplingHeight / 2;

	// 调整ROI的y坐标，确保不超出图像边界
	if (roiY < 0) roiY = 0;
	if (roiY + samplingHeight > input.rows) {
		samplingHeight = input.rows - roiY;
	}

	Rect curveROI(minX, roiY, curveWidth, samplingHeight);

	// 7. 预计算法向量
	vector<Point2f> normals(curveWidth);
	for (int i = 0; i < curveWidth; i++) {
		Point2f tangent(1.0f, derivatives[i]);
		float length = norm(tangent);
		if (length > 1e-5) {
			normals[i] = Point2f(-tangent.y / length, tangent.x / length);
		}
		else {
			normals[i] = Point2f(0, 1);
		}
	}

	// 8. 仅对曲线ROI区域计算映射关系
	parallel_for_(Range(0, curveROI.height), [&](const Range& range) {
		for (int y = range.start; y < range.end; y++) {
			for (int x = 0; x < curveROI.width; x++) {
				// 计算当前点在曲线上的位置
				double ratio = static_cast<double>(x) / curveROI.width;
				double targetLength = ratio * curveLength;

				// 使用二分查找定位曲线段
				int low = 0, high = curveWidth - 1;
				while (low <= high) {
					int mid = low + (high - low) / 2;
					if (curveLengths[mid] < targetLength) {
						low = mid + 1;
					}
					else {
						high = mid - 1;
					}
				}

				int index = min(max(high, 0), curveWidth - 2);
				double segRatio = (targetLength - curveLengths[index]) /
					(curveLengths[index + 1] - curveLengths[index]);
				segRatio = max(0.0, min(1.0, segRatio));

				Point2f curvePoint = curvePoints[index] +
					(curvePoints[index + 1] - curvePoints[index]) * segRatio;

				// 插值法向量
				Point2f normal = normals[index] +
					(normals[index + 1] - normals[index]) * segRatio;
				float n = norm(normal);
				if (n > 1e-5) {
					normal.x /= n;
					normal.y /= n;
				}
				else {
					normal = Point2f(0, 1);
				}

				// 计算采样点 - 沿法线方向
				int offsetY = y - curveROI.height / 2;
				Point2f srcPoint = curvePoint + normal * offsetY;

				// 存储映射到全局坐标（curveROI区域）
				int globalY = y + curveROI.y;
				int globalX = x + curveROI.x;

				if (globalX >= 0 && globalX < input.cols && globalY >= 0 && globalY < input.rows) {
					mapX.at<float>(globalY, globalX) = srcPoint.x;
					mapY.at<float>(globalY, globalX) = srcPoint.y;
				}
			}
		}
		});

	// 9. 对整个输入图像应用映射
	remap(input, output, mapX, mapY, INTER_LINEAR, BORDER_CONSTANT, Scalar(0, 0, 0));
	m_imgWarp = output;
}

void InspCode::Code_SetROI(InspCodeOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != CODE_RETURN_OK) {
		Log::WriteAsyncLog("跳过ROI区域获取!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始ROI区域获取!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	outInfo.images.roi.data = std::make_shared<cv::Mat>(m_img(m_params.basicInfo.roi).clone());
	outInfo.images.roi.stageName = "Code_Main";
	outInfo.images.roi.description = "ROI区域获取";
	outInfo.images.roi.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

}


// 角度归一化到 [-180,180)
inline double NormalizeDeg(double deg) {
	while (deg >= 180.0) deg -= 360.0;
	while (deg < -180.0) deg += 360.0;
	return deg;
}

// 将 Halcon 的弧度（如果传入）转换为度并做归一化（Halcon->你的代码里是 -rad 转 deg）
inline double HalconRadToDegNormalized(double rad) {
	double deg = -rad * 180.0 / CV_PI;
	return NormalizeDeg(deg);
}

// 绕原点按角度（度，逆时针为正）旋转点
inline cv::Point2d rotate_point_ccw(const cv::Point2d& p, double angle_deg) {
	double rad = angle_deg * CV_PI / 180.0;
	double c = std::cos(rad), s = std::sin(rad);
	return cv::Point2d(p.x * c - p.y * s, p.x * s + p.y * c);
}

// ---------- 结果结构 ----------
struct MappedTargetResult {
	cv::Point2d targetCenter;               // 计算得到的 targetRoi 中心（在 src 图像坐标系）
	cv::RotatedRect targetRRect;            // 该 targetRoi 映射到 src 上的最小旋转矩形
	std::array<cv::Point2f, 4> corners;     // 四个角（按顺时针或逆时针）
	double usedScale = 1.0;                 // 使用的 scale
	double usedAngleDeg = 0.0;              // 使用的角度（度）
};

// ---------- 主函数 ----------
// 说明：
//  - matchLocatePara.templatePose: 模板上用于匹配的特征区域（矩形）
//  - matchLocatePara.targetRoi: 在模板坐标系下的目标检测区域
//  - matchLocatePara.templateMats: 含模板图像矩阵的 vector（用于从尺寸估算 scale）
//  - matchLocateResult: 已匹配得到的 MatchResult（其中 .center 为 templatePose 在 src 上的映射位置）
// 返回：MappedTargetResult 包含计算好的 targetRoi 中心、旋转矩形、角点、以及所用 scale/angle
inline MappedTargetResult MapTargetRoiByRelativePosition(
	const MatchLocateConfig& matchLocatePara,
	const MatchResult& matchLocateResult)
{
	MappedTargetResult out;

	// 检查前置条件
	if (matchLocateResult.id < 0) {
		throw std::invalid_argument("matchLocateResult.id < 0");
	}
	if (matchLocateResult.center.x == 0 && matchLocateResult.center.y == 0) {
		// 这只是一个提醒：通常 center 应由匹配结果给出
		// 仍然继续计算（结果会以 matchLocateResult.center 为基准）
	}

	// 1) 计算模板坐标系下的两个中心：featureCenter（templatePose 的中心）和 targetCenter（targetRoi 的中心）
	cv::Point2d tplFeatureCenter(
		matchLocatePara.templatePose.x + matchLocatePara.templatePose.width * 0.5,
		matchLocatePara.templatePose.y + matchLocatePara.templatePose.height * 0.5
	);

	cv::Point2d tplTargetCenter(
		matchLocatePara.targetRoi.x + matchLocatePara.targetRoi.width * 0.5,
		matchLocatePara.targetRoi.y + matchLocatePara.targetRoi.height * 0.5
	);

	// 2) 模板坐标系中的相对向量（从 featureCenter 指向 targetCenter）
	cv::Point2d deltaTpl = cv::Point2d(tplTargetCenter.x - tplFeatureCenter.x,
		tplTargetCenter.y - tplFeatureCenter.y);

	// 3) 确定 scale：优先取 Halcon 给出的 hv_Scale（如果存在），否则用 boundingRect / 模板尺寸 估算
	double scale = 1.0;
	try {
		// 如果 MatchResult.hv_Scale 是 Halcon::HTuple 且有值（你的 MatchResult 含 hv_Scale）
		if (matchLocateResult.hv_Scale.Length() > 0) {
			double s = matchLocateResult.hv_Scale[0].D();
			if (s > 0.0) scale = s;
		}
	}
	catch (...) {
		// 忽略
	}

	// 如果尚未获取到有效 scale，则用 boundingRect 与模板图尺寸估算（取宽高比的平均）
	if (scale <= 0.0) scale = 1.0;
	if (std::abs(scale - 1.0) < 1e-9) {
		bool canEstimate = false;
		double estScale = 1.0;
		if (!matchLocatePara.templateMats.empty() &&
			matchLocateResult.id >= 0 &&
			matchLocateResult.id < (int)matchLocatePara.templateMats.size()) {
			cv::Mat tpl = matchLocatePara.templateMats[matchLocateResult.id];
			double tplW = (tpl.cols > 0) ? double(tpl.cols) : 0.0;
			double tplH = (tpl.rows > 0) ? double(tpl.rows) : 0.0;
			double rectW = matchLocateResult.boundingRect.size.width;
			double rectH = matchLocateResult.boundingRect.size.height;
			if (tplW > 0 && tplH > 0 && rectW > 0 && rectH > 0) {
				double sx = rectW / tplW;
				double sy = rectH / tplH;
				estScale = (sx + sy) * 0.5;
				canEstimate = true;
			}
			else if (tplW > 0 && rectW > 0) {
				estScale = rectW / tplW;
				canEstimate = true;
			}
		}
		if (canEstimate && estScale > 0) {
			scale = estScale;
		}
		else {
			scale = 1.0; // 最后兜底
		}
	}

	// 4) 确定角度（度）：
	//    若 matchLocateResult.angle 已经填写（你的原代码用 result.angle = -hv_Angle*180/pi），优先使用它；
	//    否则尝试从 hv_Angle（弧度）转换
	double angleDeg = matchLocateResult.angle;
	if (std::isnan(angleDeg) || std::isinf(angleDeg)) angleDeg = 0.0; // 保底
	if (std::abs(angleDeg) < 1e-9) {
		try {
			if (matchLocateResult.hv_Angle.Length() > 0) {
				angleDeg = HalconRadToDegNormalized(matchLocateResult.hv_Angle[0].D());
			}
		}
		catch (...) {
			// 忽略
		}
	}
	angleDeg = NormalizeDeg(angleDeg);

	// 5) 把相对向量按 angleDeg 旋转并按 scale 缩放，然后平移到 matchLocateResult.center（该 center 表示 templateFeature 在 src 的位置）
	cv::Point2d rotated = rotate_point_ccw(deltaTpl, angleDeg);
	cv::Point2d mappedOffset(rotated.x * scale, rotated.y * scale);

	cv::Point2d matchCenter = matchLocateResult.center; // 模板特征在 src 上的映射位置（你的代码里是这样计算并存储）
	cv::Point2d targetCenterInSrc(matchCenter.x + mappedOffset.x, matchCenter.y + mappedOffset.y);

	out.targetCenter = targetCenterInSrc;
	out.usedScale = scale;
	out.usedAngleDeg = angleDeg;

	// 6) 同理把 targetRoi 的四角也映射到 src，以便获得旋转矩形
	std::vector<cv::Point2f> tplCorners(4);
	tplCorners[0] = cv::Point2f((float)matchLocatePara.targetRoi.x, (float)matchLocatePara.targetRoi.y);
	tplCorners[1] = cv::Point2f((float)(matchLocatePara.targetRoi.x + matchLocatePara.targetRoi.width), (float)matchLocatePara.targetRoi.y);
	tplCorners[2] = cv::Point2f((float)(matchLocatePara.targetRoi.x + matchLocatePara.targetRoi.width), (float)(matchLocatePara.targetRoi.y + matchLocatePara.targetRoi.height));
	tplCorners[3] = cv::Point2f((float)matchLocatePara.targetRoi.x, (float)(matchLocatePara.targetRoi.y + matchLocatePara.targetRoi.height));

	std::vector<cv::Point2f> mappedCorners(4);
	for (int i = 0; i < 4; ++i) {
		cv::Point2d off(tplCorners[i].x - tplFeatureCenter.x, tplCorners[i].y - tplFeatureCenter.y);
		cv::Point2d offRot = rotate_point_ccw(off, angleDeg);
		cv::Point2d offScaled(offRot.x * scale, offRot.y * scale);
		cv::Point2d pSrc(matchCenter.x + offScaled.x, matchCenter.y + offScaled.y);
		mappedCorners[i] = cv::Point2f((float)pSrc.x, (float)pSrc.y);
		out.corners[i] = mappedCorners[i];
	}

	// 最小外接旋转矩形
	out.targetRRect = cv::minAreaRect(mappedCorners);

	return out;
}


void InspCode::Code_Assist(InspCodeOut& outInfo) {
	DebugLogCodeAssist("pre-fix", "H1", "OIL/InspCode.cpp:Code_Assist:entry",
		outInfo.system.jobId, outInfo.system.cameraId, outInfo.status.statusCode, -999, 0, -1.0f);
	if (outInfo.status.statusCode != CODE_RETURN_OK) {
		LOG->WriteLog("跳过辅助定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}

	if (!m_params.isAssist) {
		LOG->WriteLog("未开启辅助定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		LOG->WriteLog("开始辅助定位!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	try {
		MatchLocateConfig& matchCfg = m_params.assistPara;
		if (matchCfg.matchType == 0 || matchCfg.matchType == 1) {
			auto t0 = std::chrono::high_resolution_clock::now();

			m_params.assistPara.timeOut = 3000;
			int rv = MF->MatchLocateHalcon(m_img, matchCfg, outInfo.geometry.matchLocateResult);
			auto t1 = std::chrono::high_resolution_clock::now();
			int elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
			DebugLogCodeAssist("pre-fix", "H2", "OIL/InspCode.cpp:Code_Assist:afterMatchLocateHalcon",
				outInfo.system.jobId, outInfo.system.cameraId, outInfo.status.statusCode,
				outInfo.geometry.matchLocateResult.valid, elapsedMs, outInfo.geometry.matchLocateResult.score);

			ApplyCodeAssistMatchStatus(outInfo, rv, LOG.get());
			if (outInfo.status.statusCode != CODE_RETURN_OK) {
				return;
			}
		}

		LOG->WriteLog("辅助定位完成!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	catch (const std::exception& e) {
		outInfo.status.statusCode = CODE_RETURN_MATCH_ERR0;
		LOG->WriteLog("辅助定位异常: " + std::string(e.what()),
			ERR, outInfo.paths.logFile, true);
	}

	if (outInfo.status.statusCode == CODE_RETURN_OK)
	{
		DebugLogCodeAssist("pre-fix", "H3", "OIL/InspCode.cpp:Code_Assist:beforeRotate",
			outInfo.system.jobId, outInfo.system.cameraId, outInfo.status.statusCode,
			outInfo.geometry.matchLocateResult.valid, 0, outInfo.geometry.matchLocateResult.score);
		ANA->RotateImg(m_img, m_img, outInfo.geometry.matchLocateResult.center, outInfo.geometry.matchLocateResult.angle);
		MapTargetRoiByRelativePosition(m_params.assistPara, outInfo.geometry.matchLocateResult);
		outInfo.geometry.transformedRoi.center = outInfo.geometry.matchLocateResult.center;
		outInfo.geometry.transformedRoi.center.x -= (m_params.assistPara.templatePose.x + m_params.assistPara.templatePose.width / 2 - m_params.assistPara.targetRoi.x - m_params.assistPara.targetRoi.width / 2);
		outInfo.geometry.transformedRoi.center.y -= (m_params.assistPara.templatePose.y + m_params.assistPara.templatePose.height / 2 - m_params.assistPara.targetRoi.y - m_params.assistPara.targetRoi.height / 2);

		outInfo.geometry.codeRect.x = outInfo.geometry.transformedRoi.center.x - m_params.assistPara.targetRoi.width / 2;
		outInfo.geometry.codeRect.y = outInfo.geometry.transformedRoi.center.y - m_params.assistPara.targetRoi.height / 2;
		outInfo.geometry.codeRect.width = m_params.assistPara.targetRoi.width;
		outInfo.geometry.codeRect.height = m_params.assistPara.targetRoi.height;
		DebugLogCodeAssist("pre-fix", "H4", "OIL/InspCode.cpp:Code_Assist:exit",
			outInfo.system.jobId, outInfo.system.cameraId, outInfo.status.statusCode,
			outInfo.geometry.matchLocateResult.valid, outInfo.geometry.codeRect.width * outInfo.geometry.codeRect.height,
			outInfo.geometry.matchLocateResult.score);
	}


}

void InspCode::Code_CheckYW(InspCodeOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != CODE_RETURN_OK) {
		Log::WriteAsyncLog("跳过喷码有无检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else if (!m_params.isYW)
	{
		Log::WriteAsyncLog("喷码有无检测未启用!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始喷码有无检测!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (m_params.ywModel.find(".onnx") != std::string::npos)
	{
		outInfo.locate.ywDetails = InferenceWorker::Run(outInfo.system.cameraId, m_params.ywModel, m_params.ywClassName, outInfo.images.roi.mat(), 0.1, 0.3);
	}
	else
	{
		outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}

	if (m_params.saveDebugImage)
	{
		outInfo.images.codeRegionDetectLog.data = std::make_shared<cv::Mat>(outInfo.images.roi.mat().clone());
		outInfo.images.codeRegionDetectLog.stageName = "Code_LocateCode";
		outInfo.images.codeRegionDetectLog.description = "Locate绘制: " + std::to_string(m_params.saveDebugImage);
		outInfo.images.codeRegionDetectLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
		DAS->DAS_FinsObject(outInfo.images.codeRegionDetectLog.mat(), outInfo.locate.ywDetails, outInfo.paths.intermediateImagesDir + "2.1.1.ywDetails.jpg", m_params.saveDebugImage);
	}

	for (int i = 0; i < outInfo.locate.ywDetails.size(); i++)
	{
		outInfo.locate.ywDetails[i].box = ANA->AdjustROI(outInfo.locate.ywDetails[i].box, outInfo.images.roi.mat());
		outInfo.locate.ywDetails[i].box.x += m_params.basicInfo.roi.x;
		outInfo.locate.ywDetails[i].box.y += m_params.basicInfo.roi.y;
	}

	Log::WriteAsyncLog("开始分析定位结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	for (int i = outInfo.locate.ywDetails.size() - 1; i >= 0; --i)
	{
		auto& locate = outInfo.locate.ywDetails[i];
		int paramIndex = -1; // 根据缺陷类别设置对应参数索引

		bool valid = true;
		if (locate.className == "NG")paramIndex = 1;
		else if (locate.className == "OK")	paramIndex = 0;

		if (paramIndex != -1)
		{
			auto& para = m_params.ywPara[paramIndex];
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
			outInfo.locate.ywDetails.erase(outInfo.locate.ywDetails.begin() + i);

		}
	}

	bool findNG = false;

	for (int i = 0; i < outInfo.locate.ywDetails.size(); i++)
	{

		if (outInfo.locate.ywDetails[i].className == "NG")
		{
			findNG = true;
		}
	}

	if (findNG)
	{
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			COM->CreateDir(outInfo.paths.trainDir + "YW/NG");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.ywDetails,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "YW/NG/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".json");
			cv::imwrite(outInfo.paths.trainDir + "YW/NG/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".jpg", m_img);
		}

		outInfo.status.errorMessage = "定位-缺陷喷码!";
		Log::WriteAsyncLog("定位-缺陷喷码!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = CODE_RETURN_YW_BAD_CODE;
		return;
	}

	if (outInfo.locate.ywDetails.empty()) {

		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			COM->CreateDir(outInfo.paths.trainDir + "YW/NONE");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.ywDetails,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "YW/NONE/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".json");
			cv::imwrite(outInfo.paths.trainDir + "YW/NONE/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".jpg", m_img);
		}
		outInfo.status.errorMessage = "定位-无喷码!";
		Log::WriteAsyncLog("定位-无喷码!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = CODE_RETURN_YW_NO_CODE;
		return;
	}
	else if (outInfo.locate.ywDetails.size() > 1)
	{
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			COM->CreateDir(outInfo.paths.trainDir + "YW/MULTY");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.ywDetails,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "YW/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".json");
			cv::imwrite(outInfo.paths.trainDir + "YW/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".jpg", m_img);
		}
		outInfo.status.errorMessage = "多个喷码!";
		Log::WriteAsyncLog("多个喷码!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = CODE_RETURN_YW_MULTY_CODE;
		return;
	}
	else
	{
		if (outInfo.locate.ywDetails[0].box.width > m_params.basicInfo.codeWidthRange[1])
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
			{
				COM->CreateDir(outInfo.paths.trainDir + "YW/LONG");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.ywDetails,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LONG" + ".jpg",
					m_img.rows,
					m_img.cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "YW/LONG/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LONG" + ".json");
				cv::imwrite(outInfo.paths.trainDir + "YW/LONG/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LONG" + ".jpg", m_img);
			}
			outInfo.status.errorMessage = "喷码过长!";
			Log::WriteAsyncLog("喷码过长!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = CODE_RETURN_YW_LONG;
			return;
		}
		else if (outInfo.locate.ywDetails[0].box.width < m_params.basicInfo.codeWidthRange[0])
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
			{
				COM->CreateDir(outInfo.paths.trainDir + "YW/SHORT");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.ywDetails,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_SHORT" + ".jpg",
					m_img.rows,
					m_img.cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "YW/SHORT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_SHORT" + ".json");
				cv::imwrite(outInfo.paths.trainDir + "YW/SHORT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_SHORT" + ".jpg", m_img);
			}
			outInfo.status.errorMessage = "喷码过短!";
			Log::WriteAsyncLog("喷码过短!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = CODE_RETURN_YW_SHORT;
			return;
		}
		else if (outInfo.locate.ywDetails[0].box.height > m_params.basicInfo.codeHeightRange[1])
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
			{
				COM->CreateDir(outInfo.paths.trainDir + "YW/HIGH");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.ywDetails,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_HIGH" + ".jpg",
					m_img.rows,
					m_img.cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "YW/HIGH/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_HIGH" + ".json");
				cv::imwrite(outInfo.paths.trainDir + "YW/HIGH/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_HIGH" + ".jpg", m_img);
			}
			outInfo.status.errorMessage = "喷码过高!";
			Log::WriteAsyncLog("喷码过高!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = CODE_RETURN_YW_HIGH;
			return;
		}
		else if (outInfo.locate.ywDetails[0].box.height < m_params.basicInfo.codeHeightRange[0])
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
			{
				COM->CreateDir(outInfo.paths.trainDir + "YW/LOW");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.ywDetails,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOW" + ".jpg",
					m_img.rows,
					m_img.cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "YW/LOW/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOW" + ".json");
				cv::imwrite(outInfo.paths.trainDir + "YW/LOW/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOW" + ".jpg", m_img);
			}

			outInfo.status.errorMessage = "喷码过矮!";
			Log::WriteAsyncLog("喷码过矮!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = CODE_RETURN_YW_LOW;
			return;
		}
		else
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
			{
				COM->CreateDir(outInfo.paths.trainDir + "YW/OK");
				auto jsonData = generateXAnyLabelingJSON(
					outInfo.locate.ywDetails,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".jpg",
					m_img.rows,
					m_img.cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "YW/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".json");
				cv::imwrite(outInfo.paths.trainDir + "YW/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_YW" + ".jpg", m_img);
			}
			outInfo.geometry.codeRect = outInfo.locate.ywDetails[0].box;
			Log::WriteAsyncLog("定位喷码成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
		}

	}


	outInfo.geometry.codeRect.x -= m_params.basicInfo.extW;
	outInfo.geometry.codeRect.y -= m_params.basicInfo.extH;
	outInfo.geometry.codeRect.width += m_params.basicInfo.extW * 2;
	outInfo.geometry.codeRect.height += m_params.basicInfo.extH * 2;

	outInfo.geometry.codeRect = ANA->AdjustROI(outInfo.geometry.codeRect, m_img);



}


void InspCode::Code_LocateCode(InspCodeOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != CODE_RETURN_OK) {
		Log::WriteAsyncLog("跳过字符定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else if (!m_params.isLocate)
	{
		Log::WriteAsyncLog("字符定位模型未启用!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始字符定位!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	/*if (m_params.isYW)
	{
		m_params.checkArea = outInfo.geometry.codeRect;
	}
	else
	{
		m_params.checkArea = m_params.basicInfo.roi;
	}*/

	/*cv::namedWindow("m_imgLocate", cv::WINDOW_NORMAL);
	cv::imshow("m_imgLocate", m_imgLocate);
	cv::waitKey(0);*/
	if (m_params.locateModel.find(".onnx") != std::string::npos)
	{
		outInfo.locate.locateDetails = InferenceWorker::Run(outInfo.system.cameraId, m_params.locateModel, m_params.locateClassName, m_imgLocate, m_params.locatePara[0].confidenceThresh, m_params.locatePara[0].maxOverLap);
	}
	else
	{
		outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}

	if (m_params.saveDebugImage)
	{
		outInfo.images.codeRegionDetectLog.data = std::make_shared<cv::Mat>(outInfo.images.roi.mat().clone());
		outInfo.images.codeRegionDetectLog.stageName = "Code_LocateCode";
		outInfo.images.codeRegionDetectLog.description = "Locate绘制: " + std::to_string(m_params.saveDebugImage);
		outInfo.images.codeRegionDetectLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
		DAS->DAS_FinsObject(outInfo.images.codeRegionDetectLog.mat(), outInfo.locate.locateDetails, outInfo.paths.intermediateImagesDir + "2.1.1.locateDetails.jpg", m_params.saveDebugImage);
	}

	Log::WriteAsyncLog("开始分析定位结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	for (int i = outInfo.locate.locateDetails.size() - 1; i >= 0; --i)
	{
		auto& locate = outInfo.locate.locateDetails[i];
		int paramIndex = -1; // 根据缺陷类别设置对应参数索引

		bool valid = true;
		if (locate.className == "OK")	paramIndex = 0;

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
			outInfo.locate.badLocateDetails.push_back(outInfo.locate.locateDetails[i]);
			outInfo.locate.locateDetails.erase(outInfo.locate.locateDetails.begin() + i);
		}
	}

	if (outInfo.locate.locateDetails.empty())
	{
		outInfo.status.statusCode = CODE_RETURN_LOCATE_NO_CODE;
		if (m_params.isYW)
		{
			outInfo.status.errorMessage = "字符定位失败，请更换检测模型!";
			Log::WriteAsyncLog("字符定位失败，请更换检测模型!", ERR, outInfo.paths.logFile, true);
		}
		else
		{
			outInfo.status.errorMessage = "无喷码!";
			Log::WriteAsyncLog("无喷码，如果喷码清晰，请更换检测模型!", ERR, outInfo.paths.logFile, true);
		}
	}
}


void InspCode::Code_ClassfyCode(InspCodeOut& outInfo)
{
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != CODE_RETURN_OK) {
		Log::WriteAsyncLog("跳过喷码字符识别!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	if (!m_params.isClassfy)
	{
		Log::WriteAsyncLog("喷码字符识别未启用!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始喷码字符识别!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (m_params.classfyModel.find(".onnx") == std::string::npos) {
		outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);
		return;
	}

	size_t numDetails = outInfo.locate.locateDetails.size();
	if (numDetails == 0) {
		return;
	}

	std::vector<cv::Mat> classifyInputs;
	classifyInputs.reserve(numDetails);
	for (size_t i = 0; i < numDetails; ++i) {
		cv::Rect curRect;
		ANA->ChangeRectBnd(outInfo.locate.locateDetails[i].box, m_params.inputInfo.extW, m_params.inputInfo.extH, curRect);
		curRect = ANA->AdjustROI(curRect, m_imgLocate);
		if (curRect.width <= 0 || curRect.height <= 0) {
			classifyInputs.emplace_back();
			continue;
		}
		classifyInputs.push_back(m_imgLocate(curRect).clone());
	}

	if (CheckTimeout(m_params.timeOut)) return;
	std::vector<FinsClassification> classResults(numDetails, { "", 0.0f });
	const size_t chunkSize = 32;
	for (size_t chunkStart = 0; chunkStart < classifyInputs.size(); chunkStart += chunkSize) {
		const size_t chunkEnd = std::min(chunkStart + chunkSize, classifyInputs.size());
		std::vector<cv::Mat> chunkInputs(
			classifyInputs.begin() + chunkStart,
			classifyInputs.begin() + chunkEnd
		);

		std::vector<FinsClassification> chunkResults = InferenceWorker::RunClassificationBatch(
			outInfo.system.cameraId,
			m_params.classfyModel,
			m_params.classfyClassName,
			chunkInputs
		);

		for (size_t i = 0; i < chunkResults.size(); ++i) {
			classResults[chunkStart + i] = chunkResults[i];
		}

		if (CheckTimeout(m_params.timeOut)) {
			outInfo.status.statusCode = CODE_RETURN_TIMEOUT;
			outInfo.status.errorMessage = "字符识别超时!";
			return;
		}
	}

	for (size_t i = 0; i < classResults.size() && i < outInfo.locate.locateDetails.size(); ++i) {
		outInfo.locate.locateDetails[i].className = classResults[i].className;
	}
}

void InspCode::Code_AnalysisCodePos(InspCodeOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != CODE_RETURN_OK) {
		Log::WriteAsyncLog("跳过喷码字符排序!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	if (!m_params.isLocate)
	{
		Log::WriteAsyncLog("喷码字符排序未启用,跳过排序!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始喷码字符排序!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	int min_h = 1000;
	for (int i = 0; i < outInfo.locate.locateDetails.size(); i++)
	{
		min_h = MIN(outInfo.locate.locateDetails[i].box.height, min_h);
	}
	min_h *= 0.75;//避免行间距为0的情况
	min_h = MAX(10, min_h);
	min_h += m_params.basicInfo.lineDis;//避免字符曲率过大的情况

	//从上往下，循环找到N行喷码
	vector<FinsObject> nexLineRects = outInfo.locate.locateDetails;


	int cnt = 0;  //循环次数限制
	while (nexLineRects.size() != 0) {
		cnt++;
		if (cnt > 100) {
			break;
		}
		vector<FinsObject> lineRects;                       //单行喷码
		vector<FinsObject> leftRects = nexLineRects;        //剩余喷码

		//喷码位置排序
		ANA->RankFinsObjectByX(leftRects);
		nexLineRects.clear();

		//找到最高点字符
		FinsObject minYRect;
		int id;
		ANA->FindObjectMinY(leftRects, minYRect, id);
		lineRects.push_back(minYRect);


		//找到最高点字符左边相邻字符
		FinsObject tmpLeftRect = minYRect;
		bool findScratchL = false;
		for (int i = id - 1; i >= 0; i--) {
			if (tmpLeftRect.box.x - leftRects[i].box.x - leftRects[i].box.width > m_params.basicInfo.charMaxDis)  //防止字符附近干扰
			{
				findScratchL = true;
			}
			if (abs(leftRects[i].box.y - tmpLeftRect.box.y) < min_h &&
				abs(leftRects[i].box.y + leftRects[i].box.height * 0.5 - tmpLeftRect.box.y - tmpLeftRect.box.height * 0.5) < min_h &&
				tmpLeftRect.box.y < leftRects[i].box.y + leftRects[i].box.height &&
				tmpLeftRect.box.y + tmpLeftRect.box.height > leftRects[i].box.y && !findScratchL)  //防止“一”字符距离波动、防止部分划痕造成的分行错误
			{
				lineRects.push_back(leftRects[i]);
				tmpLeftRect = leftRects[i];
			}
			else {
				nexLineRects.push_back(leftRects[i]);
			}
		}

		//找到最高点字符右边相邻字符
		FinsObject tmpRightRect = minYRect;
		bool findScratchR = false;
		for (int i = id + 1; i < leftRects.size(); i++) {
			if (leftRects[i].box.x - tmpRightRect.box.x - tmpRightRect.box.width > m_params.basicInfo.charMaxDis)  //防止字符附近干扰
			{
				findScratchR = true;
			}
			if (abs(leftRects[i].box.y - tmpRightRect.box.y) < min_h &&
				abs(leftRects[i].box.y + leftRects[i].box.height * 0.5 - tmpRightRect.box.y - tmpRightRect.box.height * 0.5) < min_h &&
				tmpRightRect.box.y < leftRects[i].box.y + leftRects[i].box.height &&
				tmpRightRect.box.y + tmpRightRect.box.height > leftRects[i].box.y && !findScratchR)  //防止“一”字符距离波动、防止部分划痕造成的分行错误
			{
				lineRects.push_back(leftRects[i]);

				tmpRightRect = leftRects[i];
			}
			else {
				nexLineRects.push_back(leftRects[i]);
			}
		}

		ANA->RankFinsObjectByX(lineRects);

		//单行数量大于N的为一行喷码，否则判定为干扰
		if (lineRects.size() >= m_params.basicInfo.lineWordMinNum) {
			outInfo.locate.lineDetails.push_back(lineRects);
		}
		else
		{
			outInfo.locate.badDetails.push_back(lineRects);
		}

		lineRects.clear();
		leftRects.clear();
	}

	nexLineRects.clear();

	for (int i = 0; i < outInfo.locate.lineDetails.size(); i++)
	{
		string resultLine;
		for (int j = 0; j < outInfo.locate.lineDetails[i].size(); j++)
		{
			/*outInfo.locate.lineDetails[i][j].box.x += outInfo.geometry.codeRect.x;
			outInfo.locate.lineDetails[i][j].box.y += outInfo.geometry.codeRect.y;*/
			resultLine += outInfo.locate.lineDetails[i][j].className;
		}
		// 存储行号和对应的文本
		outInfo.locate.codeInfo.push_back({ i + 1, resultLine });
	}

	for (int i = 0; i < outInfo.locate.badDetails.size(); i++)
	{
		for (int j = 0; j < outInfo.locate.badDetails[i].size(); j++)
		{
			outInfo.locate.badDetails[i][j].box.x += outInfo.geometry.codeRect.x;
			outInfo.locate.badDetails[i][j].box.y += outInfo.geometry.codeRect.y;
		}
	}



}


void InspCode::Code_CheckCode(InspCodeOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != CODE_RETURN_OK) {
		return;
	}

	if (m_params.isLocate)
	{
		if (outInfo.locate.locateDetails.size() < m_params.basicInfo.charNumRange[0]) {

			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/LESS");
				auto jsonData = generateXAnyLabelingJSONMulty(
					outInfo.locate.lineDetails,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".jpg",
					m_img.rows,
					m_img.cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/LESS/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/LESS/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".jpg", m_img);
			}
			outInfo.status.errorMessage = "定位-喷码字符数量过少!";
			Log::WriteAsyncLog("定位-喷码字符数量过少!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = CODE_RETURN_LOCATE_LESS;
		}
		else if (outInfo.locate.locateDetails.size() > m_params.basicInfo.charNumRange[1])
		{
			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
			{
				COM->CreateDir(outInfo.paths.trainDir + "LOCATE/MORE");
				auto jsonData = generateXAnyLabelingJSONMulty(
					outInfo.locate.lineDetails,
					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".jpg",
					m_img.rows,
					m_img.cols
				);
				saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/MORE/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".json");
				cv::imwrite(outInfo.paths.trainDir + "LOCATE/MORE/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".jpg", m_img);
			}
			outInfo.status.errorMessage = "喷码字符数量超出!";
			Log::WriteAsyncLog("喷码字符数量超出!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = CODE_RETURN_LOCATE_MORE;
		}

	}

	if (m_params.isLocate && m_params.isClassfy)
	{
		if (m_params.inputInfo.checkType)
		{
			Code_ValidateAllOCRResults(outInfo);
		}
		else
		{
			Code_ValidateSimpleOCRResults(outInfo);
		}

		if (m_params.inputInfo.infoRepeat > 0)
		{
			Code_CheckRepeat(outInfo);
		}
	}

}

void InspCode::Code_SaveLocate(InspCodeOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if(!m_params.isLocate)
	{
		return;
	}
	if (outInfo.status.statusCode == CODE_RETURN_LOCATE_LESS) {

		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/LESS");
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/LESS_LOG");
			auto jsonData = generateXAnyLabelingJSONMulty(
				outInfo.locate.lineDetails,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/LESS/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/LESS_LOG/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE_LOG" + ".jpg", outInfo.images.outputImg);
		}
	}
	else if (outInfo.status.statusCode == CODE_RETURN_LOCATE_MORE)
	{
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/MORE");
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/MORE_LOG");
			auto jsonData = generateXAnyLabelingJSONMulty(
				outInfo.locate.lineDetails,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/MORE/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/MORE_LOG/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE_LOG" + ".jpg", outInfo.images.outputImg);
		}
	}
	else if (outInfo.status.statusCode == CODE_RETURN_CLASSFY_INFO_ERR)
	{
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/ERROR");
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/ERROR_LOG");
			auto jsonData = generateXAnyLabelingJSONMulty(
				outInfo.locate.lineDetails,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/ERROR/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/ERROR/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".jpg", m_img);
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/ERROR_LOG/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE_LOG" + ".jpg", outInfo.images.outputImg);
		}
		Log::WriteAsyncLog("定位喷码成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	else if (outInfo.status.statusCode == CODE_RETURN_OK)
	{
		if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
		{
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/OK");
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/OK_LOG");
			auto jsonData = generateXAnyLabelingJSONMulty(
				outInfo.locate.lineDetails,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE" + ".jpg", m_img);
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/OK_LOG/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + "_LOCATE_LOG" + ".jpg", outInfo.images.outputImg);
		}
		Log::WriteAsyncLog("定位喷码成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

}

std::string normalizeDateString(const std::string& s) {
	std::string result;
	for (char ch : s) {
		if (isdigit(static_cast<unsigned char>(ch))) {
			result.push_back(ch);
		}
	}
	return result;
}

static inline bool isLeap(int y) {
	return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static inline int daysInMonth(int y, int m) {
	static const int mdays[13] = { 0,31,28,31,30,31,30,31,31,30,31,30,31 };
	if (m == 2) return isLeap(y) ? 29 : 28;
	return mdays[m];
}

// 规范化 (Y, M) 到 1..12，处理负/正进位
static inline void normalizeYearMonth(int& y, int& m) {
	// 保证 m 最终在 [1, 12]
	int adj = (m - 1) / 12;       // 向零取整
	if (m <= 0) adj = (m - 12) / 12; // 负数月份时做“地板除”
	y += adj;
	m -= adj * 12;
	if (m <= 0) { y -= 1; m += 12; }
}
// 将 src 按年、月、日依次相加；若目标月没有对应的“日”，则压到该月最后一天
Date addYearsMonthsDays(const Date& src, int addYears, int addMonths, int addDays) {
	int Y = src.year + addYears;
	int M = src.month + addMonths;
	normalizeYearMonth(Y, M);

	// 压日：如 3/31 + 1 个月 => 4/30
	int D = std::min(src.day, daysInMonth(Y, M));

	Date tmp;
	tmp.year = Y;
	tmp.month = M;
	tmp.day = D;
	tmp.hour = src.hour;
	tmp.minute = src.minute;
	tmp.second = src.second;
	tmp.valid = true;

	// “加日”可以安全复用你已有的 addDays（它应已处理跨月/闰年）
	return tmp.addDays(addDays);
}

// 返回逻辑字符数（将 GBK/ANSI 多字节转 wchar 后的长度）
int getCharCountFromGBK(const std::string& text) {
	if (text.empty()) return 0;
	int len = MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), NULL, 0);
	if (len <= 0) return 0;
	return len;
}

// string (GBK/ANSI) -> wstring
std::wstring toWStringFromGBK(const std::string& text) {
	if (text.empty()) return L"";
	int len = MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), NULL, 0);
	if (len <= 0) return L"";
	std::wstring w;
	w.resize(len);
	MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), &w[0], len);
	return w;
}

// wstring -> string (GBK/ANSI)
std::string fromWStringToGBK(const std::wstring& wtext) {
	if (wtext.empty()) return std::string();
	int len = WideCharToMultiByte(CP_ACP, 0, wtext.c_str(), (int)wtext.size(), NULL, 0, NULL, NULL);
	if (len <= 0) return std::string();
	std::string s;
	s.resize(len);
	WideCharToMultiByte(CP_ACP, 0, wtext.c_str(), (int)wtext.size(), &s[0], len, NULL, NULL);
	return s;
}


// wstring 按“字符数”截取（start:字符索引，count:字符个数）
std::wstring wstring_substrByCount(const std::wstring& src, size_t start, size_t count) {
	if (start >= src.size()) return L"";
	return src.substr(start, count);
}

// 计算 target.info 的“期望字符数”（统一以 wchar 个数计）
// 这样无论 target.info 包含中文、ASCII 分隔符还是 YYYY/M/D 占位，都按字符数一致处理
static size_t getExpectedLength(const std::string& pattern) {
	std::wstring wpat = toWStringFromGBK(pattern);
	return wpat.size();
}


void InspCode::Code_CheckRepeat(InspCodeOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	if (outInfo.status.statusCode != CODE_RETURN_OK) {
		Log::WriteAsyncLog("跳过喷码内容重复检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else if (m_params.inputInfo.infoRepeat == 0)
	{
		Log::WriteAsyncLog("喷码内容重复检测未开启!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("开始喷码内容重复检测!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	map<int, string> rowTexts;
	for (const auto& codeInfo : outInfo.locate.codeInfo) {
		rowTexts[codeInfo.first] = codeInfo.second;
	}

	std::string allContent;
	for (const auto& rowEntry : rowTexts) {
		allContent += rowEntry.second; // 直接拼接所有行内容
	}



	// 3. 检查喷码内容是否重复（与前N个比较）
	{
		std::lock_guard<std::mutex> lock(staticMutex);
		auto& recentCodes = recentCodeContents[outInfo.system.cameraId];

		// 检查当前内容是否在最近N个中出现过
		if (std::find(recentCodes.begin(), recentCodes.end(), allContent) != recentCodes.end()) {

			outInfo.status.statusCode = CODE_RETURN_CLASSFY_INFO_REPEAT;
			outInfo.status.errorMessage = "字符识别-内容与之前产品重复!";
			Log::WriteAsyncLog("字符识别-内容与之前产品重复!", ERR, outInfo.paths.logFile, true);
			return;
		}

		// 将当前内容添加到最近记录中
		recentCodes.push_back(allContent);
		// 保持队列大小不超过N
		if (recentCodes.size() > m_params.inputInfo.infoRepeat) {
			recentCodes.pop_front();
		}
	}

}

enum class DatePrecision { Year, Month, Day, Hour, Minute, Second };

//std::regex(R"(^(\d{4})$)"),                                         // YYYY
//std::regex(R"(^(\d{4})(\d{2})$)"),                                  // YYYYMM
//std::regex(R"(^(\d{4})(\d{2})(\d{2})$)"),                           // YYYYMMDD
//std::regex(R"(^(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})$)"),             // YYYYMMDDHHMM
//std::regex(R"(^(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})$)"),      // YYYYMMDDHHMMSS
//std::regex(R"(^(\d{4})[.-](\d{2})[.-](\d{2})$)"),                   // YYYY.MM.DD 或 YYYY-MM-DD
//std::regex(R"(^(\d{4})年(\d{1,2})月(\d{1,2})日$)"),                  // YYYY年MM月DD日
//std::regex(R"(^(\d{4})/(\d{2})$)"),                                 // YYYY/MM
//std::regex(R"(^(\d{4})/(\d{2})/(\d{2})$)"),                         // YYYY/MM/DD
//std::regex(R"(^(\d{4})/(\d{2})/(\d{2})(\d{2}):(\d{2})$)"),         // YYYY/MM/DDHH:MM
//std::regex(R"(^(\d{4})/(\d{2})/(\d{2})(\d{2}):(\d{2}):(\d{2})$)"), // YYYY/MM/DDHH:MM:SS
//std::regex(R"(^(\d{4})-(\d{2})-(\d{2})(\d{2}):(\d{2}):(\d{2})$)")  // YYYY-MM-DDHH:MM:SS

DatePrecision getDatePrecision(const std::string& dateFormat) {
	if (dateFormat == "YYYY") return DatePrecision::Year;
	if (dateFormat == "YYYYMM") return DatePrecision::Month;
	if (dateFormat == "YYYY/MM") return DatePrecision::Month;
	if (dateFormat == "YYYY.MM") return DatePrecision::Month;
	if (dateFormat == "YYYY年MM月") return DatePrecision::Month;
	if (dateFormat == "YYYYMMDD") return DatePrecision::Day;
	if (dateFormat == "YYYY/MM/DD") return DatePrecision::Day;
	if (dateFormat == "YYYY.MM.DD") return DatePrecision::Day;
	if (dateFormat == "YYYY年MM月DD日") return DatePrecision::Day;
	if (dateFormat == "YYYYMMDDHH") return DatePrecision::Hour;
	if (dateFormat == "YYYY/MM/DDHH") return DatePrecision::Hour;
	if (dateFormat == "YYYY年MM月DDHH") return DatePrecision::Hour;
	if (dateFormat == "YYYY.MM.DDHH") return DatePrecision::Hour;
	if (dateFormat == "YYYYMMDDHHMM") return DatePrecision::Minute;
	if (dateFormat == "YYYYMMDDHHMMSS") return DatePrecision::Second;
	if (dateFormat == "YYYY/MM/DDHH:MM") return DatePrecision::Minute;
	if (dateFormat == "YYYY/MM/DDHH:MM:SS") return DatePrecision::Second;
	if (dateFormat == "YYYY-MM-DDHH:MM:SS") return DatePrecision::Second;

	// 默认返回天精度
	return DatePrecision::Day;
}

std::string formatDateByPrecision(const Date& date, DatePrecision precision) {
	switch (precision) {
	case DatePrecision::Year:
		return std::to_string(date.year);
	case DatePrecision::Month:
		return std::to_string(date.year) + (date.month < 10 ? "0" : "") + std::to_string(date.month);
	case DatePrecision::Day:
		return std::to_string(date.year) +
			(date.month < 10 ? "0" : "") + std::to_string(date.month) +
			(date.day < 10 ? "0" : "") + std::to_string(date.day);
	case DatePrecision::Minute:
		return std::to_string(date.year) +
			(date.month < 10 ? "0" : "") + std::to_string(date.month) +
			(date.day < 10 ? "0" : "") + std::to_string(date.day) +
			(date.hour < 10 ? "0" : "") + std::to_string(date.hour) +
			(date.minute < 10 ? "0" : "") + std::to_string(date.minute);
	case DatePrecision::Second:
		return std::to_string(date.year) +
			(date.month < 10 ? "0" : "") + std::to_string(date.month) +
			(date.day < 10 ? "0" : "") + std::to_string(date.day) +
			(date.hour < 10 ? "0" : "") + std::to_string(date.hour) +
			(date.minute < 10 ? "0" : "") + std::to_string(date.minute) +
			(date.second < 10 ? "0" : "") + std::to_string(date.second);
	}
	return "";
}


// 获取宽松日期正则，支持 YYYY, YYYYMM, YYYY/MM, YYYYMMDD, YYYY/MM/DD 等
#include <regex>
#include <string>

std::regex getLooseDatePattern(const std::string& dateFormat) {
	// 年
	if (dateFormat == "YYYY") return std::regex(R"(\d{4})");

	// 年月
	if (dateFormat == "YYYYMM") return std::regex(R"(\d{6})");

	// 年月日
	if (dateFormat == "YYYYMMDD") return std::regex(R"(\d{8})");

	// 年月日时
	if (dateFormat == "YYYYMMDDHH") return std::regex(R"(\d{10})");

	// 年月日时分
	if (dateFormat == "YYYYMMDDHHMM") return std::regex(R"(\d{12})");

	// 年月日时分秒
	if (dateFormat == "YYYYMMDDHHMMSS") return std::regex(R"(\d{14})");

	// 带分隔符的年月
	if (dateFormat == "YYYY/MM") return std::regex(R"(\d{4}/\d{2})");

	// 带分隔符的年月日
	if (dateFormat == "YYYY/MM/DD") return std::regex(R"(\d{4}/\d{2}/\d{2})");

	// 带分隔符的年月日时
	if (dateFormat == "YYYY/MM/DDHH") return std::regex(R"(\d{4}/\d{2}/\d{2}\d{2})");

	// 带分隔符的年月日时分
	if (dateFormat == "YYYY/MM/DDHHMM") return std::regex(R"(\d{4}/\d{2}/\d{2}\d{4})");

	// 带分隔符的年月日时分秒
	if (dateFormat == "YYYY/MM/DDHHMMSS") return std::regex(R"(\d{4}/\d{2}/\d{2}\d{6})");

	// 点分隔年月日
	if (dateFormat == "YYYY.MM.DD") return std::regex(R"(\d{4}[.]\d{2}[.]\d{2})");

	// 横线分隔年月日
	if (dateFormat == "YYYY-MM-DD") return std::regex(R"(\d{4}-\d{2}-\d{2})");

	// 中文年月日
	if (dateFormat == "YYYY年MM月DD日") return std::regex(R"(\d{4}年\d{1,2}月\d{1,2}日)");

	// 带时间的分隔符格式
	if (dateFormat == "YYYY/MM/DDHH:MM") return std::regex(R"(\d{4}/\d{2}/\d{2}\d{2}:\d{2})");
	if (dateFormat == "YYYY/MM/DDHH:MM:SS") return std::regex(R"(\d{4}/\d{2}/\d{2}\d{2}:\d{2}:\d{2})");
	if (dateFormat == "YYYY-MM-DDHH:MM:SS") return std::regex(R"(\d{4}-\d{2}-\d{2}\d{2}:\d{2}:\d{2})");

	// 默认宽松匹配：支持 YYYYMMDD 或 YYYY-MM-DD / YYYY/MM/DD / YYYY.MM.DD 等
	return std::regex(R"(\d{4}[-./]?\d{2}[-./]?\d{2})");
}


// 格式化日期为目标格式字符串
std::string formatDateByPattern(const Date& dt, const std::string& pattern) {
	if (!dt) return "";

	auto pad2 = [](int v) { return (v < 10 ? "0" : "") + std::to_string(v); };

	if (pattern == "YYYY") return std::to_string(dt.year);
	if (pattern == "YYYYMM") return std::to_string(dt.year) + pad2(dt.month);
	if (pattern == "YYYY/MM") return std::to_string(dt.year) + "/" + pad2(dt.month);
	if (pattern == "YYYYMMDD") return std::to_string(dt.year) + pad2(dt.month) + pad2(dt.day);
	if (pattern == "YYYY/MM/DD") return std::to_string(dt.year) + "/" + pad2(dt.month) + "/" + pad2(dt.day);
	if (pattern == "YYYY.MM.DD") return std::to_string(dt.year) + "." + pad2(dt.month) + "." + pad2(dt.day);
	if (pattern == "YYYY年MM月DD日") return std::to_string(dt.year) + "年" + std::to_string(dt.month) + "月" + std::to_string(dt.day) + "日";
	if (pattern == "YYYYMMDDHH") return std::to_string(dt.year) + pad2(dt.month) + pad2(dt.day) + pad2(dt.hour);
	if (pattern == "YYYYMMDDHHMM") return std::to_string(dt.year) + pad2(dt.month) + pad2(dt.day) + pad2(dt.hour) + pad2(dt.minute);
	if (pattern == "YYYYMMDDHHMMSS") return std::to_string(dt.year) + pad2(dt.month) + pad2(dt.day) + pad2(dt.hour) + pad2(dt.minute) + pad2(dt.second);
	if (pattern == "YYYY/MM/DDHH:MM") return std::to_string(dt.year) + "/" + pad2(dt.month) + "/" + pad2(dt.day) + pad2(dt.hour) + ":" + pad2(dt.minute);
	if (pattern == "YYYY/MM/DDHH:MM:SS") return std::to_string(dt.year) + "/" + pad2(dt.month) + "/" + pad2(dt.day) + pad2(dt.hour) + ":" + pad2(dt.minute) + ":" + pad2(dt.second);
	if (pattern == "YYYY-MM-DDHH:MM:SS") return std::to_string(dt.year) + "-" + pad2(dt.month) + "-" + pad2(dt.day) + pad2(dt.hour) + ":" + pad2(dt.minute) + ":" + pad2(dt.second);

	return "";
}

// ------------------ Code_ValidateAllOCRResults ------------------
void InspCode::Code_ValidateAllOCRResults(InspCodeOut& outInfo) {
	bool all_ok = true;

	// 1. 按行和 part 对目标配置进行分组
	map<pair<int, int>, vector<TargetConfig>> groupedTargets;
	for (const auto& target : m_params.inputInfo.targets) {
		groupedTargets[{target.row, target.part}].push_back(target);
	}

	// 获取当前时间（线程安全）
	Date currentDate;
	time_t now = time(nullptr);
#if defined(_WIN32) || defined(_WIN64)
	tm tm_now{};
	localtime_s(&tm_now, &now);
#else
	tm tm_now{};
	localtime_r(&now, &tm_now);
#endif
	currentDate.year = tm_now.tm_year + 1900;
	currentDate.month = tm_now.tm_mon + 1;
	currentDate.day = tm_now.tm_mday;
	currentDate.hour = tm_now.tm_hour;
	currentDate.minute = tm_now.tm_min;
	currentDate.second = tm_now.tm_sec;
	currentDate.valid = true;

	// 2. 按行分组 OCR 结果
	map<int, string> rowTexts;
	for (const auto& codeInfo : outInfo.locate.codeInfo) {
		rowTexts[codeInfo.first] = codeInfo.second;
	}

	Date productionDate;
	for (const auto& rowEntry : rowTexts) {
		int row = rowEntry.first;
		const string& text = rowEntry.second;
		std::wstring wText = toWStringFromGBK(text);

		// 4.1 计算整行期望字符数（按 target.info 字符计数）
		size_t expectedLength = 0;
		for (int part = 1;; part++) {
			auto key = make_pair(row, part);
			if (groupedTargets.find(key) == groupedTargets.end()) break;
			for (const auto& target : groupedTargets[key])
				expectedLength += getExpectedLength(target.info);
		}

		DetectionResult charCountResult;
		charCountResult.expectedLength = expectedLength;
		charCountResult.row = row;
		charCountResult.part = 1;
		charCountResult.type = "整体验证";
		charCountResult.expectedInfo = std::to_string(expectedLength) + " 个字符";
		charCountResult.charNum = (int)wText.size();
		charCountResult.actualInfo = std::to_string(charCountResult.charNum) + " 个字符";

		bool rowSuccess = (expectedLength == charCountResult.charNum);
		if (!rowSuccess) { charCountResult.status = "NG"; charCountResult.message = "字符数量错误"; all_ok = false; }
		else charCountResult.status = "OK";
		outInfo.classification.codeMatchResult.push_back(charCountResult);

		// 4.2 逐个验证
		size_t pos = 0;
		for (int part = 1;; part++) {
			auto key = make_pair(row, part);
			if (groupedTargets.find(key) == groupedTargets.end()) break;
			for (const auto& target : groupedTargets[key]) {
				DetectionResult partResult;
				partResult.row = row;
				partResult.part = part;
				partResult.type = target.type;

				size_t expectedChars = getExpectedLength(target.info);
				std::wstring wActual = wstring_substrByCount(wText, pos, expectedChars);
				pos += expectedChars;

				std::string actualText = fromWStringToGBK(wActual);
				partResult.actualInfo = actualText;

				// 设置期望值
				if (target.type == "生产日期")
				{
					partResult.expectedInfo = formatDateByPattern(currentDate, target.info);
					if (partResult.expectedInfo.empty()) {
						partResult.status = "NG";
						partResult.message = "生产日期格式配置错误";
						all_ok = false;
						outInfo.classification.codeMatchResult.push_back(partResult);

						outInfo.status.errorMessage = "生产日期格式配置错误!";
						Log::WriteAsyncLog("生产日期格式配置错误!", ERR, outInfo.paths.logFile, true);
						outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;

						return;
					}
				}
				else if (target.type == "保质日期") {
					Date expectedExpiration = addYearsMonthsDays(currentDate,
						m_params.inputInfo.expirationDateYear,
						m_params.inputInfo.expirationDateMonth,
						m_params.inputInfo.expirationDateDay);
					partResult.expectedInfo = formatDateByPattern(expectedExpiration, target.info);
					if (partResult.expectedInfo.empty()) {
						partResult.status = "NG";
						partResult.message = "保质日期格式配置错误";
						all_ok = false;


						outInfo.status.errorMessage = "保质日期格式配置错误!";
						Log::WriteAsyncLog("生保质日期格式配置错误!", ERR, outInfo.paths.logFile, true);
						outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;

						return;
					}
				}
				else if (target.type == "生产流水号") partResult.expectedInfo = "递增序列";
				else partResult.expectedInfo = target.info;

				string danwei = "";
				if (rowSuccess) {
					if (target.type == "生产日期") {
						Date parsedDate = Date::fromString(actualText);
						if (!parsedDate.valid) {
							partResult.status = "NG";
							partResult.message = "无效的日期格式";
							all_ok = false;
						}
						else {
							productionDate = parsedDate;

							// 根据日期格式精度选择合适的比较方法
							DatePrecision precision = getDatePrecision(target.info);
							int diff = 0;
							bool withinTolerance = false;

							switch (precision) {
							case DatePrecision::Year:
								danwei = "年";
								// 年份比较
								diff = std::abs(parsedDate.year - currentDate.year);
								withinTolerance = (abs(diff) * 365 * 24 * 60 <= m_params.inputInfo.timeError);
								break;
							case DatePrecision::Month:
								danwei = "月";
								// 月份比较（近似处理）
								diff = std::abs((parsedDate.year - currentDate.year) * 12 +
									(parsedDate.month - currentDate.month));
								withinTolerance = (abs(diff) * 30 * 24 * 60 <= m_params.inputInfo.timeError);
								break;
							case DatePrecision::Day:
								danwei = "天";
								// 天数比较
								diff = parsedDate.daysBetween(currentDate);
								withinTolerance = (abs(diff) * 24 * 60 <= m_params.inputInfo.timeError);
								break;
							case DatePrecision::Hour:
								danwei = "小时";
								// 小时比较
								diff = parsedDate.hoursBetween(currentDate);
								withinTolerance = (abs(diff) * 60 <= m_params.inputInfo.timeError);
								break;
							case DatePrecision::Minute:
								danwei = "分钟";
								// 分钟比较
								diff = parsedDate.minutesBetween(currentDate);
								withinTolerance = (abs(diff) <= m_params.inputInfo.timeError);
								break;
							case DatePrecision::Second:
								danwei = "秒";
								// 秒比较（转换为分钟）
								diff = parsedDate.minutesBetween(currentDate);
								withinTolerance = (abs(diff) <= m_params.inputInfo.timeError);
								break;
							default:
								danwei = "分钟";
								// 默认按分钟比较
								diff = parsedDate.minutesBetween(currentDate);
								withinTolerance = (abs(diff) <= m_params.inputInfo.timeError);
								break;
							}

							if (withinTolerance) {
								partResult.status = "OK";
							}
							else {
								partResult.status = "NG";
								partResult.message = "时差: " + std::to_string(diff) + " " + danwei +
									", 超出允许允许误差 " + std::to_string(m_params.inputInfo.timeError) + " 分钟";
								all_ok = false;
							}
						}
					}
					else if (target.type == "保质日期") {
						if (!productionDate) {
							partResult.status = "NG";
							partResult.message = "缺少生产日期";
							all_ok = false;
						}
						else {
							Date expectedExpiration = addYearsMonthsDays(productionDate,
								m_params.inputInfo.expirationDateYear,
								m_params.inputInfo.expirationDateMonth,
								m_params.inputInfo.expirationDateDay);

							Date actualExpiration = Date::fromString(actualText);
							if (!actualExpiration.valid) {
								partResult.status = "NG";
								partResult.message = "无效的日期格式";
								all_ok = false;
							}
							else {
								// 根据日期格式精度选择合适的比较方法
								DatePrecision precision = getDatePrecision(target.info);
								int diff = 0;
								bool withinTolerance = false;

								switch (precision) {
								case DatePrecision::Year:
									danwei = "年";
									// 年份比较
									diff = std::abs(actualExpiration.year - expectedExpiration.year);
									withinTolerance = (diff * 365 <= m_params.inputInfo.expirationDateError);
									break;
								case DatePrecision::Month:
									danwei = "月";
									// 月份比较（近似处理）
									diff = std::abs((actualExpiration.year - expectedExpiration.year) * 12 +
										(actualExpiration.month - expectedExpiration.month));
									withinTolerance = (diff * 30 <= m_params.inputInfo.expirationDateError);
									break;
								case DatePrecision::Day:
									danwei = "天";
									// 天数比较
									diff = std::abs(actualExpiration.daysBetween(expectedExpiration));
									withinTolerance = (diff <= m_params.inputInfo.expirationDateError);
									break;
								case DatePrecision::Hour:
									danwei = "小时";
									// 小时比较（转换为天）
									diff = std::abs(actualExpiration.hoursBetween(expectedExpiration));
									withinTolerance = (diff / 24 <= m_params.inputInfo.expirationDateError);
									break;
								case DatePrecision::Minute:
									danwei = "分钟";
									// 分钟比较（转换为天）
									diff = std::abs(actualExpiration.minutesBetween(expectedExpiration));
									withinTolerance = (diff / (24 * 60) <= m_params.inputInfo.expirationDateError);
									break;
								case DatePrecision::Second:
									danwei = "秒";
									// 秒比较（转换为天）
									diff = std::abs(actualExpiration.minutesBetween(expectedExpiration));
									withinTolerance = (diff / (24 * 60) <= m_params.inputInfo.expirationDateError);
									break;
								default:
									danwei = "分钟";
									// 默认按天比较
									diff = std::abs(actualExpiration.daysBetween(expectedExpiration));
									withinTolerance = (diff <= m_params.inputInfo.expirationDateError);
									break;
								}

								if (withinTolerance) {
									partResult.status = "OK";
								}
								else {
									partResult.status = "NG";
									partResult.message = "时差: " + std::to_string(diff) + " " + danwei +
										", 超出允许允许误差 " + std::to_string(m_params.inputInfo.expirationDateError) + " 天";
									all_ok = false;
								}
							}
						}
					}
					else if (target.type == "生产流水号") {
						std::lock_guard<std::mutex> lock(staticMutex);
						string& lastSerial = lastSerialNumber[outInfo.system.cameraId];
						bool serialValid = true;
						string errorMessage;
						if (!lastSerial.empty()) {
							try { long curr = std::stol(actualText); long last = std::stol(lastSerial); if (curr <= last) { serialValid = false; errorMessage = "流水号必须大于前一个 (" + lastSerial + ")"; } }
							catch (...) { if (actualText <= lastSerial) { serialValid = false; errorMessage = "流水号必须大于前一个 (" + lastSerial + ")"; } }
						}
						if (serialValid) { partResult.status = "OK"; partResult.message = "流水号递增正常"; lastSerial = actualText; }
						else { partResult.status = "NG"; partResult.message = errorMessage; all_ok = false; }
						if (lastSerial.empty()) { partResult.status = "OK"; partResult.message = "第一个流水号"; lastSerial = actualText; }
					}
					else if (target.type == "固定字符") { if (actualText != target.info) { partResult.status = "NG"; partResult.message = "字符不匹配"; all_ok = false; } else partResult.status = "OK"; }
					else partResult.status = "跳过";
				}
				else { partResult.status = "NG"; partResult.message = "字符数量不匹配，验证结果可能不准确"; }

				outInfo.classification.codeMatchResult.push_back(partResult);
			}
		}
	}

	if (!all_ok && outInfo.status.statusCode == CODE_RETURN_OK) {
		outInfo.status.statusCode = CODE_RETURN_CLASSFY_INFO_ERR;
		outInfo.status.errorMessage = "喷码信息错误!";
		Log::WriteAsyncLog("喷码信息错误!", ERR, outInfo.paths.logFile, true);
	}
}

void InspCode::Code_ValidateSimpleOCRResults(InspCodeOut& outInfo) {
	bool all_ok = true;
	string allText;
	for (const auto& ocr : outInfo.locate.codeInfo) allText += ocr.second + " ";

	// 获取当前时间（线程安全）
	Date currentDate;
	time_t now = time(nullptr);
#if defined(_WIN32) || defined(_WIN64)
	tm tm_now{};
	localtime_s(&tm_now, &now);
#else
	tm tm_now{};
	localtime_r(&now, &tm_now);
#endif
	currentDate.year = tm_now.tm_year + 1900;
	currentDate.month = tm_now.tm_mon + 1;
	currentDate.day = tm_now.tm_mday;
	currentDate.hour = tm_now.tm_hour;
	currentDate.minute = tm_now.tm_min;
	currentDate.second = tm_now.tm_sec;
	currentDate.valid = true;

	Date validatedProductionDate;
	string danwei = "";
	// 首先处理生产日期
	for (const auto& target : m_params.inputInfo.targets) {
		if (target.type == "生产日期") {
			DetectionResult result;
			result.type = target.type;
			result.expectedInfo = formatDateByPattern(currentDate, target.info);
			result.row = target.row;
			result.part = target.part;

			if (result.expectedInfo.empty()) {
				result.status = "NG";
				result.message = "生产日期格式配置错误";
				all_ok = false;
				outInfo.classification.codeMatchResult.push_back(result);

				outInfo.status.errorMessage = "生产日期格式配置错误!";
				Log::WriteAsyncLog("生产日期格式配置错误!", ERR, outInfo.paths.logFile, true);
				outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;

				return;
			}

			// 使用正则表达式搜索日期
			regex datePattern = getLooseDatePattern(target.info);
			smatch match;
			if (regex_search(allText, match, datePattern)) {
				string extracted = match.str();
				result.actualInfo = extracted;
				validatedProductionDate = Date::fromString(extracted);
				if (!validatedProductionDate.valid) {
					result.status = "NG";
					result.message = "无效的生产日期格式";
					all_ok = false;
				}
				else {
					// 根据日期格式精度选择合适的比较方法
					DatePrecision precision = getDatePrecision(target.info);
					int diff = 0;
					bool withinTolerance = false;

					switch (precision) {
					case DatePrecision::Year:
						danwei = "年";
						// 年份比较
						diff = std::abs(validatedProductionDate.year - currentDate.year);
						withinTolerance = (abs(diff) * 365 * 24 * 60 <= m_params.inputInfo.timeError);
						break;
					case DatePrecision::Month:
						danwei = "月";
						// 月份比较（近似处理）
						diff = std::abs((validatedProductionDate.year - currentDate.year) * 12 +
							(validatedProductionDate.month - currentDate.month));
						withinTolerance = (abs(diff) * 30 * 24 * 60 <= m_params.inputInfo.timeError);
						break;
					case DatePrecision::Day:
						danwei = "天";
						// 天数比较
						diff = validatedProductionDate.daysBetween(currentDate);
						withinTolerance = (abs(diff) * 24 * 60 <= m_params.inputInfo.timeError);
						break;
					case DatePrecision::Hour:
						danwei = "小时";
						// 小时比较
						diff = validatedProductionDate.hoursBetween(currentDate);
						withinTolerance = (abs(diff) * 60 <= m_params.inputInfo.timeError);
						break;
					case DatePrecision::Minute:
						danwei = "分钟";
						// 分钟比较
						diff = validatedProductionDate.minutesBetween(currentDate);
						withinTolerance = (abs(diff) <= m_params.inputInfo.timeError);
						break;
					case DatePrecision::Second:
						danwei = "秒";
						// 秒比较（转换为分钟）
						diff = validatedProductionDate.minutesBetween(currentDate);
						withinTolerance = (abs(diff) <= m_params.inputInfo.timeError);
						break;
					default:
						danwei = "分钟";
						// 默认按分钟比较
						diff = validatedProductionDate.minutesBetween(currentDate);
						withinTolerance = (abs(diff) <= m_params.inputInfo.timeError);
						break;
					}

					if (withinTolerance) {
						result.status = "OK";
					}
					else {
						result.status = "NG";
						result.message = "时差: " + std::to_string(diff) + " " + danwei +
							", 超出允许允许误差 " + std::to_string(m_params.inputInfo.timeError) + " 分钟";
						all_ok = false;
					}
				}
			}
			else {
				result.status = "NG";
				result.message = "未找到生产日期";
				all_ok = false;
			}
			outInfo.classification.codeMatchResult.push_back(result);
		}
	}

	// 然后处理其他类型的验证
	for (const auto& target : m_params.inputInfo.targets) {
		if (target.type == "生产日期") continue;

		DetectionResult result;
		result.type = target.type;
		result.row = target.row;
		result.part = target.part;

		if (target.type == "保质日期") {
			// 计算期望的保质日期
			Date expectedExpiration = addYearsMonthsDays(
				currentDate,
				m_params.inputInfo.expirationDateYear,
				m_params.inputInfo.expirationDateMonth,
				m_params.inputInfo.expirationDateDay
			);

			result.expectedInfo = formatDateByPattern(expectedExpiration, target.info);
			if (result.expectedInfo.empty()) {
				result.status = "NG";
				result.message = "保质日期格式配置错误";
				all_ok = false;
				outInfo.classification.codeMatchResult.push_back(result);

				outInfo.status.errorMessage = "保质日期格式配置错误!";
				Log::WriteAsyncLog("生保质日期格式配置错误!", ERR, outInfo.paths.logFile, true);
				outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;

				return;
			}
			// 在全文搜索保质日期
			vector<string> allDates;
			regex datePattern = getLooseDatePattern(target.info);
			smatch dateMatch;
			string::const_iterator start = allText.begin();
			string::const_iterator end = allText.end();

			while (regex_search(start, end, dateMatch, datePattern)) {
				string extracted = dateMatch.str();
				allDates.push_back(extracted);
				start = dateMatch.suffix().first;
			}

			if (!allDates.empty()) {
				int minDiff = INT_MAX;
				string bestDateStr;
				Date bestDate;

				for (const auto& d : allDates) {
					Date candidate = Date::fromString(d);
					if (candidate.valid) {
						// 根据日期格式精度选择合适的比较方法
						DatePrecision precision = getDatePrecision(target.info);
						int diff = 0;

						switch (precision) {
						case DatePrecision::Year:
							danwei = "年";
							// 年份比较
							diff = std::abs(candidate.year - expectedExpiration.year);
							break;
						case DatePrecision::Month:
							danwei = "月";
							// 月份比较（近似处理）
							diff = std::abs((candidate.year - expectedExpiration.year) * 12 +
								(candidate.month - expectedExpiration.month));
							break;
						case DatePrecision::Day:
							danwei = "天";
							// 天数比较
							diff = candidate.daysBetween(expectedExpiration);
							break;
						case DatePrecision::Hour:
							danwei = "小时";
							// 小时比较
							diff = candidate.hoursBetween(expectedExpiration);
							break;
						case DatePrecision::Minute:
							danwei = "分钟";
							// 分钟比较
							diff = candidate.minutesBetween(expectedExpiration);
							break;
						case DatePrecision::Second:
							danwei = "秒";
							// 秒比较（转换为分钟）
							diff = candidate.minutesBetween(expectedExpiration);
							break;
						default:
							// 默认按天比较
							diff = candidate.daysBetween(expectedExpiration);
							break;
						}

						if (abs(diff) < abs(minDiff)) {
							minDiff = diff;
							bestDateStr = d;
							bestDate = candidate;
						}
					}
				}

				if (!bestDateStr.empty()) {
					result.actualInfo = bestDateStr;

					// 根据日期格式精度选择合适的比较方法
					DatePrecision precision = getDatePrecision(target.info);
					bool withinTolerance = false;

					switch (precision) {
					case DatePrecision::Year:
						withinTolerance = (abs(minDiff) <= m_params.inputInfo.expirationDateError / 365);
						break;
					case DatePrecision::Month:
						withinTolerance = (abs(minDiff) <= m_params.inputInfo.expirationDateError / 30);
						break;
					case DatePrecision::Day:
						withinTolerance = (abs(minDiff) <= m_params.inputInfo.expirationDateError);
						break;
					case DatePrecision::Hour:
						withinTolerance = (abs(minDiff) <= m_params.inputInfo.expirationDateError * 24);
						break;
					case DatePrecision::Minute:
						withinTolerance = (abs(minDiff) <= m_params.inputInfo.expirationDateError * 24 * 60);
						break;
					case DatePrecision::Second:
						withinTolerance = (abs(minDiff) <= m_params.inputInfo.expirationDateError * 24 * 60);
						break;
					default:
						withinTolerance = (abs(minDiff) <= m_params.inputInfo.expirationDateError);
						break;
					}

					if (withinTolerance) {
						result.status = "OK";
					}
					else {
						result.status = "NG";
						result.message = "时差: " + std::to_string(minDiff) + " " + danwei +
							" , 超出允许误差 " + std::to_string(m_params.inputInfo.expirationDateError) + " 天";
						all_ok = false;
					}
				}
				else {
					result.status = "NG";
					result.message = "未找到有效保质日期";
					all_ok = false;
				}
			}
			else {
				result.status = "NG";
				result.message = "未找到任何日期格式";
				all_ok = false;
			}
		}
		else if (target.type == "生产流水号") {
			result.expectedInfo = "递增序列";

			// 搜索流水号（假设流水号是纯数字）
			regex serialPattern(R"(\d+)");
			smatch serialMatch;
			string foundSerial;

			if (regex_search(allText, serialMatch, serialPattern)) {
				foundSerial = serialMatch.str();
				result.actualInfo = foundSerial;

				std::lock_guard<std::mutex> lock(staticMutex);
				string& lastSerial = lastSerialNumber[outInfo.system.cameraId];

				if (!lastSerial.empty()) {
					try {
						long curr = std::stol(foundSerial);
						long last = std::stol(lastSerial);
						if (curr <= last) {
							result.status = "NG";
							result.message = "流水号必须大于前一个 (" + lastSerial + ")";
							all_ok = false;
						}
						else {
							result.status = "OK";
							result.message = "流水号递增正常";
						}
					}
					catch (...) {
						if (foundSerial <= lastSerial) {
							result.status = "NG";
							result.message = "流水号必须大于前一个 (" + lastSerial + ")";
							all_ok = false;
						}
						else {
							result.status = "OK";
							result.message = "流水号递增正常";
						}
					}
					lastSerial = foundSerial;
				}
				else {
					result.status = "OK";
					result.message = "第一个流水号";
					lastSerial = foundSerial;
				}
			}
			else {
				result.status = "NG";
				result.message = "未找到生产流水号";
				all_ok = false;
			}
		}
		else if (target.type == "固定字符") {
			result.expectedInfo = target.info;

			if (allText.find(target.info) != string::npos) {
				result.actualInfo = target.info;
				result.status = "OK";
			}
			else {
				result.status = "NG";
				result.message = "未找到: " + target.info;
				all_ok = false;
			}
		}
		else {
			result.status = "跳过";
			result.message = "已跳过验证";
		}

		outInfo.classification.codeMatchResult.push_back(result);
	}

	if (!all_ok && outInfo.status.statusCode == CODE_RETURN_OK) {
		outInfo.status.statusCode = CODE_RETURN_CLASSFY_INFO_ERR;
		outInfo.status.errorMessage = "喷码信息错误!";
		Log::WriteAsyncLog("喷码信息错误!", ERR, outInfo.paths.logFile, true);
	}
}


// 将类别名称转换为安全的文件夹名称
std::string toSafeFolderName(const std::string& class_name) {
	std::string safe_name;
	for (char c : class_name) {
		auto it = symbol_map.find(c);
		if (it != symbol_map.end()) {
			safe_name += it->second;
		}
		else {
			safe_name += c;
		}
	}
	return safe_name;
}

void InspCode::Code_SaveChars(InspCodeOut& outInfo)
{

	if (outInfo.locate.lineDetails.empty())
	{
		return;
	}

	for (int i = 0; i < outInfo.locate.locateDetails.size(); i++)
	{
		outInfo.locate.locateDetails[i].box = ANA->AdjustROI(outInfo.locate.locateDetails[i].box, m_imgLocate);
		outInfo.locate.locateDetails[i].box.x += (m_params.checkArea.x + outInfo.geometry.codeRect.x);
		outInfo.locate.locateDetails[i].box.y += (m_params.checkArea.y + outInfo.geometry.codeRect.y);
	}

	for (int i = 0; i < outInfo.locate.lineDetails.size(); i++)
	{
		for (int j = 0; j < outInfo.locate.lineDetails[i].size(); j++)
		{
			outInfo.locate.lineDetails[i][j].box.x += (m_params.checkArea.x + outInfo.geometry.codeRect.x);
			outInfo.locate.lineDetails[i][j].box.y += (m_params.checkArea.y + outInfo.geometry.codeRect.y);
			outInfo.locate.lineDetails[i][j].box = ANA->AdjustROI(outInfo.locate.lineDetails[i][j].box, m_img);
		}
	}

	if (outInfo.status.statusCode == CODE_RETURN_CONFIG_ERR)
	{
		return;
	}
	//if (m_params.saveTrain)
	//{
	//	//匹配模式关键字和全字符
	//	//全字符
	//	//字符数量一致，OK/NG的分开存储
	//	//字符数量不一致，全部存OTHER
	//	// 
	//	//关键字，全部存OTHER
	//	auto format = [](float conf) {
	//		return (std::ostringstream() << std::fixed << std::setprecision(2) << conf).str();
	//	};
	//	if (m_params.inputInfo.checkType == 1)
	//	{
	//		int sc = 0;
	//		int line0 = 0;
	//		if (outInfo.status.statusCode == CODE_RETURN_LOCATE_LESS || outInfo.status.statusCode == CODE_RETURN_LOCATE_MORE)
	//		{
	//			for (int i = 0; i < outInfo.classification.codeMatchResult.size(); i++)
	//			{
	//				if (outInfo.classification.codeMatchResult[i].type == "整体验证")
	//				{
	//					continue;
	//				}
	//				int line = outInfo.classification.codeMatchResult[i].row;
	//				if (line > line0)
	//				{
	//					sc = 0;
	//					line0 = line;
	//				}
	//				int charNum = getCharCountFromGBK(outInfo.classification.codeMatchResult[i].actualInfo);
	//				std::string base_path = outInfo.paths.trainDir + "CLASSFY/全字符/OTHER/";

	//				for (int j = sc; j < sc + charNum; j++)
	//				{
	//					std::string safe_name = toSafeFolderName(outInfo.locate.lineDetails[line - 1][j].className);
	//					if (safe_name == "")
	//					{
	//						safe_name = "unknow";
	//					}
	//					std::string class_folder_path = base_path + safe_name;

	//					if (!fs::exists(class_folder_path)) {
	//						fs::create_directories(class_folder_path);
	//					}
	//					cv::imwrite(class_folder_path + "/" +
	//						outInfo.system.startTime + "_" +
	//						std::to_string(outInfo.system.cameraId) + "_" +
	//						std::to_string(outInfo.system.jobId) + "_safe_name" + "_" +
	//						format(outInfo.locate.lineDetails[line - 1][j].confidence) + ".jpg",
	//						m_img(outInfo.locate.lineDetails[line - 1][j].box));
	//				}

	//				sc += charNum;
	//			}
	//		}
	//		else
	//		{
	//			for (int i = 0; i < outInfo.classification.codeMatchResult.size(); i++)
	//			{
	//				if (outInfo.classification.codeMatchResult[i].type == "整体验证")
	//				{
	//					continue;
	//				}
	//				int line = outInfo.classification.codeMatchResult[i].row;
	//				if (line > line0)
	//				{
	//					sc = 0;
	//					line0 = line;
	//				}
	//				int charNum = getCharCountFromGBK(outInfo.classification.codeMatchResult[i].actualInfo);
	//				std::string base_path;

	//				if (outInfo.classification.codeMatchResult[i].status == "OK")
	//				{
	//					base_path = outInfo.paths.trainDir + "CLASSFY/全字符/OK/";
	//				}
	//				else if (outInfo.classification.codeMatchResult[i].status == "NG")
	//				{
	//					base_path = outInfo.paths.trainDir + "CLASSFY/全字符/NG/";
	//				}
	//				else
	//				{
	//					base_path = outInfo.paths.trainDir + "CLASSFY/全字符/OTHER/";
	//				}
	//				for (int j = sc; j < sc + charNum; j++)
	//				{
	//					std::string safe_name = toSafeFolderName(outInfo.locate.lineDetails[line - 1][j].className);
	//					if (safe_name == "")
	//					{
	//						safe_name = "unknow";
	//					}
	//					std::string class_folder_path = base_path + safe_name;

	//					if (!fs::exists(class_folder_path)) {
	//						fs::create_directories(class_folder_path);
	//					}
	//					cv::imwrite(class_folder_path + "/" +
	//						outInfo.system.startTime + "_" +
	//						std::to_string(outInfo.system.cameraId) + "_" +
	//						std::to_string(outInfo.system.jobId) + "_" +
	//						format(outInfo.locate.lineDetails[line - 1][j].confidence) + ".jpg",
	//						m_img(outInfo.locate.lineDetails[line - 1][j].box));
	//				}

	//				sc += charNum;
	//			}
	//		}

	//	}
	//	else if (m_params.inputInfo.checkType == 0 && outInfo.status.statusCode != CODE_RETURN_LOCATE_LESS && outInfo.status.statusCode != CODE_RETURN_LOCATE_MORE)
	//	{
	//		int sc = 0;
	//		int line0 = 0;
	//		for (int i = 0; i < outInfo.locate.lineDetails.size(); i++)
	//		{
	//			for (int j = 0; j < outInfo.locate.lineDetails[i].size(); j++)
	//			{
	//				int charNum = getCharCountFromGBK(outInfo.classification.codeMatchResult[i].actualInfo);
	//				std::string base_path = outInfo.paths.trainDir + "CLASSFY/关键字/";


	//				std::string safe_name = toSafeFolderName(outInfo.locate.lineDetails[i][j].className);
	//				if (safe_name == "")
	//				{
	//					safe_name = "unknow";
	//				}
	//				std::string class_folder_path = base_path + safe_name;

	//				if (!fs::exists(class_folder_path)) {
	//					fs::create_directories(class_folder_path);
	//				}
	//				cv::imwrite(class_folder_path + "/" +
	//					outInfo.system.startTime + "_" +
	//					std::to_string(outInfo.system.cameraId) + "_" +
	//					std::to_string(outInfo.system.jobId) + "_" +
	//					format(outInfo.locate.lineDetails[i][j].confidence) + ".jpg",
	//					m_img(outInfo.locate.lineDetails[i][j].box));
	//			}
	//		}
	//	}



	//}
}


//void InspCode::Code_CheckDefect(InspCodeOut& outInfo) {
//	if (CheckTimeout(m_params.timeOut)) return;
//	if (outInfo.status.statusCode != CODE_RETURN_OK) {
//		Log::WriteAsyncLog("跳过缺陷检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
//		return;
//	}
//	else
//	{
//		Log::WriteAsyncLog("开始缺陷检测!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
//	}
//
//
//	if (m_params.defectWeightsFile.find(".onnx") != std::string::npos)
//	{
//		outInfo.defects.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.defectWeightsFile, m_params.defectClassName, outInfo.images.codeRegion.mat(), 0.1, 0.5);
//	}
//	else
//	{
//		outInfo.status.statusCode = CODE_RETURN_CONFIG_ERR;
//		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
//		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);
//
//		return;
//	}
//
//	if (m_params.saveDebugImage)
//	{
//		outInfo.images.codeRegionDefectLog.data = std::make_shared<cv::Mat>(outInfo.images.codeRegion.mat().clone());
//		outInfo.images.codeRegionDefectLog.stageName = "Code_CheckDefect";
//		outInfo.images.codeRegionDefectLog.description = "缺陷定位";
//		outInfo.images.codeRegionDefectLog.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
//		DAS->DAS_FinsObject(outInfo.images.codeRegionDefectLog.mat(), outInfo.locate.details, outInfo.paths.intermediateImagesDir + "3.1.1.detections.jpg", m_params.saveDebugImage);
//	}
//
//	//
//	/*  0 word_LRP
//		1 word_BARERR
//		2 word_BARB
//		3 word_BARD
//		4 word_LEAK
//		5 word_CRIMP*/
//	Log::WriteAsyncLog("开始分析缺陷检测结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
//	std::vector<cv::Rect> lrpRects;
//	for (int i = outInfo.defects.details.size() - 1; i >= 0; --i)
//	{
//		auto& defect = outInfo.defects.details[i];
//		int paramIndex = -1; // 根据缺陷类别设置对应参数索引
//
//		bool valid = true;
//		if (defect.className == "喷码破损") paramIndex = 5;  // 喷码破损
//		else if (defect.className == "防盗环缺陷")paramIndex = 1;  // 防盗环缺陷
//		else if (defect.className == "防盗环断桥")   paramIndex = 2;  // 防盗环断桥
//		else if (defect.className == "上下盖分离")  paramIndex = 3;  // 上下盖分离
//		else if (defect.className == "压盖不严") paramIndex = 4;  // 压盖不严
//		else if (defect.className == "支撑环端点")	paramIndex = 0;  // 支撑环端点
//
//		if (paramIndex != -1)
//		{
//			auto& para = m_params.defectPara[paramIndex];
//			if (defect.box.width < para.widthRange[0] ||
//				defect.box.width > para.widthRange[1] ||
//				defect.box.height < para.heightRange[0] ||
//				defect.box.height > para.heightRange[1] ||
//				defect.confidence < para.confidenceThresh)
//			{
//				valid = false;
//			}
//		}
//
//		if (!valid) {
//			// 移除不符合条件的缺陷
//			outInfo.defects.details.erase(outInfo.defects.details.begin() + i);
//
//		}
//	}
//
//	for (int i = 0; i < outInfo.defects.details.size(); i++)
//	{
//		if (outInfo.defects.details[i].className == "喷码破损")
//		{
//			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
//			{
//				outInfo.system.startTime = COM->time_t2string_with_ms();
//				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/喷码破损/");
//				auto jsonData = generateXAnyLabelingJSON(
//					outInfo.defects.details,
//					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
//					outInfo.images.codeRegion.mat().rows,
//					outInfo.images.codeRegion.mat().cols
//				);
//				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/喷码破损/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
//				cv::imwrite(outInfo.paths.trainDir + "DEFECT/喷码破损/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", outInfo.images.codeRegion.mat());
//
//			}
//			outInfo.defects.findCRIMP = true;
//			outInfo.status.errorMessage = "缺陷-喷码破损!";
//			Log::WriteAsyncLog("缺陷-喷码破损!", ERR, outInfo.paths.logFile, true);
//			outInfo.status.statusCode = CODE_RETURN_CAP_CRIMP;
//			return;
//		}
//		else if (outInfo.defects.details[i].className == "防盗环缺陷")
//		{
//			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
//			{
//				outInfo.system.startTime = COM->time_t2string_with_ms();
//				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/防盗环缺陷/");
//				auto jsonData = generateXAnyLabelingJSON(
//					outInfo.defects.details,
//					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
//					outInfo.images.codeRegion.mat().rows,
//					outInfo.images.codeRegion.mat().cols
//				);
//				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/防盗环缺陷/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
//				cv::imwrite(outInfo.paths.trainDir + "DEFECT/防盗环缺陷/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", outInfo.images.codeRegion.mat());
//
//			}
//			outInfo.defects.findBARERR = true;
//			outInfo.status.errorMessage = "缺陷-防盗环缺陷!";
//			Log::WriteAsyncLog("缺陷-防盗环缺陷!", ERR, outInfo.paths.logFile, true);
//			outInfo.status.statusCode = CODE_RETURN_BAR_BREAK;
//			return;
//		}
//		else if (outInfo.defects.details[i].className == "防盗环断桥")
//		{
//			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
//			{
//				outInfo.system.startTime = COM->time_t2string_with_ms();
//				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/防盗环断桥/");
//				auto jsonData = generateXAnyLabelingJSON(
//					outInfo.defects.details,
//					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
//					outInfo.images.codeRegion.mat().rows,
//					outInfo.images.codeRegion.mat().cols
//				);
//				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/防盗环断桥/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
//				cv::imwrite(outInfo.paths.trainDir + "DEFECT/防盗环断桥/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", outInfo.images.codeRegion.mat());
//
//			}
//			outInfo.defects.findBARB = true;
//			outInfo.status.errorMessage = "缺陷-防盗环断桥!";
//			Log::WriteAsyncLog("缺陷-防盗环断桥!", ERR, outInfo.paths.logFile, true);
//			outInfo.status.statusCode = CODE_RETURN_BAR_BRIDGE_BREAK;
//			return;
//		}
//		else if (outInfo.defects.details[i].className == "上下盖分离")
//		{
//			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
//			{
//				outInfo.system.startTime = COM->time_t2string_with_ms();
//				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/上下盖分离/");
//				auto jsonData = generateXAnyLabelingJSON(
//					outInfo.defects.details,
//					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
//					outInfo.images.codeRegion.mat().rows,
//					outInfo.images.codeRegion.mat().cols
//				);
//				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/上下盖分离/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
//				cv::imwrite(outInfo.paths.trainDir + "DEFECT/上下盖分离/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", outInfo.images.codeRegion.mat());
//
//			}
//			outInfo.defects.findBARD = true;
//			outInfo.status.errorMessage = "缺陷-上下盖分离!";
//			Log::WriteAsyncLog("缺陷-上下盖分离!", ERR, outInfo.paths.logFile, true);
//			outInfo.status.statusCode = CODE_RETURN_BAR_CAP_SEP;
//			return;
//		}
//		else if (outInfo.defects.details[i].className == "压盖不严")
//		{
//			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
//			{
//				outInfo.system.startTime = COM->time_t2string_with_ms();
//				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/压盖不严/");
//				auto jsonData = generateXAnyLabelingJSON(
//					outInfo.defects.details,
//					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
//					outInfo.images.codeRegion.mat().rows,
//					outInfo.images.codeRegion.mat().cols
//				);
//				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/压盖不严/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
//				cv::imwrite(outInfo.paths.trainDir + "DEFECT/压盖不严/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", outInfo.images.codeRegion.mat());
//
//			}
//			outInfo.defects.findLEAK = true;
//			outInfo.status.errorMessage = "缺陷-压盖不严!";
//			Log::WriteAsyncLog("缺陷-压盖不严!", ERR, outInfo.paths.logFile, true);
//			outInfo.status.statusCode = CODE_RETURN_LEAK;
//			return;
//		}
//		else if (outInfo.defects.details[i].className == "支撑环端点")
//		{
//			lrpRects.push_back(outInfo.defects.details[i].box);
//		}
//	}
//
//	if (lrpRects.size() < 2) {
//		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
//		{
//			outInfo.system.startTime = COM->time_t2string_with_ms();
//			COM->CreateDir(outInfo.paths.trainDir + "DEFECT/OUT/");
//			auto jsonData = generateXAnyLabelingJSON(
//				outInfo.defects.details,
//				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
//				outInfo.images.codeRegion.mat().rows,
//				outInfo.images.codeRegion.mat().cols
//			);
//			saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/OUT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
//			cv::imwrite(outInfo.paths.trainDir + "DEFECT/OUT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", outInfo.images.codeRegion.mat());
//
//
//		}
//		outInfo.status.errorMessage = "缺陷-支撑环端点定位异常!";
//		Log::WriteAsyncLog("缺陷-支撑环端点定位异常!", ERR, outInfo.paths.logFile, true);
//		outInfo.status.statusCode = CODE_RETURN_LR_FAILED;
//		return;
//	}
//	else if (lrpRects.size() > 2) {
//		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
//		{
//			outInfo.system.startTime = COM->time_t2string_with_ms();
//			COM->CreateDir(outInfo.paths.trainDir + "DEFECT/MULTY/");
//			auto jsonData = generateXAnyLabelingJSON(
//				outInfo.defects.details,
//				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
//				outInfo.images.codeRegion.mat().rows,
//				outInfo.images.codeRegion.mat().cols
//			);
//			saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
//			cv::imwrite(outInfo.paths.trainDir + "DEFECT/MULTY/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", outInfo.images.codeRegion.mat());
//
//		}
//		outInfo.status.errorMessage = "缺陷-支撑环端点定位异常!";
//		Log::WriteAsyncLog("缺陷-支撑环端点定位异常!", ERR, outInfo.paths.logFile, true);
//		outInfo.status.statusCode = CODE_RETURN_LR_FAILED;
//		return;
//	}
//	else {
//
//		if (abs(lrpRects[0].x - lrpRects[1].x) < outInfo.images.codeRegion.mat().cols / 3)
//		{
//			if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
//			{
//				outInfo.system.startTime = COM->time_t2string_with_ms();
//				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/OUT/");
//				auto jsonData = generateXAnyLabelingJSON(
//					outInfo.defects.details,
//					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
//					outInfo.images.codeRegion.mat().rows,
//					outInfo.images.codeRegion.mat().cols
//				);
//				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/OUT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
//				cv::imwrite(outInfo.paths.trainDir + "DEFECT/OUT/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", outInfo.images.codeRegion.mat());
//
//			}
//			outInfo.status.errorMessage = "缺陷-支撑环端点定位异常!";
//			Log::WriteAsyncLog("缺陷-支撑环端点定位异常!", ERR, outInfo.paths.logFile, true);
//			outInfo.status.statusCode = CODE_RETURN_LR_FAILED;
//			return;
//		}
//		else
//		{
//			if (m_params.saveTrain == 1 || m_params.saveTrain == 2)
//			{
//				outInfo.system.startTime = COM->time_t2string_with_ms();
//				COM->CreateDir(outInfo.paths.trainDir + "DEFECT/OK/");
//				auto jsonData = generateXAnyLabelingJSON(
//					outInfo.defects.details,
//					outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
//					outInfo.images.codeRegion.mat().rows,
//					outInfo.images.codeRegion.mat().cols
//				);
//				saveJSONToFile(jsonData, outInfo.paths.trainDir + "DEFECT/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
//				cv::imwrite(outInfo.paths.trainDir + "DEFECT/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", outInfo.images.codeRegion.mat());
//
//
//			}
//			Log::WriteAsyncLog("正常定位到支撑环端点!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
//		}
//		if (lrpRects[0].x < lrpRects[1].x)
//		{
//			outInfo.geometry.codeNeckLeftRect = lrpRects[0];
//			outInfo.geometry.codeNeckRightRect = lrpRects[1];
//		}
//		else
//		{
//			outInfo.geometry.codeNeckLeftRect = lrpRects[1];
//			outInfo.geometry.codeNeckRightRect = lrpRects[0];
//		}
//		outInfo.geometry.leftBottomPoint.x = outInfo.geometry.codeNeckLeftRect.x;
//		outInfo.geometry.leftBottomPoint.y = outInfo.geometry.codeNeckLeftRect.y;
//		outInfo.geometry.rightBottomPoint.x = outInfo.geometry.codeNeckRightRect.x + outInfo.geometry.codeNeckRightRect.width;
//		outInfo.geometry.rightBottomPoint.y = outInfo.geometry.codeNeckRightRect.y;
//
//
//		CALC_LinePara bottomLine;
//		CAL->CALC_Line(outInfo.geometry.leftBottomPoint, outInfo.geometry.rightBottomPoint, bottomLine);
//		if (bottomLine.angle > 90) {
//			outInfo.geometry.bottomAngle = bottomLine.angle - 180;
//		}
//		else {
//			outInfo.geometry.bottomAngle = bottomLine.angle;
//		}
//
//	}
//}
//

void InspCode::drawDetectionResults(cv::Mat& image,
	const std::vector<DetectionResult>& results,
	const std::vector<TargetConfig>& targets)
{
	// 设置绘制参数
	const cv::Scalar color_ok(0, 255, 0);     // 绿色 - OK
	const cv::Scalar color_ng(0, 0, 255);     // 红色 - NG
	const cv::Scalar color_skip(0, 255, 255);  // 黄色 - 跳过

	float lf = image.cols / 1280.0;
	const int fontSize = 20 * lf;  // 字体大小
	const int fontWeight = 2;  // 字体权重
	const char* fontName = "SimHei"; // 支持中文的字体

	// Y坐标偏移量（所有文本下移100像素）
	const int yOffset = 120;


	int yPos = 20 + yOffset; // 起始Y位置
	int okCount = 0, ngCount = 0, skipCount = 0; // 统计各类结果数量

	for (const auto& result : results) {
		// 确定状态颜色
		cv::Scalar status_color;
		if (result.status == "OK") {
			status_color = color_ok;
			okCount++;
		}
		else if (result.status == "NG") { // 或者 "NG"，根据实际状态字符串
			status_color = color_ng;
			ngCount++;
		}
		else {
			status_color = color_skip; // 跳过状态使用黄色
			skipCount++;
		}

		// 创建合并的状态信息字符串
		std::string status_text = "行" + std::to_string(result.row) +
			" 部分" + std::to_string(result.part) +
			" 实际: " + result.actualInfo +
			" 预期: " + result.expectedInfo +
			" - " + result.type +
			" [" + result.status + "]";

		// 添加消息（如果有）
		if (!result.message.empty()) {
			status_text += " (" + result.message + ")";
		}

		// 绘制状态文本
		putTextZH(image, status_text.c_str(),
			cv::Point(10, yPos), // 使用动态Y位置
			status_color, fontSize, fontWeight, fontName,
			false, false);

		// 增加Y位置，为下一个结果留出空间
		yPos += 40 * lf; // 每行增加40像素高度
	}
}

void InspCode::Code_CheckBar(InspCodeOut& outInfo){
	if (outInfo.status.statusCode != CODE_RETURN_OK) {
		LOG->WriteLog("跳过一维码/二维码检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}

	if (!m_params.isCheckBar) {
		LOG->WriteLog("未开启一维码/二维码检测!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		LOG->WriteLog("开始一维码/二维码检测!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	for (int i = 0; i < m_params.barConfigs.size(); i++)
	{
		if (!ANA->JudgeRectIn(cv::Rect(0, 0, m_img.cols, m_img.rows), m_params.barConfigs[i].roi)) {
			outInfo.status.statusCode = CODE_RETURN_OK;
			outInfo.status.errorMessage = "一维码/二维码roi设置超出图像范围!";
			LOG->WriteLog("一维码/二维码roi设置超出图像范围", ERR, outInfo.paths.logFile, true);
			return;
		}

		Mat checkArea = m_imgGray(m_params.barConfigs[i].roi).clone();
		std::vector<BarResult> barResult;
		if (m_params.barConfigs[i].checkType == "一维码" || m_params.barConfigs[i].checkType == "1D")
		{
			BAQ->BAQ_CheckBar(checkArea, m_params.barConfigs[i], barResult);
		}
		else
		{
			BAQ->BAQ_CheckQR(checkArea, m_params.barConfigs[i], barResult);
		}
		outInfo.bar.barResults.push_back(barResult);
	}


	for (int i = 0; i < outInfo.bar.barResults.size(); i++)
	{
		if (m_params.barConfigs[i].checkType == "一维码" || m_params.barConfigs[i].checkType == "1D")
		{
			if (outInfo.bar.barResults[i].empty())
			{
				outInfo.status.statusCode = CODE_RETURN_NO_1D;
				outInfo.status.errorMessage = "无一维码!";
				LOG->WriteLog("无一维码！", ERR, outInfo.paths.logFile, true);
				return;
			}
			int cntBar = 0;
			for (int j = 0; j < outInfo.bar.barResults[i].size(); j++)
			{
				std::cout << outInfo.bar.barResults[i][j].barType << ": " << outInfo.bar.barResults[i][j].infoResult << std::endl;
				if (outInfo.bar.barResults[i][j].infoResult == m_params.barConfigs[i].info)
				{
					outInfo.bar.barResults[i][j].state = true;
					cntBar++;
				}
				else
				{
					outInfo.bar.barResults[i][j].state = false;
				}
			}
			if (cntBar > m_params.barConfigs[i].countRange[1])
			{
				outInfo.status.statusCode = CODE_RETURN_MISS_1D;
				outInfo.status.errorMessage = "多一维码!";
				LOG->WriteLog("多一维码！", ERR, outInfo.paths.logFile, true);
				return;
			}
			if (cntBar == 0)
			{
				outInfo.status.statusCode = CODE_RETURN_INFO_ERR_1D;
				outInfo.status.errorMessage = "一维码信息错误!";
				LOG->WriteLog("一维码信息错误！", ERR, outInfo.paths.logFile, true);
				return;
			}
		}
		if (m_params.barConfigs[i].checkType == "二维码" || m_params.barConfigs[i].checkType == "2D")
		{
			if (outInfo.bar.barResults[i].empty())
			{
				outInfo.status.statusCode = CODE_RETURN_NO_2D;
				outInfo.status.errorMessage = "无二维码!";
				LOG->WriteLog("无二维码！", ERR, outInfo.paths.logFile, true);
				return;
			}
			int cntQr = 0;
			for (int j = 0; j < outInfo.bar.barResults[i].size(); j++)
			{
				std::cout << outInfo.bar.barResults[i][j].barType << ": " << outInfo.bar.barResults[i][j].infoResult << std::endl;
				if (outInfo.bar.barResults[i][j].infoResult == m_params.barConfigs[i].info)
				{
					outInfo.bar.barResults[i][j].state = true;
					cntQr++;
				}
				else
				{
					outInfo.bar.barResults[i][j].state = false;
				}
			}
			if (cntQr > m_params.barConfigs[i].countRange[1])
			{
				outInfo.status.statusCode = CODE_RETURN_MISS_2D;
				outInfo.status.errorMessage = "多二维码!";
				LOG->WriteLog("多二维码！", ERR, outInfo.paths.logFile, true);
				return;
			}
			if (cntQr == 0)
			{
				outInfo.status.statusCode = CODE_RETURN_INFO_ERR_2D;
				outInfo.status.errorMessage = "二维码信息错误!";
				LOG->WriteLog("二维码信息错误！", ERR, outInfo.paths.logFile, true);
				return;
			}
		}

	}


	return;
}

int calculateAdaptiveFontSize(cv::Mat& img, const std::string& text,
	int maxFontSize, int minFontSize = 10,
	double maxWidthRatio = 0.9)
{
	if (text.empty() || img.cols <= 0) {
		return maxFontSize;
	}

	// 计算最大允许宽度
	int maxAllowedWidth = static_cast<int>(img.cols * maxWidthRatio);

	// 二分查找最优字体大小
	int left = minFontSize;
	int right = maxFontSize;
	int bestSize = minFontSize;

	while (left <= right) {
		int mid = left + (right - left) / 2;

		// 计算文本在mid字体大小下的估算宽度
		int textWidth = 0;
		for (size_t i = 0; i < text.length();)
		{
			unsigned char c = static_cast<unsigned char>(text[i]);
			if (c >= 0xE0 && c <= 0xEF && i + 2 < text.length()) {  // UTF-8 中文开头
				// 中文字符，宽度等于字体大小
				textWidth += mid;
				i += 3;  // 跳过UTF-8 3字节
			}
			else if (c >= 0xC0 && c <= 0xDF && i + 1 < text.length()) {  // UTF-8 2字节
			 // 其他UTF-8 2字节字符
				textWidth += static_cast<int>(mid * 0.8);
				i += 2;
			}
			else {
				// ASCII字符
				textWidth += static_cast<int>(mid * 0.6);
				i += 1;
			}
		}

		if (textWidth <= maxAllowedWidth) {
			// 当前字体大小可以容纳，尝试更大的字体
			bestSize = mid;
			left = mid + 1;
		}
		else {
			// 当前字体大小太大，尝试更小的字体
			right = mid - 1;
		}
	}

	return bestSize;
}

void InspCode::Code_DrawResult(InspCodeOut& outInfo) {
	if (CheckTimeout(m_params.timeOut)) return;
	Log::WriteAsyncLog("开始绘制结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

	// 计算动态字体大小（以图像高度1000像素为基准）
	int imgHeight = MIN(outInfo.images.outputImg.rows, outInfo.images.outputImg.cols);
	double fontScale = imgHeight / 1000.0;
	/*if (outInfo.geometry.matchResults.size() > 5 && outInfo.geometry.matchResults.size() < 10)
	{
		fontScale *= 0.5;;
	}
	else if (outInfo.geometry.matchResults.size() > 10)
	{
		fontScale *= 0.3;;
	}*/

	// 定义基准字体大小
	int baseFontSizeStatus = static_cast<int>(45 * fontScale); // 状态信息基准大小
	int baseFontSizeFeature = static_cast<int>(35 * fontScale); // 特征状态基准大小
	int baseFontSizeDetail = static_cast<int>(30 * fontScale); // 详细信息基准大小
	int baseFontSizeBar = static_cast<int>(30 * fontScale); // 一维码信息基准大小


	auto format = [](float conf) {
		return (std::ostringstream() << std::fixed << std::setprecision(2) << conf).str();
	};


	if (m_params.basicInfo.warp)
	{
		m_imgLocate.copyTo(outInfo.images.outputImg(outInfo.geometry.codeRect));
	}

	if (m_params.isYW && outInfo.locate.ywDetails.empty())
	{
		rectangle(outInfo.images.outputImg, m_params.basicInfo.roi, Colors::YELLOW, 3, cv::LINE_AA);
	}
	for (int i = 0; i < outInfo.locate.ywDetails.size(); i++)
	{
		if (outInfo.locate.ywDetails[i].className == "OK")
		{


			outInfo.locate.ywDetails[i].box.x -= m_params.basicInfo.extW;
			outInfo.locate.ywDetails[i].box.y -= m_params.basicInfo.extH;
			outInfo.locate.ywDetails[i].box.width += m_params.basicInfo.extW * 2;
			outInfo.locate.ywDetails[i].box.height += m_params.basicInfo.extH * 2;

			rectangle(outInfo.images.outputImg, outInfo.locate.ywDetails[i].box, Colors::GREEN, 3, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg,
				("喷码," + std::to_string(outInfo.locate.ywDetails[i].box.width) + "," + std::to_string(outInfo.locate.ywDetails[i].box.height) + "," + format(outInfo.locate.ywDetails[i].confidence)).c_str(),
				cv::Point(outInfo.locate.ywDetails[i].box.x, outInfo.locate.ywDetails[i].box.y + outInfo.locate.ywDetails[i].box.height + 10),
				Colors::GREEN, 45, FW_BOLD);
		}
		else if (outInfo.locate.ywDetails[i].className == "NG")
		{
			rectangle(outInfo.images.outputImg, outInfo.locate.ywDetails[i].box, Colors::RED, 3, cv::LINE_AA);
			putTextZH(outInfo.images.outputImg,
				(outInfo.locate.ywDetails[i].className + "," + std::to_string(outInfo.locate.ywDetails[i].box.width) + "," + std::to_string(outInfo.locate.ywDetails[i].box.height) + "," + format(outInfo.locate.ywDetails[i].confidence)).c_str(),
				cv::Point(outInfo.locate.ywDetails[i].box.x, outInfo.locate.ywDetails[i].box.y + outInfo.locate.ywDetails[i].box.height + 10),
				Colors::RED, 45, FW_BOLD);
		}
	}

	for (int i = 0; i < outInfo.locate.locateDetails.size(); i++)
	{
		rectangle(outInfo.images.outputImg, outInfo.locate.locateDetails[i].box, Colors::GREEN, 1, cv::LINE_AA);
		if (outInfo.locate.locateDetails[i].className == "")
		{
			continue;
		}
		if (m_params.isClassfy)
		{
			putTextZH(outInfo.images.outputImg,
				outInfo.locate.locateDetails[i].className.c_str(),
				cv::Point(outInfo.locate.locateDetails[i].box.x + 2, outInfo.locate.locateDetails[i].box.y),
				Colors::GREEN, 15, FW_BOLD);
		}
		//putTextZH(outInfo.images.outputImg,
		//	(std::to_string(outInfo.locate.locateDetails[i].box.width) + "," + std::to_string(outInfo.locate.locateDetails[i].box.height)).c_str(),
		//	cv::Point(outInfo.locate.locateDetails[i].box.x, outInfo.locate.locateDetails[i].box.y + 10),
		//	Colors::GREEN, 10, FW_BOLD);
		//putTextZH(outInfo.images.outputImg,
		//	format(outInfo.locate.locateDetails[i].confidence).c_str(),
		//	cv::Point(outInfo.locate.locateDetails[i].box.x, outInfo.locate.locateDetails[i].box.y + 20),
		//	Colors::GREEN, 10, FW_BOLD);
	}

	for (int i = 0; i < outInfo.locate.badLocateDetails.size(); i++)
	{
		rectangle(outInfo.images.outputImg, outInfo.locate.badLocateDetails[i].box, Colors::WHITE, 1, cv::LINE_AA);

		putTextZH(outInfo.images.outputImg,
			std::to_string(outInfo.locate.badLocateDetails[i].box.width).c_str(),
			cv::Point(outInfo.locate.badLocateDetails[i].box.x, outInfo.locate.badLocateDetails[i].box.y + 10),
			Colors::WHITE, 15, FW_BOLD);
		putTextZH(outInfo.images.outputImg,
			std::to_string(outInfo.locate.badLocateDetails[i].box.height).c_str(),
			cv::Point(outInfo.locate.badLocateDetails[i].box.x, outInfo.locate.badLocateDetails[i].box.y + 20),
			Colors::WHITE, 15, FW_BOLD);
		putTextZH(outInfo.images.outputImg,
			format(outInfo.locate.badLocateDetails[i].confidence).c_str(),
			cv::Point(outInfo.locate.badLocateDetails[i].box.x, outInfo.locate.badLocateDetails[i].box.y + 30),
			Colors::WHITE, 15, FW_BOLD);
	}




	if (m_params.isAssist)
	{
		rectangle(outInfo.images.outputImg, outInfo.geometry.codeRect, Colors::YELLOW, 3, cv::LINE_AA);
		ANA->RotateImg(outInfo.images.outputImg, outInfo.images.outputImg, outInfo.geometry.matchLocateResult.center, -outInfo.geometry.matchLocateResult.angle);

		string matchResult1 = "得分:" + format(outInfo.geometry.matchLocateResult.score) + " 角度:" + format(outInfo.geometry.matchLocateResult.angle);
		string matchResult2 = "偏移: 水平:" + format(outInfo.geometry.matchLocateResult.shiftHor) + " 垂直:" + format(outInfo.geometry.matchLocateResult.shiftVer) + " 距离:" + format(outInfo.geometry.matchLocateResult.offset);

		cv::Point2f corners[4];
		outInfo.geometry.matchLocateResult.boundingRect.points(corners);
		outInfo.geometry.matchLocateResult.corners.assign(corners, corners + 4);

		if (outInfo.geometry.matchLocateResult.valid == 1)
		{
			// 绘制匹配的角点（四边形）
			if (!outInfo.geometry.matchLocateResult.corners.empty() &&
				outInfo.geometry.matchLocateResult.corners.size() >= 4)
			{
				// 绘制四边形的四条边
				for (int kk = 0; kk < 4; kk++)
				{
					cv::line(outInfo.images.outputImg,
						outInfo.geometry.matchLocateResult.corners[kk],
						outInfo.geometry.matchLocateResult.corners[(kk + 1) % 4],
						Colors::PURPLE,  // 使用OK/NG颜色
						2); // 线宽为2
				}
			}
			float sx = std::min_element(outInfo.geometry.matchLocateResult.corners.begin(), outInfo.geometry.matchLocateResult.corners.end(),
				[](const cv::Point2f& a, const cv::Point2f& b) {
					return a.x < b.x;
				})->x;

			float sy = std::max_element(outInfo.geometry.matchLocateResult.corners.begin(), outInfo.geometry.matchLocateResult.corners.end(),
				[](const cv::Point2f& a, const cv::Point2f& b) {
					return a.y < b.y;
				})->y;


			putTextZH(outInfo.images.outputImg, matchResult1.c_str(), cv::Point(sx, sy + 5), Colors::GREEN, 25, FW_HEAVY);
			putTextZH(outInfo.images.outputImg, matchResult2.c_str(), cv::Point(sx, sy + 45), Colors::GREEN, 25, FW_HEAVY);
		}
		else if (outInfo.geometry.matchLocateResult.valid > 1)
		{
			// 绘制匹配的角点（四边形）
			if (!outInfo.geometry.matchLocateResult.corners.empty() &&
				outInfo.geometry.matchLocateResult.corners.size() >= 4)
			{
				// 绘制四边形的四条边
				for (int kk = 0; kk < 4; kk++)
				{
					cv::line(outInfo.images.outputImg,
						outInfo.geometry.matchLocateResult.corners[kk],
						outInfo.geometry.matchLocateResult.corners[(kk + 1) % 4],
						Colors::RED,  // 使用OK/NG颜色
						2); // 线宽为2
				}
			}

			float sx = std::min_element(outInfo.geometry.matchLocateResult.corners.begin(), outInfo.geometry.matchLocateResult.corners.end(),
				[](const cv::Point2f& a, const cv::Point2f& b) {
					return a.x < b.x;
				})->x;

			float sy = std::max_element(outInfo.geometry.matchLocateResult.corners.begin(), outInfo.geometry.matchLocateResult.corners.end(),
				[](const cv::Point2f& a, const cv::Point2f& b) {
					return a.y < b.y;
				})->y;

			putTextZH(outInfo.images.outputImg, matchResult1.c_str(), cv::Point(sx, sy + 5), Colors::RED, 25, FW_HEAVY);
			putTextZH(outInfo.images.outputImg, matchResult2.c_str(), cv::Point(sx, sy + 45), Colors::RED, 25, FW_HEAVY);

			rectangle(outInfo.images.outputImg, m_params.assistPara.targetRoi, Colors::RED, 3, cv::LINE_AA);
		}
		else
		{
			m_params.assistPara.templatePose.x -= m_params.assistPara.extW;
			m_params.assistPara.templatePose.y -= m_params.assistPara.extH;
			m_params.assistPara.templatePose.width += m_params.assistPara.extW * 2;
			m_params.assistPara.templatePose.height += m_params.assistPara.extH * 2;
			rectangle(outInfo.images.outputImg, m_params.assistPara.templatePose, Colors::YELLOW, 3, cv::LINE_AA);
		}
	}
	else
	{
		rectangle(outInfo.images.outputImg, m_params.basicInfo.roi, Colors::YELLOW, 3, cv::LINE_AA);
	}

	

	// 在绘图代码中使用
	if (m_params.isClassfy)
	{
		
		for (int i = 0; i < outInfo.locate.codeInfo.size(); i++)
		{
			std::string text = outInfo.locate.codeInfo[i].second;
			int baseFontSize = 200;
			int yPos = outInfo.images.outputImg.rows - (outInfo.locate.codeInfo.size() - i) * baseFontSize - 30;

			// 计算自适应字体大小
			int fontSize = calculateAdaptiveFontSize(outInfo.images.outputImg, text, baseFontSize, 10, 0.95);

			putTextZH(outInfo.images.outputImg, text.c_str(),
				cv::Point(15, yPos),
				Colors::BLUE, fontSize, FW_BOLD);
		}
		for (int i = 0; i < outInfo.locate.codeInfo.size(); i++)
		{
			std::string text = outInfo.locate.codeInfo[i].second;
			int baseFontSize = 200;
			int yPos = outInfo.images.outputImg.rows - (outInfo.locate.codeInfo.size() - i) * baseFontSize - 30;

			// 计算自适应字体大小
			int fontSize = calculateAdaptiveFontSize(outInfo.images.outputImg, text, baseFontSize, 10, 0.95);

			putTextZH(outInfo.images.outputImg, text.c_str(),
				cv::Point(15, yPos),
				Colors::BLUE, fontSize, FW_BOLD);
		}
	}
	if (m_params.isClassfy)
	{
		drawDetectionResults(outInfo.images.outputImg, outInfo.classification.codeMatchResult, m_params.inputInfo.targets);
	}

	for (int kk = 0; kk < outInfo.bar.barResults.size(); kk++) {

		const auto& frameResults = outInfo.bar.barResults[kk];

		// 遍历当前帧的所有检测结果
		for (const auto& result : frameResults) {

			// 构建完整信息文本（不再缩短）
			std::string infoText = result.barType + ": " + result.infoResult;

			// 绘制旋转矩形
			cv::Point2f vertices[4];
			result.rect.points(vertices);
			cv::Point2f sPoint(10000, 10000);

			// 计算旋转矩形的最小点（左上角）
			for (int i = 0; i < 4; i++) {
				sPoint.x = MIN(sPoint.x, vertices[i].x);
				sPoint.y = MIN(sPoint.y, vertices[i].y);
			}

			// 计算文本位置（在矩形上方）
			cv::Point textPoint(static_cast<int>(sPoint.x), static_cast<int>(sPoint.y - 70));

			// 检查文本位置是否超出图像边界
			if (textPoint.y < 20) {
				textPoint.y = sPoint.y + 70; // 如果太靠上，移到下方
			}

			// 检查文本是否超出右边界
			int textWidth = static_cast<int>(infoText.length() * baseFontSizeBar * 0.6);
			if (textPoint.x + textWidth > outInfo.images.outputImg.cols - 10) {
				textPoint.x = outInfo.images.outputImg.cols - textWidth - 10;
			}

			// 根据一维码类型选择颜色
			cv::Scalar barColor;
			if (m_params.barConfigs[kk].checkType == "一维码" || m_params.barConfigs[kk].checkType == "1D") {
				barColor = Colors::GREEN;
			}
			else {
				barColor = Colors::BLUE;
			}

			// 绘制旋转矩形
			for (int i = 0; i < 4; i++) {
				cv::line(outInfo.images.outputImg, vertices[i], vertices[(i + 1) % 4], barColor, 2);
			}

			// 绘制完整文本（不再缩短）
			putTextZH(outInfo.images.outputImg, infoText.c_str(),
				textPoint, barColor, baseFontSizeBar, FW_BOLD);


		}
	}

	// 绘制状态信息（使用动态字体大小）
	std::string rv = "ID = " + std::to_string(outInfo.system.jobId) + ", " +
		"RV = " + std::to_string(outInfo.status.statusCode) + ", " +
		outInfo.status.errorMessage;

	if (outInfo.status.statusCode == CODE_RETURN_OK) {
		putTextZH(outInfo.images.outputImg, rv.c_str(),
			cv::Point(15, 30), Colors::GREEN, baseFontSizeStatus, FW_HEAVY);
	}
	else {
		putTextZH(outInfo.images.outputImg, rv.c_str(),
			cv::Point(15, 30), Colors::RED, baseFontSizeStatus, FW_HEAVY);
	}



	DAS->DAS_Img(outInfo.images.outputImg, outInfo.paths.intermediateImagesDir + "10.outputImg.jpg", m_params.saveDebugImage);

}

std::future<int> InspCode::RunInspectionAsync(InspCodeOut& outInfo) {
	return std::async(std::launch::async, [this, &outInfo] {
		// 设置超时检查起点
		m_startTime = std::chrono::high_resolution_clock::now();

		// 执行主检测逻辑
		return Code_Main(outInfo, true);
		});
}

int InspCode::Code_Main(InspCodeOut& outInfo, bool checkTimeout) {
	try {
		double time0 = static_cast<double>(cv::getTickCount());
		if (outInfo.status.statusCode == CODE_RETURN_OK)
		{
			Log::WriteAsyncLog("Code_Main!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			if (checkTimeout && CheckTimeout(m_params.timeOut)) {
				Log::WriteAsyncLog("超时!", WARNING, outInfo.paths.logFile, true);
				*m_timeoutFlagRef = true;
				outInfo.status.statusCode = CODE_RETURN_TIMEOUT;
				return CODE_RETURN_TIMEOUT;
			}



			Code_RotateImg(outInfo);
			outInfo.images.outputImg = m_img.clone();

			Code_SetROI(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return CODE_RETURN_TIMEOUT;
			}


			Code_Assist(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return CODE_RETURN_TIMEOUT;
			}

			Code_CheckYW(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return CODE_RETURN_TIMEOUT;
			}

			if (!m_params.isYW && !m_params.isAssist)
			{
				outInfo.geometry.codeRect = m_params.basicInfo.roi;
			}
			Code_WarpImg(outInfo);
			outInfo.images.outputImg = m_img.clone();

			if (m_params.basicInfo.warp)
			{
				m_imgLocate = m_imgWarp.clone();
			}
			else {
				m_imgLocate = m_img(outInfo.geometry.codeRect).clone();
			}


			Code_LocateCode(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return CODE_RETURN_TIMEOUT;
			}

			Code_ClassfyCode(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return CODE_RETURN_TIMEOUT;
			}


			Code_AnalysisCodePos(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return CODE_RETURN_TIMEOUT;
			}

			Code_CheckCode(outInfo);
			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return CODE_RETURN_TIMEOUT;
			}


			if (CheckTimeout(m_params.timeOut))
			{
				Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return CODE_RETURN_TIMEOUT;
			}
			//
			Code_SaveChars(outInfo);


			Code_CheckBar(outInfo);



		}



		// 第9步:绘制结果
		Code_DrawResult(outInfo);
		Code_SaveLocate(outInfo);
		if (CheckTimeout(m_params.timeOut))
		{
			Log::WriteAsyncLog("超时!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
			return CODE_RETURN_TIMEOUT;
		}

		if (outInfo.status.statusCode == CODE_RETURN_OK) {
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
		std::cerr << "[ERROR] Inference failed: " << e.what() << std::endl;
		return CODE_RETURN_ALGO_ERR;
	}

	return outInfo.status.statusCode;
}