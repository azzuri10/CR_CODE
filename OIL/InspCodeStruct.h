#pragma once
#ifndef CODE_STRUCT_H
#define CODE_STRUCT_H

#include "HeaderDefine.h"



enum CODE_RETURN_VAL {
    CODE_RETURN_ALGO_ERR = -2, //算法异常
    CODE_RETURN_INPUT_PARA_ERR = -1, //软件输入参数异常
    CODE_RETURN_TIMEOUT = 0, //算法超时
    CODE_RETURN_OK = 1, //OK
    CODE_RETURN_CONFIG_ERR = 18002, //配置错误
    CODE_RETURN_YW_NO_CODE = 18003, //有无检测-无喷码
    CODE_RETURN_YW_BAD_CODE = 18004, //有无检测-缺陷喷码
    CODE_RETURN_YW_MULTY_CODE = 18005, //有无检测-多次喷码
    CODE_RETURN_YW_LONG = 18006, //有无检测-喷码过长
    CODE_RETURN_YW_SHORT = 18007, //有无检测-喷码过短
    CODE_RETURN_YW_HIGH = 18008, //有无检测-喷码过高
    CODE_RETURN_YW_LOW = 18009, //有无检测-喷码过矮

    CODE_RETURN_LOCATE_NO_CODE = 18011, //字符定位-无喷码
    CODE_RETURN_LOCATE_LESS= 18012, //字符定位-字符缺失
    CODE_RETURN_LOCATE_MORE = 18013, //字符定位-字符多出

    CODE_RETURN_CLASSFY_INFO_ERR = 18021, //字符识别-喷码信息错误
    CODE_RETURN_CLASSFY_INFO_REPEAT = 18022, //字符识别-喷码信息与之前产品重复

    CODE_RETURN_DEFECT_CHAR = 18031, //字符缺陷

    CODE_RETURN_MATCH_ERR0 = 18040,                     //辅助定位-匹配失败
    CODE_RETURN_MATCH_ERR1 = 18041,                     //辅助定位-歪斜
    CODE_RETURN_MATCH_ERR2 = 18042,                     //辅助定位-水平偏移
    CODE_RETURN_MATCH_ERR3 = 18043,                     //辅助定位-垂直偏移
    CODE_RETURN_MATCH_ERR4 = 18044,                     //辅助定位-距离偏移
    CODE_RETURN_MATCH_ERR5 = 18045,                     //辅助定位-得分过低

    CODE_RETURN_NO_1D = 18050,                          //读码-无一维码
    CODE_RETURN_MISS_1D = 18051,                        //读码-少一维码
    CODE_RETURN_MULTIPLE_1D = 18052,                    //读码-多一维码
    CODE_RETURN_INFO_ERR_1D = 18053,                    //读码-一维码信息错误

    CODE_RETURN_NO_2D = 18060,                          //读码-无二维码
    CODE_RETURN_MISS_2D = 18061,                        //读码-少二维码
    CODE_RETURN_MULTIPLE_2D = 18062,                    //读码-多二维码
    CODE_RETURN_INFO_ERR_2D = 18063,                    //读码-二维码信息错误


    CODE_RETURN_OTHER = 18100, //其他
    CODE_RETURN_THREAD_CONTENTION = 18101, //线程竞争
};


struct CodeBasic {
    cv::Rect roi;               // roi
    double rotateAngle;         //喷码角度(范围:-180-180)
    cv::Point rotateCenter;    //喷码旋转中心X坐标， 若X、Y同时为0,则以图像中心为旋转中心


    int warp;           //曲线矫正 0不启用 1,曲线凸起 2曲线凹陷
    double warpL;       //喷码起始高度比例 范围0.0-1.0
    double warpM;       //喷码中点高度比例 范围0.0-1.0
    double warpR;       //喷码终点高度比例 范围0.0-1.0

    std::vector<int> charNumRange; //喷码字符数量范围
    int lineWordMinNum;          //单行喷码字符最小数量(范围:1-100)
    int charMaxDis;              //字符之间的最大距离(范围:1-1000)
    int lineDis;              //行间距(范围:1-1000)
    std::vector<int> codeWidthRange; //喷码长度范围
    std::vector<int> codeHeightRange; //喷码高度范围
    std::vector<int> codeAngleRange; //喷码旋转角度范围
    int extW;
    int extH;

};

