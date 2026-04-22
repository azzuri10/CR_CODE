#pragma once

#include <QString>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

struct FinsObject {
    cv::Rect box;
    float confidence;
    std::string className;
};

struct InspCapOmniResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];
    int capHeight;
    int capHeightDeviation;
    float topAngle;
    float bottomAngle;
    float topBottomAngleDif;
    std::vector<FinsObject> locates;
    std::vector<FinsObject> defect;
    std::string capTopType;
    std::string capBottomType;
};

struct InspLevelResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];
    int levelY;
    int grayDis;
    int project;
};

struct InspHandleResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];
    std::vector<FinsObject> locates;
    std::string handleType;
    std::string filmType;
};

struct InspCodeResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];
};

struct InspBoxBagResult {
    cv::Mat imgOut;
    int jobId;
    int cameraId;
    char startTime[64];
    char endTime[64];
    int statusCode;
    char errorMessage[256];
};

enum class InspectType {
    CapOmni,
    Level,
    Handle,
    Box,
    Code
};

class DllInvoker {
public:
    DllInvoker();
    ~DllInvoker();

    bool load(const QString& dllPath, QString* errMsg);
    bool isLoaded() const;
    int inspect(InspectType type, const cv::Mat& input, int jobId, int cameraId, cv::Mat* output, QString* errMsg);

private:
    using FnInspectCapOmni = int (*)(cv::Mat, int, int, const char*, bool, int, InspCapOmniResult*);
    using FnInspectLevel = int (*)(cv::Mat, int, int, const char*, bool, int, InspLevelResult*);
    using FnInspectHandle = int (*)(cv::Mat, int, int, const char*, bool, int, InspHandleResult*);
    using FnInspectBox = int (*)(cv::Mat, int, int, const char*, bool, int, InspBoxBagResult*);
    using FnInspectCode = int (*)(cv::Mat, int, int, const char*, bool, int, InspCodeResult*);

    void* module_;
    FnInspectCapOmni fnCapOmni_;
    FnInspectLevel fnLevel_;
    FnInspectHandle fnHandle_;
    FnInspectBox fnBox_;
    FnInspectCode fnCode_;
};
