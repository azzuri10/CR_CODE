#pragma once
#pragma once
#ifndef INSPMOLDING_H
#define INSPMOLDING_H

#include "InspMoldingStruct.h"
#include "InferenceWorker.h"
#include "Log.h"
#include "AnalyseMat.h"
#include "CalcFun.h"
#include "Common.h"
#include "TxtOperater.h"
#include "DrawAndShowImg.h"
#include "MatchFun.h"
#include <mutex>


class InspMolding {
private:
    std::unique_ptr<Log> LOG;
    std::unique_ptr<Common> COM;
    std::unique_ptr<AnalyseMat> ANA;
    std::unique_ptr<TxtOperater> TXT;
    std::unique_ptr<DrawAndShowImg> DAS;
    std::unique_ptr<CalcFun> CAL;
    std::unique_ptr<MatchFun> MF;

public:
    InspMolding(std::string configPath, const cv::Mat& img, int cameraId, int jobId, bool isLoadConfig, InspMoldingOut& inspMoldingOutInfo);
    ~InspMolding();

    int Molding_Main(InspMoldingOut& inspMoldingOutInfo);

private:
    static std::mutex modelLoadMutex;
    static std::map<std::string, std::string> moldingDetectionModelMap;  // 检测模型映射表
    static std::map<std::string, std::string> moldingDefectModelMap;     // 缺陷模型映射表

    cv::Mat m_img;

    InspMoldingIn m_params;
    bool readParams(cv::Mat img, const std::string& filePath, InspMoldingIn& params, InspMoldingOut& inspMoldingOutInfo, const std::string& fileName);
    bool validateCameraModels(int cameraId);
    void ResetModels();
    bool loadAllModels(InspMoldingOut& inspMoldingOutInfo, bool ini);  // 集中加载所有模型
    void Molding_SetROI(InspMoldingOut& inspMoldingOutInfo);
    void Molding_Locate(InspMoldingOut& inspMoldingOutInfo);
    void HandleDefect(const char* dirName, MOLDING_RETURN_VAL statusCode, InspMoldingOut& inspMoldingOutInfo, const char* errMsg);
    void Molding_CheckDefect(InspMoldingOut& inspMoldingOutInfo);
    void Molding_DrawResult(InspMoldingOut& inspMoldingOutInfo);
};

#endif // INSPMOLDING_H

