#pragma once
#ifndef BARANDQR_H
#define BARANDQR_H

#include "HeaderDefine.h"

class BarAndQR {
public:

    BarAndQR();
    ~BarAndQR();

    // 一维码检测接口（带模式参数）
    bool BAQ_CheckBar(cv::Mat img, BarConfig barConfig, std::vector<BarResult>& barResult);

    // 二维码检测接口（带模式参数）
    bool BAQ_CheckQR(cv::Mat img, BarConfig barConfig, std::vector<BarResult>& barResult);


private:
    // 图像预处理方法
    cv::Mat standardPreprocess(const cv::Mat& img);
    cv::Mat enhancedPreprocess(const cv::Mat& img);

    // 自适应参数设置
    void applyAdaptiveParameters(HalconCpp::HTuple barCodeHandle, const cv::Mat& img);

    // 几何处理工具
    static cv::RotatedRect CreateRotatedRect(double col, double row,
        double phiRad,
        double width, double height);

    // RAII资源管理
    template<typename CleanFunc>
    class HalconHandleGuard {
    public:
        HalconHandleGuard(HalconCpp::HTuple handle, CleanFunc cleanFunc)
            : handle_(handle), cleanFunc_(cleanFunc) {}
        ~HalconHandleGuard() { cleanFunc_(handle_); }
    private:
        HalconCpp::HTuple handle_;
        CleanFunc cleanFunc_;
    };

    // 结果处理
    bool ProcessSymbolRegions(
        HalconCpp::HObject& symbolRegions,
        HalconCpp::HTuple& codeStrings,
        bool isBarCode,
        std::vector<BarResult>& barResults,
        HalconCpp::HTuple handle,
        const std::string& currentType);

};

#endif // BARANDQR_H