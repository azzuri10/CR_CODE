#include "InspLevel.h"
#include "InspLevelStruct.h"
#include "ModelManager.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <locale>
#include "InferenceWorker.h"
#include "Data.h"
#include "AnalyseMat.h"

std::mutex InspLevel::modelLoadMutex;
std::map<std::string, std::string> InspLevel::levelDetectionModelMap;
std::map<int, InspLevelIn> InspLevel::cameraConfigMap;


// ���캯��:��ʼ��ʱ�����������ģ��
InspLevel::InspLevel(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
	bool isLoadConfig, int timeOut, InspLevelOut& outInfo)
	: ANA(std::make_unique<AnalyseMat>()),
	COM(std::make_unique<Common>())
{
	m_timeOut = timeOut;
	m_timeoutFlagRef = &outInfo.system.timeoutFlag;
	m_startTime = std::chrono::high_resolution_clock::now();
	COM->CreateDir(outInfo.paths.logDirectory);
	Log::WriteAsyncLog("********** Start Inspction JobID = ", INFO, outInfo.paths.logFile, true, outInfo.system.jobId, " ***********");
	if (img.channels() == 1)
	{
		m_img = img.clone();
		cv::cvtColor(img, outInfo.images.outputImg, cv::COLOR_GRAY2BGR);
	}
	else if (img.channels() == 3)
	{
		cv::cvtColor(img, m_img, cv::COLOR_BGR2GRAY);
		outInfo.images.outputImg = img.clone();
	}

	outInfo.status.errorMessage = "OK";
	outInfo.system.startTime = COM->time_t2string_with_ms();

	bool shouldLoadConfig = isLoadConfig ||
		jobId == 0 ||
		cameraConfigMap.find(cameraId) == cameraConfigMap.end();

	//��ȡconfig
	if (shouldLoadConfig)
	{
		bool rv_loadConfig = readParams(m_img, outInfo.paths.configFile, m_params, outInfo, outInfo.paths.logFile);
		if (!rv_loadConfig) {
			outInfo.status.statusCode = LEVEL_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = outInfo.status.errorMessage;
			Log::WriteAsyncLog(outInfo.status.errorMessage, ERR, outInfo.paths.logFile, true);
			return;
		}
		else
		{
			Log::WriteAsyncLog("��ȡconfig�ɹ�!", INFO, outInfo.paths.logFile, true);
		}


		//���roi
		if (!ANA->JudgeRectIn(cv::Rect(0, 0, img.cols, img.rows), m_params.roiRect)) {
			outInfo.status.statusCode = LEVEL_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "roi ���ó���ͼ��Χ!";
			Log::WriteAsyncLog("roi ���ó���ͼ��Χ", ERR, outInfo.paths.logFile, true);
			return;
		}



		/*if (isLoadConfig || outInfo.system.jobId == 0|| !loadLevelConfigSuccess[outInfo.system.cameraId])
		{
			bool loadModel = loadAllModels(outInfo, true);
			if (!loadModel) {
				outInfo.status.statusCode = LEVEL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "���ѧϰģ�ͼ����쳣!";
				Log::WriteAsyncLog(m_params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "---���ѧϰģ�ͼ����쳣!");
				return;
			}
		}
		else
		{
			Log::WriteAsyncLog("����ģ�ͼ��أ�", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		}



		if (!validateCameraModels(outInfo.system.cameraId)) {
			Log::WriteAsyncLog("���ID���ô���/ģ���ļ�ȱʧ!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = PRESSCAP_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "���ID���ô���/ģ���ļ�ȱʧ!";
			throw std::invalid_argument("���ID���ô���/ģ���ļ�ȱʧ!");
		}*/

		m_params.locateClassName = { "��Һλ","Һλ", "��ƿ" };
		cameraConfigMap[cameraId] = m_params;
	}
	else
	{
		m_params = cameraConfigMap[cameraId];
	}

	if (m_params.saveDebugImage) {
		COM->CreateDir(outInfo.paths.intermediateImagesDir);
	}
	if (m_params.saveResultImage) {
		COM->CreateDir(outInfo.paths.resultsOKDir);
		COM->CreateDir(outInfo.paths.resultsNGDir);
	}
}

