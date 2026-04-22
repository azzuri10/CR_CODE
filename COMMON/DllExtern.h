#pragma once
#include "HeaderDefine.h"
#include "InspPressCap.h"
#include "InspHandle.h"
#include "InspLevel.h"
#include "InspLabelAll.h"
#include "InspCode.h"
#include "InspBottleNum.h"
#include "InspBoxBag.h"
#include "InspSew.h"
#include "InspHeatSeal.h"
#include "InspBottleNeck.h"
#include "ModelManager.h"
#include "Log.h"


// InspCapOmniResult结构体定义
struct InspCapOmniResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];
    int capHeight;              // 瓶盖高度
    int capHeightDeviation;    // 瓶盖高度偏差
    float topAngle;             // 盖帽顶部角度
    float bottomAngle;          // 支撑环角度
    float topBottomAngleDif;    // 盖帽顶部与支撑环角度差值
    std::vector<FinsObject> locates;
    std::vector<FinsObject> defect;
    string capTopType;
    string capBottomType;

    // 构造函数初始化
    InspCapOmniResult() :
        jobId(0),
        cameraId(0),
        statusCode(0),
        capHeight(0),
        capHeightDeviation(0),
        topAngle(0.0f),
        bottomAngle(0.0f),
        topBottomAngleDif(0.0f)
    {
        startTime[0] = '\0';
        endTime[0] = '\0';
        errorMessage[0] = '\0';
    }
};

struct InspLevelResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];
    int levelY;                 // 液位高度
    int grayDis;                //液位上下灰度差阈值
    int project;                //投影得分

    // 构造函数初始化
    InspLevelResult() :
        jobId(0),
        cameraId(0),
        statusCode(0),
        levelY(0),
        grayDis(0),
        project(0)
    {
        startTime[0] = '\0';
        endTime[0] = '\0';
        errorMessage[0] = '\0';
    }
};

struct InspHandleResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];
    std::vector<FinsObject> locates;
    string handleType;
    string filmType;

    // 构造函数初始化
    InspHandleResult() :
        jobId(0),
        cameraId(0),
        statusCode(0)
    {
        startTime[0] = '\0';
        endTime[0] = '\0';
        errorMessage[0] = '\0';
    }
};


struct InspLabelAllResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];
    std::vector<FinsObject> locates;
    std::vector<MatchResult> matchResult;
    std::vector<BarResult> barResult;

    // 构造函数初始化
    InspLabelAllResult() :
        jobId(0),
        cameraId(0),
        statusCode(0)
    {
        startTime[0] = '\0';
        endTime[0] = '\0';
        errorMessage[0] = '\0';
    }
};

struct InspCodeResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];



    // 构造函数初始化
    InspCodeResult() :
        jobId(0),
        cameraId(0),
        statusCode(0)
    {
        startTime[0] = '\0';
        endTime[0] = '\0';
        errorMessage[0] = '\0';
    }
};

struct InspBottleNumResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];



    // 构造函数初始化
    InspBottleNumResult() :
        jobId(0),
        cameraId(0),
        statusCode(0)
    {
        startTime[0] = '\0';
        endTime[0] = '\0';
        errorMessage[0] = '\0';
    }
};

struct InspBoxBagResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];



    // 构造函数初始化
    InspBoxBagResult() :
        jobId(0),
        cameraId(0),
        statusCode(0)
    {
        startTime[0] = '\0';
        endTime[0] = '\0';
        errorMessage[0] = '\0';
    }
};

struct InspSewResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];



    // 构造函数初始化
    InspSewResult() :
        jobId(0),
        cameraId(0),
        statusCode(0)
    {
        startTime[0] = '\0';
        endTime[0] = '\0';
        errorMessage[0] = '\0';
    }
};

struct InspHeatSealResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];



    // 构造函数初始化
    InspHeatSealResult() :
        jobId(0),
        cameraId(0),
        statusCode(0)
    {
        startTime[0] = '\0';
        endTime[0] = '\0';
        errorMessage[0] = '\0';
    }
};

