#include "Common.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <locale>
#include <codecvt>
#include <algorithm>
#include <array>

namespace fs = std::filesystem;
namespace {
inline bool LocalTimeSafe(const time_t* in, tm* out) {
#ifdef _WIN32
    return localtime_s(out, in) == 0;
#else
    return localtime_r(in, out) != nullptr;
#endif
}
}
std::string g_logSysTime_YMD;
std::string g_logSysTime_HMS;
std::string g_logSysTime_HS;
time_t g_curSysTime;
int g_year;
int g_month;
int g_day;
int g_hour;
int g_minute;
int g_second;

Common::Common() {
    GetCurrentTimeMsec();
}

Common::~Common() {}


std::string Common::GBKtoUTF8(const std::string& gbkStr) {
#ifdef _WIN32
    // ??GBK?????????????UTF-16??
    int wstrLen = MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, NULL, 0);
    if (wstrLen == 0) return "";

    std::wstring wstr(wstrLen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, &wstr[0], wstrLen);

    // ???????????UTF-16??????UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (utf8Len == 0) {
        return "";
    }

    std::string utf8Str(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8Str[0], utf8Len, NULL, NULL);
    if (!utf8Str.empty() && utf8Str.back() == '\0') {
        utf8Str.pop_back();
    }
    return utf8Str;
#else
    return gbkStr;
#endif
}

// UTF-8?GBK
std::string Common::UTF8toGBK(const std::string& utf8Str) {
#ifdef _WIN32
    // ??UTF-8?????????????UTF-16??
    int wstrLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
    if (wstrLen == 0) return "";

    std::wstring wstr(wstrLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wstr[0], wstrLen);

    // ???????????UTF-16??????GBK
    int gbkLen = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (gbkLen == 0) {
        return "";
    }

    std::string gbkStr(gbkLen, '\0');
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &gbkStr[0], gbkLen, NULL, NULL);
    if (!gbkStr.empty() && gbkStr.back() == '\0') {
        gbkStr.pop_back();
    }
    return gbkStr;
#else
    return utf8Str;
#endif
}

bool Common::IsValidUtf8(const std::string& str) {
    int bytes = 0;
    for (auto c : str) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (bytes == 0) {
            if (uc <= 0x7F) continue; // ASCII
            if (uc >= 0xC2 && uc <= 0xDF) bytes = 1;
            else if (uc >= 0xE0 && uc <= 0xEF) bytes = 2;
            else if (uc >= 0xF0 && uc <= 0xF4) bytes = 3;
            else return false; // ????????????
        }
        else {
            if ((uc & 0xC0) != 0x80) return false; // ????????????
            --bytes;
        }
    }
    return bytes == 0;
}

std::wstring Common::Utf8ToWideString(const std::string& utf8Str) {
    if (utf8Str.empty()) {
        return L"";
    }

    // ?????????????? UTF-8
    if (!IsValidUtf8(utf8Str)) {
        return TryFixEncoding(utf8Str);
}

#ifdef _WIN32
    // Windows ???
    int size_needed = MultiByteToWideChar(
        CP_UTF8, 0, utf8Str.c_str(), (int)utf8Str.size(), NULL, 0);
    if (size_needed <= 0) return L"";

    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(
        CP_UTF8, 0, utf8Str.c_str(), (int)utf8Str.size(),
        &wstr[0], size_needed);
    return wstr;
#else
    // Linux ???
    try {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(utf8Str);
    }
    catch (...) {
        return TryFixEncoding(utf8Str);
    }
#endif
}

std::wstring Common::TryFixEncoding(const std::string& str) {
#ifdef _WIN32
    // ??????????????? (????? GBK ?? GB2312)
    int size_needed = MultiByteToWideChar(
        CP_ACP, 0, str.c_str(), (int)str.size(), NULL, 0);
    if (size_needed <= 0) return L"";

    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(
        CP_ACP, 0, str.c_str(), (int)str.size(),
        &wstr[0], size_needed);
    return wstr;
#else
    // Linux ???????? locale ???
    std::setlocale(LC_ALL, "zh_CN.UTF-8");
    std::mbstate_t state = std::mbstate_t();
    const char* src = str.c_str();
    size_t len = std::mbsrtowcs(nullptr, &src, 0, &state);
    if (len == static_cast<size_t>(-1)) return L"";

    std::wstring wstr(len, L'\0');
    std::mbsrtowcs(&wstr[0], &src, len, &state);
    return wstr;
#endif
}

bool Common::FileExistsModern(const std::string& path) {
    return std::filesystem::exists(path);
}

bool Common::CreateDir(const std::string& dirName) {
    if (dirName.empty()) return false;
    try {
        if (fs::exists(dirName)) {
            return true;
        }
        return fs::create_directories(dirName);
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
        return false;
    }
}

