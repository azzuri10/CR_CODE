#include "yolo26_detector.h"
#include <fstream>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <thread>
#include <functional>
#ifdef _OPENMP
#include <omp.h>
#endif

// 在包含Windows头文件之前定义NOMINMAX宏
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#endif

// TensorRT日志记录器
class Logger : public nvinfer1::ILogger
{
    void log(Severity severity, const char* msg) noexcept override
    {
        // 只记录警告及以上级别的日志
        if (severity <= Severity::kWARNING)
        {
            std::cout << "[TensorRT] " << msg << std::endl;
        }
    }
} gLogger;

// 预处理信息的方法实现
void YoloDetector::PreprocessInfo::transformBox(float& x1, float& y1, float& x2, float& y2) const
{
    // 去填充
    float pad_left = static_cast<float>(pad_w) / 2.0f;
    float pad_top = static_cast<float>(pad_h) / 2.0f;

    x1 -= pad_left;
    y1 -= pad_top;
    x2 -= pad_left;
    y2 -= pad_top;

    // 缩放回原图
    float inv_scale = 1.0f / scale;
    x1 *= inv_scale;
    y1 *= inv_scale;
    x2 *= inv_scale;
    y2 *= inv_scale;

    // 裁剪到原图边界
    x1 = std::max(0.0f, std::min(x1, static_cast<float>(src_width)));
    y1 = std::max(0.0f, std::min(y1, static_cast<float>(src_height)));
    x2 = std::max(0.0f, std::min(x2, static_cast<float>(src_width)));
    y2 = std::max(0.0f, std::min(y2, static_cast<float>(src_height)));
}

YoloDetector::YoloDetector(const std::string& engine_file, int num_classes)
    : engine_file_(engine_file),
    num_classes_(num_classes),
    conf_threshold_(DEFAULT_CONF_THRESH),
    nms_threshold_(DEFAULT_NMS_THRESH),
    padding_color_(cv::Scalar(114, 114, 114)),  // 默认填充颜色
    enable_profiling_(false),
    is_initialized_(false)
{
    // 所有指针已在声明时初始化为nullptr
}

YoloDetector::YoloDetector(YoloDetector&& other) noexcept
    : engine_file_(std::move(other.engine_file_)),
    num_classes_(other.num_classes_),
    conf_threshold_(other.conf_threshold_),
    nms_threshold_(other.nms_threshold_),
    class_filter_(std::move(other.class_filter_)),
    padding_color_(other.padding_color_),
    enable_profiling_(other.enable_profiling_),
    is_initialized_(other.is_initialized_),
    runtime_(other.runtime_),
    engine_(other.engine_),
    context_(other.context_),
    stream_(other.stream_),
    gpu_buffers_(std::move(other.gpu_buffers_)),
    cpu_input_buffer_(std::move(other.cpu_input_buffer_)),
    cpu_output_buffer_(std::move(other.cpu_output_buffer_)),
    input_w_(other.input_w_),
    input_h_(other.input_h_),
    input_c_(other.input_c_),
    batch_size_(other.batch_size_),
    input_size_(other.input_size_),
    output_size_(other.output_size_),
    input_dims_(other.input_dims_),
    output_dims_(other.output_dims_),
    input_index_(other.input_index_),
    output_index_(other.output_index_),
    output_boxes_(other.output_boxes_),
    output_format_(other.output_format_),
    last_inference_time_(other.last_inference_time_)
{
    // 将原对象置为无效状态
    other.runtime_ = nullptr;
    other.engine_ = nullptr;
    other.context_ = nullptr;
    other.stream_ = nullptr;
    other.is_initialized_ = false;
}

