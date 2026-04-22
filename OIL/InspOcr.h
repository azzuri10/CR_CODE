#pragma once
#ifndef INSPOCR_H
#define INSPOCR_H

#include "InspOcrStruct.h"
#include "InferenceWorker.h"
#include "LoadJsonConfig.h"
#include "Log.h"
#include "AnalyseMat.h"
#include "Common.h"
#include "DrawAndShowImg.h"

class InspOcr {
public:
    InspOcr(const cv::Mat& img, int cameraId, int jobId, int timeOut, InspOcrOut& outInfo);
    int Ocr_Main(InspOcrOut& outInfo);

private:
    bool readParams(const std::string& filePath, InspOcrIn& params, InspOcrOut& outInfo);
    static std::vector<std::string> SplitCsv(const std::string& s);
    void Ocr_SetROI(InspOcrOut& outInfo);
    void Ocr_DetectText(InspOcrOut& outInfo);
    bool Ocr_DetectTextPaddleNative(InspOcrOut& outInfo);
    void Ocr_RecognizeText(InspOcrOut& outInfo);
    bool Ocr_RecognizeTextPaddleNative(InspOcrOut& outInfo);
    void Ocr_RecognizeTextConcurrentFallback(InspOcrOut& outInfo);
    void Ocr_CompareTargets(InspOcrOut& outInfo);
    void Ocr_DrawResult(InspOcrOut& outInfo);
    bool CheckTimeout() const;

private:
    std::unique_ptr<Log> LOG;
    std::unique_ptr<Common> COM;
    std::unique_ptr<AnalyseMat> ANA;
    std::unique_ptr<DrawAndShowImg> DAS;

    cv::Mat m_img;
    InspOcrIn m_params;
    std::chrono::high_resolution_clock::time_point m_startTime;
};

#endif // INSPOCR_H

