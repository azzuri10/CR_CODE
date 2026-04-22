#pragma once
#ifndef BOTTLENECK_STRUCT_H
#define BOTTLENECK_STRUCT_H

#include "HeaderDefine.h"
#include <atomic>
#include <chrono>
#include <mutex>

enum BOTTLENECK_RETURN_VAL {
    BOTTLENECK_RETURN_INPUT_PARA_ERR = -1,                  //软件输入参数异常
    BOTTLENECK_RETURN_TIMEOUT = 0,                          //算法超时
    BOTTLENECK_RETURN_OK = 1,                               // OK
    BOTTLENECK_RETURN_CONFIG_ERR = 19002,                   // 配置错误
    BOTTLENECK_RETURN_LOCATE_OUT_CIRCLE_FAILED = 19003,     // 支撑环定位失败
    BOTTLENECK_RETURN_CLOSE_BOUNDARY = 19004,               // 靠近边界
    BOTTLENECK_RETURN_OUT_NOT_CIRCLE = 19005,               // 椭圆
    BOTTLENECK_RETURN_OUT_IRREGULARITY = 19006,             // 不规则圆
    BOTTLENECK_RETURN_OUT_BREAK0 = 19007,                   // 支撑环边缘缺陷-缺失
    BOTTLENECK_RETURN_OUT_BREAK1 = 19008,                   // 支撑环边缘缺陷-凹凸
    BOTTLENECK_RETURN_CLASSFY_DEFECT0 = 19011,              // 分类-瓶口严重缺陷
    BOTTLENECK_RETURN_CLASSFY_DEFECT1 = 19012,              // 分类-瓶口轻微缺陷
    BOTTLENECK_RETURN_LOCATE_DEFECT_IN0 = 19021,            // 定位-瓶口缺陷0
    BOTTLENECK_RETURN_LOCATE_DEFECT_IN1 = 19022,            // 定位-瓶口缺陷1
    BOTTLENECK_RETURN_LOCATE_DEFECT_OUT0 = 19031,           // 定位-支撑环缺陷0
    BOTTLENECK_RETURN_LOCATE_DEFECT_OUT1 = 19032,           // 定位-支撑环缺陷1


    BOTTLENECK_RETURN_OTHER = 19100,                        // 其他错误
    BOTTLENECK_RETURN_THREAD_CONTENTION = 19101,            // 线程竞争
};

struct InspBottleNeckIn {
    bool saveDebugImage = false;
    bool saveResultImage = false;
    bool saveLogTxt = false;
    bool drawResult = false;
    int saveTrain = 0;  // 0:不保存 1:全保存 2:OK保存 3:NG保存
    bool isCheckOutCircle = false;
    bool defectDL = false;

    cv::Rect roiRect;

    int filterThresh = 0;
    int outCircleBwThresh = 0;

    float outCircleDiameter = 0;
    float outCircleDiameterRange = 0;
    float outCircleDp = 0;
    int outCircleEdgeThresh = 0;

    int neckExt = 0;
    bool fineOut = false;
    float circlePointDevThresh; // 边缘点偏离阈值(范围:0-1000)偏离支撑环边缘点的点剔除
    float outBreakThresh; // 外环边缘缺陷阈值(范围:0-1000)（设置为0不检测，越小越严格）（偏离正常支撑环边缘的连续点进行计数，大于阈值判定为缺陷）
    float outNotCircleThresh;//支撑环椭圆检测阈值(范围:0.0-100)（设置为0不检测，越小越严格）（拟合的圆长短轴偏差）
    float irregularity;//支撑环不规则度检测阈值(范围0-100)（设置为0不检测，越小越严格）（外环边缘点距离圆心的平均距离误差）

    int hardwareType = 0;
    int netType = 0;

    int isClassfy = 0; // 0:不分类, 1:微缺陷判断为OK, 2:微缺陷判断为NG
    std::string neckClassifyWeightsFile; // 模型文件
    std::string neckClassifyNameFile;    // 名称文件
    std::vector<std::string> neckClassifyClassName;

