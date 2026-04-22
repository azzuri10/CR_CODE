#pragma once
#ifndef CALCFUN_H
#define CALCFUN_H
#include "HeaderDefine.h"

struct CALC_LinePara {
    float a;
    float b;
    float c;
    cv::Point2f pt0;
    cv::Point2f pt1;
    float angle;
    float dis;
};

class CalcFun {
public:
    CalcFun();
    ~CalcFun();

    // 计算角度（范围 0 - 180 度）
    float CALC_Angle180(float delta_x, float delta_y);
    // 计算两点之间的距离
    double CALC_DisPoints(const cv::Point2f& pt0, const cv::Point2f& pt1);
    // 计算点到直线的距离
    double CALC_DisPointToLine(const cv::Point2f& pt, const CALC_LinePara& line);
    // 计算图像的平均 HSV 值
    bool CALC_AvgHSV(const cv::Mat& img_hsv, int& avg_h, int& avg_s, int& avg_v);

    double CALC_GetAbsoluteAngle(const cv::RotatedRect& rect);
    // 计算图像的 HSV 范围
    bool CALC_HSV(const cv::Mat& img_hsv, int& max_h, int& max_s, int& max_v, int& min_h, int& min_s, int& min_v);
    // 根据点集拟合直线
    int CALC_FitLineByPts(const std::vector<cv::Point>& inPoints, float rate, float disThresh, CALC_LinePara& line, std::vector<cv::Point>& outPoints);
    //int CALC_FitLineByPts(std::vector<cv::Point> inPoints, float rate, float disThresh, CALC_LinePara& line, std::vector<cv::Point>& outPoints);
    // 根据点集和角度信息拟合直线
    int CALC_FitLineByPts(std::vector<cv::Point> inPoints, std::vector<float> inPointsAngle, float rate, float disThresh, float angleThresh, CALC_LinePara& line, std::vector<cv::Point>& outPoints);
    // 横向拟合直线
    CALC_LinePara  CALC_FitLineHor(std::vector<cv::Point>& ptList, float sx, float ex);
    // 纵向拟合直线
    CALC_LinePara  CALC_FitLineVer(std::vector<cv::Point>& ptList, float sy, float ey);
    // 计算直线参数
    int CALC_Line(const cv::Point2f& pt0, const cv::Point2f& pt1, CALC_LinePara& lineGen);
    // 计算直线角度
    int CALC_LineAngle(CALC_LinePara& line);
    // 计算两点构成直线的角度
    float CALC_LineAngle(const cv::Point& pt0, const cv::Point& pt1);

    // 计算图像区域的均值和标准差
    bool CALC_MeanStdDev(cv::Mat img, cv::Rect rect, double& meanVal, double& stdVal);
    // 计算点集的最小和最大坐标
    void CALC_PointsMinMaxXY(std::vector<cv::Point> points, int& minX, int& minY, int& maxX, int& maxY);
    // 获取直线上的所有点
    std::vector<cv::Point> CALC_PointsOnLine(cv::Vec4f line);

    cv::RotatedRect CALC_RotatedRect(double x, double y, double angle, double width, double height);
    // 计算矩形列表的中位数宽高
    bool CALC_RectsMedWh(std::vector<cv::Rect> rectList, int& w, int& h);
    bool CALC_RectsMedWh1(std::vector<FinsObject> rectList, int& w, int& h);
    // 根据方向去除边缘点
    bool CALC_RemoveEdgeByDir(cv::Mat& grayImg, cv::Mat& edgeImg, float angleThresh,
        std::vector<cv::Point>& pts);
    // 根据方向去除点
    bool CALC_RemovePointsByDir(const cv::Mat& grayImg, float angleThresh,
        std::vector<cv::Point>& pts);
    cv::RotatedRect rotateRotatedRect(const cv::RotatedRect& rect,
        const cv::Point2f& center,
        float angle);


    CALC_LinePara CALC_FitLineImpl(std::vector<cv::Point>& ptList, bool isVertical, float start, float end);

};

// 计算两个像素数组之间的距离
float calcuDistance(uchar* ptr, uchar* ptrCen, int cols);
// 最大最小距离函数（未实现）
cv::Mat MaxMinDisFun(cv::Mat data, float Theta, std::vector<int> centerIndex);

#endif // CALCFUN_H