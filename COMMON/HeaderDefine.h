#pragma once

#ifndef PROJECT_HEADER_DEFINE_H
#define PROJECT_HEADER_DEFINE_H

// Standard library headers
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>
#include <future>
#include <algorithm>
#include <locale>
#include <format>
#include <map>
#include <ctime>
#include <fcntl.h>
#include <io.h>
#include <regex>
// Platform-specific headers
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif

// Computer vision libraries
#include <opencv2/opencv.hpp>

// Third-party SDKs
#include "HalconCpp.h"
#if defined(__has_include)
#if __has_include(<NvInfer.h>)
#include <NvInfer.h>
#define PROJECT_HAS_TENSORRT 1
#elif __has_include("NvInfer.h")
#include "NvInfer.h"
#define PROJECT_HAS_TENSORRT 1
#else
#define PROJECT_HAS_TENSORRT 0
namespace nvinfer1 { class IBuilder; }
#endif
#else
#include <NvInfer.h>
#define PROJECT_HAS_TENSORRT 1
#endif

// Namespace aliases
namespace cvx = cv;

using namespace::cv;
using HalconCpp::HTuple;
using HalconCpp::HObject;
#if PROJECT_HAS_TENSORRT
using nvinfer1::IBuilder;
#endif
using namespace std::chrono_literals;


// Constants configuration
namespace ProjectConstants {
	constexpr auto CONFIG_PATH = "D:/CONFIG_CR_3.0/";
	constexpr auto LOG_PATH = "D:/LOG_CR_3.0/";
	constexpr auto TRAIN_PATH = "E:/TRAIN_CR_3.0/";
	constexpr int MAX_CAMERA_NUM = 10;
}


enum ResType {
	NG,
	OK,
	INGORE
};

// 目标配置
struct TargetConfig {
	int row;
	int part;
	std::string type;
	std::string info;
	cv::Rect roi;
	std::pair<int, int> charWidthRange;
	std::pair<int, int> charHeightRange;
	bool changeNum;
};

// 检测结果结构体
struct DetectionResult {
	int row;
	int part;
	std::string type;
	std::string expectedInfo;
	std::string actualInfo;
	std::string status;
	std::string message;
	int expectedLength;
	int charNum;
};


// 全局配置
struct CodeInfo {
	int extW;
	int extH;
	int checkType;
	bool fuzzy;
	std::string fuzzyConfig;
	int infoRepeat;
	int timeError;
	int expirationDateYear;
	int expirationDateMonth;
	int expirationDateDay;
	int expirationDateError;
	bool ignoreRow;
	std::vector<TargetConfig> targets;

	// 记录上一个生产流水号
	std::map<std::string, std::string> lastSerialNumbers;
};




struct MatchConfig {
	int matchType; //匹配方法
	std::vector <std::string> templatePaths; //模板路径文件夹
	bool templateType = true; //是否正常模板（false:不能出现的模板）
	std::vector <cv::Rect> rois;
	int extW;
	int extH;
	int templateCenterX;//模板中心坐标
	int templateCenterY;
	int shiftHor;//模板中心偏移水平阈值
	int shiftVer;//模板中心偏移垂直阈值
	int offset;//模板中心偏移距离
	int channels;//1 黑白 3彩色 
	int numLevels; //值越大，搜索越快但精度越低（建议设为0，由系统自动计算）
	std::vector<double> angleRange;//角度范围
	double angleStep; //角度步长（单位：度）或设置为“auto”自动选择
	std::vector<double> scaleRange;
	bool warp = false;
	std::vector<std::vector<double>> warpRange;
	double scaleStep;
	int optimization;
	int metric; 
	int polarity;
	int contrast; //模板对比度阈值​： -数值（如30）：手动指定边缘梯度阈值 - "auto"：系统自动计算
	int minContrast; //最小对比度阈值​：搜索时忽略低于此阈值的边缘（噪声过滤，建议设为5 - 10）
	int subPixel; //像素精度
	double greediness;
	int numMatches; //- 0表示返回所有匹配 -通常设为1（找最佳匹配）或10（多目标）
	double maxOverlap; //最大重叠度​（0~1）： -抑制重叠检测结果（0 = 完全抑制重叠，1 = 允许完全重叠）
	std::vector<double> angleThreshold;
	double scoreThreshold; //只返回分数≥此值的匹配结果（建议0.5~0.8）
	std::vector<cv::Mat> templateMats;
	std::vector<cv::Mat> roiTemplates;
	std::vector<cv::Rect> templateRois;
	std::vector < std::vector<cv::KeyPoint>> templateMatsKeypoints; 
	std::vector<cv::Mat> templateMatsDescriptors;
	std::vector<HObject> labelAllTemplateHObjects;
	std::vector<HTuple> labelAllTemplateHTuple;
	int timeOut;
};


