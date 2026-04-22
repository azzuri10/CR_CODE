#include "CalcFun.h"
#include <opencv2/opencv.hpp> 
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>

// 定义常量
const int HIST_SIZE = 256;
const float HIST_RANGE[] = { 0, 256 };

constexpr int MIN_POINTS_THRESHOLD = 50;
constexpr int MIN_CONSECUTIVE_POINTS = 30;
constexpr float ANGLE_THRESHOLD_HOR = 45.0f;


CalcFun::CalcFun() {}

CalcFun::~CalcFun() {}

// 计算两点之间的角度
float CalcFun::CALC_Angle180(float delta_x, float delta_y) {
    float a = std::atan2(std::abs(delta_y), std::abs(delta_x));
    float angle = a * 180 / CV_PI;
    if (delta_y * delta_x < 0) {
        angle = 180 - angle;
    }
    return angle;
}

// 计算两点之间的距离
double CalcFun::CALC_DisPoints(const cv::Point2f& pt0, const cv::Point2f& pt1) {
    double dx = pt0.x - pt1.x;
    double dy = pt0.y - pt1.y;
    return std::sqrt(dx * dx + dy * dy);
}

// 计算点到直线的距离
double CalcFun::CALC_DisPointToLine(const cv::Point2f& pt, const CALC_LinePara& line)
{
    const double numerator = std::abs(line.a * pt.x + line.b * pt.y + line.c);
    const double denominator = std::hypot(line.a, line.b);

    if (denominator < std::numeric_limits<double>::epsilon()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return numerator / denominator;
}

// 计算 HSV 图像的平均 HSV 值
bool CalcFun::CALC_AvgHSV(const cv::Mat& img_hsv, int& avg_h, int& avg_s, int& avg_v) {
    if (img_hsv.empty()) {
        return false;
    }
    int cntAll = 0;
    int cntH = 0;
    int cntS = 0;
    int cntV = 0;
    for (int i = 0; i < img_hsv.rows; ++i) {
        for (int j = 0; j < img_hsv.cols; ++j) {
            const cv::Vec3b& pixel = img_hsv.at<cv::Vec3b>(i, j);
            cntH += pixel[0];
            cntS += pixel[1];
            cntV += pixel[2];
            cntAll++;
        }
    }
    avg_h = cntH / cntAll;
    avg_s = cntS / cntAll;
    avg_v = cntV / cntAll;
    return true;
}

double CalcFun::CALC_GetAbsoluteAngle(const cv::RotatedRect& rect) {
    cv::Point2f vertices[4];
    rect.points(vertices);

    // 计算相邻边长度
    double edge1 = norm(vertices[1] - vertices[0]);
    double edge2 = norm(vertices[2] - vertices[1]);

    // 选择较长边作为基准
    cv::Point2f pt1, pt2;
    if (edge1 > edge2) {
        pt1 = vertices[0];
        pt2 = vertices[1];
    }
    else {
        pt1 = vertices[1];
        pt2 = vertices[2];
    }

    // 计算线段角度（转换为0-180°）
    double angle = atan2(pt2.y - pt1.y, pt2.x - pt1.x) * 180 / CV_PI;
    return angle < 0 ? angle + 180 : angle;
}

// 计算 HSV 图像的最大和最小 HSV 值
bool CalcFun::CALC_HSV(const cv::Mat& img_hsv, int& max_h, int& max_s, int& max_v, int& min_h, int& min_s, int& min_v) {
    if (img_hsv.empty())  return false;

    // 通道分离 
    std::vector<cv::Mat> hsv_channels;
    cv::split(img_hsv, hsv_channels);

    // 直方图参数配置 
    int channels[] = { 0 };        // H通道 
    int histSize[] = { 180 };      // 180 bins 
    float h_range[] = { 0, 180 };  // H范围 
    const float* ranges[] = { h_range };

    // 计算各通道直方图 
    cv::Mat h_hist, s_hist, v_hist;

    // H通道直方图 
    cv::calcHist(hsv_channels.data(), 1, channels, cv::Mat(), h_hist, 1, histSize, ranges, true, false);

    // S通道直方图（调整参数）
    int s_channels[] = { 1 };
    float s_range[] = { 0, 256 };
    const float* s_ranges[] = { s_range };
    cv::calcHist(hsv_channels.data(), 1, s_channels, cv::Mat(), s_hist, 1, histSize, s_ranges, true, false);

    // V通道直方图（调整参数）
    int v_channels[] = { 2 };
    float v_range[] = { 0, 256 };
    const float* v_ranges[] = { v_range };
    cv::calcHist(hsv_channels.data(), 1, v_channels, cv::Mat(), v_hist, 1, histSize, v_ranges, true, false);

    if (h_hist.empty() || s_hist.empty() || v_hist.empty()) {
        std::cerr << "[ERROR] Histogram calculation failed" << std::endl;
        return false;
    }

    int minh = 255, maxh = 0, mins = 255, maxs = 0, minv = 255, maxv = 0;
    int minh1 = 255, maxh1 = 0;
    bool findRedLow = false;
    bool findRedUp = false;

    for (int i = 0; i < HIST_SIZE; ++i) {
        if (h_hist.at<float>(i) > 10) {
            if (i < 90) {
                maxh = std::max(maxh, i);
                minh = std::min(minh, i);
                findRedLow = true;
            }
            else {
                maxh1 = std::max(maxh1, i);
                minh1 = std::min(minh1, i);
                findRedUp = true;
            }
        }
    }

    for (int i = 0; i < HIST_SIZE; ++i) {
        if (s_hist.at<float>(i) > 10) {
            maxs = std::max(maxs, i);
            mins = std::min(mins, i);
        }
    }
    if (mins > maxs) {
        std::cout << "颜色模板图像选择不合理!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
    }

    for (int i = 0; i < HIST_SIZE; ++i) {
        if (v_hist.at<float>(i) > 10) {
            maxv = std::max(maxv, i);
            minv = std::min(minv, i);
        }
    }
    if (minv > maxv) {
        std::cout << "颜色模板图像选择不合理!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
    }

    int bndh = 10;
    int bnds = 60;
    int bndv = 120;
    if (findRedLow && findRedUp) {
        min_h = minh1 - 180 - bndh;
        max_h = std::min(maxh + bndh, 180);
    }
    else if (findRedLow && !findRedUp) {
        min_h = std::max(0, minh - bndh);
        max_h = maxh + bndh;
    }
    else if (!findRedLow && findRedUp) {
        min_h = minh1 - bndh;
        max_h = std::min(maxh1 + bndh, 180);
    }
    else {
        min_h = std::max(0, minh - bndh);
        max_h = std::min(maxh + bndh, 180);
    }

    min_s = std::max(0, mins - bnds);
    max_s = std::min(maxs + bnds, 255);

    min_v = std::max(0, minv - bndv);
    max_v = std::min(maxv + bndv, 255);

    return true;
}


// 计算两点之间的直线角度
float CalcFun::CALC_LineAngle(const cv::Point& pt0, const cv::Point& pt1) {
    float delta_x = pt1.x - pt0.x;
    float delta_y = pt1.y - pt0.y;
    delta_y = -delta_y;
    return CALC_Angle180(delta_x, delta_y);
}

// 计算直线的角度
int CalcFun::CALC_LineAngle(CALC_LinePara& line) {
    float delta_x = std::fabs(line.pt1.x - line.pt0.x);
    float delta_y = std::fabs(line.pt1.y - line.pt0.y);
    line.angle = std::atan2(delta_y, delta_x) * 180 / CV_PI;
    return 1;
}

int CalcFun::CALC_FitLineByPts(const std::vector<cv::Point>& inPoints, float rate,
    float disThresh, CALC_LinePara& line,
    std::vector<cv::Point>& outPoints)
{
    const size_t minPoints = 10;
    if (inPoints.size() < minPoints) return 0;

    // RANSAC参数
    constexpr int maxLoops = 300;
    const int checkNum = static_cast<int>(inPoints.size() * rate);
    float minAvg = FLT_MAX;
    CALC_LinePara bestLine;
    cv::RNG rng(cv::getTickCount());

    // 预分配内存
    std::vector<float> distances;
    distances.reserve(inPoints.size());

    // RANSAC主循环
    for (int k = 0; k < maxLoops; ++k) {
        // 随机采样改进:确保不同点
        int idx0 = rng.uniform(0, static_cast<int>(inPoints.size()));
        int idx1;
        do {
            idx1 = rng.uniform(0, static_cast<int>(inPoints.size()));
        } while (idx1 == idx0);

        const auto& pt0 = inPoints[idx0];
        const auto& pt1 = inPoints[idx1];

        // 计算直线参数
        CALC_LinePara curLine;
        CALC_Line(pt0, pt1, curLine);

        // 计算所有点到直线的距离
        distances.clear();
        for (const auto& pt : inPoints) {
            distances.push_back(CALC_DisPointToLine(pt, curLine));
        }

        // 部分排序代替完全排序
        auto middle = distances.begin() + checkNum;
        std::nth_element(distances.begin(), middle, distances.end());

        // 计算前checkNum个点的平均距离
        float avg = std::accumulate(distances.begin(), middle, 0.0f) / checkNum;

        if (avg < minAvg) {
            minAvg = avg;
            bestLine = curLine;
        }
    }

    // 筛选内点
    std::vector<cv::Point> innerPts;
    innerPts.reserve(inPoints.size());
    for (const auto& pt : inPoints) {
        if (CALC_DisPointToLine(pt, bestLine) <= disThresh) {
            innerPts.push_back(pt);
        }
    }

    // 连续性过滤优化
    std::vector<cv::Point> bestSegment;
    std::vector<cv::Point> currentSegment;
    for (size_t i = 1; i < innerPts.size(); ++i) {
        const cv::Point& prev = innerPts[i - 1];
        const cv::Point& curr = innerPts[i];

        if (cv::norm(curr - prev) < MIN_CONSECUTIVE_POINTS) {
            currentSegment.push_back(curr);
        }
        else {
            if (currentSegment.size() > bestSegment.size()) {
                bestSegment = std::move(currentSegment);
            }
            currentSegment.clear();
        }
    }
    if (!currentSegment.empty() && currentSegment.size() > bestSegment.size()) {
        bestSegment = std::move(currentSegment);
    }

    // 结果检查
    if (bestSegment.size() < MIN_POINTS_THRESHOLD) return 0;
    outPoints = std::move(bestSegment);

    // 区域统计优化:直接使用内点范围
    auto [minX, maxX] = std::minmax_element(outPoints.begin(), outPoints.end(),
        [](const cv::Point& a, const cv::Point& b) { return a.x < b.x; });
    auto [minY, maxY] = std::minmax_element(outPoints.begin(), outPoints.end(),
        [](const cv::Point& a, const cv::Point& b) { return a.y < b.y; });

    // 根据角度选择拟合方向
    const float absAngle = std::abs(bestLine.angle);
    if (absAngle < ANGLE_THRESHOLD_HOR || absAngle > 180.0f - ANGLE_THRESHOLD_HOR) {
        line = CALC_FitLineImpl(outPoints, false, (*minX).x, (*maxX).x);
    }
    else {
        line = CALC_FitLineImpl(outPoints, true, (*minY).y, (*maxY).y);
    }

    return 1;
}

CALC_LinePara CalcFun::CALC_FitLineImpl(std::vector<cv::Point>& ptList, bool isVertical, float start, float end)
{
    cv::Vec4f lineParams;
    cv::fitLine(ptList, lineParams, cv::DIST_FAIR, 0, 0.01, 0.01);

    const float vx = lineParams[0];
    const float vy = lineParams[1];
    const float x0 = lineParams[2];
    const float y0 = lineParams[3];

    cv::Point2f p1, p2;
    if (isVertical) {
        p1.y = start;
        p1.x = ((p1.y - y0) * vx + vy * x0) / vy;
        p2.y = end;
        p2.x = ((p2.y - y0) * vx + vy * x0) / vy;
    }
    else {
        p1.x = start;
        p1.y = ((p1.x - x0) * vy + vx * y0) / vx;
        p2.x = end;
        p2.y = ((p2.x - x0) * vy + vx * y0) / vx;
    }

    CALC_LinePara result;
    CALC_Line(p1, p2, result);
    return result;
}

// 优化后的辅助函数
int CalcFun::CALC_Line(const cv::Point2f& pt0, const cv::Point2f& pt1, CALC_LinePara& lineGen)
{
    lineGen.pt0 = pt0.y < pt1.y ? pt0 : pt1;
    lineGen.pt1 = pt0.y < pt1.y ? pt1 : pt0;

    lineGen.a = pt1.y - pt0.y;
    lineGen.b = pt0.x - pt1.x;
    lineGen.c = pt1.x * pt0.y - pt0.x * pt1.y;
    lineGen.dis = cv::norm(pt1 - pt0);

    // 修正角度计算
    const float delta_x = pt1.x - pt0.x;
    const float delta_y = pt1.y - pt0.y;
    lineGen.angle = std::atan2(delta_y, delta_x) * 180.0f / CV_PI;

    return 1;
}


//int CalcFun::CALC_FitLineByPts(std::vector<cv::Point> inPoints, float rate, float disThresh, CALC_LinePara& line, std::vector<cv::Point>& outPoints) {
//
//    if (inPoints.size() < 10) {
//        return 0;
//    }
//
//    /**************** param ****************/
//    float checkRate = rate;
//    int maxLoops = 300; // 最大迭代次数
//    int checkNum = inPoints.size() * checkRate; // RANSAC中采样的点数
//    float min_avg = 1000; // 初始的最小平均距离
//    float dis_thresh = disThresh; // 距离阈值
//    int bestLoop = 0; // 记录最佳循环次数
//    CALC_LinePara best_line; // 最优拟合线
//    cv::RNG rng; // 随机数生成器
//
//    // RANSAC迭代拟合直线
//    for (int k = 0; k < maxLoops; k++) {
//        std::vector<cv::Point> curInnerPts;
//        int idx0 = rng.uniform(0, inPoints.size() - 1);
//        int idx1 = rng.uniform(0, inPoints.size() - 1);
//
//        if (idx0 == idx1) {
//            continue; // 跳过相同点
//        }
//
//        cv::Point2f pt0 = inPoints[idx0];
//        cv::Point2f pt1 = inPoints[idx1];
//        CALC_LinePara curLine;
//        CALC_Line(pt0, pt1, curLine); // 拟合当前线
//        CALC_LineAngle(curLine); // 计算角度
//
//        std::vector<float> distanceListAll;
//        for (int i = 0; i < inPoints.size(); i++) {
//            cv::Point2f ptTmp = inPoints[i];
//            float dis = CALC_DisPointToLine(ptTmp, curLine); // 计算点到直线的距离
//            distanceListAll.push_back(dis);
//        }
//
//        // 对距离进行排序，找出最小的点集
//        sort(distanceListAll.begin(), distanceListAll.end());
//        float avg = 0;
//        for (int i = 0; i < checkNum; i++) {
//            avg += distanceListAll[i];
//        }
//        avg = avg / checkNum;
//
//        if (avg < min_avg) {
//            min_avg = avg;
//            best_line = curLine;
//            bestLoop = k;
//        }
//    }
//
//    /***************** refine by inner points !**********************/
//    std::vector<cv::Point>  innerPts;
//    for (int i = 0; i < inPoints.size(); i++) {
//        float dis = CALC_DisPointToLine(cv::Point2f(inPoints[i]), best_line);
//
//        // 去除离散点
//        if (dis <= disThresh) {
//            innerPts.push_back(inPoints[i]);
//        }
//    }
//
//    /***************** 保留连续点数大于 100 并且两点最小间隔小于 10 的点 **********************/
//    std::vector<cv::Point>  filteredPts;
//    std::vector<cv::Point>  tempPts;
//    std::vector<cv::Point>  bestPts; //保留点数最多的
//
//    for (int i = 1; i < innerPts.size(); i++) {
//        float dist_x = abs(innerPts[i].x - innerPts[i - 1].x);
//        float dist_y = abs(innerPts[i].y - innerPts[i - 1].y);// 计算相邻点的距离
//
//        if (dist_x < 30 && dist_y < 30) {
//            tempPts.push_back(innerPts[i]);
//        }
//        else {
//            // 判断当前连续点集是否大于 100
//            if (tempPts.size() > 100) {
//                filteredPts.insert(filteredPts.end(), tempPts.begin(), tempPts.end());
//                if (bestPts.size() < filteredPts.size())
//                {
//                    bestPts = filteredPts;
//                }
//            }
//            tempPts.clear(); // 清空临时存储的连续点
//            filteredPts.clear();
//        }
//    }
//
//    // 最后检查一次 tempPts 是否符合条件
//    if (bestPts.size() < tempPts.size())
//    {
//        bestPts = tempPts;
//    }
//    outPoints = bestPts; // 将符合条件的点集传递出去
//    if (outPoints.size() < 50)
//    {
//        return 0;
//    }
//
//    /***************** only for show !**********************/
//    int max_x = 0;
//    int min_x = 10000;
//    int max_y = 0;
//    int min_y = 10000;
//
//    for (int i = 0; i < inPoints.size(); i++) {
//        max_x = MAX(max_x, inPoints[i].x);
//        max_y = MAX(max_y, inPoints[i].y);
//        min_x = MIN(min_x, inPoints[i].x);
//        min_y = MIN(min_y, inPoints[i].y);
//    }
//
//    // 根据角度判断横向或纵向拟合
//    if (abs(best_line.angle) < 45 || abs(best_line.angle) > 135) {
//        line = CALC_FitLineHor(outPoints, min_x, max_x); // 横向拟合
//        CALC_LineAngle(line);
//    }
//    else {
//        line = CALC_FitLineVer(outPoints, min_y, max_y); // 纵向拟合
//        CALC_LineAngle(line);
//    }
//
//    return 1;
//}



int CalcFun::CALC_FitLineByPts(std::vector<cv::Point>  inPoints, std::vector<float> inPointsAngle, float rate, float disThresh, float angleThresh, CALC_LinePara& line, std::vector<cv::Point>& outPoints) {
    if (inPoints.size() < 10) {
        return 0;
    }
    /**************** param ****************/
    float checkRate = rate;
    int maxLoops = 300;
    int checkNum = inPoints.size() * checkRate;
    float min_avg = 1000;
    float minAngle_avg = 1000;
    float dis_thresh = 0;
    int bestLoop = 0;
    CALC_LinePara best_line;
    cv::RNG rng;
    for (int k = 0; k < maxLoops; k++) {
        std::vector<cv::Point> curInnerPts;
        int idx0 = rng.uniform(0, inPoints.size() - 1);
        int idx1 = rng.uniform(0, inPoints.size() - 1);
        if (idx0 == idx1) {
            continue;
        }
        cv::Point2f pt0 = inPoints[idx0];
        cv::Point2f pt1 = inPoints[idx1];
        CALC_LinePara curLine;
        CALC_Line(pt0, pt1, curLine);
        CALC_LineAngle(curLine);
        if (curLine.angle > angleThresh) {
            continue;
        }
        std::vector<float> distanceListAll;
        std::vector<float> angleListAll;
        for (int i = 0; i < inPoints.size(); i++) {
            cv::Point2f ptTmp = inPoints[i];
            float dis = CALC_DisPointToLine(ptTmp, curLine);
            float angleErr = fabs(inPointsAngle[i] - curLine.angle);
            distanceListAll.push_back(dis);
            angleListAll.push_back(angleErr);
        }
        sort(distanceListAll.begin(), distanceListAll.end());
        float avgDis = 0;
        float avgAngle = 0;
        for (int i = 0; i < checkNum; i++) {
            avgDis += distanceListAll[i];
            avgAngle += angleListAll[i];
        }
        avgDis = avgDis / checkNum;
        avgAngle = avgAngle / checkNum;
        if (avgDis < min_avg && avgAngle < minAngle_avg) {
            min_avg = avgDis;
            minAngle_avg = avgAngle;
            best_line = curLine;
            bestLoop = k;
        }
    }
    /***************** refine by inner points !**********************/
    std::vector<cv::Point> innerPts;
    for (int i = 0; i < inPoints.size(); i++) {
        float dis = CALC_DisPointToLine(cv::Point2f(inPoints[i]), best_line);
        float angleErr = fabs(inPointsAngle[i] - best_line.angle);
        if (dis <= disThresh && angleErr <= angleThresh) {
            innerPts.push_back(inPoints[i]);
        }
    }
    outPoints = innerPts;
    /***************** only for show !**********************/
    int max_x = 0;
    int min_x = 10000;
    int max_y = 0;
    int min_y = 10000;
    for (int i = 0; i < inPoints.size(); i++) {
        max_x = MAX(max_x, inPoints[i].x);
        max_y = MAX(max_y, inPoints[i].y);
        min_x = MIN(min_x, inPoints[i].x);
        min_y = MIN(min_y, inPoints[i].y);
    }
    if (abs(best_line.angle) < 45 || abs(best_line.angle) > 135) {
        line = CALC_FitLineHor(innerPts, min_x, max_x);
        CALC_LineAngle(line);
    }
    else {
        line = CALC_FitLineVer(innerPts, min_y, max_y);
        CALC_LineAngle(line);
    }
    return 1;
}

CALC_LinePara CalcFun::CALC_FitLineVer(std::vector<cv::Point>& ptList, float sy, float ey) {
    cv::Vec4f line;
    fitLine(ptList, line, cv::DIST_FAIR, 0, 0.01, 0.01);
    float x0 = line[2];
    float y0 = line[3];
    float y1 = sy;
    float x1 = ((y1 - y0) * line[0] + line[1] * x0) / line[1];
    float y2 = ey;
    float x2 = ((y2 - y0) * line[0] + line[1] * x0) / line[1];
    CALC_LinePara line_;
    CALC_Line(cv::Point2f(x1, y1), cv::Point2f(x2, y2), line_);
    return line_;
}

CALC_LinePara CalcFun::CALC_FitLineHor(std::vector<cv::Point>& ptList, float sx,
    float ex) {
    cv::Vec4f line;
    fitLine(ptList, line, cv::DIST_FAIR, 0, 0.01, 0.01);
    float x0 = line[2];
    float y0 = line[3];
    float x1 = sx;
    float y1 = ((x1 - x0) * line[1] + line[0] * y0) / line[0];
    float x2 = ex;
    float y2 = ((x2 - x0) * line[1] + line[0] * y0) / line[0];
    CALC_LinePara line_;
    CALC_Line(cv::Point2f(x1, y1), cv::Point2f(x2, y2), line_);
    return line_;
}

bool CalcFun::CALC_MeanStdDev(cv::Mat img, cv::Rect rect, double& meanVal,
    double& stdVal) {
    if (img.empty()) {
        return false;
    }
    cv::Mat imgRoi = img(rect).clone();
    cv::Mat mean;
    cv::Mat stdDev;
    meanStdDev(imgRoi, mean, stdDev);
    meanVal = mean.ptr<double>(0)[0];
    stdVal = stdDev.ptr<double>(0)[0];
    return true;
}


void CalcFun::CALC_PointsMinMaxXY(std::vector<cv::Point> points, int& minX, int& minY, int& maxX, int& maxY)
{
    for (const auto& p : points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }
}
std::vector<cv::Point> CalcFun::CALC_PointsOnLine(cv::Vec4f line) {
    std::vector<cv::Point> allPoints;

    cv::Point2f pt1(line[0], line[1]);
    cv::Point2f pt2(line[2], line[3]);

    float dist = norm(pt2 - pt1);
    int numPoints = static_cast<int>(dist);

    for (int i = 0; i <= numPoints; ++i) {
        float t = static_cast<float>(i) / numPoints;
        cv::Point2f pt = (1 - t) * pt1 + t * pt2;
        allPoints.push_back(cv::Point(cvRound(pt.x), cvRound(pt.y)));
    }

    return allPoints;
}

cv::RotatedRect CalcFun::CALC_RotatedRect(double x, double y, double angle, double width, double height) {
    return cv::RotatedRect(
        cv::Point2f(static_cast<float>(x), static_cast<float>(y)),
        cv::Size2f(static_cast<float>(width), static_cast<float>(height)),
        static_cast<float>(angle)
    );
}

bool CalcFun::CALC_RectsMedWh(std::vector<cv::Rect> rectList, int& w, int& h) {
    if (rectList.empty()) {
        w = 0;
        h = 0;
        return false;
    }
    int rectListSize = rectList.size();
    std::vector<float> height_list;
    height_list.reserve(rectListSize);
    std::vector<float> width_list;
    width_list.reserve(rectListSize);
    for (int i = 0; i < rectListSize; i++) {
        height_list.push_back(rectList[i].height);
        width_list.push_back(rectList[i].width);
    }
    sort(height_list.begin(), height_list.end());
    sort(width_list.begin(), width_list.end());
    h = height_list[height_list.size() / 2];
    w = width_list[width_list.size() / 2];
    height_list.clear();
    width_list.clear();
    return true;
}

bool CalcFun::CALC_RectsMedWh1(std::vector<FinsObject> rectList, int& w, int& h) {
    if (rectList.empty()) {
        w = 0;
        h = 0;
        return false;
    }
    int rectListSize = rectList.size();
    std::vector<float> height_list;
    height_list.reserve(rectListSize);
    std::vector<float> width_list;
    width_list.reserve(rectListSize);
    for (int i = 0; i < rectListSize; i++) {
        height_list.push_back(rectList[i].box.height);
        width_list.push_back(rectList[i].box.width);
    }
    sort(height_list.begin(), height_list.end());
    sort(width_list.begin(), width_list.end());
    h = height_list[height_list.size() / 2];
    w = width_list[width_list.size() / 2];
    height_list.clear();
    width_list.clear();
    return true;
}

bool CalcFun::CALC_RemoveEdgeByDir(cv::Mat& grayImg, cv::Mat& edgeImg,
    float angleThresh, std::vector<cv::Point>& pts) {
    if (grayImg.empty() || edgeImg.empty()) {
        return false;
    }
    pts.clear();
    std::vector<cv::Point> curPtList;
    for (int j = 1; j < edgeImg.cols - 1; j++) {
        for (int i = 1; i < edgeImg.rows - 1; i++) {
            if (edgeImg.at<uchar>(i, j) > 0) {
                curPtList.push_back(cv::Point(j, i));
                break;
            }
        }
    }
    std::vector<float> angleList;
    for (int i = 0; i < curPtList.size(); i++) {
        int jj = curPtList[i].x;
        int ii = curPtList[i].y;
        float delta_x =
            grayImg.at<uchar>(ii - 1, jj + 1) + grayImg.at<uchar>(ii, jj + 1) * 2 +
            grayImg.at<uchar>(ii + 1, jj + 1) - grayImg.at<uchar>(ii - 1, jj - 1) -
            grayImg.at<uchar>(ii, jj - 1) * 2 - grayImg.at<uchar>(ii + 1, jj - 1);
        float delta_y =
            grayImg.at<uchar>(ii + 1, jj - 1) + grayImg.at<uchar>(ii + 1, jj) * 2 +
            grayImg.at<uchar>(ii + 1, jj + 1) - grayImg.at<uchar>(ii - 1, jj - 1) -
            grayImg.at<uchar>(ii - 1, jj) * 2 - grayImg.at<uchar>(ii - 1, jj + 1);
        float angle = CALC_Angle180(delta_x, delta_y);
        angleList.push_back(angle);
    }
    for (int i = 0; i < angleList.size(); i++) {
        if (abs(angleList[i] - 90) < angleThresh) {
            pts.push_back(curPtList[i]);
        }
    }
    return true;
}


bool CalcFun::CALC_RemovePointsByDir(const cv::Mat& grayImg, float angleThresh,
    std::vector<cv::Point>& pts) {
    if (grayImg.empty() || pts.empty()) {
        return false;
    }

    const int rows = grayImg.rows;
    const int cols = grayImg.cols;
    std::vector<cv::Point> filteredPts;
    filteredPts.reserve(pts.size()); // 预分配内存减少动态扩展

    for (const auto& pt : pts) {
        const int x = pt.x;
        const int y = pt.y;

        // 正确边界检查:确保3x3邻域存在
        if (x <= 0 || x >= cols - 1 || y <= 0 || y >= rows - 1)
            continue;

        // 快速行指针访问
        const uchar* prev = grayImg.ptr<uchar>(y - 1);
        const uchar* curr = grayImg.ptr<uchar>(y);
        const uchar* next = grayImg.ptr<uchar>(y + 1);

        // 改进的梯度计算（Sobel优化）
        const int dx = (prev[x + 1] + 2 * curr[x + 1] + next[x + 1])
            - (prev[x - 1] + 2 * curr[x - 1] + next[x - 1]);
        const int dy = (next[x - 1] + 2 * next[x] + next[x + 1])
            - (prev[x - 1] + 2 * prev[x] + prev[x + 1]);

        // 直接计算角度差，避免浮点运算
        if (dx == 0 && dy == 0) continue; // 零梯度处理

        const float angle = std::atan2(dy, dx) * 180.0f / CV_PI;
        if (std::abs(std::fmod(angle + 360.0f, 180.0f) - 90.0f) < angleThresh) {
            filteredPts.push_back(pt);
        }
    }
    pts = std::move(filteredPts); // 移动语义转移数据
    return true;
}
//bool CalcFun::CALC_RemovePointsByDir(cv::Mat& grayImg, float angleThresh,
//    std::vector<cv::Point>& pts) {
//    if (grayImg.empty() || pts.empty()) {
//        return false;
//    }
//    std::vector<cv::Point> pointsTmp;
//    std::vector<float> angleList;
//    for (int i = 0; i < pts.size(); i++) {
//        int jj = pts[i].x;
//        int ii = pts[i].y;
//
//        if (ii == 0 || ii == grayImg.cols - 1 || jj == 0 || jj == grayImg.rows - 1)
//        {
//            continue;
//        }
//        float delta_x =
//            grayImg.at<uchar>(ii - 1, jj + 1) + grayImg.at<uchar>(ii, jj + 1) * 2 +
//            grayImg.at<uchar>(ii + 1, jj + 1) - grayImg.at<uchar>(ii - 1, jj - 1) -
//            grayImg.at<uchar>(ii, jj - 1) * 2 - grayImg.at<uchar>(ii + 1, jj - 1);
//        float delta_y =
//            grayImg.at<uchar>(ii + 1, jj - 1) + grayImg.at<uchar>(ii + 1, jj) * 2 +
//            grayImg.at<uchar>(ii + 1, jj + 1) - grayImg.at<uchar>(ii - 1, jj - 1) -
//            grayImg.at<uchar>(ii - 1, jj) * 2 - grayImg.at<uchar>(ii - 1, jj + 1);
//        float angle = CALC_Angle180(delta_x, delta_y);
//        angleList.push_back(angle);
//    }
//    for (int i = 0; i < angleList.size(); i++) {
//        if (abs(angleList[i] - 90) < angleThresh) {
//            pointsTmp.push_back(pts[i]);
//        }
//    }
//    pts = pointsTmp;
//    return true;
//}

float calcuDistance(uchar* ptr, uchar* ptrCen, int cols) {
    float d = 0.0;
    for (size_t j = 0; j < cols; j++) {
        d += (double)(ptr[j] - ptrCen[j]) * (ptr[j] - ptrCen[j]);
    }
    d = sqrt(d);
    return d;
}

cv::RotatedRect CalcFun::rotateRotatedRect(const cv::RotatedRect& rect,
    const cv::Point2f& center,
    float angle)
{
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, angle, 1.0);
    cv::Mat centerMat = (cv::Mat_<double>(3, 1) << rect.center.x, rect.center.y, 1);
    cv::Mat transformedCenterMat = rotationMatrix * centerMat;
    cv::Point2f newCenter(transformedCenterMat.at<double>(0), transformedCenterMat.at<double>(1));

    // 计算新角度（考虑角度范围）
    float newAngle = rect.angle + angle;
    if (newAngle < 0) newAngle += 360;
    if (newAngle >= 360) newAngle -= 360;

    // 创建新的旋转矩形
    return cv::RotatedRect(newCenter, rect.size, newAngle);
}