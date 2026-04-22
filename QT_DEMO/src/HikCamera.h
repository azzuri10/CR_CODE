#pragma once

#include <opencv2/opencv.hpp>
#include <string>

class HikCamera {
public:
    HikCamera();
    ~HikCamera();

    bool openFirst(std::string* errMsg);
    bool openByIndex(int deviceIndex, std::string* errMsg);
    bool isOpened() const;
    bool grab(cv::Mat* frame, std::string* errMsg, int timeoutMs = 1000);
    void close();

private:
    void* handle_;
};

