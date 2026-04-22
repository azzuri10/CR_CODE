#include "TxtOperater.h"

TxtOperater::~TxtOperater() = default; 

bool IsCommentOrEmpty(const std::string& line) {
    return line.find("##") != std::string::npos || line.empty();
}

std::string TxtOperater::Trim(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";

    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

void TxtOperater::StringSplit(const std::string& s, const std::string& delim, std::vector<std::string>& ans) {
    size_t start = 0, end;
    while ((end = s.find(delim, start)) != std::string::npos) {
        if (end != start) ans.push_back(s.substr(start, end - start));
        start = end + delim.length();
    }
    if (start < s.length())  ans.push_back(s.substr(start));
}

bool TxtOperater::ChangeWordForShow(const std::string& path,
    std::vector<std::vector<std::string>>& changeWords,
    const std::string& logFile) {
    std::ifstream ifs(path.c_str());
    if (!ifs.is_open()) {
        Log::WriteAsyncLog("字符标准对照表文件丢失!", ERR, logFile, true);
        return false;
    }

    std::string line;
    std::vector<std::vector<std::string>> standardWords;
    standardWords.reserve(200);
    while (!ifs.eof()) {
        if (IsCommentOrEmpty(line)) continue;
        //读取行字符串
        //发现"##"为注释，跳过；空行跳过
        getline(ifs, line);
        int findShap = line.find_first_of("##");

        size_t findPos = line.find("##");

        if (findPos != std::string::npos || line.empty()) {
            continue;
        }
        std::vector<std::string> temWords;
        StringSplit(line, ",", temWords);
        if (temWords.size() != 2)
        {
            Log::WriteAsyncLog("字符标准对照表文件内容异常，每行一对字符串应当用逗号隔开!", ERR, logFile, true);
            return false;
        }
        standardWords.push_back(temWords);
    }

    bool changeSuccess = false;
    for (int i = 0; i < changeWords.size(); i++) 
    {
        for (int j = 0; j < changeWords[i].size(); j++) 
        {
            for (int kk = 0; kk < standardWords.size(); kk++)
            {
                if (changeWords[i][j] == standardWords[kk][0])
                {
                    changeWords[i][j] = standardWords[kk][1];
                    changeSuccess = true;
                    continue;
                }
            }
            if (changeSuccess == false)
            {
                Log::WriteAsyncLog("字符标准对照表文件内容缺失，请检查训练类型与对照表是否有出入!", ERR, logFile, true);
                return false;
            }
        }
    }

    return true;
}

bool TxtOperater::LoadConfigPara(std::string configPath, std::string paraName,
    std::string& out, std::string logFile) {
    // Load names of classes
    std::ifstream ifs_log(logFile.c_str());
    //if (!ifs_log.is_open()) {
    //    // to be add
    //    return false;
    //}
    std::ifstream ifs(configPath.c_str());
    if (!ifs.is_open()) {
        Log::WriteAsyncLog("算法config文件丢失!", ERR, logFile, true);
        return false;
    }
    std::string line;
    while (!ifs.eof()) {
        //读取行字符串
        //发现"#"为注释，跳过；空行跳过
        //发现“:”，提取关键字；未发现则config异常
        getline(ifs, line);
        if (IsCommentOrEmpty(line)) continue;
        /*int findShap = line.find_first_of("#");
        if (findShap >= 0 || line.empty()) {
            continue;
        }  */
        size_t findPos = line.find("##");

        if (findPos != std::string::npos || line.empty()) {
            continue;
        }
        int findCommon = line.find_first_of(":");
        std::string keyWord;
        if (findCommon >= 0) {
            keyWord = line.substr(0, findCommon);
        }
        else {
            // to be add
            return false;
        }
        //是否存储中间图像(0:否  1:是)
        if (keyWord == paraName) {
            std::string cutName;
            std::string tmp = line.substr(findCommon + 1);
            int stringSize = tmp.size();
            if (stringSize > 1) {
                cutName = tmp.substr(stringSize - 1, stringSize - 1);
                if (cutName == "\r") {
                    out = tmp.substr(0, stringSize - 1);
                }
                else {
                    out = tmp;
                }
            }
            else {
                out = tmp;
            }
            return true;
        }
    }
    return false;
}

bool TxtOperater::LoadRectInTxt(std::string path, std::vector<cv::Rect>& outRects,
    std::string logFile) {
    outRects.clear();
    std::ifstream ifs(path.c_str());
    if (!ifs.is_open()) {
        // to be add
        return false;
    }
    std::string line;
    while (!ifs.eof()) {
        //读取行字符串
        //发现"#"为注释，跳过；空行跳过
        getline(ifs, line);
        if (IsCommentOrEmpty(line)) continue;

        int findShap = line.find_first_of("#");
        if (findShap >= 0 || line.empty()) {
            continue;
        }
        int sx = -1;
        int sy = -1;
        int totalW = -1;
        int totalH = -1;
        sscanf(line.c_str(), "%d,%d,%d,%d\n", &sx, &sy, &totalW, &totalH);
        if (sx == -1 || sy == -1 || totalW == -1 || totalH == -1) {
            Log::WriteAsyncLog("label.txt中cv::Rect参数缺失!", ERR, logFile, true);
            return false;
        }
        cv::Rect templRect;
        templRect.x = sx;
        templRect.y = sy;
        templRect.width = totalW;
        templRect.height = totalH;
        outRects.push_back(templRect);
    }
    return true;
}

bool TxtOperater::LoadTypeConfigInTxt(const std::string& path,
    std::vector<DetectionCriteria>& typeConfig,
    const std::string& logFile)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        Log::WriteAsyncLog(path, ERR, logFile, true, "--配置文件打开失败");
        return false;
    }

    typeConfig.clear();
    std::string line;
    int lineNum = 0;

    while (std::getline(ifs, line)) {
        lineNum++;
        line = Trim(line);  // 去除前后空格的工具函数

        // 跳过注释和空行
        if (line.empty() || line[0] == '#') continue;

        DetectionCriteria criteria;
        int parseCount = sscanf(line.c_str(),
            "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%f",  // 调整参数顺序对应新结构体
            &criteria.target_type,
            &criteria.search_area.x,
            &criteria.search_area.y,
            &criteria.search_area.width,
            &criteria.search_area.height,
            &criteria.count.minimum,
            &criteria.count.maximum,
            &criteria.length.minimum,
            &criteria.length.maximum,
            &criteria.angle.minimum,
            &criteria.angle.maximum,
            &criteria.confidence_threshold);

        // 验证参数数量
        if (parseCount != 12) {
            Log::WriteAsyncLog(path, ERR, logFile, true,
                "行" + std::to_string(lineNum) + "参数格式错误");
            return false;
        }

        // 参数有效性验证
        const bool invalidParams =
            criteria.target_type < 0 ||
            criteria.search_area.area() <= 0 ||
            criteria.count.minimum < 0 || criteria.count.maximum < criteria.count.minimum ||
            criteria.length.minimum < 0 || criteria.length.maximum < criteria.length.minimum ||
            criteria.angle.minimum < -180 || criteria.angle.maximum > 180 ||
            criteria.confidence_threshold < 0.0f || criteria.confidence_threshold > 1.0f;

        if (invalidParams) {
            Log::WriteAsyncLog(path, ERR, logFile, true,
                "行" + std::to_string(lineNum) + "参数值超出有效范围");
            return false;
        }

        typeConfig.emplace_back(std::move(criteria));
    }

    return !typeConfig.empty();  // 确保至少加载了一个有效配置
}


