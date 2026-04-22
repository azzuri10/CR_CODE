#pragma once
#ifndef INSPLABELALL_H
#define INSPLABELALL_H

#include "InspLabelAllStruct.h"
#include "Log.h"
#include "AnalyseMat.h"
#include "CalcFun.h"
#include "MatchFun.h"
#include "Common.h"
#include "TxtOperater.h"
#include "DrawAndShowImg.h"
#include "BarAndQR.h"
#include <mutex>
#include <halconcpp/HalconCpp.h>
#include <opencv2/features2d.hpp>
#include <shared_mutex>

using namespace HalconCpp;

class InspLabelAll {
private:
    std::unique_ptr<Log> LOG;
    std::unique_ptr<Common> COM;
    std::unique_ptr<AnalyseMat> ANA;
    std::unique_ptr<TxtOperater> TXT;
    std::unique_ptr<DrawAndShowImg> DAS;
    std::unique_ptr<BarAndQR> BAQ;
    std::unique_ptr<CalcFun> CAL;
    std::unique_ptr<MatchFun> MF;
    std::atomic<bool> m_timeoutFlag{ false };
    std::chrono::high_resolution_clock::time_point m_startTime;  // 使用高精度时钟
    static std::map<int, InspLabelAllIn> cameraConfigMap;

public:
    InspLabelAll(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
        bool isLoadConfig, int timeOut, InspLabelAllOut& outInfo);
    ~InspLabelAll();

    int LabelAll_Main(InspLabelAllOut& outInfo);

    void SetTimeoutFlagRef(std::atomic<bool>& flag) {
        m_timeoutFlagRef = &flag;
    }

    void SetStartTimePoint(std::chrono::high_resolution_clock::time_point startTime) {
        m_startTime = startTime;
    }
private:
    static std::shared_mutex modelLoadMutex;
    static std::map<std::string, std::string> labelAllDetectionModelMap;  // 检测模型映射表

    std::atomic<bool>* m_timeoutFlagRef = nullptr;
    struct TemplateCache {
        std::vector<cv::Mat> cvTemplates;
        std::vector<HalconCpp::HObject> halconModels;
        bool initialized = false;
    };

    // 相机隔离的模板缓存
    static std::mutex templateMutex_;
    static std::unordered_map<int, TemplateCache> templateCache_;

    cv::Mat m_img;
    cv::Mat m_imgGray; 
    int m_timeOut;

    // 超时检查函数
    bool CheckTimeout(int timeOut) const {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();

        return (elapsed > timeOut);
    }


    InspLabelAllIn m_params;

    bool loadTemplatesForCamera(int cameraId, InspLabelAllOut& outInfo);
    bool readParams(cv::Mat img, const std::string& filePath, InspLabelAllIn& params, InspLabelAllOut& outInfo, const std::string& fileName);
    bool validateCameraModels(int cameraId);
    void ResetModels();
    bool loadAllModels(InspLabelAllOut& outInfo, bool ini);  // 集中加载所有模型
    void LabelAll_SetROI(InspLabelAllOut& outInfo);
    void LabelAll_LocateLabel(InspLabelAllOut& outInfo);
    void LabelAll_MatchTemplate(InspLabelAllOut& outInfo);
    void ProcessMatchResult(InspLabelAllOut& outInfo, int index, int rv,
        MatchResult& result, const std::string& matchTypeName);
    void LabelAll_CheckBar(InspLabelAllOut& outInfo);
    void LabelAll_DrawResult(InspLabelAllOut& outInfo);
    void DrawSIFTMatchResult(InspLabelAllOut& outInfo, const MatchResult& result, cv::Scalar color);
    void DrawHalconMatchResult(InspLabelAllOut& outInfo, const MatchResult& result, cv::Scalar color);
    void CalculateCornersFromContours(HObject ho_Contours, int adjustedRoiX, int adjustedRoiY,
        std::vector<cv::Point2f>& corners);
    void DrawNCCMatchResult(InspLabelAllOut& outInfo, const MatchResult& result, cv::Scalar color);
};

#endif // INSPPRESSCAP_H