struct MatchLocateConfig {
	int matchType; //匹配方法
	std::string templatePath; //模板路径文件夹
	cv::Rect templatePose;
	cv::Rect targetRoi;
	int extW;
	int extH;
	int templateCenterX;//模板中心坐标
	int templateCenterY;
	int shiftHor;//模板中心偏移水平阈值
	int shiftVer;//模板中心偏移垂直阈值
	int offset;//模板中心偏移距离
	int channels;//1 黑白 3彩色 
	int numLevels; //值越大，搜索越快但精度越低（建议设为0，由系统自动计算）
	std::vector<double> angleRange;//角度范围
	double angleStep; //角度步长（单位：度）或设置为“auto”自动选择
	std::vector<double> scaleRange;
	double scaleStep;
	int optimization;
	int metric;
	int polarity;
	int contrast; //模板对比度阈值​： -数值（如30）：手动指定边缘梯度阈值 - "auto"：系统自动计算
	int minContrast; //最小对比度阈值​：搜索时忽略低于此阈值的边缘（噪声过滤，建议设为5 - 10）
	int subPixel; //像素精度
	double greediness;
	int numMatches; //- 0表示返回所有匹配 -通常设为1（找最佳匹配）或10（多目标）
	double maxOverlap; //最大重叠度​（0~1）： -抑制重叠检测结果（0 = 完全抑制重叠，1 = 允许完全重叠）
	std::vector<double> angleThreshold;
	double scoreThreshold; //只返回分数≥此值的匹配结果（建议0.5~0.8）
	std::vector<cv::Mat> templateMats;
	std::vector < std::vector<cv::KeyPoint>> templateMatsKeypoints;
	std::vector < std::vector<cv::Mat>> templateMatsDescriptors;
	std::vector<HObject> labelAllTemplateHObjects;
	std::vector<HTuple> labelAllTemplateHTuple;

	int timeOut;
};

struct MatchResult {
	int id = -1;
	double score = 0;
	double angle = 0;
	cv::Rect rect;
	cv::RotatedRect boundingRect;
	std::vector<cv::Point2f> corners;
	int valid = 0;  // 0:匹配失败  1：匹配成功 2：匹配得分过低 3：歪斜 4:错误特征 5:水平偏移 6:垂直偏移 7:距离偏移
	bool timeout = false;
	cv::Point2d center;
	double shiftHor;//模板中心偏移水平阈值
	double shiftVer;//模板中心偏移垂直阈值
	double offset;//模板中心偏移距离

	int adjustedRoiX; //对与出界宽的调整
	int adjustedRoiY;

	HalconCpp::HObject ho_Image, ho_TransContours;
	HalconCpp::HTuple hv_Row, hv_Column, hv_Angle, hv_Scale, hv_Score, hv_HomMat2D;

};




struct BarConfig {
	std::string checkType;//类型：一维码/二维码
	std::string barType;//种类
	std::vector<std::string> targetTypes;
	cv::Rect roi;
	std::string info;//信息内容
	std::vector<int> countRange;//一维码/二维码数量范围
	bool checkModel;//解析模式 0：快速 1：高精度 2：深度学习
	std::string modelPath;//深度学习模型路径
	HalconCpp::HTuple barCodeHandle;
	HalconCpp::HTuple qrCodeHandle;
};

struct BarResult {
	std::string barType;//一维码/二维码种类
	std::string infoResult;//信息内容
	cv::RotatedRect rect;//目标位置
	double barAngle;//旋转角度
	double detectScore = 0;//定位得分（深度学习）
	double analysisScore = 0;//解析得分
	bool state = false;//结果是否准确
};


struct CodeResult {
	int id = -1;
	std::string charName;
	double classfyScore = 0;
	double defectScore = 0;
};

struct BottleType {
	std::string capType;
	std::vector<int> capWidthRange;
	std::vector<int> capHeightRange;
	std::string handleType;
	std::vector<int> handleWidthRange;
	std::vector<int> handleHeightRange;
	std::string bottleType;
	int num;
};

struct BottleResult {
	std::string capType;
	cv::Rect capRect;
	double capScore = 0;
	std::string handleType;
	cv::Rect handleRect;
	double handleScore = 0;
	std::string bottleType;
	int num;
	bool isMatched = false;
};

