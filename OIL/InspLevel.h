#ifndef INSPLEVEL_H
#define INSPLEVEL_H

#include "InspLevelStruct.h"
#include "Log.h"
#include "AnalyseMat.h"
#include "CalcFun.h"
#include "Common.h"
#include "TxtOperater.h"
#include "DrawAndShowImg.h"
#include <mutex>


class InspLevel {
private:
    std::unique_ptr<Common> COM;
    std::unique_ptr<AnalyseMat> ANA;
    std::unique_ptr<DrawAndShowImg> DAS;
    std::unique_ptr<CalcFun> CAL;
    // 超时标志（原子操作保证线程安全）
    std::atomic<bool> m_timeoutFlag{ false };
    std::chrono::high_resolution_clock::time_point m_startTime;  // 使用高精度时钟
    static std::map<int, InspLevelIn> cameraConfigMap;

public:
    InspLevel(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
        bool isLoadConfig, int timeOut, InspLevelOut& outInfo);
    ~InspLevel();

    int Level_Main(InspLevelOut& outInfo);
    // 设置超时标志引用
    void SetTimeoutFlagRef(std::atomic<bool>& flag) {
        m_timeoutFlagRef = &flag;
    }

    void SetStartTimePoint(std::chrono::high_resolution_clock::time_point startTime) {
        m_startTime = startTime;
    }

private:
    static std::mutex modelLoadMutex;
    static std::map<std::string, std::string> levelDetectionModelMap;  // 检测模型映射表
    std::atomic<bool>* m_timeoutFlagRef = nullptr;

    cv::Mat m_img;
    cv::Mat m_imgColor; 

    int m_timeOut;

    InspLevelIn m_params; 
    bool CheckTimeout() const {
        if (m_timeoutFlagRef && m_timeoutFlagRef->load()) {
            return true;
        }
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();
        return (elapsed > m_timeOut);
    }
    bool readParams(cv::Mat img, const std::string& filePath, InspLevelIn& params, InspLevelOut& outInfo, const std::string& fileName);
    bool validateCameraModels(int cameraId);
    bool loadAllModels(InspLevelOut& outInfo, bool ini);  // 集中加载所有模型
    void Level_SetROI(InspLevelOut& outInfo);
    void Level_LocateLevel(InspLevelOut& outInfo);
    void Level_CheckLevel(InspLevelOut& outInfo);
    void Level_DrawResult(InspLevelOut& outInfo);
};

#endif // INSPPRESSCAP_H