YoloDetector& YoloDetector::operator=(YoloDetector&& other) noexcept
{
    if (this != &other) {
        // 清理当前对象的资源
        cleanup();

        // 移动资源
        engine_file_ = std::move(other.engine_file_);
        num_classes_ = other.num_classes_;
        conf_threshold_ = other.conf_threshold_;
        nms_threshold_ = other.nms_threshold_;
        class_filter_ = std::move(other.class_filter_);
        padding_color_ = other.padding_color_;
        enable_profiling_ = other.enable_profiling_;
        is_initialized_ = other.is_initialized_;
        runtime_ = other.runtime_;
        engine_ = other.engine_;
        context_ = other.context_;
        stream_ = other.stream_;
        gpu_buffers_ = std::move(other.gpu_buffers_);
        cpu_input_buffer_ = std::move(other.cpu_input_buffer_);
        cpu_output_buffer_ = std::move(other.cpu_output_buffer_);
        input_w_ = other.input_w_;
        input_h_ = other.input_h_;
        input_c_ = other.input_c_;
        batch_size_ = other.batch_size_;
        input_size_ = other.input_size_;
        output_size_ = other.output_size_;
        input_dims_ = other.input_dims_;
        output_dims_ = other.output_dims_;
        input_index_ = other.input_index_;
        output_index_ = other.output_index_;
        output_boxes_ = other.output_boxes_;
        output_format_ = other.output_format_;
        last_inference_time_ = other.last_inference_time_;

        // 将原对象置为无效状态
        other.runtime_ = nullptr;
        other.engine_ = nullptr;
        other.context_ = nullptr;
        other.stream_ = nullptr;
        other.is_initialized_ = false;
    }
    return *this;
}

bool YoloDetector::checkCudaError(cudaError_t err, const std::string& context)
{
    if (err != cudaSuccess) {
        std::cerr << "[CUDA Error] " << context << ": "
            << cudaGetErrorString(err) << std::endl;
        return false;
    }
    return true;
}

void YoloDetector::cleanup() noexcept
{
    // 注意：这个函数不获取锁，调用者需要确保线程安全
    for (auto& kv : worker_contexts_) {
        if (kv.second) {
            destroyWorkerContext(*kv.second);
        }
    }
    worker_contexts_.clear();

    // 智能指针会自动释放内存，只需重置指针
    cpu_input_buffer_.reset();
    cpu_output_buffer_.reset();

    if (context_) {
        context_->destroy();
        context_ = nullptr;
    }

    if (engine_) {
        engine_->destroy();
        engine_ = nullptr;
    }

    if (runtime_) {
        runtime_->destroy();
        runtime_ = nullptr;
    }

    is_initialized_ = false;
}

void YoloDetector::destroyWorkerContext(WorkerContext& worker) noexcept {
    if (worker.stream) {
        cudaStreamSynchronize(worker.stream);
        cudaStreamDestroy(worker.stream);
        worker.stream = nullptr;
    }
    for (auto& buffer : worker.gpu_buffers) {
        if (buffer) {
            cudaFree(buffer);
            buffer = nullptr;
        }
    }
    worker.gpu_buffers.clear();
    if (worker.context) {
        worker.context->destroy();
        worker.context = nullptr;
    }
}

YoloDetector::WorkerContext* YoloDetector::getOrCreateWorkerContext() {
    const std::thread::id tid = std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(worker_map_mutex_);
    auto it = worker_contexts_.find(tid);
    if (it != worker_contexts_.end() && it->second) {
        return it->second.get();
    }

    auto worker = std::make_unique<WorkerContext>();
    worker->context = engine_->createExecutionContext();
    if (!worker->context) {
        std::cerr << "[Error] Failed to create per-thread execution context" << std::endl;
        return nullptr;
    }
    if (!checkCudaError(cudaStreamCreate(&worker->stream), "Failed to create per-thread CUDA stream")) {
        worker->context->destroy();
        worker->context = nullptr;
        return nullptr;
    }

    int nbBindings = engine_->getNbBindings();
    worker->gpu_buffers.resize(nbBindings, nullptr);
    for (int i = 0; i < nbBindings; ++i) {
        nvinfer1::Dims dims = engine_->getBindingDimensions(i);
        nvinfer1::DataType dtype = engine_->getBindingDataType(i);
        int volume = 1;
        for (int j = 0; j < dims.nbDims; ++j) {
            volume *= dims.d[j];
        }
        size_t elem_size = 0;
        switch (dtype) {
        case nvinfer1::DataType::kFLOAT: elem_size = sizeof(float); break;
        case nvinfer1::DataType::kHALF: elem_size = sizeof(uint16_t); break;
        case nvinfer1::DataType::kINT8: elem_size = sizeof(int8_t); break;
        case nvinfer1::DataType::kINT32: elem_size = sizeof(int32_t); break;
        default:
            destroyWorkerContext(*worker);
            return nullptr;
        }
        void* d_ptr = nullptr;
        if (!checkCudaError(cudaMalloc(&d_ptr, volume * elem_size), "Failed to allocate per-thread GPU buffer")) {
            destroyWorkerContext(*worker);
            return nullptr;
        }
        worker->gpu_buffers[i] = d_ptr;
    }

    auto [insertedIt, ok] = worker_contexts_.emplace(tid, std::move(worker));
    if (!ok || !insertedIt->second) {
        return nullptr;
    }
    return insertedIt->second.get();
}

