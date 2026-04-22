#pragma once
#ifndef BOXBAG_STRUCT_H
#define BOXBAG_STRUCT_H

#include "HeaderDefine.h"

enum BOXBAG_RETURN_VAL {
    BOXBAG_RETURN_ALGO_ERR = -2,
    BOXBAG_RETURN_INPUT_PARA_ERR = -1,
    BOXBAG_RETURN_TIMEOUT = 0,
    BOXBAG_RETURN_OK = 1,
    BOXBAG_RETURN_CONFIG_ERR = 21002,
    BOXBAG_RETURN_BAG = 21003,//露袋



    BOXBAG_RETURN_OTHER = 21100,
    BOXBAG_RETURN_THREAD_CONTENTION = 21101,
};

struct InspBoxBagIn {
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

struct InspBoxBagOut {
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
        BOXBAG_RETURN_VAL statusCode = BOXBAG_RETURN_OK;
        std::string errorMessage;
        std::vector<std::string> logs;
    } status{};
};

#endif // BOXBAG_STRUCT_H