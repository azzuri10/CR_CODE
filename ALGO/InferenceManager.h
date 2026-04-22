// InferenceManager.h
#pragma once

#include <vector>
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <tbb/concurrent_unordered_map.h>
#include <atomic>
#include <stdexcept>
#include <tuple>

class InferenceManager {
public:
    static InferenceManager& Instance() {
        static InferenceManager instance;
        return instance;
    }

    // �ύ�������
    template<typename F, typename... Args>
    auto Submit(int priority, F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [func = std::forward<F>(f), params = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                return std::apply(std::move(func), std::move(params));
            }
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("InferenceManager is stopping, cannot accept new tasks.");
            }

            // ���ȼ����У������ȼ���������ִ��
            tasks_.push({
                priority,
                sequence_.fetch_add(1, std::memory_order_relaxed),
                [task] { (*task)(); }
                });
        }

        condition_.notify_one();
        return result;
    }

private:
    InferenceManager(size_t threads = std::thread::hardware_concurrency())
        : stop_(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] {
                            return stop_ || !tasks_.empty();
                            });

                        if (stop_ && tasks_.empty()) return;

                        task = std::move(tasks_.top());
                        tasks_.pop();
                    }

                    task.function();
                }
                });
        }
    }

    ~InferenceManager() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }

        condition_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    struct Task {
        int priority;
        uint64_t sequence;
        std::function<void()> function;

        bool operator<(const Task& rhs) const {
            if (priority != rhs.priority) {
                return priority < rhs.priority; // �������ȼ�����
            }
            return sequence > rhs.sequence; // ͬ���ȼ��Ƚ��ȳ�
        }
    };

    std::vector<std::thread> workers_;
    std::priority_queue<Task> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
    std::atomic<uint64_t> sequence_{ 0 };

    // ���ø��ƺ��ƶ�
    InferenceManager(const InferenceManager&) = delete;
    InferenceManager& operator=(const InferenceManager&) = delete;
};