YoloDetector::~YoloDetector()
{
    // 析构函数不需要加锁，对象即将被销毁
    cleanup();
}

bool YoloDetector::init()
{
    std::lock_guard<std::mutex> lock(inference_mutex_);

    // 如果已经初始化，先清理
    if (is_initialized_) {
        std::cout << "[Warning] Detector already initialized, cleaning up first..." << std::endl;
        cleanup();
    }

    // 1. 检查文件是否存在
    std::ifstream test_file(engine_file_, std::ios::binary);
    if (!test_file.good()) {
        std::cerr << "[Error] Engine file not found: " << engine_file_ << std::endl;
        return false;
    }
    test_file.close();

    // 2. 加载引擎文件
    auto start_time = std::chrono::high_resolution_clock::now();

    std::ifstream file(engine_file_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[Error] Failed to open engine file: " << engine_file_ << std::endl;
        return false;
    }

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0, file.beg);

    // 使用vector管理引擎数据
    std::vector<char> engine_data(size);
    file.read(engine_data.data(), size);
    file.close();

    if (!file) {
        std::cerr << "[Error] Failed to read engine file: " << engine_file_ << std::endl;
        return false;
    }

    // 3. 创建运行时和引擎
    runtime_ = nvinfer1::createInferRuntime(gLogger);
    if (!runtime_) {
        std::cerr << "[Error] Failed to create TensorRT runtime" << std::endl;
        return false;
    }

    engine_ = runtime_->deserializeCudaEngine(engine_data.data(), size);
    if (!engine_) {
        std::cerr << "[Error] Failed to deserialize CUDA engine" << std::endl;
        cleanup();
        return false;
    }

    context_ = engine_->createExecutionContext();
    if (!context_) {
        std::cerr << "[Error] Failed to create execution context" << std::endl;
        cleanup();
        return false;
    }

    // 5. 获取输入输出信息
    int nbBindings = engine_->getNbBindings();
    gpu_buffers_.resize(nbBindings, nullptr);

    for (int i = 0; i < nbBindings; ++i) {
        nvinfer1::Dims dims = engine_->getBindingDimensions(i);
        nvinfer1::DataType dtype = engine_->getBindingDataType(i);
        const char* name = engine_->getBindingName(i);

        // 计算绑定大小
        int volume = 1;
        for (int j = 0; j < dims.nbDims; ++j) {
            if (dims.d[j] <= 0) {
                std::cerr << "[Error] Invalid dimension at index " << j
                    << " for binding " << name << std::endl;
                cleanup();
                return false;
            }
            volume *= dims.d[j];
        }

        size_t elem_size = 0;
        switch (dtype) {
        case nvinfer1::DataType::kFLOAT:
            elem_size = sizeof(float);
            break;
        case nvinfer1::DataType::kHALF:
            elem_size = sizeof(uint16_t);
            break;
        case nvinfer1::DataType::kINT8:
            elem_size = sizeof(int8_t);
            break;
        case nvinfer1::DataType::kINT32:
            elem_size = sizeof(int32_t);
            break;
        default:
            std::cerr << "[Error] Unsupported data type: "
                << static_cast<int>(dtype) << std::endl;
            cleanup();
            return false;
        }

        size_t binding_size = volume * elem_size;

        if (engine_->bindingIsInput(i)) {
            input_index_ = i;
            input_dims_ = dims;

            // 解析输入维度
            if (dims.nbDims == 4) {
                // NCHW格式 [batch, channel, height, width]
                batch_size_ = dims.d[0];
                input_c_ = dims.d[1];
                input_h_ = dims.d[2];
                input_w_ = dims.d[3];
            }
            else if (dims.nbDims == 3) {
                // CHW格式 [channel, height, width]
                batch_size_ = 1;
                input_c_ = dims.d[0];
                input_h_ = dims.d[1];
                input_w_ = dims.d[2];
            }
            else {
                std::cerr << "[Error] Unexpected input dimension: "
                    << dims.nbDims << std::endl;
                cleanup();
                return false;
            }

            input_size_ = binding_size;

            // 使用智能指针分配CPU输入缓冲区
            cpu_input_buffer_ = std::make_unique<float[]>(binding_size / sizeof(float));

            std::cout << "[Info] Input binding: " << name
                << ", format: " << (dims.nbDims == 4 ? "NCHW" : "CHW")
                << ", size: " << input_w_ << "x" << input_h_
                << "x" << input_c_
                << ", batch: " << batch_size_ << std::endl;
        }
        else {
            output_index_ = i;
            output_dims_ = dims;
            output_size_ = binding_size;

            // 使用智能指针分配CPU输出缓冲区
            cpu_output_buffer_ = std::make_unique<float[]>(binding_size / sizeof(float));

            std::cout << "[Info] Output binding: " << name
                << ", dimensions: [";
            for (int j = 0; j < dims.nbDims; ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << dims.d[j];
            }
            std::cout << "]" << std::endl;

            // 自动检测输出格式
            if (dims.nbDims == 3) {
                if (dims.d[2] == 6) {
                    // YOLO26格式 [batch, boxes, 6]
                    output_format_ = YOLO26_FORMAT;
                    output_boxes_ = dims.d[1];
                    std::cout << "[Info] Detected YOLO26 format: [batch=" << dims.d[0]
                        << ", boxes=" << dims.d[1]
                        << ", elements=" << dims.d[2] << "]" << std::endl;
                }
                else {
                    // YOLOv8格式 [batch, 4+classes, boxes]
                    output_format_ = YOLOV8_FORMAT;
                    output_boxes_ = dims.d[2];

                    // 根据输出维度自动计算类别数
                    int output_channels = dims.d[1];
                    if (output_channels >= 5) {  // 至少4个坐标+1个置信度
                        int inferred_classes = output_channels - 5;
                        if (num_classes_ != -1 && num_classes_ != inferred_classes) {
                            std::cout << "[Warning] Class mismatch! Constructor expects "
                                << num_classes_ << " classes, but model has "
                                << inferred_classes << " classes. Using "
                                << inferred_classes << " classes." << std::endl;
                        }
                        num_classes_ = inferred_classes;
                    }

                    std::cout << "[Info] Detected YOLOv8 format: [batch=" << dims.d[0]
                        << ", channels=" << dims.d[1]
                        << ", boxes=" << dims.d[2]
                        << "], num_classes=" << num_classes_ << std::endl;
                }
            }
            else if (dims.nbDims == 2) {
                // 一维输出
                output_format_ = YOLO26_FORMAT;  // 默认YOLO26格式
                output_boxes_ = dims.d[0] / 6;
                std::cout << "[Info] Flat output format, boxes: " << output_boxes_ << std::endl;
            }
            else {
                std::cerr << "[Error] Unsupported output dimension: " << dims.nbDims << std::endl;
                cleanup();
                return false;
            }
        }
    }

    if (input_index_ == -1 || output_index_ == -1) {
        std::cerr << "[Error] Failed to find input or output binding" << std::endl;
        cleanup();
        return false;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "[Info] YoloDetector initialized successfully in "
        << duration.count() << "ms" << std::endl;
    std::cout << "[Info] Input size: " << input_w_ << "x" << input_h_ << std::endl;
    std::cout << "[Info] Number of classes: " << num_classes_ << std::endl;
    std::cout << "[Info] Batch size: " << batch_size_ << std::endl;

    is_initialized_ = true;
    return true;
}

