#include "InspLabelAll.h"
#include "InspLabelAllStruct.h"
#include "ModelManager.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <locale>
#include <fstream>
#include <chrono>
#include "InferenceWorker.h"
#include "Data.h"
#include "AnalyseMat.h"
#include "../XML/write_json.h"
#include "../XML/LoadJsonConfig.h"

using namespace std;
using namespace cv;

std::shared_mutex InspLabelAll::modelLoadMutex;
std::map<std::string, std::string> InspLabelAll::labelAllDetectionModelMap;
std::mutex InspLabelAll::templateMutex_;
std::unordered_map<int, InspLabelAll::TemplateCache> InspLabelAll::templateCache_;
std::map<int, InspLabelAllIn> InspLabelAll::cameraConfigMap;


// 构造函数:初始化时加载所有相关模型
InspLabelAll::InspLabelAll(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
	bool isLoadConfig, int timeOut, InspLabelAllOut& outInfo)
	: LOG(std::make_unique<Log>()),
	ANA(std::make_unique<AnalyseMat>()),
	COM(std::make_unique<Common>()),
	MF(std::make_unique<MatchFun>()),
	TXT(std::make_unique<TxtOperater>()),
	BAQ(std::make_unique<BarAndQR>()),
	DAS(std::make_unique<DrawAndShowImg>())
{
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.status.logs.reserve(100);
	if (outInfo.status.statusCode != LABELALL_RETURN_OK)
	{
		return;
	}
	// 输入参数初始化
	if (img.empty()) {
		outInfo.status.statusCode = LABELALL_RETURN_INPUT_PARA_ERR;
		outInfo.status.errorMessage = "输入图像为空!";
		LOG->WriteLog("输入图像为空!", ERR, outInfo.paths.logFile, true);
		return;
	}

	if (img.channels() == 1) {
		m_imgGray = img.clone();
		cv::cvtColor(img, m_img, cv::COLOR_GRAY2BGR);
		outInfo.images.outputImg.data = std::make_shared<cv::Mat>(m_img.clone());
		outInfo.images.outputImg.stageName = "初始化";
		outInfo.images.outputImg.description = "初始化";
	}
	else if (img.channels() == 3) {
		m_img = img.clone();
		cv::cvtColor(img, m_imgGray, cv::COLOR_BGR2GRAY);
		outInfo.images.outputImg.data = std::make_shared<cv::Mat>(m_img.clone());
	}

	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << outInfo.system.cameraId << "  jobId_" << outInfo.system.jobId << std::endl;

	char bufLog[100];
	sprintf(bufLog, "LabelAll/camera_%d/", outInfo.system.cameraId);
	char bufConfig[100];
	sprintf(bufConfig, "/InspLabelAllConfig_%d.txt", outInfo.system.cameraId);
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

		// 读取config
		bool rv_loadConfig = readParams(m_img, outInfo.paths.configFile, m_params, outInfo, outInfo.paths.logFile);
		if (!rv_loadConfig) {
			outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = outInfo.status.errorMessage;
			LOG->WriteLog(outInfo.status.errorMessage, ERR, outInfo.paths.logFile, true);
			return;
		}

		// 检测roi
		if (!ANA->JudgeRectIn(cv::Rect(0, 0, img.cols, img.rows), m_params.roiRect)) {
			outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "定位检测ROI设置超出图像范围!";
			LOG->WriteLog("定位检测ROI设置超出图像范围", ERR, outInfo.paths.logFile, true);
			return;
		}

		// 深度学习模型加载
		if (m_params.isCheckLocate) {
			if (!validateCameraModels(outInfo.system.cameraId)) {
				LOG->WriteLog("相机ID配置错误/模型文件缺失!", ERR, outInfo.paths.logFile, true);
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "相机ID配置错误/模型文件缺失!";
				return;
			}

			if (isLoadConfig || outInfo.system.jobId == 0 || !loadLabelAllConfigSuccess[outInfo.system.cameraId]) {
				bool loadModel = loadAllModels(outInfo, true);
				if (!loadModel) {
					outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "深度学习模型加载异常!";
					LOG->WriteLog(m_params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "---深度学习模型加载异常!");
					return;
				}
			}

			int load_rv = LoadConfigYOLO(m_params.locateThreshConfig, m_params.locatePara, m_params.locateClassName, outInfo.paths.logFile);
			if (load_rv == -1) {
				LOG->WriteLog(m_params.matchConfigFile, INFO, outInfo.paths.logFile, true, " -- 定位阈值文件缺失!");
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "定位阈值文件缺失!";
				loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}
			else if (load_rv == -2) {
				LOG->WriteLog(m_params.matchConfigFile, INFO, outInfo.paths.logFile, true, " -- 定位阈值文件格式错误!");
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "定位阈值文件格式错误!";
				loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}
			else if (load_rv == -3) {
				LOG->WriteLog(m_params.matchConfigFile, INFO, outInfo.paths.logFile, true, " -- 定位阈值配置为空!");
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "定位阈值配置为空!";
				loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}

		}

		// 模板匹配配置加载
		if (m_params.isCheckTemplate) {
			int load_rv = LoadConfigMatch(
				m_params.matchConfigFile,
				m_params.matchPara,
				outInfo.paths.logFile);
			if (load_rv == -1) {
				LOG->WriteLog(m_params.matchConfigFile, INFO, outInfo.paths.logFile, true, " -- 模板配置文件丢失!");
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "模板配置文件丢失!";
				loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}
			else if (load_rv == -2) {
				LOG->WriteLog(m_params.matchConfigFile, INFO, outInfo.paths.logFile, true, " -- 模板配置文件格式错误!");
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "模板配置文件格式错误!";
				loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}
			else if (load_rv == -3) {
				LOG->WriteLog(m_params.matchConfigFile, INFO, outInfo.paths.logFile, true, " -- 模板配置为空!");
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "模板配置为空!";
				loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}



			// 模板图像加载和特征提取
			if (isLoadConfig || !loadLabelAllConfigSuccess[outInfo.system.cameraId] || jobId == 0 || jobId == 1) {
				LOG->WriteLog("开始读取匹配模板!", INFO, outInfo.paths.logFile, true);

				for (int i = 0; i < m_params.matchPara.size(); i++) {
					MatchConfig& matchCfg = m_params.matchPara[i];
					m_params.matchPara[i].timeOut = 3000;

					if (matchCfg.matchType == 0 || matchCfg.matchType == 1) {
						// Halcon模板匹配 - 创建形状模型
						for (int j = 0; j < matchCfg.templatePaths.size(); j++) {
							try {
								// 读取图像
								HObject ho_Image;

								Mat templ = imread(matchCfg.templatePaths[j], 1);
								Rect roiCur = matchCfg.rois[j];
								if (ANA->IsRectOutOfBounds(roiCur, templ))
								{
									LOG->WriteLog(m_params.matchConfigFile, INFO, outInfo.paths.logFile, true, " -- 模版roi超出图像范围!");
									outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
									outInfo.status.errorMessage = "模版roi超出图像范围!";
									loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
									return;
								}


								ANA->Mat2HObject(templ(roiCur), ho_Image);

								matchCfg.templateMats.push_back(templ(roiCur));

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
									LOG->WriteLog(m_params.matchConfigFile, INFO, outInfo.paths.logFile, true, " -- 模板配置文件内容为空!");
									outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
									outInfo.status.errorMessage = "模板配置文件内容为空!";
									loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
									return;
								}

								// 获取模型轮廓
								HObject ho_ModelContours;
								HalconCpp::GetShapeModelContours(&ho_ModelContours, hv_ModelID, 1);
								matchCfg.labelAllTemplateHObjects.push_back(ho_ModelContours);
								matchCfg.labelAllTemplateHTuple.push_back(hv_ModelID);
								//matchCfg.templateMats = COM->ReadImages(matchCfg.templatePaths[j], 0);

								if (matchCfg.warp)
								{
									// 对整个模板图像进行变换
									cv::Mat templClone = templ.clone();

									// 原始ROI的四个角点在原图中的坐标
									std::vector<cv::Point2f> baseCorners = {
										cv::Point2f(roiCur.x, roiCur.y),                              // 左上
										cv::Point2f(roiCur.x + roiCur.width, roiCur.y),               // 右上
										cv::Point2f(roiCur.x + roiCur.width, roiCur.y + roiCur.height), // 右下
										cv::Point2f(roiCur.x, roiCur.y + roiCur.height)                // 左下
									};

									// 计算原始ROI的中心点
									cv::Point2f center(roiCur.x + roiCur.width / 2.0f,
										roiCur.y + roiCur.height / 2.0f);

									// 对每个warpRange配置应用变换
									for (const auto& warpParams : matchCfg.warpRange) {
										if (warpParams.size() < 4) {
											continue; // 跳过不完整的参数组
										}

										// 提取变换参数
										double h0 = warpParams[0]; // 左侧高度缩放
										double h1 = warpParams[1]; // 右侧高度缩放
										double w0 = warpParams[2]; // 上侧宽度缩放
										double w1 = warpParams[3]; // 下侧宽度缩放

										// 计算变换后的四个角点（保持中心点不变）
										std::vector<cv::Point2f> warpedCorners;

										// 左上角点
										float left0X = center.x - (roiCur.width * w0) / 2.0f;
										float left0Y = center.y - (roiCur.height * h0) / 2.0f;
										warpedCorners.push_back(cv::Point2f(left0X, left0Y));

										// 右上角点
										float right0X = center.x + (roiCur.width * w0) / 2.0f;
										float right0Y = center.y - (roiCur.height * h1) / 2.0f;
										warpedCorners.push_back(cv::Point2f(right0X, right0Y));

										// 右下角点
										float left1X = center.x + (roiCur.width * w1) / 2.0f;
										float left1Y = center.y + (roiCur.height * h1) / 2.0f;
										warpedCorners.push_back(cv::Point2f(left1X, left1Y));

										// 左下角点
										float right1X = center.x - (roiCur.width * w1) / 2.0f;
										float right1Y = center.y + (roiCur.height * h0) / 2.0f;
										warpedCorners.push_back(cv::Point2f(right1X, right1Y));

										// 计算透视变换矩阵
										cv::Mat H = cv::getPerspectiveTransform(baseCorners, warpedCorners);

										// 对整个模板图像应用透视变换
										cv::Mat warpedTempl;
										cv::warpPerspective(templClone, warpedTempl, H, templClone.size(),
											cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

										// 计算变换后的ROI位置
										// 应用相同的变换到原始ROI的四个角点
										std::vector<cv::Point2f> warpedCornersInTempl;
										cv::perspectiveTransform(baseCorners, warpedCornersInTempl, H);

										// 计算变换后的边界框
										float minX = FLT_MAX, minY = FLT_MAX, maxX = FLT_MIN, maxY = FLT_MIN;
										for (const auto& pt : warpedCornersInTempl) {
											minX = std::min(minX, pt.x);
											minY = std::min(minY, pt.y);
											maxX = std::max(maxX, pt.x);
											maxY = std::max(maxY, pt.y);
										}

										cv::Rect warpedRoi(
											static_cast<int>(minX),
											static_cast<int>(minY),
											static_cast<int>(maxX - minX),
											static_cast<int>(maxY - minY)
										);

										// 确保ROI不超出图像边界
										warpedRoi = warpedRoi & cv::Rect(0, 0, warpedTempl.cols, warpedTempl.rows);

										// 检查变换后的ROI是否有效
										if (warpedRoi.width <= 0 || warpedRoi.height <= 0) {
											LOG->WriteLog("变换后的ROI无效: 宽度=" + std::to_string(warpedRoi.width) +
												", 高度=" + std::to_string(warpedRoi.height),
												WARNING, outInfo.paths.logFile, true);
											continue;
										}

										// 从变换后的图像中提取ROI
										cv::Mat warpedRoiImage = warpedTempl(warpedRoi).clone();

										// 保存变换后的模板和ROI
										matchCfg.templateMats.push_back(warpedRoiImage);
										matchCfg.rois.push_back(warpedRoi);
										rectangle(warpedTempl, warpedRoi, Colors::WHITE, 3, cv::LINE_AA);
										/*cv::namedWindow("warpedTempl", cv::WINDOW_NORMAL);
										cv::imshow("warpedTempl", warpedTempl);
										cv::waitKey(0);*/

										ANA->Mat2HObject(warpedRoiImage, ho_Image);

										// 创建形状模型
										HTuple hv_ModelID1;
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
												&hv_ModelID1);
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
												&hv_ModelID1);
											break;

										default:
											LOG->WriteLog(m_params.matchConfigFile, INFO, outInfo.paths.logFile, true, " -- 模板配置文件内容为空!");
											outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
											outInfo.status.errorMessage = "模板配置文件内容为空!";
											loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
											return;
										}

										// 获取模型轮廓
										HObject ho_ModelContours1;
										HalconCpp::GetShapeModelContours(&ho_ModelContours1, hv_ModelID1, 1);
										matchCfg.labelAllTemplateHObjects.push_back(ho_ModelContours1);
										matchCfg.labelAllTemplateHTuple.push_back(hv_ModelID1);

									}

								}


							}
							catch (HException& e) {
								LOG->WriteLog("Halcon模型创建失败: " + string(e.ErrorMessage().Text()),
									ERR, outInfo.paths.logFile, true);
								outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
								outInfo.status.errorMessage = "模板模型创建失败!";
								return;
							}
						}
					}
					else if (matchCfg.matchType == 2) {
						// NCC匹配 - 直接加载模板图像
						for (int j = 0; j < matchCfg.templatePaths.size(); j++) {
							Mat templ = imread(matchCfg.templatePaths[j], IMREAD_GRAYSCALE);
							if (templ.empty()) {
								LOG->WriteLog(matchCfg.templatePaths[j], ERR, outInfo.paths.logFile, true, " -- 模板图像读取失败!");
								outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
								outInfo.status.errorMessage = "模板图像读取失败!";
								return;
							}

							// 检查ROI是否有效
							Rect roiCur = matchCfg.rois[j];
							if (ANA->IsRectOutOfBounds(roiCur, templ)) {
								LOG->WriteLog(m_params.matchConfigFile, INFO, outInfo.paths.logFile, true, " -- 模板ROI超出图像范围!");
								outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
								outInfo.status.errorMessage = "模板ROI超出图像范围!";
								loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
								return;
							}

							matchCfg.templateMats.push_back(templ(roiCur));
							LOG->WriteLog("模板[" + std::to_string(j) + "]加载成功，尺寸: " +
								std::to_string(templ.cols) + "x" + std::to_string(templ.rows),
								INFO, outInfo.paths.logFile, true);
						}
					}
					else if (matchCfg.matchType == 3) {
					/*Ptr<SIFT> detector = SIFT::create();
					detector = SIFT::create(0, 3, 0.01, 5, 1.6); */

					cv::Ptr<cv::SIFT> detector = MF->CreateSIFTDetectorFromHalconConfig(matchCfg);
					//Ptr<SIFT> detector = SIFT::create(
					//	0,                      // nfeatures: 保留所有特征点
					//	matchCfg.numLevels,     // nOctaveLayers: 使用Halcon的金字塔层数
					//	matchCfg.minContrast / 1000.0f, // contrastThreshold: 将Halcon的对比度转换为SIFT格式
					//	matchCfg.contrast / 10.0f,      // edgeThreshold: 将Halcon的对比度转换为边缘阈值
					//	1.6 + (matchCfg.greediness * 0.4) // sigma: 基于贪婪度调整模糊参数
					//);

					for (int j = 0; j < matchCfg.templatePaths.size(); j++) {
						Mat templ = imread(matchCfg.templatePaths[j], IMREAD_GRAYSCALE);
						if (templ.empty()) {
							LOG->WriteLog(matchCfg.templatePaths[j], ERR, outInfo.paths.logFile, true, " -- 模板图像读取失败!");
							outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
							outInfo.status.errorMessage = "模板图像读取失败!";
							return;
						}

						// 检查ROI是否有效
						Rect roiCur = matchCfg.rois[j];
						if (ANA->IsRectOutOfBounds(roiCur, templ)) {
							LOG->WriteLog(m_params.matchConfigFile, INFO, outInfo.paths.logFile, true, " -- 模板ROI超出图像范围!");
							outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
							outInfo.status.errorMessage = "模板ROI超出图像范围!";
							loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
							return;
						}

						// 提取ROI区域进行SIFT特征提取
						Mat roiTemplate = templ(roiCur);

						vector<KeyPoint> keypoints;
						Mat descriptors;
						detector->detectAndCompute(roiTemplate, noArray(), keypoints, descriptors);

						if (keypoints.size() <= 10) {
							LOG->WriteLog("增强后特征点不足，尝试使用原始图像", WARNING, outInfo.paths.logFile, true);
							detector->detectAndCompute(roiTemplate, noArray(), keypoints, descriptors);
						}

						// 如果仍然不足，尝试多尺度检测
						if (keypoints.size() <= 10) {
							keypoints = MF->MultiScaleFeatureDetection(roiTemplate);
							if (!keypoints.empty()) {
								detector->compute(roiTemplate, keypoints, descriptors);
							}
						}
						else {
							LOG->WriteLog("SIFT模板[" + std::to_string(j) + "]特征点数量: " +
								std::to_string(keypoints.size()), INFO, outInfo.paths.logFile, true);
						}

						// 存储整个模板图像和ROI区域的特征
						matchCfg.templateMats.push_back(templ);  // 存储完整模板用于显示
						matchCfg.roiTemplates.push_back(roiTemplate);  // 存储ROI区域用于匹配
						matchCfg.templateMatsKeypoints.push_back(keypoints);
						matchCfg.templateMatsDescriptors.push_back(descriptors);

						// 记录ROI信息
						matchCfg.templateRois.push_back(roiCur);
					}
					}
				}
			}
		}

		// 一维码配置加载
		if (m_params.isCheckBar) {
			int load_rv = LoadConfigBar(m_params.barConfigFile, m_params.barConfigs, outInfo.paths.logFile);
			if (load_rv == -1) {
				LOG->WriteLog(m_params.barConfigFile, INFO, outInfo.paths.logFile, true, " -- 一维码配置文件丢失!");
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "一维码配置文件丢失!";
				loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}
			else if (load_rv == -2) {
				LOG->WriteLog(m_params.barConfigFile, INFO, outInfo.paths.logFile, true, " -- 一维码配置文件格式错误!");
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "一维码配置文件格式错误!";
				loadLabelAllConfigSuccess[outInfo.system.cameraId] = false;
				return;
			}
			else if (load_rv == -3) {
				LOG->WriteLog(m_params.barConfigFile, INFO, outInfo.paths.logFile, true, " -- 一维码配置文件内容为空!");
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
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
						outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
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
						outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
						outInfo.status.errorMessage = "二维码类型选择错误，请参考二维码类型选项!";
						return;
					}

					m_params.barConfigs[i].targetTypes =
						(m_params.barConfigs[i].barType == "auto") ? qrTypes : std::vector<std::string>{ m_params.barConfigs[i].barType };
				}

				if (!ANA->JudgeRectIn(cv::Rect(0, 0, img.cols, img.rows), m_params.barConfigs[i].roi)) {
					outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "一维码/二维码检测roi设置超出图像范围!";
					LOG->WriteLog("一维码/二维码检测roi设置超出图像范围", ERR, outInfo.paths.logFile, true);
					return;
				}
				if (m_params.barConfigs[i].barType.empty())
				{
					outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "一维码/二维码检测种类选择错误!";
					LOG->WriteLog("一维码/二维码检测种类选择错误", ERR, outInfo.paths.logFile, true);
					return;
				}
				if (m_params.barConfigs[i].checkModel < 0 || m_params.barConfigs[i].checkModel > 1)
				{
					outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "一维码/二维码解析模式选择错误!";
					LOG->WriteLog("一维码/二维码解析模式选择错误", ERR, outInfo.paths.logFile, true);
					return;
				}
				if (m_params.barConfigs[i].info.empty())
				{
					outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "一维码/二维码信息输入空值！";
					LOG->WriteLog("一维码/二维码信息输入空值", ERR, outInfo.paths.logFile, true);
					return;
				}
				if (m_params.barConfigs[i].countRange[0] < m_params.barConfigs[i].countRange[1])
				{
					outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "一维码/二维码数量范围设置错误！";
					LOG->WriteLog("一维码/二维码数量范围设置错误", ERR, outInfo.paths.logFile, true);
					return;
				}
				/*if (m_params.barConfigs[i].modelPath.empty())
				{
					outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
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

	loadLabelAllConfigSuccess[outInfo.system.cameraId] = true;
	LOG->WriteLog("config成功加载！", INFO, outInfo.paths.logFile, true);

	if (m_params.saveDebugImage) {
		COM->CreateDir(outInfo.paths.intermediateImagesDir);
	}
	if (m_params.saveResultImage) {
		COM->CreateDir(outInfo.paths.resultsOKDir);
		COM->CreateDir(outInfo.paths.resultsNGDir);
	}
}

InspLabelAll::~InspLabelAll() {
	//// 释放Halcon模型资源
	//for (auto& cameraConfigs : m_matchConfig) {
	//    for (auto& matchCfg : cameraConfigs) {
	//        for (auto& model : matchCfg.m_labelAllTemplateHObjects) {
	//            try {
	//                ClearShapeModel(model);
	//            }
	//            catch (...) {
	//                // 忽略清理异常
	//            }
	//        }
	//    }
	//}
}


// 验证摄像头ID对应的模型配置是否存在
bool InspLabelAll::validateCameraModels(int cameraId) {
	std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
	return labelAllDetectionModelMap.count("labelAllDetection_" + std::to_string(cameraId));
}

// 加载所有模型到ModelManager
bool InspLabelAll::loadAllModels(InspLabelAllOut& outInfo, bool ini) {
	if (!ini) {
		Log::WriteAsyncLog("跳过模型加载!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return true;
	}

	const int cameraId = outInfo.system.cameraId;
	const cv::String key = std::to_string(cameraId);

	// 获取当前相机专用模型路径
	std::vector<std::string> cameraModelPaths;

	std::string detectionKey = "labelAllDetection_" + std::to_string(cameraId);
	if (auto it = labelAllDetectionModelMap.find(detectionKey); it != labelAllDetectionModelMap.end()) {
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
		Log::WriteAsyncLog("模型初始化完成！", INFO, outInfo.paths.logFile, true);

		return true;
	}
	catch (const std::exception& e) {
		Log::WriteAsyncLog("相机" + std::to_string(cameraId) + "模型加载异常: " + std::string(e.what()), ERR, outInfo.paths.logFile, true);
		return false;
	}
}

// 读取参数的函数
bool InspLabelAll::readParams(cv::Mat img, const std::string& filePath, InspLabelAllIn& params, InspLabelAllOut& outInfo, const std::string& fileName) {
	std::ifstream ifs(filePath.c_str());
	if (!ifs.is_open()) {
		outInfo.status.errorMessage = "config文件丢失!";
		Log::WriteAsyncLog("config文件丢失！", WARNING, outInfo.paths.logFile, true);
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
			outInfo.status.errorMessage = "参数缺失!";
			LOG->WriteLog(keyWord, WARNING, outInfo.paths.logFile, true, " 参数缺失！");
			return false;
		}

		std::string value = line.substr(findCommon + 1);
		value.erase(0, value.find_first_not_of(" "));
		value.erase(value.find_last_not_of(" ") + 1);

		// 是否存储中间图像(0:否  1:是)
		if (keyWord == "LABELALL_SAVE_DEBUG_IMAGE") {
			params.saveDebugImage = std::stoi(value);
		}
		else if (keyWord == "LABELALL_SAVE_RESULT_IMAGE") {
			params.saveResultImage = std::stoi(value);
		}
		else if (keyWord == "LABELALL_SAVE_LOG_TXT") {
			params.saveLogTxt = std::stoi(value);
		}
		else if (keyWord == "LABELALL_DRAW_RESULT") {
			params.drawResult = std::stoi(value);
		}
		else if (keyWord == "LABELALL_SAVE_TRAIN") {
			params.saveTrain = std::stoi(value);
		}
		else if (keyWord == "LABELALL_ROI_X") {
			params.roiRect.x = std::stoi(value);
			if (params.roiRect.x < 0 || params.roiRect.x > img.cols) {
				outInfo.status.errorMessage = "ROI_X: 超出图像范围!";
				LOG->WriteLog("ROI_X: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LABELALL_ROI_Y") {
			params.roiRect.y = std::stoi(value);
			if (params.roiRect.y < 0 || params.roiRect.y > img.rows) {
				outInfo.status.errorMessage = "ROI_Y: 超出图像范围!";
				LOG->WriteLog("ROI_Y: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LABELALL_ROI_W") {
			params.roiRect.width = std::stoi(value);
			if (params.roiRect.x + params.roiRect.width > img.cols) {
				outInfo.status.errorMessage = "ROI_X+ROI_W: 超出图像范围!";
				LOG->WriteLog("ROI_X+ROI_W: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LABELALL_ROI_H") {
			params.roiRect.height = std::stoi(value);
			if (params.roiRect.y + params.roiRect.height > img.rows) {
				outInfo.status.errorMessage = "ROI_Y+ROI_H: 超出图像范围!";
				LOG->WriteLog("ROI_Y+ROI_H: 超出图像范围！", ERR, outInfo.paths.logFile, true);
				return false;
			}
		}
		else if (keyWord == "LABELALL_IS_LOCATE") {
			params.isCheckLocate = std::stoi(value);
		}
		else if (keyWord == "LABELALL_HARDWARE_TYPE") {
			params.hardwareType = std::stoi(value);
		}
		else if (keyWord == "LABELALL_MODEL_TYPE") {
			params.modelType = std::stoi(value);
		}
		else if (keyWord == "LABELALL_LOCATE_WEIGHTS_FLIE") {
			std::lock_guard<std::shared_mutex> lock(modelLoadMutex);
			std::string camera = std::to_string(outInfo.system.cameraId);
			labelAllDetectionModelMap["labelAllDetection_" + camera] = value;
			params.locateWeightsFile = value;
			if (m_params.isCheckLocate)
			{
				if (!COM->FileExistsModern(params.locateWeightsFile)) {
					outInfo.status.errorMessage = "定位模型文件缺失!";
					LOG->WriteLog(params.locateWeightsFile, ERR, outInfo.paths.logFile, true, "--定位模型文件文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "LABELALL_LOCATE_CONFIG_FLIE") {
			params.locateThreshConfig = value;
			if (m_params.isCheckLocate)
			{
				if (!COM->FileExistsModern(params.locateThreshConfig)) {
					outInfo.status.errorMessage = "定位阈值文件缺失!";
					LOG->WriteLog(params.locateThreshConfig, ERR, outInfo.paths.logFile, true, "--定位阈值文件缺失！");
					return false;
				}
			}

		}
		else if (keyWord == "LABELALL_IS_CHECK_TEMPLATE") {
			params.isCheckTemplate = std::stoi(value);
		}
		else if (keyWord == "LABELALL_TEMPLATE_CONFIG_FILE") {
			params.matchConfigFile = value;
			if (m_params.isCheckTemplate)
			{
				if (!COM->FileExistsModern(params.matchConfigFile)) {
					outInfo.status.errorMessage = "模板匹配阈值文件缺失!";
					LOG->WriteLog(params.matchConfigFile, ERR, outInfo.paths.logFile, true, "--模板匹配阈值文件缺失！");
					return false;
				}
			}
		}
		else if (keyWord == "LABELALL_IS_CHECK_BAR") {
			params.isCheckBar = std::stoi(value);
		}
		else if (keyWord == "LABELALL_BAR_CONFIG_FILE") {
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

void InspLabelAll::LabelAll_SetROI(InspLabelAllOut& outInfo) {
	if (outInfo.status.statusCode != LABELALL_RETURN_OK) {
		LOG->WriteLog("跳过ROI区域获取!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}

	LOG->WriteLog("开始ROI区域获取!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

	outInfo.images.roi.data = std::make_shared<cv::Mat>(m_img(m_params.roiRect).clone());
	outInfo.images.roi.stageName = "LabelAll_Main";
	outInfo.images.roi.description = "ROI区域获取";

	outInfo.images.roiLog.data = std::make_shared<cv::Mat>(outInfo.images.outputImg.data->clone());
	outInfo.images.roiLog.stageName = "LabelAll_Main";
	outInfo.images.roiLog.description = "ROI_LOG绘制: " + std::to_string(m_params.saveDebugImage);

	if (m_params.saveDebugImage) {
		DAS->DAS_Rect(outInfo.images.roiLog.mat(), m_params.roiRect, outInfo.paths.intermediateImagesDir + "1.0.0.roiRect.jpg", true);
	}
}

void InspLabelAll::LabelAll_LocateLabel(InspLabelAllOut& outInfo) {
	if (outInfo.status.statusCode != LABELALL_RETURN_OK) {
		LOG->WriteLog("跳过标签定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}

	if (!m_params.isCheckLocate) {
		LOG->WriteLog("未开标签定位!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		LOG->WriteLog("开始标签定位!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	if (m_params.locateWeightsFile.find(".onnx") != std::string::npos)
	{
		outInfo.locate.details = InferenceWorker::Run(outInfo.system.cameraId, m_params.locateWeightsFile, m_params.locateClassName, outInfo.images.roi.mat(), 0.5, 0.3);
	}
	else
	{
		outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
		outInfo.status.errorMessage = "模型文件异常，目前仅支持onnx!";
		Log::WriteAsyncLog("模型文件异常，目前仅支持onnx!", ERR, outInfo.paths.logFile, true);

		return;
	}

	// 调整检测框到原图坐标
	for (auto& detail : outInfo.locate.details) {
		detail.box = ANA->AdjustROI(detail.box, *outInfo.images.roi.data);
		detail.box.x += m_params.roiRect.x;
		detail.box.y += m_params.roiRect.y;
	}

	LOG->WriteLog("开始分析定位结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

	for (int i = outInfo.locate.details.size() - 1; i >= 0; --i)
	{
		auto& locate = outInfo.locate.details[i];
		int paramIndex = -1; // 根据缺陷类别设置对应参数索引

		bool valid = true;
		if (locate.className == "无标")paramIndex = 1;
		else if (locate.className == "标签")   paramIndex = 0;
		else if (locate.className == "破损")  paramIndex = 2;
		else if (locate.className == "重标")	paramIndex = 3;
		else if (locate.className == "翘标")	paramIndex = 4;
		else if (locate.className == "褶皱")	paramIndex = 5;
		else if (locate.className == "一维码")	paramIndex = 6;
		else if (locate.className == "二维码")	paramIndex = 7;

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

	std::vector<FinsObject> detectionsFil;  // 过滤后的检测结果
	bool findNL = false, findOK = false, findMult = false;
	bool findBreak = false, findCurl = false, findFold = false;

	for (const auto& detail : outInfo.locate.details) {
		if (detail.className == "无标") findNL = true;
		else if (detail.className == "标签") detectionsFil.push_back(detail);
		else if (detail.className == "重标") findMult = true;
		else if (detail.className == "破损") findBreak = true;
		else if (detail.className == "翘标") findCurl = true;
		else if (detail.className == "褶皱") findFold = true;
		else if (detail.className == "一维码") findFold = true;
		else if (detail.className == "二维码") findFold = true;

	}

	// 处理各种检测情况
	if (findNL) {
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/无标");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/无标/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/无标/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);
		}

		outInfo.status.errorMessage = "定位-无标!";
		Log::WriteAsyncLog("定位-无标!", ERR, outInfo.paths.logFile, true);
		outInfo.status.statusCode = LABELALL_RETURN_LOCATE_NO_LABEL;
		return;
	}
	else if (findMult) {
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/重标");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/重标/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/重标/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);
		}
		outInfo.status.errorMessage = "定位-重标!";
		outInfo.status.statusCode = LABELALL_RETURN_LOCATE_MULT_LABEL;
		LOG->WriteLog("定位-重标!", ERR, outInfo.paths.logFile, true);
	}
	else if (findBreak) {
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/破损");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/破损/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/破损/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);
		}
		outInfo.status.errorMessage = "定位-破损!";
		outInfo.status.statusCode = LABELALL_RETURN_LOCATE_LABEL_BREAK;
		LOG->WriteLog("定位-破损!", ERR, outInfo.paths.logFile, true);
	}
	else if (findCurl) {
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/翘标");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/翘标/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/翘标/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);
		}
		outInfo.status.errorMessage = "定位-翘标!";
		outInfo.status.statusCode = LABELALL_RETURN_LOCATE_LABEL_CURL;
		LOG->WriteLog("定位-翘标!", ERR, outInfo.paths.logFile, true);
	}
	else if (findFold) {
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/褶皱");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/褶皱/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/褶皱/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);
		}
		outInfo.status.errorMessage = "定位-褶皱!";
		outInfo.status.statusCode = LABELALL_RETURN_LOCATE_LABEL_FOLD;
		LOG->WriteLog("定位-褶皱!", ERR, outInfo.paths.logFile, true);
	}
	else if (detectionsFil.empty()) {
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/无目标");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/无目标/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/无目标/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);
		}
		outInfo.status.errorMessage = "定位-无目标!";
		outInfo.status.statusCode = LABELALL_RETURN_LOCATE_NO_TARGET;
		LOG->WriteLog("定位-无目标!", ERR, outInfo.paths.logFile, true);
	}
	else if (detectionsFil.size() > 1) {
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/多个目标");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/多个目标/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/多个目标/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);
		}
		outInfo.status.errorMessage = "定位-多个目标!";
		outInfo.status.statusCode = LABELALL_RETURN_LOCATE_MULT_TARGET;
		LOG->WriteLog("定位-多个目标!", ERR, outInfo.paths.logFile, true);
	}
	else {
		if (m_params.saveTrain == 1 || m_params.saveTrain == 3)
		{
			outInfo.system.startTime = COM->time_t2string_with_ms();
			COM->CreateDir(outInfo.paths.trainDir + "LOCATE/OK");
			auto jsonData = generateXAnyLabelingJSON(
				outInfo.locate.details,
				outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg",
				m_img.rows,
				m_img.cols
			);
			saveJSONToFile(jsonData, outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".json");
			cv::imwrite(outInfo.paths.trainDir + "LOCATE/OK/" + outInfo.system.startTime + "_" + std::to_string(outInfo.system.cameraId) + "_" + std::to_string(outInfo.system.jobId) + ".jpg", m_img);
		}
		outInfo.locate.lableRect = detectionsFil[0].box;
		LOG->WriteLog("定位标签成功!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

}

void InspLabelAll::ProcessMatchResult(InspLabelAllOut& outInfo, int index, int rv,
	MatchResult& result, const std::string& matchTypeName) {

	std::string logPrefix = matchTypeName + "[" + std::to_string(index) + "]";

	switch (rv) {
	case 1: // 匹配成功
		LOG->WriteLog(logPrefix + "匹配成功! 分数: " + std::to_string(result.score) +
			", 角度: " + std::to_string(result.angle),
			INFO, outInfo.paths.logFile, true);
		break;

	case 2: // 匹配得分过低
		outInfo.status.errorMessage = matchTypeName + "-匹配得分过低!";
		outInfo.status.statusCode = LABELALL_RETURN_MATCH_ERR0;
		LOG->WriteLog(logPrefix + "匹配得分过低: " + std::to_string(result.score),
			ERR, outInfo.paths.logFile, true);
		break;

	case 3: // 歪斜
		outInfo.status.errorMessage = matchTypeName + "-歪斜!";
		outInfo.status.statusCode = LABELALL_RETURN_MATCH_ERR2;
		LOG->WriteLog(logPrefix + "歪斜: " + std::to_string(result.angle),
			ERR, outInfo.paths.logFile, true);
		break;

	case 5: // 水平偏移过大
		outInfo.status.errorMessage = matchTypeName + "-水平偏移过大!";
		outInfo.status.statusCode = LABELALL_RETURN_MATCH_ERR3;
		LOG->WriteLog(logPrefix + "水平偏移过大: " + std::to_string(result.shiftHor),
			ERR, outInfo.paths.logFile, true);
		break;

	case 6: // 垂直偏移过大
		outInfo.status.errorMessage = matchTypeName + "-垂直偏移过大!";
		outInfo.status.statusCode = LABELALL_RETURN_MATCH_ERR4;
		LOG->WriteLog(logPrefix + "垂直偏移过大: " + std::to_string(result.shiftVer),
			ERR, outInfo.paths.logFile, true);
		break;

	case 7: // 距离偏移过大
		outInfo.status.errorMessage = matchTypeName + "-距离偏移过大!";
		outInfo.status.statusCode = LABELALL_RETURN_MATCH_ERR5;
		LOG->WriteLog(logPrefix + "距离偏移过大: " + std::to_string(result.offset),
			ERR, outInfo.paths.logFile, true);
		break;

	case 8: // 超时
		outInfo.status.errorMessage = matchTypeName + "-超时!";
		outInfo.status.statusCode = LABELALL_RETURN_TIMEOUT;
		LOG->WriteLog(logPrefix + "超时!", ERR, outInfo.paths.logFile, true);
		break;

	case 0: // 匹配失败
	default:
		outInfo.status.errorMessage = matchTypeName + "-匹配失败!";
		outInfo.status.statusCode = LABELALL_RETURN_MATCH_ERR0;
		LOG->WriteLog(logPrefix + "匹配失败!", ERR, outInfo.paths.logFile, true);
		break;
	}
}

void InspLabelAll::LabelAll_MatchTemplate(InspLabelAllOut& outInfo) {
	if (outInfo.status.statusCode != LABELALL_RETURN_OK) {
		LOG->WriteLog("跳过模板匹配!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}

	if (!m_params.isCheckTemplate) {
		LOG->WriteLog("未开启模板匹配!", WARNING, outInfo.paths.logFile, m_params.saveLogTxt);
		return;
	}
	else
	{
		LOG->WriteLog("开始模板匹配!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}

	try {
		for (int i = 0; i < m_params.matchPara.size(); i++) {
			MatchResult matchResult;
			MatchConfig& matchCfg = m_params.matchPara[i];

			int rv = -1;
			std::string matchTypeName;

			// 根据匹配类型选择不同的匹配算法
			switch (matchCfg.matchType) {
			case 0: // Halcon形状匹配
			case 1: // Halcon缩放形状匹配
				rv = MF->MatchHalcon(m_img, matchCfg, matchResult);
				matchTypeName = (matchCfg.matchType == 0) ? "形状匹配" : "缩放形状匹配";
				break;

			case 2: // NCC匹配
				rv = MF->MatchNCC(m_img, matchCfg, matchResult);
				matchTypeName = "灰度匹配";
				break;

			case 3: // SIFT匹配
				rv = MF->MatchSIFT(m_img, matchCfg, matchResult);
				matchTypeName = "特征点匹配";
				break;

			default:
				LOG->WriteLog("不支持的匹配类型: " + std::to_string(matchCfg.matchType),
					ERR, outInfo.paths.logFile, true);
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "不支持的匹配类型!";
				return;
			}

			outInfo.geometry.matchResults.push_back(matchResult);

			// 处理匹配结果
			ProcessMatchResult(outInfo, i, rv, matchResult, matchTypeName);
		}

		LOG->WriteLog("模板匹配完成!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);
	}
	catch (const std::exception& e) {
		outInfo.status.errorMessage = "模板匹配-算法异常!";
		outInfo.status.statusCode = LABELALL_RETURN_ALGO_ERR;
		LOG->WriteLog("模板匹配异常: " + std::string(e.what()),
			ERR, outInfo.paths.logFile, true);
	}
}

void InspLabelAll::LabelAll_CheckBar(InspLabelAllOut& outInfo) {
	if (outInfo.status.statusCode != LABELALL_RETURN_OK) {
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
			outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
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
				outInfo.status.statusCode = LABELALL_RETURN_NO_1D;
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
				outInfo.status.statusCode = LABELALL_RETURN_MISS_1D;
				outInfo.status.errorMessage = "多一维码!";
				LOG->WriteLog("多一维码！", ERR, outInfo.paths.logFile, true);
				return;
			}
			if (cntBar == 0)
			{
				outInfo.status.statusCode = LABELALL_RETURN_INFO_ERR_1D;
				outInfo.status.errorMessage = "一维码信息错误!";
				LOG->WriteLog("一维码信息错误！", ERR, outInfo.paths.logFile, true);
				return;
			}
		}
		if (m_params.barConfigs[i].checkType == "二维码" || m_params.barConfigs[i].checkType == "2D")
		{
			if (outInfo.bar.barResults[i].empty())
			{
				outInfo.status.statusCode = LABELALL_RETURN_NO_2D;
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
				outInfo.status.statusCode = LABELALL_RETURN_MISS_2D;
				outInfo.status.errorMessage = "多二维码!";
				LOG->WriteLog("多二维码！", ERR, outInfo.paths.logFile, true);
				return;
			}
			if (cntQr == 0)
			{
				outInfo.status.statusCode = LABELALL_RETURN_INFO_ERR_2D;
				outInfo.status.errorMessage = "二维码信息错误!";
				LOG->WriteLog("二维码信息错误！", ERR, outInfo.paths.logFile, true);
				return;
			}
		}

	}


	return;
}

// 从轮廓计算角点
void InspLabelAll::CalculateCornersFromContours(HObject ho_Contours, int adjustedRoiX, int adjustedRoiY,
	std::vector<cv::Point2f>& corners) {
	try {
		// 获取轮廓的边界框
		HTuple hv_Row1, hv_Column1, hv_Row2, hv_Column2;
		HalconCpp::SmallestRectangle1Xld(ho_Contours, &hv_Row1, &hv_Column1, &hv_Row2, &hv_Column2);

		if (hv_Row1.Length() > 0 && hv_Column1.Length() > 0 &&
			hv_Row2.Length() > 0 && hv_Column2.Length() > 0) {
			// 计算四个角点
			corners.push_back(cv::Point2f(hv_Column1[0].D() + adjustedRoiX, hv_Row1[0].D() + adjustedRoiY));
			corners.push_back(cv::Point2f(hv_Column2[0].D() + adjustedRoiX, hv_Row1[0].D() + adjustedRoiY));
			corners.push_back(cv::Point2f(hv_Column2[0].D() + adjustedRoiX, hv_Row2[0].D() + adjustedRoiY));
			corners.push_back(cv::Point2f(hv_Column1[0].D() + adjustedRoiX, hv_Row2[0].D() + adjustedRoiY));
		}
	}
	catch (const std::exception& e) {
		std::cerr << "计算角点异常: " << e.what() << std::endl;
	} 
}

void InspLabelAll::LabelAll_DrawResult(InspLabelAllOut& outInfo) {
	LOG->WriteLog("开始绘制结果!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

	// 根据图像大小动态调整字体大小
	int imgHeight = MIN(outInfo.images.outputImg.mat().rows, outInfo.images.outputImg.mat().cols);
	double fontScale = imgHeight / 1000.0;

	// 根据匹配结果数量调整字体大小
	if (outInfo.geometry.matchResults.size() > 5 && outInfo.geometry.matchResults.size() < 10) {
		fontScale *= 0.5;
	}
	else if (outInfo.geometry.matchResults.size() >= 10) {
		fontScale *= 0.3;
	}

	// 设置字体大小
	int baseFontSizeStatus = static_cast<int>(50 * fontScale);   // 状态信息字体大小
	int baseFontSizeFeature = static_cast<int>(35 * fontScale);  // 特征信息字体大小
	int baseFontSizeDetail = static_cast<int>(30 * fontScale);   // 详细信息字体大小
	int baseFontSizeBar = static_cast<int>(30 * fontScale);       // 条码信息字体大小

	// 1. 绘制定位ROI
	if (m_params.isCheckLocate) {
		rectangle(outInfo.images.outputImg.mat(), m_params.roiRect, Colors::YELLOW, 3, cv::LINE_AA);
	}

	// 2. 绘制定位结果
	auto formatConfidence = [](float conf) {
		return (std::ostringstream() << std::fixed << std::setprecision(2) << conf).str();
	};

	for (int i = 0; i < outInfo.locate.details.size(); i++) {
		cv::Scalar color;
		std::string className = outInfo.locate.details[i].className;

		if (className == "标签") {
			color = Colors::GREEN;
		}
		else if (className == "无标签" || className == "多标签" ||
			className == "标签断裂" || className == "标签卷曲" ||
			className == "标签折叠") {
			color = Colors::RED;
		}
		else {
			color = Colors::BLUE; // 一维码、二维码等
		}

		rectangle(outInfo.images.outputImg.mat(), outInfo.locate.details[i].box, color, 2, cv::LINE_AA);

		std::string infoText = className + "," +
			std::to_string(outInfo.locate.details[i].box.width) + "," +
			std::to_string(outInfo.locate.details[i].box.height) + "," +
			formatConfidence(outInfo.locate.details[i].confidence);

		cv::Point textPos(outInfo.locate.details[i].box.x,
			outInfo.locate.details[i].box.y + outInfo.locate.details[i].box.height + 20);

		putTextZH(outInfo.images.outputImg.mat(), infoText.c_str(),
			textPos, color, baseFontSizeDetail, FW_BOLD);
	}

	// 3. 绘制模板匹配结果
	const int startX = 10;
	int currentY = static_cast<int>(100 * fontScale);
	const int lineSpacing = static_cast<int>(40 * fontScale);
	const int sectionSpacing = static_cast<int>(15 * fontScale);

	if (m_params.isCheckTemplate) {
		// 设置绘制参数
		const cv::Scalar successColor = Colors::GREEN;    // 绿色 - 完全匹配
		const cv::Scalar failColor = Colors::RED;         // 红色 - 匹配失败

		for (int i = 0; i < outInfo.geometry.matchResults.size(); i++) {
			// 记录当前特征开始位置
			int featureStartY = currentY;

			// 根据匹配状态确定主要绘制颜色
			bool isOK = (outInfo.geometry.matchResults[i].valid == 1);
			cv::Scalar drawColor = isOK ? successColor : failColor;

			// 在左上角绘制特征状态
			std::stringstream ss0;
			ss0 << "特征" << i << ": ";
			switch (outInfo.geometry.matchResults[i].valid) {
			case 0: ss0 << "匹配失败"; break;
			case 1: ss0 << "匹配成功"; break;
			case 2: ss0 << "匹配得分过低"; break;
			case 3: ss0 << "歪斜"; break;
			case 4: ss0 << "错误特征"; break;
			case 5: ss0 << "水平偏移过大"; break;
			case 6: ss0 << "垂直偏移过大"; break;
			case 7: ss0 << "距离偏移过大"; break;
			default: ss0 << "匹配失败(" << outInfo.geometry.matchResults[i].valid << ")"; break;
			}

			putTextZH(outInfo.images.outputImg.mat(), ss0.str().c_str(),
				cv::Point(startX, currentY), drawColor, baseFontSizeFeature, FW_HEAVY);
			currentY += lineSpacing;

			// 如果匹配失败，只显示状态信息
			if (outInfo.geometry.matchResults[i].valid <= 0) {
				currentY += sectionSpacing;
				continue;
			}

			// 绘制特征详细信息（缩进显示）
			const int indent = static_cast<int>(0 * fontScale);
			int detailY = currentY;

			// 模版ID、得分和角度
			std::stringstream ss;
			ss << "模版ID: " << outInfo.geometry.matchResults[i].id
				<< "  得分: " << std::fixed << std::setprecision(2) << outInfo.geometry.matchResults[i].score
				<< "  角度: " << std::setprecision(2) << outInfo.geometry.matchResults[i].angle << "°";
			putTextZH(outInfo.images.outputImg.mat(), ss.str().c_str(),
				cv::Point(startX + indent, detailY), drawColor, baseFontSizeDetail, FW_HEAVY);
			detailY += lineSpacing;

			// 中心坐标
			ss.str("");
			ss << "中心: (" << std::fixed << std::setprecision(2)
				<< outInfo.geometry.matchResults[i].center.x << ", "
				<< outInfo.geometry.matchResults[i].center.y << ")";
			putTextZH(outInfo.images.outputImg.mat(), ss.str().c_str(),
				cv::Point(startX + indent, detailY), drawColor, baseFontSizeDetail, FW_HEAVY);
			detailY += lineSpacing;

			// 偏移量
			ss.str("");
			ss << "偏移: 水平 =" << std::fixed << std::setprecision(2)
				<< outInfo.geometry.matchResults[i].shiftHor
				<< ", 垂直 =" << outInfo.geometry.matchResults[i].shiftVer
				<< ", 距离 =" << outInfo.geometry.matchResults[i].offset;
			putTextZH(outInfo.images.outputImg.mat(), ss.str().c_str(),
				cv::Point(startX + indent, detailY), drawColor, baseFontSizeDetail, FW_HEAVY);
			detailY += lineSpacing;

			currentY = detailY + sectionSpacing;

			// 根据匹配类型选择不同的绘制方法
			int matchType = m_params.matchPara[i].matchType;

			switch (matchType) {
			case 0: // Halcon形状匹配
			case 1: // Halcon缩放形状匹配
				DrawHalconMatchResult(outInfo, outInfo.geometry.matchResults[i], drawColor);
				break;

			case 2: // NCC匹配
				DrawNCCMatchResult(outInfo, outInfo.geometry.matchResults[i], drawColor);
				break;

			case 3: // SIFT匹配
				DrawSIFTMatchResult(outInfo, outInfo.geometry.matchResults[i], drawColor);
				break;

			default:
				LOG->WriteLog("未知的匹配类型: " + std::to_string(matchType),
					WARNING, outInfo.paths.logFile, true);
				break;
			}

			// 绘制匹配编号
			putTextZH(outInfo.images.outputImg.mat(), std::to_string(i).c_str(),
				cv::Point(outInfo.geometry.matchResults[i].center.x - 10, outInfo.geometry.matchResults[i].center.y - 20),
				Colors::BLUE, 40, FW_HEAVY);
		}
	}

	// 4. 绘制条码/二维码结果
	for (int kk = 0; kk < outInfo.bar.barResults.size(); kk++) {
		int featureStartY = currentY;
		const auto& frameResults = outInfo.bar.barResults[kk];

		for (const auto& result : frameResults) {
			// 确定条码类型颜色
			cv::Scalar barColor = (m_params.barConfigs[kk].checkType == "一维码" ||
				m_params.barConfigs[kk].checkType == "1D") ? Colors::GREEN : Colors::BLUE;

			// 绘制旋转矩形
			if (result.rect.size.width > 0 && result.rect.size.height > 0) {
				cv::Point2f vertices[4];
				result.rect.points(vertices);

				for (int i = 0; i < 4; i++) {
					cv::line(outInfo.images.outputImg.mat(), vertices[i], vertices[(i + 1) % 4],
						barColor, 2);
				}
			}

			// 绘制条码信息
			std::string infoText = result.barType + ": " + result.infoResult;

			// 计算文本位置（在条码上方）
			cv::Point textPoint(static_cast<int>(result.rect.center.x - infoText.length() * 5),
				static_cast<int>(result.rect.center.y - result.rect.size.height / 2 - 20));

			// 确保文本在图像范围内
			if (textPoint.y < 20) {
				textPoint.y = result.rect.center.y + result.rect.size.height / 2 + 30;
			}

			putTextZH(outInfo.images.outputImg.mat(), infoText.c_str(),
				textPoint, barColor, baseFontSizeBar, FW_BOLD);
		}
	}

	// 5. 绘制总体状态信息
	std::string statusText = "ID = " + std::to_string(outInfo.system.jobId) + ", " +
		"状态码 = " + std::to_string(outInfo.status.statusCode) + ", " +
		outInfo.status.errorMessage;

	cv::Scalar statusColor = (outInfo.status.statusCode == LABELALL_RETURN_OK) ?
		Colors::GREEN : Colors::RED;

	putTextZH(outInfo.images.outputImg.mat(), statusText.c_str(),
		cv::Point(15, 10), statusColor, baseFontSizeStatus, FW_HEAVY);

	// 6. 保存调试图像
	if (m_params.saveDebugImage) {
		DAS->DAS_Img(outInfo.images.outputImg.mat(),
			outInfo.paths.intermediateImagesDir + "10.outputImg.jpg",
			true);
	}
}

// 新增辅助函数：绘制Halcon匹配结果
void InspLabelAll::DrawHalconMatchResult(InspLabelAllOut& outInfo, const MatchResult& result, cv::Scalar color) {
	try {
		// 检查是否有变换后的轮廓
		if (result.ho_TransContours.IsInitialized()) {
			HTuple hv_NumContours;
			HalconCpp::CountObj(result.ho_TransContours, &hv_NumContours);

			if (hv_NumContours[0].I() > 0) {
				LOG->WriteLog("找到变换后的轮廓，数量: " + std::to_string(hv_NumContours[0].I()),
					INFO, outInfo.paths.logFile, true);

				// 遍历所有轮廓对象
				for (int contourIdx = 1; contourIdx <= hv_NumContours[0].I(); contourIdx++) {
					HObject ho_SingleContour;
					SelectObj(result.ho_TransContours, &ho_SingleContour, contourIdx);

					HTuple hv_Rows, hv_Cols;
					GetContourXld(ho_SingleContour, &hv_Rows, &hv_Cols);

					std::vector<cv::Point> currentContour;
					bool inContour = false;

					// 处理轮廓点
					for (int k = 0; k < hv_Rows.Length(); k++) {
						double rowVal = hv_Rows[k].D();
						double colVal = hv_Cols[k].D();

						if (std::isfinite(rowVal) && std::isfinite(colVal)) {
							// 关键：将轮廓点从ROI坐标系转换到原图坐标系
							currentContour.push_back(cv::Point(
								static_cast<int>(colVal) + result.adjustedRoiX,
								static_cast<int>(rowVal) + result.adjustedRoiY
							));
							inContour = true;
						}
						else if (inContour) {
							// 遇到非有限值，绘制当前轮廓段
							if (!currentContour.empty() && currentContour.size() > 1) {
								cv::polylines(outInfo.images.outputImg.mat(),
									currentContour, false, color, 2, cv::LINE_AA);
							}
							currentContour.clear();
							inContour = false;
						}
					}

					// 绘制最后一个轮廓段
					if (!currentContour.empty() && currentContour.size() > 1) {
						cv::polylines(outInfo.images.outputImg.mat(),
							currentContour, false, color, 2, cv::LINE_AA);
					}
				}
			}
			else {
				LOG->WriteLog("变换后的轮廓数量为0", WARNING, outInfo.paths.logFile, true);
			}
		}
		else {
			LOG->WriteLog("未找到变换后的轮廓", WARNING, outInfo.paths.logFile, true);
		}

		// 绘制边界框
		if (!result.corners.empty() && result.corners.size() >= 4) {
			// 检查是否是多个实例的角点（每个实例4个角点）
			size_t numInstances = result.corners.size() / 4;

			for (size_t instanceIdx = 0; instanceIdx < numInstances; instanceIdx++) {
				size_t startIdx = instanceIdx * 4;

				// 确保有足够的角点
				if (startIdx + 3 < result.corners.size()) {
					// 绘制四边形边界
					for (int kk = 0; kk < 4; kk++) {
						cv::line(outInfo.images.outputImg.mat(),
							result.corners[startIdx + kk],
							result.corners[startIdx + (kk + 1) % 4],
							color, 2);
					}

					// 绘制实例编号
					if (numInstances > 1) {
						cv::Point center(0, 0);
						for (int kk = 0; kk < 4; kk++) {
							center.x += result.corners[startIdx + kk].x;
							center.y += result.corners[startIdx + kk].y;
						}
						center.x /= 4;
						center.y /= 4;

						putTextZH(outInfo.images.outputImg.mat(),
							std::to_string(instanceIdx).c_str(),
							cv::Point(center.x - 10, center.y - 20),
							Colors::BLUE, 40, FW_HEAVY);
					}
				}
			}
		}

		// 绘制中心点（如果有）
		if (result.center.x >= 0 && result.center.y >= 0) {
			cv::circle(outInfo.images.outputImg.mat(), result.center, 5, color, -1);

			// 绘制偏移线（从模板中心到实际中心）
			if (result.shiftHor != 0 || result.shiftVer != 0) {
				cv::Point templateCenter(
					static_cast<int>(result.center.x - result.shiftHor),
					static_cast<int>(result.center.y - result.shiftVer)
				);

				cv::circle(outInfo.images.outputImg.mat(), templateCenter, 3, Colors::YELLOW, -1);
				cv::line(outInfo.images.outputImg.mat(), templateCenter, result.center, Colors::YELLOW, 1);
			}
		}
	}
	catch (const std::exception& e) {
		LOG->WriteLog("绘制Halcon匹配结果异常: " + std::string(e.what()),
			WARNING, outInfo.paths.logFile, true);
	}
}

// 新增辅助函数：绘制NCC匹配结果
void InspLabelAll::DrawNCCMatchResult(InspLabelAllOut& outInfo, const MatchResult& result, cv::Scalar color) {
	// NCC匹配绘制矩形边界框
	if (result.rect.width > 0 && result.rect.height > 0) {
		rectangle(outInfo.images.outputImg.mat(), result.rect, color, 2, cv::LINE_AA);

		//// 绘制中心十字
		//cv::line(outInfo.images.outputImg.mat(),
		//	cv::Point(result.center.x - 10, result.center.y),
		//	cv::Point(result.center.x + 10, result.center.y),
		//	color, 2);
		//cv::line(outInfo.images.outputImg.mat(),
		//	cv::Point(result.center.x, result.center.y - 10),
		//	cv::Point(result.center.x, result.center.y + 10),
		//	color, 2);
	}

	if (result.shiftHor != 0 || result.shiftVer != 0) {
		cv::Point templateCenter(
			static_cast<int>(result.center.x - result.shiftHor),
			static_cast<int>(result.center.y - result.shiftVer)
		);

		cv::circle(outInfo.images.outputImg.mat(), templateCenter, 3, Colors::YELLOW, -1);
		cv::line(outInfo.images.outputImg.mat(), templateCenter, result.center, Colors::YELLOW, 1);
	}

	//// 绘制角点（如果有）
	//if (!result.corners.empty() && result.corners.size() >= 4) {
	//	for (int kk = 0; kk < 4; kk++) {
	//		cv::circle(outInfo.images.outputImg.mat(), result.corners[kk], 5, color, -1);
	//		cv::line(outInfo.images.outputImg.mat(),
	//			result.corners[kk],
	//			result.corners[(kk + 1) % 4],
	//			color, 1);
	//	}
	//}
}

// 新增辅助函数：绘制SIFT匹配结果
void InspLabelAll::DrawSIFTMatchResult(InspLabelAllOut& outInfo, const MatchResult& result, cv::Scalar color) {
	// 绘制旋转矩形
	if (result.boundingRect.size.width > 0 && result.boundingRect.size.height > 0) {
		cv::Point2f vertices[4];
		result.boundingRect.points(vertices);

		for (int i = 0; i < 4; i++) {
			cv::line(outInfo.images.outputImg.mat(), vertices[i], vertices[(i + 1) % 4], color, 2);
		}

		if (result.shiftHor != 0 || result.shiftVer != 0) {
			cv::Point templateCenter(
				static_cast<int>(result.center.x - result.shiftHor),
				static_cast<int>(result.center.y - result.shiftVer)
			);

			cv::circle(outInfo.images.outputImg.mat(), templateCenter, 3, Colors::YELLOW, -1);
			cv::line(outInfo.images.outputImg.mat(), templateCenter, result.center, Colors::YELLOW, 1);
		}

		//// 绘制中心点和方向
		//cv::circle(outInfo.images.outputImg.mat(), result.boundingRect.center, 5, color, -1);

		//// 绘制方向线
		//double angleRad = result.boundingRect.angle * CV_PI / 180.0;
		//cv::Point2f direction(
		//	result.boundingRect.center.x + 30 * cos(angleRad),
		//	result.boundingRect.center.y + 30 * sin(angleRad)
		//);
		//cv::arrowedLine(outInfo.images.outputImg.mat(),
		//	result.boundingRect.center, direction, color, 2);
	}

	// 绘制角点（SIFT特征点）
	if (!result.corners.empty()) {
		for (const auto& corner : result.corners) {
			cv::circle(outInfo.images.outputImg.mat(), corner, 3, color, -1);
		}
	}
}


int InspLabelAll::LabelAll_Main(InspLabelAllOut& outInfo) {
	double time0 = static_cast<double>(cv::getTickCount());
	LOG->WriteLog("LabelAll_Main!", INFO, outInfo.paths.logFile, m_params.saveLogTxt);

	try {
		double time0 = static_cast<double>(cv::getTickCount());
		if (outInfo.status.statusCode == LABELALL_RETURN_OK)
		{
			// 处理流程
			LabelAll_SetROI(outInfo);
			LabelAll_LocateLabel(outInfo);
			LabelAll_MatchTemplate(outInfo);
			LabelAll_CheckBar(outInfo);
		}
		LabelAll_DrawResult(outInfo);

		if (outInfo.status.statusCode == LABELALL_RETURN_OK) {
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
		return LABELALL_RETURN_ALGO_ERR;
	}

	return outInfo.status.statusCode;
}