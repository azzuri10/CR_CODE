#pragma once
#pragma once
#include <opencv2/opencv.hpp>
#include <openvino/openvino.hpp>
#include <mutex>

struct PerCameraData {
    // 模型相关
    cv::dnn::Net net;
    ov::CompiledModel compiled_model;
    ov::InferRequest infer_request;

    // 处理缓存
    std::vector<cv::Mat> outs;
    std::vector<cv::String> names;
    std::vector<int> outLayers;
    std::vector<cv::String> layersNames;

    // 状态控制
    bool processing = false;
    std::mutex mtx;
};

class CameraDataPool {
public:
    static CameraDataPool& getInstance() {
        static CameraDataPool instance;
        return instance;
    }

    PerCameraData& getCameraData(int cameraId) {
        std::lock_guard<std::mutex> lock(poolMutex);
        if (cameraId >= dataPool.size()) {
            dataPool.resize(cameraId + 1);
        }
        return dataPool[cameraId];
    }

private:
    CameraDataPool() = default;
    std::vector<PerCameraData> dataPool;
    std::mutex poolMutex;
};