void YoloDetector::normalizeAndCopyToCHW(const cv::Mat& rgb, float* buffer)
{
    int img_size = input_w_ * input_h_;
    const float inv_255 = 1.0f / 255.0f;  // 预计算倒数，避免重复除法

    // 检查图像是否连续，以优化内存访问
    if (rgb.isContinuous()) {
        const uchar* data = rgb.data;

        // 使用指针运算提高效率
        float* r_channel = buffer;
        float* g_channel = buffer + img_size;
        float* b_channel = buffer + 2 * img_size;

        // 分离通道并拷贝
        for (int i = 0; i < img_size; ++i) {
            r_channel[i] = data[i * 3] * inv_255;      // R通道
            g_channel[i] = data[i * 3 + 1] * inv_255;  // G通道
            b_channel[i] = data[i * 3 + 2] * inv_255;  // B通道
        }
    }
    else {
        // 如果不是连续的，使用逐行拷贝
        for (int h = 0; h < input_h_; ++h) {
            const uchar* row = rgb.ptr<uchar>(h);
            float* r_row = buffer + h * input_w_;
            float* g_row = r_row + img_size;
            float* b_row = g_row + img_size;

            for (int w = 0; w < input_w_; ++w) {
                r_row[w] = row[w * 3] * inv_255;      // R
                g_row[w] = row[w * 3 + 1] * inv_255;  // G
                b_row[w] = row[w * 3 + 2] * inv_255;  // B
            }
        }
    }
}