bool TxtOperater::LoadWordInTxt(std::string path,
    std::vector<std::vector<std::string>>& checkWords) {
    checkWords.clear();
    std::ifstream ifs(path.c_str());
    if (!ifs.is_open()) {
        // to be add
        return false;
    }
    std::string line;
    while (!ifs.eof()) {
        //读取行字符串
        //发现"#"为注释，跳过；空行跳过
        getline(ifs, line);
        if (IsCommentOrEmpty(line)) continue;

        int findShap = line.find_first_of("##");

        size_t findPos = line.find("##");

        if (findPos != std::string::npos || line.empty()) {
            continue;
        }
        std::vector<std::string> temWords;
        StringSplit(line, ",", temWords);
        checkWords.push_back(temWords);
    }
    return true;
}

bool TxtOperater::LoadAssistLocateFile(std::vector<std::vector<cv::Rect>>& assRects,
    std::vector<cv::Mat>& markImgs, int cameraId,
    std::string assLocateFilePath,
    std::string logFile) {
    if (cameraId < 0 || cameraId >= assRects.size()) {
        Log::WriteAsyncLog("相机ID越界", ERR, logFile, true);
        return false;
    }

    //assRects.clear();
    //markImgs.clear();
    if (!LoadRectInTxt(assLocateFilePath + "/assist_locate.txt",
        assRects[cameraId], logFile)) {
        Log::WriteAsyncLog("辅助定位配置:txt文件丟失!", ERR, logFile, true);
    }
    if (assRects[cameraId].size() != 4) {
        Log::WriteAsyncLog("辅助定位配置:txt中参数错误!", ERR, logFile, true);
        return false;
    }
    if (!ANA->JudgeRectIn(assRects[cameraId][0], assRects[cameraId][1]) ||
        !ANA->JudgeRectIn(assRects[cameraId][0], assRects[cameraId][2]) ||
        !ANA->JudgeRectIn(assRects[cameraId][0], assRects[cameraId][3]) ||
        !ANA->JudgeRectIn(assRects[cameraId][1], assRects[cameraId][2])) {
        Log::WriteAsyncLog("辅助定位配置:第一行参数区域必须包含其他三行!", ERR, logFile, true);
        return false;
    }
    // load mark img
    std::string markImgPath = assLocateFilePath + "/0.jpg";
    cv::Mat markImg = cv::imread(markImgPath, 0);
    if (markImg.empty()) {
        Log::WriteAsyncLog("辅助定位配置:标志图像文件缺失!", ERR, logFile, true);
        return false;
    }
    else {
        markImgs[cameraId] = markImg;
    }
    return true;
}
