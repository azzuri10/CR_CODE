// AsyncLogger.cpp
#include "../COMMON/AsyncLogge.h"
#include "../COMMON/Log.h"
#include <iostream> // �����ڴ�����

AsyncLogger& AsyncLogger::Instance() {
    static AsyncLogger instance;
    return instance;
}

AsyncLogger::AsyncLogger() : m_stop(false) {
    m_logThread = std::thread(&AsyncLogger::ProcessLogQueue, this);
}

AsyncLogger::~AsyncLogger() {
    Stop();
}

void AsyncLogger::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }
    m_condition.notify_one();

    if (m_logThread.joinable()) {
        m_logThread.join();
    }
    Log::FlushAll();
}

void AsyncLogger::EnqueueLog(std::function<void()> logTask) {
    if (!logTask) return;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_logQueue.push(std::move(logTask));
    }
    m_condition.notify_one();
}

void AsyncLogger::ProcessLogQueue() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_condition.wait(lock, [this] {
                return m_stop || !m_logQueue.empty();
                });

            if (m_stop && m_logQueue.empty()) return;

            task = std::move(m_logQueue.front());
            m_logQueue.pop();
        }

        if (task) {
            try {
                task();
            }
            catch (const std::exception& e) {
                // �������׼���󣬱���ݹ���־����
                std::cerr << "Async log error: " << e.what() << std::endl;
            }
            catch (...) {
                std::cerr << "Unknown async log error" << std::endl;
            }
        }
    }
}