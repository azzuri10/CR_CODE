//#include "HeaderDefine.h"
//#include "InferenceWorker.h"
//#include <filesystem>
//#include <map>
//#include <vector>
//#include <string>
//#include <iostream>
//#include <fstream>
//#include <chrono>
//#include <mutex>
//#include <algorithm>
//#include <iomanip>
//#include <numeric>
//#include <opencv2/opencv.hpp>
//#include <opencv2/dnn.hpp>
//
//namespace fs = std::filesystem;
//
//// 特殊字符映射表
//std::map<char, std::string> symbol_test = {
//    {'/', "slash"}, {'\\', "backslash"}, {':', "colon"}, {'*', "asterisk"},
//    {'?', "question"}, {'"', "quote"}, {'<', "less"}, {'>', "greater"},
//    {'|', "pipe"}, {'#', "hash"}, {'(', "paren_open"}, {')', "paren_close"},
//    {'[', "bracket_open"}, {']', "bracket_close"}, {'{', "brace_open"},
//    {'}', "brace_close"}, {'&', "ampersand"}, {'%', "percent"}, {'$', "dollar"},
//    {'@', "at"}, {'!', "exclamation"}, {';', "semicolon"}, {',', "comma"},
//    {'.', "dot"}, {' ', "space"}
//};
//
//// 模型管理器类（简化版）
//class ModelManager {
//private:
//    std::map<std::string, cv::dnn::Net> models;
//    std::mutex model_mutex;
//
//public:
//    static ModelManager& Instance() {
//        static ModelManager instance;
//        return instance;
//    }
//
//    bool IsModelLoaded(const std::string& model_path) {
//        std::lock_guard<std::mutex> lock(model_mutex);
//        return models.find(model_path) != models.end();
//    }
//
//    void LoadModel(const std::string& model_path, int backend) {
//        std::lock_guard<std::mutex> lock(model_mutex);
//        cv::dnn::Net net = cv::dnn::readNetFromONNX(model_path);
//
//        if (backend == 0) {
//            net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
//            net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
//        }
//        else {
//            net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
//            net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
//        }
//
//        models[model_path] = net;
//    }
//
//    cv::dnn::Net& GetModel(const std::string& model_path) {
//        std::lock_guard<std::mutex> lock(model_mutex);
//        return models[model_path];
//    }
//};
//
//// 将类别名称转换为安全的文件夹名称
//std::string toSafeFolderName1(const std::string& class_name) {
//    std::string safe_name;
//    for (char c : class_name) {
//        auto it = symbol_test.find(c);
//        if (it != symbol_test.end()) {
//            safe_name += it->second;
//        }
//        else {
//            safe_name += c;
//        }
//    }
//    return safe_name;
//}
//
//// 加载类别文件
//std::vector<std::string> loadClassNames(const std::string& filename) {
//    std::vector<std::string> classes;
//    std::ifstream file(filename);
//    if (!file.is_open()) {
//        std::cerr << "Error: Could not open class names file: " << filename << std::endl;
//        return classes;
//    }
//
//    std::string line;
//    while (std::getline(file, line)) {
//        if (!line.empty()) {
//            classes.push_back(line);
//        }
//    }
//    return classes;
//}
//
//// 获取文件夹中的所有图片文件
//std::vector<std::string> getImageFiles(const std::string& folderPath) {
//    std::vector<std::string> imageFiles;
//    try {
//        for (const auto& entry : fs::directory_iterator(folderPath)) {
//            if (fs::is_regular_file(entry)) {
//                std::string extension = entry.path().extension().string();
//                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
//
//                if (extension == ".jpg" || extension == ".jpeg" ||
//                    extension == ".png" || extension == ".bmp") {
//                    imageFiles.push_back(entry.path().string());
//                }
//            }
//        }
//        std::sort(imageFiles.begin(), imageFiles.end());
//    }
//    catch (const fs::filesystem_error& e) {
//        std::cerr << "Error accessing directory: " << e.what() << std::endl;
//    }
//    return imageFiles;
//}
//
//// 生成三位数字的文件夹名称
//std::string toThreeDigitName(int index) {
//    if (index < 0 || index > 999) {
//        return "000";
//    }
//
//    std::string result = std::to_string(index);
//    while (result.length() < 3) {
//        result = "0" + result;
//    }
//    return result;
//}
//
//int main(int argc, char* argv[]) {
//    // 1. 配置参数
//    const std::string model_path = "D:/CONFIG_CR_3.0/Code/YHBB/CLASSFY/best.onnx";  // ONNX模型路径
//    const std::string class_names_path = "D:/CONFIG_CR_3.0/Code/all/CLASSFY/CLASS.txt";  // 所有类型
//    std::string base_image_path = "E:/TRAIN_DATA_3.0/CODE_CLASSFY/YHALL/塑包/2024";  // 测试文件夹路径
//
//    // 检查命令行参数
//    if (argc > 1) {
//        base_image_path = argv[1];
//    }
//
//    // 2. 加载类别
//    auto classes = loadClassNames(class_names_path);
//    if (classes.empty()) {
//        std::cerr << "Error: No classes loaded from: " << class_names_path << std::endl;
//        return -1;
//    }
//
//    // 3. 检查每个数字文件夹是否存在，并建立数字文件夹到类别的映射
//    std::vector<std::string> valid_class_folders;
//    std::vector<std::string> missing_class_folders;
//    std::map<std::string, std::string> folder_to_class;  // 数字文件夹名称到类别的映射
//    std::map<std::string, std::string> class_to_safe_name;  // 原始类别名到安全名称的映射
//
//    // 为每个类别生成安全名称
//    for (const auto& class_name : classes) {
//        class_to_safe_name[class_name] = toSafeFolderName1(class_name);
//    }
//
//    // 检查数字文件夹
//    for (int i = 0; i < classes.size(); i++) {
//        std::string folder_name = toThreeDigitName(i);
//        std::string class_name = classes[i];
//        std::string folder_path = base_image_path + "/" + folder_name;
//
//        if (fs::exists(folder_path) && fs::is_directory(folder_path)) {
//            valid_class_folders.push_back(class_name);
//            folder_to_class[folder_name] = class_name;
//            std::cout << "Found folder " << folder_name << " for class: " << class_name << std::endl;
//        }
//        else {
//            missing_class_folders.push_back(class_name);
//            std::cout << "WARNING: No folder found for class: " << class_name << " (expected folder: " << folder_name << ")" << std::endl;
//        }
//    }
//
//    if (valid_class_folders.empty()) {
//        std::cerr << "Error: No valid class folders found in: " << base_image_path << std::endl;
//        return -1;
//    }
//
//    // 4. 创建主结果目录
//    const std::string output_dir = base_image_path + "/classification_results";
//    if (!fs::exists(output_dir)) {
//        fs::create_directory(output_dir);
//    }
//
//    // 5. 为每个有效类别创建OK和NG子目录
//    for (const auto& class_name : valid_class_folders) {
//        std::string safe_name = class_to_safe_name[class_name];
//        std::string ok_dir = output_dir + "/" + safe_name ;
//        std::string ng_dir = output_dir + "/" + safe_name ;
//
//        /*if (!fs::exists(ok_dir)) {
//            fs::create_directories(ok_dir);
//        }*/
//        if (!fs::exists(ng_dir)) {
//            fs::create_directories(ng_dir);
//        }
//    }
//
//    // 6. 预热推理
//    std::cout << "\nPerforming warmup runs..." << std::endl;
//    // 获取第一张图片进行预热
//    std::string first_image_path;
//    for (int i = 0; i < classes.size(); i++) {
//        std::string folder_name = toThreeDigitName(i);
//        std::string folder_path = base_image_path + "/" + folder_name;
//        if (fs::exists(folder_path) && fs::is_directory(folder_path)) {
//            auto image_files = getImageFiles(folder_path);
//            if (!image_files.empty()) {
//                first_image_path = image_files[0];
//                break;
//            }
//        }
//    }
//
//    if (first_image_path.empty()) {
//        std::cerr << "Error: No images found in any class folder" << std::endl;
//        return -1;
//    }
//
//    cv::Mat warmup_image = cv::imread(first_image_path);
//    for (int i = 0; i < 3; i++) {
//        auto result = InferenceWorker::RunClassification(0, model_path, classes, warmup_image, 0.5f);
//    }
//    std::cout << "Warmup completed." << std::endl;
//
//    // 7. 批量处理图片
//    std::cout << "\nRunning classification inference..." << std::endl;
//
//    // 性能统计
//    std::vector<long> inference_times;
//    std::vector<long> image_load_times;
//    int total_images = 0;
//    int correct_classifications = 0;
//    int incorrect_classifications = 0;
//
//    // 记录总体开始时间
//    auto batch_start_time = std::chrono::high_resolution_clock::now();
//
//    // 处理每个有效类别的文件夹
//    for (int i = 0; i < classes.size(); i++) {
//        std::string folder_name = toThreeDigitName(i);
//        std::string class_name = classes[i];
//
//        // 跳过不存在的文件夹
//        if (folder_to_class.find(folder_name) == folder_to_class.end()) {
//            continue;
//        }
//
//        std::string folder_path = base_image_path + "/" + folder_name;
//        auto image_files = getImageFiles(folder_path);
//
//        if (image_files.empty()) {
//            std::cout << "No images found in folder " << folder_name << " for class: " << class_name << std::endl;
//            continue;
//        }
//
//        std::cout << "Processing " << image_files.size() << " images in folder " << folder_name << " for class: " << class_name << std::endl;
//
//        for (const auto& file_path : image_files) {
//            // 加载图片并计时
//            auto load_start = std::chrono::high_resolution_clock::now();
//            cv::Mat image = cv::imread(file_path);
//            if (image.empty()) {
//                std::cerr << "Warning: Could not load image: " << file_path << std::endl;
//                continue;
//            }
//            auto load_end = std::chrono::high_resolution_clock::now();
//            auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start).count();
//            image_load_times.push_back(load_time);
//
//            try {
//                // 执行推理
//                auto infer_start = std::chrono::high_resolution_clock::now();
//                auto result = InferenceWorker::RunClassification(0, model_path, classes, image, 0.1f);
//                auto infer_end = std::chrono::high_resolution_clock::now();
//
//                // 记录推理时间
//                auto infer_time = std::chrono::duration_cast<std::chrono::milliseconds>(infer_end - infer_start).count();
//                inference_times.push_back(infer_time);
//
//                // 更新统计数据
//                total_images++;
//
//                // 判断分类是否正确
//                bool is_correct = (result.className == class_name && result.confidence > 0.5f);
//
//                if (is_correct) {
//                    correct_classifications++;
//                    continue;
//                }
//                else {
//                    incorrect_classifications++;
//                }
//
//                // 复制图像到对应的OK或NG文件夹
//                std::string filename = fs::path(file_path).filename().string();
//                std::string safe_name = class_to_safe_name[class_name];
//                std::string destination_dir = output_dir + "/" + safe_name + "/" + (is_correct ? "OK" : "NG");
//                std::string destination_path = destination_dir + "/" + result.className + "_" + std::to_string(result.confidence) + "_" + filename;
//
//                fs::copy_file(file_path, destination_path, fs::copy_options::overwrite_existing);
//
//                std::cout << destination_path <<  std::endl;
//                // 显示进度
//                if (total_images % 10 == 0) {
//                    std::cout << "Processed " << total_images << " images" << std::endl;
//                }
//            }
//            catch (const cv::Exception& e) {
//                std::cerr << "OpenCV DNN error processing " << file_path << ": " << e.what() << std::endl;
//            }
//            catch (const std::exception& e) {
//                std::cerr << "Error processing " << file_path << ": " << e.what() << std::endl;
//            }
//        }
//    }
//
//    // 8. 计算总体统计信息
//    auto batch_end_time = std::chrono::high_resolution_clock::now();
//    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(batch_end_time - batch_start_time).count();
//
//    // 计算平均值
//    double avg_load_time = (total_images > 0) ?
//        std::accumulate(image_load_times.begin(), image_load_times.end(), 0.0) / total_images : 0;
//    double avg_infer_time = (total_images > 0) ?
//        std::accumulate(inference_times.begin(), inference_times.end(), 0.0) / total_images : 0;
//
//    // 计算FPS
//    double total_infer_time_ms = std::accumulate(inference_times.begin(), inference_times.end(), 0.0);
//    double fps = (total_images > 0 && total_infer_time_ms > 0) ?
//        (total_images * 1000.0) / total_infer_time_ms : 0;
//
//    // 计算准确率
//    double accuracy = (total_images > 0) ?
//        (static_cast<double>(correct_classifications) / total_images) * 100.0 : 0.0;
//
//    // 9. 打印性能报告
//    std::cout << "\n============== CLASSIFICATION PERFORMANCE REPORT ==============" << std::endl;
//    std::cout << "Model:                  " << model_path << std::endl;
//    std::cout << "Base Folder:            " << base_image_path << std::endl;
//    std::cout << "Total images processed: " << total_images << std::endl;
//    std::cout << "Correct classifications:" << correct_classifications << std::endl;
//    std::cout << "Incorrect classifications:" << incorrect_classifications << std::endl;
//    std::cout << "Accuracy:               " << std::fixed << std::setprecision(2) << accuracy << "%" << std::endl;
//    std::cout << "Total processing time:  " << total_duration << " ms" << std::endl;
//    std::cout << "Average image load time:" << std::fixed << std::setprecision(2) << avg_load_time << " ms" << std::endl;
//    std::cout << "Average inference time: " << std::fixed << std::setprecision(2) << avg_infer_time << " ms" << std::endl;
//    std::cout << "FPS (inference only):   " << std::fixed << std::setprecision(2) << fps << std::endl;
//    std::cout << "FPS (total):            " << std::fixed << std::setprecision(2)
//        << (total_images * 1000.0 / total_duration) << std::endl;
//
//    // 打印缺失文件夹的类别
//    if (!missing_class_folders.empty()) {
//        std::cout << "\nClasses with missing folders (" << missing_class_folders.size() << "):" << std::endl;
//        for (const auto& class_name : missing_class_folders) {
//            std::cout << "  " << class_name << std::endl;
//        }
//    }
//    std::cout << "===============================================================" << std::endl;
//
//    // 10. 保存性能报告到文件
//    std::ofstream report_file(output_dir + "/classification_performance_report.txt");
//    if (report_file.is_open()) {
//        report_file << "Classification Performance Report\n";
//        report_file << "=================================\n";
//        report_file << "Model: " << model_path << "\n";
//        report_file << "Base Folder: " << base_image_path << "\n";
//        report_file << "Total images: " << total_images << "\n";
//        report_file << "Correct classifications: " << correct_classifications << "\n";
//        report_file << "Incorrect classifications: " << incorrect_classifications << "\n";
//        report_file << "Accuracy: " << std::fixed << std::setprecision(2) << accuracy << "%\n";
//        report_file << "Total time: " << total_duration << " ms\n";
//        report_file << "Avg load time: " << avg_load_time << " ms\n";
//        report_file << "Avg infer time: " << avg_infer_time << " ms\n";
//        report_file << "FPS (inference): " << fps << "\n";
//        report_file << "FPS (total): " << (total_images * 1000.0 / total_duration) << "\n";
//
//        if (!missing_class_folders.empty()) {
//            report_file << "\nClasses with missing folders:\n";
//            for (const auto& class_name : missing_class_folders) {
//                report_file << "  " << class_name << "\n";
//            }
//        }
//
//        report_file.close();
//        std::cout << "\nSaved performance report to: " << output_dir << "/classification_performance_report.txt" << std::endl;
//    }
//
//    return 0;
//}