YoloDetector::PreprocessInfo YoloDetector::preprocess(const cv::Mat& img, float* cpu_input_buffer)
{
    PreprocessInfo info;
    info.src_width = img.cols;
    info.src_height = img.rows;

    // 计算缩放比例（保持宽高比）
    float scale_w = static_cast<float>(input_w_) / info.src_width;
    float scale_h = static_cast<float>(input_h_) / info.src_height;
    info.scale = std::min(scale_w, scale_h);

    info.resized_w = static_cast<int>(info.src_width * info.scale);
    info.resized_h = static_cast<int>(info.src_height * info.scale);
    info.pad_w = input_w_ - info.resized_w;
    info.pad_h = input_h_ - info.resized_h;

    // 使用浮点数计算填充，避免精度损失
    float pad_left_float = static_cast<float>(info.pad_w) / 2.0f;
    float pad_top_float = static_cast<float>(info.pad_h) / 2.0f;

    int pad_left_int = static_cast<int>(pad_left_float);
    int pad_top_int = static_cast<int>(pad_top_float);

    // 1. 缩放图片
    cv::Mat resized;
    cv::resize(img, resized, cv::Size(info.resized_w, info.resized_h),
        0, 0, cv::INTER_LINEAR);

    // 2. 创建画布并填充指定颜色
    cv::Mat padded = cv::Mat::zeros(input_h_, input_w_, CV_8UC3);
    padded.setTo(padding_color_);

    // 3. 将缩放后的图片放到画布中心
    cv::Rect roi(pad_left_int, pad_top_int, info.resized_w, info.resized_h);
    resized.copyTo(padded(roi));

    // 4. BGR 转 RGB
    cv::Mat rgb;
    cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);

    // 5. 归一化并转换为CHW格式
    normalizeAndCopyToCHW(rgb, cpu_input_buffer);

    return info;
}

