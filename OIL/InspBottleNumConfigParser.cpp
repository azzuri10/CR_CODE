#include "InspBottleNumConfigParser.h"
#include "Common.h"
#include "Log.h"
#include "LoadConfigYOLO.h"
#include <algorithm>
#include <locale>

// 构造函数
InspBottleNumConfigParser::InspBottleNumConfigParser(std::shared_ptr<Common> common, std::shared_ptr<Log> log)
    : m_common(common), m_log(log) {}

bool InspBottleNumConfigParser::ReadParams(const cv::Mat& img, 
                                          const std::string& filePath, 
                                          InspBottleNumIn& params,
                                          int cameraId,
                                          InspBottleNumOut& outInfo,
                                          const std::string& fileName) {
    std::ifstream ifs(filePath.c_str());
    if (!ifs.is_open()) {
        outInfo.status.errorMessage = "config文件丢失!";
        m_log->WriteAsyncLog("config文件丢失！", WARNING, fileName, true);
        return false;
    }
    
    std::string line;
    while (!ifs.eof()) {
        std::getline(ifs, line);
        
        // 跳过注释和空行
        if (line.find("##") != std::string::npos || line.empty()) {
            continue;
        }
        
        // 解析行
        std::string keyword, value;
        if (!ParseLine(line, keyword, value)) {
            outInfo.status.errorMessage = "参数格式错误: " + line;
            m_log->WriteAsyncLog("参数格式错误: " + line, WARNING, fileName, true);
            return false;
        }
        
        // 根据关键字处理不同类型的参数
        bool success = false;
        if (keyword.find("ROI") != std::string::npos) {
            success = HandleROIParams(keyword, value, img, params, outInfo);
        } else if (keyword.find("WEIGHTS") != std::string::npos || 
                   keyword.find("CLASSFY") != std::string::npos ||
                   keyword.find("CONFIG") != std::string::npos) {
            success = HandleModelParams(keyword, value, cameraId, params, outInfo);
        } else if (keyword.find("SAVE") != std::string::npos ||
                   keyword.find("DRAW") != std::string::npos) {
            success = HandleBoolParams(keyword, value, params);
        } else if (keyword == "BOTTLENUM_TIMEOUT") {
            params.timeOut = std::stoi(value);
            success = true;
        } else if (keyword == "BOTTLENUM_CHECK_COLOR") {
            params.isCheckColor = (std::stoi(value) == 1);
            success = true;
        } else if (keyword == "BOTTLENUM_BOTTLE_NUM") {
            params.bottleNum = std::stoi(value);
            success = true;
        } else if (keyword == "BOTTLENUM_HARDWARE_TYPE") {
            params.hardwareType = std::stoi(value);
            success = true;
        } else if (keyword == "BOTTLENUM_MODEL_TYPE") {
            params.modelType = std::stoi(value);
            success = true;
        }
        
        if (!success) {
            outInfo.status.errorMessage = "参数处理失败: " + keyword;
            m_log->WriteAsyncLog("参数处理失败: " + keyword, WARNING, fileName, true);
            return false;
        }
    }
    
    ifs.close();
    return true;
}

bool InspBottleNumConfigParser::ParseLine(const std::string& line, std::string& keyword, std::string& value) {
    size_t colonPos = line.find_first_of(":");
    if (colonPos == std::string::npos || colonPos == 0) {
        return false;
    }
    
    keyword = line.substr(0, colonPos);
    value = line.substr(colonPos + 1);
    
    // 修剪字符串
    keyword = TrimString(keyword);
    value = TrimString(value);
    
    return true;
}

