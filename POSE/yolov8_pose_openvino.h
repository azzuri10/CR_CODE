#pragma once

#include <opencv2/opencv.hpp>
#include <openvino/openvino.hpp>


class YOLOv8PoseDetector {
public:
	void initConfig(std::string onnxpath, float score_threshold);
	void detect(cv::Mat & frame);
private:
	float confidence_threshold = 0.4;
	float score_threshold = 0.25;
	std::string input_name = "images";
	std::string out_name = "output";
	ov::InferRequest infer_request;
	std::vector<cv::Scalar> color_tables;
};