std::string Common::DatetimeToString(time_t time) {
    std::ostringstream oss;
    tm tm_{};
    if (!LocalTimeSafe(&time, &tm_)) {
        return "";
    }
    oss << std::put_time(&tm_, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

time_t Common::StringToDatetime(const std::string& str) {
    std::istringstream iss(str);
    tm tm_ = {};
    iss >> std::get_time(&tm_, "%Y-%m-%d %H:%M:%S");
    return mktime(&tm_);
}

#ifdef _WIN32
std::wstring Common::StringToWString(const std::string& str, UINT codePage) {
    if (str.empty()) return std::wstring();

    int length = MultiByteToWideChar(codePage, 0, str.c_str(), -1, nullptr, 0);
    if (length == 0) return std::wstring();

    std::wstring wstr(length - 1, 0);
    MultiByteToWideChar(codePage, 0, str.c_str(), -1, &wstr[0], length);
    return wstr;
}

std::string Common::WStringToString(const std::wstring& wstr, UINT codePage) {
    if (wstr.empty()) return std::string();

    int length = WideCharToMultiByte(codePage, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length == 0) return std::string();

    std::string str(length - 1, 0);
    WideCharToMultiByte(codePage, 0, wstr.c_str(), -1, &str[0], length, nullptr, nullptr);
    return str;
}
#endif

std::string Common::UTF8ToGBK(const std::string& strUTF8) {
#ifdef _WIN32
    if (strUTF8.empty()) return std::string();
    std::wstring wstr = StringToWString(strUTF8, CP_UTF8);
    if (wstr.empty()) return strUTF8;
    return WStringToString(wstr, CP_ACP);
#else
    return strUTF8;
#endif
}

std::string Common::GBKToUTF8(const std::string& strGBK) {
#ifdef _WIN32
    if (strGBK.empty()) return std::string();
    std::wstring wstr = StringToWString(strGBK, CP_ACP);
    if (wstr.empty()) return strGBK;
    return WStringToString(wstr, CP_UTF8);
#else
    return strGBK;
#endif
}

std::vector<cv::Mat> Common::ReadImages(const std::string& imgFolder, int type) {
    std::cout << "Reading images, please wait..." << std::endl;
    std::vector<cv::Mat> images;

    if (!fs::exists(imgFolder)) {
        std::cout << "Folder does not exist!" << std::endl;
        return images;
    }

    std::vector<std::string> fn;
    cv::glob(imgFolder, fn, false);
    for (const auto& file : fn) {
        cv::Mat img = cv::imread(file, type);
        if (!img.empty()) {
            images.emplace_back(img);
        }
    }
    return images;
}

std::vector<HalconCpp::HObject> Common::ReadImagesHalcon(const std::string& imgFolder, int type) {
    std::cout << "Reading images, please wait..." << std::endl;
    std::vector<HalconCpp::HObject> images;

    HalconCpp::SetHcppInterfaceStringEncodingIsUtf8(false);
    if (!fs::exists(imgFolder)) {
        std::cout << "Folder does not exist!" << std::endl;
        return images;
    }

    std::vector<std::string> fn;
    cv::glob(imgFolder, fn, false);
    for (const auto& file : fn) {
        try {
            HalconCpp::HObject templateImg;
            HTuple hFileName(file.c_str());
            HalconCpp::ReadImage(&templateImg, hFileName);
            if (type == 0) {
                Rgb1ToGray(templateImg, &templateImg);
            }
            images.emplace_back(templateImg);
        }
        catch (HalconCpp::HException& e) {
            std::cerr << "Error: " << e.ErrorMessage() << std::endl;
        }
    }
    return images;
}

bool Common::Contains(const std::string& A, const std::string& B) {
    return A.find(B) != std::string::npos;
}

void Common::ReplaceBySystemTime(std::vector<std::vector<std::string>>& check_word_list, tm* curTimeTM, int period) {
    if (check_word_list.empty()) return;

    auto formatTime = [](int value) -> std::string {
        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0') << value;
        return oss.str();
    };

    std::string year_s = std::to_string(curTimeTM->tm_year + 1900);
    std::string month_s = formatTime(curTimeTM->tm_mon + 1);
    std::string day_s = formatTime(curTimeTM->tm_mday);
    std::string hour_s = formatTime(curTimeTM->tm_hour);
    std::string minute_s = formatTime(curTimeTM->tm_min);
    std::string second_s = formatTime(curTimeTM->tm_sec);

    time_t periodTime = mktime(curTimeTM) + period * 3600 * 24 + 8 * 3600;
    tm* periodTimeTm = gmtime(&periodTime);

    std::string year_e = std::to_string(periodTimeTm->tm_year + 1900);
    std::string month_e = formatTime(periodTimeTm->tm_mon + 1);
    std::string day_e = formatTime(periodTimeTm->tm_mday);
    std::string hour_e = formatTime(periodTimeTm->tm_hour);
    std::string minute_e = formatTime(periodTimeTm->tm_min);
    std::string second_e = formatTime(periodTimeTm->tm_sec);

    for (auto& row : check_word_list) {
        for (auto& word : row) {
            if (word == "YEAR_S") word = year_s;
            else if (word == "MONTH_S") word = month_s;
            else if (word == "DAY_S") word = day_s;
            else if (word == "HOUR_S") word = hour_s;
            else if (word == "MINUTE_S") word = minute_s;
            else if (word == "SECOND_S") word = second_s;
            else if (word == "YEAR_E") word = year_e;
            else if (word == "MONTH_E") word = month_e;
            else if (word == "DAY_E") word = day_e;
            else if (word == "HOUR_E") word = hour_e;
            else if (word == "MINUTE_E") word = minute_e;
            else if (word == "SECOND_E") word = second_e;
        }
    }
}

void Common::GetCurrentTimeMsec() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);

    struct tm sysTime;
    if (!LocalTimeSafe(&now_c, &sysTime)) {
        return;
    }

    g_year = sysTime.tm_year + 1900;
    g_month = sysTime.tm_mon + 1;
    g_day = sysTime.tm_mday;
    g_hour = sysTime.tm_hour;
    g_minute = sysTime.tm_min;
    g_second = sysTime.tm_sec;

    char ymd[32], hms[32];
    std::strftime(ymd, sizeof(ymd), "%Y_%m_%d", &sysTime);
    std::strftime(hms, sizeof(hms), "%H:%M:%S", &sysTime);
    g_logSysTime_YMD = ymd;
    g_logSysTime_HMS = hms;

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
        ) % 1000;
    g_logSysTime_HS = std::to_string(ms.count());
}

