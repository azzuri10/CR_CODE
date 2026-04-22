#include "InferenceWorker.h"
#include "ModelManager.h"
#include <opencv2/dnn.hpp>
#include <fstream>
#include <iostream>
#include <atomic>
#include <shared_mutex>

// 使用读写锁保护相机映射
std::shared_mutex InferenceWorker::camera_map_mutex_;
tbb::concurrent_unordered_map<std::string, InferenceWorker::ModelPool> InferenceWorker::model_pools_;
tbb::concurrent_unordered_map<int, InferenceWorker::CameraModelMutex> InferenceWorker::camera_mutexes_;  // 关键修复

// InferenceWorker.cpp
// 添加新的分割推理函数实现
cv::Rect SafeBBoxConversion(float x, float y, float w, float h, float scale, const cv::Size& image_size);

std::vector<FinsObjectSeg> InferenceWorker::RunSegmentation(
    int cameraId,
    const std::string& model_path,
    const std::vector<std::string>& classes,
    const cv::Mat& input,
    float conf_threshold,
    float nms_threshold) {

    return RunSegmentationImpl(cameraId, model_path, classes, input,
        conf_threshold, nms_threshold);
}

std::future<std::vector<FinsObjectSeg>> InferenceWorker::RunSegmentationAsync(
    int cameraId,
    const std::string& model_path,
    const std::vector<std::string>& classes,
    const cv::Mat& input,
    float conf_threshold,
    float nms_threshold,
    int priority) {

    return InferenceManager::Instance().Submit(
        priority,
        [=] {
            return RunSegmentation(cameraId, model_path, classes, input,
                conf_threshold, nms_threshold);
        }
    );
}

