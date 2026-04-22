#include "Log.h"
#include <filesystem>

Log::Log() {
    COM = new Common;
}

Log::~Log() {
    delete COM;
    FlushAll();
}

thread_local std::unique_ptr<Log> g_threadLog = std::make_unique<Log>();

Log& Log::Instance() {
    static Log instance;
    return instance;
}

std::string Log::LogRankToString(LogRank logRank) {
    switch (logRank) {
    case INFO:    return "INFO";
    case WARNING: return "WARNING";
    case ERR:     return "ERROR";
    case FATAL:   return "FATAL";
    default:      return "UNKNOW";
    }
}

std::string Log::GetTimestamp() {
    // ��ȡ��ǰʱ��㣨��ȷ�����룩
    auto now = std::chrono::system_clock::now();

    // ת��Ϊʱ��ṹ���뼶���ȣ�
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm time_info;
#if defined(_WIN32)
    localtime_s(&time_info, &in_time_t);
#else
    localtime_r(&in_time_t, &time_info);
#endif

    // ��ȡ���벿��
    auto since_epoch = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch) % 1000;

    // ��ʽ�����
    std::ostringstream oss;
    oss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S")
        << "."
        << std::setw(3) << std::setfill('0') << millis.count(); // ǿ��3λ����
    return oss.str();
}

std::shared_ptr<Log::FileBuffer> Log::GetOrCreateWriter(const std::string& fileName) {
    if (fileName.empty()) {
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(writers_mutex_);
        auto it = file_writers_.find(fileName);
        if (it != file_writers_.end()) {
            return it->second;
        }
    }

    auto writer = std::make_shared<FileBuffer>();
    try {
        std::filesystem::path target(fileName);
        if (target.has_parent_path()) {
            std::filesystem::create_directories(target.parent_path());
        }
    }
    catch (...) {
        return nullptr;
    }

    writer->stream.open(fileName, std::ios::app);
    if (!writer->stream.is_open()) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(writers_mutex_);
    auto [it, inserted] = file_writers_.emplace(fileName, writer);
    if (!inserted) {
        return it->second;
    }
    return writer;
}

void Log::FlushAll() {
    auto& logger = Instance();
    std::lock_guard<std::mutex> lock(logger.writers_mutex_);
    for (auto& entry : logger.file_writers_) {
        auto& writer = entry.second;
        if (!writer) {
            continue;
        }
        std::lock_guard<std::mutex> fileLock(writer->mutex);
        if (!writer->pending.empty() && writer->stream.is_open()) {
            writer->stream << writer->pending;
            writer->pending.clear();
            writer->pendingLines = 0;
        }
        if (writer->stream.is_open()) {
            writer->stream.flush();
        }
    }
}
