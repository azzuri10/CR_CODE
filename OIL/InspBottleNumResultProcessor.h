#pragma once
#ifndef INSPBOTTLENUM_RESULTPROCESSOR_H
#define INSPBOTTLENUM_RESULTPROCESSOR_H

#include "InspBottleNumStruct.h"
#include <vector>
#include <string>
#include <memory>

class Common;
class Log;
class AnalyseMat;
class CalcFun;

/**
 * @class InspBottleNumResultProcessor
 * @brief 处理瓶子数量检查结果的类
 * 
 * 负责瓶子匹配、结果检查、类型比较等后处理逻辑
 */
class InspBottleNumResultProcessor {
public:
    /**
     * @brief 构造函数
     * @param common 通用工具类实例
     * @param log 日志记录器实例
     * @param analyse 图像分析类实例
     * @param calc 计算函数类实例
     */
    InspBottleNumResultProcessor(std::shared_ptr<Common> common,
                               std::shared_ptr<Log> log,
                               std::shared_ptr<AnalyseMat> analyse,
                               std::shared_ptr<CalcFun> calc);
    
    /**
     * @brief 匹配瓶盖和提手
     * @param locateDetails 定位检测结果
     * @param bottleResult 输出瓶子结果
     * @return 匹配的瓶子数量
     */
    int MatchBottles(const std::vector<FinsObject>& locateDetails,
                    std::vector<BottleResult>& bottleResult);
    
    /**
     * @brief 检查结果是否符合目标配置
     * @param bottleResult 瓶子结果
     * @param targetTypes 目标类型配置
     * @param bottleNum 目标瓶子数量
     * @param checkResult 输出检查结果
     */
    void CheckResult(const std::vector<BottleResult>& bottleResult,
                    const std::vector<BottleType>& targetTypes,
                    int bottleNum,
                    BottleCheckResult& checkResult);
    
    /**
     * @brief 统计不同类型的瓶子
     * @param bottleResult 瓶子结果
     * @return 类型到数量的映射
     */
    std::map<std::string, int> CountBottleTypes(const std::vector<BottleResult>& bottleResult);
    
    /**
     * @brief 判断是否为异常瓶盖
     * @param bottleResult 瓶子结果
     * @param targetTypes 目标类型配置
     * @return 发现异常瓶盖返回true，否则返回false
     */
    bool HasAbnormalCaps(const std::vector<BottleResult>& bottleResult,
                        const std::vector<BottleType>& targetTypes);
    
    /**
     * @brief 判断是否为异常提手
     * @param bottleResult 瓶子结果
     * @param targetTypes 目标类型配置
     * @return 发现异常提手返回true，否则返回false
     */
    bool HasAbnormalHandles(const std::vector<BottleResult>& bottleResult,
                           const std::vector<BottleType>& targetTypes);
    
    /**
     * @brief 验证瓶子类型是否在目标配置中
     * @param bottleResult 瓶子结果
     * @param targetTypes 目标类型配置
     * @param undefinedTypes 输出未定义的类型
     * @return 所有类型都定义返回true，否则返回false
     */
    bool ValidateBottleTypes(const std::vector<BottleResult>& bottleResult,
                            const std::vector<BottleType>& targetTypes,
                            std::vector<std::string>& undefinedTypes);
    
    /**
     * @brief 计算总瓶子数量
     * @param bottleResult 瓶子结果
     * @return 总瓶子数量
     */
    int CalculateTotalBottles(const std::vector<BottleResult>& bottleResult);
    
    /**
     * @brief 计算目标总瓶子数量
     * @param targetTypes 目标类型配置
     * @return 目标总瓶子数量
     */
    int CalculateTargetTotalBottles(const std::vector<BottleType>& targetTypes);
    
    /**
     * @brief 生成检查结果消息
     * @param checkResult 检查结果
     * @return 格式化消息
     */
    std::string GenerateResultMessage(const BottleCheckResult& checkResult);
    
private:
    std::shared_ptr<Common> m_common;
    std::shared_ptr<Log> m_log;
    std::shared_ptr<AnalyseMat> m_analyse;
    std::shared_ptr<CalcFun> m_calc;
    
    /**
     * @brief 计算两个矩形的IOU（交并比）
     * @param rect1 矩形1
     * @param rect2 矩形2
     * @return IOU值
     */
    float CalculateIOU(const cv::Rect& rect1, const cv::Rect& rect2);
    
    /**
     * @brief 判断两个瓶子类型是否匹配
     * @param bottle 瓶子结果
     * @param target 目标类型
     * @return 匹配返回true，否则返回false
     */
    bool IsTypeMatch(const BottleResult& bottle, const BottleType& target);
    
    /**
     * @brief 判断是否为异常类型
     * @param capType 瓶盖类型
     * @param handleType 提手类型
     * @param targetTypes 目标类型配置
     * @return 异常类型返回true，否则返回false
     */
    bool IsAbnormalType(const std::string& capType,
                       const std::string& handleType,
                       const std::vector<BottleType>& targetTypes);
    
    /**
     * @brief 提取类型关键字
     * @param typeString 类型字符串
     * @return 提取的关键字
     */
    std::string ExtractTypeKey(const std::string& typeString);
};

#endif // INSPBOTTLENUM_RESULTPROCESSOR_H