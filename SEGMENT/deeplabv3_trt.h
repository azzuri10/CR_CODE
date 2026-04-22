#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <opencv2/opencv.hpp>
#include "NvInfer.h"

using namespace nvinfer1;
using namespace cv;


class Deeplabv3TRTSegment {
public:
	void initConfig(std::string enginefile);
	void segment(cv::Mat &frame);
	~Deeplabv3TRTSegment();
private:
	int input_h = 720;
	int input_w = 1080;
	int output_h;
	int output_w;
	IRuntime* runtime{ nullptr };
	ICudaEngine* engine{ nullptr };
	IExecutionContext* context{ nullptr };
	void* buffers[3] = { NULL, NULL, NULL };
	std::vector<float> prob;
	cudaStream_t stream;
};