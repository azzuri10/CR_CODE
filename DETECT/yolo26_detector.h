#pragma once

// 1. 系统头文件
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <functional>
#include <cmath>
#include <cstring>
#include <chrono>
#include <thread>
#include <unordered_map>

// 2. 第三方库头文件
#include <opencv2/opencv.hpp>
#include <NvInfer.h>
#include <cuda_runtime.h>

// 3. 项目头文件
#include "detection.h"

class YoloDetector
{
public:
    // 常量定义
    static constexpr int DEFAULT_IMAGE_CHANNELS = 3;      // 默认图像通道数
    static constexpr float DEFAULT_CONF_THRESH = 0.5f;    // 默认置信度阈值
    static constexpr float DEFAULT_NMS_THRESH = 0.5f;     // 默认NMS阈值
    static constexpr int YOLOV8_FORMAT = 0;              // YOLOv8输出格式
    static constexpr int YOLO26_FORMAT = 1;               // YOLO26输出格式

    /**
     * @brief 构造函数
     * @param engine_file TensorRT引擎文件路径
     * @param num_classes 类别数量，如果为-1则自动从模型推断
     */
    explicit YoloDetector(const std::string& engine_file, int num_classes = -1);

    /**
     * @brief 禁止拷贝构造函数
     */
    YoloDetector(const YoloDetector&) = delete;

    /**
     * @brief 禁止赋值运算符
     */
    YoloDetector& operator=(const YoloDetector&) = delete;

    /**
     * @brief 移动构造函数
     */
    YoloDetector(YoloDetector&& other) noexcept;

    /**
     * @brief 移动赋值运算符
     */
    YoloDetector& operator=(YoloDetector&& other) noexcept;

    /**
     * @brief 析构函数
     */
    ~YoloDetector();

    /**
     * @brief 初始化TensorRT引擎
     * @return 初始化成功返回true，失败返回false
     */
    bool init();

    /**
     * @brief 执行推理
     * @param image 输入图像
     * @return 检测结果向量
     */
    std::vector<Detection> infer(const cv::Mat& image);

    // 设置检测参数
    void setConfidenceThreshold(float threshold) { conf_threshold_ = threshold; }
    void setNMSThreshold(float threshold) { nms_threshold_ = threshold; }
    void setClassFilter(const std::vector<int>& classes) { class_filter_ = classes; }

    // 设置填充颜色，支持不同模型的预处理
    void setPaddingColor(const cv::Scalar& color) { padding_color_ = color; }

    // 启用/禁用性能分析
    void enableProfiling(bool enable) { enable_profiling_ = enable; }

    int getInputWidth() const { return input_w_; }
    int getInputHeight() const { return input_h_; }
    int getNumClasses() const { return num_classes_; }
    bool isInitialized() const { return is_initialized_; }

    /**
     * @brief 获取最后一次推理的耗时
     * @return 耗时（毫秒）
     */
    float getLastInferenceTime() const { return last_inference_time_; }

private:
    struct WorkerContext {
        nvinfer1::IExecutionContext* context = nullptr;
        cudaStream_t stream = nullptr;
        std::vector<void*> gpu_buffers;
        ~WorkerContext() = default;
    };

    // 预处理信息结构体
    struct PreprocessInfo {
        int src_width;      // 原图宽度
        int src_height;     // 原图高度
        float scale;        // 缩放比例
        int pad_w;          // 宽度填充
        int pad_h;          // 高度填充
        int resized_w;      // 缩放后宽度
        int resized_h;      // 缩放后高度

        // 计算去填充和缩放转换
        void transformBox(float& x1, float& y1, float& x2, float& y2) const;
    };

    // 内部框信息结构体
    struct BoxInfo {
        float x1;        // 左上角x
        float y1;        // 左上角y
        float x2;        // 右下角x
        float y2;        // 右下角y
        float conf;      // 置信度
        int class_id;    // 类别ID
    };

    // 预处理
    PreprocessInfo preprocess(const cv::Mat& img, float* cpu_input_buffer);

    // 后处理
    std::vector<Detection> postprocess_yolov8(
        const float* cpu_output_buffer,
        const PreprocessInfo& info
    );

    std::vector<Detection> postprocess_yolo26(
        const float* cpu_output_buffer,
        const PreprocessInfo& info
    );

    // NMS处理
    std::vector<BoxInfo> nms(const std::vector<BoxInfo>& boxes, float iou_threshold = 0.5f);
    float calculateIoU(const BoxInfo& box1, const BoxInfo& box2);

    // 优化工具函数
    void normalizeAndCopyToCHW(const cv::Mat& rgb, float* buffer);

    // 清理资源的内部函数
    void cleanup() noexcept;
    WorkerContext* getOrCreateWorkerContext();
    void destroyWorkerContext(WorkerContext& worker) noexcept;

    // 工具函数
    static float sigmoid(float x) {
        return 1.0f / (1.0f + expf(-x));
    }

    // 检查TensorRT错误
    bool checkCudaError(cudaError_t err, const std::string& context);
    bool checkCudaError(cudaError_t err, const char* context) {
        return checkCudaError(err, std::string(context));
    }

private:
    std::string engine_file_;          // 引擎文件路径
    int num_classes_;                   // 类别数
    float conf_threshold_;              // 置信度阈值
    float nms_threshold_;               // NMS阈值
    std::vector<int> class_filter_;     // 类别过滤器
    cv::Scalar padding_color_;          // 填充颜色
    bool enable_profiling_;             // 是否启用性能分析
    bool is_initialized_ = false;       // 是否已初始化

    // TensorRT相关
    nvinfer1::IRuntime* runtime_ = nullptr;
    nvinfer1::ICudaEngine* engine_ = nullptr;
    nvinfer1::IExecutionContext* context_ = nullptr;

    // CUDA相关
    cudaStream_t stream_ = nullptr;
    std::vector<void*> gpu_buffers_;           // GPU缓冲区指针
    std::unique_ptr<float[]> cpu_input_buffer_;  // CPU端输入缓冲区（智能指针管理）
    std::unique_ptr<float[]> cpu_output_buffer_; // CPU端输出缓冲区（智能指针管理）

    // 模型信息
    int input_w_ = 0;
    int input_h_ = 0;
    int input_c_ = 0;
    int batch_size_ = 1;               // 批量大小

    size_t input_size_ = 0;
    size_t output_size_ = 0;
    nvinfer1::Dims input_dims_;
    nvinfer1::Dims output_dims_;
    int input_index_ = -1;
    int output_index_ = -1;

    int output_boxes_ = 0;             // 输出框数量
    int output_format_ = 0;            // 输出格式: 0=YOLOv8, 1=YOLO26

    // 性能统计
    float last_inference_time_ = 0.0f;

    // 线程安全
    mutable std::mutex inference_mutex_;
    mutable std::mutex worker_map_mutex_;
    std::unordered_map<std::thread::id, std::unique_ptr<WorkerContext>> worker_contexts_;
};