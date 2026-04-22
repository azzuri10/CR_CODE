#pragma once
#ifndef INSPBOTTLENUM_CONFIGPARSER_H
#define INSPBOTTLENUM_CONFIGPARSER_H

#include "InspBottleNumStruct.h"
#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <memory>

class Common;
class Log;

/**
 * @class InspBottleNumConfigParser
 * @brief 专门处理InspBottleNum配置文件读取和解析的类
 * 
 * 负责从配置文件中读取所有参数，并进行基本的验证和错误检查
 */
class InspBottleNumConfigParser {
public:
    /**
     * @brief 构造函数
     * @param common 通用工具类实例
     * @param log 日志记录器实例
     */
    InspBottleNumConfigParser(std::shared_ptr<Common> common, std::shared_ptr<Log> log);
    
    /**
     * @brief 从文件读取参数到InspBottleNumIn结构体
     * @param img 输入图像（用于验证ROI范围）
     * @param filePath 配置文件路径
     * @param params 输出参数结构体
     * @param cameraId 相机ID
     * @param outInfo 输出信息（用于错误状态）
     * @param fileName 日志文件名
     * @return 读取成功返回true，否则返回false
     */
    bool ReadParams(const cv::Mat& img, 
                   const std::string& filePath, 
                   InspBottleNumIn& params,
                   int cameraId,
                   InspBottleNumOut& outInfo,
                   const std::string& fileName);
    
    /**
     * @brief 验证配置参数的有效性
     * @param img 输入图像
     * @param params 参数结构体
     * @param outInfo 输出信息
     * @param cameraId 相机ID
     * @return 验证成功返回true，否则返回false
     */
    bool ValidateParams(const cv::Mat& img,
                       const InspBottleNumIn& params,
                       InspBottleNumOut& outInfo,
                       int cameraId);
    
    /**
     * @brief 加载YOLO配置
     * @param configFile 配置文件路径
     * @param configs 输出配置向量
     * @param classNames 输出类别名称向量
     * @param logFile 日志文件路径
     * @return 加载成功返回1，否则返回错误码
     */
    int LoadConfigYOLO(const std::string& configFile,
                      std::vector<YoloConfig>& configs,
                      std::vector<std::string>& classNames,
                      const std::string& logFile);
    
    /**
     * @brief 加载目标类型配置
     * @param configFile 配置文件路径
     * @param bottleNum 输出瓶子数量
     * @param targetTypes 输出目标类型向量
     * @param logFile 日志文件路径
     * @return 加载成功返回true，否则返回false
     */
    bool LoadTargetConfig(const std::string& configFile,
                         int& bottleNum,
                         std::vector<BottleType>& targetTypes,
                         const std::string& logFile);
    
    /**
     * @brief 加载分类类别名称
     * @param classFile 类别文件路径
     * @param classNames 输出类别名称向量
     * @param logFile 日志文件路径
     * @return 加载成功返回true，否则返回false
     */
    bool LoadClassNames(const std::string& classFile,
                       std::vector<std::string>& classNames,
                       const std::string& logFile);
    
private:
    std::shared_ptr<Common> m_common;
    std::shared_ptr<Log> m_log;
    
    /**
     * @brief 解析单行配置
     * @param line 配置行
     * @param keyword 输出关键字
     * @param value 输出值
     * @return 解析成功返回true，否则返回false
     */
    bool ParseLine(const std::string& line, std::string& keyword, std::string& value);
    
    /**
     * @brief 处理ROI相关参数
     * @param keyword 关键字
     * @param value 值
     * @param img 图像
     * @param params 参数结构体
     * @param outInfo 输出信息
     * @return 处理成功返回true，否则返回false
     */
    bool HandleROIParams(const std::string& keyword,
                        const std::string& value,
                        const cv::Mat& img,
                        InspBottleNumIn& params,
                        InspBottleNumOut& outInfo);
    
    /**
     * @brief 处理模型相关参数
     * @param keyword 关键字
     * @param value 值
     * @param cameraId 相机ID
     * @param params 参数结构体
     * @param outInfo 输出信息
     * @return 处理成功返回true，否则返回false
     */
    bool HandleModelParams(const std::string& keyword,
                          const std::string& value,
                          int cameraId,
                          InspBottleNumIn& params,
                          InspBottleNumOut& outInfo);
    
    /**
     * @brief 处理布尔类型参数
     * @param keyword 关键字
     * @param value 值
     * @param params 参数结构体
     * @return 处理成功返回true，否则返回false
     */
    bool HandleBoolParams(const std::string& keyword,
                         const std::string& value,
                         InspBottleNumIn& params);
    
    /**
     * @brief 验证文件是否存在
     * @param filePath 文件路径
     * @param errorMessage 错误信息
     * @param outInfo 输出信息
     * @param logFile 日志文件路径
     * @return 文件存在返回true，否则返回false
     */
    bool ValidateFileExists(const std::string& filePath,
                           const std::string& errorMessage,
                           InspBottleNumOut& outInfo,
                           const std::string& logFile);
    
    /**
     * @brief 修剪字符串（移除前后空格）
     * @param str 输入字符串
     * @return 修剪后的字符串
     */
    std::string TrimString(const std::string& str);
};

#endif // INSPBOTTLENUM_CONFIGPARSER_H