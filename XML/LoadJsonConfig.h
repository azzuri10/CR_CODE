#pragma once
#include "HeaderDefine.h"
#include "InspCodeStruct.h"
#include <json.hpp>
#include "Log.h"
#include <map>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <iomanip>
#include <fcntl.h>
#include <regex>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
using namespace std;
// 日期结构
#include <string>
#include <vector>
#include <ctime>
#include <regex>
#include <cstdio>
#include <locale>
#include <codecvt>

#include <string>
#include <regex>
#include <vector>
#include <ctime>
#include <cstdio>


struct Date {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    bool valid = false;

    Date() : year(0), month(0), day(0), hour(0), minute(0), second(0), valid(false) {}

    // --- 内部辅助函数 ---
    static bool isValidDate(int y, int m, int d, int h, int min, int s) {
        if (y < 1 || m < 1 || m > 12) return false;
        static const int daysInMonth[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
        int dim = daysInMonth[m - 1];
        if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) {
            dim = 29;
        }
        if (d < 1 || d > dim) return false;
        if (h < 0 || h > 23) return false;
        if (min < 0 || min > 59) return false;
        if (s < 0 || s > 59) return false;
        return true;
    }

    static time_t toTimeT(const Date& d) {
        tm t = {};
        t.tm_year = d.year - 1900;
        t.tm_mon = d.month - 1;
        t.tm_mday = d.day;
        t.tm_hour = d.hour;
        t.tm_min = d.minute;
        t.tm_sec = d.second;
        return mktime(&t);
    }

    // --- 从字符串解析日期 ---
    static Date fromString(const std::string& str) {
        Date date;
        static const std::vector<std::regex> patterns = {
            std::regex(R"(^(\d{4})$)"),                                         // YYYY
            std::regex(R"(^(\d{4})(\d{2})$)"),                                  // YYYYMM
            std::regex(R"(^(\d{4})(\d{2})(\d{2})$)"),                           // YYYYMMDD
            std::regex(R"(^(\d{4})(\d{2})(\d{2})(\d{2})$)"),                    // YYYYMMDDHH
            std::regex(R"(^(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})$)"),             // YYYYMMDDHHMM
            std::regex(R"(^(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})$)"),      // YYYYMMDDHHMMSS
            std::regex(R"(^(\d{4})[.-](\d{2})[.-](\d{2})$)"),                   // YYYY.MM.DD 或 YYYY-MM-DD
            std::regex(R"(^(\d{4})年(\d{1,2})月(\d{1,2})日$)"),                  // YYYY年MM月DD日
            std::regex(R"(^(\d{4})/(\d{2})$)"),                                 // YYYY/MM
            std::regex(R"(^(\d{4})/(\d{2})/(\d{2})$)"),                         // YYYY/MM/DD
            std::regex(R"(^(\d{4})/(\d{2})/(\d{2})(\d{2})$)"),         // YYYY/MM/DDHH
            std::regex(R"(^(\d{4})/(\d{2})/(\d{2})(\d{2}):(\d{2})$)"),         // YYYY/MM/DDHH:MM
            std::regex(R"(^(\d{4})/(\d{2})/(\d{2})(\d{2}):(\d{2}):(\d{2})$)"), // YYYY/MM/DDHH:MM:SS
            std::regex(R"(^(\d{4})-(\d{2})-(\d{2})(\d{2}):(\d{2}):(\d{2})$)")  // YYYY-MM-DDHH:MM:SS
        };

        for (const auto& pat : patterns) {
            std::smatch match;
            if (std::regex_match(str, match, pat)) {
                try {
                    int y = std::stoi(match[1].str());
                    int M = (match.size() > 2) ? std::stoi(match[2].str()) : 1;
                    int d = (match.size() > 3) ? std::stoi(match[3].str()) : 1;
                    int h = (match.size() > 4) ? std::stoi(match[4].str()) : 0;
                    int m = (match.size() > 5) ? std::stoi(match[5].str()) : 0;
                    int s = (match.size() > 6) ? std::stoi(match[6].str()) : 0;

                    if (isValidDate(y, M, d, h, m, s)) {
                        date.year = y; date.month = M; date.day = d;
                        date.hour = h; date.minute = m; date.second = s;
                        date.valid = true;
                        return date;
                    }
                }
                catch (...) {
                    continue;
                }
            }
        }
        return date; // 无效
    }

    // --- 格式化为字符串 (YYYYMMDDHHMMSS) ---
    std::string toString() const {
        char buffer[20];
        snprintf(buffer, sizeof(buffer), "%04d%02d%02d%02d%02d%02d",
            year, month, day, hour, minute, second);
        return buffer;
    }

    // --- 计算天数差 ---
    int daysBetween(const Date& other) const {
        if (!valid || !other.valid) return INT_MAX;
        time_t t1 = toTimeT(*this);
        time_t t2 = toTimeT(other);
        return static_cast<int>(difftime(t1, t2) / 86400);
    }
    int hoursBetween(const Date& other) const {
        if (!valid || !other.valid) return INT_MAX;

        time_t t1 = toTimeT(*this);
        time_t t2 = toTimeT(other);
        return static_cast<int>(difftime(t1, t2) / 3600);
    }
    // --- 计算分钟差 ---
    int minutesBetween(const Date& other) const {
        if (!valid || !other.valid) return INT_MAX;
        time_t t1 = toTimeT(*this);
        time_t t2 = toTimeT(other);
        return static_cast<int>(difftime(t1, t2) / 60);
    }

    // --- 添加天数 ---
    Date addDays(int days) const {
        Date result = *this;
        time_t rawtime = toTimeT(*this);
        if (rawtime == -1) { result.valid = false; return result; }
        rawtime += days * 24 * 60 * 60;

        tm newtime{};
#if defined(_WIN32)
        _localtime64_s(&newtime, &rawtime);
#else
        localtime_r(&rawtime, &newtime);
#endif

        result.year = newtime.tm_year + 1900;
        result.month = newtime.tm_mon + 1;
        result.day = newtime.tm_mday;
        result.hour = newtime.tm_hour;
        result.minute = newtime.tm_min;
        result.second = newtime.tm_sec;
        result.valid = true;
        return result;
    }

    // --- 有效性检查 ---
    explicit operator bool() const { return valid; }
};

wstring utf8_to_wstring(const string& str);
string wstring_to_utf8(const wstring& wstr);

int LoadConfigYOLO(
	const std::string detectConfigFile,
	std::vector<YoloConfig>& details,
	std::vector<std::string>& locateClassName,
	const std::string logFileName);

int LoadConfigMatch(
	const std::string& matchConfigFile,
	std::vector<MatchConfig>& details,
	const std::string& logFileName);

int LoadConfigBar(
	const std::string& configFile,
	std::vector<BarConfig>& barConfigs,
	const std::string& logFileName = "");

int LoadConfigCodeBasic(
	const std::string codeBasicConfigFile,
	CodeBasic& details,
	const std::string logFileName);

int LoadConfigCodeClassfy(
	const std::string codeClassfyConfigFile,
	std::vector<CodeClassfy>& details,
	std::vector<std::string>& ClassFyName,
	const std::string logFileName);

CodeInfo readConfig(const std::string& configPath);

int LoadConfigBottleType(
    const std::string bottleTypeConfigFile,
    std::vector<BottleType>& details,
    const std::string logFileName);

int LoadConfigMatchLocate(
    const std::string& matchConfigFile,
    MatchLocateConfig& details,
    const std::string& logFileName);


