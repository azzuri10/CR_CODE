#ifndef TXTOPERATER_H
#define TXTOPERATER_H

#include "AnalyseMat.h"
#include "HeaderDefine.h"
#include "Log.h"

class TxtOperater {
private:
    std::unique_ptr<Log> LOG;
    std::unique_ptr<AnalyseMat> ANA;
public:
    TxtOperater() : LOG(std::make_unique<Log>()), ANA(std::make_unique<AnalyseMat>()) {}
    ~TxtOperater(void);

    bool ChangeWordForShow(const std::string& path, std::vector<std::vector<std::string>>& changeWords, const std::string& logFile);
    bool LoadConfigPara(std::string configPath, std::string paraName, std::string& out, std::string logFile);
    bool LoadRectInTxt(std::string path, std::vector<cv::Rect>& outRects, std::string logFile);

    bool LoadTypeConfigInTxt(const std::string& path,
        std::vector<DetectionCriteria>& typeConfig,
        const std::string& logFile);

    std::string Trim(const std::string& str);
    void StringSplit(const std::string& s, const std::string& delim, std::vector<std::string>& ans);
    bool LoadWordInTxt(std::string path, std::vector<std::vector<std::string>>& checkWords);
    bool LoadAssistLocateFile(std::vector<std::vector<cv::Rect>>& assRects, std::vector<cv::Mat>& markImgs, int cameraId, std::string assLocateFilePath, std::string logFile);

private:
};

#endif  // TXTOPERATER_H
#pragma once
