//// 在包含Windows头文件前定义这些宏
//#define NOMINMAX
//#define WIN32_LEAN_AND_MEAN
//#include <Windows.h>
//#include <commdlg.h>
//#include <shlobj.h>
//
//#include "HeaderDefine.h"
//#include "InferenceWorker.h"
//#include <filesystem>
//#include <map>
//#include <vector>
//#include <string>
//#include <fstream>
//#include <iostream>
//#include <chrono>
//#include <mutex>
//#include <numeric>
//#include <algorithm>
//#include <iomanip>
//#include <opencv2/opencv.hpp>
//#include <opencv2/dnn.hpp>
//
//namespace fs = std::filesystem;
//
//// 模型管理器类
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
//// 将类别名称转换为安全的文件名（用于文件创建，而不是文件夹路径）
//std::string toSafeFileName(const std::string& class_name) {
//    std::string safe_name = class_name;
//
//    // 替换Windows文件系统不允许的字符
//    std::vector<std::pair<char, char>> replacements = {
//        {'\\', '_'}, {'/', '_'}, {':', '_'}, {'*', '_'},
//        {'?', '_'}, {'"', '_'}, {'<', '_'}, {'>', '_'},
//        {'|', '_'}
//    };
//
//    for (const auto& [old_char, new_char] : replacements) {
//        std::replace(safe_name.begin(), safe_name.end(), old_char, new_char);
//    }
//
//    return safe_name;
//}
//
//// 确保文件夹名称是安全的
//std::string ensureSafeFolderName(const std::string& folder_name) {
//    std::string safe_name = folder_name;
//
//    // 替换Windows文件系统不允许的字符
//    std::vector<std::pair<char, char>> replacements = {
//        {'\\', '_'}, {'/', '_'}, {':', '_'}, {'*', '_'},
//        {'?', '_'}, {'"', '_'}, {'<', '_'}, {'>', '_'},
//        {'|', '_'}
//    };
//
//    for (const auto& [old_char, new_char] : replacements) {
//        std::replace(safe_name.begin(), safe_name.end(), old_char, new_char);
//    }
//
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
//// 生成三位数编号（000, 001, ..., 119）
//std::string toThreeDigitNumber(int num) {
//    if (num < 0 || num > 119) {  // 修改上限为119
//        std::cerr << "Warning: Number " << num << " is out of range (0-119), using 000 instead" << std::endl;
//        return "000";
//    }
//
//    if (num < 10) {
//        return "00" + std::to_string(num);
//    }
//    else if (num < 100) {
//        return "0" + std::to_string(num);
//    }
//    else {
//        return std::to_string(num);
//    }
//}
//
//// 获取编号文件夹对应的类别名称
//std::string getClassNameFromNumber(const std::vector<std::string>& classes, const std::string& folderNumber) {
//    try {
//        int index = std::stoi(folderNumber);
//        if (index >= 0 && index < static_cast<int>(classes.size())) {
//            return classes[index];
//        }
//        else {
//            std::cerr << "Error: Index " << index << " out of range [0, " << classes.size() - 1 << "]" << std::endl;
//        }
//    }
//    catch (const std::exception& e) {
//        std::cerr << "Error converting folder number to index: " << folderNumber << " - " << e.what() << std::endl;
//    }
//    return "unknown";
//}
//
//// 打开文件选择对话框，选择模型文件
//std::string OpenModelFileDialog() {
//    std::cout << "Please select ONNX model file..." << std::endl;
//    std::cout << "Press Enter to open file selection dialog...";
//    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
//    std::cin.get();
//
//    OPENFILENAMEA ofn = { 0 };
//    char szFile[260] = { 0 };
//
//    ZeroMemory(&ofn, sizeof(ofn));
//    ofn.lStructSize = sizeof(ofn);
//    ofn.hwndOwner = GetConsoleWindow();  // 使用控制台窗口作为父窗口
//    ofn.lpstrFile = szFile;
//    ofn.nMaxFile = sizeof(szFile);
//    ofn.lpstrFilter = "ONNX Model Files (*.onnx)\0*.onnx\0All Files (*.*)\0*.*\0";
//    ofn.nFilterIndex = 1;
//    ofn.lpstrFileTitle = NULL;
//    ofn.nMaxFileTitle = 0;
//    ofn.lpstrInitialDir = NULL;
//    ofn.lpstrTitle = "Select ONNX Model File";
//    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
//
//    if (GetOpenFileNameA(&ofn) == TRUE) {
//        std::string selected_file = szFile;
//        std::cout << "Selected: " << selected_file << std::endl;
//        return selected_file;
//    }
//    else {
//        DWORD error = CommDlgExtendedError();
//        if (error != 0) {
//            std::cerr << "Error opening file dialog: " << error << std::endl;
//        }
//        return "";
//    }
//}
//
//// 打开自定义的文件夹输入对话框
//std::string OpenFolderInputDialog() {
//    // 创建内存分配器
//    HGLOBAL hGlobal = GlobalAlloc(GMEM_ZEROINIT, 1024);
//    if (!hGlobal) {
//        std::cerr << "Error: Memory allocation failed" << std::endl;
//        return "";
//    }
//
//    char* buffer = (char*)GlobalLock(hGlobal);
//    if (!buffer) {
//        GlobalFree(hGlobal);
//        std::cerr << "Error: Failed to lock global memory" << std::endl;
//        return "";
//    }
//
//    // 显示输入对话框
//    std::cout << "\nPlease input folder path (should contain 000-119 subfolders):" << std::endl;
//    std::cout << "You can paste the path or type it manually." << std::endl;
//    std::cout << "Folder path: ";
//
//    // 读取用户输入的路径
//    std::string input_path;
//    std::getline(std::cin, input_path);
//
//    // 清理路径（移除首尾空格和引号）
//    if (!input_path.empty()) {
//        // 移除首尾空格
//        input_path.erase(0, input_path.find_first_not_of(" \t\n\r\f\v"));
//        input_path.erase(input_path.find_last_not_of(" \t\n\r\f\v") + 1);
//
//        // 移除引号
//        if (!input_path.empty() && input_path.front() == '"' && input_path.back() == '"') {
//            input_path = input_path.substr(1, input_path.length() - 2);
//        }
//
//        // 检查路径是否存在
//        std::filesystem::path path(input_path);
//
//        if (!std::filesystem::exists(path)) {
//            std::cerr << "Warning: Path does not exist: " << input_path << std::endl;
//
//            // 询问用户是否创建目录
//            std::cout << "The path does not exist. Would you like to create it? (y/n): ";
//            std::string create_response;
//            std::getline(std::cin, create_response);
//
//            if (create_response == "y" || create_response == "Y" ||
//                create_response == "yes" || create_response == "Yes") {
//                try {
//                    std::filesystem::create_directories(path);
//                    std::cout << "Directory created: " << input_path << std::endl;
//                }
//                catch (const std::exception& e) {
//                    std::cerr << "Error creating directory: " << e.what() << std::endl;
//                    GlobalUnlock(hGlobal);
//                    GlobalFree(hGlobal);
//                    return "";
//                }
//            }
//            else {
//                std::cout << "Please enter a valid path or press Enter to use folder browser..." << std::endl;
//
//                // 如果用户不想创建，提供浏览文件夹的选项
//                std::cout << "\nPress Enter to browse for folder, or 'c' to cancel: ";
//                std::string choice;
//                std::getline(std::cin, choice);
//
//                if (choice == "c" || choice == "C") {
//                    GlobalUnlock(hGlobal);
//                    GlobalFree(hGlobal);
//                    return "";
//                }
//
//                // 使用文件夹选择对话框
//                BROWSEINFOA bi = { 0 };
//                char szDisplayName[MAX_PATH] = { 0 };
//                char szPath[MAX_PATH] = { 0 };
//
//                bi.hwndOwner = GetConsoleWindow();
//                bi.pidlRoot = NULL;
//                bi.pszDisplayName = szDisplayName;
//                bi.lpszTitle = "Select Test Image Folder (should contain 000-119 subfolders)";
//                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
//
//                LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
//                if (pidl != NULL) {
//                    if (SHGetPathFromIDListA(pidl, szPath)) {
//                        input_path = szPath;
//                        std::cout << "Selected: " << input_path << std::endl;
//                    }
//
//                    // 释放内存
//                    IMalloc* imalloc = 0;
//                    if (SUCCEEDED(SHGetMalloc(&imalloc))) {
//                        imalloc->Free(pidl);
//                        imalloc->Release();
//                    }
//                }
//                else {
//                    std::cout << "No folder selected." << std::endl;
//                    GlobalUnlock(hGlobal);
//                    GlobalFree(hGlobal);
//                    return "";
//                }
//            }
//        }
//    }
//    else {
//        // 如果输入为空，使用文件夹选择对话框
//        std::cout << "\nNo input provided. Using folder browser instead..." << std::endl;
//
//        BROWSEINFOA bi = { 0 };
//        char szDisplayName[MAX_PATH] = { 0 };
//        char szPath[MAX_PATH] = { 0 };
//
//        bi.hwndOwner = GetConsoleWindow();
//        bi.pidlRoot = NULL;
//        bi.pszDisplayName = szDisplayName;
//        bi.lpszTitle = "Select Test Image Folder (should contain 000-119 subfolders)";
//        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
//
//        LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
//        if (pidl != NULL) {
//            if (SHGetPathFromIDListA(pidl, szPath)) {
//                input_path = szPath;
//                std::cout << "Selected: " << input_path << std::endl;
//            }
//
//            // 释放内存
//            IMalloc* imalloc = 0;
//            if (SUCCEEDED(SHGetMalloc(&imalloc))) {
//                imalloc->Free(pidl);
//                imalloc->Release();
//            }
//        }
//        else {
//            std::cout << "No folder selected." << std::endl;
//            GlobalUnlock(hGlobal);
//            GlobalFree(hGlobal);
//            return "";
//        }
//    }
//
//    GlobalUnlock(hGlobal);
//    GlobalFree(hGlobal);
//
//    return input_path;
//}
//
//int main(int argc, char* argv[]) {
//    // 设置控制台窗口标题
//    SetConsoleTitleA("Image Classification Tool");
//
//    // 默认参数
//    std::string model_path = "";
//    std::string class_names_path = "F:/TRAIN_SAMPLE_3.0/CODE/CLASS-TEST119.txt";
//    std::string base_image_path = "";
//
//    std::cout << "========================================" << std::endl;
//    std::cout << "    Image Classification Tool" << std::endl;
//    std::cout << "========================================" << std::endl;
//    std::cout << std::endl;
//
//    // 步骤1: 弹窗选择模型文件
//    std::cout << "STEP 1: Select ONNX Model File" << std::endl;
//    std::cout << "----------------------------------------" << std::endl;
//    model_path = OpenModelFileDialog();
//
//    if (model_path.empty()) {
//        std::cerr << "Error: No model file selected. Program will exit." << std::endl;
//        std::cout << "Press Enter to exit...";
//        std::cin.get();
//        return 0;
//    }
//
//    // 步骤2: 输入或选择测试文件夹
//    std::cout << "\n========================================" << std::endl;
//    std::cout << "STEP 2: Select Test Image Folder" << std::endl;
//    std::cout << "----------------------------------------" << std::endl;
//    std::cout << "The folder should contain 000-119 subfolders." << std::endl;
//    std::cout << "You can:" << std::endl;
//    std::cout << "1. Type or paste the folder path directly" << std::endl;
//    std::cout << "2. Press Enter to browse for folder" << std::endl;
//    std::cout << "3. Type 'c' to cancel" << std::endl;
//
//    base_image_path = OpenFolderInputDialog();
//
//    if (base_image_path.empty()) {
//        std::cerr << "Error: No test folder selected. Program will exit." << std::endl;
//        std::cout << "Press Enter to exit...";
//        std::cin.get();
//        return 0;
//    }
//
//    std::cout << "\n========================================" << std::endl;
//    std::cout << "    Selected Parameters" << std::endl;
//    std::cout << "========================================" << std::endl;
//    std::cout << "Model file: " << model_path << std::endl;
//    std::cout << "Test folder: " << base_image_path << std::endl;
//    std::cout << "Class file: " << class_names_path << std::endl;
//    std::cout << "========================================\n" << std::endl;
//
//    // 检查文件是否存在
//    if (!fs::exists(model_path)) {
//        std::cerr << "Error: Model file does not exist: " << model_path << std::endl;
//        std::cout << "Press Enter to exit...";
//        std::cin.get();
//        return -1;
//    }
//
//    if (!fs::exists(class_names_path)) {
//        std::cerr << "Error: Class names file does not exist: " << class_names_path << std::endl;
//        std::cout << "Press Enter to exit...";
//        std::cin.get();
//        return -1;
//    }
//
//    if (!fs::exists(base_image_path)) {
//        std::cerr << "Error: Test folder does not exist: " << base_image_path << std::endl;
//        std::cout << "Press Enter to exit...";
//        std::cin.get();
//        return -1;
//    }
//
//    // 加载类别
//    auto classes = loadClassNames(class_names_path);
//    if (classes.empty()) {
//        std::cerr << "Error: No classes loaded from: " << class_names_path << std::endl;
//        std::cout << "Press Enter to exit...";
//        std::cin.get();
//        return -1;
//    }
//
//    std::cout << "Loaded " << classes.size() << " classes from file." << std::endl;
//
//    // 验证类别数量是否足够120个
//    if (classes.size() < 120) {
//        std::cerr << "ERROR: Expected at least 120 classes (0-119), but only found "
//            << classes.size() << " classes in file." << std::endl;
//        std::cerr << "Please check your CLASS-TEST119.txt file has exactly 120 lines." << std::endl;
//        std::cout << "Press Enter to exit...";
//        std::cin.get();
//        return -1;
//    }
//
//    // 询问是否使用GPU加速
//    std::cout << "\n========================================" << std::endl;
//    std::cout << "    GPU Acceleration Setting" << std::endl;
//    std::cout << "========================================" << std::endl;
//    std::cout << "Do you want to use GPU acceleration? (y/n): ";
//
//    std::string use_gpu_input;
//    std::getline(std::cin, use_gpu_input);
//
//    int backend = 0;  // 0: CPU, 1: CUDA
//    bool use_gpu = (use_gpu_input == "y" || use_gpu_input == "Y" || use_gpu_input == "yes" || use_gpu_input == "Yes");
//
//    if (use_gpu) {
//        std::cout << "Using GPU acceleration (CUDA)." << std::endl;
//        backend = 1;
//    }
//    else {
//        std::cout << "Using CPU inference." << std::endl;
//    }
//
//    // 检查编号文件夹（000-119）
//    std::vector<std::pair<std::string, std::string>> valid_folders;
//    std::vector<std::string> missing_folders;
//
//    const int MAX_FOLDER_NUM = 119;  // 支持0-119共120个类别
//
//    std::cout << "\nChecking folder structure..." << std::endl;
//    for (int i = 0; i <= MAX_FOLDER_NUM; i++) {  // 包含119
//        std::string folder_number = toThreeDigitNumber(i);
//        std::string class_name = classes[i];  // 直接使用i作为索引
//        std::string folder_path = base_image_path + "/" + folder_number;
//
//        if (fs::exists(folder_path) && fs::is_directory(folder_path)) {
//            valid_folders.push_back({ folder_number, class_name });
//        }
//        else {
//            missing_folders.push_back(folder_number);
//        }
//    }
//
//    if (valid_folders.empty()) {
//        std::cerr << "Error: No valid folders found in: " << base_image_path << std::endl;
//        std::cerr << "Expected folders: 000, 001, ..., 119" << std::endl;
//        std::cout << "Press Enter to exit...";
//        std::cin.get();
//        return -1;
//    }
//
//    std::cout << "Valid folders found: " << valid_folders.size() << " out of 120" << std::endl;
//    std::cout << "Missing folders: " << missing_folders.size() << std::endl;
//
//    if (!missing_folders.empty()) {
//        std::cout << "\nMissing folders list (first 20):" << std::endl;
//        int count = 0;
//        for (const auto& folder_num : missing_folders) {
//            if (count >= 20) {
//                std::cout << "  ... and " << (missing_folders.size() - 20) << " more" << std::endl;
//                break;
//            }
//            std::string class_name = getClassNameFromNumber(classes, folder_num);
//            std::cout << "  " << folder_num << " -> " << class_name << std::endl;
//            count++;
//        }
//    }
//    std::cout << std::endl;
//
//    // 询问是否继续处理
//    std::cout << "Continue processing? (y/n): ";
//    std::string continue_input;
//    std::getline(std::cin, continue_input);
//
//    if (continue_input != "y" && continue_input != "Y" && continue_input != "yes" && continue_input != "Yes") {
//        std::cout << "Operation cancelled by user." << std::endl;
//        std::cout << "Press Enter to exit...";
//        std::cin.get();
//        return 0;
//    }
//
//    // 创建主结果目录和NG文件夹
//    const std::string output_dir = base_image_path + "/classification_results";
//    const std::string ng_dir = output_dir + "/NG";
//
//    if (!fs::exists(output_dir)) {
//        fs::create_directory(output_dir);
//    }
//    if (!fs::exists(ng_dir)) {
//        fs::create_directory(ng_dir);
//    }
//
//    std::cout << "\nOutput directory created: " << output_dir << std::endl;
//    std::cout << "NG directory created: " << ng_dir << std::endl;
//    std::cout << std::endl;
//
//    // 预热推理
//    std::cout << "Looking for an image for warmup..." << std::endl;
//    std::string first_image_path;
//
//    // 查找第一张图片进行预热（跳过placeholder.png）
//    for (const auto& folder_info : valid_folders) {
//        std::string folder_path = base_image_path + "/" + folder_info.first;
//        auto image_files = getImageFiles(folder_path);
//        for (const auto& file_path : image_files) {
//            std::string filename = fs::path(file_path).filename().string();
//            if (filename != "placeholder.png") {
//                first_image_path = file_path;
//                break;
//            }
//        }
//        if (!first_image_path.empty()) break;
//    }
//
//    if (first_image_path.empty()) {
//        std::cerr << "Error: No valid images found in any folder (excluding placeholder.png)" << std::endl;
//        std::cout << "Press Enter to exit...";
//        std::cin.get();
//        return -1;
//    }
//
//    cv::Mat warmup_image = cv::imread(first_image_path);
//    if (warmup_image.empty()) {
//        std::cerr << "Error: Could not load warmup image: " << first_image_path << std::endl;
//        std::cout << "Press Enter to exit...";
//        std::cin.get();
//        return -1;
//    }
//
//    std::cout << "Starting warmup with image: " << fs::path(first_image_path).filename() << std::endl;
//    std::cout << "Warmup progress: ";
//    for (int i = 0; i < 3; i++) {
//        try {
//            auto result = InferenceWorker::RunClassification(0, model_path, classes, warmup_image, 0.5f);
//            std::cout << ".";
//        }
//        catch (const std::exception& e) {
//            std::cerr << "Warmup error: " << e.what() << std::endl;
//        }
//    }
//    std::cout << " done" << std::endl;
//    std::cout << std::endl;
//
//    // 批量处理图片
//    std::cout << "Starting classification test..." << std::endl;
//    std::cout << std::endl;
//
//    // 性能统计
//    std::vector<long> inference_times;
//    std::vector<long> image_load_times;
//    int total_images = 0;
//    int correct_classifications = 0;
//    int incorrect_classifications = 0;
//    int skipped_placeholder = 0;
//
//    // 记录每个类别是否有NG图片
//    std::map<std::string, bool> has_ng_images;
//
//    // 记录总体开始时间
//    auto batch_start_time = std::chrono::high_resolution_clock::now();
//
//    // 处理每个有效文件夹
//    for (const auto& folder_info : valid_folders) {
//        std::string folder_number = folder_info.first;
//        std::string expected_class_name = folder_info.second;
//        std::string folder_path = base_image_path + "/" + folder_number;
//
//        auto image_files = getImageFiles(folder_path);
//        if (image_files.empty()) {
//            std::cout << "Folder " << folder_number << " (" << expected_class_name << "): No images found" << std::endl;
//            continue;
//        }
//
//        std::cout << "Processing folder " << folder_number << " (" << expected_class_name << "): "
//            << image_files.size() << " images" << std::endl;
//
//        for (const auto& file_path : image_files) {
//            // 跳过placeholder.png
//            std::string filename = fs::path(file_path).filename().string();
//            if (filename == "placeholder.png") {
//                skipped_placeholder++;
//                continue;
//            }
//
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
//                bool is_correct = (result.className == expected_class_name && result.confidence > 0.5f);
//
//                if (is_correct) {
//                    correct_classifications++;
//                }
//                else {
//                    incorrect_classifications++;
//                    // 标记这个类别有NG图片
//                    has_ng_images[expected_class_name] = true;
//
//                    // 移动NG图片到对应文件夹
//                    // 注意：使用原始类别名称，但需要确保文件夹名称是安全的
//                    std::string safe_class_name = ensureSafeFolderName(expected_class_name);
//                    std::string ng_class_dir = ng_dir + "/" + safe_class_name;
//
//                    // 如果这个类别的NG文件夹不存在，则创建它
//                    if (!fs::exists(ng_class_dir)) {
//                        fs::create_directory(ng_class_dir);
//                    }
//
//                    // 移动文件（剪切），使用安全的文件名
//                    std::string safe_filename = toSafeFileName(filename);
//                    std::string destination_path = ng_class_dir + "/" + safe_filename;
//
//                    try {
//                        fs::rename(file_path, destination_path);
//                        std::cout << "  NG: " << filename << " -> moved to NG/" << safe_class_name << "/" << std::endl;
//                    }
//                    catch (const fs::filesystem_error& e) {
//                        std::cerr << "  Error moving file " << file_path << ": " << e.what() << std::endl;
//
//                        // 如果因为文件名问题移动失败，尝试使用原始类别名称
//                        if (e.code() == std::make_error_code(std::errc::invalid_argument)) {
//                            try {
//                                // 创建一个更安全的文件名
//                                std::string timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
//                                std::string new_destination = ng_class_dir + "/" + timestamp + "_" + safe_filename;
//                                fs::rename(file_path, new_destination);
//                                std::cout << "  NG: " << filename << " -> moved to NG/" << safe_class_name << "/"
//                                    << timestamp << "_" << safe_filename << std::endl;
//                            }
//                            catch (const fs::filesystem_error& e2) {
//                                std::cerr << "  Error moving file (second attempt) " << file_path << ": " << e2.what() << std::endl;
//                            }
//                        }
//                    }
//                }
//
//                // 显示进度
//                if (total_images % 10 == 0) {
//                    std::cout << "  Processed " << total_images << " images" << std::endl;
//                }
//
//            }
//            catch (const cv::Exception& e) {
//                std::cerr << "  OpenCV DNN error processing " << file_path << ": " << e.what() << std::endl;
//            }
//            catch (const std::exception& e) {
//                std::cerr << "  Error processing " << file_path << ": " << e.what() << std::endl;
//            }
//        }
//    }
//
//    // 计算总体统计信息
//    auto batch_end_time = std::chrono::high_resolution_clock::now();
//    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(batch_end_time - batch_start_time).count();
//
//    // 计算平均时间
//    long avg_inference_time = 0;
//    long avg_load_time = 0;
//
//    if (!inference_times.empty()) {
//        avg_inference_time = std::accumulate(inference_times.begin(), inference_times.end(), 0L) / inference_times.size();
//    }
//
//    if (!image_load_times.empty()) {
//        avg_load_time = std::accumulate(image_load_times.begin(), image_load_times.end(), 0L) / image_load_times.size();
//    }
//
//    // 计算准确率
//    double accuracy = 0.0;
//    if (total_images > 0) {
//        accuracy = static_cast<double>(correct_classifications) / total_images * 100.0;
//    }
//
//    // 打印统计结果
//    std::cout << "\n" << std::string(60, '=') << std::endl;
//    std::cout << "CLASSIFICATION TEST RESULTS" << std::endl;
//    std::cout << std::string(60, '=') << std::endl;
//
//    std::cout << "Model: " << fs::path(model_path).filename() << std::endl;
//    std::cout << "Test folder: " << base_image_path << std::endl;
//    std::cout << "GPU acceleration: " << (use_gpu ? "Yes" : "No") << std::endl;
//    std::cout << std::endl;
//
//    std::cout << "Total images processed: " << total_images << std::endl;
//    std::cout << "Skipped placeholder.png files: " << skipped_placeholder << std::endl;
//    std::cout << "Correct classifications: " << correct_classifications << std::endl;
//    std::cout << "Incorrect classifications: " << incorrect_classifications << std::endl;
//    std::cout << "Accuracy: " << std::fixed << std::setprecision(2) << accuracy << "%" << std::endl;
//    std::cout << "Total processing time: " << total_duration << " ms" << std::endl;
//    std::cout << "Average inference time: " << avg_inference_time << " ms" << std::endl;
//    std::cout << "Average image load time: " << avg_load_time << " ms" << std::endl;
//
//    if (total_images > 0) {
//        double images_per_second = (total_images * 1000.0) / total_duration;
//        std::cout << "Images per second: " << std::fixed << std::setprecision(2) << images_per_second << std::endl;
//    }
//
//    // 打印有NG图片的类别
//    if (!has_ng_images.empty()) {
//        std::cout << "\nClasses with NG images (" << has_ng_images.size() << "):" << std::endl;
//        for (const auto& entry : has_ng_images) {
//            // 使用原始类别名称显示
//            std::cout << "  " << entry.first << std::endl;
//        }
//    }
//
//    // 打印缺失的文件夹
//    if (!missing_folders.empty()) {
//        std::cout << "\nMissing folders (" << missing_folders.size() << "):" << std::endl;
//        for (const auto& folder_num : missing_folders) {
//            std::string class_name = getClassNameFromNumber(classes, folder_num);
//            std::cout << "  " << folder_num << " -> " << class_name << std::endl;
//        }
//    }
//
//    std::cout << "\nResults saved to:" << std::endl;
//    std::cout << "  NG images: " << ng_dir << " (organized by class, using original class names)" << std::endl;
//    std::cout << "Note: Some characters in class names may be replaced with '_' for file system compatibility." << std::endl;
//    std::cout << std::string(60, '=') << std::endl;
//
//    std::cout << std::endl << "Process completed. Press Enter to exit...";
//    std::cin.get();
//
//    return 0;
//}