std::vector<FinsObjectSeg> InferenceWorker::RunSegmentationImpl(
    int cameraId,
    const std::string& model_path,
    const std::vector<std::string>& classes,
    const cv::Mat& input,
    float conf_threshold,
    float nms_threshold) {

    auto& mgr = ModelManager::Instance(cameraId);

    if (!mgr.IsModelLoaded(model_path)) {
        mgr.LoadModel(model_path, 0);
    }

    TensorRTContext* trt_context = nullptr;
    auto model_mutex = GetModelMutex(cameraId, model_path, trt_context);
    auto net_ptr = mgr.GetCVDNNModel(model_path);

    // 获取输入尺寸
    cv::dnn::MatShape inputShape;
    std::vector<cv::dnn::MatShape> inLayerShapes, outLayerShapes;
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        net_ptr->getLayerShapes(inputShape, 0, inLayerShapes, outLayerShapes);
    }
    const int inputWidth = inLayerShapes[0][3];
    const int inputHeight = inLayerShapes[0][2];

    // 预处理
    auto [image, max_size] = PreprocessWithPadding(input, inputWidth, inputHeight);
    cv::Mat blob;
    cv::dnn::blobFromImage(image, blob, 1. / 255., cv::Size(inputWidth, inputHeight),
        cv::Scalar(), true, false);

    std::vector<cv::Mat> outputs;
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        net_ptr->setInput(blob);
        net_ptr->forward(outputs, net_ptr->getUnconnectedOutLayersNames());
    }

    // YOLOv8 Mask 模型通常有2个输出：检测输出和掩码原型
    if (outputs.size() < 2) {
        throw std::runtime_error("Segmentation model should have at least 2 outputs");
    }

    // 第一个输出是检测结果 [1, 116, 8400] 格式
    // 116 = 4(box) + 80(class) + 32(mask coefficients)
    cv::Mat& detection_output = outputs[0];
    cv::Mat& mask_protos = outputs[1];

    // 重塑检测输出
    const int dimensions = detection_output.size[1];  // 应该是116
    const int num_boxes = detection_output.size[2];

    detection_output = detection_output.reshape(1, dimensions).t();
    float* detection_data = reinterpret_cast<float*>(detection_output.data);

    // 掩码原型尺寸 [1, 32, 80, 80] 或其他尺寸
    const int mask_proto_height = mask_protos.size[2];
    const int mask_proto_width = mask_protos.size[3];
    const int mask_proto_channels = mask_protos.size[1];

    const cv::Size image_size = input.size();
    const float scale = max_size * 1.0f / inputWidth;

    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> class_ids;
    std::vector<std::vector<float>> mask_coefficients;

    // 解析检测结果
    for (int i = 0; i < num_boxes; ++i) {
        // 获取框坐标
        float x = detection_data[0];
        float y = detection_data[1];
        float w = detection_data[2];
        float h = detection_data[3];

        // 获取类别分数
        float* class_scores = detection_data + 4;
        cv::Mat scores(1, classes.size(), CV_32FC1, class_scores);

        cv::Point class_id;
        double max_score;
        cv::minMaxLoc(scores, nullptr, &max_score, nullptr, &class_id);

        if (max_score > conf_threshold) {
            // 安全转换边界框
            cv::Rect bbox = SafeBBoxConversion(x, y, w, h, scale, image_size);

            if (bbox.width > 0 && bbox.height > 0) {
                boxes.push_back(bbox);
                confidences.push_back(static_cast<float>(max_score));
                class_ids.push_back(class_id.x);

                // 提取掩码系数 (最后32个值)
                std::vector<float> coefficients;
                float* mask_coeffs = detection_data + 4 + classes.size();
                coefficients.assign(mask_coeffs, mask_coeffs + 32);
                mask_coefficients.push_back(coefficients);
            }
        }

        detection_data += dimensions;
    }

    // NMS处理
    std::vector<int> indices;
    if (!boxes.empty()) {
        cv::dnn::NMSBoxes(boxes, confidences, conf_threshold, nms_threshold, indices);
    }

    // 生成分割结果
    std::vector<FinsObjectSeg> detections;
    detections.reserve(indices.size());

    for (int idx : indices) {
        FinsObjectSeg seg_obj;
        seg_obj.box = boxes[idx];
        seg_obj.confidence = confidences[idx];
        seg_obj.class_name = classes[class_ids[idx]];

        // 生成掩码
        if (!mask_coefficients.empty() && idx < mask_coefficients.size()) {
            // 重塑掩码原型为 [32, mask_proto_height * mask_proto_width]
            cv::Mat mask_protos_reshaped = mask_protos.reshape(1, mask_proto_channels);

            // 计算掩码：coefficients * protos
            cv::Mat mask_mat(1, mask_proto_height * mask_proto_width, CV_32FC1);
            for (int c = 0; c < mask_proto_channels; ++c) {
                float coeff = mask_coefficients[idx][c];
                if (c == 0) {
                    mask_protos_reshaped.row(c).convertTo(mask_mat, CV_32FC1, coeff);
                }
                else {
                    mask_mat += coeff * mask_protos_reshaped.row(c);
                }
            }

            // 重塑掩码到原型尺寸
            cv::Mat mask_proto = mask_mat.reshape(1, mask_proto_height);

            // 使用sigmoid激活
            cv::Mat mask_sigmoid;
            cv::exp(-mask_proto, mask_sigmoid);
            mask_sigmoid = 1.0 / (1.0 + mask_sigmoid);

            // 调整掩码到原图尺寸
            cv::Mat mask_resized;
            cv::resize(mask_sigmoid, mask_resized, image_size);

            // 二值化
            cv::Mat binary_mask;
            cv::threshold(mask_resized, binary_mask, 0.5, 1.0, cv::THRESH_BINARY);
            binary_mask.convertTo(seg_obj.mask, CV_8UC1);

            // 保存原型掩码（可选）
            mask_sigmoid.convertTo(seg_obj.proto_mask, CV_32FC1);
        }

        detections.push_back(seg_obj);
    }

    return detections;
}


// 修改预处理函数返回图像和max_size
std::pair<cv::Mat, int> InferenceWorker::PreprocessWithPadding(const cv::Mat& input, int target_width, int target_height) {
    int w = input.cols;
    int h = input.rows;
    int max_size = std::max(h, w);
    cv::Mat image = cv::Mat::zeros(cv::Size(max_size, max_size), input.type());
    cv::Rect roi(0, 0, w, h);
    input.copyTo(image(roi));
    cv::resize(image, image, cv::Size(target_width, target_height));
    return { image, max_size };
}

