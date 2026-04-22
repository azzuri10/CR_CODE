#ifndef INSPHANDLE_H
#define INSPHANDLE_H

#include "InspHandleStruct.h"
#include "LoadJsonConfig.h"
#include "Log.h"
#include "AnalyseMat.h"
#include "CalcFun.h"
#include "Common.h"
#include "TxtOperater.h"
#include "DrawAndShowImg.h"
#include <mutex>
#include <shared_mutex>
#include "write_json.h"


class InspHandle {
private:
    std::unique_ptr<Common> COM;
    std::unique_ptr<AnalyseMat> ANA;
    std::unique_ptr<DrawAndShowImg> DAS;
    std::unique_ptr<CalcFun> CAL;
    std::atomic<bool> m_timeoutFlag{ false };
    std::chrono::high_resolution_clock::time_point m_startTime;  // 使用高精度时钟
    static std::map<int, InspHandleIn> cameraConfigMap;

public:
    InspHandle(std::string configPath, const cv::Mat& img, int cameraId, int jobId,
        bool isLoadConfig, int timeOut, InspHandleOut& outInfo);
    ~InspHandle();

    int Handle_Main(InspHandleOut& outInfo);

    void SetTimeoutFlagRef(std::atomic<bool>& flag) {
        m_timeoutFlagRef = &flag;
    }

    void SetStartTimePoint(std::chrono::high_resolution_clock::time_point startTime) {
        m_startTime = startTime;
    }
private:
    static std::shared_mutex modelLoadMutex;
    static std::map<std::string, std::string> handleLocationModelMap;  // 检测模型映射表
    static std::map<std::string, std::string> handleClassfyModelMap;    // 检测模型映射表
    static std::map<std::string, std::string> filmClassfyModelMap;      // 检测模型映射表

    std::atomic<bool>* m_timeoutFlagRef = nullptr;
    cv::Mat m_img;
    bool m_isLoadConfig; 
    int m_timeOut;

    // 超时检查函数
    bool CheckTimeout(int timeOut) const {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();

        return (elapsed > timeOut);
    }


    InspHandleIn m_params;
    bool readParams(cv::Mat img, const std::string& filePath, InspHandleIn& params, InspHandleOut& outInfo, const std::string& fileName);
    bool validateCameraModels(int cameraId);
    bool loadAllModels(InspHandleOut& outInfo, bool ini);  // 集中加载所有模型
    void Handle_SetROI(InspHandleOut& outInfo);
    void Handle_LocateHandle(InspHandleOut& outInfo);
    void Handle_CheckHandle(InspHandleOut& outInfo);
    void Handle_CheckFilm(InspHandleOut& outInfo);
    void Handle_DrawResult(InspHandleOut& outInfo);
};

#endif // INSPPRESSCAP_H