bool InspBottleNumConfigParser::HandleROIParams(const std::string& keyword,
                                               const std::string& value,
                                               const cv::Mat& img,
                                               InspBottleNumIn& params,
                                               InspBottleNumOut& outInfo) {
    try {
        int intValue = std::stoi(value);
        
        if (keyword == "BOTTLENUM_ROI_X") {
            params.roiRect.x = intValue;
            if (params.roiRect.x < 0 || params.roiRect.x >= img.cols) {
                outInfo.status.errorMessage = "ROI_X: 超出图像范围!";
                return false;
            }
        } else if (keyword == "BOTTLENUM_ROI_Y") {
            params.roiRect.y = intValue;
            if (params.roiRect.y < 0 || params.roiRect.y >= img.rows) {
                outInfo.status.errorMessage = "ROI_Y: 超出图像范围!";
                return false;
            }
        } else if (keyword == "BOTTLENUM_ROI_W") {
            params.roiRect.width = intValue;
            if (params.roiRect.width <= 0) {
                outInfo.status.errorMessage = "ROI_W: 宽度必须大于0!";
                return false;
            }
        } else if (keyword == "BOTTLENUM_ROI_H") {
            params.roiRect.height = intValue;
            if (params.roiRect.height <= 0) {
                outInfo.status.errorMessage = "ROI_H: 高度必须大于0!";
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        outInfo.status.errorMessage = std::string("ROI参数转换失败: ") + e.what();
        return false;
    }
}

bool InspBottleNumConfigParser::HandleModelParams(const std::string& keyword,
                                                 const std::string& value,
                                                 int cameraId,
                                                 InspBottleNumIn& params,
                                                 InspBottleNumOut& outInfo) {
    if (keyword == "BOTTLENUM_LOCATE_WEIGHTS_FLIE") {
        params.locateWeightsFile = value;
        if (!ValidateFileExists(params.locateWeightsFile, "定位模型文件缺失!", outInfo, outInfo.paths.logFile)) {
            return false;
        }
    } else if (keyword == "BOTTLENUM_LOCATE_CONFIG_FLIE") {
        params.locateThreshConfig = value;
        if (!ValidateFileExists(params.locateThreshConfig, "定位阈值文件缺失!", outInfo, outInfo.paths.logFile)) {
            return false;
        }
    } else if (keyword == "BOTTLENUM_CAP_CLASSFY_WEIGHTS_FILE") {
        params.capClassifyWeightsFile = value;
        if (params.isCheckColor && !ValidateFileExists(params.capClassifyWeightsFile, "瓶盖分类模型文件缺失!", outInfo, outInfo.paths.logFile)) {
            return false;
        }
    } else if (keyword == "BOTTLENUM_CAP_CLASSFY_CLASSES_FILE") {
        params.capClassifyNameFile = value;
        if (params.isCheckColor && !ValidateFileExists(params.capClassifyNameFile, "瓶盖分类类型文件缺失!", outInfo, outInfo.paths.logFile)) {
            return false;
        }
    } else if (keyword == "BOTTLENUM_HANDLE_CLASSFY_WEIGHTS_FILE") {
        params.handleClassifyWeightsFile = value;
        if (params.isCheckColor && !ValidateFileExists(params.handleClassifyWeightsFile, "提手分类模型文件缺失!", outInfo, outInfo.paths.logFile)) {
            return false;
        }
    } else if (keyword == "BOTTLENUM_HANDLE_CLASSFY_CLASSES_FILE") {
        params.handleClassifyNameFile = value;
        if (params.isCheckColor && !ValidateFileExists(params.handleClassifyNameFile, "提手分类类型文件缺失!", outInfo, outInfo.paths.logFile)) {
            return false;
        }
    } else if (keyword == "BOTTLENUM_TYPE_CONFIG") {
        params.targetConfigFile = value;
        if (params.isCheckColor && !ValidateFileExists(params.targetConfigFile, "目标类型配置文件缺失!", outInfo, outInfo.paths.logFile)) {
            return false;
        }
    }
    
    return true;
}

bool InspBottleNumConfigParser::HandleBoolParams(const std::string& keyword,
                                                const std::string& value,
                                                InspBottleNumIn& params) {
    try {
        int intValue = std::stoi(value);
        bool boolValue = (intValue == 1);
        
        if (keyword == "BOTTLENUM_SAVE_DEBUG_IMAGE") {
            params.saveDebugImage = boolValue;
        } else if (keyword == "BOTTLENUM_SAVE_RESULT_IMAGE") {
            params.saveResultImage = boolValue;
        } else if (keyword == "BOTTLENUM_SAVE_LOG_TXT") {
            params.saveLogTxt = boolValue;
        } else if (keyword == "BOTTLENUM_DRAW_RESULT") {
            params.drawResult = boolValue;
        } else if (keyword == "BOTTLENUM_SAVE_TRAIN") {
            params.saveTrain = intValue; // 0-3 整数
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool InspBottleNumConfigParser::ValidateFileExists(const std::string& filePath,
                                                  const std::string& errorMessage,
                                                  InspBottleNumOut& outInfo,
                                                  const std::string& logFile) {
    if (!m_common->FileExistsModern(filePath)) {
        outInfo.status.errorMessage = errorMessage;
        m_log->WriteAsyncLog(filePath, ERR, logFile, true, "--" + errorMessage);
        return false;
    }
    return true;
}

bool InspBottleNumConfigParser::ValidateParams(const cv::Mat& img,
                                              const InspBottleNumIn& params,
                                              InspBottleNumOut& outInfo,
                                              int cameraId) {
    // 检查ROI是否在图像范围内
    if (params.roiRect.x < 0 || params.roiRect.x + params.roiRect.width > img.cols ||
        params.roiRect.y < 0 || params.roiRect.y + params.roiRect.height > img.rows) {
        outInfo.status.errorMessage = "ROI设置超出图像范围!";
        return false;
    }
    
    // 检查超时设置
    if (params.timeOut <= 0) {
        outInfo.status.errorMessage = "超时时间必须大于0!";
        return false;
    }
    
    // 检查瓶子数量
    if (params.bottleNum < 0) {
        outInfo.status.errorMessage = "瓶子数量不能为负数!";
        return false;
    }
    
    // 如果检查颜色，验证相关文件
    if (params.isCheckColor) {
        // 验证分类模型文件
        if (params.capClassifyWeightsFile.empty() || !m_common->FileExistsModern(params.capClassifyWeightsFile)) {
            outInfo.status.errorMessage = "瓶盖分类模型文件缺失!";
            return false;
        }
        
        if (params.handleClassifyWeightsFile.empty() || !m_common->FileExistsModern(params.handleClassifyWeightsFile)) {
            outInfo.status.errorMessage = "提手分类模型文件缺失!";
            return false;
        }
        
        // 验证目标配置文件
        if (params.targetConfigFile.empty() || !m_common->FileExistsModern(params.targetConfigFile)) {
            outInfo.status.errorMessage = "目标类型配置文件缺失!";
            return false;
        }
    }
    
    // 验证定位模型文件
    if (params.locateWeightsFile.empty() || !m_common->FileExistsModern(params.locateWeightsFile)) {
        outInfo.status.errorMessage = "定位模型文件缺失!";
        return false;
    }
    
    return true;
}

int InspBottleNumConfigParser::LoadConfigYOLO(const std::string& configFile,
                                             std::vector<YoloConfig>& configs,
                                             std::vector<std::string>& classNames,
                                             const std::string& logFile) {
    // 调用现有的LoadConfigYOLO函数
    return ::LoadConfigYOLO(configFile, configs, classNames, logFile);
}

bool InspBottleNumConfigParser::LoadTargetConfig(const std::string& configFile,
                                                int& bottleNum,
                                                std::vector<BottleType>& targetTypes,
                                                const std::string& logFile) {
    std::ifstream ifs(configFile);
    if (!ifs.is_open()) {
        m_log->WriteAsyncLog("目标类型配置文件打开失败: " + configFile, ERR, logFile, true);
        return false;
    }
    
    std::string line;
    bottleNum = 0;
    targetTypes.clear();
    
    while (std::getline(ifs, line)) {
        if (line.empty() || line.find("##") != std::string::npos) {
            continue;
        }
        
        std::string keyword, value;
        if (!ParseLine(line, keyword, value)) {
            continue;
        }
        
        if (keyword == "BOTTLENUM_BOTTLE_NUM") {
            try {
                bottleNum = std::stoi(value);
            } catch (...) {
                m_log->WriteAsyncLog("瓶子数量解析失败: " + value, ERR, logFile, true);
                return false;
            }
        } else if (keyword == "TARGET_TYPE") {
            // 解析目标类型格式: 类型名称,瓶盖类型,提手类型,数量
            std::vector<std::string> parts;
            size_t start = 0, end;
            
            while ((end = value.find(',', start)) != std::string::npos) {
                parts.push_back(value.substr(start, end - start));
                start = end + 1;
            }
            parts.push_back(value.substr(start));
            
            if (parts.size() >= 4) {
                BottleType type;
                type.typeName = TrimString(parts[0]);
                type.capType = TrimString(parts[1]);
                type.handleType = TrimString(parts[2]);
                try {
                    type.num = std::stoi(TrimString(parts[3]));
                } catch (...) {
                    type.num = 0;
                }
                
                targetTypes.push_back(type);
            }
        }
    }
    
    ifs.close();
    return true;
}

bool InspBottleNumConfigParser::LoadClassNames(const std::string& classFile,
                                              std::vector<std::string>& classNames,
                                              const std::string& logFile) {
    std::ifstream ifs(classFile);
    if (!ifs.is_open()) {
        m_log->WriteAsyncLog("类别文件打开失败: " + classFile, ERR, logFile, true);
        return false;
    }
    
    classNames.clear();
    std::string line;
    
    while (std::getline(ifs, line)) {
        line = TrimString(line);
        if (!line.empty()) {
            classNames.push_back(line);
        }
    }
    
    ifs.close();
    return true;
}

std::string InspBottleNumConfigParser::TrimString(const std::string& str) {
    if (str.empty()) return str;
    
    auto start = str.find_first_not_of(" \t\r\n");
    auto end = str.find_last_not_of(" \t\r\n");
    
    if (start == std::string::npos) return "";
    
    return str.substr(start, end - start + 1);
}