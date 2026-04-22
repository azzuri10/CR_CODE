//#include "HeaderDefine.h"
//#include <json.hpp>
//
//
//#include "InspBottleNum.h"
//
//using json = nlohmann::json;
//namespace fs = std::filesystem;
//
//
//// ===================== UTF8 工具函数 =====================
//nlohmann::json loadJsonUTF8(const std::string& filePath) {
//    std::ifstream ifs(filePath, std::ios::binary);
//    if (!ifs.is_open()) {
//        throw std::runtime_error("无法打开文件: " + filePath);
//    }
//    nlohmann::json j;
//    ifs >> j;
//    return j;
//}
//
//// ===================== IoU 计算 =====================
//double IoU(const cv::Rect& a, const cv::Rect& b) {
//    int x1 = MAX(a.x, b.x);
//    int y1 = MAX(a.y, b.y);
//    int x2 = MIN(a.x + a.width, b.x + b.width);
//    int y2 = MIN(a.y + a.height, b.y + b.height);
//
//    int interArea = MAX(0, x2 - x1) * MAX(0, y2 - y1);
//    int unionArea = a.area() + b.area() - interArea;
//    return unionArea > 0 ? (double)interArea / unionArea : 0.0;
//}
//
//// ===================== 解析 JSON =====================
//std::vector<std::pair<cv::Rect, std::string>> parseJsonObjects(const json& j) {
//
//    Common COM;
//    std::vector<std::pair<cv::Rect, std::string>> objects;
//    if (j.contains("shapes")) {
//        for (auto& s : j["shapes"]) {
//            auto pts = s["points"];
//            if (pts.size() < 2) continue; // 至少要两个点
//
//            int minX = INT_MAX, minY = INT_MAX;
//            int maxX = INT_MIN, maxY = INT_MIN;
//
//            // 遍历所有点，取 bounding box
//            for (auto& p : pts) {
//                int x = static_cast<int>(p[0]);
//                int y = static_cast<int>(p[1]);
//                minX = std::min(minX, x);
//                minY = std::min(minY, y);
//                maxX = std::max(maxX, x);
//                maxY = std::max(maxY, y);
//            }
//
//            cv::Rect rect(minX, minY, maxX - minX, maxY - minY);
//
//            // label（兼容中文）
//            std::string label = COM.UTF8ToGBK(s["label"].get<std::string>());
//
//            objects.push_back({ rect, label });
//        }
//    }
//    return objects;
//}
//
//
//
//// ===================== 保存 NG 结果 =====================
//void saveNGMarked(const cv::Mat& img,
//    const std::vector<FinsObject>& ngObjects,
//    const std::string& savePath) {
//    cv::Mat vis = img.clone();
//    //for (auto& obj : ngObjects) {
//    //    cv::rectangle(vis, obj.box, cv::Scalar(0, 0, 255), 2);
//    //    std::string text = obj.className + " (" + std::to_string(obj.confidence) + ")";
//    //    cv::putText(vis, text, obj.box.tl() + cv::Point(0, -5),
//    //        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
//    //}
//    cv::imwrite(savePath, vis);
//}
//
//void saveNGJson(const json& j,
//    const std::vector<FinsObject>& badOnes,
//    const std::string& savePath) {
//    json newJson = j;
//    if (newJson.contains("shapes")) {
//        for (auto& s : newJson["shapes"]) {
//            auto pts = s["points"];
//            cv::Rect rect(cv::Point((int)pts[0][0], (int)pts[0][1]),
//                cv::Point((int)pts[1][0], (int)pts[1][1]));
//            std::string label = s["label"];
//
//            for (auto& bad : badOnes) {
//                if (label == bad.className && IoU(rect, bad.box) > 0.5) {
//                    s["status"] = "NG"; // 增加标记
//                }
//            }
//        }
//    }
//
//    std::ofstream ofs(savePath);
//    ofs << newJson.dump(4); // 缩进4个空格，美观一些
//}
//
//// ===================== 主流程 =====================
//void processFolder(const std::string& root, const std::string& saveFolder, int cameraId) {
//    std::string okDir = saveFolder + "/OK/";
//    std::string ngDir = saveFolder + "/NG/";
//    fs::create_directories(okDir);
//    fs::create_directories(ngDir);
//
//    int jobId = 0;
//    for (auto& p : fs::recursive_directory_iterator(root)) {
//        if (p.path().extension() == ".jpg" || p.path().extension() == ".png") {
//            std::string imgPath = p.path().string();
//            std::filesystem::path jsonPath = p.path();
//            jsonPath.replace_extension(".json");
//            std::string jsonPathStr = jsonPath.string();
//
//            if (!fs::exists(jsonPath)) continue;
//
//            cv::Mat img = cv::imread(imgPath);
//            if (img.empty()) {
//                std::cerr << "无法读取图像: " << imgPath << std::endl;
//                continue;
//            }
//
//            // ================= 调用 InspBottleNum =================
//			auto start = std::chrono::high_resolution_clock::now();
//			double time0 = static_cast<double>(cv::getTickCount());
//			InspBottleNumOut outInfo;
//			outInfo.system.cameraId = cameraId;
//			outInfo.system.jobId = jobId;
//			char bufLog[100];
//			sprintf(bufLog, "BottleNum/camera_%d/", outInfo.system.cameraId);
//			char bufConfig[100];
//			sprintf(bufConfig, "/InspBottleNumConfig_%d.txt", outInfo.system.cameraId);
//			outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
//			outInfo.paths.intermediateImagesDir =
//				ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
//			outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
//			outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
//			outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
//			outInfo.paths.configFile = ProjectConstants::CONFIG_PATH + std::string(bufConfig);
//			outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";
//
//			outInfo.status.errorMessage = "OK";
//			outInfo.status.statusCode= BOTTLENUM_RETURN_OK;
//			outInfo.status.logs.reserve(100);
//
//            InspBottleNum* pInspBottleNum = new InspBottleNum(ProjectConstants::CONFIG_PATH, img, cameraId, jobId, true, 1000000, outInfo);
//            pInspBottleNum->SetStartTimePoint(start);
//            int rv = pInspBottleNum->BottleNum_Main(outInfo);
//			delete pInspBottleNum;
//
//
//            jobId++;
//
//            // ================= 加载 JSON =================
//            json j = loadJsonUTF8(jsonPathStr);
//            auto gtObjects = parseJsonObjects(j);
//
//            
//
//            // ================= 对比 =================
//            bool allMatch = true;
//            std::vector<FinsObject> badOnes;
//
//            for (auto& detObj : outInfo.locate.details) {
//                bool matched = false;
//                for (auto& [gtRect, gtLabel] : gtObjects) {
//                    if (detObj.className == gtLabel && IoU(detObj.box, gtRect) > 0.45) {
//                        matched = true;
//                        break;
//                    }
//                }
//                if (!matched) {
//                    allMatch = false;
//                    badOnes.push_back(detObj);
//                }
//            }
//
//            // ================= 存放结果 =================
//            if (allMatch) {
//                /*fs::copy(imgPath, okDir + fs::path(imgPath).filename().string(),
//                    fs::copy_options::overwrite_existing);
//                fs::copy(jsonPathStr, okDir + fs::path(jsonPathStr).filename().string(),
//                    fs::copy_options::overwrite_existing);
//                std::cout << "[OK] " << imgPath << std::endl;*/
//            }
//            else {
//                std::string ngImgPath = ngDir + fs::path(imgPath).filename().string();
//                std::string ngJsonPath = ngDir + fs::path(jsonPathStr).filename().string();
//
//                saveNGMarked(img, badOnes, ngImgPath);
//                saveNGJson(j, badOnes, ngJsonPath);
//                std::cout << "[NG] " << imgPath << " (问题目标数: " << badOnes.size() << ")" << std::endl;
//            }
//
//            //cv::namedWindow("img", cv::WINDOW_NORMAL);
//            //cv::imshow("img", outInfo.images.outputImg.mat());
//            //cv::waitKey(0);
//        }
//    }
//}
//
//// ===================== 主入口 =====================
//int main() {
//    std::string rootFolder = "F:/TRAIN_SAMPLE_3.0/BOX/ALL/2RW-BAD";  // 修改成你数据集的根目录
//    std::string saveFolder = "F:/TRAIN_SAMPLE_3.0/BOX/ALL/2RW-BAD-CHECK";  // 修改成你数据集的根目录
//    fs::create_directories(saveFolder);
//    int cameraId = 0; // 相机编号
//
//    processFolder(rootFolder, saveFolder, cameraId);
//
//    return 0;
//}
