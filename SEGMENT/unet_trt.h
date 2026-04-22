#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <opencv2/opencv.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudawarping.hpp>
#include <opencv2/cudafilters.hpp>
#include <opencv2/cudaarithm.hpp>
#include "NvInfer.h"

using namespace nvinfer1;
using namespace cv;


class UNetTRTSegment {
public:
	void initConfig(std::string enginefile);
	void segment(cv::Mat &frame);
	~UNetTRTSegment();
private:
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
};