    int isLocate = 0; // 0:不定位 1:定位
    std::string locateWeightsFile;   // 定位模型
    std::string locateThreshConfig;
    std::vector<std::string> locateClassName;
    std::vector<YoloConfig> locatePara;

    int timeOut = 5000; // 超时时间(ms)
};

struct InspBottleNeckOut {
    // ============== 系统信息 ==============
    struct SystemInfo {
        int jobId = -1;                  // 任务ID
        int cameraId = 0;                // 相机ID
        std::string startTime;           // 开始时间(YYYY-MM-DD HH:mm:ss)
        std::chrono::milliseconds elapsedTime; // 耗时(ms)
        std::atomic<bool> timeoutFlag{ false };
    } system;

    // ============== 路径配置 ==============
    struct OutputConfig {
        std::string logDirectory;        // 日志目录
        std::string intermediateImagesDir; // 中间图片目录
        std::string resultsOKDir;        // OK结果目录
        std::string resultsNGDir;        // NG结果目录
        std::string trainDir;            // 训练文件目录
        std::string configFile;          // 算法配置文件
        std::string logFile;             // 日志文件
    } paths;

    // ============== 图像数据 ==============
    struct {
        cv::Mat outputImg;               // 输出图像
        cv::Mat roiImg;                  // ROI图像
        cv::Mat imgNeck;                 // 瓶颈图像
        cv::Mat remapImg;                // 重映射图像
        cv::Mat reDeep;                  // 深度图
    } images;

    // ============== 几何参数 ==============
    struct Geometry {
        std::vector<cv::Point> outEdgePoints;     // 支撑环边缘点集合
        std::vector<cv::Point> outEdgePointsBad;  // 坏边缘点
        std::vector<cv::Point> outlierPoints;     // 椭圆拟合离群点（需要添加这个成员）

    // 缺陷信息
        struct DefectInfo {
            std::vector<cv::Point> points;        // 缺陷点集
            cv::Rect boundingBox;                 // 缺陷边界框
            cv::Point centroid;                   // 缺陷质心
            int pointCount;                       // 缺陷点数
            float severity;                       // 缺陷严重程度(0-1)
        };
        std::vector<DefectInfo> outEdgeDefects;   // 外边缘缺陷
        std::vector<DefectInfo> outlierDefects;   // 离群点缺陷

        cv::Point2f outCenter;                    // 外圆中心
        cv::Vec4f outCircle;                      // 外圆
        float outCircleRadius = 0;                // 外圆半径
        cv::Rect rectNeck;                        // 瓶颈区域
        cv::RotatedRect outCircleBox;             // 外圆旋转矩形
        float ellipticity = 0;                    // 椭圆度
        float irregularity = 0;                   // 不规则度
    } geometry;

    // ============== 统计信息 ==============
    struct Statistics {
        int initialPointCount = 0;       // 初始点数
        int filteredPointCount = 0;      // 过滤后点数
        int outlierCount = 0;            // 偏离点数
        int maxContinuousOutliers = 0;   // 最大连续偏离点数
        double distanceThreshold = 0;    // 距离阈值
    } statistics;

    // ============== 定位结果 ==============
    struct Locate {
        std::vector<FinsObject> details; // 定位详细信息
    } locate;

    // ============== 分类结果 ==============
    struct Classification {
        FinsClassification neckType;
    } classification;

    // ============== 运行时状态 ==============
    struct RuntimeInfo {
        BOTTLENECK_RETURN_VAL statusCode = BOTTLENECK_RETURN_OK;
        std::string errorMessage;
        std::vector<std::string> logs;
        std::string errLog;

        // 修改为 vector 存储键值对，保持插入顺序
        std::vector<std::pair<std::string, double>> stageDurations; // 键值对列表

        // 添加一个方法用于记录耗时
        void AddStageDuration(const std::string& stage, double duration) {
            stageDurations.emplace_back(stage, duration);
        }
    } status;

    int returnVal = BOTTLENECK_RETURN_OK; // 返回值
};

#endif // BOTTLENECK_STRUCT_H