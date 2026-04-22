#pragma once
#ifndef INSPHEATSEAL_H
#define INSPHEATSEAL_H

#include "InspHeatSealStruct.h"
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

class InspHeatSeal {
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
    static std::map<int, InspHeatSealIn> cameraConfigMap;

public:
    InspHeatSeal(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
        bool isLoadConfig, int timeOut, InspHeatSealOut& outInfo);

    ~InspHeatSeal() {
        if (m_safeTimeoutFlag) {
            m_safeTimeoutFlag->store(true); // 安全修改
        }
    }

    int HeatSeal_Main(InspHeatSealOut& outInfo, bool checkTimeout = false);
    std::future<int> RunInspectionAsync(InspHeatSealOut& outInfo);

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
    static std::map<std::string, std::string> sewDetectionModelMap;
    static std::map<std::string, std::string> sewClassifyModelMap;

    std::atomic<bool>* m_timeoutFlagRef = nullptr;
    cv::Mat m_img;
    InspHeatSealIn m_params;

    // 超时检查函数
    bool CheckTimeout(int timeOut) const {
        if (m_safeTimeoutFlag && m_safeTimeoutFlag->load()) {
            return true;
        }

        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();

        return (elapsed > timeOut);
    }

    bool readParams(cv::Mat img, const std::string& filePath, InspHeatSealIn& params,
        InspHeatSealOut& outInfo, const std::string& fileName);
    bool validateCameraModels(int cameraId);
    bool loadAllModels(InspHeatSealOut& outInfo, bool ini);

    void HeatSeal_SetROI(InspHeatSealOut& outInfo);
    void HeatSeal_LocateHeatSeal(InspHeatSealOut& outInfo);
    void HeatSeal_CheckHeatSealType(InspHeatSealOut& outInfo);
    void HeatSeal_DrawResult(InspHeatSealOut& outInfo);
};

#endif // INSPHEATSEAL_H