std::shared_ptr<std::mutex> InferenceWorker::GetModelMutex(
    int cameraId,
    const std::string& model_key,
    TensorRTContext*& trt_context)
{
    // 1. 获取相机级锁（细粒度）
    CameraModelMutex* camera_mutex_ptr = nullptr;

    {
        // 读锁访问（高效）
        std::shared_lock<std::shared_mutex> read_lock(camera_map_mutex_);
        auto it = camera_mutexes_.find(cameraId);
        if (it != camera_mutexes_.end()) {
            camera_mutex_ptr = &it->second;
        }
    }

    // 如果相机不存在，创建新条目（写锁）
    if (!camera_mutex_ptr) {
        std::unique_lock<std::shared_mutex> write_lock(camera_map_mutex_);
        camera_mutex_ptr = &camera_mutexes_[cameraId];
    }

    CameraModelMutex& camera_mutex = *camera_mutex_ptr;

    // 2. 获取该相机的内部锁
    std::lock_guard<std::mutex> camera_lock(camera_mutex.camera_mutex);

    // 3. 双重检查TRT上下文初始化
    auto trt_it = camera_mutex.trt_contexts.find(model_key);
    if (trt_it == camera_mutex.trt_contexts.end()) {
        auto result = camera_mutex.trt_contexts.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(model_key),
            std::forward_as_tuple()
        );
        trt_context = &result.first->second;
        trt_context->detector = std::make_shared<YoloDetector>(model_key, -1); // 使用 YoloDetector
    }
    else {
        // 修复：使用const_cast移除const限定符
        trt_context = const_cast<TensorRTContext*>(&trt_it->second);
    }

    // 4. 获取/创建模型级互斥锁
    auto& mutex_ptr = camera_mutex.model_mutexes[model_key];
    if (!mutex_ptr) {
        mutex_ptr = std::make_shared<std::mutex>();
    }

    return mutex_ptr;
}

// 加载类别名称文件
std::vector<std::string> InferenceWorker::LoadClassNames(const std::string& filename) {
    std::vector<std::string> classNames;
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open class names file: " + filename);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            classNames.push_back(line);
        }
    }
    return classNames;
}


cv::Rect SafeBBoxConversion(float x, float y, float w, float h, float scale,
    const cv::Size& image_size) {
    // 计算原始坐标
    int left = static_cast<int>((x - 0.5f * w) * scale);
    int top = static_cast<int>((y - 0.5f * h) * scale);
    int width = static_cast<int>(w * scale);
    int height = static_cast<int>(h * scale);

    // 确保宽高为正
    width = std::max(1, width);
    height = std::max(1, height);

    // 边界约束[3](@ref)
    left = std::max(0, std::min(left, image_size.width - 1));
    top = std::max(0, std::min(top, image_size.height - 1));

    // 调整宽度高度确保不越界[3](@ref)
    width = std::min(width, image_size.width - left);
    height = std::min(height, image_size.height - top);

    return cv::Rect(left, top, width, height);
}

