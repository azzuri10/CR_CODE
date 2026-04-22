# include "unet_trt.h"
class Logger : public ILogger
{
	void log(Severity severity, const char* msg)  noexcept
	{
		// suppress info-level messages
		if (severity != Severity::kINFO)
			std::cout << msg << std::endl;
	}
} gLogger;

UNetTRTSegment::~UNetTRTSegment() {
	// 同步结束，释放资源
	cudaStreamSynchronize(stream);
	cudaStreamDestroy(stream);

	if (!context) {
		context->destroy();
	}
	if (!engine) {
		engine->destroy();
	}
	if (!runtime) {
		runtime->destroy();
	}
	if (!buffers[0]) {
		delete[] buffers;
	}
}

void UNetTRTSegment::initConfig(std::string enginefile) {
	std::ifstream file(enginefile, std::ios::binary);
	char* trtModelStream = NULL;
	int size = 0;
	if (file.good()) {
		file.seekg(0, file.end);
		size = file.tellg();
		file.seekg(0, file.beg);
		trtModelStream = new char[size];
		assert(trtModelStream);
		file.read(trtModelStream, size);
		file.close();
	}

	// 初始化几个对象
	this->runtime = createInferRuntime(gLogger);
	assert(this->runtime != nullptr);
	this->engine = runtime->deserializeCudaEngine(trtModelStream, size);
	assert(this->engine != nullptr);
	this->context = engine->createExecutionContext();
	assert(this->context != nullptr);
	delete[] trtModelStream;

	int input_index = engine->getBindingIndex("input.1");
	int output_index = engine->getBindingIndex("203");

	// 获取输入维度信息 NCHW=1x3x640x640
	this->input_h = engine->getBindingDimensions(input_index).d[2];
	this->input_w = engine->getBindingDimensions(input_index).d[3];
	printf("inputH : %d, inputW: %d \n", input_h, input_w);

	// 获取输出维度信息
	this->output_h = engine->getBindingDimensions(output_index).d[2];
	this->output_w = engine->getBindingDimensions(output_index).d[3];
	std::cout << "out data format: " << output_h << "x" << output_w << std::endl;

	// 创建GPU显存输入/输出缓冲区
	std::cout << " input/outpu : " << engine->getNbBindings() << std::endl;
	cudaMalloc(&buffers[input_index], this->input_h * this->input_w * sizeof(float));
	cudaMalloc(&buffers[output_index], 2 * this->output_h *this->output_w * sizeof(float));

	// 创建临时缓存输出
	prob.resize(2 * this->output_h *this->output_w);

	// 创建cuda流
	cudaStreamCreate(&stream);
}

void UNetTRTSegment::segment(cv::Mat &frame) {
	int64 start = cv::getTickCount();

	cv::Mat rgb, blob;
	cv::cvtColor(frame, rgb, cv::COLOR_BGR2GRAY);
	cv::resize(rgb, blob, cv::Size(this->input_w, this->input_h));
	blob.convertTo(blob, CV_32F);
	blob = blob / 255.0;

	//cv::cuda::GpuMat gpu_frame, gpu_gray, blob;
	//gpu_frame.upload(frame);
	//cv::cuda::cvtColor(gpu_frame, gpu_gray, cv::COLOR_BGR2GRAY);
	//cv::cuda::resize(gpu_gray, blob, cv::Size(this->input_w, this->input_h));
	//cv::cuda::GpuMat tensor = cv::cuda::GpuMat(cv::Size(input_w, input_h), CV_32FC1, (float*)buffers[0] + input_w * input_h);
	//blob.convertTo(tensor, CV_32FC1, 1.0/255.0);

	// 内存到GPU显存
	cudaMemcpyAsync(buffers[0], blob.ptr<float>(), input_h * input_w * sizeof(float), cudaMemcpyHostToDevice, stream);

	// 推理
	context->enqueueV2(buffers, stream, nullptr);

	// GPU显存到内存
	cudaMemcpyAsync(prob.data(), buffers[1], 2 * output_h *output_w * sizeof(float), cudaMemcpyDeviceToHost, stream);

	// 后处理
	const float* det_output = (float*)prob.data();
	cv::Mat result(cv::Size(this->output_w, this->output_h), CV_32FC1);
	int t_pixels = this->output_h * this->output_w;
	for (int row = 0; row < output_h; row++) {
		int current_row = row * this->output_w;
		for (int col = 0; col < output_w; col++) {
			float c1 = det_output[current_row + col];
			float c2 = det_output[t_pixels + current_row + col];
			if (c1 > c2) {
				result.at<float>(row, col) = 0;
			}
			else {
				result.at<float>(row, col) = 1.0;
			}
		}
	}
	result = result * 255;
	result.convertTo(result, CV_8U);
	cv::resize(result, result, frame.size());

	std::vector<std::vector<cv::Point>> contours;
	std::vector<cv::Vec4i> hierarchy;
	cv::findContours(result, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE, cv::Point());
	cv::drawContours(frame, contours, -1, cv::Scalar(0, 0, 255), -1, 8);

	// 计算FPS render it
	float t = (cv::getTickCount() - start) / static_cast<float>(cv::getTickFrequency());
	putText(frame, cv::format("FPS: %.2f", 1.0 / t), cv::Point(20, 40), cv::FONT_HERSHEY_PLAIN, 2.0, cv::Scalar(255, 0, 0), 2, 8);
}