struct YoloConfig {
	// 成员顺序必须与JSON键对应
	std::string className;
	cv::Rect roi;
	std::vector<int> widthRange;
	std::vector<int> heightRange;
	std::vector<int> countRange;
	std::vector<double> angleRange;
	std::vector<int> areaRange;
	std::vector<double> aspectRange;
	double maxOverLap;
	double confidenceThresh;
	bool use;

	// 添加默认构造函数
	YoloConfig() = default;
};

struct FinsObjects {
	std::vector<cv::Rect> boxes;
	std::vector<std::string> classNames;
	std::vector<float> confidences;
};

// 分类结果结构体
struct FinsClassification {
	std::string className;
	float confidence;
};

struct FinsObject {
	cv::Rect box;                   // 包围盒
	float confidence;               // 置信度
	std::string className;			// 类别名称
};

struct FinsObjectRotate {
	cv::RotatedRect box;            // 包围盒
	float confidence;               // 置信度
	std::string className;			// 类别名称
};

struct FinsObjectSeg {
	cv::Rect box;
	float confidence;
	std::string class_name;
	cv::Mat mask;  // 二值掩码
	cv::Mat proto_mask;  // 原型掩码（可选）
};

// 2D Code information
struct Code2D {
	std::string data;
	std::string symbology;
	cv::RotatedRect position;  // Using OpenCV's rotated rect

	explicit Code2D(const std::string& d = "", const std::string& t = "QR")
		: data(d), symbology(t) {}
};

struct BreakSize {
	cv::Rect rect;  // 包含x,y,width,height四个参数
	int size;       // 尺寸标注

	// 必须添加的构造函数
	BreakSize(int w, int h, int s); // 推荐:宽高+尺寸
	BreakSize(cv::Rect r, int s);   // 或完整矩形+尺寸
};

// Target parameters
struct DetectionCriteria {
	int target_type = -1;
	cv::Rect search_area;
	struct {
		int minimum;
		int maximum;
	} count, length, angle, area;

	float confidence_threshold = 0.5f;
};

struct matchHalconConfig {
	int type;
	HTuple AngleStart;
	HTuple AngleExtent;
	HTuple AngleStep;
	HTuple ScaleMin;
	HTuple ScaleMax;
	HTuple ScaleStep;
	HTuple Optimization;
	HTuple Metric;
	HTuple Contrast;
	HTuple NumMatches;
	HTuple MaxOverlap;
	HTuple SubPixel;
	HTuple NumLevels;
	HTuple MinContrast;
	HTuple MinScore;
	HTuple Greediness;
	int timeoutMs = 1000;
};

// 图像信息结构体
struct ProcessImage {
	std::shared_ptr<const cv::Mat> data;  // 共享图像数据
	std::string stageName;                // 处理阶段名称
	std::string description;               // 处理描述 
	cv::Rect focusArea;                    // 重点关注区域坐标
	double timestamp = 0.0;               // 处理时间戳(秒)

	// 缩略图生成方法（按需生成）
	cv::Mat getThumbnail(int width = 256) const {
		cv::Mat thumb;
		if (data && !data->empty()) {
			double ratio = width / (double)data->cols;
			cv::resize(*data, thumb, cv::Size(width, data->rows * ratio));
		}
		return thumb;
	}
	
	const cv::Mat& mat() const {
		if (!data || data->empty())
			throw std::runtime_error("Invalid image data");
		return *data;
	}

};

template<typename T>
std::string format_float(T value, int precision = 2) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(precision) << value;
	return oss.str();
}

extern std::vector<bool> loadNeckConfigSuccess;
extern std::vector<bool> loadLevelConfigSuccess;
extern std::vector<bool> loadHandleConfigSuccess;
extern std::vector<bool> loadCapConfigSuccess;
extern std::vector<bool> loadCodeConfigSuccess;
extern std::vector<bool> loadBottleNeckConfigSuccess;
extern std::vector<bool> loadBoxNumConfigSuccess;
extern std::vector<bool> loadSewConfigSuccess;
extern std::vector<bool> loadSewBottomConfigSuccess;
extern std::vector<bool> loadLabelAllConfigSuccess;
extern std::vector<bool> loadMoldingConfigSuccess;
extern std::vector<bool> loadBoxBagConfigSuccess;

class HeaderDefine {
public:
	HeaderDefine();


private:

};


#endif // PROJECT_HEADER_DEFINE_H