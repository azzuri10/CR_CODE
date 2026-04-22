#pragma once
#ifndef PRESSCAP_STRUCT_H
#define PRESSCAP_STRUCT_H

#include "HeaderDefine.h"


enum PRESSCAP_RETURN_VAL {
    PRESSCAP_RETURN_ALGO_ERR = -2, //算法异常
    PRESSCAP_RETURN_INPUT_PARA_ERR = -1, //软件输入参数异常
    PRESSCAP_RETURN_TIMEOUT = 0, //算法超时
    PRESSCAP_RETURN_OK = 1, //OK
    PRESSCAP_RETURN_CONFIG_ERR = 11002, //配置错误
    PRESSCAP_RETURN_CAP_CLOSE_LR_BOUNDARY = 11003, //定位-无目标
    PRESSCAP_RETURN_CAP_CLOSE_TOP_BOUNDARY = 11004, //靠近上边界
    PRESSCAP_RETURN_LEAK = 11005, //压盖不严
    PRESSCAP_RETURN_NO_CAP_TOP = 11006, //定位-无上盖
    PRESSCAP_RETURN_NO_CAP = 11007, //定位-无盖
    PRESSCAP_RETURN_CAP_SCRAP = 11008, //定位-缺陷盖
    PRESSCAP_RETURN_FIND_LOW_CAP = 11009, //瓶盖高度偏差-矮盖
    PRESSCAP_RETURN_FIND_HIGH_CAP = 11010, //瓶盖高度偏差-高盖
    PRESSCAP_RETURN_FIND_TOP_ANGLE_ERR = 11011, //盖顶歪斜
    PRESSCAP_RETURN_FIND_BOTTOM_ANGLE_ERR = 11012, //支撑环歪斜
    PRESSCAP_RETURN_FIND_ANGLE_ERR = 11013, //瓶盖歪斜
    PRESSCAP_RETURN_CAP_BOTTOM_TYPE_FAILED = 11014, //盖底类型错误
    PRESSCAP_RETURN_CAP_TOP_TYPE_FAILED = 11015, //盖帽类型错误
    PRESSCAP_RETURN_CAP_CRIMP = 11016, //缺陷-瓶盖破损
    PRESSCAP_RETURN_BAR_BRIDGE_BREAK = 11017, //缺陷-防盗环断桥
    PRESSCAP_RETURN_BAR_CAP_SEP = 11018, //缺陷-上下盖分离
    PRESSCAP_RETURN_BAR_BREAK = 11019, //缺陷-防盗环缺陷
    PRESSCAP_RETURN_DEFECT_LEAK = 11020, //缺陷-压盖不严
    PRESSCAP_RETURN_LR_FAILED = 11021, //缺陷-支撑环端点定位失败

    PRESSCAP_RETURN_OTHER = 11100, //其他
    PRESSCAP_RETURN_THREAD_CONTENTION = 11101, //线程竞争
};


struct InspPressCapIn {
    bool saveDebugImage = false;
    bool saveResultImage = false;
    bool saveLogTxt = false;
    bool drawResult = false;
    int saveTrain = 0; //0: 不存 1: 全部  2:OK  3:NG
    int timeOut;
    int capType = 0;
    bool capOut = false;
    cv::Rect roiRect;
    double rotateAngle = 0;
    int capHeight = 0;
    int capTopY = 0;
    int capHeightErr = 0;
    int capTopHeight = 0;
    int capBottomHeight = 0;



    int hardwareType = 0;
    int modelType = 0;

    std::string locateWeightsFile;   // 目标检测模型
    std::string locateThreshConfig;
    std::vector<std::string> locateClassName;
    std::vector<YoloConfig> locatePara;

    std::string defectWeightsFile;   // 缺陷检测模型
    std::string defectThreshConfig;
    std::vector<std::string> defectClassName;
    std::vector<YoloConfig> defectPara;

    std::string classifyWeightsFile; // 分类模型
    std::string classifyNameFile;
    std::vector<std::string> classifyClassName;
    std::string topType = "不检测";
    std::string bottomType = "不检测";
    std::string specialType = "不检测";