// 运行标准检测推理
std::vector<FinsObject> InferenceWorker::Run(
    int cameraId,
    const std::string& model_path,
    const std::vector<std::string>& classes,
    const cv::Mat& input,
    float conf_threshold,
    float nms_threshold) {

    auto& mgr = ModelManager::Instance(cameraId);

    if (!mgr.IsModelLoaded(model_path)) {
        mgr.LoadModel(model_path, 0);
    }

    TensorRTContext* trt_context = nullptr;
    auto model_mutex = GetModelMutex(cameraId, model_path, trt_context);

    // 检查是否是TensorRT引擎文件，使用不同的处理路径
    if (model_path.find(".engine") != std::string::npos) {
        std::vector<Detection> detections;
        {
            std::unique_lock<std::mutex> lock(*model_mutex);
            // TensorRT引擎文件，使用YoloDetector
            auto detector_ptr = mgr.GetYoloDetector(model_path);
            // 执行推理
            detections = detector_ptr->infer(input);
        }
        
        // 转换为FinsObject格式
        std::vector<FinsObject> results;
        for (const auto& detection : detections) {
            if (detection.conf > conf_threshold) {
                // 确保类别ID在范围内
                int class_id = detection.class_id;
                if (class_id < 0 || class_id >= classes.size()) {
                    continue;
                }
                
                // 创建边界框
                cv::Rect bbox(
                    static_cast<int>(detection.x1),
                    static_cast<int>(detection.y1),
                    static_cast<int>(detection.x2 - detection.x1),
                    static_cast<int>(detection.y2 - detection.y1)
                );
                
                // 确保边界框有效
                if (bbox.width > 0 && bbox.height > 0 && 
                    bbox.x >= 0 && bbox.y >= 0 &&
                    bbox.x + bbox.width <= input.cols &&
                    bbox.y + bbox.height <= input.rows) {
                    FinsObject obj;
                    obj.box = bbox;
                    obj.confidence = detection.conf;
                    obj.className = classes[class_id];
                    results.push_back(obj);
                }
            }
        }
        
        // 应用NMS
        if (!results.empty()) {
            std::vector<cv::Rect> boxes;
            std::vector<float> confidences;
            boxes.reserve(results.size());
            confidences.reserve(results.size());
            
            for (const auto& obj : results) {
                boxes.push_back(obj.box);
                confidences.push_back(obj.confidence);
            }
            
            std::vector<int> indices;
            cv::dnn::NMSBoxes(boxes, confidences, conf_threshold, nms_threshold, indices);
            
            std::vector<FinsObject> filtered_results;
            filtered_results.reserve(indices.size());
            for (int idx : indices) {
                filtered_results.push_back(results[idx]);
            }
            return filtered_results;
        }
        return results;
    }

    // ONNX文件，使用OpenCV DNN
    auto net_ptr = mgr.GetCVDNNModel(model_path);

    cv::dnn::MatShape inputShape;
    std::vector<cv::dnn::MatShape> inLayerShapes, outLayerShapes;
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        net_ptr->getLayerShapes(inputShape, 0, inLayerShapes, outLayerShapes);
    }
    const int inputWidth = inLayerShapes[0][3];
    const int inputHeight = inLayerShapes[0][2];

    // 预处理并获取max_size
    auto [image, max_size] = PreprocessWithPadding(input, inputWidth, inputHeight);
    cv::Mat blob;
    cv::dnn::blobFromImage(image, blob, 1. / 255., cv::Size(inputWidth, inputHeight), cv::Scalar(), true, false);
    
    std::vector<cv::Mat> outputs;
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        net_ptr->setInput(blob);
        net_ptr->forward(outputs, net_ptr->getUnconnectedOutLayersNames());
    }

    // 解析输出
    cv::Mat out = outputs[0];
    int dimensions = 0;
    int rows = 0;
    bool is_yolo26 = false;  // [x1,y1,x2,y2,conf,cls]
    bool has_obj_conf = false; // [x,y,w,h,obj,cls...]

    if (out.dims == 3) {
        // 常见格式:
        // [1, C, N]  -> 传统YOLO
        // [1, N, 6]  -> YOLO26
        if (out.size[1] == 6) {
            dimensions = out.size[1];
            rows = out.size[2];
            out = out.reshape(1, dimensions).t(); // [N,6]
            is_yolo26 = true;
        }
        else if (out.size[2] == 6) {
            dimensions = out.size[2];
            rows = out.size[1];
            out = out.reshape(1, rows); // [N,6]
            is_yolo26 = true;
        }
        else {
            dimensions = out.size[1];
            rows = out.size[2];
            out = out.reshape(1, dimensions).t(); // [N,C]
            has_obj_conf = (dimensions == 5 + static_cast<int>(classes.size()));
        }
    }
    else if (out.dims == 2) {
        // [N,6] or [N,C]
        rows = out.size[0];
        dimensions = out.size[1];
        is_yolo26 = (dimensions == 6);
        has_obj_conf = (!is_yolo26 && dimensions == 5 + static_cast<int>(classes.size()));
    }
    else {
        // 兜底
        dimensions = out.total() >= 6 ? 6 : static_cast<int>(out.total());
        rows = static_cast<int>(out.total()) / std::max(1, dimensions);
        out = out.reshape(1, rows);
        is_yolo26 = (dimensions == 6);
    }

    float* data = reinterpret_cast<float*>(out.data);

    const cv::Size image_size = input.size();
    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> class_ids;

    // 使用从预处理获取的max_size
    const float scale = max_size * 1.0f / inputWidth;

    if (is_yolo26) {
        // YOLO26 输出格式: [x1, y1, x2, y2, conf, class_id]
        for (int i = 0; i < rows; ++i) {
            const float x1 = data[0];
            const float y1 = data[1];
            const float x2 = data[2];
            const float y2 = data[3];
            const float confidence = data[4];
            const int class_id = static_cast<int>(data[5]);

            if (confidence >= conf_threshold &&
                class_id >= 0 && class_id < static_cast<int>(classes.size())) {

                int left = static_cast<int>(x1 * scale);
                int top = static_cast<int>(y1 * scale);
                int right = static_cast<int>(x2 * scale);
                int bottom = static_cast<int>(y2 * scale);

                left = std::max(0, std::min(left, image_size.width - 1));
                top = std::max(0, std::min(top, image_size.height - 1));
                right = std::max(0, std::min(right, image_size.width));
                bottom = std::max(0, std::min(bottom, image_size.height));

                cv::Rect bbox(left, top, std::max(0, right - left), std::max(0, bottom - top));
                if (bbox.width > 0 && bbox.height > 0) {
                    boxes.push_back(bbox);
                    confidences.push_back(confidence);
                    class_ids.push_back(class_id);
                }
            }
            data += dimensions;
        }
    }
    else if (has_obj_conf) {
        // YOLOv11 输出格式: [x, y, w, h, obj_conf, cls1, cls2, ...]
        for (int i = 0; i < rows; ++i) {
            float obj_conf = data[4];
            if (obj_conf < conf_threshold) {
                data += dimensions;
                continue;
            }

            float* classes_scores = data + 5;
            cv::Mat scores(1, classes.size(), CV_32FC1, classes_scores);

            cv::Point class_id;
            double max_score;
            cv::minMaxLoc(scores, nullptr, &max_score, nullptr, &class_id);

            float confidence = obj_conf * static_cast<float>(max_score);
            if (confidence > conf_threshold) {
                float x = data[0];
                float y = data[1];
                float w = data[2];
                float h = data[3];

                // 使用安全的边界框计算
                cv::Rect bbox = SafeBBoxConversion(x, y, w, h, scale, image_size);

                // 只添加有效的边界框
                if (bbox.width > 0 && bbox.height > 0) {
                    boxes.push_back(bbox);
                    confidences.push_back(confidence);
                    class_ids.push_back(class_id.x);
                }
            }
            data += dimensions;
        }
    }
    else {
        // 传统YOLO输出格式处理
        for (int i = 0; i < rows; ++i) {
            float* classes_scores = data + 4;
            cv::Mat scores(1, classes.size(), CV_32FC1, classes_scores);

            cv::Point class_id;
            double max_score;
            cv::minMaxLoc(scores, nullptr, &max_score, nullptr, &class_id);

            if (max_score > conf_threshold) {
                float x = data[0];
                float y = data[1];
                float w = data[2];
                float h = data[3];

                // 使用安全的边界框计算
                cv::Rect bbox = SafeBBoxConversion(x, y, w, h, scale, image_size);

                if (bbox.width > 0 && bbox.height > 0) {
                    boxes.push_back(bbox);
                    confidences.push_back(static_cast<float>(max_score));
                    class_ids.push_back(class_id.x);
                }
            }
            data += dimensions;
        }
    }

    // 在NMS前添加边界框有效性检查[1](@ref)
    std::vector<cv::Rect> valid_boxes;
    std::vector<float> valid_confidences;
    std::vector<int> valid_class_ids;

    for (size_t i = 0; i < boxes.size(); ++i) {
        cv::Rect box = boxes[i];

        // 计算边界框的原始面积
        float original_area = box.width * box.height;

        // 计算边界框在图像范围内的部分
        cv::Rect image_rect(0, 0, image_size.width, image_size.height);
        cv::Rect valid_part = box & image_rect;

        // 计算有效部分的面积
        float valid_area = valid_part.width * valid_part.height;

        // 如果有效面积小于原始面积的90%（即超出边界部分大于10%），则过滤掉
        if (valid_area < original_area * 0.7f) {
            continue; // 跳过这个边界框
        }

        // 否则，将边界框调整到图像范围内
        cv::Rect adjusted_box = box;

        // 调整x坐标
        if (adjusted_box.x < 0) {
            adjusted_box.width += adjusted_box.x; // 减少宽度
            adjusted_box.x = 0;
        }

        // 调整y坐标
        if (adjusted_box.y < 0) {
            adjusted_box.height += adjusted_box.y; // 减少高度
            adjusted_box.y = 0;
        }

        // 调整宽度（防止超出右边界）
        if (adjusted_box.x + adjusted_box.width > image_size.width) {
            adjusted_box.width = image_size.width - adjusted_box.x;
        }

        // 调整高度（防止超出下边界）
        if (adjusted_box.y + adjusted_box.height > image_size.height) {
            adjusted_box.height = image_size.height - adjusted_box.y;
        }

        // 确保调整后的边界框仍然有效（宽度和高度大于0）
        if (adjusted_box.width > 0 && adjusted_box.height > 0) {
            valid_boxes.push_back(adjusted_box);
            valid_confidences.push_back(confidences[i]);
            valid_class_ids.push_back(class_ids[i]);
        }
    }

    // NMS  
    std::vector<int> indices;
    if (!valid_boxes.empty()) {
        cv::dnn::NMSBoxes(valid_boxes, valid_confidences, conf_threshold, nms_threshold, indices);
    }

    // 生成最终结果
    std::vector<FinsObject> detections;
    detections.reserve(indices.size());
    AnalyseMat ANA;
    for (int idx : indices) {
        if (idx < 0 || idx >= static_cast<int>(valid_boxes.size())) {
            continue;
        }
        if (!ANA.JudgeRectIn(cv::Rect(0, 0, input.cols, input.rows), valid_boxes[idx])) {
            continue;
        };
        FinsObject obj;
        obj.box = valid_boxes[idx];
        obj.confidence = valid_confidences[idx];
        obj.className = classes[valid_class_ids[idx]];
        detections.push_back(obj);
    }

    return detections;
}


