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
//
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
//// JSON解析函数 - 修复null值处理
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
//                    // 处理可能的null值
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
//                shape.difficult = false; // 默认值
//            }
//        }
//
//        if (j.contains("direction")) {
//            if (j["direction"].is_number()) {
//                j.at("direction").get_to(shape.direction);
//            }
//            else {
//                shape.direction = 0.0; // 默认值
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
//                shape.score = 1.0; // 默认值
//            }
//        }
//    }
//    catch (const nlohmann::json::exception& e) {
//        std::cerr << "Shape解析错误: " << e.what() << std::endl;
//        // 输出有问题的JSON内容以便调试
//        std::cerr << "问题JSON内容: " << j.dump() << std::endl;
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
//                annotation.imageWidth = 0; // 默认值
//            }
//        }
//
//        if (j.contains("imageHeight")) {
//            if (j["imageHeight"].is_number()) {
//                j.at("imageHeight").get_to(annotation.imageHeight);
//            }
//            else {
//                annotation.imageHeight = 0; // 默认值
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
//        // 输出有问题的JSON内容以便调试
//        std::cerr << "问题JSON内容: " << j.dump() << std::endl;
//    }
//}
//
//// 增强的JSON读取函数
//bool readAnnotationJSON(const std::string& file_path, ImageAnnotation& annotation) {
//    try {
//        if (!fs::exists(file_path)) {
//            std::cerr << "错误: JSON文件不存在: " << file_path << std::endl;
//            return false;
//        }
//
//        std::ifstream file(file_path, std::ios::binary);
//        if (!file.is_open()) {
//            std::cerr << "错误: 无法打开JSON文件: " << file_path << std::endl;
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
//        // 检查内容是否为空
//        if (content.empty()) {
//            std::cerr << "错误: JSON文件为空: " << file_path << std::endl;
//            return false;
//        }
//
//        nlohmann::json j;
//        try {
//            j = nlohmann::json::parse(content);
//        }
//        catch (const nlohmann::json::parse_error& e) {
//            std::cerr << "JSON解析错误: " << e.what() << std::endl;
//            std::cerr << "文件: " << file_path << std::endl;
//            // 输出文件前100个字符以便调试
//            std::cerr << "文件开头内容: " << content.substr(0, 100) << std::endl;
//            return false;
//        }
//
//        // 验证必要的字段
//        if (!j.contains("imagePath") || !j["imagePath"].is_string()) {
//            std::cerr << "错误: JSON缺少imagePath字段: " << file_path << std::endl;
//            return false;
//        }
//
//        if (!j.contains("shapes") || !j["shapes"].is_array()) {
//            std::cerr << "警告: JSON缺少shapes字段或格式不正确: " << file_path << std::endl;
//            // 继续处理，但shapes为空
//        }
//
//        annotation = j.get<ImageAnnotation>();
//        return true;
//    }
//    catch (const std::exception& e) {
//        std::cerr << "文件读取错误: " << e.what() << std::endl;
//        std::cerr << "文件: " << file_path << std::endl;
//    }
//    return false;
//}
//
//// 增强的点集转矩形函数，增加边界检查
//Rect pointsToRect(const std::vector<std::vector<double>>& points) {
//    if (points.empty()) {
//        std::cerr << "警告: 点集为空" << std::endl;
//        return Rect(0, 0, 0, 0);
//    }
//
//    double min_x = points[0][0], min_y = points[0][1];
//    double max_x = points[0][0], max_y = points[0][1];
//
//    for (const auto& point : points) {
//        if (point.size() < 2) {
//            std::cerr << "警告: 点坐标数量不足" << std::endl;
//            continue;
//        }
//
//        min_x = min(min_x, point[0]);
//        min_y = min(min_y, point[1]);
//        max_x = max(max_x, point[0]);
//        max_y = max(max_y, point[1]);
//    }
//
//    // 确保宽度和高度为正数
//    int width = static_cast<int>(max(0.0, max_x - min_x));
//    int height = static_cast<int>(max(0.0, max_y - min_y));
//
//    return Rect(static_cast<int>(min_x), static_cast<int>(min_y), width, height);
//}
//
//// 将ImageAnnotation转换为FinsObject向量
//vector<FinsObject> convertToFinsObjects(const ImageAnnotation& annotation) {
//    vector<FinsObject> detections;
//
//    for (const auto& shape : annotation.shapes) {
//        FinsObject obj;
//        obj.box = pointsToRect(shape.points);
//        obj.confidence = static_cast<float>(shape.score);
//        obj.className = shape.label;
//        detections.push_back(obj);
//    }
//
//    return detections;
//}
//
//// IoU计算
//double calculateIoU(const Rect& rect1, const Rect& rect2) {
//    int x1 = max(rect1.x, rect2.x);
//    int y1 = max(rect1.y, rect2.y);
//    int x2 = min(rect1.x + rect1.width, rect2.x + rect2.width);
//    int y2 = min(rect1.y + rect1.height, rect2.y + rect2.height);
//
//    if (x2 < x1 || y2 < y1) return 0.0;
//
//    int intersection = (x2 - x1) * (y2 - y1);
//    int union_area = rect1.area() + rect2.area() - intersection;
//
//    return static_cast<double>(intersection) / union_area;
//}
//
//// 比较检测结果与标注
//bool compareWithAnnotation(const vector<FinsObject>& detections,
//    const ImageAnnotation& annotation,
//    double iou_threshold = 0.5) {
//    if (detections.size() != annotation.shapes.size()) {
//        return false;
//    }
//
//    for (const auto& det : detections) {
//        bool found_match = false;
//        for (const auto& shape : annotation.shapes) {
//            if (shape.label == det.className) {
//                Rect annot_rect = pointsToRect(shape.points);
//                double iou = calculateIoU(det.box, annot_rect);
//                if (iou >= iou_threshold) {
//                    found_match = true;
//                    break;
//                }
//            }
//        }
//        if (!found_match) return false;
//    }
//    return true;
//}
//
//// 保存JSON文件
//void saveJSONToFile1(const nlohmann::json& j, const std::string& file_path) {
//    std::ofstream file(file_path);
//    if (file.is_open()) {
//        file << j.dump(4); // 使用4个空格缩进
//        file.close();
//    }
//    else {
//        std::cerr << "无法保存JSON文件: " << file_path << std::endl;
//    }
//}
//
//cv::Mat imread_utf8(const std::string& utf8_path) {
//    // 读取文件到 vector<uchar>
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
//
//    // 直接解码 [7,9](@ref)
//    return cv::imdecode(buffer, cv::IMREAD_COLOR);
//}
//
//int main() {
//    // 路径配置
//    string json_dir = "F:/TRAIN_SAMPLE_3.0/NECK/JJZL1021/12X";
//    string image_dir = "F:/TRAIN_SAMPLE_3.0/NECK/JJZL1021/12X";
//    string output_dir = "F:/TRAIN_SAMPLE_3.0/NECK/JJZL1021/13XX";
//    string output_dirY = "F:/TRAIN_SAMPLE_3.0/NECK/JJZL1021/13YY";
//    //string consistent_dir = "F:/TRAIN_SAMPLE/TS/MYYQ1/1/2";
//
//    //// 创建输出目录
//    //for (const auto& dir : { output_dir, consistent_dir }) {
//    //    if (!fs::exists(dir)) {
//    //        fs::create_directories(dir);
//    //    }
//    //}
//
//    // 统计信息
//    int consistent_count = 0;
//    int inconsistent_count = 0;
//
//    // 遍历JSON目录
//    for (const auto& entry : fs::directory_iterator(json_dir)) {
//        if (entry.is_regular_file() && entry.path().extension() == ".json") {
//            string json_path = entry.path().string();
//            string filename = entry.path().stem().string();
//
//            cout << "处理文件: " << filename << ".json" << endl;
//
//            // 读取JSON文件
//            ImageAnnotation annotation;
//            if (!readAnnotationJSON(json_path, annotation)) {
//                cerr << "无法读取JSON文件: " << json_path << endl;
//                continue;
//            }
//
//            // 构建图像路径
//            string image_path = (fs::path(image_dir) / annotation.imagePath).string();
//            if (!fs::exists(image_path)) {
//                // 如果imagePath中的路径不存在，尝试使用同名的图像文件
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
//            // 模型路径和参数[6](@ref)
//            //string model_path = "E:/yolov8-8.2.0/ultralytics/1.train_3.0/best.onnx";
//            string model_path = "D:/CONFIG_CR_1.0/Model/BOTTLENECK/JJ20/best.onnx";
//        
//            vector<string> classes = { "IN0","IN1","BURR0","BURR1","TOP0","TOP1","TOP2","OUT0","OUT1","OTHER", };
//            //vector<string> classes = { "OK" };
//            //vector<string> classes = { "无盖", "瓶盖", "无上盖", "缺陷盖" };
//            float conf_threshold = 0.2;
//            float nms_threshold = 0.1;
//            int cameraId = 0;
//            double iou_threshold = 0.5; // IoU阈值
//            vector<FinsObject> detections = InferenceWorker::Run(cameraId, model_path, classes, img, conf_threshold, nms_threshold);
//
//            // 比较检测结果与标注是否一致
//            bool is_consistent = compareWithAnnotation(detections, annotation);
//
//            // 保存结果
//            string output_filename = fs::path(image_path).filename().string();
//
//            if (!is_consistent) {
//                inconsistent_count++;
//                cout << "结果不一致: " << output_filename << endl;
//
//                // 保存原始图像到不一致目录
//                string img_path = (fs::path(output_dir) / output_filename).string();
//                if (imwrite(img_path, img)) {
//                    cout << "已保存原始图像: " << img_path << endl;
//                }
//                else {
//                    cerr << "保存原始图像失败: " << img_path << endl;
//                }
//
//                // 保存原始标注JSON（副本）
//                nlohmann::json annotation_json;
//                try {
//                    std::ifstream file(json_path);
//                    file >> annotation_json;
//                    file.close();
//                }
//                catch (const std::exception& e) {
//                    cerr << "读取原始标注JSON失败: " << e.what() << endl;
//                }
//
//                string annotation_copy_path = (fs::path(output_dir) / (filename + ".json")).string();
//                saveJSONToFile1(annotation_json, annotation_copy_path);
//                cout << "已保存标注JSON: " << annotation_copy_path << endl;
//            }
//            else
//            {
//                // 保存原始图像到不一致目录
//                string img_path = (fs::path(output_dirY) / output_filename).string();
//                if (imwrite(img_path, img)) {
//                    cout << "已保存原始图像: " << img_path << endl;
//                }
//                else {
//                    cerr << "保存原始图像失败: " << img_path << endl;
//                }
//
//                // 保存原始标注JSON（副本）
//                nlohmann::json annotation_json;
//                try {
//                    std::ifstream file(json_path);
//                    file >> annotation_json;
//                    file.close();
//                }
//                catch (const std::exception& e) {
//                    cerr << "读取原始标注JSON失败: " << e.what() << endl;
//                }
//
//                string annotation_copy_path = (fs::path(output_dirY) / (filename + ".json")).string();
//                saveJSONToFile1(annotation_json, annotation_copy_path);
//                cout << "已保存标注JSON: " << annotation_copy_path << endl;
//            }
//            //else {
//            //    consistent_count++;
//            //    cout << "结果一致: " << output_filename << endl;
//
//            //    // 保存原始图像到一致目录
//            //    string img_path = (fs::path(consistent_dir) / output_filename).string();
//            //    if (imwrite(img_path, img)) {
//            //        cout << "已保存一致图像: " << img_path << endl;
//            //    }
//            //    else {
//            //        cerr << "保存一致图像失败: " << img_path << endl;
//            //    }
//
//            //    // 保存JSON结果到一致目录
//            //    string json_output_path = (fs::path(consistent_dir) / (filename + ".json")).string();
//
//            //    // 创建结果JSON
//            //    nlohmann::json result_json;
//            //    result_json["imagePath"] = output_filename;
//            //    result_json["imageWidth"] = img.cols;
//            //    result_json["imageHeight"] = img.rows;
//
//            //    // 添加检测结果
//            //    nlohmann::json shapes_json = nlohmann::json::array();
//            //    for (const auto& det : detections) {
//            //        nlohmann::json shape_json;
//            //        shape_json["label"] = det.className;
//            //        shape_json["score"] = det.confidence;
//
//            //        // 转换矩形为点集
//            //        nlohmann::json points_json = nlohmann::json::array();
//            //        points_json.push_back({ det.box.x, det.box.y });
//            //        points_json.push_back({ det.box.x + det.box.width, det.box.y });
//            //        points_json.push_back({ det.box.x + det.box.width, det.box.y + det.box.height });
//            //        points_json.push_back({ det.box.x, det.box.y + det.box.height });
//
//            //        shape_json["points"] = points_json;
//            //        shape_json["shape_type"] = "rectangle";
//            //        shapes_json.push_back(shape_json);
//            //    }
//
//            //    result_json["shapes"] = shapes_json;
//
//            //    saveJSONToFile1(result_json, json_output_path);
//            //    cout << "已保存一致JSON: " << json_output_path << endl;
//            //}
//        }
//    }
//
//    // 输出总体统计信息
//    cout << "=== 验证完成 ===" << endl;
//    cout << "一致文件数量: " << consistent_count << endl;
//    cout << "不一致文件数量: " << inconsistent_count << endl;
//
//    return 0;
//}