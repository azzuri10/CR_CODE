#pragma once
#ifndef SEW_STRUCT_H
#define SEW_STRUCT_H

#include "HeaderDefine.h"

enum SEW_RETURN_VAL {
    SEW_RETURN_ALGO_ERR = -2,
    SEW_RETURN_INPUT_PARA_ERR = -1,
    SEW_RETURN_TIMEOUT = 0,
    SEW_RETURN_OK = 1,
    SEW_RETURN_CONFIG_ERR = 20002,
    SEW_RETURN_BAD = 20003,//缝线异常
    SEW_RETURN_POSERR = 20004,//异常袋子（仅底带使用）


    SEW_RETURN_NO_TARGET = 20009, //无目标

    SEW_RETURN_OTHER = 20100,
    SEW_RETURN_THREAD_CONTENTION = 20101,
};

struct InspSewIn {
    bool saveDebugImage = false;
    bool saveResultImage = false;
    bool saveLogTxt = false;
    bool drawResult = false;
    int saveTrain = 0;
    int timeOut;

    int hardwareType = 0;
    int modelType = 0;
    bool isCheckColor;

    cv::Rect roiRect;

    std::string locateWeightsFile;
    std::string locateThreshConfig;
    std::vector<std::string> locateClassName;
    std::vector<YoloConfig> locatePara;

    std::string sewClassifyWeightsFile;
    std::string sewClassifyNameFile;
    std::vector<std::string> sewClassifyClassName;

    std::string targetConfigFile;
    int bottleNum = 0;
    std::vector<BottleType> targetType;
};

struct InspSewOut {
    struct SystemInfo {
        int jobId = -1;
        int cameraId = 0;
        std::string startTime;
        std::chrono::milliseconds elapsedTime;
        std::atomic<bool> timeoutFlag{ false };
    } system;

    struct OutputConfig {
        std::string logDirectory;
        std::string intermediateImagesDir;
        std::string resultsOKDir;
        std::string resultsNGDir;
        std::string trainDir;
        std::string configFile;
        std::string logFile;
    } paths{};

    // 修复图像结构体定义
    struct ImageInfo {
        cv::Mat roiImg;          
        cv::Mat outputImg;       
    } images{};

    struct Geometry {
    } geometry{};

    struct LocateInfo {
        std::vector<FinsObject> details;
        std::vector<FinsObject> badDetails;
    } locate{};

    struct DefectInfo {
    } defects{};

    struct Classification {
        FinsClassification checkResult;
    } classification;

    struct RuntimeInfo {
        SEW_RETURN_VAL statusCode = SEW_RETURN_OK;
        std::string errorMessage;
        std::vector<std::string> logs;
    } status{};
};

#endif // SEW_STRUCT_H