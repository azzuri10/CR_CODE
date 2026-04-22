#ifndef DRAWANDSHOWIMG
#define DRAWANDSHOWIMG

#include "AnalyseMat.h"
#include "CalcFun.h"
#include "HeaderDefine.h"
#include "PutTextZH.h"

namespace Colors {
    const cv::Scalar RED(0, 0, 255); 
    const cv::Scalar GREEN(0, 255, 0);
    const cv::Scalar BLUE(255, 0, 0);
    const cv::Scalar LIGHT_BLUE(255, 255, 0);
    const cv::Scalar PURPLE(255, 0, 255);
    const cv::Scalar YELLOW(0, 255, 255);
    const cv::Scalar GOLDEN(32, 165, 218);
    const cv::Scalar ORANGE(0, 165, 255);
    const cv::Scalar WHITE(255, 255, 255);
    const cv::Scalar BLACK(0, 0, 0);
}

const std::vector<cv::Scalar> TEMPLATE_COLORS = {
    cv::Scalar(0, 100, 255),     // 亮橙色 (最醒目的警示色)
    cv::Scalar(0, 255, 255),     // 纯青色 (高对比度)
    cv::Scalar(255, 0, 150),     // 亮粉色 (极易识别)
    cv::Scalar(0, 215, 255),     // 金黄 (类似交通标志黄)
    cv::Scalar(0, 180, 255),     // 安全橙 (类似安全背心)
    cv::Scalar(255, 255, 0),     // 纯蓝 (深空蓝)
    cv::Scalar(0, 255, 150),     // 霓虹绿 (夜光效果)

    // 以下是额外提供的5种专业级高饱和度颜色
    cv::Scalar(0, 128, 255),     // 珊瑚橙 (Web安全色)
    cv::Scalar(255, 0, 100),     // 玫瑰红 (女性化高亮)
    cv::Scalar(0, 255, 200),     // 蓝绿色 (类似泳池水色)
    cv::Scalar(255, 100, 0),      // 天蓝 (冷色调高亮)
    cv::Scalar(200, 0, 255),     // 电光紫 (极高对比度)

    // 中等饱和度色系
    cv::Scalar(0, 100, 0),     // 深绿
    cv::Scalar(70, 130, 180),   // 钢蓝色
    cv::Scalar(128, 0, 128),     // 亮紫色 

    // 柔和色系 (适合背景)
    cv::Scalar(173, 216, 230), // 浅蓝色
    cv::Scalar(144, 238, 144), // 浅绿色
    cv::Scalar(255, 182, 193), // 粉红色
    cv::Scalar(255, 215, 0),   // 金色

    // 深色调 (适合前景)
    cv::Scalar(139, 0, 0),     // 深红色
    cv::Scalar(0, 0, 139),     // 深蓝色
    cv::Scalar(85, 107, 47),   // 深橄榄绿

    // 特殊色系
    cv::Scalar(255, 140, 0),   // 深橙色
    cv::Scalar(148, 0, 211),   // 紫罗兰色
    cv::Scalar(46, 139, 87)    // 海绿色
};

class DrawAndShowImg {
public:
    DrawAndShowImg();
    ~DrawAndShowImg();

    // 基础绘图模板
    template<typename... Args>
    static bool DAS_Rect(cv::Mat img, bool isSaveImg, std::string fileName, Args&&... rects) {
        if (!isSaveImg || img.empty())
            return isSaveImg;  // 快速返回逻辑优化

        cv::Mat imgCopy = ProcessImageAna(img);
        DrawRects(imgCopy, std::forward<Args>(rects)...);
        return cv::imwrite(fileName, imgCopy);
    }

    template<typename T>
    bool drawElements(const cv::Mat& img, const std::vector<T>& elements,
        const std::string& filename, bool saveFlag = true,
        const cv::Scalar& color = cv::Scalar(0, 255, 0),
        int thickness = 1);

    // 专用绘图方法
    bool DAS_HoughLinesP(const cv::Mat& img, const std::vector<cv::Vec4i>& lines,
        const std::string& filename, bool saveFlag = true);

