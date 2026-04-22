//#include <iostream>
//#include <windows.h>
//#include <opencv2/opencv.hpp>
//#include <thread>
//#include <vector>
//#include <future>
//#include <mutex>
//
//// 定义结构体
//struct TextDetectionResult {
//    std::vector<std::vector<int>> boxes;
//};
//
//struct TextRecognitionResult {
//    std::string text;
//    float score;
//};
//
//// 定义函数指针类型
//typedef void (*ImageProcessFunc)(cv::Mat, TextDetectionResult***, int*, TextRecognitionResult***, int*);
//typedef void (*FreeMemoryFunc)(TextDetectionResult**, int, TextRecognitionResult**, int);
//
//// 全局互斥锁，用于线程安全输出
//std::mutex print_mutex;
//
//// 线程函数：处理单个相机的图像
//void ProcessCameraImage(cv::Mat img, ImageProcessFunc ImageProcess, FreeMemoryFunc FreeMemory) {
//    // 检查图像是否为空
//    if (img.empty()) {
//        std::lock_guard<std::mutex> lock(print_mutex);
//        std::cerr << "Error: Input image is empty." << std::endl;
//        return;
//    }
//
//    // 调用 ImageProcess 函数
//    TextDetectionResult** detection_results = nullptr;
//    int num_detection_results = 0;
//    TextRecognitionResult** recognition_results = nullptr;
//    int num_recognition_results = 0;
//
//    ImageProcess(img, &detection_results, &num_detection_results, &recognition_results, &num_recognition_results);
//
//    // 输出结果
//    {
//        std::lock_guard<std::mutex> lock(print_mutex);
//        std::cout << "Results for image:" << std::endl;
//        if (num_detection_results > 0 && num_recognition_results > 0) {
//            for (int i = 0; i < num_detection_results; i++) {
//                std::cout << "Detection Boxes: ";
//                for (const auto& box : detection_results[i]->boxes) {
//                    for (int val : box) {
//                        std::cout << val << " ";
//                    }
//                }
//                std::cout << std::endl;
//            }
//
//            for (int i = 0; i < num_recognition_results; i++) {
//                std::cout << "Recognized Text: " << recognition_results[i]->text << std::endl;
//                std::cout << "Confidence Score: " << recognition_results[i]->score << std::endl;
//            }
//        }
//        else {
//            std::cout << "No results found." << std::endl;
//        }
//    }
//
//    // 释放内存
//    FreeMemory(detection_results, num_detection_results, recognition_results, num_recognition_results);
//}
//
//int main() {
//    system("chcp 65001");
//    // 加载 DLL
//    HMODULE hDll = LoadLibrary(TEXT("ppocr.dll")); 
//    if (!hDll) {
//        std::cerr << "Failed to load DLL." << std::endl;
//        return 1;
//    }
//
//    // 获取函数地址
//    ImageProcessFunc ImageProcess = (ImageProcessFunc)GetProcAddress(hDll, "ImageProcess");
//    FreeMemoryFunc FreeMemory = (FreeMemoryFunc)GetProcAddress(hDll, "FreeMemory");
//
//    if (!ImageProcess || !FreeMemory) {
//        std::cerr << "Failed to get function address." << std::endl;
//        FreeLibrary(hDll);
//        return 1;
//    }
//
//    // 模拟多个相机的图像
//    std::vector<cv::Mat> images = {
//       /* cv::imread("1.jpg"),
//        cv::imread("2.jpg"),
//        cv::imread("3.jpg"),*/
//        cv::imread("4.jpg")
//    };
//
//    // 检查图像是否加载成功
//    for (const auto& img : images) {
//        if (img.empty()) {
//            std::cerr << "Error: Failed to load one or more images." << std::endl;
//            FreeLibrary(hDll);
//            return 1;
//        }
//    }
//
//    // 创建线程池
//    std::vector<std::future<void>> futures;
//    for (const auto& img : images) {
//        futures.push_back(std::async(std::launch::async, ProcessCameraImage, img, ImageProcess, FreeMemory));
//    }
//
//    // 等待所有线程完成
//    for (auto& future : futures) {
//        future.wait();
//    }
//
//    // 释放 DLL
//    FreeLibrary(hDll);
//    return 0;
//}