struct TemplateConfig {
    int matchType; //匹配方法
    int extW;
    int extH;
    cv::Rect roi;//匹配区域
    int templateCenterX;//模板中心坐标X
    int templateCenterY;//模板中心坐标Y
    int channels;//1灰度匹配 3彩色匹配
    std::vector<double> angleRange;//模板创建角度范围
    double angleStep; //模板创建角度步长
    std::vector<double> scaleRange; //模板缩放范围
    double scaleStep;//模板缩放步长
    int optimization;//模型生成优化选项
    int metric;//匹配度量方式
    int contrast; // 模板对比度阈值（自动或手动）
    int minContrast; //搜索图像的最小对比度
    int subPixel; //像素精度
    double greediness;//搜索贪婪度
    int numLevels; //金字塔层数
    int numMatches; //最大匹配数量
    double maxOverlap; //允许的最大重叠比例
};

struct InspBottleNeckResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];
    std::vector<FinsObject> locates;
    std::vector<FinsObject> defect;
    string capTopType;
    string capBottomType;

    // 构造函数初始化
    InspBottleNeckResult() :
        jobId(0),
        cameraId(0),
        statusCode(0)
    {
        startTime[0] = '\0';
        endTime[0] = '\0';
        errorMessage[0] = '\0';
    }
};


// DLL 函数声明
extern "C" __declspec(dllexport) int CR_DLL_InspCapOmni(
    cv::Mat img,             // 输入图像
    int cameraId,            // 相机ID
    int jobId,               // 任务ID
    const char* configPath,  // 配置路径
    bool loadConfig,         // 是否加载配置
    int timeOut,             // 超时时间(ms)
    InspCapOmniResult * result // 输出结果
);
extern "C" __declspec(dllexport) int CR_DLL_InspLevel(
    cv::Mat img,             // 输入图像
    int cameraId,            // 相机ID
    int jobId,               // 任务ID
    const char* configPath,  // 配置路径
    bool loadConfig,         // 是否加载配置
    int timeOut,             // 超时时间(ms)
    InspLevelResult * result // 输出结果
);
extern "C" __declspec(dllexport) int CR_DLL_InspHandle(
    cv::Mat img,             // 输入图像
    int cameraId,            // 相机ID
    int jobId,               // 任务ID
    const char* configPath,  // 配置路径
    bool loadConfig,         // 是否加载配置
    int timeOut,             // 超时时间(ms)
    InspHandleResult * result // 输出结果
);
extern "C" __declspec(dllexport) int CR_DLL_InspLabelAll(
    cv::Mat img,             // 输入图像
    int cameraId,            // 相机ID
    int jobId,               // 任务ID
    const char* configPath,  // 配置路径
    bool loadConfig,         // 是否加载配置
    int timeOut,             // 超时时间(ms)
    InspLabelAllResult * result // 输出结果
);
extern "C" __declspec(dllexport) int CR_DLL_CreatTemplate(
    cv::Mat img,
    TemplateConfig matchCfg,
    int cameraId,
    cv::Mat * imgOut);
extern "C" __declspec(dllexport) int CR_DLL_InspCode(
    cv::Mat img,             // 输入图像
    int cameraId,            // 相机ID
    int jobId,               // 任务ID
    const char* configPath,  // 配置路径
    bool loadConfig,         // 是否加载配置
    int timeOut,             // 超时时间(ms)
    InspCodeResult * result // 输出结果
);
extern "C" __declspec(dllexport) int CR_DLL_InspBottleNum(
    cv::Mat img,             // 输入图像
    int cameraId,            // 相机ID
    int jobId,               // 任务ID
    const char* configPath,  // 配置路径
    bool loadConfig,         // 是否加载配置
    int timeOut,             // 超时时间(ms)
    InspBottleNumResult * result // 输出结果
);
extern "C" __declspec(dllexport) int CR_DLL_InspBoxBag(
    cv::Mat img,             // 输入图像
    int cameraId,            // 相机ID
    int jobId,               // 任务ID
    const char* configPath,  // 配置路径
    bool loadConfig,         // 是否加载配置
    int timeOut,             // 超时时间(ms)
    InspBoxBagResult * result // 输出结果
);

extern "C" __declspec(dllexport) int CR_DLL_InspBottleNeck(
    cv::Mat img,             // 输入图像
    int cameraId,            // 相机ID
    int jobId,               // 任务ID
    const char* configPath,  // 配置路径
    bool loadConfig,         // 是否加载配置
    int timeOut,             // 超时时间(ms)
    InspBottleNeckResult * result // 输出结果
);



extern "C" __declspec(dllexport) void ReleaseNetResources();
extern "C" __declspec(dllexport) void FreeInspCapResult(InspCapOmniResult * result);