float YoloDetector::calculateIoU(const BoxInfo& box1, const BoxInfo& box2)
{
    // 计算交集区域
    float inter_x1 = std::max(box1.x1, box2.x1);
    float inter_y1 = std::max(box1.y1, box2.y1);
    float inter_x2 = std::min(box1.x2, box2.x2);
    float inter_y2 = std::min(box1.y2, box2.y2);

    float inter_width = std::max(0.0f, inter_x2 - inter_x1);
    float inter_height = std::max(0.0f, inter_y2 - inter_y1);
    float inter_area = inter_width * inter_height;

    // 计算并集区域
    float area1 = (box1.x2 - box1.x1) * (box1.y2 - box1.y1);
    float area2 = (box2.x2 - box2.x1) * (box2.y2 - box2.y1);
    float union_area = area1 + area2 - inter_area;

    if (union_area <= std::numeric_limits<float>::epsilon()) {
        return 0.0f;
    }

    return inter_area / union_area;
}

std::vector<YoloDetector::BoxInfo> YoloDetector::nms(const std::vector<BoxInfo>& boxes, float iou_threshold)
{
    std::vector<BoxInfo> result;
    if (boxes.empty()) {
        return result;
    }

    // 使用vector<char>而不是vector<bool>，因为vector<bool>是特化版本，可能有性能问题
    std::vector<char> suppressed(boxes.size(), 0);

    // 生成索引并排序
    std::vector<size_t> indices(boxes.size());
    std::iota(indices.begin(), indices.end(), 0);

    // 按置信度降序排序
    std::sort(indices.begin(), indices.end(), [&](size_t i, size_t j) {
        return boxes[i].conf > boxes[j].conf;
        });

    for (size_t i = 0; i < indices.size(); ++i) {
        size_t idx_i = indices[i];

        // 如果这个框已经被抑制，跳过
        if (suppressed[idx_i]) {
            continue;
        }

        // 保留当前框
        result.push_back(boxes[idx_i]);

        for (size_t j = i + 1; j < indices.size(); ++j) {
            size_t idx_j = indices[j];

            // 如果这个框已经被抑制，跳过
            if (suppressed[idx_j]) {
                continue;
            }

            // 只对同一类别的框进行NMS
            if (boxes[idx_i].class_id != boxes[idx_j].class_id) {
                continue;
            }

            // 计算IoU
            float iou = calculateIoU(boxes[idx_i], boxes[idx_j]);
            if (iou > iou_threshold) {
                suppressed[idx_j] = 1;
            }
        }
    }

    return result;
}

std::vector<Detection> YoloDetector::postprocess_yolov8(
    const float* cpu_output_buffer,
    const PreprocessInfo& info)
{
    std::vector<Detection> results;

    int total_boxes = output_boxes_;

    // 为每个线程创建私有向量，避免临界区竞争
#ifdef _OPENMP
    int num_threads = omp_get_max_threads();
    std::vector<std::vector<BoxInfo>> thread_boxes(num_threads);
#else
    std::vector<std::vector<BoxInfo>> thread_boxes(1);
#endif

    // 并行处理所有框
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
    for (int i = 0; i < total_boxes; ++i) {
#ifdef _OPENMP
        int thread_id = omp_get_thread_num();
#else
        int thread_id = 0;
#endif
        auto& local_boxes = thread_boxes[thread_id];

        float max_conf = 0.0f;
        int max_class = -1;

        // 查找最大置信度的类别
        for (int c = 0; c < num_classes_; ++c) {
            int index = i + (4 + c) * total_boxes;
            float conf = cpu_output_buffer[index];

            if (conf > max_conf) {
                max_conf = conf;
                max_class = c;
            }
        }

        // 置信度过滤
        if (max_conf < conf_threshold_) {
            continue;
        }

        // 解析坐标
        int base_idx = i;
        float cx = cpu_output_buffer[base_idx];
        float cy = cpu_output_buffer[base_idx + total_boxes];
        float w = cpu_output_buffer[base_idx + 2 * total_boxes];
        float h = cpu_output_buffer[base_idx + 3 * total_boxes];

        // 转换为角点坐标
        float x1 = cx - w * 0.5f;
        float y1 = cy - h * 0.5f;
        float x2 = cx + w * 0.5f;
        float y2 = cy + h * 0.5f;

        // 坐标变换
        info.transformBox(x1, y1, x2, y2);

        // 过滤无效框
        if (x2 <= x1 || y2 <= y1) {
            continue;
        }

        // 类别过滤
        if (!class_filter_.empty()) {
            if (std::find(class_filter_.begin(), class_filter_.end(), max_class) == class_filter_.end()) {
                continue;
            }
        }

        BoxInfo box;
        box.x1 = x1;
        box.y1 = y1;
        box.x2 = x2;
        box.y2 = y2;
        box.conf = max_conf;
        box.class_id = max_class;

        local_boxes.push_back(box);
    }

    // 合并所有线程的结果
    std::vector<BoxInfo> all_boxes;
    for (auto& boxes : thread_boxes) {
        all_boxes.insert(all_boxes.end(), boxes.begin(), boxes.end());
    }

    // 应用NMS
    auto nms_boxes = nms(all_boxes, nms_threshold_);

    // 转换为Detection格式
    results.reserve(nms_boxes.size());
    for (auto& box : nms_boxes) {
        Detection det;
        det.x1 = box.x1;
        det.y1 = box.y1;
        det.x2 = box.x2;
        det.y2 = box.y2;
        det.conf = box.conf;
        det.class_id = box.class_id;
        results.push_back(det);
    }

    return results;
}