std::vector<FinsObjectRotate> InferenceWorker::RunObb(
    int cameraId,
    const std::string& model_path,
    const std::vector<std::string>& classes,
    const cv::Mat& input,
    float conf_threshold,
    float nms_threshold) {

    auto& mgr = ModelManager::Instance(cameraId);

    if (!mgr.IsModelLoaded(model_path)) {
        mgr.LoadModel(model_path, 0);
    }

    TensorRTContext* trt_context = nullptr;
    auto model_mutex = GetModelMutex(cameraId, model_path, trt_context);

    // 获取智能指针
    auto net_ptr = mgr.GetCVDNNModel(model_path);

    cv::dnn::MatShape inputShape;
    std::vector<cv::dnn::MatShape> inLayerShapes, outLayerShapes;
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        net_ptr->getLayerShapes(inputShape, 0, inLayerShapes, outLayerShapes);
    }
    const int inputWidth = inLayerShapes[0][3];
    const int inputHeight = inLayerShapes[0][2];

    // 预处理并获取max_size
    auto [image, max_size] = PreprocessWithPadding(input, inputWidth, inputHeight);

    cv::Mat blob;
    cv::dnn::blobFromImage(image, blob, 1. / 255., cv::Size(inputWidth, inputHeight), cv::Scalar(), true, false);

    std::vector<cv::Mat> outputs;
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        net_ptr->setInput(blob);
        net_ptr->forward(outputs, net_ptr->getUnconnectedOutLayersNames());
    }

    // 解析输出
    const int dimensions = outputs[0].size[1];
    const int rows = outputs[0].size[2];
    outputs[0] = outputs[0].reshape(1, dimensions).t();
    float* data = reinterpret_cast<float*>(outputs[0].data);

    std::vector<cv::RotatedRect> boxes;
    std::vector<float> confidences;
    std::vector<int> class_ids;

    // 使用从预处理获取的max_size
    const float scale = max_size * 1.0f / inputWidth;

    // 假设输出格式为 [x_center, y_center, width, height, angle, class_scores...]
    for (int i = 0; i < rows; ++i) {
        float x = data[0];
        float y = data[1];
        float w = data[2];
        float h = data[3];
        float angle = data[4];

        float* class_scores = data + 5;
        int num_classes = dimensions - 5;

        int class_id = -1;
        float max_class_score = -1.0f;
        for (int c = 0; c < num_classes; ++c) {
            if (class_scores[c] > max_class_score) {
                max_class_score = class_scores[c];
                class_id = c;
            }
        }

        float confidence = max_class_score;

        if (confidence > conf_threshold) {
            cv::Point2f center(x * scale, y * scale);
            cv::Size2f size(w * scale, h * scale);
            float angle_deg = angle * 180.0f / CV_PI;

            if (angle_deg >= 180) angle_deg -= 180;
            if (angle_deg < 0) angle_deg += 180;

            boxes.emplace_back(center, size, angle_deg);
            confidences.push_back(confidence);
            class_ids.push_back(class_id);
        }

        data += dimensions;
    }

    // NMS
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, conf_threshold, nms_threshold, indices);

    // 构建结果
    std::vector<FinsObjectRotate> detections;
    detections.reserve(indices.size());
    for (int idx : indices) {
        FinsObjectRotate obj;
        obj.box = boxes[idx];
        obj.confidence = confidences[idx];
        obj.className = classes[class_ids[idx]];
        detections.push_back(obj);
    }
    return detections;
}