struct CodeClassfy {
    std::string type;            // 字符类型
    double classifyScoreThresh;  // 字符分类得分阈值
    std::vector<int> charWidthRange; //字符长度范围
    std::vector<int> charHeightRange; //字符高度范围

    int defectCheckMethod;          //字符缺陷检测方法 0：辅助定位，字符大小不可缩放 1：辅助定位字符大小可缩放 2：深度学习分类
    double defectScoreThresh;       // 字符缺陷得分阈值
    std::string charImgTemplate;    // 字符模板路径
    std::string charDefectModel;    // 字符模型路径

};

struct InspCodeIn {
    bool saveDebugImage = false;
    bool saveResultImage = false;
    bool saveLogTxt = false;
    bool drawResult = false;
    int saveTrain = 0; //0: 不存 1: 全部  2:OK  3:NG
    int timeOut;


    int hardwareType = 0;
    int modelType = 0;

    std::string basicConfig;   // 喷码基础参数
    CodeBasic basicInfo;

    bool isAssist = false;
    std::string assistConfig;   // 辅助定位配置参数
    MatchLocateConfig assistPara;

    bool isYW = false;
    std::string ywConfig;   // 有无检测模型配置
    std::string ywModel;   // 有无检测模型
    std::vector<std::string> ywClassName;
    std::vector<YoloConfig> ywPara;

    bool isLocate= false;
    cv::Rect checkArea;
    std::string locateConfig;   // 字符定位模型配置
    std::string locateModel;   // 字符定位模型
    std::vector<std::string> locateClassName;
    std::vector<YoloConfig> locatePara;
    float conf_threshold;
    float nms_threshold;


    bool isClassfy = false;
    std::string infoConfig;   // 喷码内容匹配
    std::string classfyConfig;   // 字符分类模型配置
    std::string classfyModel;   // 字符定位模型
    std::vector<std::string> classfyClassName; //字符类型
    std::vector<CodeClassfy> classfyPara;//字符参数
    CodeInfo inputInfo;

    bool isDefect = false;
    std::vector <std::string> defectConfig;   // 字符缺陷模型配置   
    std::vector <std::string> defectModel;   // 字符定位模型 
    std::vector <std::vector<std::string>> defectClassName;
    std::string matchDefectConfigFile;		//辅助定位参数路径
    std::vector<MatchConfig> matchDefectPara;

    bool isCheckBar;				//是否检测一维码
    std::string barConfigFile;			//检测参数配置文件
    std::vector<BarConfig> barConfigs; //模板参数

};


struct InspCodeOut {
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

        ProcessImage codeRegionDetectLog;
        cv::Mat outputImg;           // 带标注的结果图

    } images{};

    // ============== 几何测量结果 ==============
    struct Geometry {
        cv::Rect codeRect; //有无定位喷码区域
        cv::Rect codeLocateRect = cv::Rect(10000,10000,0,0); //字符定位喷码区域

        MatchResult matchLocateResult;
        cv::RotatedRect transformedRoi;

        std::vector<MatchConfig> matchPara;
    } geometry{};

    // ============== 初定位检测结果 ==============
    struct LocateInfo {

        std::vector<FinsObject> ywDetails; // 
        std::vector<FinsObject> locateDetails; // 
        std::vector<FinsObject> badLocateDetails; // 
        std::vector <std::vector<FinsObject>> lineDetails;
        std::vector <std::vector<FinsObject>> badDetails;
        std::vector<std::pair<int, std::string>> codeInfo;
    } locate{};

    

    // ============== 分类结果 ==============
    struct Classification {
        std::vector<DetectionResult> codeMatchResult;
        std::vector <FinsClassification> charType; // 字符类型
        std::vector <FinsClassification>  defectType; // 缺陷类型
    } classification;

    // ============== 状态反馈 ==============
    struct RuntimeInfo {
        CODE_RETURN_VAL statusCode = CODE_RETURN_OK;
        std::string errorMessage;       // 错误描述
        std::vector<std::string> logs;  // 日志信息
    } status{};

    struct Bar {
        std::vector <std::vector<BarResult>> barResults;
    } bar{};
};

#endif // CODE_STRUCT_H
