//#include <iostream>
//#include <filesystem>
//#include <vector>
//#include <string>
//#include <algorithm>
//#include <set>
//#include <limits>
//#include <unordered_map>
//
//namespace fs = std::filesystem;
//
//class FileSplitter {
//private:
//    std::string sourceDir;
//    std::string targetDir;
//    int batchSize;
//    std::vector<std::string> imageExtensions = { ".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".gif", ".webp" };
//    std::vector<std::string> jsonExtensions = { ".json" };
//
//public:
//    FileSplitter(const std::string& src, const std::string& tgt, int size)
//        : sourceDir(src), targetDir(tgt), batchSize(size) {}
//
//    // 检查是否是图片文件
//    bool isImageFile(const fs::path& path) {
//        if (!fs::is_regular_file(path)) {
//            return false;
//        }
//
//        std::string ext = path.extension().string();
//        // 转换为小写比较
//        std::transform(ext.begin(), ext.end(), ext.begin(),
//            [](unsigned char c) { return std::tolower(c); });
//
//        for (const auto& imgExt : imageExtensions) {
//            if (ext == imgExt) {
//                return true;
//            }
//        }
//        return false;
//    }
//
//    // 检查是否是JSON文件
//    bool isJsonFile(const fs::path& path) {
//        if (!fs::is_regular_file(path)) {
//            return false;
//        }
//
//        std::string ext = path.extension().string();
//        std::transform(ext.begin(), ext.end(), ext.begin(),
//            [](unsigned char c) { return std::tolower(c); });
//
//        for (const auto& jsonExt : jsonExtensions) {
//            if (ext == jsonExt) {
//                return true;
//            }
//        }
//        return false;
//    }
//
//    // 获取文件名（不带扩展名）
//    std::string getStem(const fs::path& path) {
//        return path.stem().string();
//    }
//
//    // 安全移动文件
//    bool safeMoveFile(const fs::path& source, const fs::path& destination) {
//        try {
//            // 如果目标文件已存在，先删除
//            if (fs::exists(destination)) {
//                fs::remove(destination);
//            }
//
//            // 创建目标目录（如果不存在）
//            fs::create_directories(destination.parent_path());
//
//            // 移动文件
//            fs::rename(source, destination);
//            return true;
//        }
//        catch (const fs::filesystem_error& e) {
//            std::cerr << "移动文件失败: " << source << " -> " << destination
//                << " 错误: " << e.what() << std::endl;
//            return false;
//        }
//    }
//
//    // 安全删除文件
//    bool safeDeleteFile(const fs::path& filePath) {
//        try {
//            if (fs::exists(filePath)) {
//                fs::remove(filePath);
//                return true;
//            }
//            return false;
//        }
//        catch (const fs::filesystem_error& e) {
//            std::cerr << "删除文件失败: " << filePath
//                << " 错误: " << e.what() << std::endl;
//            return false;
//        }
//    }
//
//    // 分割文件
//    bool split() {
//        int totalImages = 0;
//        int totalJsons = 0;
//        int movedImages = 0;
//        int movedJsons = 0;
//        int deletedJsons = 0;
//
//        try {
//            // 检查源目录是否存在
//            if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir)) {
//                std::cerr << "错误: 源目录不存在或不是目录: " << sourceDir << std::endl;
//                return false;
//            }
//
//            // 创建目标目录
//            if (!fs::exists(targetDir)) {
//                fs::create_directories(targetDir);
//            }
//
//            // 收集所有图片文件
//            std::vector<fs::path> imageFiles;
//            for (const auto& entry : fs::directory_iterator(sourceDir)) {
//                if (isImageFile(entry.path())) {
//                    imageFiles.push_back(entry.path());
//                }
//            }
//
//            // 收集所有JSON文件
//            std::vector<fs::path> jsonFiles;
//            for (const auto& entry : fs::directory_iterator(sourceDir)) {
//                if (isJsonFile(entry.path())) {
//                    jsonFiles.push_back(entry.path());
//                }
//            }
//
//            totalImages = imageFiles.size();
//            totalJsons = jsonFiles.size();
//
//            std::cout << "找到 " << totalImages << " 个图片文件" << std::endl;
//            std::cout << "找到 " << totalJsons << " 个JSON文件" << std::endl;
//
//            if (imageFiles.empty() && jsonFiles.empty()) {
//                std::cout << "源目录为空，无需处理" << std::endl;
//                return true;
//            }
//
//            // 按文件名排序
//            std::sort(imageFiles.begin(), imageFiles.end(),
//                [](const fs::path& a, const fs::path& b) {
//                    return a.filename().string() < b.filename().string();
//                });
//
//            std::sort(jsonFiles.begin(), jsonFiles.end(),
//                [](const fs::path& a, const fs::path& b) {
//                    return a.filename().string() < b.filename().string();
//                });
//
//            // 创建JSON文件名到路径的映射（不包括扩展名）
//            std::unordered_map<std::string, fs::path> jsonMap;
//            for (const auto& jsonPath : jsonFiles) {
//                std::string stem = getStem(jsonPath);
//                jsonMap[stem] = jsonPath;
//            }
//
//            // 收集有效的文件对（图片+JSON）和单独的图片文件
//            std::vector<std::pair<fs::path, fs::path>> filePairs;  // 图片+JSON对
//            std::vector<fs::path> imageOnlyFiles;  // 只有图片的文件
//
//            for (const auto& imagePath : imageFiles) {
//                std::string stem = getStem(imagePath);
//
//                auto it = jsonMap.find(stem);
//                if (it != jsonMap.end()) {
//                    // 找到对应的JSON文件
//                    filePairs.emplace_back(imagePath, it->second);
//                    // 从映射中移除，这样剩下的就是没有图片的JSON文件
//                    jsonMap.erase(it);
//                }
//                else {
//                    // 没有对应的JSON文件
//                    imageOnlyFiles.push_back(imagePath);
//                }
//            }
//
//            // 剩下在jsonMap中的就是只有JSON文件的情况
//            std::vector<fs::path> jsonOnlyFiles;
//            for (const auto& [stem, jsonPath] : jsonMap) {
//                jsonOnlyFiles.push_back(jsonPath);
//            }
//
//            std::cout << "找到 " << filePairs.size() << " 个图片-JSON文件对" << std::endl;
//            std::cout << "找到 " << imageOnlyFiles.size() << " 个只有图片的文件" << std::endl;
//            std::cout << "找到 " << jsonOnlyFiles.size() << " 个只有JSON的文件" << std::endl;
//
//            // 删除只有JSON的文件
//            if (!jsonOnlyFiles.empty()) {
//                std::cout << "\n开始删除只有JSON的文件..." << std::endl;
//                for (size_t i = 0; i < jsonOnlyFiles.size(); ++i) {
//                    if (safeDeleteFile(jsonOnlyFiles[i])) {
//                        deletedJsons++;
//                    }
//
//                    if ((i + 1) % 100 == 0 || (i + 1) == jsonOnlyFiles.size()) {
//                        std::cout << "  - 已删除 " << (i + 1) << "/"
//                            << jsonOnlyFiles.size() << " 个只有JSON的文件" << std::endl;
//                    }
//                }
//                std::cout << "已删除 " << deletedJsons << " 个只有JSON的文件" << std::endl;
//            }
//
//            // 计算批次数量
//            int totalFiles = filePairs.size() + imageOnlyFiles.size();
//            if (totalFiles == 0) {
//                std::cout << "没有需要分割的文件" << std::endl;
//                return true;
//            }
//
//            int totalBatches = (totalFiles + batchSize - 1) / batchSize;
//            std::cout << "\n将分割成 " << totalBatches << " 个批次，每批最多 "
//                << batchSize << " 个文件" << std::endl;
//
//            // 询问用户是否继续
//            std::cout << "\n按 Enter 键开始分割，或按 Ctrl+C 取消..." << std::endl;
//            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
//
//            // 创建所有批次的文件夹
//            for (int i = 0; i < totalBatches; ++i) {
//                std::string batchDir = targetDir + "/batch_" +
//                    std::to_string(i + 1);
//                // 添加前导零
//                if (i + 1 < 10) {
//                    batchDir = targetDir + "/batch_00" + std::to_string(i + 1);
//                }
//                else if (i + 1 < 100) {
//                    batchDir = targetDir + "/batch_0" + std::to_string(i + 1);
//                }
//                fs::create_directories(batchDir);
//            }
//
//            // 处理文件对（图片+JSON）
//            std::cout << "\n开始处理图片-JSON文件对..." << std::endl;
//            for (int batch = 0; batch < totalBatches; ++batch) {
//                int startIdx = batch * batchSize;
//                int endIdx = std::min(startIdx + batchSize, (int)filePairs.size());
//
//                if (startIdx >= endIdx) break;
//
//                int batchCount = endIdx - startIdx;
//
//                std::cout << "=== 处理批次 " << batch + 1 << "/" << totalBatches
//                    << " (" << batchCount << " 个文件对) ===" << std::endl;
//
//                std::string batchDir = targetDir + "/batch_" + std::to_string(batch + 1);
//                if (batch + 1 < 10) {
//                    batchDir = targetDir + "/batch_00" + std::to_string(batch + 1);
//                }
//                else if (batch + 1 < 100) {
//                    batchDir = targetDir + "/batch_0" + std::to_string(batch + 1);
//                }
//
//                for (int i = startIdx; i < endIdx; ++i) {
//                    const auto& [imagePath, jsonPath] = filePairs[i];
//
//                    // 移动图片文件
//                    if (safeMoveFile(imagePath, fs::path(batchDir) / imagePath.filename())) {
//                        movedImages++;
//                    }
//
//                    // 移动JSON文件
//                    if (safeMoveFile(jsonPath, fs::path(batchDir) / jsonPath.filename())) {
//                        movedJsons++;
//                    }
//
//                    int processed = i - startIdx + 1;
//                    if (processed % 50 == 0 || processed == batchCount) {
//                        int percent = (batchCount > 0) ? (processed * 100 / batchCount) : 0;
//                        std::cout << "  - 进度: " << processed << "/"
//                            << batchCount << " (" << percent << "%)" << std::endl;
//                    }
//                }
//
//                std::cout << "批次 " << batch + 1 << " 完成" << std::endl;
//            }
//
//            // 处理单独的图片文件
//            if (!imageOnlyFiles.empty()) {
//                std::cout << "\n开始处理单独的图片文件..." << std::endl;
//
//                // 计算从哪个批次开始继续
//                int startBatch = (filePairs.size() + batchSize - 1) / batchSize;
//
//                for (int batch = startBatch; batch < totalBatches; ++batch) {
//                    int startIdx = batch * batchSize - filePairs.size();
//                    int endIdx = std::min(startIdx + batchSize, (int)imageOnlyFiles.size());
//
//                    if (startIdx >= endIdx) break;
//
//                    int batchCount = endIdx - startIdx;
//
//                    std::cout << "=== 处理批次 " << batch + 1 << "/" << totalBatches
//                        << " (" << batchCount << " 个图片文件) ===" << std::endl;
//
//                    std::string batchDir = targetDir + "/batch_" + std::to_string(batch + 1);
//                    if (batch + 1 < 10) {
//                        batchDir = targetDir + "/batch_00" + std::to_string(batch + 1);
//                    }
//                    else if (batch + 1 < 100) {
//                        batchDir = targetDir + "/batch_0" + std::to_string(batch + 1);
//                    }
//
//                    for (int i = startIdx; i < endIdx; ++i) {
//                        const auto& imagePath = imageOnlyFiles[i];
//
//                        // 移动图片文件
//                        if (safeMoveFile(imagePath, fs::path(batchDir) / imagePath.filename())) {
//                            movedImages++;
//                        }
//
//                        int processed = i - startIdx + 1;
//                        if (processed % 50 == 0 || processed == batchCount) {
//                            int percent = (batchCount > 0) ? (processed * 100 / batchCount) : 0;
//                            std::cout << "  - 进度: " << processed << "/"
//                                << batchCount << " (" << percent << "%)" << std::endl;
//                        }
//                    }
//
//                    std::cout << "批次 " << batch + 1 << " 完成" << std::endl;
//                }
//            }
//
//            // 统计信息
//            std::cout << "\n========================================" << std::endl;
//            std::cout << "分割完成！" << std::endl;
//            std::cout << "========================================" << std::endl;
//            std::cout << "源目录: " << fs::absolute(sourceDir).string() << std::endl;
//            std::cout << "目标目录: " << fs::absolute(targetDir).string() << std::endl;
//            std::cout << "总计: " << totalFiles << " 个文件" << std::endl;
//            std::cout << "批次数量: " << totalBatches << " 个" << std::endl;
//            std::cout << "批次大小: " << batchSize << " 个文件/批次" << std::endl;
//            std::cout << "----------------------------------------" << std::endl;
//            std::cout << "已移动图片文件: " << movedImages << " 个" << std::endl;
//            std::cout << "已移动JSON文件: " << movedJsons << " 个" << std::endl;
//            std::cout << "已删除只有JSON的文件: " << deletedJsons << " 个" << std::endl;
//
//            // 检查源目录是否为空
//            int remainingFiles = 0;
//            for (const auto& entry : fs::directory_iterator(sourceDir)) {
//                remainingFiles++;
//            }
//
//            if (remainingFiles > 0) {
//                std::cout << "警告: 源目录中还有 " << remainingFiles << " 个文件未处理" << std::endl;
//            }
//            else {
//                std::cout << "源目录已清空" << std::endl;
//            }
//
//            std::cout << "========================================" << std::endl;
//
//            return true;
//
//        }
//        catch (const fs::filesystem_error& e) {
//            std::cerr << "文件系统错误: " << e.what() << std::endl;
//            return false;
//        }
//        catch (const std::exception& e) {
//            std::cerr << "错误: " << e.what() << std::endl;
//            return false;
//        }
//    }
//};
//
//// 清空输入缓冲区
//void clearInputBuffer() {
//    std::cin.clear();
//    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
//}
//
//// 获取用户输入的函数
//std::string getUserInput(const std::string& prompt) {
//    std::string input;
//    std::cout << prompt;
//    std::getline(std::cin, input);
//
//    // 去除首尾空格
//    size_t start = input.find_first_not_of(" \t\n\r");
//    size_t end = input.find_last_not_of(" \t\n\r");
//
//    if (start == std::string::npos || end == std::string::npos) {
//        return "";
//    }
//
//    return input.substr(start, end - start + 1);
//}
//
//int main() {
//    std::cout << "========================================" << std::endl;
//    std::cout << "      文件分割工具 v1.0" << std::endl;
//    std::cout << "  (剪切图片，删除单独JSON文件)" << std::endl;
//    std::cout << "========================================" << std::endl;
//    std::cout << std::endl;
//    std::cout << "警告: 此工具会修改源文件！" << std::endl;
//    std::cout << "      会将图片从源目录剪切到目标目录" << std::endl;
//    std::cout << "      会删除没有对应图片的JSON文件" << std::endl;
//    std::cout << "========================================" << std::endl;
//    std::cout << std::endl;
//
//    // 获取源目录路径
//    std::string sourceDir;
//    while (true) {
//        sourceDir = getUserInput("请输入源文件夹路径: ");
//        if (sourceDir.empty()) {
//            std::cout << "错误: 源文件夹路径不能为空，请重新输入" << std::endl;
//            continue;
//        }
//
//        // 检查路径是否存在
//        if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir)) {
//            std::cout << "错误: 路径不存在或不是文件夹，请重新输入" << std::endl;
//            continue;
//        }
//
//        // 检查源目录是否为空
//        int fileCount = 0;
//        for (const auto& entry : fs::directory_iterator(sourceDir)) {
//            fileCount++;
//        }
//
//        if (fileCount == 0) {
//            std::cout << "错误: 源文件夹为空，请重新输入" << std::endl;
//            continue;
//        }
//
//        std::cout << "源文件夹包含 " << fileCount << " 个文件/文件夹" << std::endl;
//        break;
//    }
//
//    // 获取目标目录路径
//    std::string targetDir = getUserInput("请输入目标文件夹路径 (默认: output): ");
//    if (targetDir.empty()) {
//        targetDir = "output";
//        std::cout << "使用默认目标文件夹: " << targetDir << std::endl;
//    }
//    else {
//        // 检查目标目录是否与源目录相同
//        if (fs::absolute(sourceDir) == fs::absolute(targetDir)) {
//            std::cout << "错误: 目标文件夹与源文件夹相同，这会导致问题" << std::endl;
//            std::string response = getUserInput("是否继续? (y/n): ");
//            if (response != "y" && response != "Y") {
//                std::cout << "操作已取消" << std::endl;
//                return 0;
//            }
//        }
//    }
//
//    // 获取批次大小
//    int batchSize = 1000;
//    while (true) {
//        std::string batchStr = getUserInput("请输入每批文件数量 (默认: 1000): ");
//        if (batchStr.empty()) {
//            batchSize = 1000;
//            std::cout << "使用默认批次大小: " << batchSize << std::endl;
//            break;
//        }
//
//        try {
//            batchSize = std::stoi(batchStr);
//            if (batchSize <= 0) {
//                std::cout << "错误: 批次大小必须是正整数，请重新输入" << std::endl;
//                continue;
//            }
//            if (batchSize > 10000) {
//                std::cout << "警告: 批次大小过大 (" << batchSize << ")，可能导致性能问题" << std::endl;
//                std::string response = getUserInput("是否继续? (y/n): ");
//                if (response != "y" && response != "Y") {
//                    continue;
//                }
//            }
//            break;
//        }
//        catch (const std::exception&) {
//            std::cout << "错误: 请输入有效的数字" << std::endl;
//        }
//    }
//
//    // 显示配置信息
//    std::cout << "\n========================================" << std::endl;
//    std::cout << "配置信息:" << std::endl;
//    std::cout << "  源文件夹: " << fs::absolute(sourceDir).string() << std::endl;
//    std::cout << "  目标文件夹: " << fs::absolute(targetDir).string() << std::endl;
//    std::cout << "  批次大小: " << batchSize << " 个文件/批次" << std::endl;
//    std::cout << "========================================" << std::endl;
//    std::cout << std::endl;
//
//    // 确认操作
//    std::cout << "警告: 此操作将执行以下操作:" << std::endl;
//    std::cout << "  1. 从源文件夹剪切图片文件到目标文件夹" << std::endl;
//    std::cout << "  2. 删除没有对应图片的JSON文件" << std::endl;
//    std::cout << "  3. 源文件夹中的文件将被移动或删除" << std::endl;
//    std::cout << std::endl;
//
//    std::string confirm = getUserInput("确认执行以上操作? (输入 'Y' 继续): ");
//    if (confirm != "Y" && confirm != "y") {
//        std::cout << "操作已取消" << std::endl;
//        return 0;
//    }
//
//    // 创建分割器并执行分割
//    FileSplitter splitter(sourceDir, targetDir, batchSize);
//
//    std::cout << "\n开始处理..." << std::endl;
//    if (!splitter.split()) {
//        std::cerr << "\n处理失败" << std::endl;
//        std::cout << "按 Enter 键退出..." << std::endl;
//        std::cin.get();
//        return 1;
//    }
//
//    std::cout << "\n按 Enter 键退出..." << std::endl;
//    std::cin.get();
//    return 0;
//}