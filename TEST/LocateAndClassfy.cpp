//#include <opencv2/opencv.hpp>
//#include <opencv2/dnn.hpp>
//#include <iostream>
//#include <vector>
//#include <filesystem>
//#include <map>
//#include "write_json.h"  // 包含JSON生成函数
//#include "InferenceWorker.h"
//namespace fs = std::filesystem;
//
//int main() {
//    // 设置输入和输出目录
//    string input_dir = "G:/IMG/面包/颜色浅20250911";  // 输入文件夹路径
//    string output_base_dir = input_dir + "-XXXX";  // 输出基础文件夹路径
//
//    // 定义目标检测类别（单类型定位）
//    vector<string> detection_classes = { "bread" };  // 假设只有一个类别"bread"
//
//    // 定义分类类别
//    vector<string> classification_classes = { "OK", "颜色浅", "颜色深" };
//
//    // 模型路径
//    string detection_model_path = "detection_model.onnx";  // 目标检测模型
//    string classification_model_path = "classification_model1.onnx";  // 分类模型
//
//    // 置信度和NMS阈值
//    float conf_threshold = 0.1;
//    float nms_threshold = 0.1;
//
//    // 相机ID（根据实际情况设置）
//    int cameraId = 0;
//
//    // 创建类别映射
//    map<int, string> class_map = {
//        {0, "OK"},
//        {1, "颜色浅"},
//        {2, "颜色深"}
//    };
//
//    // 检查并创建输出基础目录（如果不存在）
//    if (!fs::exists(output_base_dir)) {
//        if (!fs::create_directories(output_base_dir)) {
//            cerr << "无法创建基础输出目录: " << output_base_dir << endl;
//            return -1;
//        }
//        cout << "已创建基础输出目录: " << output_base_dir << endl;
//    }
//
//    // 创建类别输出目录（如果不存在）
//    for (const auto& [id, name] : class_map) {
//        fs::path class_dir = fs::path(output_base_dir) / name;
//        if (!fs::exists(class_dir)) {
//            if (!fs::create_directories(class_dir)) {
//                cerr << "无法创建目录: " << class_dir << endl;
//                return -1;
//            }
//            cout << "已创建目录: " << class_dir << endl;
//        }
//    }
//
//    // 创建未知类别目录（如果不存在）
//    fs::path unknown_dir = fs::path(output_base_dir) / "未知";
//    if (!fs::exists(unknown_dir)) {
//        if (!fs::create_directories(unknown_dir)) {
//            cerr << "无法创建目录: " << unknown_dir << endl;
//            return -1;
//        }
//        cout << "已创建目录: " << unknown_dir << endl;
//    }
//
//    // 检查输入目录是否存在
//    if (!fs::exists(input_dir)) {
//        cerr << "输入目录不存在: " << input_dir << endl;
//        return -1;
//    }
//
//    // 获取所有图像文件路径
//    vector<string> image_paths;
//    for (const auto& entry : fs::directory_iterator(input_dir)) {
//        if (entry.is_regular_file()) {
//            string ext = entry.path().extension().string();
//            transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
//            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
//                image_paths.push_back(entry.path().string());
//            }
//        }
//    }
//
//    if (image_paths.empty()) {
//        cerr << "未找到支持的图像文件!" << endl;
//        return -1;
//    }
//
//    cout << "找到 " << image_paths.size() << " 张图像" << endl;
//
//    // 处理每张图像
//    for (const string& image_path : image_paths) {
//        Mat img = imread(image_path);
//        if (img.empty()) {
//            cerr << "无法读取图像: " << image_path << endl;
//            continue;
//        }
//
//        // 第一步：使用目标检测模型定位目标
//        vector<FinsObject> detections = InferenceWorker::Run(
//            cameraId, detection_model_path, detection_classes, img, conf_threshold, nms_threshold
//        );
//
//        // 绘制检测结果
//        Mat result_img = img.clone();
//        map<int, bool> has_detection; // 记录检测到的类别
//
//        vector<FinsObject> final_detections;
//
//        // 第二步：对每个检测到的目标进行分类
//        for (auto& det : detections) {
//            // 裁剪检测区域
//            cv::Rect roi_rect(
//                max(0, static_cast<int>(det.box.x - det.box.width / 2)),
//                max(0, static_cast<int>(det.box.y - det.box.height / 2)),
//                min(img.cols - 1, static_cast<int>(det.box.width)),
//                min(img.rows - 1, static_cast<int>(det.box.height))
//            );
//
//            if (roi_rect.width > 0 && roi_rect.height > 0) {
//                cv::Mat roi = img(det.box).clone();
//				cv::namedWindow("roi", cv::WINDOW_NORMAL);
//				cv::imshow("roi", roi);
//				cv::waitKey(1);
//                // 使用分类模型进行分类
//                FinsClassification classification = InferenceWorker::RunClassification(
//                    cameraId, classification_model_path, classification_classes, roi, conf_threshold
//                );
//
//                // 更新检测结果的类别
//                det.className = classification.className;
//                det.confidence = classification.confidence;
//
//                // 查找类别ID
//                int class_id = -1;
//                for (int i = 0; i < classification_classes.size(); i++) {
//                    if (classification_classes[i] == classification.className) {
//                        class_id = i;
//                        break;
//                    }
//                }
//
//                if (class_id != -1) {
//                    // 记录检测到的类别
//                    has_detection[class_id] = true;
//                    det.className = classification.className;
//                }
//
//                //// 绘制检测框和分类结果
//                //cv::rectangle(result_img, roi_rect, cv::Scalar(0, 255, 0), 2);
//                //std::string label = cv::format("%s: %.2f", det.className.c_str(), det.confidence);
//                //cv::putText(result_img, label, cv::Point(roi_rect.x, roi_rect.y - 5),
//                //    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
//            }
//
//            final_detections.push_back(det);
//        }
//
//        // 保存图像到对应类别文件夹
//        string filename = fs::path(image_path).filename().string();
//
//        json json_data = generateXAnyLabelingJSON(final_detections, filename, img.rows, img.cols);
//
//        // 保存图像和JSON到对应类别文件夹
//        if (!final_detections.empty()) {
//            for (const auto& [class_id, detected] : has_detection) {
//                if (detected) {
//                    fs::path output_dir = fs::path(output_base_dir) / class_map[class_id];
//                    fs::path img_output_path = output_dir / filename;
//                    fs::path json_output_path = output_dir / (fs::path(filename).stem().string() + ".json");
//
//                    // 保存图像
//                    if (imwrite(img_output_path.string(), result_img)) {
//                        cout << "已保存图像: " << img_output_path << endl;
//                    }
//                    else {
//                        cerr << "保存图像失败: " << img_output_path << endl;
//                    }
//
//                    // 保存JSON
//                    saveJSONToFile(json_data, json_output_path.string());
//                    cout << "已保存JSON: " << json_output_path << endl;
//                   
//                }
//            }
//        }
//        else {
//            // 未检测到任何目标，保存到未知文件夹
//            fs::path img_output_path = unknown_dir / filename;
//            fs::path json_output_path = unknown_dir / (fs::path(filename).stem().string() + ".json");
//
//            // 保存图像
//            if (imwrite(img_output_path.string(), result_img)) {
//                cout << "未检测到目标，已保存图像到未知文件夹: " << img_output_path << endl;
//            }
//            else {
//                cerr << "保存图像失败: " << img_output_path << endl;
//            }
//
//            // 保存JSON（即使没有检测结果也保存空JSON）
//            saveJSONToFile(json_data, json_output_path.string());
//            cout << "已保存JSON到未知文件夹: " << json_output_path << endl;
//        }
//    }
//
//    cout << "处理完成! 所有结果已保存到: " << output_base_dir << endl;
//    return 0;
//}