//#include "HeaderDefine.h"
//#include <json.hpp>  // 使用nlohmann/json库[2,6](@ref)
//#include <filesystem> // 新增文件系统库[6](@ref)
//namespace fs = std::filesystem;
//using json = nlohmann::json;
//
//struct AnnotatedRect {
//	cv::RotatedRect rect;
//	std::string label;
//};
//
//// 新增JSON解析函数
//std::vector<cv::Rect> parse_json_annotations(const std::string& json_path) {
//	std::vector<cv::Rect> rois;
//	try {
//		std::ifstream jfile(json_path);
//		json jdata = json::parse(jfile);
//
//		// 解析shapes数组中的矩形坐标[8,10](@ref)
//		for (auto& shape : jdata["shapes"]) {
//			if (shape["shape_type"] == "rectangle") {
//				auto points = shape["points"];
//				int x1 = static_cast<int>(points[0][0]);
//				int y1 = static_cast<int>(points[0][1]);
//				int x2 = static_cast<int>(points[1][0]);
//				int y2 = static_cast<int>(points[1][1]);
//
//				// 生成矩形区域[9](@ref)
//				rois.emplace_back(x1, y1, x2 - x1, y2 - y1);
//			}
//		}
//	}
//	catch (const std::exception& e) {
//		std::cerr << "JSON解析错误: " << e.what() << std::endl;
//	}
//	return rois;
//}
//
//std::vector<AnnotatedRect> parse_rotated_rects(const std::string& json_path) {
//	std::vector<AnnotatedRect> rects;
//	try {
//		std::ifstream jfile(json_path);
//		json jdata = json::parse(jfile);
//
//		for (auto& shape : jdata["shapes"]) {
//			if (shape["shape_type"] == "rotation") {
//				AnnotatedRect ar;
//				ar.label = shape.value("label", "unknown"); // 获取标注类型[3](@ref)
//
//				std::vector<cv::Point2f> points;
//				for (auto& pt : shape["points"]) {
//					points.emplace_back(pt[0], pt[1]);
//				}
//
//				if (points.size() == 4) {
//					ar.rect = cv::minAreaRect(points);
//					rects.push_back(ar);
//				}
//			}
//		}
//	}
//	catch (const std::exception& e) {
//		std::cerr << "JSON解析错误: " << e.what() << std::endl;
//	}
//	return rects;
//}
//
//cv::Mat extract_rotated_roi(const cv::Mat& src, const cv::RotatedRect& rect) {
//	cv::Mat M, rotated;
//	float angle = rect.angle;
//	cv::Size rect_size = rect.size;
//
//	// 调整角度和尺寸
//	if (angle < -45.) {
//		angle += 90.0;
//		std::swap(rect_size.width, rect_size.height);
//	}
//
//	// 计算旋转矩阵
//	M = cv::getRotationMatrix2D(rect.center, angle, 1.0);
//	cv::warpAffine(src, rotated, M, src.size(), cv::INTER_CUBIC);
//
//	// 裁剪区域
//	cv::getRectSubPix(rotated, rect_size, rect.center, rotated);
//	return rotated;
//}
//
//std::string getFileName(const std::string& filePath) {
//	size_t pos = filePath.find_last_of("/\\");
//	if (pos != std::string::npos) {
//		return filePath.substr(pos + 1);
//	}
//	return filePath;  // 如果找不到分隔符，直接返回原始路径
//}
//
//#define NUM2NUM 1
//
//int main()
//{
//	std::string IMG_PATH;
//	std::string SAVE_PATH = "F:/TRAIN_SAMPLE/CAR/BIG/OBB/";
//	std::cout << "图像文件夹路径: ";
//	//cin >> IMG_PATH;
//	std::getline(std::cin, IMG_PATH);
//
//	std::vector<std::string> imgPath;
//	cv::glob(IMG_PATH, imgPath, false);
//
//
//	//for (const auto& img_path : imgPath) {
//	//	// 生成对应JSON路径[8](@ref)
//	//	std::string json_path = img_path.substr(0, img_path.find_last_of('.')) + ".json";
//
//	//	// 解析标注信息
//	//	auto rois = parse_json_annotations(json_path);
//
//	//	cv::Mat m_img = cv::imread(img_path);
//	//	if (m_img.empty()) continue;
//
//	//	// 提取所有目标区域
//	//	int idx = 0;
//	//	for (const auto& roi : rois) {
//	//		// 边界检查[9](@ref)
//	//		if (roi.x + roi.width > m_img.cols ||
//	//			roi.y + roi.height > m_img.rows) continue;
//
//	//		cv::Mat roi_img = m_img(roi);
//
//	//		// 保存带标注名称的文件[10](@ref)
//	//		std::string save_path = "F://TRAIN_SAMPLE//CAR//BIG//OBB//" +
//	//			std::to_string(idx++) + "_" +
//	//			getFileName(img_path);
//	//		cv::imwrite(save_path, roi_img);
//	//	}
//	//}
//
//	for (const auto& img_path : imgPath) {
//		// 生成对应JSON路径
//		std::string json_path = img_path.substr(0, img_path.find_last_of('.')) + ".json";
//
//		// 读取图像
//		cv::Mat image = cv::imread(img_path);
//		if (image.empty()) continue;
//
//		// 解析标注
//		auto rects = parse_rotated_rects(json_path);
//
//		// 提取并保存每个区域
//		int index = 0;
//		std::string imgFileName = getFileName(img_path);
//		for (const auto& ar : rects) {
//			fs::path save_dir = fs::path(SAVE_PATH) / ar.label;
//			if (!fs::exists(save_dir)) {
//				fs::create_directories(save_dir); // 创建多级目录[8](@ref)
//			}
//
//			cv::Mat roi = extract_rotated_roi(image, ar.rect);
//
//			// 生成保存路径
//			std::string img_path = imgFileName.substr(0, imgFileName.find_last_of('.'));
//			std::string save_path = (save_dir / img_path).string() + "_" + std::to_string(index++) + ".jpg";
//
//			// 保存结果
//			if (!cv::imwrite(save_path, roi)) {
//				std::cerr << "保存失败: " << save_path << std::endl;
//			}
//		}
//	}
//
//}