InspLevel::~InspLevel() {
	// ���ڴ˴�������Դ�ͷ��߼���������Ҫ��
}

// ��֤����ͷID��Ӧ��ģ�������Ƿ����
bool InspLevel::validateCameraModels(int cameraId) {
	std::lock_guard<std::mutex> lock(modelLoadMutex);
	return levelDetectionModelMap.count("levelDetection_" + std::to_string(cameraId));
}

// ��������ģ�͵�ModelManager
bool InspLevel::loadAllModels(InspLevelOut& outInfo, bool ini) {
	//if (!ini)
	//{
	//    Log::WriteAsyncLog("����ģ�ͼ���!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
	//    return true;
	//}
	//std::vector<std::string> modelPaths;

	//// �ռ�����ģ��·��
	//for (const auto& pair : levelDetectionModelMap) {
	//    modelPaths.push_back(pair.second);
	//}

	//// ȥ��
	//std::sort(modelPaths.begin(), modelPaths.end());
	//auto last = std::unique(modelPaths.begin(), modelPaths.end());
	//modelPaths.erase(last, modelPaths.end());

	//// ����ģ��
	//try {
	//    ModelManager::Instance().LoadModels(modelPaths, ini, m_params.hardwareType);
	//}
	//catch (const std::exception& e) {
	//    Log::WriteAsyncLog("ģ�ͼ����쳣!", WARNING, outInfo.paths.logFile, true);
	//    outInfo.status.errorMessage = "ģ�ͼ����쳣!";
	//    std::cerr << "[ERROR] Failed to load models: " << e.what() << std::endl;
	//    throw;
	//}

	return 1;
}