std::string Common::time_t2string(const time_t time_t_time) {
    tm localTm;
    if (!LocalTimeSafe(&time_t_time, &localTm)) {
        throw std::runtime_error("localtime conversion failed");
    }

    int year = localTm.tm_year + 1900;
    int month = localTm.tm_mon + 1;

    std::array<char, 64> buf{};
    snprintf(buf.data(), buf.size(),
        "%04d%02d%02d%02d%02d%02d",
        year, month, localTm.tm_mday,
        localTm.tm_hour, localTm.tm_min, localTm.tm_sec
    );

    return std::string(buf.data());
}

std::string Common::time_t2string_with_ms() {
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    tm local_tm;
    if (!LocalTimeSafe(&now_time, &local_tm)) {
        throw std::runtime_error("localtime_s failed");
    }

    auto since_epoch = now.time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch) % 1000;

    std::array<char, 64> buf{};
    snprintf(buf.data(), buf.size(),
        "%04d%02d%02d%02d%02d%02d%03d",
        local_tm.tm_year + 1900,
        local_tm.tm_mon + 1,
        local_tm.tm_mday,
        local_tm.tm_hour,
        local_tm.tm_min,
        local_tm.tm_sec,
        static_cast<int>(ms.count())
    );

    return std::string(buf.data());
}

std::string Common::normalizePath(const std::string& path) {
    std::string result;
    for (char c : path) {
        result.push_back(c == '\\' ? '/' : c);
    }
    size_t pos = 0;
    while ((pos = result.find("//", pos)) != std::string::npos) {
        result.replace(pos, 2, "/");
    }
    return result;
}

std::vector<std::string> Common::getImageFilesInDirectory(const std::string& directoryPath) {
    std::vector<std::string> imageFiles;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                std::string extension = entry.path().extension().string();
                std::string lowerExt = extension;
                std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(),
                    [](unsigned char c) { return std::tolower(c); });

                if (lowerExt == ".jpg" || lowerExt == ".jpeg" ||
                    lowerExt == ".png" || lowerExt == ".bmp" ||
                    lowerExt == ".tiff" || lowerExt == ".tif" ||
                    lowerExt == ".webp" || lowerExt == ".jp2") {
                    std::string reFilePath = normalizePath(filePath);
                    imageFiles.push_back(reFilePath);
                }
            }
        }
        std::sort(imageFiles.begin(), imageFiles.end());
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Directory error: " << directoryPath << " - " << e.what() << std::endl;
    }
    return imageFiles;
}

void Common::ReadImageWithChinesePath(HalconCpp::HObject* Image, const std::string& FileName) {
    try {
        std::string normalizedPath = normalizePath(FileName);
        HalconCpp::HTuple halconFileName;

#ifdef _WIN32
        std::string utf8Path = GBKToUTF8(normalizedPath);
        halconFileName = HalconCpp::HTuple(utf8Path.c_str());
#else
        halconFileName = HalconCpp::HTuple(normalizedPath.c_str());
#endif

        HalconCpp::ReadImage(Image, halconFileName);
    }
    catch (HalconCpp::HException& e) {
        std::string errorMsg = "HALCON error: ";
        errorMsg += e.ErrorMessage().Text();
        errorMsg += " [";
        errorMsg += e.ProcName().Text();
        errorMsg += "]";

#ifdef _WIN32
        errorMsg = UTF8ToGBK(errorMsg);
#endif
        std::cerr << errorMsg << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}