FinsClassification InferenceWorker::RunClassification(
    int cameraId,
    const std::string& model_path,
    const std::vector<std::string>& classes,
    const cv::Mat& input,
    float conf_threshold) {

    auto& mgr = ModelManager::Instance(cameraId);

    if (!mgr.IsModelLoaded(model_path)) {
        mgr.LoadModel(model_path, 0);
    }

    TensorRTContext* trt_context = nullptr;
    auto model_mutex = GetModelMutex(cameraId, model_path, trt_context);

    if (model_path.find(".engine") != std::string::npos) {
        // engine优先走检测器；如果输出异常，回退到Run的统一检测流程，提升兼容性
        float best_confidence = -1.0f;
        int best_class_id = -1;

        {
            std::unique_lock<std::mutex> lock(*model_mutex);
            auto detector_ptr = mgr.GetYoloDetector(model_path);
            if (detector_ptr) {
                std::vector<Detection> detections = detector_ptr->infer(input);
                for (const auto& detection : detections) {
                    if (detection.class_id < 0 ||
                        detection.class_id >= static_cast<int>(classes.size())) {
                        continue;
                    }
                    if (detection.conf > best_confidence) {
                        best_confidence = detection.conf;
                        best_class_id = detection.class_id;
                    }
                }
            }
        }

        if (best_class_id < 0 || best_confidence < conf_threshold) {
            auto fallback = Run(cameraId, model_path, classes, input, conf_threshold, 0.45f);
            for (const auto& obj : fallback) {
                auto it = std::find(classes.begin(), classes.end(), obj.className);
                if (it == classes.end()) {
                    continue;
                }
                int class_id = static_cast<int>(std::distance(classes.begin(), it));
                if (obj.confidence > best_confidence) {
                    best_confidence = obj.confidence;
                    best_class_id = class_id;
                }
            }
        }

        if (best_class_id < 0 || best_class_id >= static_cast<int>(classes.size()) ||
            best_confidence < conf_threshold) {
            return { "", 0.0f };
        }

        return { classes[best_class_id], best_confidence };
    }

    // 获取智能指针
    auto net_ptr = mgr.GetCVDNNModel(model_path);

    cv::dnn::MatShape inputShape;
    std::vector<cv::dnn::MatShape> inLayerShapes, outLayerShapes;
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        net_ptr->getLayerShapes(inputShape, 0, inLayerShapes, outLayerShapes);
    }
    const int inputWidth = inLayerShapes[0][3];
    const int inputHeight = inLayerShapes[0][2];

    cv::Mat resized;
    cv::resize(input, resized, cv::Size(inputWidth, inputHeight));

    cv::Mat blob;
    cv::dnn::blobFromImage(resized, blob, 1.0 / 255.0, cv::Size(), cv::Scalar(0, 0, 0), true, false);

    cv::Mat prob;
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        net_ptr->setInput(blob);
        prob = net_ptr->forward();
    }

    cv::Point class_id_point;
    double confidence;
    cv::minMaxLoc(prob.reshape(1, 1), nullptr, &confidence, nullptr, &class_id_point);
    int class_id = class_id_point.x;

    if (confidence < conf_threshold) {
        return { "", 0.0f };
    }

    if (class_id < 0 || class_id >= classes.size()) {
        throw std::out_of_range("Class ID out of range: " + std::to_string(class_id));
    }

    return { classes[class_id], static_cast<float>(confidence) };
}

