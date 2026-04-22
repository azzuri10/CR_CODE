//#include <opencv2/opencv.hpp>
//#include <opencv2/dnn.hpp>
//#include <iostream>
//#include <vector>
//#include <filesystem>
//#include <map>
//#include <fstream>
//#include <json.hpp>
//#include "write_json.h"
//#include "InferenceWorker.h"
//
//namespace fs = std::filesystem;
//using namespace std;
//using namespace cv;
//using json = nlohmann::json;
//
//// 分割结果结构体
//struct SegmentationResult {
//    cv::Rect box;
//    float confidence;
//    std::string class_name;
//    cv::Mat mask;
//    std::vector<std::vector<cv::Point>> contours; // 轮廓信息
//};
//
//// Shape结构体
//struct Shape {
//    std::string label;
//    std::string shape_type;
//    std::vector<std::vector<double>> points;
//    bool difficult;
//    double direction;
//    nlohmann::json flags;
//    nlohmann::json attributes;
//    double score;
//
//    Shape() : difficult(false), direction(0.0), score(1.0) {}
//};
//
//// ImageAnnotation结构体
//struct ImageAnnotation {
//    std::string description;
//    std::string imagePath;
//    int imageWidth;
//    int imageHeight;
//    std::vector<Shape> shapes;
//    std::string version;
//    nlohmann::json flags;
//
//    ImageAnnotation() : imageWidth(0), imageHeight(0) {}
//};
//
//// 字符编码转换函数
//std::wstring StringToWString(const std::string& str, UINT codePage) {
//    if (str.empty()) return std::wstring();
//
//    int length = MultiByteToWideChar(codePage, 0, str.c_str(), -1, nullptr, 0);
//    if (length == 0) return std::wstring();
//
//    std::wstring wstr(length - 1, 0);
//    MultiByteToWideChar(codePage, 0, str.c_str(), -1, &wstr[0], length);
//    return wstr;
//}
//
//std::string WStringToString(const std::wstring& wstr, UINT codePage) {
//    if (wstr.empty()) return std::string();
//
//    int length = WideCharToMultiByte(codePage, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
//    if (length == 0) return std::string();
//
//    std::string str(length - 1, 0);
//    WideCharToMultiByte(codePage, 0, wstr.c_str(), -1, &str[0], length, nullptr, nullptr);
//    return str;
//}
//
//std::string UTF8ToGBK(const std::string& strUTF8) {
//#ifdef _WIN32
//    if (strUTF8.empty()) return std::string();
//    std::wstring wstr = StringToWString(strUTF8, CP_UTF8);
//    if (wstr.empty()) return strUTF8;
//    return WStringToString(wstr, CP_ACP);
//#else
//    return strUTF8;
//#endif
//}
//
//// JSON解析函数
//void from_json(const nlohmann::json& j, Shape& shape) {
//    try {
//        if (j.contains("label") && j["label"].is_string()) {
//            string className = UTF8ToGBK(j["label"].get<std::string>());
//            shape.label = className;
//        }
//
//        if (j.contains("shape_type") && j["shape_type"].is_string()) {
//            j.at("shape_type").get_to(shape.shape_type);
//        }
//
//        if (j.contains("points") && j["points"].is_array()) {
//            shape.points.clear();
//            for (const auto& point_array : j["points"]) {
//                if (point_array.is_array() && point_array.size() >= 2) {
//                    double x = point_array[0].is_null() ? 0.0 : point_array[0].get<double>();
//                    double y = point_array[1].is_null() ? 0.0 : point_array[1].get<double>();
//                    std::vector<double> point = { x, y };
//                    shape.points.push_back(point);
//                }
//            }
//        }
//
//        if (j.contains("difficult")) {
//            if (j["difficult"].is_boolean()) {
//                j.at("difficult").get_to(shape.difficult);
//            }
//            else {
//                shape.difficult = false;
//            }
//        }
//
//        if (j.contains("direction")) {
//            if (j["direction"].is_number()) {
//                j.at("direction").get_to(shape.direction);
//            }
//            else {
//                shape.direction = 0.0;
//            }
//        }
//
//        if (j.contains("flags")) {
//            shape.flags = j["flags"];
//        }
//
//        if (j.contains("attributes")) {
//            shape.attributes = j["attributes"];
//        }
//
//        if (j.contains("score")) {
//            if (j["score"].is_number()) {
//                j.at("score").get_to(shape.score);
//            }
//            else {
//                shape.score = 1.0;
//            }
//        }
//    }
//    catch (const nlohmann::json::exception& e) {
//        std::cerr << "Shape解析错误: " << e.what() << std::endl;
//    }
//}
//
//void from_json(const nlohmann::json& j, ImageAnnotation& annotation) {
//    try {
//        if (j.contains("description")) {
//            if (j["description"].is_string()) {
//                j.at("description").get_to(annotation.description);
//            }
//        }
//
//        if (j.contains("imagePath")) {
//            if (j["imagePath"].is_string()) {
//                j.at("imagePath").get_to(annotation.imagePath);
//            }
//        }
//
//        if (j.contains("imageWidth")) {
//            if (j["imageWidth"].is_number()) {
//                j.at("imageWidth").get_to(annotation.imageWidth);
//            }
//            else {
//                annotation.imageWidth = 0;
//            }
//        }
//
//        if (j.contains("imageHeight")) {
//            if (j["imageHeight"].is_number()) {
//                j.at("imageHeight").get_to(annotation.imageHeight);
//            }
//            else {
//                annotation.imageHeight = 0;
//            }
//        }
//
//        if (j.contains("shapes") && j["shapes"].is_array()) {
//            annotation.shapes.clear();
//            for (const auto& shape_json : j["shapes"]) {
//                Shape shape;
//                from_json(shape_json, shape);
//                annotation.shapes.push_back(shape);
//            }
//        }
//
//        if (j.contains("version")) {
//            if (j["version"].is_string()) {
//                j.at("version").get_to(annotation.version);
//            }
//        }
//
//        if (j.contains("flags")) {
//            annotation.flags = j["flags"];
//        }
//    }
//    catch (const nlohmann::json::exception& e) {
//        std::cerr << "ImageAnnotation解析错误: " << e.what() << std::endl;
//    }
//}
//
//// 读取JSON标注文件
//bool readAnnotationJSON(const std::string& file_path, ImageAnnotation& annotation) {
//    try {
//        if (!fs::exists(file_path)) {
//            std::cerr << "JSON文件不存在: " << file_path << std::endl;
//            return false;
//        }
//
//        std::ifstream file(file_path, std::ios::binary);
//        if (!file.is_open()) {
//            std::cerr << "无法打开JSON文件: " << file_path << std::endl;
//            return false;
//        }
//
//        std::string content((std::istreambuf_iterator<char>(file)),
//            std::istreambuf_iterator<char>());
//        file.close();
//
//        // 去除UTF-8 BOM
//        if (content.size() >= 3 &&
//            static_cast<unsigned char>(content[0]) == 0xEF &&
//            static_cast<unsigned char>(content[1]) == 0xBB &&
//            static_cast<unsigned char>(content[2]) == 0xBF) {
//            content = content.substr(3);
//        }
//
//        if (content.empty()) {
//            std::cerr << "JSON文件为空: " << file_path << std::endl;
//            return false;
//        }
//
//        nlohmann::json j;
//        try {
//            j = nlohmann::json::parse(content);
//        }
//        catch (const nlohmann::json::parse_error& e) {
//            std::cerr << "JSON解析错误: " << e.what() << std::endl;
//            return false;
//        }
//
//        if (!j.contains("imagePath") || !j["imagePath"].is_string()) {
//            std::cerr << "JSON缺少imagePath字段: " << file_path << std::endl;
//            return false;
//        }
//
//        annotation = j.get<ImageAnnotation>();
//        return true;
//    }
//    catch (const std::exception& e) {
//        std::cerr << "文件读取异常: " << e.what() << std::endl;
//    }
//    return false;
//}
//
//// 将点集转换为矩形
//Rect pointsToRect(const std::vector<std::vector<double>>& points) {
//    if (points.empty()) {
//        return Rect(0, 0, 0, 0);
//    }
//
//    double min_x = points[0][0], min_y = points[0][1];
//    double max_x = points[0][0], max_y = points[0][1];
//
//    for (const auto& point : points) {
//        if (point.size() < 2) continue;
//
//        min_x = min(min_x, point[0]);
//        min_y = min(min_y, point[1]);
//        max_x = max(max_x, point[0]);
//        max_y = max(max_y, point[1]);
//    }
//
//    int width = static_cast<int>(max(0.0, max_x - min_x));
//    int height = static_cast<int>(max(0.0, max_y - min_y));
//
//    return Rect(static_cast<int>(min_x), static_cast<int>(min_y), width, height);
//}
//
//// 计算掩码IoU
//double calculateMaskIoU(const cv::Mat& mask1, const cv::Mat& mask2) {
//    if (mask1.empty() || mask2.empty()) return 0.0;
//
//    cv::Mat intersection, union_mask;
//    cv::bitwise_and(mask1, mask2, intersection);
//    cv::bitwise_or(mask1, mask2, union_mask);
//
//    double intersection_area = cv::countNonZero(intersection);
//    double union_area = cv::countNonZero(union_mask);
//
//    if (union_area == 0) return 0.0;
//    return intersection_area / union_area;
//}
//
//// 从分割结果中提取轮廓
//std::vector<std::vector<cv::Point>> extractContours(const cv::Mat& mask) {
//    std::vector<std::vector<cv::Point>> contours;
//    if (mask.empty()) return contours;
//
//    cv::Mat binary_mask;
//    cv::threshold(mask, binary_mask, 0.5, 255, cv::THRESH_BINARY);
//    binary_mask.convertTo(binary_mask, CV_8UC1);
//
//    cv::findContours(binary_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
//    return contours;
//}
//
//// 将分割结果转换为JSON格式
//nlohmann::json segmentationToJson(const std::vector<SegmentationResult>& seg_results,
//    const std::string& image_filename,
//    int image_width, int image_height) {
//
//    nlohmann::json result_json;
//    result_json["imagePath"] = image_filename;
//    result_json["imageWidth"] = image_width;
//    result_json["imageHeight"] = image_height;
//    result_json["imageData"] = nullptr;  // 显式设置为null
//
//    nlohmann::json shapes_json = nlohmann::json::array();
//
//    // 遍历所有分割结果
//    for (const auto& seg_result : seg_results) {
//        // 添加边界框信息
//        nlohmann::json bbox_json;
//        bbox_json["label"] = seg_result.class_name + "_bbox";
//        bbox_json["score"] = seg_result.confidence;
//        bbox_json["shape_type"] = "rectangle";
//
//        nlohmann::json bbox_points = nlohmann::json::array();
//        bbox_points.push_back({ seg_result.box.x, seg_result.box.y });
//        bbox_points.push_back({ seg_result.box.x + seg_result.box.width,
//                              seg_result.box.y + seg_result.box.height });
//        bbox_json["points"] = bbox_points;
//        shapes_json.push_back(bbox_json);
//
//        // 添加掩码轮廓信息
//        if (!seg_result.contours.empty()) {
//            for (const auto& contour : seg_result.contours) {
//                nlohmann::json contour_json;
//                contour_json["label"] = seg_result.class_name + "_mask";
//                contour_json["score"] = seg_result.confidence;
//                contour_json["shape_type"] = "polygon";
//
//                nlohmann::json contour_points = nlohmann::json::array();
//                for (const auto& point : contour) {
//                    contour_points.push_back({ point.x, point.y });
//                }
//                contour_json["points"] = contour_points;
//                shapes_json.push_back(contour_json);
//            }
//        }
//    }
//
//    result_json["shapes"] = shapes_json;
//    return result_json;
//}
//
//// 可视化分割结果
//// 可视化分割结果
//cv::Mat visualizeSegmentation(const cv::Mat& image,
//    const std::vector<SegmentationResult>& results) {
//    cv::Mat vis_image = image.clone();
//
//    // 定义颜色
//    std::vector<cv::Scalar> colors = {
//        cv::Scalar(255, 0, 0),   // 蓝色
//        cv::Scalar(0, 255, 0),   // 绿色
//        cv::Scalar(0, 0, 255),   // 红色
//        cv::Scalar(255, 255, 0), // 青色
//        cv::Scalar(255, 0, 255), // 洋红
//        cv::Scalar(0, 255, 255)  // 黄色
//    };
//
//    for (size_t i = 0; i < results.size(); ++i) {
//        const auto& result = results[i];
//        cv::Scalar color = colors[i % colors.size()];
//
//        // 绘制边界框
//        cv::rectangle(vis_image, result.box, color, 2);
//
//        // 绘制类别和置信度
//        std::string label = result.class_name + " " + std::to_string(result.confidence).substr(0, 4);
//        cv::putText(vis_image, label,
//            cv::Point(result.box.x, result.box.y - 10),
//            cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
//
//        // 绘制掩码
//        if (!result.mask.empty()) {
//            // 创建与原图相同尺寸的彩色掩码
//            cv::Mat colored_mask = cv::Mat::zeros(image.size(), CV_8UC3);
//
//            // 将二值掩码转换为彩色
//            cv::Mat mask_colored;
//            cv::cvtColor(result.mask, mask_colored, cv::COLOR_GRAY2BGR);
//            mask_colored = mask_colored.mul(cv::Scalar(color[0], color[1], color[2]) / 255.0);
//
//            // 将彩色掩码复制到对应位置
//            cv::Rect box = result.box;
//
//            // 确保边界框在图像范围内
//            box.x = MAX(0, box.x);
//            box.y = MAX(0, box.y);
//            box.width = MIN(box.width, image.cols - box.x);
//            box.height = MIN(box.height, image.rows - box.y);
//
//            if (box.width > 0 && box.height > 0) {
//                // 调整掩码尺寸以匹配边界框
//                cv::Mat resized_mask;
//                cv::resize(mask_colored(box), resized_mask, box.size());
//
//                // 将调整后的掩码放到colored_mask的对应位置
//                resized_mask.copyTo(colored_mask(box));
//
//                // 透明叠加
//                double alpha = 0.3;
//                cv::addWeighted(colored_mask, alpha, vis_image, 1 - alpha, 0, vis_image);
//            }
//        }
//    }
//
//    return vis_image;
//}
//
//cv::Mat imread_utf8(const std::string& utf8_path) {
//    std::ifstream file(utf8_path, std::ios::binary | std::ios::ate);
//    if (!file.is_open()) return {};
//
//    std::streamsize size = file.tellg();
//    file.seekg(0, std::ios::beg);
//
//    std::vector<uchar> buffer(size);
//    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return {};
//
//    file.close();
//    return cv::imdecode(buffer, cv::IMREAD_COLOR);
//}
//
//int main() {
//    // 路径配置
//    string json_dir = "F:/TRAIN_SAMPLE_3.0/NECK/JJZL_MASK/0";
//    string image_dir = "F:/TRAIN_SAMPLE_3.0/NECK/JJZL_MASK/0";
//    string output_dir = "F:/TRAIN_SAMPLE_3.0/NECK/JJZL_MASK/segmentation_results";
//    string visualization_dir = output_dir + "/visualization";
//
//    // 创建输出目录
//    for (const auto& dir : { output_dir, visualization_dir }) {
//        if (!fs::exists(dir)) {
//            fs::create_directories(dir);
//        }
//    }
//
//    // 分割模型配置
//    string model_path = "E:/yolov8-8.2.0/ultralytics/1.train_3.0/best.onnx"; // 分割模型路径
//    vector<string> classes = { "CIRCLE_mask","IN", "TOP","TOP1","OUT"  }; // 分割类别
//    float conf_threshold = 0.75f;
//    float nms_threshold = 0.2f;
//    int cameraId = 0;
//
//    // 统计信息
//    int total_images = 0;
//    int processed_images = 0;
//    int detection_count = 0;
//    double total_confidence = 0.0;
//
//    cout << "开始验证分割模型..." << endl;
//    cout << "模型路径: " << model_path << endl;
//    cout << "类别: ";
//    for (const auto& cls : classes) {
//        cout << cls << " ";
//    }
//    cout << endl;
//
//    // 处理JSON目录中的所有文件
//    for (const auto& entry : fs::directory_iterator(json_dir)) {
//        if (entry.is_regular_file() && entry.path().extension() == ".json") {
//            total_images++;
//            string json_path = entry.path().string();
//            string filename = entry.path().stem().string();
//
//            cout << "\n处理文件: " << filename << ".json" << endl;
//
//            // 读取JSON标注
//            ImageAnnotation annotation;
//            if (!readAnnotationJSON(json_path, annotation)) {
//                cerr << "无法读取JSON文件: " << json_path << endl;
//                continue;
//            }
//
//            // 查找图像文件
//            string image_path = (fs::path(image_dir) / annotation.imagePath).string();
//            if (!fs::exists(image_path)) {
//                image_path = (fs::path(image_dir) / (filename + ".jpg")).string();
//                if (!fs::exists(image_path)) {
//                    image_path = (fs::path(image_dir) / (filename + ".png")).string();
//                    if (!fs::exists(image_path)) {
//                        cerr << "图像文件不存在: " << image_path << endl;
//                        continue;
//                    }
//                }
//            }
//
//            // 读取图像
//            Mat img = imread_utf8(image_path);
//            if (img.empty()) {
//                cerr << "无法读取图像: " << image_path << endl;
//                continue;
//            }
//
//            // 执行分割推理
//            vector<SegmentationResult> segmentation_results;
//            try {
//                // 使用分割推理函数
//                auto seg_objects = InferenceWorker::RunSegmentation(
//                    cameraId, model_path, classes, img, conf_threshold, nms_threshold);
//
//                // 转换为SegmentationResult
//                for (const auto& seg_obj : seg_objects) {
//                    SegmentationResult result;
//                    result.box = seg_obj.box;
//                    result.confidence = seg_obj.confidence;
//                    result.class_name = seg_obj.class_name;
//                    result.mask = seg_obj.mask;
//                    result.contours = extractContours(seg_obj.mask);
//
//                 /*   cv::namedWindow("mask", cv::WINDOW_NORMAL);
//                    cv::imshow("mask", result.mask);
//                    cv::waitKey(0);*/
//
//                    segmentation_results.push_back(result);
//                    detection_count++;
//                    total_confidence += seg_obj.confidence;
//                }
//
//                processed_images++;
//
//            }
//            catch (const std::exception& e) {
//                cerr << "分割推理错误: " << e.what() << endl;
//                continue;
//            }
//
//            // 输出结果信息
//            cout << "检测到 " << segmentation_results.size() << " 个分割目标" << endl;
//            for (const auto& result : segmentation_results) {
//                cout << "  - " << result.class_name << ": 置信度=" << result.confidence
//                    << ", 边界框=(" << result.box.x << "," << result.box.y
//                    << "," << result.box.width << "," << result.box.height << ")" << endl;
//            }
//
//            for (int i = 0; i < segmentation_results[0].contours.size(); i++) {
//                for (int j = 0; j < segmentation_results[0].contours[i].size(); j++) {
//                    circle(img, segmentation_results[0].contours[i][j], 3, cv::Scalar (0, 255, 0), -1, 8, 0);
//                }
//            }
//            
//            // 保存分割结果JSON
//            if (!segmentation_results.empty()) {
//                nlohmann::json result_json = segmentationToJson(
//                    segmentation_results, // 这里简化处理，只保存第一个结果
//                    fs::path(image_path).filename().string(),
//                    img.cols, img.rows);
//
//                string result_json_path = (fs::path(output_dir) / (filename + ".json")).string();
//                std::ofstream result_file(result_json_path);
//                if (result_file.is_open()) {
//                    result_file << result_json.dump(4);
//                    result_file.close();
//                    cout << "分割结果已保存: " << result_json_path << endl;
//                }
//            }
//
//            // 可视化并保存结果
//            if (!segmentation_results.empty()) {
//                cv::Mat vis_image = visualizeSegmentation(img, segmentation_results);
//                string vis_path = (fs::path(visualization_dir) / (filename + "_vis.jpg")).string();
//                if (cv::imwrite(vis_path, vis_image)) {
//                    cout << "可视化结果已保存: " << vis_path << endl;
//                }
//            }
//
//            // 比较分割结果与标注（可选）
//            if (!annotation.shapes.empty() && !segmentation_results.empty()) {
//                // 这里可以添加更复杂的分割结果评估逻辑
//                // 比如计算掩码IoU、精度、召回率等
//                cout << "标注数量: " << annotation.shapes.size() << endl;
//            }
//        }
//    }
//
//    // 输出统计信息
//    cout << "\n=== 分割模型验证完成 ===" << endl;
//    cout << "总图像数: " << total_images << endl;
//    cout << "成功处理: " << processed_images << endl;
//    cout << "总检测数: " << detection_count << endl;
//    if (detection_count > 0) {
//        cout << "平均置信度: " << (total_confidence / detection_count) << endl;
//    }
//    cout << "结果保存在: " << output_dir << endl;
//
//    return 0;
//}