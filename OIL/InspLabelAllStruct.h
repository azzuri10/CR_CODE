#pragma once
#ifndef LABELALL_STRUCT_H
#define LABELALL_STRUCT_H

#include "HeaderDefine.h"

enum LABELALL_RETURN_VAL {
    LABELALL_RETURN_ALGO_ERR = -2,                          //算法异常
    LABELALL_RETURN_INPUT_PARA_ERR = -1,                    //软件输入参数异常
    LABELALL_RETURN_TIMEOUT = 0,                            //算法超时
    LABELALL_RETURN_OK = 1,                                 // OK
    LABELALL_RETURN_CONFIG_ERR = 16002,                     //配置错误
    LABELALL_RETURN_LOCATE_NO_LABEL = 16003,                //定位-无标
    LABELALL_RETURN_LOCATE_MULT_LABEL = 16004,              //定位-重复贴标
    LABELALL_RETURN_LOCATE_LABEL_BREAK = 16005,             //定位-标签破损
    LABELALL_RETURN_LOCATE_LABEL_CURL = 16006,              //定位-标签翘起
    LABELALL_RETURN_LOCATE_LABEL_FOLD = 16007,              //定位-标签褶皱
    LABELALL_RETURN_LOCATE_NO_BAR = 16008,                  //定位-无一维码
    LABELALL_RETURN_LOCATE_NO_DM = 16009,                   //定位-无二维码
    LABELALL_RETURN_LOCATE_NO_TARGET = 16010,               //定位-无目标
    LABELALL_RETURN_LOCATE_MULT_TARGET = 16011,             //定位-多个目标

    LABELALL_RETURN_MATCH_ERR0 = 16020,                     //模板匹配-匹配失败
    LABELALL_RETURN_MATCH_ERR1 = 16021,                     //模板匹配-错误特征
    LABELALL_RETURN_MATCH_ERR2 = 16022,                     //模板匹配-歪斜
    LABELALL_RETURN_MATCH_ERR3 = 16023,                     //模板匹配-水平偏移
    LABELALL_RETURN_MATCH_ERR4 = 16024,                     //模板匹配-垂直偏移
    LABELALL_RETURN_MATCH_ERR5 = 16025,                     //模板匹配-距离偏移
    LABELALL_RETURN_MATCH_ERR6 = 16026,                     //模板匹配-得分过低

    LABELALL_RETURN_NO_1D = 16040,                          //读码-无一维码
    LABELALL_RETURN_MISS_1D = 16041,                        //读码-少一维码
    LABELALL_RETURN_MULTIPLE_1D = 16042,                    //读码-多一维码
    LABELALL_RETURN_INFO_ERR_1D = 16043,                    //读码-一维码信息错误

    LABELALL_RETURN_NO_2D = 16050,                          //读码-无二维码
    LABELALL_RETURN_MISS_2D = 16051,                        //读码-少二维码
    LABELALL_RETURN_MULTIPLE_2D = 16052,                    //读码-多二维码
    LABELALL_RETURN_INFO_ERR_2D = 16053,                    //读码-二维码信息错误

    LABELALL_RETURN_COLOR_ERR = 16060,                      //颜色错误

    LABELALL_RETURN_OTHER = 16100,                          //其他
    LABELALL_RETURN_THREAD_CONTENTION = 16101,              //线程竞争

};


struct InspLabelAllIn {
    bool saveDebugImage = false;
    bool saveResultImage = false;
    bool saveLogTxt = false;
    bool drawResult = false;
    int saveTrain = 0;//0: 不存 1: 全部  2:OK  3:NG

    bool isCheckLocate;		//是否开启定位（0：否  1：是）
    cv::Rect roiRect;
    int hardwareType;				//目标检测使用硬件(0:GPU 1:CPU 2:GPU_I 3:CPU_I) 
    std::string locateWeightsFile;   // 目标检测模型
    std::string locateThreshConfig;
    std::vector<std::string> locateClassName;
    std::vector<YoloConfig> locatePara;
    int modelType;					//(0::Y8 1:Y12）

    bool isCheckTemplate;			//是否开启模板匹配（种类检测）（0：否  1：是）
    std::string matchConfigFile;		//模板匹配参数路径
    std::vector<MatchConfig> matchPara;

    bool isCheckBar;				//是否检测一维码
    std::string barConfigFile;			//检测参数配置文件
    std::vector<BarConfig> barConfigs; //模板参数


};

struct InspLabelAllOut {
    // ============== 基础信息 ==============
    struct SystemInfo {
        int jobId = -1;
        int cameraId = 0;
        std::string startTime;
        std::chrono::milliseconds elapsedTime;
    } system;

    // ============== 路径配置 ==============
    struct OutputConfig {
        std::string logDirectory;
        std::string intermediateImagesDir;
        std::string resultsOKDir;
        std::string resultsNGDir;
        std::string trainDir;
        std::string configFile;
        std::string logFile;
    } paths{};

    // ============== 图像处理结果 ==============
    struct Images {
        ProcessImage roi;
        ProcessImage barRoi;
        ProcessImage cannyImg;
        ProcessImage roiLog;
        ProcessImage labelAllRegionDetectFilLog;
        ProcessImage outputImg;
    } images{};

    // ============== 几何测量结果 ==============
    struct Geometry {
        std::vector<MatchResult> matchResults;  
    } geometry{};

    struct Bar {
        std::vector <std::vector<BarResult>> barResults;
    } bar{};

    // ============== 初定位检测结果 ==============
    struct LocateInfo {
        cv::Rect lableRect;
        std::vector<FinsObject> details;
    } locate{};

    // ============== 状态反馈 ==============
    struct RuntimeInfo {
        LABELALL_RETURN_VAL statusCode = LABELALL_RETURN_OK;
        std::string errorMessage;
        std::vector<std::string> logs;
    } status{};
};



////opencv
//extern std::vector <std::vector<std::vector<cv::Mat>>> m_labelAllTemplateMats;   //模板图片
//extern std::vector<std::vector<cv::Rect>> m_labelAllTemplateRois;  //模板检测ROI
//extern std::vector <std::vector<std::vector<std::vector<cv::KeyPoint>>>> m_labelAllTemplatesKeypoints; //标签模板关键点
//extern std::vector <std::vector<std::vector<cv::Mat>>>  m_labelAllTemplatesDescriptors; //标签模板描述子
//
////halcon
//extern std::vector <std::vector<std::vector<HTuple>>> m_labelAllTemplateHObjects;   //模板图片

#endif // LABELALL_STRUCT_H
