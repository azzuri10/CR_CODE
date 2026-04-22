#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <opencv2/opencv.hpp>
#include "NvInfer.h"
#include <cuda_runtime_api.h>
#include <cuda.h>

using namespace nvinfer1;
using namespace cv;

struct DetectResult {
	int classId;
	float conf;
	cv::Rect box;
};

class YOLOv8TRTPose {
public:
	void initConfig(std::string enginefile, float conf_thresholod);
	void detect(cv::Mat &frame, std::vector<DetectResult> &results);
	~YOLOv8TRTPose();
private:
	float conf_thresholod = 0.25;
	int input_h = 640;
	int input_w = 640;
	int output_h;
	int output_w;
	IRuntime* runtime{ nullptr };
	ICudaEngine* engine{ nullptr };
	IExecutionContext* context{ nullptr };
	void* buffers[2] = { NULL, NULL };
	std::vector<float> prob;
	cudaStream_t stream;
	std::vector<cv::Scalar> color_tables;
};
