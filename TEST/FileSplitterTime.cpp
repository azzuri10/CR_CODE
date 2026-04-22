//#include <iostream>
//#include <filesystem>
//#include <vector>
//#include <string>
//#include <algorithm>
//#include <iomanip>
//#include <sstream>
//#include <ctime>
//#include <regex>
//#include <set>
//#include <map>
//#include <chrono>
//
//namespace fs = std::filesystem;
//
//class TimeBasedFileSplitter {
//private:
//    struct FileInfo {
//        fs::path filePath;
//        std::time_t modifyTime;
//        uintmax_t size;
//    };
//
//    std::string sourceDir;
//    std::string targetDir;
//    std::string timeFormat;  // 时间格式: "day", "month", "year", 或自定义格式
//    bool recursive;
//    bool preserveSubfolders;  // 是否保留原始子文件夹结构
//
//    // 支持的图片扩展名
//    std::set<std::string> imageExtensions = {
//        ".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".tif",
//        ".gif", ".webp", ".heic", ".heif", ".raw", ".cr2",
//        ".nef", ".arw", ".dng", ".ico", ".svg", ".jfif", ".jp2"
//    };
//
//public:
//    TimeBasedFileSplitter(const std::string& src, const std::string& tgt,
//        const std::string& format, bool rec = true, bool preserve = false)
//        : sourceDir(src), targetDir(tgt), timeFormat(format),
//        recursive(rec), preserveSubfolders(preserve) {
//
//        // 转换为小写
//        std::transform(timeFormat.begin(), timeFormat.end(), timeFormat.begin(), ::tolower);
//    }
//
//    // 检查是否是图片文件
//    bool isImageFile(const fs::path& path) {
//        if (!fs::is_regular_file(path)) {
//            return false;
//        }
//
//        std::string ext = path.extension().string();
//        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
//
//        return imageExtensions.find(ext) != imageExtensions.end();
//    }
//
//    // 获取文件时间信息
//    FileInfo getFileInfo(const fs::path& filePath) {
//        FileInfo info;
//        info.filePath = filePath;
//
//        try {
//            // 获取文件大小
//            info.size = fs::file_size(filePath);
//
//            // 获取修改时间
//            auto ftime = fs::last_write_time(filePath);
//
//            // 转换为time_t（兼容C++17的方法）
//            // 注意：这是C++17的简化方法，在C++20中有更好的方法
//#if defined(_WIN32) || defined(_WIN64)
//    // Windows系统
//#if _MSC_VER >= 1920
//    // Visual Studio 2019及以上版本
//            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
//                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
//            info.modifyTime = std::chrono::system_clock::to_time_t(sctp);
//#else
//    // 较旧版本的Visual Studio
//    // 使用备用方法
//            info.modifyTime = decltype(ftime)::clock::to_time_t(ftime);
//#endif
//#else
//    // GCC/MinGW等
//            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
//                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
//            info.modifyTime = std::chrono::system_clock::to_time_t(sctp);
//#endif
//        }
//        catch (const std::exception& e) {
//            std::cerr << "无法获取文件时间: " << filePath << " - " << e.what() << std::endl;
//            info.size = 0;
//            info.modifyTime = std::time(nullptr);
//        }
//
//        return info;
//    }
//
//    // 将时间转换为字符串格式
//    std::string timeToString(std::time_t t, const std::string& format) {
//        std::tm* tm = std::localtime(&t);
//        if (!tm) {
//            return "unknown_time";
//        }
//
//        std::ostringstream oss;
//
//        if (format == "day") {
//            oss << std::put_time(tm, "%Y-%m-%d");
//        }
//        else if (format == "month") {
//            oss << std::put_time(tm, "%Y-%m");
//        }
//        else if (format == "year") {
//            oss << std::put_time(tm, "%Y");
//        }
//        else if (format == "hour") {
//            oss << std::put_time(tm, "%Y-%m-%d_%H");
//        }
//        else if (format == "minute") {
//            oss << std::put_time(tm, "%Y-%m-%d_%H-%M");
//        }
//        else if (format == "week") {
//            // 计算一年中的第几周
//            char weekStr[20];
//            strftime(weekStr, sizeof(weekStr), "%Y-W%U", tm);
//            oss << weekStr;
//        }
//        else if (format == "quarter") {
//            int quarter = (tm->tm_mon / 3) + 1;
//            oss << std::put_time(tm, "%Y") << "-Q" << quarter;
//        }
//        else if (format == "season") {
//            int season = (tm->tm_mon / 3) + 1;
//            std::string seasonName;
//            switch (season) {
//            case 1: seasonName = "Spring"; break;
//            case 2: seasonName = "Summer"; break;
//            case 3: seasonName = "Autumn"; break;
//            case 4: seasonName = "Winter"; break;
//            }
//            oss << std::put_time(tm, "%Y") << "_" << seasonName;
//        }
//        else if (format == "decade") {
//            int year = tm->tm_year + 1900;
//            int decadeStart = (year / 10) * 10;
//            int decadeEnd = decadeStart + 9;
//            oss << decadeStart << "-" << decadeEnd;
//        }
//        else if (format == "yymmdd") {
//            oss << std::put_time(tm, "%y%m%d");
//        }
//        else if (format == "mmddyy") {
//            oss << std::put_time(tm, "%m%d%y");
//        }
//        else if (format == "ddmmyy") {
//            oss << std::put_time(tm, "%d%m%y");
//        }
//        else {
//            // 使用自定义格式
//            try {
//                char buffer[256];
//                if (strftime(buffer, sizeof(buffer), format.c_str(), tm) > 0) {
//                    oss << buffer;
//                }
//                else {
//                    oss << std::put_time(tm, "%Y-%m-%d");  // 默认格式
//                }
//            }
//            catch (...) {
//                oss << std::put_time(tm, "%Y-%m-%d");  // 默认格式
//            }
//        }
//
//        return oss.str();
//    }
//
//    // 从文件名中提取时间（如果文件名包含时间戳）
//    bool tryParseTimeFromFilename(const std::string& filename, std::time_t& timeOut) {
//        // 常见的时间戳格式的正则表达式
//        std::vector<std::string> patterns = {
//            R"((\d{4})-(\d{2})-(\d{2})[T_\s](\d{2})-(\d{2})-(\d{2}))",
//            R"((\d{4})(\d{2})(\d{2})[T_\s](\d{2})(\d{2})(\d{2}))",
//            R"((\d{4})-(\d{2})-(\d{2}))",
//            R"((\d{4})(\d{2})(\d{2}))",
//            R"((\d{2})-(\d{2})-(\d{4}))",
//            R"((\d{2})\.(\d{2})\.(\d{4}))",
//            R"((\d{4})_(\d{2})_(\d{2}))",
//            R"(IMG_(\d{8})_(\d{6}))",
//            R"(PANO_(\d{8})_(\d{6}))",
//            R"(VID_(\d{8})_(\d{6}))"
//        };
//
//        for (const auto& patternStr : patterns) {
//            std::regex pattern(patternStr);
//            std::smatch match;
//            if (std::regex_search(filename, match, pattern)) {
//                try {
//                    std::tm tm = {};
//
//                    if (match.size() >= 4) {  // 至少有年月日
//                        if (match[1].length() == 4) {  // yyyy-mm-dd
//                            tm.tm_year = std::stoi(match[1]) - 1900;
//                            tm.tm_mon = std::stoi(match[2]) - 1;
//                            tm.tm_mday = std::stoi(match[3]);
//                        }
//                        else {  // dd-mm-yyyy
//                            tm.tm_mday = std::stoi(match[1]);
//                            tm.tm_mon = std::stoi(match[2]) - 1;
//                            tm.tm_year = std::stoi(match[3]) - 1900;
//                        }
//
//                        if (match.size() >= 7) {  // 包含时分秒
//                            tm.tm_hour = std::stoi(match[4]);
//                            tm.tm_min = std::stoi(match[5]);
//                            tm.tm_sec = std::stoi(match[6]);
//                        }
//                        else if (match.size() >= 4) {  // 只有年月日
//                            tm.tm_hour = 0;
//                            tm.tm_min = 0;
//                            tm.tm_sec = 0;
//                        }
//
//                        tm.tm_isdst = -1;  // 自动判断夏令时
//
//                        std::time_t t = std::mktime(&tm);
//                        if (t != -1) {
//                            timeOut = t;
//                            return true;
//                        }
//                    }
//                }
//                catch (const std::exception&) {
//                    continue;
//                }
//            }
//        }
//
//        return false;
//    }
//
//    // 收集所有图片文件
//    std::vector<FileInfo> collectImageFiles() {
//        std::vector<FileInfo> files;
//
//        try {
//            std::cout << "正在收集图片文件..." << std::endl;
//
//            int count = 0;
//
//            if (recursive) {
//                // 递归搜索
//                for (const auto& entry : fs::recursive_directory_iterator(sourceDir)) {
//                    if (fs::is_regular_file(entry.path()) && isImageFile(entry.path())) {
//                        FileInfo info = getFileInfo(entry.path());
//                        files.push_back(info);
//                        count++;
//
//                        if (count % 100 == 0) {
//                            std::cout << "\r已找到 " << count << " 个图片文件..." << std::flush;
//                        }
//                    }
//                }
//            }
//            else {
//                // 非递归搜索
//                for (const auto& entry : fs::directory_iterator(sourceDir)) {
//                    if (fs::is_regular_file(entry.path()) && isImageFile(entry.path())) {
//                        FileInfo info = getFileInfo(entry.path());
//                        files.push_back(info);
//                        count++;
//
//                        if (count % 100 == 0) {
//                            std::cout << "\r已找到 " << count << " 个图片文件..." << std::flush;
//                        }
//                    }
//                }
//            }
//
//            std::cout << "\r共找到 " << files.size() << " 个图片文件" << std::endl;
//
//        }
//        catch (const fs::filesystem_error& e) {
//            std::cerr << "遍历目录时出错: " << e.what() << std::endl;
//        }
//
//        return files;
//    }
//
//    // 安全地复制文件
//    bool safeCopyFile(const fs::path& source, const fs::path& destination) {
//        try {
//            // 创建目标目录
//            fs::create_directories(destination.parent_path());
//
//            // 如果目标文件已存在，添加数字后缀
//            fs::path destPath = destination;
//            int counter = 1;
//            while (fs::exists(destPath)) {
//                std::string stem = destination.stem().string();
//                std::string ext = destination.extension().string();
//                destPath = destination.parent_path() / (stem + "_" + std::to_string(counter) + ext);
//                counter++;
//            }
//
//            // 复制文件
//            fs::copy_file(source, destPath, fs::copy_options::overwrite_existing);
//            return true;
//
//        }
//        catch (const fs::filesystem_error& e) {
//            std::cerr << "复制文件失败: " << source << " -> " << destination
//                << " 错误: " << e.what() << std::endl;
//            return false;
//        }
//    }
//
//    // 安全地移动文件
//    bool safeMoveFile(const fs::path& source, const fs::path& destination) {
//        try {
//            // 创建目标目录
//            fs::create_directories(destination.parent_path());
//
//            // 如果目标文件已存在，添加数字后缀
//            fs::path destPath = destination;
//            int counter = 1;
//            while (fs::exists(destPath)) {
//                std::string stem = destination.stem().string();
//                std::string ext = destination.extension().string();
//                destPath = destination.parent_path() / (stem + "_" + std::to_string(counter) + ext);
//                counter++;
//            }
//
//            // 移动文件
//            fs::rename(source, destPath);
//            return true;
//
//        }
//        catch (const fs::filesystem_error& e) {
//            std::cerr << "移动文件失败: " << source << " -> " << destination
//                << " 错误: " << e.what() << std::endl;
//            return false;
//        }
//    }
//
//    // 将文件大小转换为可读格式
//    std::string formatFileSize(uintmax_t size) {
//        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
//        int unitIndex = 0;
//        double fileSize = static_cast<double>(size);
//
//        while (fileSize >= 1024 && unitIndex < 4) {
//            fileSize /= 1024;
//            unitIndex++;
//        }
//
//        std::ostringstream oss;
//        oss << std::fixed << std::setprecision(2) << fileSize << " " << units[unitIndex];
//        return oss.str();
//    }
//
//    // 执行分割操作
//    bool split(bool moveFiles = false) {
//        try {
//            // 检查源目录
//            if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir)) {
//                std::cerr << "错误: 源目录不存在或不是目录: " << sourceDir << std::endl;
//                return false;
//            }
//
//            // 收集所有图片文件
//            std::vector<FileInfo> files = collectImageFiles();
//            if (files.empty()) {
//                std::cout << "没有找到图片文件" << std::endl;
//                return true;
//            }
//
//            // 按时间分组
//            std::cout << "\n正在按时间分组..." << std::endl;
//            std::map<std::string, std::vector<FileInfo>> timeGroups;
//            std::map<std::string, std::vector<FileInfo>> noTimeFiles;  // 无法确定时间的文件
//
//            for (const auto& fileInfo : files) {
//                std::time_t fileTime = fileInfo.modifyTime;
//                std::string filename = fileInfo.filePath.filename().string();
//
//                // 尝试从文件名解析时间
//                std::time_t timeFromFilename;
//                if (tryParseTimeFromFilename(filename, timeFromFilename)) {
//                    // 如果文件名中的时间比修改时间更早，使用文件名中的时间
//                    if (timeFromFilename < fileTime) {
//                        fileTime = timeFromFilename;
//                    }
//                }
//
//                std::string timeKey = timeToString(fileTime, timeFormat);
//                timeGroups[timeKey].push_back(fileInfo);
//            }
//
//            std::cout << "创建了 " << timeGroups.size() << " 个时间组" << std::endl;
//
//            // 显示分组统计
//            std::cout << "\n时间分组统计:" << std::endl;
//            std::cout << "========================================" << std::endl;
//            for (const auto& [timeKey, group] : timeGroups) {
//                std::cout << "  " << timeKey << ": " << group.size() << " 个文件" << std::endl;
//            }
//            std::cout << "========================================\n" << std::endl;
//
//            // 询问用户确认
//            std::cout << "确认按以上分组处理文件吗？" << std::endl;
//            std::cout << "操作模式: " << (moveFiles ? "移动" : "复制") << "文件" << std::endl;
//            std::cout << "时间格式: " << timeFormat << std::endl;
//            std::cout << "目标目录: " << targetDir << std::endl;
//            std::cout << "\n输入 'y' 继续，其他任意键取消: ";
//
//            std::string response;
//            std::getline(std::cin, response);
//            if (response != "y" && response != "Y") {
//                std::cout << "操作已取消" << std::endl;
//                return false;
//            }
//
//            // 处理文件
//            std::cout << "\n开始处理文件..." << std::endl;
//
//            int totalProcessed = 0;
//            int successful = 0;
//            int failed = 0;
//            uintmax_t totalSize = 0;
//
//            for (const auto& [timeKey, group] : timeGroups) {
//                std::cout << "处理组: " << timeKey << " (" << group.size() << " 个文件)" << std::endl;
//
//                for (const auto& fileInfo : group) {
//                    // 构建目标路径
//                    fs::path targetPath = fs::path(targetDir) / timeKey;
//
//                    if (preserveSubfolders) {
//                        // 保留相对路径结构
//                        try {
//                            fs::path relativePath = fs::relative(fileInfo.filePath, sourceDir);
//                            targetPath /= relativePath;
//                        }
//                        catch (const fs::filesystem_error&) {
//                            // 如果无法计算相对路径，只使用文件名
//                            targetPath /= fileInfo.filePath.filename();
//                        }
//                    }
//                    else {
//                        // 只保留文件名
//                        targetPath /= fileInfo.filePath.filename();
//                    }
//
//                    // 执行操作
//                    bool result = moveFiles ?
//                        safeMoveFile(fileInfo.filePath, targetPath) :
//                        safeCopyFile(fileInfo.filePath, targetPath);
//
//                    if (result) {
//                        successful++;
//                        totalSize += fileInfo.size;
//                    }
//                    else {
//                        failed++;
//                    }
//
//                    totalProcessed++;
//
//                    // 显示进度
//                    if (totalProcessed % 10 == 0 || totalProcessed == files.size()) {
//                        double progress = (static_cast<double>(totalProcessed) / files.size()) * 100;
//                        std::cout << "\r进度: " << totalProcessed << "/" << files.size()
//                            << " (" << std::fixed << std::setprecision(1) << progress << "%)" << std::flush;
//                    }
//                }
//            }
//
//            // 显示统计信息
//            std::cout << "\n\n========================================" << std::endl;
//            std::cout << "处理完成！" << std::endl;
//            std::cout << "========================================" << std::endl;
//            std::cout << "源目录: " << fs::absolute(sourceDir).string() << std::endl;
//            std::cout << "目标目录: " << fs::absolute(targetDir).string() << std::endl;
//            std::cout << "时间格式: " << timeFormat << std::endl;
//            std::cout << "操作模式: " << (moveFiles ? "移动" : "复制") << std::endl;
//            std::cout << "----------------------------------------" << std::endl;
//            std::cout << "图片总数: " << files.size() << std::endl;
//            std::cout << "时间分组: " << timeGroups.size() << " 个" << std::endl;
//            std::cout << "成功: " << successful << " 个" << std::endl;
//            std::cout << "失败: " << failed << " 个" << std::endl;
//            std::cout << "总大小: " << formatFileSize(totalSize) << std::endl;
//
//            if (failed > 0) {
//                std::cout << "警告: 有 " << failed << " 个文件处理失败" << std::endl;
//            }
//
//            std::cout << "========================================" << std::endl;
//
//            return failed == 0;
//
//        }
//        catch (const std::exception& e) {
//            std::cerr << "处理过程中出错: " << e.what() << std::endl;
//            return false;
//        }
//    }
//};
//
//// 获取用户输入
//std::string getUserInput(const std::string& prompt, const std::string& defaultValue = "") {
//    std::cout << prompt;
//    if (!defaultValue.empty()) {
//        std::cout << " [" << defaultValue << "]";
//    }
//    std::cout << ": ";
//
//    std::string input;
//    std::getline(std::cin, input);
//
//    if (input.empty() && !defaultValue.empty()) {
//        return defaultValue;
//    }
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
//// 显示帮助信息
//void showHelp() {
//    std::cout << "\n时间格式选项:" << std::endl;
//    std::cout << "  day       - 按天分组 (YYYY-MM-DD)" << std::endl;
//    std::cout << "  month     - 按月分组 (YYYY-MM)" << std::endl;
//    std::cout << "  year      - 按年分组 (YYYY)" << std::endl;
//    std::cout << "  hour      - 按小时分组 (YYYY-MM-DD_HH)" << std::endl;
//    std::cout << "  minute    - 按分钟分组 (YYYY-MM-DD_HH-MM)" << std::endl;
//    std::cout << "  week      - 按周分组 (YYYY-WW)" << std::endl;
//    std::cout << "  quarter   - 按季度分组 (YYYY-QN)" << std::endl;
//    std::cout << "  season    - 按季节分组 (YYYY_Season)" << std::endl;
//    std::cout << "  decade    - 按十年分组 (YYYY-YYYY)" << std::endl;
//    std::cout << "  yymmdd    - 简短日期分组 (YYMMDD)" << std::endl;
//    std::cout << "  mmddyy    - 月日年分组 (MMDDYY)" << std::endl;
//    std::cout << "  ddmmyy    - 日月年分组 (DDMMYY)" << std::endl;
//    std::cout << "  (自定义)  - 使用strftime格式，如: %Y_%m_%d" << std::endl;
//
//    std::cout << "\n支持的图片格式:" << std::endl;
//    std::cout << "  .jpg, .jpeg, .png, .bmp, .tiff, .tif, .gif, .webp" << std::endl;
//    std::cout << "  .heic, .heif, .raw, .cr2, .nef, .arw, .dng, .ico, .svg" << std::endl;
//}
//
//int main() {
//    std::cout << "========================================" << std::endl;
//    std::cout << "      图片时间分割工具 v2.0" << std::endl;
//    std::cout << "      自动按时间分割图片文件" << std::endl;
//    std::cout << "========================================" << std::endl;
//
//    // 显示帮助
//    char showHelpChoice;
//    std::cout << "\n查看可用时间格式和图片格式? (y/n): ";
//    std::cin >> showHelpChoice;
//    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
//
//    if (showHelpChoice == 'y' || showHelpChoice == 'Y') {
//        showHelp();
//    }
//
//    // 获取源目录
//    std::string sourceDir;
//    while (true) {
//        sourceDir = getUserInput("\n请输入源文件夹路径");
//        if (sourceDir.empty()) {
//            std::cout << "错误: 路径不能为空" << std::endl;
//            continue;
//        }
//
//        if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir)) {
//            std::cout << "错误: 路径不存在或不是文件夹" << std::endl;
//            continue;
//        }
//
//        break;
//    }
//
//    // 获取目标目录
//    std::string targetDir = getUserInput("请输入目标文件夹路径", "output");
//    if (targetDir.empty()) {
//        targetDir = "output";
//    }
//
//    // 获取时间格式
//    std::cout << "\n请选择时间分组方式:" << std::endl;
//    std::cout << "1. 按天 (YYYY-MM-DD)" << std::endl;
//    std::cout << "2. 按月 (YYYY-MM)" << std::endl;
//    std::cout << "3. 按年 (YYYY)" << std::endl;
//    std::cout << "4. 按小时 (YYYY-MM-DD_HH)" << std::endl;
//    std::cout << "5. 按周 (YYYY-WW)" << std::endl;
//    std::cout << "6. 按季度 (YYYY-QN)" << std::endl;
//    std::cout << "7. 自定义格式" << std::endl;
//    std::cout << "8. 其他预设" << std::endl;
//
//    std::string timeFormat = "day";  // 默认
//    std::string formatChoice = getUserInput("请选择 (1-8)", "1");
//
//    if (formatChoice == "1") {
//        timeFormat = "day";
//    }
//    else if (formatChoice == "2") {
//        timeFormat = "month";
//    }
//    else if (formatChoice == "3") {
//        timeFormat = "year";
//    }
//    else if (formatChoice == "4") {
//        timeFormat = "hour";
//    }
//    else if (formatChoice == "5") {
//        timeFormat = "week";
//    }
//    else if (formatChoice == "6") {
//        timeFormat = "quarter";
//    }
//    else if (formatChoice == "7") {
//        timeFormat = getUserInput("请输入自定义时间格式 (strftime格式)", "%Y-%m-%d");
//    }
//    else if (formatChoice == "8") {
//        std::cout << "\n其他预设格式:" << std::endl;
//        std::cout << "a. 按分钟 (YYYY-MM-DD_HH-MM)" << std::endl;
//        std::cout << "b. 按季节 (YYYY_Season)" << std::endl;
//        std::cout << "c. 按十年 (YYYY-YYYY)" << std::endl;
//        std::cout << "d. 简短日期 (YYMMDD)" << std::endl;
//        std::cout << "e. 月日年 (MMDDYY)" << std::endl;
//        std::cout << "f. 日月年 (DDMMYY)" << std::endl;
//
//        std::string presetChoice = getUserInput("请选择 (a-f)", "a");
//        if (presetChoice == "a") timeFormat = "minute";
//        else if (presetChoice == "b") timeFormat = "season";
//        else if (presetChoice == "c") timeFormat = "decade";
//        else if (presetChoice == "d") timeFormat = "yymmdd";
//        else if (presetChoice == "e") timeFormat = "mmddyy";
//        else if (presetChoice == "f") timeFormat = "ddmmyy";
//        else timeFormat = "minute";
//    }
//
//    // 是否递归搜索子文件夹
//    std::string recursiveStr = getUserInput("是否包含子文件夹? (y/n)", "y");
//    bool recursive = (recursiveStr == "y" || recursiveStr == "Y" || recursiveStr == "1");
//
//    // 是否保留子文件夹结构
//    std::string preserveStr = getUserInput("是否保留原始子文件夹结构? (y/n)", "n");
//    bool preserveSubfolders = (preserveStr == "y" || preserveStr == "Y");
//
//    // 移动还是复制
//    std::cout << "\n请选择操作模式:" << std::endl;
//    std::cout << "1. 复制文件 (源文件保留)" << std::endl;
//    std::cout << "2. 移动文件 (源文件删除)" << std::endl;
//    std::string moveChoice = getUserInput("请选择 (1-2)", "1");
//    bool moveFiles = (moveChoice == "2");
//
//    // 显示配置
//    std::cout << "\n========================================" << std::endl;
//    std::cout << "配置信息:" << std::endl;
//    std::cout << "  源文件夹: " << sourceDir << std::endl;
//    std::cout << "  目标文件夹: " << targetDir << std::endl;
//    std::cout << "  时间格式: " << timeFormat << std::endl;
//    std::cout << "  操作模式: " << (moveFiles ? "移动" : "复制") << std::endl;
//    std::cout << "  包含子文件夹: " << (recursive ? "是" : "否") << std::endl;
//    std::cout << "  保留子文件夹结构: " << (preserveSubfolders ? "是" : "否") << std::endl;
//    std::cout << "========================================" << std::endl;
//
//    // 确认操作
//    std::string confirm = getUserInput("\n确认以上配置并开始处理? (y/n)", "n");
//    if (confirm != "y" && confirm != "Y") {
//        std::cout << "操作已取消" << std::endl;
//        return 0;
//    }
//
//    // 创建分割器并执行
//    TimeBasedFileSplitter splitter(sourceDir, targetDir, timeFormat, recursive, preserveSubfolders);
//
//    std::cout << "\n开始处理..." << std::endl;
//
//    auto startTime = std::chrono::steady_clock::now();
//    bool success = splitter.split(moveFiles);
//    auto endTime = std::chrono::steady_clock::now();
//    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
//
//    std::cout << "\n处理耗时: " << duration.count() << " 秒" << std::endl;
//
//    if (success) {
//        std::cout << "\n处理完成！" << std::endl;
//    }
//    else {
//        std::cout << "\n处理过程中出现错误" << std::endl;
//    }
//
//    std::cout << "\n按 Enter 键退出..." << std::endl;
//    std::cin.ignore();
//    std::cin.get();
//
//    return success ? 0 : 1;
//}