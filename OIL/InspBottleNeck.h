#pragma once
#ifndef INSPBOTTLENECK_H
#define INSPBOTTLENECK_H

#include "InspBottleNeckStruct.h"
#include "AnalyseMat.h"
#include "Common.h"
#include "DrawAndShowImg.h"
#include "HeaderDefine.h"
#include "TxtOperater.h"
#include "LocationAndRecognition.h"
#include "Data.h"
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <chrono>
#include <future>
#include "write_json.h"
#include <map>
#include "LoadJsonConfig.h"

#define ASYNC_LOG(level, logFile, saveLog, format, ...) \
    AsyncWriteLog(level, logFile, saveLog, format, ##__VA_ARGS__)

class InspBottleNeck {
private:
    std::unique_ptr<Log> LOG;
    std::unique_ptr<Common> COM;
    std::unique_ptr<AnalyseMat> ANA;
    std::unique_ptr<TxtOperater> TXT;
    std::unique_ptr<DrawAndShowImg> DAS;
    std::unique_ptr<CalcFun> CAL;

    // 超时全局标志
    std::shared_ptr<std::atomic<bool>> m_safeTimeoutFlag;
    std::chrono::high_resolution_clock::time_point m_startTime;

public:
    InspBottleNeck(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
        bool isLoadConfig, int timeOut, InspBottleNeckOut& outInfo);
    ~InspBottleNeck();

    int BottleNeck_Main(InspBottleNeckOut& outInfo);
    std::future<int> RunInspectionAsync(InspBottleNeckOut& outInfo);

    // 操作超时标志
    void SetTimeoutFlagRef(std::atomic<bool>& flag) {
        m_timeoutFlagRef = &flag;
    }

    void SetStartTimePoint(std::chrono::high_resolution_clock::time_point startTime) {
        m_startTime = startTime;
    }

    // 统计相关函数
    struct AverageStats {
        int fittedCount = 0;
        double avgEllipticity = 0.0;
        double avgIrregularity = 0.0;
        double avgDiameter = 0.0;
    };

    AverageStats GetAverageStats() const;
    void ResetStatistics();

private:
    // 静态模型缓存
    static std::shared_mutex modelLoadMutex;
    static std::map<std::string, std::string> bottleNeckDetectionModelMap;
    static std::map<std::string, std::string> bottleNeckClassifyModelMap;
    static std::map<int, InspBottleNeckIn> cameraConfigMap;

    // 静态统计变量 - 所有实例共享
    static std::mutex m_statsMutex;                // 保护统计变量的互斥锁
    static int m_totalFittedCount;                 // 成功拟合的总次数
    static double m_sumEllipticity;                // 椭圆度总和
    static double m_sumIrregularity;               // 不规则度总和
    static double m_sumDiameter;                   // 直径总和
    static bool m_statsInitialized;                // 标记是否已初始化

    std::atomic<bool>* m_timeoutFlagRef = nullptr;
    cv::Mat m_img;
    InspBottleNeckIn m_params;

    // 路径
    std::string m_bottleNeckLogPath;
    std::string m_bottleNeckSaveImgPath;
    std::string m_bottleNeckSaveResOkPath;
    std::string m_bottleNeckSaveResNgPath;
    std::string m_bottleNeckConfigPath;
    std::string m_bottleNeckLogFile;
    std::string m_returnStr;

    // 超时检查
    bool CheckTimeout(int timeOut) const {
        if (m_safeTimeoutFlag && m_safeTimeoutFlag->load()) {
            return true;
        }

        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();
        return (elapsed > timeOut);
    }

    // 核心功能
    bool readParams(cv::Mat img, const std::string& filePath, InspBottleNeckIn& params,
        InspBottleNeckOut& outInfo, const std::string& fileName);
    bool validateCameraModels(int cameraId);
    bool loadAllModels(InspBottleNeckOut& outInfo, bool ini);

    void BottleNeck_Locate(InspBottleNeckOut& outInfo);
    void AnalyzeAndMarkDefects(InspBottleNeckOut& outInfo);
    void FitEllipseWithOutlierRemoval(InspBottleNeckOut& outInfo);
    double DistanceToEllipse(const cv::Point2f& pt, const cv::RotatedRect& ellipse);
    void BottleNeck_Classfy(InspBottleNeckOut& outInfo);
    void BottleNeck_Defect(InspBottleNeckOut& outInfo);
    void BottleNeck_DrawResult(InspBottleNeckOut& outInfo);
    void BottleNeck_ClearNet();

    // 统计函数
    void UpdateStatistics(InspBottleNeckOut& outInfo);
    void LogStatistics(InspBottleNeckOut& outInfo);

    // 初始化统计变量
    void InitializeStatistics(int jobId);
};

class ContinuousEdgeDetector {
private:
    cv::Point center_;
    int min_radius_;
    int max_radius_;
    int num_directions_;  // 扫描方向数

public:
    ContinuousEdgeDetector(cv::Point center, int min_radius, int max_radius)
        : center_(center), min_radius_(min_radius), max_radius_(max_radius) {
        num_directions_ = 2160;  // 使用360度每0.1667度扫描一次
    }

    // 查找连续边缘
    std::pair<std::vector<cv::Point>, std::vector<cv::Point>>
        findContinuousEdge(cv::Mat& canny_input, cv::Mat& edge_result, bool is_inner_edge) {
        std::vector<std::pair<int, cv::Point>> angle_points;
        performDenseRadialScan(canny_input, angle_points, is_inner_edge);

        std::vector<cv::Point> valid_points;
        std::vector<cv::Point> missing_points;

        double angle_step = 2 * CV_PI / num_directions_;

        for (auto& ap : angle_points) {
            int idx = ap.first;
            cv::Point p = ap.second;

            if (p.x >= 0 && p.y >= 0) {
                // 有效点
                valid_points.push_back(p);
            }
            else {
                // 缺失点：一个圆形轮廓的"缺"点
                double angle = idx * angle_step;
                int r = is_inner_edge ? min_radius_ : max_radius_;
                int x = center_.x + static_cast<int>(r * cos(angle));
                int y = center_.y + static_cast<int>(r * sin(angle));
                missing_points.push_back(cv::Point(x, y));
            }
        }

        // 有效点排序
        std::vector<cv::Point> ordered_points = sortPointsCircularly(valid_points);
        std::vector<cv::Point> continuous_contour = interpolateAndSmooth(ordered_points);

        return { continuous_contour, missing_points };
    }

private:
    // 高密度径向扫描（360度）
    void performDenseRadialScan(cv::Mat& canny_img,
        std::vector<std::pair<int, cv::Point>>& angle_points,
        bool is_inner) {
        double angle_step = 2 * CV_PI / num_directions_;

        for (int i = 0; i < num_directions_; i++) {
            double angle = i * angle_step;
            cv::Point p = scanRadialLine(canny_img, angle, is_inner);
            angle_points.push_back({ i, p }); // 有效点或(-1,-1)
        }
    }

    // 扫描径向线，只找一个点：内环 or 外环
    cv::Point scanRadialLine(cv::Mat& canny_img, double angle, bool is_inner) {
        double cos_a = cos(angle);
        double sin_a = sin(angle);

        int best_r = -1;
        cv::Point best_point(-1, -1);

        int start_r = min_radius_;
        int end_r = max_radius_;
        int step = 1;

        for (int r = start_r; r <= end_r; r += step) {
            int x = center_.x + static_cast<int>(r * cos_a);
            int y = center_.y + static_cast<int>(r * sin_a);

            if (x < 0 || x >= canny_img.cols || y < 0 || y >= canny_img.rows)
                continue;

            if (canny_img.at<uchar>(y, x) > 0) {
                if (is_inner) {
                    // 内环：找到最近的边缘点（一找到就返回）
                    return cv::Point(x, y);
                }
                else {
                    // 外环：找到最远的边缘点
                    best_r = r;
                    best_point = cv::Point(x, y);
                }
            }
        }

        return best_point; // 如果是外环则返回最远点，如果没找到则返回(-1,-1)
    }

    // 按极角排序
    std::vector<cv::Point> sortPointsCircularly(std::vector<cv::Point>& points) {
        std::vector<std::pair<double, cv::Point>> polar_points;

        for (auto& p : points) {
            double dx = p.x - center_.x;
            double dy = p.y - center_.y;
            double angle = atan2(dy, dx);  // 计算极角
            if (angle < 0) angle += 2 * CV_PI;  // 转换为0-2π范围

            polar_points.push_back({ angle, p });
        }

        // 按角度排序
        std::sort(polar_points.begin(), polar_points.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        std::vector<cv::Point> ordered;
        for (auto& pp : polar_points) {
            ordered.push_back(pp.second);
        }

        return ordered;
    }

    // 插值和平滑
    std::vector<cv::Point> interpolateAndSmooth(std::vector<cv::Point>& ordered_points) {
        if (ordered_points.size() < 3) return ordered_points;

        std::vector<cv::Point> smoothed;

        // 简单的滑动平均平滑
        int window = 3;
        for (size_t i = 0; i < ordered_points.size(); i++) {
            int sum_x = 0, sum_y = 0, count = 0;

            for (int j = -window; j <= window; j++) {
                int idx = (i + j + ordered_points.size()) % ordered_points.size();
                sum_x += ordered_points[idx].x;
                sum_y += ordered_points[idx].y;
                count++;
            }

            smoothed.push_back(cv::Point(sum_x / count, sum_y / count));
        }

        return smoothed;
    }

    // 绘制连续边缘
    void drawContinuousEdge(std::vector<cv::Point>& contour, cv::Mat& result) {
        result.setTo(0);

        if (contour.size() < 2) return;

        // 连接成轮廓
        for (size_t i = 0; i < contour.size(); i++) {
            size_t next_i = (i + 1) % contour.size();
            cv::line(result, contour[i], contour[next_i], cv::Scalar(255), 1);
        }

        // 绘制点，确保边缘可识别
        for (auto& p : contour) {
            cv::circle(result, p, 1, cv::Scalar(255), -1);
        }
    }
};

#endif // INSPBOTTLENECK_H