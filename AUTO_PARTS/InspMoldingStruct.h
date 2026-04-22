#pragma once
#pragma once
#ifndef MOLDING_STRUCT_H
#define MOLDING_STRUCT_H

#include "HeaderDefine.h"


enum MOLDING_RETURN_VAL {
    MOLDING_RETURN_OK = 1, //OK
    MOLDING_RETURN_CONFIG_ERR = 21002, //配置错误
    MOLDING_RETURN_FIND_FM = 21003, //异物
    MOLDING_RETURN_FIND_BAD = 21004, //坏料
    MOLDING_RETURN_FIND_NONE = 21005, //无目标
    MOLDING_RETURN_FIND_HJ = 21006, //缺胶、断裂、划痕hungry joint
    MOLDING_RETURN_FIND_EE = 21007, //多胶、飞边excess epoxy
    MOLDING_RETURN_FIND_HJL = 21008, //轻微粘合异常
    MOLDING_RETURN_FIND_OC = 21009, //碎料
    MOLDING_RETURN_FIND_MULTY = 21010, //多目标
    MOLDING_RETURN_FIND_MISS = 21011, //少目标
    

    MOLDING_RETURN_OTHER = 21100, //其他
    MOLDING_RETURN_INPUT_PARA_ERR = 21101, //软件输入参数异常
    MOLDING_RETURN_THREAD_CONTENTION = 21102, //线程竞争
    MOLDING_RETURN_ALGO_ERR = 21103, //算法异常
};


struct InspMoldingIn {
    bool saveDebugImage = false;
    bool saveResultImage = false;
    bool saveLogTxt = false;
    bool drawResult = false;
    int saveTrain = 0;

    cv::Rect roiRect;

    int hardwareType = 0;
    int modelType = 0;

    std::string locateWeightsFile;   // 目标检测模型
    std::vector<std::string> locateClassName;

    std::string defectWeightsFile;   // 缺陷检测模型
    std::string defectNameFile;
    std::string defectThreshConfig;
    std::vector<std::string> defectClassName;
    std::vector<DetectionCriteria> defectPara;



};

struct InspMoldingOut {
    // ============== 基础信息 ==============
    struct SystemInfo {
        int jobId = -1;                  // 任务ID 
        int cameraId = 0;                // 相机编号
        std::string startTime;           // 开始时间(YYYY-MM-DD HH:mm:ss)
        std::chrono::milliseconds elapsedTime; // 处理耗时(ms)
    } system;

    // ============== 路径配置 ==============
    struct OutputConfig {
        std::string logDirectory;        // 日志根目录
        std::string intermediateImagesDir; // 中间图像目录
        std::string resultsOKDir;          // OK结果目录
        std::string resultsNGDir;          // NG结果目录
        std::string configFile;            // 算法配置文件
        std::string logFile;               // 日志文件
    } paths{};

    // ============== 图像处理结果 ==============
    struct {
        ProcessImage roi;                           // 原始ROI区域
        //std::vector <ProcessImage> moldingRegion;   // 定位的注塑件区域
        std::vector<cv::Mat> moldingImgs;           // 注塑件区域
        ProcessImage moldingRegionFil;              // 提取注塑件区域

        ProcessImage roiLog;                        // 原始ROI区域
        ProcessImage moldingRegionDetectLog;

        ProcessImage outputImg;                     // 带标注的结果图

    } images{};

    // ============== 几何测量结果 ==============
    struct Geometry { 
        std::vector<cv::Rect> moldingRects;           // 注塑件区域

    } geometry{};

    // ============== 初定位检测结果 ==============
    struct LocateInfo {
        std::vector<FinsObjectRotate> details; // 定位详细信息
        std::vector<FinsObjectRotate> okDetails; // 定位详细信息
        std::vector<FinsObjectRotate> badDetails; // 定位详细信息
    } locate{};

    // ============== 缺陷检测结果 ==============
    struct DefectInfo {
        bool findHJ = false;     //缺胶、断裂、划痕hungry joint
        bool findEE = false;     //多胶、飞边excess epoxy
        bool findFR = false;     //磨花frayed



        std::vector<FinsObject> details; // 缺陷详细信息
    } defects{};


    // ============== 状态反馈 ==============
    struct RuntimeInfo {
        MOLDING_RETURN_VAL statusCode = MOLDING_RETURN_OK;
        std::string errorMessage;       // 错误描述
        std::vector<std::string> logs;  // 日志信息
    } status{};
};

#endif // MOLDING_STRUCT_H