std::vector<Detection> YoloDetector::postprocess_yolo26(
    const float* cpu_output_buffer,
    const PreprocessInfo& info)
{
    std::vector<Detection> detections;
    std::vector<BoxInfo> boxes;
    boxes.reserve(output_boxes_);

    constexpr int elem_per_box = 6;  // x1, y1, x2, y2, conf, class_id

    // 解析YOLO26输出格式
    for (int i = 0; i < output_boxes_; ++i) {
        int base_idx = i * elem_per_box;

        float x1_net = cpu_output_buffer[base_idx];
        float y1_net = cpu_output_buffer[base_idx + 1];
        float x2_net = cpu_output_buffer[base_idx + 2];
        float y2_net = cpu_output_buffer[base_idx + 3];
        float conf = cpu_output_buffer[base_idx + 4];
        int class_id = static_cast<int>(cpu_output_buffer[base_idx + 5]);

        // 置信度过滤
        if (conf < conf_threshold_) {
            continue;
        }

        // 类别过滤
        if (!class_filter_.empty()) {
            if (std::find(class_filter_.begin(), class_filter_.end(), class_id) == class_filter_.end()) {
                continue;
            }
        }

        // 坐标映射
        float x1 = x1_net;
        float y1 = y1_net;
        float x2 = x2_net;
        float y2 = y2_net;

        info.transformBox(x1, y1, x2, y2);

        // 过滤无效框
        if (x1 >= x2 || y1 >= y2) {
            continue;
        }

        BoxInfo box;
        box.x1 = x1;
        box.y1 = y1;
        box.x2 = x2;
        box.y2 = y2;
        box.conf = conf;
        box.class_id = class_id;
        boxes.push_back(box);
    }

    // 应用NMS
    auto nms_boxes = nms(boxes, nms_threshold_);

    // 转换为Detection格式
    detections.reserve(nms_boxes.size());
    for (const auto& box : nms_boxes) {
        Detection det;
        det.x1 = box.x1;
        det.y1 = box.y1;
        det.x2 = box.x2;
        det.y2 = box.y2;
        det.conf = box.conf;
        det.class_id = box.class_id;
        detections.push_back(det);
    }

    return detections;
}

