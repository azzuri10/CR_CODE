#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include <opencv2/dnn.hpp>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <tbb/concurrent_unordered_map.h>
#include <filesystem>
#include "../DETECT/yolo26_detector.h"

class ModelManager {
public:
    // 获取指定相机ID的单例实例
    static ModelManager& Instance(int cameraId);

    // 加载指定模型
    void LoadModel(const std::string& model_path, int hardwareType, int modeType=0);

    // 获取OpenCV DNN模型
    std::shared_ptr<cv::dnn::Net> GetCVDNNModel(const std::string& model_path);

    // 获取YoloDetector模型 (用于TensorRT引擎)
    std::shared_ptr<YoloDetector> GetYoloDetector(const std::string& model_path);

    // 检查模型是否已加载
    bool IsModelLoaded(const std::string& model_path) const;

    // 释放指定模型
    static void UnloadModel(int cameraId, const std::string& model_path);

    // 释放相机所有模型
    static void ReleaseCameraModels(int cameraId);

    // 全局释放所有模型
    static void ReleaseAllNetResources();

    // 公有构造函数
    ModelManager() = default;

private:
    // 不同相机ID对应的模型管理器实例映射
    static std::map<int, std::unique_ptr<ModelManager>> instances_;
    static std::mutex instance_mutex_;

    mutable std::mutex models_mutex_;
    std::unordered_map<std::string, std::shared_ptr<cv::dnn::Net>> cv_models_;
    std::unordered_map<std::string, std::shared_ptr<YoloDetector>> yolo_detectors_;
};

#endif // MODEL_MANAGER_H