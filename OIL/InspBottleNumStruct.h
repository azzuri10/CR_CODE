#pragma once
#ifndef BOTTLENUM_STRUCT_H
#define BOTTLENUM_STRUCT_H

#include "HeaderDefine.h"

struct TypeComparison {
    std::string typeKey;
    int targetNum;
    int actualNum;
    bool isMatch;
    std::string status;
};

struct BottleCheckResult {
    bool allMatch = true;
    bool findBadCap = false;
    bool findBadHandle = false;
    int totalTarget = 0;
    int totalActual = 0;
    std::vector<TypeComparison> typeComparisons;
    std::vector<std::string> undefinedTypes;
    std::string overallMessage;
};  // 确保有分号

struct Classification {
    std::vector<BottleResult> bottleResult;
    BottleCheckResult checkResult;
};


enum BOTTLENUM_RETURN_VAL {
    BOTTLENUM_RETURN_ALGO_ERR = -2, //算法异常
    BOTTLENUM_RETURN_INPUT_PARA_ERR = -1, //软件输入参数异常
    BOTTLENUM_RETURN_TIMEOUT = 0, //算法超时
    BOTTLENUM_RETURN_OK = 1, //OK
    BOTTLENUM_RETURN_CONFIG_ERR = 12002, //配置错误
    BOTTLENUM_RETURN_BOTTLE_MORE = 12003, //装箱数量多
    BOTTLENUM_RETURN_BOTTLE_LESS = 12004, //装箱数量少
    BOTTLENUM_RETURN_BOTTLE_TYPE_ERR = 12005, //品种错误
    BOTTLENUM_RETURN_BOTTLE_CAP_ERR = 12006, //发现异常瓶盖
    BOTTLENUM_RETURN_BOTTLE_HANDLE_ERR = 12007, //发现异常提手

    BOTTLENUM_RETURN_OTHER = 12100, //其他
    BOTTLENUM_RETURN_THREAD_CONTENTION = 12101, //线程竞争
};


struct InspBottleNumIn {
    bool saveDebugImage = false;
    bool saveResultImage = false;
    bool saveLogTxt = false;
    bool drawResult = false;
    int saveTrain = 0; //0: 不存 1: 全部  2:OK  3:NG
    int timeOut;

    int hardwareType = 0;
    int modelType = 0;
    bool isCheckColor;

    cv::Rect roiRect;

    std::string locateWeightsFile;   // 目标检测模型
    std::string locateThreshConfig;
    std::vector<std::string> locateClassName;
    std::vector<YoloConfig> locatePara;

    std::string capClassifyWeightsFile; //瓶盖分类模型文件
    std::string capClassifyNameFile;//瓶盖分类类型文件
    std::vector<std::string> capClassifyClassName;

    std::string handleClassifyWeightsFile; //提手分类模型文件
    std::string handleClassifyNameFile;//提手分类类型文件
    std::vector<std::string> handleClassifyClassName;

    std::string targetConfigFile;   // 目标类型配置
    int bottleNum = 0;
    std::vector<BottleType> targetType;
};

struct InspBottleNumOut {
    // ============== 基础信息 ==============
    struct SystemInfo {
        int jobId = -1;                  // 任务ID 
        int cameraId = 0;                // 相机编号
        std::string startTime;           // 开始时间(YYYY-MM-DD HH:mm:ss)
        std::chrono::milliseconds elapsedTime; // 处理耗时(ms)

        std::atomic<bool> timeoutFlag{ false };
    } system;

    // ============== 路径配置 ==============
    struct OutputConfig {
        std::string logDirectory;        // 日志根目录
        std::string intermediateImagesDir; // 中间图像目录
        std::string resultsOKDir;          // OK结果目录
        std::string resultsNGDir;          // NG结果目录
        std::string trainDir;               // 训练文件目录
        std::string configFile;            // 算法配置文件
        std::string logFile;               // 日志文件
    } paths{};

    // ============== 图像处理结果 ==============
    struct {
        ProcessImage roi;                       // 原始ROI区域


        ProcessImage outputImg;           // 带标注的结果图

    } images{};

    // ============== 几何测量结果 ==============
    struct Geometry {
       
    } geometry{};

    // ============== 初定位检测结果 ==============
    struct LocateInfo {
      
        std::vector<FinsObject> details; // 定位详细信息
        std::vector<FinsObject> badDetails; 
    } locate{};

    // ============== 缺陷检测结果 ==============
    struct DefectInfo {
        
    } defects{};

    // ============== 分类结果 ==============
    struct Classification {
        std::vector<BottleResult> bottleResult;
        BottleCheckResult checkResult;
    } classification;

    // ============== 状态反馈 ==============
    struct RuntimeInfo {
        BOTTLENUM_RETURN_VAL statusCode = BOTTLENUM_RETURN_OK;
        std::string errorMessage;       // 错误描述
        std::vector<std::string> logs;  // 日志信息
    } status{};
};

#endif // BOTTLENUM_STRUCT_H