std::vector<Detection> YoloDetector::infer(const cv::Mat& image)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    if (!is_initialized_) {
        std::cerr << "[Error] Detector not initialized. Call init() first." << std::endl;
        return {};
    }

    if (image.empty()) {
        std::cerr << "[Error] Input image is empty" << std::endl;
        return {};
    }

    if (image.channels() != DEFAULT_IMAGE_CHANNELS) {
        std::cerr << "[Error] Input image must have 3 channels (BGR), but got "
            << image.channels() << " channels" << std::endl;
        return {};
    }

    // 使用线程本地缓冲，避免在CPU预处理和后处理阶段争用共享资源
    const size_t input_elems = input_size_ / sizeof(float);
    const size_t output_elems = output_size_ / sizeof(float);
    std::vector<float> local_input(input_elems);
    std::vector<float> local_output(output_elems);

    // 1. 预处理（锁外）
    auto preprocess_start = std::chrono::high_resolution_clock::now();
    PreprocessInfo preprocess_info = preprocess(image, local_input.data());
    auto preprocess_end = std::chrono::high_resolution_clock::now();

    // 2. GPU推理区：每线程独立 execution context / stream / gpu buffer
    auto copy_start = std::chrono::high_resolution_clock::now();
    auto inference_start = std::chrono::high_resolution_clock::now();
    auto copy_back_start = inference_start;
    auto copy_back_end = inference_start;
    {
        WorkerContext* worker = getOrCreateWorkerContext();
        if (!worker || !worker->context || !worker->stream || worker->gpu_buffers.empty()) {
            std::cerr << "[Error] Failed to get worker context for inference" << std::endl;
            return {};
        }
        cudaError_t cuda_status = cudaMemcpyAsync(
            worker->gpu_buffers[input_index_],
            local_input.data(),
            input_size_,
            cudaMemcpyHostToDevice,
            worker->stream
        );

        if (!checkCudaError(cuda_status, "Failed to copy input to GPU")) {
            return {};
        }

        if (!worker->context->enqueueV2(worker->gpu_buffers.data(), worker->stream, nullptr)) {
            std::cerr << "[Error] Failed to enqueue inference" << std::endl;
            return {};
        }

        copy_back_start = std::chrono::high_resolution_clock::now();
        cuda_status = cudaMemcpyAsync(
            local_output.data(),
            worker->gpu_buffers[output_index_],
            output_size_,
            cudaMemcpyDeviceToHost,
            worker->stream
        );

        if (!checkCudaError(cuda_status, "Failed to copy output from GPU")) {
            return {};
        }

        cudaStreamSynchronize(worker->stream);
        copy_back_end = std::chrono::high_resolution_clock::now();
    }

    // 5. 后处理（锁外）
    auto postprocess_start = std::chrono::high_resolution_clock::now();
    std::vector<Detection> detections;
    if (output_format_ == YOLOV8_FORMAT) {
        detections = postprocess_yolov8(local_output.data(), preprocess_info);
    }
    else {
        detections = postprocess_yolo26(local_output.data(), preprocess_info);
    }
    auto postprocess_end = std::chrono::high_resolution_clock::now();

    auto end_time = std::chrono::high_resolution_clock::now();
    last_inference_time_ = std::chrono::duration<float, std::milli>(end_time - start_time).count();

    if (enable_profiling_) {
        auto preprocess_time = std::chrono::duration<float, std::milli>(preprocess_end - preprocess_start).count();
        auto copy_time = std::chrono::duration<float, std::milli>(copy_back_start - copy_start).count();
        auto inference_time = std::chrono::duration<float, std::milli>(copy_back_start - inference_start).count();
        auto copy_back_time = std::chrono::duration<float, std::milli>(copy_back_end - copy_back_start).count();
        auto postprocess_time = std::chrono::duration<float, std::milli>(postprocess_end - postprocess_start).count();

        std::cout << "[Profiling] Preprocess: " << preprocess_time << "ms" << std::endl;
        std::cout << "[Profiling] Copy to GPU: " << copy_time << "ms" << std::endl;
        std::cout << "[Profiling] Inference: " << inference_time << "ms" << std::endl;
        std::cout << "[Profiling] Copy from GPU: " << copy_back_time << "ms" << std::endl;
        std::cout << "[Profiling] Postprocess: " << postprocess_time << "ms" << std::endl;
        std::cout << "[Profiling] Total inference time: " << last_inference_time_ << "ms" << std::endl;
    }

    return detections;
}