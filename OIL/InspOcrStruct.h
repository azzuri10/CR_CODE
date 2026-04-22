#pragma once
#ifndef OCR_STRUCT_H
#define OCR_STRUCT_H

#include "HeaderDefine.h"

enum OCR_RETURN_VAL {
    OCR_RETURN_ALGO_ERR = -2,
    OCR_RETURN_INPUT_PARA_ERR = -1,
    OCR_RETURN_TIMEOUT = 0,
    OCR_RETURN_OK = 1,
    OCR_RETURN_CONFIG_ERR = 19001,
    OCR_RETURN_NO_TEXT = 19002,
    OCR_RETURN_TEXT_MISMATCH = 19003,
};

struct OcrBasicConfig {
    cv::Rect roi;
    int extW = 0;
    int extH = 0;
};

struct InspOcrIn {
    bool saveDebugImage = false;
    bool saveResultImage = false;
    bool saveLogTxt = false;
    int timeOut = 3000;

    OcrBasicConfig basic;

    std::string detModel;
    std::vector<std::string> detClassNames;
    float detConf = 0.25f;
    float detNms = 0.45f;

    std::string recModel;
    std::vector<std::string> recClassNames;
    float recConf = 0.25f;
    size_t recSchedChunk = 32;
    size_t recInferChunk = 16;
    int recThreads = 4;

    bool enableCls = false;
    std::string clsModel;
    float clsConf = 0.9f;
    int clsBatch = 8;

    bool preferPaddleNative = true;
    std::string recDictPath;
    bool useGpu = true;
    bool useTrt = true;
    bool useMkldnn = false;
    int gpuId = 0;
    int gpuMemMB = 2048;
    int cpuThreads = 8;
    std::string precision = "fp16";

    std::string infoConfig;
    CodeInfo inputInfo;
};

struct InspOcrOut {
    struct SystemInfo {
        int jobId = -1;
        int cameraId = 0;
        std::string startTime;
        std::atomic<bool> timeoutFlag{ false };
    } system;

    struct OutputConfig {
        std::string logDirectory;
        std::string intermediateImagesDir;
        std::string resultsOKDir;
        std::string resultsNGDir;
        std::string configFile;
        std::string logFile;
    } paths;

    struct ImageInfo {
        cv::Mat roi;
        cv::Mat outputImg;
    } images;

    struct OcrInfo {
        std::vector<FinsObject> detBoxes;
        std::vector<std::string> recTexts;
        std::vector<float> recScores;
        std::string mergedText;
        std::vector<DetectionResult> compareResults;
    } ocr;

    struct RuntimeInfo {
        OCR_RETURN_VAL statusCode = OCR_RETURN_OK;
        std::string errorMessage;
    } status;
};

#endif // OCR_STRUCT_H