    bool DAS_BreakArea(cv::Mat img, BreakSize breakAreas, int bnd, std::string fileName, bool isSaveImg);
    bool DAS_BreakAreas(cv::Mat img, std::vector<BreakSize> breakAreas, int bnd, std::string fileName, bool isSaveImg);
    bool DAS_BreakAreasPoints(cv::Mat img, std::vector <int> points, std::vector<BreakSize> breakAreas,
        int bnd, std::string fileName, bool isSaveImg);
    bool DAS_Counters(cv::Mat img, std::vector<std::vector<cv::Point>> contours, std::string fileName,
        bool isSaveImg);
    bool DAS_DynamicLines(cv::Mat img, std::vector<int> leftList, std::vector<int> rightList,
        std::string fileName, bool isSaveImg);
    bool DAS_Circle(cv::Mat img, cv::Vec3f drawCircle, std::string fileName, bool isSaveImg);
    bool DAS_Circles(cv::Mat img, std::vector<cv::Vec3f> drawCircle, std::string fileName,
        bool isSaveImg);
    bool DAS_Circles(cv::Mat img, std::vector<cv::Vec4f> drawCircles, std::string fileName,
        bool isSaveImg);
    bool DAS_CirclesScore(cv::Mat img, std::vector<cv::Vec4f> drawCircles,
        std::vector<float> scores, std::string fileName, bool isSaveImg);
    bool DAS_HoughLineP(cv::Mat img, cv::Vec4i linesP, std::string fileName, bool isSaveImg);
    bool DAS_HoughLinesP(cv::Mat img, std::vector<cv::Vec4i> linesP, std::string fileName, bool isSaveImg);
    bool DAS_HoughLinesP0(cv::Mat img, cv::Vec4f linesP, std::string fileName, bool isSaveImg);
    bool DAS_HoughLinesP(cv::Mat img, std::vector<cv::Vec4f> linesP, std::string fileName, bool isSaveImg);
    bool DAS_Img(cv::Mat img, std::string fileName, bool isSaveImg);
    bool DAS_Line(cv::Mat img, CALC_LinePara line, std::string fileName, bool isSaveImg);
    bool DAS_Line(cv::Mat img, cv::Point pt0, cv::Point pt1, std::string fileName, bool isSaveImg);
    bool DAS_Lines(cv::Mat img, std::vector<CALC_LinePara> lines, std::string fileName, bool isSaveImg);
    bool DAS_Lines(cv::Mat img, std::vector<cv::Vec2f> lines, std::string fileName, bool isSaveImg);
    bool DAS_Match(cv::Mat img0, std::vector<cv::KeyPoint> point0, cv::Mat img1,  std::vector<cv::KeyPoint> point1, std::vector<cv::DMatch> matches, std::string fileName, bool isSaveImg);
    bool DAS_MatchHalcon(cv::Mat img, std::vector<cv::Point2f> dstCorners, double score, double angle, std::string fileName, bool isSaveImg);
    bool DAS_Point(cv::Mat img, cv::Point point, std::string fileName, bool isSaveImg);
    bool DAS_Points(cv::Mat img, std::vector<cv::Point> points, std::string fileName,
        bool isSaveImg);
    bool DAS_Points(cv::Mat img, std::vector<std::vector<cv::Point>> points, std::string fileName,
        bool isSaveImg);
    bool DAS_PointsState(cv::Mat img, std::vector<cv::Point> points, std::vector<bool> states,
        std::string fileName, bool isSaveImg);
    bool DAS_PosAndScore(cv::Mat img, std::vector<int> posList, std::vector<double> grayDif,
        std::string fileName, bool isSaveImg);
    bool DAS_ProjectHor(std::vector<int> valList, std::string fileName, bool isSaveImg);
    bool DAS_ProjectHor(std::vector<float> valList, std::string fileName, bool isSaveImg);
    bool DAS_ProjectVer(std::vector<int> valList, std::string fileName, bool isSaveImg);
    bool DAS_ProjectVer(std::vector<float> valList, std::string fileName, bool isSaveImg);



	bool DAS_Rects(cv::Mat img, std::vector<cv::Rect> rects, std::string fileName, bool isSaveImg);
    bool DAS_RectsScore(cv::Mat img, std::vector<cv::Rect> rects, std::vector<float> scores, std::string fileName, bool isSaveImg);
    bool DAS_RectsScoreType(cv::Mat img, std::vector<cv::Rect> rects, std::vector<float> scores, std::vector<std::string> types, std::string fileName, bool isSaveImg);
    bool DAS_RectsScoreType(cv::Mat img, std::vector < std::vector<cv::Rect>> rects, std::vector < std::vector<float>> scores, std::vector < std::vector<std::string>> types, std::string fileName, bool isSaveImg);
    bool DAS_RectsState(cv::Mat img, std::vector<cv::Rect> rects, std::vector<bool> states,
        std::string fileName, bool isSaveImg);
    bool DAS_RotateRect(cv::Mat img, cv::RotatedRect box, std::string fileName,
        bool isSaveImg);
    bool DAS_RotateRects(cv::Mat img, std::vector<cv::RotatedRect> box, std::string fileName,
        bool isSaveImg);

    //NEW
    bool DAS_String(
        cv::Mat img, 
        std::string info, 
        std::string fileName,
        bool isSaveImg);

    bool DAS_Rect(cv::Mat img,
        const cv::Rect& rect,
        const std::string& fileName,
        bool isSaveImg);

    bool DAS_FinsObject(const cv::Mat& img,
        const std::vector<FinsObject>& details,
        const std::string& fileName,
        bool isSaveImg);
    bool DAS_FinsObjectObb(const cv::Mat& img,
        const std::vector<FinsObjectRotate>& details,
        const std::string& fileName,
        bool isSaveImg);


private:
    AnalyseMat* ANA;

    static cv::Mat ProcessImageAna(cv::Mat& src) {
        cv::Mat dst;
        if (src.channels() == 3) {
            src.copyTo(dst);
        }
        else {
            cv::cvtColor(src, dst, cv::COLOR_GRAY2BGR);
        }
        return dst;
    }
    // 递归绘制模板
    static void DrawRects(cv::Mat& img) {}  // 基例

    template<typename T, typename... Args>
    static void DrawRects(cv::Mat& img, T&& rect, Args&&... args) {
        cv::rectangle(img, std::forward<T>(rect), Colors::GREEN, 1);
        DrawRects(img, std::forward<Args>(args)...);  // 递归展开参数包
    }
};

#endif  // DRAWANDSHOWIMG
#pragma once
