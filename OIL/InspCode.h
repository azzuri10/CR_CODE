#pragma once
#ifndef INSPCODE_H
#define INSPCODE_H

#include "BarAndQR.h"
#include "InspCodeStruct.h"
#include "LoadJsonConfig.h"
#include "InferenceWorker.h"
#include "Log.h"
#include "AnalyseMat.h"
#include "MatchFun.h"
#include "CalcFun.h"
#include "Common.h"
#include "TxtOperater.h"
#include "DrawAndShowImg.h"
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <chrono>
#include <future>

using namespace HalconCpp;
#define ASYNC_LOG(level, logFile, saveLog, format, ...) \
    AsyncWriteLog(level, logFile, saveLog, format, ##__VA_ARGS__)

class InspCode {
private:
    std::unique_ptr<Log> LOG;
    std::unique_ptr<Common> COM;
    std::unique_ptr<AnalyseMat> ANA;
    std::unique_ptr<TxtOperater> TXT;
    std::unique_ptr<MatchFun> MF;
    std::unique_ptr<BarAndQR> BAQ;
    std::unique_ptr<DrawAndShowImg> DAS;
    std::unique_ptr<CalcFun> CAL;

    // 超时标志（原子操作保证线程安全）
    std::atomic<bool> m_timeoutFlag{ false };
    std::chrono::high_resolution_clock::time_point m_startTime;  // 使用高精度时钟
    static std::map<int, InspCodeIn> cameraConfigMap;
    static std::map<int, std::deque<std::string>> recentCodeContents; // 每个相机最近N个喷码内容
    static std::map<int, std::string> lastSerialNumber; // 每个相机上一个流水号
    static std::mutex staticMutex; // 静态互斥锁，保护上述静态成员


public:
    InspCode(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
        bool isLoadConfig, int timeOut, InspCodeOut& outInfo);
    ~InspCode();

    int Code_Main(InspCodeOut& outInfo, bool checkTimeout = false);
    std::future<int> RunInspectionAsync(InspCodeOut& outInfo);

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
    static std::map<std::string, std::string> codeYWModelMap;
    static std::map<std::string, std::string> codeLocateModelMap;
    static std::map<std::string, std::string> codeClassifyModelMap;
    static std::map<std::string, std::string> codeDefectModelMap;

    std::atomic<bool>* m_timeoutFlagRef = nullptr;
    cv::Mat m_img;
    cv::Mat m_imgGray;
    cv::Mat m_imgRotate;
    cv::Mat m_imgWarp;
    cv::Mat m_imgCodeRectOri;
    cv::Mat m_imgLocate;
    InspCodeIn m_params;

    // 超时检查函数
    bool CheckTimeout(int timeOut) const {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();

        return (elapsed > timeOut);
    }

    bool readParams(cv::Mat img, const std::string& filePath, InspCodeIn& params,
        InspCodeOut& outInfo, const std::string& fileName);
    bool validateCameraModels(int cameraId);
    bool loadAllModels(InspCodeOut& outInfo, bool ini);

    void Code_RotateImg(InspCodeOut& outInfo);
    void Code_WarpImg(InspCodeOut& outInfo);
    void Code_SetROI(InspCodeOut& outInfo);
    void Code_Assist(InspCodeOut& outInfo);
    void Code_CheckYW(InspCodeOut& outInfo);
    void Code_LocateCode(InspCodeOut& outInfo);
    void Code_AnalysisCodePos(InspCodeOut& outInfo);
    void Code_CheckCode(InspCodeOut& outInfo);
    void Code_CheckRepeat(InspCodeOut& outInfo);
    void Code_ClassfyCode(InspCodeOut& outInfo);
    void Code_Rotate(InspCodeOut& outInfo);

    void Code_ValidateAllOCRResults(InspCodeOut& outInfo);
    void Code_ValidateSimpleOCRResults(InspCodeOut& outInfo);

    void Code_SaveLocate(InspCodeOut& outInfo);
    void Code_SaveChars(InspCodeOut& outInfo);

    void drawDetectionResults(cv::Mat& image,
        const std::vector<DetectionResult>& results,
        const std::vector<TargetConfig>& targets);
    void Code_CheckBar(InspCodeOut& outInfo);
    void Code_DrawResult(InspCodeOut& outInfo);
};

#endif // INSPCODE_H