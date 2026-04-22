// ModelManager.cpp
#include "ModelManager.h"
#include <stdexcept>
#include <iostream>

std::map<int, std::unique_ptr<ModelManager>> ModelManager::instances_;
std::mutex ModelManager::instance_mutex_;

ModelManager& ModelManager::Instance(int cameraId) {
    static std::mutex initMutex;
    std::lock_guard<std::mutex> lock(initMutex);

    auto it = instances_.find(cameraId);
    if (it == instances_.end()) {
        instances_.emplace(cameraId, std::make_unique<ModelManager>());
    }
    return *instances_[cameraId];
}

//void ModelManager::LoadModel(const std::string& model_path, int hardwareType) {
//    std::lock_guard<std::mutex> lock(models_mutex_);
//    
//    if (cv_models_.find(model_path) != cv_models_.end()) {
//        return; // 模型已加载
//    }
//    
//    try {
//        if (model_path.find(".onnx") != std::string::npos) {
//            cv::dnn::Net net = cv::dnn::readNetFromONNX(model_path);
//            auto net = std::make_shared<cv::dnn::Net>(std::move(net));
//            if (net.empty()) {
//                throw std::runtime_error("Failed to load ONNX model: " + model_path);
//            }
//            
//            // 配置硬件加速
//            if (hardwareType == 0) {
//                net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
//                net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
//            } else {
//                net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
//                net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
//            }
//            
//            cv_models_.emplace(model_path, std::move(net));
//            std::cout << model_path << " 模型已加载********************"  << std::endl;
//        }
//    }
//    catch (const std::exception& e) {
//        throw std::runtime_error("Model load failed: " + std::string(e.what()));
//    }
//}
void ModelManager::LoadModel(const std::string& model_path, int hardwareType, int modeType) {
    std::lock_guard<std::mutex> lock(models_mutex_);

    // 检查是否已加载（包括ONNX和TensorRT引擎）
    if (cv_models_.find(model_path) != cv_models_.end() || 
        yolo_detectors_.find(model_path) != yolo_detectors_.end()) {
        return; // 模型已加载
    }

    try {
        if (model_path.find(".onnx") != std::string::npos) {
            // 加载ONNX模型
            auto net_ptr = std::make_shared<cv::dnn::Net>(
                cv::dnn::readNetFromONNX(model_path)
                );

            if (net_ptr->empty()) {
                throw std::runtime_error("Failed to load ONNX model: " + model_path);
            }

            // 配置硬件加速
            if (hardwareType == 0) {
                net_ptr->setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
                net_ptr->setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
                if (modeType == 1)
                {
                    net_ptr->enableFusion(false);//YOLOV11 CUDA关闭 layer fusion（层融合）优化,不然会挂
                }
                
            }
            else {
                net_ptr->setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
                net_ptr->setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            }

            cv_models_.emplace(model_path, net_ptr);
            std::cout << model_path << " ONNX模型已加载" << std::endl;
        }
        else if (model_path.find(".engine") != std::string::npos) {
            // 加载TensorRT引擎文件
            if (hardwareType != 0) {
                throw std::runtime_error("TensorRT engine requires CUDA hardware (hardwareType=0)");
            }
            
            auto detector_ptr = std::make_shared<YoloDetector>(model_path, -1);
            if (!detector_ptr->init()) {
                throw std::runtime_error("Failed to initialize YoloDetector with engine: " + model_path);
            }
            
            yolo_detectors_.emplace(model_path, detector_ptr);
            std::cout << model_path << " TensorRT引擎已加载" << std::endl;
        }
        else {
            throw std::runtime_error("Unsupported model format: " + model_path + " (expected .onnx or .engine)");
        }
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Model load failed: " + std::string(e.what()));
    }
}

std::shared_ptr<cv::dnn::Net> ModelManager::GetCVDNNModel(const std::string& model_path) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    return cv_models_.at(model_path);
}

std::shared_ptr<YoloDetector> ModelManager::GetYoloDetector(const std::string& model_path) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    return yolo_detectors_.at(model_path);
}

bool ModelManager::IsModelLoaded(const std::string& model_path) const {
    std::lock_guard<std::mutex> lock(models_mutex_);
    return cv_models_.find(model_path) != cv_models_.end() || 
           yolo_detectors_.find(model_path) != yolo_detectors_.end();
}

// ModelManager.cpp 实现
void ModelManager::UnloadModel(int cameraId, const std::string& model_path) {
    if (instances_.find(cameraId) != instances_.end()) {
        auto& instance = *instances_[cameraId];
        std::lock_guard<std::mutex> lock(instance.models_mutex_);
        instance.cv_models_.erase(model_path);
        instance.yolo_detectors_.erase(model_path);
    }
}

void ModelManager::ReleaseCameraModels(int cameraId) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (auto it = instances_.find(cameraId); it != instances_.end()) {
        auto& instance = *it->second;
        std::lock_guard<std::mutex> inner_lock(instance.models_mutex_);
        instance.cv_models_.clear();
        instance.yolo_detectors_.clear();
        instances_.erase(it); // 可选：是否保留空管理器实例
    }
}

void ModelManager::ReleaseAllNetResources() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    for (auto& [cameraId, instance] : instances_) {
        std::lock_guard<std::mutex> inner_lock(instance->models_mutex_);
        instance->cv_models_.clear();
        instance->yolo_detectors_.clear();
    }
    instances_.clear();
}
