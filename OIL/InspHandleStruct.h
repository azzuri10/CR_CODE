#pragma once
#ifndef HANDLE_STRUCT_H
#define HANDLE_STRUCT_H

#include "HeaderDefine.h"


enum HANDLE_RETURN_VAL {
    HANDLE_RETURN_ALGO_ERR = -2, //算法异常
    HANDLE_RETURN_INPUT_PARA_ERR = -1, //软件输入参数异常
    HANDLE_RETURN_TIMEOUT = 0, //算法超时
    HANDLE_RETURN_OK = 1,                   // OK
    HANDLE_RETURN_CONFIG_ERR = 14002,       //配置错误
    HANDLE_RETURN_HANDLE_MISS = 14003,      //定位-无提手
    HANDLE_RETURN_HANDLE_POS_ERR = 14004,   //定位-不到位
    HANDLE_RETURN_HANDLE_POS_REV = 14005,   //定位-提手压反
    HANDLE_RETURN_NO_FILM = 14006,          //定位-无塑膜
    HANDLE_RETURN_BAD_FILM = 14007,         //定位-坏膜
    HANDLE_RETURN_MULT_TARGET = 14008,      //定位-多个目标
    HANDLE_RETURN_NO_TARGET = 14009,        //定位-无目标

    HANDLE_RETURN_HANDLE_PTYPE_ERR = 14020, //提手类型错误
    HANDLE_RETURN_FILM_TYPE_ERR = 14030,    //塑膜种类错误

    HANDLE_RETURN_OTHER = 14100,            //其他
    HANDLE_RETURN_THREAD_CONTENTION = 14101,//线程竞争
};


struct InspHandleIn {
    bool saveDebugImage = false;
    bool saveResultImage = false;
    bool saveLogTxt = false;
    bool drawResult = false;
    int saveTrain = 0; //0: 不存 1: 全部  2:OK  3:NG
    int timeOut;

    cv::Rect roiRect;


    int checkHandleType = 1; //（0：不检测  1：有无  2：有无 + 到位）
    int checkFilmType = 1; //（0：不检测  1：有无 2:有无+坏膜）

    int hardwareType = 0; //(0:GPU 1:CPU 2:TRT)
    int modelType = 0;//模型类型(0:Y8 1:Y11)

    std::string locateWeightsFile;   // 目标检测模型
    std::string locateThreshConfig;
    std::vector<std::string> locateClassName;
    std::vector<YoloConfig> locatePara;

    std::string handleClassfyFile;   // 提手分类检测模型
    std::string handleClassfyNameFile;   // 提手分类检测类型文件
    std::vector<std::string> handleClassfyName;
    std::string handleType;

    std::string filmClassfyFile;   // 塑膜分类检测模型
    std::string filmClassfyNameFile;   // 塑膜分类检测类型文件
    std::vector<std::string> filmClassfyName;
    std::string filmType;


};

struct InspHandleOut {
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
        std::string logDirectory;           // 日志根目录
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
        ProcessImage handleRegion;              // 原始ROI区域
        ProcessImage filmRegion;                // 原始ROI区域


        ProcessImage roiLog;                    // 原始ROI区域
        ProcessImage locationLog;               // 原始ROI区域
        ProcessImage handleRegionDetectLog;
        ProcessImage filmRegionDetectLog;
        ProcessImage outputImg;           // 带标注的结果图

    } images{};

    // ============== 几何测量结果 ==============
    struct Geometry {

        cv::Rect handleRect;            // 提手区域
        cv::Rect filmRect;              // 塑膜区域


    } geometry{};

    // ============== 初定位检测结果 ==============
    struct LocateInfo {
        bool findHandle = false;         // 提手
        bool findFull = false;          // 满瓶
        bool findEmpty = false;          // 空瓶

        std::vector<FinsObject> details; // 定位详细信息
    } locate{};

    // ============== 分类结果 ==============
    struct Classification {
        FinsClassification handleType; // 提手类型
        FinsClassification filmType; // 塑膜类型
    } classification;


    // ============== 状态反馈 ==============
    struct RuntimeInfo {
        HANDLE_RETURN_VAL statusCode = HANDLE_RETURN_OK;
        std::string errorMessage;       // 错误描述
        std::vector<std::string> logs;  // 日志信息
    } status{};
};

#endif // HANDLE_STRUCT_H
