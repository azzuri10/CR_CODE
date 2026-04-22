// InferenceWorker.h
#pragma once
#ifndef INFERENCE_WORKER_H
#define INFERENCE_WORKER_H

#include "HeaderDefine.h"
#include <mutex>
#include <unordered_map>
#include <memory>
#include <tbb/concurrent_unordered_map.h>
#include <NvInfer.h> 
#include <shared_mutex>
#include "InferenceManager.h"
#include "AnalyseMat.h"
#include "../DETECT/yolo26_detector.h"

// Forward declarations
namespace nvinfer1 {
    class ILogger;
    class IRuntime;
    class ICudaEngine;
    class IExecutionContext;
}

class InferenceWorker {
public:
    struct TensorRTContext {
        std::shared_ptr<YoloDetector> detector;
        int inputWidth = 0;
        int inputHeight = 0;
        std::mutex init_mutex;

        nvinfer1::IRuntime* runtime = nullptr;
        nvinfer1::ICudaEngine* engine = nullptr;
        nvinfer1::IExecutionContext* context = nullptr;
        std::vector<float> prob;
        cudaStream_t stream = nullptr;
        std::vector<void*> buffers; // 注意：buffers有两个元素，输入和输出
        int outputWidth = 0;
        int outputHeight = 0;

    };


    static std::future<std::vector<FinsObject>> RunAsync(
        int cameraId,
        const std::string& model_path,
        const std::vector<std::string>& classes,
        const cv::Mat& input,
        float conf_threshold = 0.25f,
        float nms_threshold = 0.45f,
        int priority = 0) {

        return InferenceManager::Instance().Submit(
            priority,
            [=] {
                return Run(cameraId, model_path, classes, input,
                    conf_threshold, nms_threshold);
            }
        );
    }

    struct ModelPool {
        std::queue<std::shared_ptr<TensorRTContext>> idle_contexts;
        std::vector<std::shared_ptr<TensorRTContext>> all_contexts;
        std::mutex mutex;
    };

    static tbb::concurrent_unordered_map<std::string, ModelPool> model_pools_;

    static std::shared_ptr<TensorRTContext> AcquireModelContext(
        const std::string& model_key) {

        auto& pool = model_pools_[model_key];
        std::unique_lock<std::mutex> lock(pool.mutex);

        if (!pool.idle_contexts.empty()) {
            auto ctx = pool.idle_contexts.front();
            pool.idle_contexts.pop();
            return ctx;
        }

        // 创建新上下文
        auto ctx = std::make_shared<TensorRTContext>();
        // 初始化TensorRT上下文...
        pool.all_contexts.push_back(ctx);
        return ctx;
    }

    static void ReleaseModelContext(
        const std::string& model_key,
        std::shared_ptr<TensorRTContext> ctx) {

        auto& pool = model_pools_[model_key];
        std::unique_lock<std::mutex> lock(pool.mutex);
        pool.idle_contexts.push(ctx);
    }

    //************
    static std::vector<std::string> LoadClassNames(const std::string& filename);

    static std::vector<FinsObject> Run(
        int cameraId,
        const std::string& model_path,
        const std::vector<std::string>& classes,
        const cv::Mat& input,
        float conf_threshold = 0.25f,
        float nms_threshold = 0.45f);

    static std::vector<FinsObjectRotate> RunObb(
        int cameraId,
        const std::string& model_path,
        const std::vector<std::string>& classes,
        const cv::Mat& input,
        float conf_threshold = 0.25f,
        float nms_threshold = 0.45f);

    static FinsClassification RunClassification(
        int cameraId,
        const std::string& model_path,
        const std::vector<std::string>& classes,
        const cv::Mat& input,
        float conf_threshold = 0.25f);
    static std::vector<FinsClassification> RunClassificationBatch(
        int cameraId,
        const std::string& model_path,
        const std::vector<std::string>& classes,
        const std::vector<cv::Mat>& inputs,
        float conf_threshold = 0.25f);

    static std::vector<FinsObject> RunTensorrt(
        int cameraId,
        const std::string& model_path,
        const std::vector<std::string>& classes,
        const cv::Mat& input,
        float conf_threshold = 0.25f,
        float nms_threshold = 0.45f);

    static std::vector<FinsObjectSeg> RunSegmentation(
        int cameraId,
        const std::string& model_path,
        const std::vector<std::string>& classes,
        const cv::Mat& input,
        float conf_threshold = 0.25f,
        float nms_threshold = 0.45f);

    // 异步版本
    static std::future<std::vector<FinsObjectSeg>> RunSegmentationAsync(
        int cameraId,
        const std::string& model_path,
        const std::vector<std::string>& classes,
        const cv::Mat& input,
        float conf_threshold = 0.25f,
        float nms_threshold = 0.45f,
        int priority = 0);

    static std::pair<cv::Mat, int> PreprocessWithPadding(const cv::Mat& input, int target_width, int target_height);

private:
    // TensorRT上下文
    static std::vector<FinsObject> RunImpl(
        int cameraId,
        const std::string& model_path,
        const std::vector<std::string>& classes,
        const cv::Mat& input,
        float conf_threshold,
        float nms_threshold);

    static std::vector<FinsObjectSeg> RunSegmentationImpl(
        int cameraId,
        const std::string& model_path,
        const std::vector<std::string>& classes,
        const cv::Mat& input,
        float conf_threshold,
        float nms_threshold);

    // 线程池相关
    class ThreadPool {
    public:
        ThreadPool(size_t threads = 0);
        ~ThreadPool();

        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args)
            ->std::future<typename std::result_of<F(Args...)>::type>;
    private:
        // 工作线程
        std::vector<std::thread> workers;
        // 任务队列
        std::queue<std::function<void()>> tasks;

        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;
    };

    static ThreadPool pool;


    // 相机模型互斥体结构
    struct CameraModelMutex {
        std::mutex camera_mutex;  // 添加相机级锁
        std::unordered_map<std::string, std::shared_ptr<std::mutex>> model_mutexes;
        std::unordered_map<std::string, TensorRTContext> trt_contexts;
    };

    // Thread-safe storage for camera mutexes
    static tbb::concurrent_unordered_map<int, CameraModelMutex> camera_mutexes_;
    static std::shared_mutex camera_map_mutex_;

    /// @brief Get model-specific mutex for a camera
    static std::shared_ptr<std::mutex> GetModelMutex(
        int cameraId,
        const std::string& model_key,
        TensorRTContext*& trt_context);

    /// @brief Preprocess image with padding
    //static cv::Mat PreprocessWithPadding(const cv::Mat& input, int target_width, int target_height);
   
};

#endif // INFERENCE_WORKER_H