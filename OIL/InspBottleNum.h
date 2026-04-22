#pragma once
#ifndef INSPBOTTLENUM_H
#define INSPBOTTLENUM_H

#include "InspBottleNumStruct.h"
#include "LoadJsonConfig.h"
#include "InferenceWorker.h"
#include "Log.h"
#include "AnalyseMat.h"
#include "CalcFun.h"
#include "Common.h"
#include "TxtOperater.h"
#include "DrawAndShowImg.h"
#include "../DETECT/yolo26_detector.h"
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <chrono>
#include <future>
#include <unordered_set>

#define ASYNC_LOG(level, logFile, saveLog, format, ...) \
    AsyncWriteLog(level, logFile, saveLog, format, ##__VA_ARGS__)

class InspBottleNum {
private:
    std::unique_ptr<Log> LOG;
    std::unique_ptr<Common> COM;
    std::unique_ptr<AnalyseMat> ANA;
    std::unique_ptr<TxtOperater> TXT;
    std::unique_ptr<DrawAndShowImg> DAS;
    std::unique_ptr<CalcFun> CAL;

    // 超时标志（原子操作保证线程安全）
    std::shared_ptr<std::atomic<bool>> m_safeTimeoutFlag;
    std::chrono::high_resolution_clock::time_point m_startTime;  // 使用高精度时钟
    static std::map<int, InspBottleNumIn> cameraConfigMap;

public:
    InspBottleNum(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
        bool isLoadConfig, int timeOut, InspBottleNumOut& outInfo);

    ~InspBottleNum() {
        if (m_safeTimeoutFlag) {
            m_safeTimeoutFlag->store(true); // 安全修改
        }
    }

    int BottleNum_Main(InspBottleNumOut& outInfo, bool checkTimeout = false);
    std::future<int> RunInspectionAsync(InspBottleNumOut& outInfo);

    // 设置超时标志引用
    void SetTimeoutFlagRef(std::atomic<bool>& flag) {
        m_timeoutFlagRef = &flag;
    }

    void SetStartTimePoint(std::chrono::high_resolution_clock::time_point startTime) {
        m_startTime = startTime;
    }

private:
    // 静态资源（模型加载相关）
    static std::shared_mutex modelLoadMutex;
    static std::map<std::string, std::string> bottleNumDetectionModelMap;
    static std::map<std::string, std::string> capClassifyModelMap;
    static std::map<std::string, std::string> handleClassifyModelMap;
    static std::map<std::string, std::shared_ptr<YoloDetector>> yoloDetectorMap;

    std::atomic<bool>* m_timeoutFlagRef = nullptr;
    cv::Mat m_img;  // 原始图像数据
    InspBottleNumIn m_params;

    // 超时检查函数
    bool CheckTimeout(int timeOut) const {
        if (m_safeTimeoutFlag && m_safeTimeoutFlag->load()) {
            return true;
        }

        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();

        return (elapsed > timeOut);
    }

    bool readParams(cv::Mat img, const std::string& filePath, InspBottleNumIn& params,
        InspBottleNumOut& outInfo, const std::string& fileName);
    bool validateCameraModels(int cameraId);
    bool loadAllModels(InspBottleNumOut& outInfo, bool ini);

    void BottleNum_SetROI(InspBottleNumOut& outInfo);
    void BottleNum_LocateBottle(InspBottleNumOut& outInfo);
    void BottleNum_CheckBottleType(InspBottleNumOut& outInfo);
    void BottleNum_MatchBottles(InspBottleNumOut& outInfo);
    void BottleNum_CheckResult(InspBottleNumOut& outInfo);
    void BottleNum_SaveForTrain(InspBottleNumOut& outInfo);
    void BottleNum_DrawResult(InspBottleNumOut& outInfo);
};

#endif // INSPBOTTLENUM_H