#pragma once
#ifndef INSPPRESSCAP_H
#define INSPPRESSCAP_H

#include "InspPressCapStruct.h"
#include "LoadJsonConfig.h"
#include "InferenceWorker.h"
#include "Log.h"
#include "AnalyseMat.h"
#include "CalcFun.h"
#include "Common.h"
#include "TxtOperater.h"
#include "DrawAndShowImg.h"
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <chrono>
#include <future>

#define ASYNC_LOG(level, logFile, saveLog, format, ...) \
    AsyncWriteLog(level, logFile, saveLog, format, ##__VA_ARGS__)

class InspPressCap {
private:
    std::unique_ptr<Common> COM;
    std::unique_ptr<AnalyseMat> ANA;
    std::unique_ptr<TxtOperater> TXT;
    std::unique_ptr<DrawAndShowImg> DAS;
    std::unique_ptr<CalcFun> CAL;

    // 超时标志（原子操作保证线程安全）
    std::atomic<bool> m_timeoutFlag{ false };
    std::chrono::high_resolution_clock::time_point m_startTime;  // 使用高精度时钟
    static std::map<int, InspPressCapIn> cameraConfigMap;

public:
    InspPressCap(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
        bool isLoadConfig, int timeOut, InspPressCapOut& outInfo);
    ~InspPressCap();

    int PressCap_Main(InspPressCapOut& outInfo, bool checkTimeout = false);
    std::future<int> RunInspectionAsync(InspPressCapOut& outInfo);

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
    static std::map<std::string, std::string> capDetectionModelMap;
    static std::map<std::string, std::string> capDefectModelMap;
    static std::map<std::string, std::string> capClassifyModelMap;

    std::atomic<bool>* m_timeoutFlagRef = nullptr;
    cv::Mat m_img;
    cv::Mat m_imgRotate;
    InspPressCapIn m_params;

    // 超时检查函数
    bool CheckTimeout(int timeOut) const {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();

        return (elapsed > timeOut);
    }

    bool readParams(cv::Mat img, const std::string& filePath, InspPressCapIn& params,
        InspPressCapOut& outInfo, const std::string& fileName);
    bool validateCameraModels(int cameraId);
    bool loadAllModels(InspPressCapOut& outInfo, bool ini);

    void PressCap_RotateImg(InspPressCapOut& outInfo);
    void PressCap_SetROI(InspPressCapOut& outInfo);
    void PressCap_LocateCap(InspPressCapOut& outInfo);
    void PressCap_CheckDefect(InspPressCapOut& outInfo);
    void PressCap_LocateTopBottom(InspPressCapOut& outInfo);
    void PressCap_LocateTopBottom_CapTpye2(InspPressCapOut& outInfo);
    void PressCap_CheckAngle(InspPressCapOut& outInfo);
    void PressCap_CheckTopType(InspPressCapOut& outInfo);
    void PressCap_CheckBottomType(InspPressCapOut& outInfo);
    void PressCap_Rotate(InspPressCapOut& outInfo);
    void PressCap_CheckLeak(InspPressCapOut& outInfo);
    void PressCap_DrawResult(InspPressCapOut& outInfo);
};

#endif // INSPPRESSCAP_H