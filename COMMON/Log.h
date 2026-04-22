#ifndef LOG_H
#define LOG_H

#include "Common.h" 
#include "../COMMON/AsyncLogge.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <memory>

enum LogRank {
    INFO,
    WARNING,
    ERR,
    FATAL
};

class Log {
public:
    Log();
    ~Log();

    // ͨ����־д�루�ɱ����ģ�壩
    template<typename... Args>
    bool WriteLog(const std::string& format, LogRank logRank, const std::string& fileName, bool isDebug, Args... args);

    // �첽��־�ӿ�
    template<typename... Args>
    static void WriteAsyncLog(const std::string& message, LogRank level,
        const std::string& logFile, bool saveLog, Args... args) {
        if (!saveLog) return;

        AsyncLogger::Instance().EnqueueLog([=] {
            Log::Instance().WriteLog(message, level, logFile, true, args...);
            });
    }

    static void FlushAll();
    static Log& Instance();

private:
    struct FileBuffer {
        std::ofstream stream;
        std::string pending;
        size_t pendingLines = 0;
        std::mutex mutex;
    };

    Common* COM;
    std::mutex writers_mutex_;
    std::unordered_map<std::string, std::shared_ptr<FileBuffer>> file_writers_;

    // ��ȡ��ǰʱ���
    std::string GetTimestamp();

    // ����־�ȼ�ת��Ϊ�ַ���
    std::string LogRankToString(LogRank logRank);

    // �ַ�����ʽ��
    template<typename... Args>
    std::string FormatString(const std::string& format, Args... args);

    std::shared_ptr<FileBuffer> GetOrCreateWriter(const std::string& fileName);
    static constexpr size_t kFlushBytesThreshold = 8192;
    static constexpr size_t kFlushLinesThreshold = 64;
};

// ģ��ʵ�ַ���ͷ�ļ���
template<typename... Args>
bool Log::WriteLog(const std::string& format, LogRank logRank, const std::string& fileName, bool isDebug, Args... args) {
    // ������ǵ���ģʽ������־�������ĳ����ֵ�����磬ֻ��¼WARNING���ϣ���������
    // �������LogRank����ֵԽ�󼶱�Խ�ߣ�����ERROR>WARNING>INFO>DEBUG
    if (!isDebug && logRank < LogRank::WARNING) {
        return true;
    }

    auto writer = GetOrCreateWriter(fileName);
    if (!writer || !writer->stream.is_open()) {
        // �������������ӱ�����־���������std::cerr
        return false;
    }

    std::string message;
    if constexpr (sizeof...(args) > 0) {
        // ʹ���ַ��������и�ʽ��
        std::ostringstream oss;
        (oss << ... << args); // �۵�����ʽչ�����в���
        message = format + " " + oss.str();
    }
    else {
        message = format;
    }

    std::lock_guard<std::mutex> lock(writer->mutex);
    writer->pending.append("[")
        .append(LogRankToString(logRank))
        .append("] ")
        .append(GetTimestamp())
        .append(" ")
        .append(message)
        .append("\n");
    ++writer->pendingLines;

    if (writer->pending.size() >= kFlushBytesThreshold ||
        writer->pendingLines >= kFlushLinesThreshold) {
        writer->stream << writer->pending;
        writer->stream.flush();
        writer->pending.clear();
        writer->pendingLines = 0;
    }
    return true;
}

template<typename... Args>
std::string Log::FormatString(const std::string& format, Args... args) {
    std::ostringstream oss;
    (oss << ... << args); // �۵�����ʽչ�����в���
    return format + " " + oss.str();
}

extern thread_local std::unique_ptr<Log> g_threadLog;
#endif // LOG_H