// ��ȡ�����ĺ���
bool InspLevel::readParams(cv::Mat img, const std::string& filePath, InspLevelIn& params, InspLevelOut& outInfo, const std::string& fileName) {
	std::ifstream ifs(filePath.c_str());
	if (!ifs.is_open()) {
		ifs.close();
		outInfo.status.errorMessage = "config�ļ���ʧ!";
		Log::WriteAsyncLog("config�ļ���ʧ��", WARNING, outInfo.paths.logFile, true);
		return false;
	}
	std::string line;
	while (!ifs.eof()) {
		//��ȡ���ַ���
		//����"##"Ϊע�ͣ���������������
		//���֡�:������ȡ�ؼ��֣�δ������config�쳣
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
			outInfo.status.errorMessage = "����ȱʧ!";
			Log::WriteAsyncLog(keyWord, WARNING, outInfo.paths.logFile, true, " ����ȱʧ��");
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

		value.erase(0, value.find_first_not_of(" "));  // ȥ��ǰ��ո�
		value.erase(value.find_last_not_of(" ") + 1);
		value.erase(0, value.find_first_not_of(" "));
		value.erase(value.find_last_not_of(" ") + 1);

		std::string keyStr(value.begin(), value.end());

		//�Ƿ�洢�м�ͼ��(0:��  1:��)
		if (keyWord == "LEVEL_SAVE_DEBUG_IMAGE") {
			params.saveDebugImage = std::stoi(value);
		}
		else if (keyWord == "LEVEL_SAVE_RESULT_IMAGE") {
			params.saveResultImage = std::stoi(value);
		}
		else if (keyWord == "LEVEL_SAVE_LOG_TXT") {
			params.saveLogTxt = std::stoi(value);
		}
		else if (keyWord == "LEVEL_DRAW_RESULT") {
			params.drawResult = std::stoi(value);
		}
		else if (keyWord == "LEVEL_SAVE_TRAIN") {
			params.saveTrain = std::stoi(value);
		}
		else if (keyWord == "LEVEL_ROI_X") {
			params.roiRect.x = std::stoi(value);
			if (params.roiRect.x < 0 || params.roiRect.x > img.cols)
			{
				outInfo.status.errorMessage = "ROI_X: ����ͼ��Χ!";
				Log::WriteAsyncLog("ROI_X: ����ͼ��Χ��", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LEVEL_ROI_Y") {
			params.roiRect.y = std::stoi(value);
			if (params.roiRect.y < 0 || params.roiRect.y > img.rows)
			{
				outInfo.status.errorMessage = "ROI_Y: ����ͼ��Χ!";
				Log::WriteAsyncLog("ROI_Y: ����ͼ��Χ��", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LEVEL_ROI_W") {
			params.roiRect.width = std::stoi(value);
			if (params.roiRect.x + params.roiRect.width > img.cols)
			{
				outInfo.status.errorMessage = "ROI_X+ROI_W: ����ͼ��Χ!";
				Log::WriteAsyncLog("ROI_X+ROI_W: ����ͼ��Χ��", ERR, outInfo.paths.logFile, true);
				return false;
			}
			else if (params.roiRect.width < 10)
			{
				outInfo.status.errorMessage = "ROI_W < 10!";
				Log::WriteAsyncLog("ROI_W < 10!", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LEVEL_ROI_H") {
			params.roiRect.height = std::stoi(value);
			if (params.roiRect.y + params.roiRect.height > img.rows)
			{
				outInfo.status.errorMessage = "ROI_Y+ROI_H: ����ͼ��Χ!";
				Log::WriteAsyncLog("ROI_Y+ROI_H: ����ͼ��Χ��", ERR, outInfo.paths.logFile, true);
				return false;
			}
			else if (params.roiRect.height < 10)
			{
				outInfo.status.errorMessage = "ROI_H < 10!";
				Log::WriteAsyncLog("ROI_H < 10!", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LEVEL_BLUR_THRESH") {
			params.blurThresh = std::stoi(value);
			if (params.blurThresh > 100 || params.blurThresh < 1)
			{
				outInfo.status.errorMessage = "ƽ����ֵ: ������Χ(1,100)!";
				Log::WriteAsyncLog("ƽ����ֵ: ������Χ(1,100)��", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LEVEL_EDGE_THRESH") {
			params.edgeThresh = std::stoi(value);
			if (params.edgeThresh > 300 || params.edgeThresh < 10)
			{
				outInfo.status.errorMessage = "��Ե�����ֵ: ������Χ(10,300)!";
				Log::WriteAsyncLog("��Ե�����ֵ: ������Χ(10,300)��", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LEVEL_ANGLE_THRESH") {
			params.angleThresh = std::stoi(value);
			if (params.angleThresh > 45 || params.angleThresh < 1)
			{
				outInfo.status.errorMessage = "Һλ��������б��: ������Χ(1,45)!";
				Log::WriteAsyncLog("Һλ��������б��: ������Χ(1,45)��", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LEVEL_PROJECT_THRESH") {
			params.projectThresh = std::stoi(value);
			if (params.projectThresh > 10000 || params.projectThresh < 1)
			{
				outInfo.status.errorMessage = "ҺλˮƽͶӰ��ֵ: ������Χ(1,10000)!";
				Log::WriteAsyncLog("ҺλˮƽͶӰ��ֵ: ������Χ(1,10000)��", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LEVEL_GRAY_DIF_THRESH") {
			params.grayDis = std::stoi(value);
			if (params.grayDis > 255 || params.grayDis < -255)
			{
				outInfo.status.errorMessage = "Һλ���»ҶȲ���ֵ: ������Χ(-255,255)!";
				Log::WriteAsyncLog("Һλ���»ҶȲ���ֵ: ������Χ(-255,255)��", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LEVEL_UP_THRESH") {
			params.minY = std::stoi(value);
			if (params.minY > img.rows || params.minY < 10)
			{
				outInfo.status.errorMessage = "Һλ����: ������Χ!";
				Log::WriteAsyncLog("Һλ����: ������Χ��", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LEVEL_DOWN_THRESH") {
			params.maxY = std::stoi(value);
			if (params.maxY > img.rows || params.maxY < 10)
			{
				outInfo.status.errorMessage = "Һλ����: ������Χ!";
				Log::WriteAsyncLog("Һλ����: ������Χ��", ERR, outInfo.paths.logFile, true);
				return false;
			}
			else if (params.maxY < params.minY)
			{
				outInfo.status.errorMessage = "LEVEL_DOWN_THRESH must be >= LEVEL_UP_THRESH.";
				Log::WriteAsyncLog("LEVEL_DOWN_THRESH must be >= LEVEL_UP_THRESH.", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}


		//��ʱδʹ�����淽��******
		// 
		//else if (keyWord == "LEVEL_HARDWARE_TYPE") {
		//    params.hardwareType = std::stoi(value);
		//}
		//else if (keyWord == "LEVEL_MODEL_TYPE") {
		//    params.modelType = std::stoi(value);
		//}
		//if (keyWord == "LEVEL_LOCATE_WEIGHTS_FILE") {
		//    std::lock_guard<std::mutex> lock(modelLoadMutex);  // ����
		//    std::string camera = std::to_string(outInfo.system.cameraId);
		//    levelDetectionModelMap["levelDetection_" + camera] = value;
		//    params.locateWeightsFile = value;
		//    if (!COM->FileExistsModern(params.locateWeightsFile))
		//    {
		//        outInfo.status.errorMessage = params.locateWeightsFile + "--��λģ���ļ�ȱʧ!";
		//        Log::WriteAsyncLog(params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "--��λģ���ļ��ļ�ȱʧ��");
		//        return false;
		//    }
		//}       
	}

	ifs.close();
	return true;
}

void InspLevel::Level_SetROI(InspLevelOut& outInfo) {
	if (outInfo.status.statusCode != LEVEL_RETURN_OK) {
		Log::WriteAsyncLog("����ROI�����ȡ!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("��ʼROI�����ȡ!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}


	outInfo.images.roi = m_img(m_params.roiRect).clone();
	DAS->DAS_Rect(outInfo.images.roi, m_params.roiRect, outInfo.paths.intermediateImagesDir + "1.0.0.roiRect.jpg", m_params.saveDebugImage);

}

void InspLevel::Level_LocateLevel(InspLevelOut& outInfo) {
	if (outInfo.status.statusCode != LEVEL_RETURN_OK) {
		Log::WriteAsyncLog("����Һλ��λ!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		Log::WriteAsyncLog("��ʼҺλ��λ!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (m_params.locateWeightsFile.find(".onnx") != std::string::npos)
	{
		outInfo.locate.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.locateWeightsFile, m_params.locateClassName, outInfo.images.roi);
	}
	else
	{
		outInfo.status.statusCode = LEVEL_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "ģ���ļ��쳣��Ŀǰ��֧��onnx!";
		Log::WriteAsyncLog("ģ���ļ��쳣��Ŀǰ��֧��onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}


	Log::WriteAsyncLog("��ʼ������λ���!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	std::vector<FinsObject> detectionsFil;  // �����
	bool findEmpty = false;
	bool findLevel = false;
	bool findFull = false;
	for (int i = 0; i < outInfo.locate.details.size(); i++)
	{

		if (outInfo.locate.details[i].className == "num_0")
		{
			findEmpty = true;
		}
		else if (outInfo.locate.details[i].className == "num_1")
		{
			detectionsFil.push_back(outInfo.locate.details[i]);
		}
		else if (outInfo.locate.details[i].className == "num_2")
		{
			findFull = true;
		}
	}

	if (findEmpty)
	{
		if (m_params.saveTrain)
		{
			COM->CreateDir("D:/TRAIN_DATA_2/LEVEL/LOCATE/NL");
			writeImageXml("D:/TRAIN_DATA_2/LEVEL/LOCATE/NL", outInfo.system.startTime, m_img, outInfo.system.cameraId, outInfo.system.jobId, outInfo.locate.details);
		}

		outInfo.status.errorMessage = "��ƿ!";
		Log::WriteAsyncLog("��ƿ!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = LEVEL_RETURN_NO_LEVEL;
		return;
	}
	if (findFull)
	{
		if (m_params.saveTrain)
		{
			COM->CreateDir("D:/TRAIN_DATA_2/LEVEL/LOCATE/NCT");
			writeImageXml("D:/TRAIN_DATA_2/LEVEL/LOCATE/NCT", outInfo.system.startTime, m_img, outInfo.system.cameraId, outInfo.system.jobId, outInfo.locate.details);
		}

		outInfo.status.errorMessage = "��ƿ!";
		Log::WriteAsyncLog("��ƿ!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = LEVEL_RETURN_NO_LEVEL;
		return;
	}
	if (detectionsFil.empty()) {
		if (m_params.saveTrain)
		{
			COM->CreateDir("D:/TRAIN_DATA_2/LEVEL/LOCATE/OUT");
			writeImageXml("D:/TRAIN_DATA_2/LEVEL/LOCATE/OUT", outInfo.system.startTime, m_img, outInfo.system.cameraId, outInfo.system.jobId, outInfo.locate.details);
		}

		outInfo.status.errorMessage = "��Ŀ��!";
		Log::WriteAsyncLog("��Ŀ��!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = LEVEL_RETURN_OUT;
		return;
	}
	else if (detectionsFil.size() > 1)
	{
		if (m_params.saveTrain)
		{
			COM->CreateDir("D:/TRAIN_DATA_2/LEVEL/LOCATE/MULTY");
			writeImageXml("D:/TRAIN_DATA_2/LEVEL/LOCATE/MULTY", outInfo.system.startTime, m_img, outInfo.system.cameraId, outInfo.system.jobId, outInfo.locate.details);
		}
		outInfo.status.errorMessage = "���ƿ��!";
		Log::WriteAsyncLog("���ƿ��!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = LEVEL_RETURN_OTHER;
		return;
	}
	else
	{
		if (m_params.saveTrain)
		{
			COM->CreateDir("D:/TRAIN_DATA_2/LEVEL/LOCATE/OK");
			writeImageXml("D:/TRAIN_DATA_2/LEVEL/LOCATE/OK", outInfo.system.startTime, m_img, outInfo.system.cameraId, outInfo.system.jobId, outInfo.locate.details);
		}
		outInfo.geometry.levelRect = detectionsFil[0].box;
		Log::WriteAsyncLog("��λҺλ�ɹ�!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}
}



void InspLevel::Level_CheckLevel(InspLevelOut& outInfo) {
	if (outInfo.status.statusCode != LEVEL_RETURN_OK) {
		Log::WriteAsyncLog("����Һλ���!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	cv::Mat dst;
	cv::medianBlur(outInfo.images.roi, dst, m_params.blurThresh*2 + 1);
	DAS->DAS_Img(dst, outInfo.paths.intermediateImagesDir + "3.1.0.medianBlur.jpg", m_params.saveDebugImage);

	Canny(dst, outInfo.images.cannyImg, m_params.edgeThresh, m_params.edgeThresh >> 1);
	DAS->DAS_Img(outInfo.images.cannyImg, outInfo.paths.intermediateImagesDir + "3.1.1.imgCanny.jpg", m_params.saveDebugImage);


	//ȥ������(���ݡ����۵�)
	int minW = 20;
	int minH = 5;
	int maxW = outInfo.images.cannyImg.cols;
	int maxH = outInfo.images.cannyImg.rows;
	int minPoints = 20;
	int maxPoints = outInfo.images.cannyImg.cols * outInfo.images.cannyImg.rows;
	cv::Mat imgCannyFilter;
	ANA->RemoveEdgeBySize(outInfo.images.cannyImg, imgCannyFilter, minW, minH, maxW, maxH, minPoints, maxPoints);
	DAS->DAS_Img(outInfo.images.cannyImg, outInfo.paths.intermediateImagesDir + "3.1.2.imgCannyFilter.jpg", m_params.saveDebugImage);

	//����ˮƽ��Ե
	cv::Mat imgCannyFilterX;
	ANA->RemoveEdgeByDir(outInfo.images.roi, imgCannyFilter, false, m_params.angleThresh, imgCannyFilterX);
	DAS->DAS_Img(imgCannyFilterX, outInfo.paths.intermediateImagesDir + "3.1.3.imgCannyFilterX.jpg", m_params.saveDebugImage);

	//ˮƽͶӰ��λҺλ
	int smoothBnd = MIN(
		15, m_params.roiRect.width *
		tan(m_params.angleThresh * CV_PI / 180.0) *
		0.1);
	std::vector<int> projectHor;
	std::vector<float> projectHorSmooth;
	ANA->Project(imgCannyFilterX, projectHor, false);
	DAS->DAS_ProjectHor(projectHor, outInfo.paths.intermediateImagesDir + "3.2.1.projectHor.jpg", m_params.saveDebugImage);

	float peak;
	float valley;
	ANA->ProjectSmooth(projectHor, projectHorSmooth, smoothBnd, peak, valley);
	DAS->DAS_ProjectHor(projectHorSmooth, outInfo.paths.intermediateImagesDir + "3.2.2.projectHor.jpg", m_params.saveDebugImage);
	if (peak < m_params.projectThresh) {
		outInfo.status.errorMessage = "��Һλ!";
		Log::WriteAsyncLog("��Һλ!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = LEVEL_RETURN_NO_LEVEL;
		return;
	}

	//�ҶȲ�ֵ����
	std::vector<int> peakPosList;
	std::vector<int> peakPosSize;
	ANA->FindPeak(projectHorSmooth, peakPosList, peakPosSize,
		m_params.projectThresh);
	if (peakPosList.empty()) {
		outInfo.status.errorMessage = "��Һλ!";
		Log::WriteAsyncLog("��Һλ!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = LEVEL_RETURN_NO_LEVEL;
		return;
	}
	else {
		std::vector<double> grayDif;
		cv::Rect maxRect(0, 0, m_params.roiRect.width, m_params.roiRect.height);

		//int bnd = 50;  //���¼�ⷶΧ
		//for (int i = 0; i < peakPosList.size(); i++) {
		//	cv::Rect upRect(0, peakPosList[i] - bnd, m_params.roiRect.width, bnd);
		//	cv::Rect downRect(0, peakPosList[i], m_params.roiRect.width, bnd);
		//	cv::Rect upRectRefine;
		//	cv::Rect downRectRefine;
		//	ANA->RestrainRect(maxRect, upRect, upRectRefine);
		//	ANA->RestrainRect(maxRect, downRect, downRectRefine);

		//	double stdDev;
		//	double upAveGrayValue;
		//	CAL->CALC_MeanStdDev(outInfo.images.roi, upRectRefine, upAveGrayValue, stdDev);
		//	double downAveGrayValue;
		//	CAL->CALC_MeanStdDev(outInfo.images.roi, downRectRefine, downAveGrayValue, stdDev);
		//	double updownDis = upAveGrayValue - downAveGrayValue;
		//	grayDif.push_back(updownDis);
		//}
		int bnd = 50;  //���¼�ⷶΧ
		for (int i = 0; i < peakPosList.size(); i++) {
			Rect upRect(0, peakPosList[i] - bnd, m_params.roiRect.width, bnd);
			Rect downRect(0, peakPosList[i], m_params.roiRect.width, bnd);
			Rect upRectRefine;
			Rect downRectRefine;
			bool rvup = ANA->RestrainRect(maxRect, upRect, upRectRefine);
			bool rvdown = ANA->RestrainRect(maxRect, downRect, downRectRefine);
			if (!rvup || !rvdown)
			{
				continue;
			}
			double stdDev;
			double upAveGrayValue;
			CAL->CALC_MeanStdDev(outInfo.images.roi, upRectRefine, upAveGrayValue, stdDev);
			double downAveGrayValue;
			CAL->CALC_MeanStdDev(outInfo.images.roi, downRectRefine, downAveGrayValue, stdDev);
			double updownDis = upAveGrayValue - downAveGrayValue;
			grayDif.push_back(updownDis);
		}

		DAS->DAS_PosAndScore(outInfo.images.roi, peakPosList, grayDif, outInfo.paths.intermediateImagesDir + "3.3.1.peakPosList.jpg", m_params.saveDebugImage);

		int maxScore = 0;
		bool findLevel = false;
		int maxGray = 0;
		for (int i = 0; i < grayDif.size(); i++) {
			if (peakPosSize[i] > outInfo.geometry.project && 
				grayDif[i] > m_params.grayDis&&
				grayDif[i] > maxGray) {
				outInfo.geometry.grayDis = grayDif[i];
				outInfo.geometry.levelY = peakPosList[i] + m_params.roiRect.y;
				outInfo.geometry.project = peakPosSize[i];
				findLevel = true;
				maxGray = outInfo.geometry.grayDis;
			}
		}

		if (!findLevel) {
			outInfo.status.errorMessage = "��Һλ!";
			Log::WriteAsyncLog("��Һλ!", ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = LEVEL_RETURN_NO_LEVEL;
			return;
		}
		else {
			Log::WriteAsyncLog("Level Y = ", ERR, outInfo.paths.logDirectory, m_params.saveLogTxt, outInfo.geometry.levelY);
			Log::WriteAsyncLog("grayDis = ", ERR, outInfo.paths.logDirectory, m_params.saveLogTxt, outInfo.geometry.grayDis);
			Log::WriteAsyncLog("project = ", ERR, outInfo.paths.logDirectory, m_params.saveLogTxt, outInfo.geometry.project);
		}

		if (outInfo.geometry.levelY > 0) {
			if (outInfo.geometry.levelY > m_params.maxY) {
				outInfo.status.errorMessage = "��Һλ!";
				Log::WriteAsyncLog("��Һλ!", ERR, outInfo.paths.logFile, true);
				outInfo.status.statusCode = LEVEL_RETURN_LOW_LEVEL;
				return;
			}
			else if (outInfo.geometry.levelY < m_params.minY) {
				outInfo.status.errorMessage = "��Һλ!";
				Log::WriteAsyncLog("��Һλ!", ERR, outInfo.paths.logFile, true);
				outInfo.status.statusCode = LEVEL_RETURN_HIGH_LEVEL;
				return;
			}
		}
	}

}

void InspLevel::Level_DrawResult(InspLevelOut& outInfo) {
	Log::WriteAsyncLog("��ʼ���ƽ��!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

	rectangle(outInfo.images.outputImg, m_params.roiRect, Colors::YELLOW, 1, cv::LINE_AA);


	cv::line(outInfo.images.outputImg,
		cv::Point(0, m_params.minY),
		cv::Point(outInfo.images.outputImg.cols - 1, m_params.minY),
		Colors::BLUE, 3, 8, 0);
	cv::line(outInfo.images.outputImg,
		cv::Point(0, m_params.maxY),
		cv::Point(outInfo.images.outputImg.cols - 1, m_params.maxY),
		Colors::BLUE, 3, 8, 0);

	if (outInfo.geometry.levelY > 0)
	{
		if (outInfo.status.statusCode == LEVEL_RETURN_OK)
		{
			cv::line(outInfo.images.outputImg,
				cv::Point(0, outInfo.geometry.levelY),
				cv::Point(outInfo.images.outputImg.cols - 1, outInfo.geometry.levelY),
				Colors::GREEN, 3, 8, 0);
		}
		else
		{
			cv::line(outInfo.images.outputImg,
				cv::Point(0, outInfo.geometry.levelY),
				cv::Point(outInfo.images.outputImg.cols - 1, outInfo.geometry.levelY),
				Colors::RED, 3, 8, 0);
		}
	}



	std::string rv = "ID = " + std::to_string(outInfo.system.jobId) + ", " + "RV = " + std::to_string(outInfo.status.statusCode) + ", " + outInfo.status.errorMessage;
	if (outInfo.status.statusCode == LEVEL_RETURN_OK) {
		putTextZH(outInfo.images.outputImg, rv.c_str(), cv::Point(15, 30), Colors::GREEN, 45, FW_BOLD);
	}
	else {
		putTextZH(outInfo.images.outputImg, rv.c_str(), cv::Point(15, 30), Colors::RED, 45, FW_BOLD);
	}



	putTextZH(outInfo.images.outputImg, ("Һλ�߶� = " + std::to_string(outInfo.geometry.levelY)).c_str(), cv::Point(15, 120), Colors::GREEN, 35, FW_BOLD);
	putTextZH(outInfo.images.outputImg, ("�ҶȲ� = " + std::to_string(outInfo.geometry.grayDis)).c_str(), cv::Point(15, 180), Colors::GREEN, 35, FW_BOLD);
	putTextZH(outInfo.images.outputImg, ("�÷� = " + std::to_string(outInfo.geometry.project)).c_str(), cv::Point(15, 240), Colors::GREEN, 35, FW_BOLD);


	DAS->DAS_Img(outInfo.images.outputImg, outInfo.paths.intermediateImagesDir + "10.outputImg.jpg", m_params.saveDebugImage);
}

int InspLevel::Level_Main(InspLevelOut& outInfo) {


	try {
		double time0 = static_cast<double>(cv::getTickCount());
		if (outInfo.status.statusCode == LEVEL_RETURN_OK)
		{
			Log::WriteAsyncLog("Level_Main!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

			// ��1��:��λҺλ
			if (CheckTimeout())
			{
				Log::WriteAsyncLog("��ʱ!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return LEVEL_RETURN_TIMEOUT;
			}
			Level_SetROI(outInfo);

			// ��2��:��λҺλ
			if (CheckTimeout())
			{
				Log::WriteAsyncLog("��ʱ!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return LEVEL_RETURN_TIMEOUT;
			}
			//Level_LocateLevel(outInfo);

			// ��3��:Һλ���
			if (CheckTimeout())
			{
				Log::WriteAsyncLog("��ʱ!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return LEVEL_RETURN_TIMEOUT;
			}
			Level_CheckLevel(outInfo);

			// ��4��:���ƽ��
			if (CheckTimeout())
			{
				Log::WriteAsyncLog("��ʱ!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
				return LEVEL_RETURN_TIMEOUT;
			}
		}

		Level_DrawResult(outInfo);


		if (outInfo.status.statusCode == LEVEL_RETURN_OK) {
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
		Log::WriteAsyncLog("�㷨��ʱ��", INFO, outInfo.paths.logFile, m_params.saveLogTxt, time0);
	}
	catch (const std::exception& e) {
		std::cerr << "[ERROR] Inference failed: " << e.what() << std::endl;
		return LEVEL_RETURN_ALGO_ERR;
	}

	return outInfo.status.statusCode;
}