#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include "HeaderDefine.h"
#include <direct.h>
#include "HalconCpp.h"
#include <vector>
#include <string>

class Common {
public:
    Common();
    ~Common(void);


    std::string GBKtoUTF8(const std::string& gbkStr);
    std::string UTF8toGBK(const std::string& utf8Str);
    std::string UTF8ToGBK1(const std::string& utf8);
    bool IsValidUtf8(const std::string& str);
    std::wstring Utf8ToWideString(const std::string& utf8Str);
   // std::wstring utf8_to_wstring(const std::string& str);
    std::wstring TryFixEncoding(const std::string& str);
    bool FileExistsModern(const std::string& path);
    bool CreateDir(const std::string& dirName);
    time_t StringToDatetime(const std::string& str);
    std::string DatetimeToString(time_t time);
    std::vector<cv::Mat> ReadImages(const std::string& imgFolder, int type);
    bool Contains(const std::string& A, const std::string& B);
    std::vector<HalconCpp::HObject> ReadImagesHalcon(const std::string& imgFolder, int type);
    void ReplaceBySystemTime(std::vector<std::vector<std::string>>& check_word_list, tm* curTimeTM, int period);
    bool ConfuseWords(std::string curStr, std::string keyStr,
        std::vector<std::vector<std::string>> confuseWord);
    bool ComparePartWords(std::vector<std::string> curStr, std::vector<std::string> keyStr,
        std::vector<std::vector<std::string>> confuseWord);
    bool CompareAllWords(std::vector<std::string> curStr, std::vector<std::string> keyStr,
        std::vector<std::vector<std::string>> confuseWords,
        std::vector<int>& resType);
    void GetCurrentTimeMsec();
    std::string time_t2string(const time_t time_t_time);
    std::string time_t2string_with_ms();

    // 移除了静态声明冲突，统一使用非静态版本
    std::string UTF8ToGBK(const std::string& strUTF8);
    std::string GBKToUTF8(const std::string& strGBK);

    std::string normalizePath(const std::string& path);
    std::vector<std::string> getImageFilesInDirectory(const std::string& directoryPath);

    // 修改为常量引用避免参数类型错误
    void ReadImageWithChinesePath(HalconCpp::HObject* Image, const std::string& FileName);

private:
#ifdef _WIN32
    static std::wstring StringToWString(const std::string& str, UINT codePage);
    static std::string WStringToString(const std::wstring& wstr, UINT codePage);
#endif

};

extern std::string g_logSysTime_YMD;
extern std::string g_logSysTime_HMS;
extern std::string g_logSysTime_HS;
extern time_t g_curSysTime;
extern int g_year;
extern int g_month;
extern int g_day;
extern int g_hour;
extern int g_minute;
extern int g_second;

#endif  // COMMON_H