std::vector<FinsClassification> InferenceWorker::RunClassificationBatch(
    int cameraId,
    const std::string& model_path,
    const std::vector<std::string>& classes,
    const std::vector<cv::Mat>& inputs,
    float conf_threshold) {
    std::vector<FinsClassification> results(inputs.size(), { "", 0.0f });
    if (inputs.empty()) {
        return results;
    }

    if (model_path.find(".engine") != std::string::npos) {
        for (size_t i = 0; i < inputs.size(); ++i) {
            if (inputs[i].empty()) {
                continue;
            }
            results[i] = RunClassification(cameraId, model_path, classes, inputs[i], conf_threshold);
        }
        return results;
    }

    auto& mgr = ModelManager::Instance(cameraId);
    if (!mgr.IsModelLoaded(model_path)) {
        mgr.LoadModel(model_path, 0);
    }

    TensorRTContext* trt_context = nullptr;
    auto model_mutex = GetModelMutex(cameraId, model_path, trt_context);
    auto net_ptr = mgr.GetCVDNNModel(model_path);

    cv::dnn::MatShape inputShape;
    std::vector<cv::dnn::MatShape> inLayerShapes, outLayerShapes;
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        net_ptr->getLayerShapes(inputShape, 0, inLayerShapes, outLayerShapes);
    }

    const int inputWidth = inLayerShapes[0][3];
    const int inputHeight = inLayerShapes[0][2];
    const int channels = inLayerShapes[0][1];

    std::vector<cv::Mat> validBlobs;
    std::vector<size_t> validIndices;
    validBlobs.reserve(inputs.size());
    validIndices.reserve(inputs.size());

    for (size_t i = 0; i < inputs.size(); ++i) {
        if (inputs[i].empty()) {
            continue;
        }
        cv::Mat resized;
        cv::resize(inputs[i], resized, cv::Size(inputWidth, inputHeight));
        if (channels == 1 && resized.channels() == 3) {
            cv::cvtColor(resized, resized, cv::COLOR_BGR2GRAY);
        }
        else if (channels == 3 && resized.channels() == 1) {
            cv::cvtColor(resized, resized, cv::COLOR_GRAY2BGR);
        }

        cv::Mat blob;
        cv::dnn::blobFromImage(resized, blob, 1.0 / 255.0, cv::Size(), cv::Scalar(0, 0, 0), true, false);
        validBlobs.push_back(blob);
        validIndices.push_back(i);
    }

    if (validBlobs.empty()) {
        return results;
    }

    int dims[4] = {
        static_cast<int>(validBlobs.size()),
        validBlobs[0].size[1],
        validBlobs[0].size[2],
        validBlobs[0].size[3]
    };
    cv::Mat batchBlob(4, dims, CV_32F);
    const size_t sampleElems = static_cast<size_t>(dims[1]) * dims[2] * dims[3];
    float* dst = reinterpret_cast<float*>(batchBlob.data);
    for (size_t i = 0; i < validBlobs.size(); ++i) {
        const float* src = reinterpret_cast<const float*>(validBlobs[i].data);
        std::memcpy(dst + i * sampleElems, src, sampleElems * sizeof(float));
    }

    cv::Mat prob;
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        net_ptr->setInput(batchBlob);
        prob = net_ptr->forward();
    }

    const int batch = static_cast<int>(validBlobs.size());
    cv::Mat prob2d = prob.reshape(1, batch);
    for (int i = 0; i < batch; ++i) {
        cv::Point class_id_point;
        double confidence = 0.0;
        cv::minMaxLoc(prob2d.row(i), nullptr, &confidence, nullptr, &class_id_point);
        const int class_id = class_id_point.x;
        if (confidence < conf_threshold || class_id < 0 || class_id >= static_cast<int>(classes.size())) {
            continue;
        }
        results[validIndices[i]] = { classes[class_id], static_cast<float>(confidence) };
    }

    return results;
}


