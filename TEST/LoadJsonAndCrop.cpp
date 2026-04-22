//#pragma once
//#include "HeaderDefine.h"
//#include <json.hpp>
//
//namespace fs = std::filesystem;
//using json = nlohmann::json;
//
//// 定义目标区域数据结构
//struct TargetRegion {
//    std::vector<std::vector<double>> points;  // 四个角点坐标
//    std::string label;                        // 类别标签
//};
//
//// 定义图像标注数据结构
//struct ImageAnnotation {
//    std::string imagePath;                   // 图像路径
//    std::vector<TargetRegion> regions;       // 目标区域列表
//};
//
//// 旋转矩形区域裁剪函数
//cv::Mat crop_obb_region(const cv::Mat& img, const std::vector<std::vector<double>>& points) {
//    // 将点转换为 OpenCV 格式
//    std::vector<cv::Point2f> srcPoints;
//    for (const auto& pt : points) {
//        srcPoints.emplace_back(pt[0], pt[1]);
//    }
//
//    // 计算旋转矩形的宽高
//    float w1 = cv::norm(srcPoints[0] - srcPoints[1]);
//    float w2 = cv::norm(srcPoints[2] - srcPoints[3]);
//    float h1 = cv::norm(srcPoints[1] - srcPoints[2]);
//    float h2 = cv::norm(srcPoints[3] - srcPoints[0]);
//
//    // 取对边的平均作为宽高
//    float width = std::max((w1 + w2) / 2.0f, 1.0f);
//    float height = std::max((h1 + h2) / 2.0f, 1.0f);
//
//    // 定义目标点
//    std::vector<cv::Point2f> dstPoints;
//    if (width >= height) {
//        dstPoints = {
//            {0, 0}, {width, 0},
//            {width, height}, {0, height}
//        };
//    }
//    else {
//        std::swap(width, height);
//        dstPoints = {
//            {0, height}, {0, 0},
//            {width, 0}, {width, height}
//        };
//    }
//
//    // 计算透视变换矩阵并应用
//    cv::Mat M = cv::getPerspectiveTransform(srcPoints, dstPoints);
//    cv::Mat warped;
//    cv::warpPerspective(img, warped, M, cv::Size(width, height));
//
//    return warped;
//}
//
//// 处理单个JSON文件
//void process_json_file(const fs::path& json_path, const fs::path& img_base_dir, const fs::path& output_base_dir) {
//    try {
//        // 读取JSON文件
//        std::ifstream file(json_path);
//        if (!file.is_open()) {
//            std::cerr << "无法打开JSON文件: " << json_path << std::endl;
//            return;
//        }
//
//        // 解析JSON数据
//        json json_data;
//        file >> json_data;
//        file.close();
//
//        // 转换为标注数据结构
//        ImageAnnotation annotation;
//        annotation.imagePath = json_data["imagePath"].get<std::string>();
//
//        for (const auto& shape : json_data["shapes"]) {
//            TargetRegion region;
//            region.label = shape["label"].get<std::string>();
//
//            for (const auto& pt : shape["points"]) {
//                region.points.push_back({ pt[0].get<double>(), pt[1].get<double>() });
//            }
//
//            annotation.regions.push_back(region);
//        }
//
//        // 构建完整图像路径
//        fs::path img_path = img_base_dir / annotation.imagePath;
//        if (!fs::exists(img_path)) {
//            std::cerr << "图像不存在: " << img_path << std::endl;
//            return;
//        }
//
//        // 读取图像
//        cv::Mat img = cv::imread(img_path.string());
//        if (img.empty()) {
//            std::cerr << "无法读取图像: " << img_path << std::endl;
//            return;
//        }
//
//        // 处理每个目标区域
//        for (size_t i = 0; i < annotation.regions.size(); ++i) {
//            const auto& region = annotation.regions[i];
//
//            // 检查是否为旋转矩形且包含4个点
//            if (region.points.size() != 4) {
//                std::cerr << "无效的点数: " << json_path << " - 区域 " << i << std::endl;
//                continue;
//            }
//
//            // 切割目标区域
//            cv::Mat cropped_img;
//            try {
//                cropped_img = crop_obb_region(img, region.points);
//            }
//            catch (const cv::Exception& e) {
//                std::cerr << "切割错误: " << json_path << " - " << e.what() << std::endl;
//                continue;
//            }
//
//            // 创建类别目录
//            fs::path class_dir = output_base_dir / region.label;
//            fs::create_directories(class_dir);
//
//            // 保存图像
//            std::string filename = json_path.stem().string() + "_" + std::to_string(i) + ".jpg";
//            fs::path output_path = class_dir / filename;
//
//            if (!cv::imwrite(output_path.string(), cropped_img)) {
//                std::cerr << "保存失败: " << output_path << std::endl;
//            }
//        }
//
//        std::cout << "处理完成: " << json_path << " -> "
//            << annotation.regions.size() << "个目标" << std::endl;
//
//    }
//    catch (const json::exception& e) {
//        std::cerr << "JSON解析错误: " << json_path << " - " << e.what() << std::endl;
//    }
//    catch (const std::exception& e) {
//        std::cerr << "处理错误: " << json_path << " - " << e.what() << std::endl;
//    }
//}
//
//int main() {
//    // 配置路径
//    fs::path json_dir = "F:/TRAIN_SAMPLE/CAR/BIG/OBB/ALL";     // JSON文件目录
//    fs::path img_dir = "F:/TRAIN_SAMPLE/CAR/BIG/OBB/ALL";     // 原始图像目录
//    fs::path output_dir = "F:/TRAIN_SAMPLE/CAR/BIG/OBB/2"; // 输出目录
//
//    // 检查目录存在性
//    if (!fs::exists(json_dir) || !fs::is_directory(json_dir)) {
//        std::cerr << "JSON目录不存在或无效: " << json_dir << std::endl;
//        return 1;
//    }
//
//    if (!fs::exists(img_dir) || !fs::is_directory(img_dir)) {
//        std::cerr << "图像目录不存在或无效: " << img_dir << std::endl;
//        return 1;
//    }
//
//    // 创建输出目录
//    fs::create_directories(output_dir);
//
//    // 遍历JSON目录处理所有文件
//    for (const auto& entry : fs::directory_iterator(json_dir)) {
//        if (entry.path().extension() == ".json") {
//            process_json_file(entry.path(), img_dir, output_dir);
//        }
//    }
//
//    std::cout << "所有文件处理完成!" << std::endl;
//    return 0;
//}