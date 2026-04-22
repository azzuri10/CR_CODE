//#include <HalconCpp.h>
//#include <iostream>
//
//using namespace std;
//using namespace HalconCpp;
//
//int main() {
//	try {
//		// 读取图像
//		HImage image("F://IMG//244_2025_01_14_09_04_33_195.jpg");
//
//		// 创建条码模型
//		HTuple barCodeHandle;
//		CreateBarCodeModel(HTuple(), HTuple(), &barCodeHandle);
//
//		// 设置条码参数
//		SetBarCodeParam(barCodeHandle, "element_size_min", 1); // 设置最小元素大小
//		SetBarCodeParam(barCodeHandle, "stop_after_result_num", 1); // 只检测一个条码
//
//		// 检测条码
//		HObject symbolRegions;
//		HTuple barCodeStrings;
//		FindBarCode(image, &symbolRegions, barCodeHandle, "auto", &barCodeStrings);
//
//		// 获取条码类型
//		HTuple barCodeTypes;
//		GetBarCodeResult(barCodeHandle, "all", "decoded_types", &barCodeTypes);
//
//		// 获取检测结果
//		if (barCodeStrings.Length() > 0 && barCodeStrings.Length() == barCodeTypes.Length()) {
//			cout << "检测到条码数量: " << barCodeStrings.Length() << endl;
//			for (int i = 0; i < barCodeStrings.Length(); i++) {
//				cout << "条码类型: " << barCodeTypes[i].S() << endl;
//				cout << "条码数据: " << barCodeStrings[i].S() << endl;
//			}
//		}
//		else {
//			cout << "未检测到条码或条码类型获取失败！" << endl;
//		}
//
//		// 释放条码模型
//		ClearBarCodeModel(barCodeHandle);
//	}
//	catch (HException& e) {
//		cerr << "HALCON 错误: " << e.ErrorMessage() << endl;
//	}
//
//	return 0;
//}
////#include <iostream>
////#include <vector>
////#include <opencv2/opencv.hpp>
////#include <HalconCpp.h>
////#include <omp.h>
////#include <fstream>
////
////using namespace std;
////using namespace cv;
////using namespace HalconCpp;
////
////// 日志记录函数
////void LogToFile(const string& message) {
////    ofstream logFile("barcode_log.txt", ios::app);
////    if (logFile.is_open()) {
////        logFile << message << endl;
////        logFile.close();
////    }
////}
////
////// 将Halcon区域转换为OpenCV的RotatedRect
////RotatedRect ConvertToRotatedRect(double row, double col, double angle, double width, double height) {
////    Point2f center(col, row);
////    Size2f size(width, height);
////    return RotatedRect(center, size, angle);
////}
////
////// 条码检测函数
////bool BAQ_CheckBar(Mat img, const string& barType, vector<string>& info, vector<RotatedRect>& barRect, vector<float>& barAngle) {
////    if (img.empty()) {
////        cout << "输入条码检测图像为空！" << endl;
////        LogToFile("输入条码检测图像为空！");
////        return false;
////    }
////
////    info.clear();
////    barRect.clear();
////    barAngle.clear();
////
////    // 图像预处理
////    Mat grayImg;
////    cvtColor(img, grayImg, COLOR_BGR2GRAY);
////    //GaussianBlur(grayImg, grayImg, Size(3, 3), 0);
////    //equalizeHist(grayImg, grayImg); // 直方图均衡化
////    Mat binaryImg;
////    //threshold(grayImg, binaryImg, 120, 255, THRESH_BINARY);
////    adaptiveThreshold(grayImg, binaryImg, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 11, 2);
////
////    imshow("img", binaryImg);
////    // 将OpenCV图像转换为Halcon图像
////    HImage halconImage;
////    halconImage.GenImage1("byte", binaryImg.cols, binaryImg.rows, binaryImg.data);
////
////    // 条码阅读器模型句柄
////    HTuple barCodeHandle;
////    CreateBarCodeModel(HTuple(), HTuple(), &barCodeHandle);
////
////    // 设置条码模型参数
////    SetBarCodeParam(barCodeHandle, "stop_after_result_num", 1); // 只检测一个条码
////    SetBarCodeParam(barCodeHandle, "min_code_length", 10);      // 设置最小条码长度
////
////    // 多尺度检测
////    bool barcodeFound = false;
////    for (double scale = 1.0; scale >= 0.5; scale -= 0.1) {
////        Mat resizedImg;
////        resize(binaryImg, resizedImg, Size(), scale, scale);
////
////        HImage resizedHalconImage;
////        resizedHalconImage.GenImage1("byte", resizedImg.cols, resizedImg.rows, resizedImg.data);
////
////        HObject symbolRegions;
////        HTuple codeStrings, barcodeType, barcodeResult;
////        try {
////            FindBarCode(resizedHalconImage, &symbolRegions, barCodeHandle, barType.c_str(), &codeStrings);
////
////            HTuple numCodes;
////            TupleLength(codeStrings, &numCodes);
////
////            if (numCodes > 0) {
////                barcodeFound = true;
////                // 获取条码类型
////                GetBarCodeResult(barCodeHandle, "all", "decoded_types", &barcodeType);
////                string logMessage = "条码类型: " + string(barcodeType[0].S().Text());
////                cout << logMessage << endl;
////                LogToFile(logMessage);
////
////                HTuple numRegions;
////                CountObj(symbolRegions, &numRegions); // 获取符号区域的数量
////
////#pragma omp parallel for
////                for (int i = 0; i < numCodes.I(); i++) {
////                    // 获取并打印每个条码信息
////                    string strBarCode(codeStrings[i].S());
////                    string logMessage = "条码 " + to_string(i + 1) + ": " + strBarCode;
////                    cout << logMessage << endl;
////                    LogToFile(logMessage);
////                    info.push_back(strBarCode);
////
////                    // 获取单个条码区域
////                    HObject singleSymbolRegion;
////                    SelectObj(symbolRegions, &singleSymbolRegion, i + 1);
////
////                    // 获取每个条码的区域和位置
////                    HTuple area, row, column;
////                    AreaCenter(singleSymbolRegion, &area, &row, &column);
////                    logMessage = "条码 " + to_string(i + 1) + " - Area: " + to_string(area.D()) + ", Row: " + to_string(row.D()) + ", Column: " + to_string(column.D());
////                    cout << logMessage << endl;
////                    LogToFile(logMessage);
////
////                    HTuple row1, col1, row2, col2;
////                    SmallestRectangle1(singleSymbolRegion, &row1, &col1, &row2, &col2); // 获取包围矩形
////
////                    try {
////                        // 尝试获取条码角度参数
////                        HTuple symbolAngle;
////                        OrientationRegion(singleSymbolRegion, &symbolAngle);
////                        logMessage = "条码 " + to_string(i + 1) + " - Angle: " + to_string(symbolAngle.D());
////                        cout << logMessage << endl;
////                        LogToFile(logMessage);
////                        barAngle.push_back(symbolAngle.D());
////
////                        // 将条码区域转换为 OpenCV 的 RotatedRect
////                        RotatedRect barRectCur = ConvertToRotatedRect(row.D(), column.D(), symbolAngle.D(), col2.D() - col1.D(), row2.D() - row1.D());
////                        barRect.push_back(barRectCur);
////                    }
////                    catch (HException& e) {
////                        // 捕获并处理异常
////                        string errorMessage = "错误: " + string(e.ErrorMessage());
////                        cout << errorMessage << endl;
////                        LogToFile(errorMessage);
////                        info.clear();
////                        barRect.clear();
////                        barAngle.clear();
////                        return false;
////                    }
////                }
////                break; // 如果找到条码，退出多尺度循环
////            }
////        }
////        catch (HException& e) {
////            string errorMessage = "多尺度检测错误: " + string(e.ErrorMessage());
////            cout << errorMessage << endl;
////            LogToFile(errorMessage);
////        }
////    }
////
////    // 清理条码模型
////    ClearBarCodeModel(barCodeHandle);
////
////    if (!barcodeFound) {
////        cout << "未检测到条码！" << endl;
////        LogToFile("未检测到条码！");
////        return false;
////    }
////
////    return true;
////}
////
////int main() {
////    // 读取图像
////    Mat img = imread("F://IMG//黄冈中粮//黄冈标签ng20250114//2.jpg");
////    if (img.empty()) {
////        cout << "无法读取图像！" << endl;
////        return -1;
////    }
////
////    // 条码检测
////    vector<string> info;
////    vector<RotatedRect> barRect;
////    vector<float> barAngle;
////    string barType = "auto"; // 自动检测条码类型
////	namedWindow("img", WINDOW_NORMAL);
////    if (BAQ_CheckBar(img, barType, info, barRect, barAngle)) {
////        cout << "条码检测成功！" << endl;
////    }
////    else {
////        cout << "条码检测失败！" << endl;
////    }
////    waitKey(0);
////    return 0;
////}