    int bwThresh = 0;
    double topLineAngle = 0.0f;
    double bottomLineAngle = 0.0f;
    double topBottomAngleDif = 0.0f;
    int leakThresh = 0;


};

struct InspPressCapOut {
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
        ProcessImage capRegion;                 // 定位的瓶盖区域
        ProcessImage capRegionGrayConverted;    // 定位的瓶盖区域转灰度图
        ProcessImage capRegionBinarized;        // 定位的瓶盖区域二值化图
        ProcessImage capRegionFil;              // 提取瓶盖区域
        ProcessImage topRegion;                 // 定位的盖帽区域
        ProcessImage bottomRegion;              // 定位的盖底区域
        ProcessImage affineMat;                 // 旋转矩阵
        ProcessImage rotateCap;                 // 旋转矫正瓶盖

        ProcessImage roiLog;                    // 原始ROI区域
        ProcessImage capRegionDetectLog;        // 定位的瓶盖区域
        ProcessImage capRegionDetectFilLog;     // 定位的瓶盖区域(筛选后）
        ProcessImage capRegionDefectLog;        // 瓶盖缺陷定位
        ProcessImage capTopBottomRectLog;       // 盖帽盖底定位
        ProcessImage outputImg;           // 带标注的结果图

    } images{};

    // ============== 几何测量结果 ==============
    struct Geometry {
        cv::Point topPoint;         // 盖帽顶部点坐标
        cv::Point leftTopPoint;     // 盖帽左上角点坐标
        cv::Point rightTopPoint;     // 盖帽右上角点坐标
        cv::Point leftBottomPoint;     // 支撑环左上角点坐标
        cv::Point rightBottomPoint;     // 支撑环右上角点坐标
        cv::Point rotateCenter;     // 旋转中心点
        std::vector<cv::Point> contourTop;  // 盖顶轮廓点集
        std::vector<cv::Point> contourLeftLeak;  // 左侧压盖不严点集
        std::vector<cv::Point> contourRightLeak;  // 右侧压盖不严点集


        cv::Rect capRect;           // 瓶盖区域
        cv::Rect capTopRect;        // 盖帽区域
        cv::Rect capBottomRect;     // 盖底区域
        cv::Rect capNeckLeftRect;   // 支撑环左侧区域
        cv::Rect capNeckRightRect;  // 支撑环右侧区域

        int maxErrL = 0;
        int maxErrR = 0;
        int capHeight = 0;              // 瓶盖高度
        int capTopY = 0;                // 瓶盖盖顶Y坐标
        int capHeighttDeviation = 0;   // 瓶盖高度偏差
        float topAngle = 0.0f;          //盖帽顶部角度
        float bottomAngle = 0.0f;       //支撑环角度
        float topBottomAngleDif = 0.0f; //盖帽顶部与支撑环角度差值
    } geometry{};

    // ============== 初定位检测结果 ==============
    struct LocateInfo {
        bool findNC = false;         // 无盖
        bool findNT = false;          // 无上盖

        std::vector<FinsObject> details; // 定位详细信息
    } locate{};

    // ============== 缺陷检测结果 ==============
    struct DefectInfo {
        bool findNC = false;     //无盖
        bool findNCT = false;     //无上盖
        bool findSCRAP = false;     //异常盖标志
        bool findCRIMP = false;     //盖底缺陷标志
        bool findBARERR = false;    //防盗环缺陷标志
        bool findBARB = false;      //防盗环断桥标志
        bool findBARD = false;      //盖帽盖底分离标志
        bool findLEAK = false;     //压盖不严标志
        bool findTOPB = false;     //盖帽异常

        std::vector<FinsObject> details; // 缺陷详细信息
        cv::Rect mainDefectArea;         // 主要缺陷区域
    } defects{};

    // ============== 分类结果 ==============
    struct Classification {
        FinsClassification topType; // 盖帽类型
        FinsClassification bottomType; // 盖底类型
    } classification;

    // ============== 状态反馈 ==============
    struct RuntimeInfo {
        PRESSCAP_RETURN_VAL statusCode = PRESSCAP_RETURN_OK;
        std::string errorMessage;       // 错误描述
        std::vector<std::string> logs;  // 日志信息
    } status{};
};

#endif // PRESSCAP_STRUCT_H
