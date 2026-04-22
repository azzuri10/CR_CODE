#pragma once

#include <opencv2/opencv.hpp>

class YOLOv8PoseDetector {
public:
	void initConfig(std::string onnxpath, float score_threshold, int input_w, int input_h);
	void detect(cv::Mat & frame, std::vector<std::string> &classNames);
private:
	float score_threshold = 0.25;
	int input_w = 640;
	int input_h = 640;
	cv::dnn::Net net;
	std::vector<cv::Scalar> color_tables;
};