std::vector<FinsObject> InferenceWorker::RunTensorrt(
    int cameraId,
    const std::string& model_path,
    const std::vector<std::string>& classes,
    const cv::Mat& input,
    float conf_threshold,
    float nms_threshold)
{
    auto& mgr = ModelManager::Instance(cameraId);

    if (!mgr.IsModelLoaded(model_path)) {
        mgr.LoadModel(model_path, 0);
    }

    TensorRTContext* trt_ctx = nullptr;
    auto model_mutex = GetModelMutex(cameraId, model_path, trt_ctx);

    // 双重检查锁定优化TRT初始化
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        if (trt_ctx->inputWidth == 0) {
            // 获取TRT初始化锁
            std::lock_guard<std::mutex> init_lock(trt_ctx->init_mutex);

            // 再次检查，防止其他线程已经初始化
            if (trt_ctx->inputWidth == 0) {
                // 使用YoloDetector进行TensorRT推理
                if (!trt_ctx->detector) {
                    trt_ctx->detector = std::make_shared<YoloDetector>(model_path, -1);
                    if (!trt_ctx->detector->init()) {
                        throw std::runtime_error("Failed to initialize YoloDetector for TensorRT inference");
                    }
                }

                // 获取输入尺寸
                trt_ctx->inputWidth = 640; // YoloDetector默认输入尺寸
                trt_ctx->inputHeight = 640;
            }
        }
    }

    // 执行检测
    std::vector<Detection> detections;
    {
        std::unique_lock<std::mutex> lock(*model_mutex);
        detections = trt_ctx->detector->infer(input);
    }
    
    // 转换结果
    std::vector<FinsObject> results;
    for (const auto& detection : detections) {
        if (detection.conf > conf_threshold) {
            // 确保类别ID在范围内
            int class_id = detection.class_id;
            if (class_id < 0 || class_id >= classes.size()) {
                continue;
            }
            
            // 创建边界框
            cv::Rect bbox(
                static_cast<int>(detection.x1),
                static_cast<int>(detection.y1),
                static_cast<int>(detection.x2 - detection.x1),
                static_cast<int>(detection.y2 - detection.y1)
            );
            
            // 确保边界框有效
            if (bbox.width > 0 && bbox.height > 0 && 
                bbox.x >= 0 && bbox.y >= 0 &&
                bbox.x + bbox.width <= input.cols &&
                bbox.y + bbox.height <= input.rows) {
                FinsObject obj;
                obj.box = bbox;
                obj.confidence = detection.conf;
                obj.className = classes[class_id];
                results.push_back(obj);
            }
        }
    }
    
    // 应用NMS
    if (!results.empty()) {
        std::vector<cv::Rect> boxes;
        std::vector<float> confidences;
        boxes.reserve(results.size());
        confidences.reserve(results.size());
        
        for (const auto& obj : results) {
            boxes.push_back(obj.box);
            confidences.push_back(obj.confidence);
        }
        
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, conf_threshold, nms_threshold, indices);
        
        std::vector<FinsObject> filtered_results;
        filtered_results.reserve(indices.size());
        for (int idx : indices) {
            filtered_results.push_back(results[idx]);
        }
        return filtered_results;
    }
    return results;
}