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
//    string input_dir = "G:/IMG/面包/OK";  // 输入文件夹路径
//    string output_base_dir = input_dir + "-XXXX";  // 输出基础文件夹路径
//
//    // 定义类别
//    vector<string> classes = { "OK", "颜色深", "颜色浅" };
//
//    // 模型路径
//    string model_path = "best.onnx";
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
//        {1, "颜色深"},
//        {2, "颜色浅"}
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
//        // 调用Run函数进行推理
//        vector<FinsObject> detections = InferenceWorker::Run(
//            cameraId, model_path, classes, img, conf_threshold, nms_threshold
//        );
//        // 绘制检测结果
//        Mat result_img = img.clone();
//        map<int, bool> has_detection; // 记录检测到的类别
//
//        vector<FinsObject> detections1;
//        for (const auto& det : detections) {
//            // 记录检测到的类别
//            for (int i = 0; i < classes.size(); i++) {
//                if (classes[i] == det.className) {
//                    has_detection[i] = true;
//                    break;
//                }
//            }
//
//            // 转换为FinsObjectRotate格式
//            detections1.push_back(det);
//        }
//
//        // 保存图像到对应类别文件夹
//        string filename = fs::path(image_path).filename().string();
//
//        json json_data = generateXAnyLabelingJSON(detections1, filename, img.rows, img.cols);
//
//        // 保存图像和JSON到对应类别文件夹
//        if (!detections.empty()) {
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
