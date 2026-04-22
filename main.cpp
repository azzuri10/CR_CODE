#include "InspPressCap.h"
#include "InspBottleNum.h"
#include "InspBottleNeck.h"
#include "InspSew.h"
#include "InspBoxBag.h"
#include "InspCode.h"
#include "DllExtern.h"
#include <filesystem>
#include "HeaderDefine.h"

void rotateImage(cv::Mat src, cv::Mat dst, int degree) {
	switch (degree) {
	case 90: // 顺时针 90°
		transpose(src, dst); // 转置（行列互换）
		cv::flip(dst, dst, 1);   // 沿 Y 轴镜像
		break;
	case 180: // 旋转 180°
		cv::flip(src, dst, -1);  // 同时沿 X 和 Y 轴镜像
		break;
	case 270: // 顺时针 270°（逆时针 90°）
		transpose(src, dst);
		cv::flip(dst, dst, 0);   // 沿 X 轴镜像
		break;
	default:
		dst = src.clone();   // 无效角度返回原图
	}
}


int main() {

	int cameraId = 0;
	int isLoadConfig = 1;
	int waitTime = 0;
	int isSaveResImg = 0;
	string configPath = ProjectConstants::CONFIG_PATH;
	//check imgs
	std::string CHECK_TYPE = "0";
	std::string type_name;
	std::cout << "******请输测试类型对应数字, 回车生效******" << std::endl;
	std::cout << "喷码 1" << std::endl;
	std::cout << "瓶口 2" << std::endl;
	std::cout << "压盖 3" << std::endl;
	std::cout << "液位 4" << std::endl;
	std::cout << "版面 5" << std::endl;
	std::cout << "提手塑膜 6" << std::endl;
	std::cout << "装箱点数 7" << std::endl;
	std::cout << "缝线口 8" << std::endl;
	std::cout << "缝线底 9" << std::endl;
	std::cout << "汽车配件 10" << std::endl;
	std::cout << "漏袋 11" << std::endl << std::endl;
	bool isRight = true;
	std::cout << "测试类型: ";
	std::cin >> CHECK_TYPE;
	while (isRight)
	{
		if (atoi(CHECK_TYPE.c_str()) > 13 || atoi(CHECK_TYPE.c_str()) < 0)
		{
			std::cout << "测试类型选择错误！请重新选择！" << "\n" << std::endl;
			std::cout << "测试类型: ";
			std::cin >> CHECK_TYPE;
		}
		else
		{
			isRight = false;
			if (atoi(CHECK_TYPE.c_str()) == 0)
			{
				type_name = "测试";
			}
			else if (atoi(CHECK_TYPE.c_str()) == 1)
			{
				type_name = "喷码";
			}
			else if (atoi(CHECK_TYPE.c_str()) == 2)
			{
				type_name = "瓶口";
			}
			else if (atoi(CHECK_TYPE.c_str()) == 3)
			{
				type_name = "压盖";
			}
			else if (atoi(CHECK_TYPE.c_str()) == 4)
			{
				type_name = "液位";
			}
			else if (atoi(CHECK_TYPE.c_str()) == 5)
			{
				type_name = "版面";
			}
			else if (atoi(CHECK_TYPE.c_str()) == 6)
			{
				type_name = "提手塑膜";
			}
			else if (atoi(CHECK_TYPE.c_str()) == 7)
			{
				type_name = "装箱点数";
			}
			else if (atoi(CHECK_TYPE.c_str()) == 8)
			{
				type_name = "缝线口";
			}
			else if (atoi(CHECK_TYPE.c_str()) == 9)
			{
				type_name = "缝线底";
			}
			else if (atoi(CHECK_TYPE.c_str()) == 10)
			{
				type_name = "汽车配件";
			}
			else if (atoi(CHECK_TYPE.c_str()) == 11)
			{
				type_name = "漏袋";
			}
			std::cout << "当前检测类型:  " << type_name << std::endl << std::endl;
		}
	}

	std::cout << "******请输相机ID(0,1,2,3，4,5,6,7,8,9), 回车生效******" << std::endl;
	std::cout << "相机ID: ";
	std::cin >> cameraId;
	std::cout << std::endl;

	std::cout << "******是否等待看图（0：否； 1:是）, 回车生效******" << std::endl;
	std::cout << "等待看图: ";
	std::cin >> waitTime;
	std::cout << std::endl;
	if (waitTime == 0)
	{
		waitTime = 1;
	}
	else
	{
		waitTime = 0;
	}


	std::cout << "******请输对应图像文件夹路径, 回车生效******" << std::endl;
	bool isLoadImgs = true;
	Common* COM = new Common;
	std::string IMG_PATH;
	std::cout << "图像文件夹路径: ";
	//std::cin >> IMG_PATH;
	getline(std::cin, IMG_PATH);
	getline(std::cin, IMG_PATH);
	//std::vector<cv::Mat> imgs = COM->ReadImage(IMG_PATH, 1);
	std::vector<std::string> fn;
	cv::glob(IMG_PATH, fn, false);

	while (isLoadImgs)
	{
		if (fn.empty())
		{
			std::cout << "未找到图片！请重新输入文件夹路径！" << std::endl;
			std::cout << "图像文件夹路径: ";
			std::cin >> IMG_PATH;
			//imgs = COM->ReadImage(IMG_PATH, 1);
			cv::glob(IMG_PATH, fn, false);
		}
		else
		{
			std::cout << "文件总数： " << fn.size() << std::endl << std::endl;
			isLoadImgs = false;
		}
	}
	delete COM;


	AnalyseMat* ANA = new AnalyseMat;
	Log* LOG = new Log;

	std::vector<cv::Mat> outputImages(fn.size());
	if (type_name == "测试")
	{
		/*for (int i = 0; i < fn.size(); i++) {
			cv::Mat img = cv::imread(fn[i], 1);
			cv::namedWindow("src", cv::WINDOW_NORMAL);
			cv::imshow("src", img);
			rotateImage(img, img, 90);
			cv::namedWindow("dst", cv::WINDOW_NORMAL);
			cv::imshow("dst", img);
			cv::waitKey(waitTime);
		}*/
	}
	else if (type_name == "压盖")
	{

		for (int i = 0; i < fn.size(); i++) {
			cv::Mat img = cv::imread(fn[i], 1);
			if (img.empty())
			{
				std::cout << "发现异常文件!   " << fn[i] << std::endl;
				Log::WriteAsyncLog("发现异常文件!", INFO, "D://aoi_test", 1);
				Log::WriteAsyncLog(fn[i], INFO, "D://aoi_test", 1);
				continue;
			}

			auto start = std::chrono::high_resolution_clock::now();
			double time0 = static_cast<double>(cv::getTickCount());
			InspPressCapOut outInfo;
			outInfo.system.cameraId = cameraId;
			outInfo.system.jobId = i;
			char bufLog[100];
			sprintf(bufLog, "PressCap/camera_%d/", outInfo.system.cameraId);
			char bufConfig[100];
			sprintf(bufConfig, "/InspPressCapConfig_%d.txt", outInfo.system.cameraId);
			outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
			outInfo.paths.intermediateImagesDir =
				ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
			outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
			outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
			outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
			outInfo.paths.configFile = configPath + std::string(bufConfig);
			outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

			outInfo.status.errorMessage = "OK";
			outInfo.status.statusCode = PRESSCAP_RETURN_OK;
			outInfo.status.logs.reserve(100);

			InspPressCap* pInspPressCap = new InspPressCap(ProjectConstants::CONFIG_PATH, img, cameraId, i, true, 1000000, outInfo);
			pInspPressCap->SetStartTimePoint(start);
			int rv = pInspPressCap->PressCap_Main(outInfo);
			time0 = ((double)cv::getTickCount() - time0) / cv::getTickFrequency() * 1000;

			std::cout << "ID = " << i << "，RV = " << outInfo.status.statusCode << "，算法耗时：" << time0 << "ms" << std::endl << std::endl;
			cv::namedWindow("img", cv::WINDOW_NORMAL);
			cv::imshow("img", outInfo.images.outputImg.mat());
			cv::waitKey(waitTime);
			delete pInspPressCap;

		}
	}
	else if (type_name == "液位")
	{
		for (int i = 0; i < fn.size(); i++) {
			cv::Mat img = cv::imread(fn[i], 0);
			if (img.empty())
			{
				std::cout << "发现异常文件!   " << fn[i] << std::endl;
				Log::WriteAsyncLog("发现异常文件!", INFO, "D://aoi_test", 1);
				Log::WriteAsyncLog(fn[i], INFO, "D://aoi_test", 1);
				continue;
			}

			double time0 = static_cast<double>(cv::getTickCount());
			InspLevelOut outInfo;
			outInfo.status.statusCode = LEVEL_RETURN_OK;
			outInfo.status.logs.reserve(100);


			if (img.channels() == 1)
			{
				cv::Mat imgColor;
				cv::cvtColor(img, imgColor, cv::COLOR_GRAY2BGR);
				outInfo.images.outputImg = imgColor.clone();

			}
			else if (img.channels() == 3)
			{
				cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
				outInfo.images.outputImg = img.clone();
			}

			outInfo.system.jobId = i;
			outInfo.system.cameraId = cameraId;
			std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;

			char bufLog[100];
			sprintf(bufLog, "Level/camera_%d/", outInfo.system.cameraId);
			char bufConfig[100];
			sprintf(bufConfig, "/InspLevelConfig_%d.txt", outInfo.system.cameraId);
			outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
			outInfo.paths.intermediateImagesDir =
				ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
			outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
			outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
			outInfo.paths.configFile = configPath + std::string(bufConfig);
			outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";


			InspLevel* pInspLevel = new InspLevel(ProjectConstants::CONFIG_PATH, img, cameraId, i, true, 100000, outInfo);
			int rv = pInspLevel->Level_Main(outInfo);
			time0 = ((double)cv::getTickCount() - time0) / cv::getTickFrequency() * 1000;

			std::cout << "ID = " << i << "，RV = " << outInfo.status.statusCode << "，算法耗时：" << time0 << "ms" << std::endl << std::endl;
			cv::namedWindow("img", cv::WINDOW_NORMAL);
			cv::imshow("img", outInfo.images.outputImg);
			cv::waitKey(waitTime);
			delete pInspLevel;

		}
	}

	else if (type_name == "提手塑膜")
	{
		for (int i = 0; i < fn.size(); i++) {
			cv::Mat img = cv::imread(fn[i], 1);
			if (img.empty())
			{
				std::cout << "发现异常文件!   " << fn[i] << std::endl;
				Log::WriteAsyncLog("发现异常文件!", INFO, "D://aoi_test", 1);
				Log::WriteAsyncLog(fn[i], INFO, "D://aoi_test", 1);
				continue;
			}

			auto start = std::chrono::high_resolution_clock::now();
			double time0 = static_cast<double>(cv::getTickCount());
			InspHandleOut outInfo;
			outInfo.system.cameraId = cameraId;
			outInfo.system.jobId = i;
			char bufLog[100];
			sprintf(bufLog, "Handle/camera_%d/", outInfo.system.cameraId);
			char bufConfig[100];
			sprintf(bufConfig, "/InspHandleConfig_%d.txt", outInfo.system.cameraId);
			outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
			outInfo.paths.intermediateImagesDir =
				ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
			outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
			outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
			outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
			outInfo.paths.configFile = configPath + std::string(bufConfig);
			outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

			outInfo.status.errorMessage = "OK";
			outInfo.status.statusCode = HANDLE_RETURN_OK;
			outInfo.status.logs.reserve(100);

			InspHandle* pInspHandle = new InspHandle(ProjectConstants::CONFIG_PATH, img, cameraId, i, true, 1000000, outInfo);
			pInspHandle->SetStartTimePoint(start);
			int rv = pInspHandle->Handle_Main(outInfo);
			time0 = ((double)cv::getTickCount() - time0) / cv::getTickFrequency() * 1000;

			std::cout << "ID = " << i << "，RV = " << outInfo.status.statusCode << "，算法耗时：" << time0 << "ms" << std::endl << std::endl;
			cv::namedWindow("img", cv::WINDOW_NORMAL);
			cv::imshow("img", outInfo.images.outputImg.mat());
			cv::waitKey(waitTime);
			delete pInspHandle;

		}
	}
	else if (type_name == "版面")
	{

		for (int i = 0; i < fn.size(); i++) {
			cv::Mat img = cv::imread(fn[i], 0);
			if (img.empty())
			{
				std::cout << "发现异常文件!   " << fn[i] << std::endl;
				Log::WriteAsyncLog("发现异常文件!", INFO, "D://aoi_test", 1);
				Log::WriteAsyncLog(fn[i], INFO, "D://aoi_test", 1);
				continue;
			}

			auto start = std::chrono::high_resolution_clock::now();
			double time0 = static_cast<double>(cv::getTickCount());
			InspLabelAllOut outInfo;
			outInfo.system.cameraId = cameraId;
			outInfo.system.jobId = i;
			char bufLog[100];
			sprintf(bufLog, "LabelAll/camera_%d/", outInfo.system.cameraId);
			char bufConfig[100];
			sprintf(bufConfig, "/InspLabelAllConfig_%d.txt", outInfo.system.cameraId);
			outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
			outInfo.paths.intermediateImagesDir =
				ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
			outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
			outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
			outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
			outInfo.paths.configFile = configPath + std::string(bufConfig);
			outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

			outInfo.status.errorMessage = "OK";
			outInfo.status.statusCode = LABELALL_RETURN_OK;
			outInfo.status.logs.reserve(100);

			InspLabelAll* pInspLabelAll = new InspLabelAll(ProjectConstants::CONFIG_PATH, img, cameraId, i, true, 1000000, outInfo);
			pInspLabelAll->SetStartTimePoint(start);
			int rv = pInspLabelAll->LabelAll_Main(outInfo);
			time0 = ((double)cv::getTickCount() - time0) / cv::getTickFrequency() * 1000;

			std::cout << "ID = " << i << "，RV = " << outInfo.status.statusCode << "，算法耗时：" << time0 << "ms" << std::endl << std::endl;
			cv::namedWindow("img", cv::WINDOW_NORMAL);
			cv::imshow("img", outInfo.images.outputImg.mat());
			cv::waitKey(waitTime);
			delete pInspLabelAll;

		}

	}
	else if (type_name == "喷码")
	{

		for (int i = 0; i < fn.size(); i++) {
			cv::Mat img = cv::imread(fn[i], 1);
			if (img.empty())
			{
				std::cout << "发现异常文件!   " << fn[i] << std::endl;
				Log::WriteAsyncLog("发现异常文件!", INFO, "D://aoi_test", 1);
				Log::WriteAsyncLog(fn[i], INFO, "D://aoi_test", 1);
				continue;
			}

			auto start = std::chrono::high_resolution_clock::now();
			double time0 = static_cast<double>(cv::getTickCount());
			InspCodeOut outInfo;
			outInfo.system.cameraId = cameraId;
			outInfo.system.jobId = i;
			char bufLog[100];
			sprintf(bufLog, "Code/camera_%d/", outInfo.system.cameraId);
			char bufConfig[100];
			sprintf(bufConfig, "/InspCodeConfig_%d.txt", outInfo.system.cameraId);
			outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
			outInfo.paths.intermediateImagesDir =
				ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
			outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
			outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
			outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
			outInfo.paths.configFile = configPath + std::string(bufConfig);
			outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

			outInfo.status.errorMessage = "OK";
			outInfo.status.statusCode = CODE_RETURN_OK;
			outInfo.status.logs.reserve(100);

			InspCode* pInspCode = new InspCode(ProjectConstants::CONFIG_PATH, img, cameraId, i, true, 40000, outInfo);
			pInspCode->SetStartTimePoint(start);
			int rv = pInspCode->Code_Main(outInfo);
			time0 = ((double)cv::getTickCount() - time0) / cv::getTickFrequency() * 1000;

			std::cout << "ID = " << i << "，RV = " << outInfo.status.statusCode << "，算法耗时：" << time0 << "ms" << std::endl << std::endl;
			cv::namedWindow("outputImg", cv::WINDOW_NORMAL);
			cv::imshow("outputImg", outInfo.images.outputImg);
			cv::waitKey(waitTime);
			delete pInspCode;

		}

	}
	else if (type_name == "装箱点数")
	{

		for (int i = 0; i < fn.size(); i++) {
			cv::Mat img = cv::imread(fn[i], 1);
			if (img.empty())
			{
				std::cout << "发现异常文件!   " << fn[i] << std::endl;
				Log::WriteAsyncLog("发现异常文件!", INFO, "D://aoi_test", 1);
				Log::WriteAsyncLog(fn[i], INFO, "D://aoi_test", 1);
				continue;
			}

			auto start = std::chrono::high_resolution_clock::now();
			double time0 = static_cast<double>(cv::getTickCount());
			InspBottleNumOut outInfo;
			outInfo.system.cameraId = cameraId;
			outInfo.system.jobId = i;
			char bufLog[100];
			sprintf(bufLog, "BottleNum/camera_%d/", outInfo.system.cameraId);
			char bufConfig[100];
			sprintf(bufConfig, "/InspBottleNumConfig_%d.txt", outInfo.system.cameraId);
			outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
			outInfo.paths.intermediateImagesDir =
				ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
			outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
			outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
			outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
			outInfo.paths.configFile = configPath + std::string(bufConfig);
			outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

			outInfo.status.errorMessage = "OK";
			outInfo.status.statusCode = BOTTLENUM_RETURN_OK;
			outInfo.status.logs.reserve(100);

			InspBottleNum* pInspBottleNum = new InspBottleNum(ProjectConstants::CONFIG_PATH, img, cameraId, i, true, 1000000, outInfo);
			pInspBottleNum->SetStartTimePoint(start);
			int rv = pInspBottleNum->BottleNum_Main(outInfo);
			time0 = ((double)cv::getTickCount() - time0) / cv::getTickFrequency() * 1000;

			std::cout << "ID = " << i << "，RV = " << outInfo.status.statusCode << "，算法耗时：" << time0 << "ms" << std::endl << std::endl;
			cv::namedWindow("img", cv::WINDOW_NORMAL);
			cv::imshow("img", outInfo.images.outputImg.mat());
			cv::waitKey(waitTime);
			delete pInspBottleNum;

		}
	}

	else if (type_name == "瓶口")
	{

		for (int i = 0; i < fn.size(); i++) {
			cv::Mat img = cv::imread(fn[i], 0);
			if (img.empty())
			{
				std::cout << "发现异常文件!   " << fn[i] << std::endl;
				Log::WriteAsyncLog("发现异常文件!", INFO, "D://aoi_test", 1);
				Log::WriteAsyncLog(fn[i], INFO, "D://aoi_test", 1);
				continue;
			}

			auto start = std::chrono::high_resolution_clock::now();
			double time0 = static_cast<double>(cv::getTickCount());
			InspBottleNeckOut outInfo;
			outInfo.system.cameraId = cameraId;
			outInfo.system.jobId = i;
			char bufLog[100];
			sprintf(bufLog, "BottleNeck/camera_%d/", outInfo.system.cameraId);
			char bufConfig[100];
			sprintf(bufConfig, "/InspBottleNeckConfig_%d.txt", outInfo.system.cameraId);
			outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
			outInfo.paths.intermediateImagesDir =
				ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
			outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
			outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
			outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
			outInfo.paths.configFile = configPath + std::string(bufConfig);
			outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

			outInfo.status.errorMessage = "OK";
			outInfo.status.statusCode = BOTTLENECK_RETURN_OK;
			outInfo.status.logs.reserve(100);

			Log::WriteAsyncLog("********** Start Inspection JobID = ", INFO, outInfo.paths.logFile, true, outInfo.system.jobId, " ***********");
			InspBottleNeck* pInspBottleNeck = new InspBottleNeck(ProjectConstants::CONFIG_PATH, img, cameraId, i, true, 1000000, outInfo);
			pInspBottleNeck->SetStartTimePoint(start);
			int rv = pInspBottleNeck->BottleNeck_Main(outInfo);
			time0 = ((double)cv::getTickCount() - time0) / cv::getTickFrequency() * 1000;

			std::cout << "ID = " << i << "，RV = " << outInfo.status.statusCode << "，算法耗时：" << time0 << "ms" << std::endl << std::endl;
			cv::namedWindow("img", cv::WINDOW_NORMAL);
			cv::imshow("img", outInfo.images.outputImg);
			cv::waitKey(waitTime);
			delete pInspBottleNeck;

		}
	}
	else if (type_name == "缝线口")
	{

		for (int i = 0; i < fn.size(); i++) {
			cv::Mat img = cv::imread(fn[i], 0);
			if (img.empty())
			{
				std::cout << "发现异常文件!   " << fn[i] << std::endl;
				Log::WriteAsyncLog("发现异常文件!", INFO, "D://aoi_test", 1);
				Log::WriteAsyncLog(fn[i], INFO, "D://aoi_test", 1);
				continue;
			}

			auto start = std::chrono::high_resolution_clock::now();
			double time0 = static_cast<double>(cv::getTickCount());
			InspSewOut outInfo;
			outInfo.system.cameraId = cameraId;
			outInfo.system.jobId = 0;
			char bufLog[100];
			sprintf(bufLog, "Sew/camera_%d/", outInfo.system.cameraId);
			char bufConfig[100];
			sprintf(bufConfig, "/InspSewConfig_%d.txt", outInfo.system.cameraId);
			outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
			outInfo.paths.intermediateImagesDir =
				ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
			outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
			outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
			outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
			outInfo.paths.configFile = configPath + std::string(bufConfig);
			outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

			outInfo.status.errorMessage = "OK";
			outInfo.status.statusCode = SEW_RETURN_OK;
			outInfo.status.logs.reserve(100);

			InspSew* pInspSew = new InspSew(ProjectConstants::CONFIG_PATH, img, cameraId, i, true, 1000000, outInfo);
			pInspSew->SetStartTimePoint(start);
			int rv = pInspSew->Sew_Main(outInfo);
			time0 = ((double)cv::getTickCount() - time0) / cv::getTickFrequency() * 1000;

			std::cout << "ID = " << i << "，RV = " << outInfo.status.statusCode << "，算法耗时：" << time0 << "ms" << std::endl << std::endl;
			cv::namedWindow("img", cv::WINDOW_NORMAL);
			cv::imshow("img", outInfo.images.outputImg);
			cv::waitKey(waitTime);
			delete pInspSew;

		}
	}
	else if (type_name == "漏袋")
	{

	for (int i = 0; i < fn.size(); i++) {
		cv::Mat img = cv::imread(fn[i], 1);
		if (img.empty())
		{
			std::cout << "发现异常文件!   " << fn[i] << std::endl;
			Log::WriteAsyncLog("发现异常文件!", INFO, "D://aoi_test", 1);
			Log::WriteAsyncLog(fn[i], INFO, "D://aoi_test", 1);
			continue;
		}

		auto start = std::chrono::high_resolution_clock::now();
		double time0 = static_cast<double>(cv::getTickCount());
		InspBoxBagOut outInfo;
		outInfo.system.cameraId = cameraId;
		outInfo.system.jobId = 0;
		char bufLog[100];
		sprintf(bufLog, "BoxBag/camera_%d/", outInfo.system.cameraId);
		char bufConfig[100];
		sprintf(bufConfig, "/InspBoxBagConfig_%d.txt", outInfo.system.cameraId);
		outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
		outInfo.paths.intermediateImagesDir =
			ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
		outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
		outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
		outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
		outInfo.paths.configFile = configPath + std::string(bufConfig);
		outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

		outInfo.status.errorMessage = "OK";
		outInfo.status.statusCode = BOXBAG_RETURN_OK;
		outInfo.status.logs.reserve(100);

		InspBoxBag* pInspBoxBag = new InspBoxBag(ProjectConstants::CONFIG_PATH, img, cameraId, i, true, 1000000, outInfo);
		pInspBoxBag->SetStartTimePoint(start);
		int rv = pInspBoxBag->BoxBag_Main(outInfo);
		time0 = ((double)cv::getTickCount() - time0) / cv::getTickFrequency() * 1000;

		std::cout << "ID = " << i << "，RV = " << outInfo.status.statusCode << "，算法耗时：" << time0 << "ms" << std::endl << std::endl;
		cv::namedWindow("img", cv::WINDOW_NORMAL);
		cv::imshow("img", outInfo.images.outputImg);
		cv::waitKey(waitTime);
		delete pInspBoxBag;

	}
	}

	return 0;
}
