// AsyncLogger.h
#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>

class AsyncLogger {
public:
    static AsyncLogger& Instance();

    void EnqueueLog(std::function<void()> logTask);
    void Stop();

private:
    AsyncLogger();
    ~AsyncLogger();

    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    void ProcessLogQueue();

    std::queue<std::function<void()>> m_logQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::thread m_logThread;
    std::atomic<bool> m_stop;
};