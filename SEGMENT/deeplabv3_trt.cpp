# include "deeplabv3_trt.h"
class Logger : public ILogger
{
	void log(Severity severity, const char* msg)  noexcept
	{
		// suppress info-level messages
		if (severity != Severity::kINFO)
			std::cout << msg << std::endl;
	}
} gLogger;

Deeplabv3TRTSegment::~Deeplabv3TRTSegment() {
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

void Deeplabv3TRTSegment::initConfig(std::string enginefile) {
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

	int input_index = engine->getBindingIndex("input");
	int output_index = engine->getBindingIndex("out");
	int aux_index = engine->getBindingIndex("aux");
	std::cout << "output_index: " << output_index << " aux_index: " << aux_index << std::endl;

	// 获取输入维度信息 NCHW=1x3x720x1080
	this->input_h = engine->getBindingDimensions(input_index).d[2];
	this->input_w = engine->getBindingDimensions(input_index).d[3];
	printf("inputH : %d, inputW: %d \n", input_h, input_w);

	// 获取输出维度信息
	this->output_h = this->input_h;
	this->output_w = this->input_w;
	int cn = 21;
	std::cout << "out data format: " << output_h << "x" << output_w << std::endl;

	// 创建GPU显存输入/输出缓冲区
	std::cout << " input/outpu : " << engine->getNbBindings() << std::endl;
	cudaMalloc(&buffers[input_index], 3 * this->input_h * this->input_w * sizeof(float));
	cudaMalloc(&buffers[output_index], cn * this->output_h *this->output_w * sizeof(float));
	cudaMalloc(&buffers[aux_index], cn * this->output_h *this->output_w * sizeof(float));
	
	// 创建临时缓存输出
	prob.resize(cn * this->output_h *this->output_w);

	// 创建cuda流
	cudaStreamCreate(&stream);
}

void Deeplabv3TRTSegment::segment(cv::Mat &frame) {

	cv::RNG rng;
	std::vector<cv::Vec3b> color_tab;
	color_tab.emplace_back(cv::Vec3b(0, 0, 0)); // background
	for (int i = 1; i < 21; i++) {
		color_tab.emplace_back(cv::Vec3b(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255)));
	}

	int64 start = cv::getTickCount();

	cv::Mat rgb, blob;
	cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
	cv::resize(rgb, blob, cv::Size(this->input_w, this->input_h));
	blob.convertTo(blob, CV_32F);
	blob = blob / 255.0;
	cv::subtract(blob, cv::Scalar(0.485, 0.456, 0.406), blob);
	cv::divide(blob, cv::Scalar(0.229, 0.224, 0.225), blob);

	// HWC => CHW
	cv::Mat tensor = ::dnn::blobFromImage(blob);

	// 内存到GPU显存
	cudaMemcpyAsync(buffers[0], tensor.ptr<float>(), 3 * input_h * input_w * sizeof(float), cudaMemcpyHostToDevice, stream);

	// 推理
	context->enqueueV2(buffers, stream, nullptr);

	// GPU显存到内存
	cudaMemcpyAsync(prob.data(), buffers[1], 21 * output_h *output_w * sizeof(float), cudaMemcpyDeviceToHost, stream);

	// 后处理
	int step = output_h * output_w;
	const float* mask_data = (float*)prob.data();
	cv::Mat result = cv::Mat::zeros(cv::Size(output_w, output_h), CV_8UC3);
	for (int row = 0; row < output_h; row++) {
		for (int col = 0; col < output_w; col++) {
			int max_index = 0;
			float max_porb = mask_data[row*output_w + col];
			for (int cn = 1; cn < 21; cn++) {
				float prob = mask_data[cn*step + row * output_w + col];
				if (prob > max_porb) {
					max_porb = prob;
					max_index = cn;
				}
			}
			result.at<cv::Vec3b>(row, col) = color_tab[max_index];
		}
	}
	Mat res;
	cv::resize(result, res, frame.size());
	cv::addWeighted(frame, 0.7, res, 0.3, 0, frame);

	// 计算FPS render it
	float t = (cv::getTickCount() - start) / static_cast<float>(cv::getTickFrequency());
	putText(frame, cv::format("FPS: %.2f", 1.0 / t), cv::Point(20, 40), cv::FONT_HERSHEY_PLAIN, 2.0, cv::Scalar(255, 0, 0), 2, 8);
}
