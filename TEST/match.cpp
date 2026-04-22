//#include <iostream>
//#include <vector>
//#include <fstream>
//#include <algorithm>
//#include <iomanip>
//
//enum PRESSCAP_RETURN_VAL {
//    PRESSCAP_RETURN_ALGO_ERR = -2,
//    PRESSCAP_RETURN_INPUT_PARA_ERR = -1,
//    PRESSCAP_RETURN_TIMEOUT = 0,
//    PRESSCAP_RETURN_OK = 1,
//    PRESSCAP_RETURN_CONFIG_ERR = 11002,
//    PRESSCAP_RETURN_CAP_CLOSE_LR_BOUNDARY = 11003,
//    PRESSCAP_RETURN_CAP_CLOSE_TOP_BOUNDARY = 11004,
//    PRESSCAP_RETURN_LEAK = 11005,
//    PRESSCAP_RETURN_NO_CAP_TOP = 11006,
//    PRESSCAP_RETURN_NO_CAP = 11007,
//    PRESSCAP_RETURN_CAP_SCRAP = 11008,
//    PRESSCAP_RETURN_FIND_LOW_CAP = 11009,
//    PRESSCAP_RETURN_FIND_HIGH_CAP = 11010,
//    PRESSCAP_RETURN_FIND_TOP_ANGLE_ERR = 11011,
//    PRESSCAP_RETURN_FIND_BOTTOM_ANGLE_ERR = 11012,
//    PRESSCAP_RETURN_FIND_ANGLE_ERR = 11013,
//    PRESSCAP_RETURN_CAP_BOTTOM_TYPE_FAILED = 11014,
//    PRESSCAP_RETURN_CAP_TOP_TYPE_FAILED = 11015,
//    PRESSCAP_RETURN_CAP_CRIMP = 11016,
//    PRESSCAP_RETURN_BAR_BRIDGE_BREAK = 11017,
//    PRESSCAP_RETURN_BAR_CAP_SEP = 11018,
//    PRESSCAP_RETURN_BAR_BREAK = 11019,
//    PRESSCAP_RETURN_DEFECT_LEAK = 11020,
//    PRESSCAP_RETURN_LR_FAILED = 11021,
//    PRESSCAP_RETURN_OTHER = 11100,
//    PRESSCAP_RETURN_THREAD_CONTENTION = 11101
//};
//
//// 获取所有可能的返回值
//std::vector<PRESSCAP_RETURN_VAL> get_all_return_vals() {
//    return {
//        PRESSCAP_RETURN_ALGO_ERR,
//        PRESSCAP_RETURN_INPUT_PARA_ERR,
//        PRESSCAP_RETURN_TIMEOUT,
//        PRESSCAP_RETURN_OK,
//        PRESSCAP_RETURN_CONFIG_ERR,
//        PRESSCAP_RETURN_CAP_CLOSE_LR_BOUNDARY,
//        PRESSCAP_RETURN_CAP_CLOSE_TOP_BOUNDARY,
//        PRESSCAP_RETURN_LEAK,
//        PRESSCAP_RETURN_NO_CAP_TOP,
//        PRESSCAP_RETURN_NO_CAP,
//        PRESSCAP_RETURN_CAP_SCRAP,
//        PRESSCAP_RETURN_FIND_LOW_CAP,
//        PRESSCAP_RETURN_FIND_HIGH_CAP,
//        PRESSCAP_RETURN_FIND_TOP_ANGLE_ERR,
//        PRESSCAP_RETURN_FIND_BOTTOM_ANGLE_ERR,
//        PRESSCAP_RETURN_FIND_ANGLE_ERR,
//        PRESSCAP_RETURN_CAP_BOTTOM_TYPE_FAILED,
//        PRESSCAP_RETURN_CAP_TOP_TYPE_FAILED,
//        PRESSCAP_RETURN_CAP_CRIMP,
//        PRESSCAP_RETURN_BAR_BRIDGE_BREAK,
//        PRESSCAP_RETURN_BAR_CAP_SEP,
//        PRESSCAP_RETURN_BAR_BREAK,
//        PRESSCAP_RETURN_DEFECT_LEAK,
//        PRESSCAP_RETURN_LR_FAILED,
//        PRESSCAP_RETURN_OTHER,
//        PRESSCAP_RETURN_THREAD_CONTENTION
//    };
//}
//
//// 获取错误码名称
//std::string get_error_name(PRESSCAP_RETURN_VAL val) {
//    switch (val) {
//    case PRESSCAP_RETURN_ALGO_ERR: return "算法异常";
//    case PRESSCAP_RETURN_INPUT_PARA_ERR: return "参数异常";
//    case PRESSCAP_RETURN_TIMEOUT: return "算法超时";
//    case PRESSCAP_RETURN_OK: return "OK";
//    case PRESSCAP_RETURN_CONFIG_ERR: return "配置错误";
//    case PRESSCAP_RETURN_CAP_CLOSE_LR_BOUNDARY: return "无目标";
//    case PRESSCAP_RETURN_CAP_CLOSE_TOP_BOUNDARY: return "靠近上边界";
//    case PRESSCAP_RETURN_LEAK: return "压盖不严";
//    case PRESSCAP_RETURN_NO_CAP_TOP: return "无上盖";
//    case PRESSCAP_RETURN_NO_CAP: return "无盖";
//    case PRESSCAP_RETURN_CAP_SCRAP: return "缺陷盖";
//    case PRESSCAP_RETURN_FIND_LOW_CAP: return "矮盖";
//    case PRESSCAP_RETURN_FIND_HIGH_CAP: return "高盖";
//    case PRESSCAP_RETURN_FIND_TOP_ANGLE_ERR: return "盖顶歪斜";
//    case PRESSCAP_RETURN_FIND_BOTTOM_ANGLE_ERR: return "支撑环歪斜";
//    case PRESSCAP_RETURN_FIND_ANGLE_ERR: return "瓶盖歪斜";
//    case PRESSCAP_RETURN_CAP_BOTTOM_TYPE_FAILED: return "盖底类型错误";
//    case PRESSCAP_RETURN_CAP_TOP_TYPE_FAILED: return "盖帽类型错误";
//    case PRESSCAP_RETURN_CAP_CRIMP: return "瓶盖破损";
//    case PRESSCAP_RETURN_BAR_BRIDGE_BREAK: return "防盗环断桥";
//    case PRESSCAP_RETURN_BAR_CAP_SEP: return "上下盖分离";
//    case PRESSCAP_RETURN_BAR_BREAK: return "防盗环缺陷";
//    case PRESSCAP_RETURN_DEFECT_LEAK: return "压盖不严(缺陷)";
//    case PRESSCAP_RETURN_LR_FAILED: return "支撑环端点失败";
//    case PRESSCAP_RETURN_THREAD_CONTENTION: return "线程竞争";
//    default: return "其他错误";
//    }
//}
//
//// 综合判断函数
//PRESSCAP_RETURN_VAL get_comprehensive_result(
//    const std::vector<PRESSCAP_RETURN_VAL>& results,
//    bool QY
//) {
//    // 检查无盖 (11007) - 最高优先级
//    if (std::find(results.begin(), results.end(), PRESSCAP_RETURN_NO_CAP) != results.end()) {
//        return PRESSCAP_RETURN_NO_CAP;
//    }
//
//    if (!QY) {
//        // QY=FALSE模式
//        // 所有相机OK
//        if (std::all_of(results.begin(), results.end(),
//            [](auto val) { return val == PRESSCAP_RETURN_OK; })) {
//            return PRESSCAP_RETURN_OK;
//        }
//
//        // 返回首个非OK错误
//        for (auto val : results) {
//            if (val != PRESSCAP_RETURN_OK) {
//                return val;
//            }
//        }
//    }
//    else {
//        // QY=TRUE模式
//        // 所有相机报盖底类型错误
//        if (std::all_of(results.begin(), results.end(),
//            [](auto val) { return val == PRESSCAP_RETURN_CAP_BOTTOM_TYPE_FAILED; })) {
//            return PRESSCAP_RETURN_CAP_BOTTOM_TYPE_FAILED;
//        }
//
//        // 统计OK和盖底错误数量
//        int ok_count = 0;
//        int bottom_err_count = 0;
//
//        for (auto val : results) {
//            if (val == PRESSCAP_RETURN_OK) ok_count++;
//            if (val == PRESSCAP_RETURN_CAP_BOTTOM_TYPE_FAILED) bottom_err_count++;
//        }
//
//        // OK+盖底混合场景
//        if (ok_count >= 1 && (ok_count + bottom_err_count == results.size())) {
//            return PRESSCAP_RETURN_OK;
//        }
//
//        // 严重错误检查 (11002-11101)
//        for (auto val : results) {
//            if (val >= PRESSCAP_RETURN_CONFIG_ERR && val <= PRESSCAP_RETURN_THREAD_CONTENTION) {
//                // 跳过盖底错误（已有单独处理）
//                if (val == PRESSCAP_RETURN_CAP_BOTTOM_TYPE_FAILED) continue;
//                return val;
//            }
//        }
//
//        // 所有相机OK
//        if (ok_count == results.size()) {
//            return PRESSCAP_RETURN_OK;
//        }
//
//        // 返回首个非OK非盖底错误
//        for (auto val : results) {
//            if (val != PRESSCAP_RETURN_OK && val != PRESSCAP_RETURN_CAP_BOTTOM_TYPE_FAILED) {
//                return val;
//            }
//        }
//    }
//
//    // 默认返回
//    return PRESSCAP_RETURN_OTHER;
//}
//
//// 生成所有组合并导出到CSV
//void generate_all_combinations() {
//    auto all_vals = get_all_return_vals();
//    int total_vals = all_vals.size();
//    int total_combinations = total_vals * total_vals * total_vals;
//
//    // 创建CSV文件
//    std::ofstream outfile("all_camera_combinations.csv");
//    outfile << "组合ID,相机0值,相机0说明,相机1值,相机1说明,相机2值,相机2说明,"
//        << "QY=FALSE结果,QY=FALSE说明,QY=TRUE结果,QY=TRUE说明\n";
//
//    int combination_id = 1;
//
//    // 三重循环生成所有组合
//    for (int i = 0; i < total_vals; i++) {
//        for (int j = 0; j < total_vals; j++) {
//            for (int k = 0; k < total_vals; k++) {
//                std::vector<PRESSCAP_RETURN_VAL> results = {
//                    all_vals[i],
//                    all_vals[j],
//                    all_vals[k]
//                };
//
//                // 计算两种模式的结果
//                auto result_false = get_comprehensive_result(results, false);
//                auto result_true = get_comprehensive_result(results, true);
//
//                // 写入CSV
//                outfile << combination_id << ","
//                    << results[0] << "," << get_error_name(results[0]) << ","
//                    << results[1] << "," << get_error_name(results[1]) << ","
//                    << results[2] << "," << get_error_name(results[2]) << ","
//                    << result_false << "," << get_error_name(result_false) << ","
//                    << result_true << "," << get_error_name(result_true) << "\n";
//
//                combination_id++;
//
//                // 显示进度
//                if (combination_id % 1000 == 0) {
//                    std::cout << "已生成 " << combination_id << "/" << total_combinations
//                        << " 个组合 (" << std::fixed << std::setprecision(1)
//                        << (static_cast<double>(combination_id) / total_combinations * 100)
//                        << "%)\n";
//                }
//            }
//        }
//    }
//
//    outfile.close();
//    std::cout << "已生成所有 " << total_combinations << " 个组合到 all_camera_combinations.csv 文件\n";
//}
//
//int main() {
//    // 生成所有组合
//    generate_all_combinations();
//    return 0;
//}