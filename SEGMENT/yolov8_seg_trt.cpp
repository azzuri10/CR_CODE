# include "yolov8_seg_trt.h"
class Logger : public ILogger
{
	void log(Severity severity, const char* msg)  noexcept
	{
		// suppress info-level messages
		if (severity != Severity::kINFO)
			std::cout << msg << std::endl;
	}
} gLogger;

YOLOv8TRTSegment::~YOLOv8TRTSegment() {
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

float YOLOv8TRTSegment::sigmoid_function(float a)
{
	float b = 1. / (1. + exp(-a));
	return b;
}

void YOLOv8TRTSegment::initConfig(std::string enginefile, float conf_thresholod, float score_thresholod) {
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

	int input_index = engine->getBindingIndex("images");
	int output_index = engine->getBindingIndex("output0");
	int mask_index = engine->getBindingIndex("output1");

	// 获取输入维度信息 NCHW=1x3x640x640
	this->input_h = engine->getBindingDimensions(input_index).d[2];
	this->input_w = engine->getBindingDimensions(input_index).d[3];
	printf("inputH : %d, inputW: %d \n", input_h, input_w);

	// 获取输出维度信息
	this->output_h = engine->getBindingDimensions(output_index).d[1];
	this->output_w = engine->getBindingDimensions(output_index).d[2];
	std::cout << "out data format: " << output_h << "x" << output_w << std::endl;

	// 创建GPU显存输入/输出缓冲区
	std::cout << " input/outpu : " << engine->getNbBindings() << std::endl;
	cudaMalloc(&buffers[input_index], this->input_h * this->input_w * 3 * sizeof(float));
	cudaMalloc(&buffers[output_index], this->output_h *this->output_w * sizeof(float));
	cudaMalloc(&buffers[mask_index], 32 * 25600 * sizeof(float));

	// 创建临时缓存输出
	prob.resize(output_h * output_w);
	mprob.resize(32 * 25600);

	// 创建cuda流
	cudaStreamCreate(&stream);
}

void YOLOv8TRTSegment::detect(cv::Mat &frame, std::vector<DetectResult> &results) {
	float sx = 160.0f / 640.0f;
	float sy = 160.0f / 640.0f;
	cv::RNG rng;
	int64 start = cv::getTickCount();
	// 图象预处理 - 格式化操作
	int w = frame.cols;
	int h = frame.rows;
	int _max = std::max(h, w);
	cv::Mat image = cv::Mat::zeros(cv::Size(_max, _max), CV_8UC3);
	cv::Rect roi(0, 0, w, h);
	frame.copyTo(image(roi));
	// HWC => CHW
	float x_factor = image.cols / static_cast<float>(this->input_w);
	float y_factor = image.rows / static_cast<float>(this->input_h);
	cv::Mat tensor = cv::dnn::blobFromImage(image, 1.0f / 225.f, cv::Size(input_w, input_h), cv::Scalar(), true);

	// 内存到GPU显存
	cudaMemcpyAsync(buffers[0], tensor.ptr<float>(), input_h * input_w * 3 * sizeof(float), cudaMemcpyHostToDevice, stream);

	// 推理
	context->enqueueV2(buffers, stream, nullptr);

	// GPU显存到内存
	cudaMemcpyAsync(prob.data(), buffers[2], output_h *output_w * sizeof(float), cudaMemcpyDeviceToHost, stream);
	cudaMemcpyAsync(mprob.data(), buffers[1], 32 * 25600 * sizeof(float), cudaMemcpyDeviceToHost, stream);
	
	// 后处理, 1x84x8400
	std::vector<cv::Rect> boxes;
	std::vector<int> classIds;
	std::vector<float> confidences;
	std::vector<cv::Mat> masks;
	cv::Mat dout(output_h, output_w, CV_32F, (float*)prob.data());
	cv::Mat mask1(32, 25600, CV_32F, (float*)mprob.data());
	cv::Mat det_output = dout.t();
	for (int i = 0; i < det_output.rows; i++) {
		cv::Mat classes_scores = det_output.row(i).colRange(4, output_h-32);
		cv::Point classIdPoint;
		double score;
		minMaxLoc(classes_scores, 0, &score, 0, &classIdPoint);

		// 置信度 0～1之间
		if (score > this->score_thresholod)
		{
			float cx = det_output.at<float>(i, 0);
			float cy = det_output.at<float>(i, 1);
			float ow = det_output.at<float>(i, 2);
			float oh = det_output.at<float>(i, 3);
			int x = static_cast<int>((cx - 0.5 * ow) * x_factor);
			int y = static_cast<int>((cy - 0.5 * oh) * y_factor);
			int width = static_cast<int>(ow * x_factor);
			int height = static_cast<int>(oh * y_factor);
			cv::Rect box;
			box.x = x;
			box.y = y;
			box.width = width;
			box.height = height;
			cv::Mat mask2 = det_output.row(i).colRange(output_h - 32, output_h);
			masks.push_back(mask2);
			boxes.push_back(box);
			classIds.push_back(classIdPoint.x);
			confidences.push_back(score);
		}
	}

	// NMS
	std::vector<int> indexes;
	cv::dnn::NMSBoxes(boxes, confidences, 0.25, 0.45, indexes);
	cv::Mat rgb_mask = cv::Mat::zeros(frame.size(), frame.type());
	for (size_t i = 0; i < indexes.size(); i++) {
		DetectResult dr;
		int index = indexes[i];
		int idx = classIds[index];
		dr.box = boxes[index];
		dr.classId = idx;
		dr.conf = confidences[index];

		cv::Rect box = boxes[index];
		int x1 = std::max(0, box.x);
		int y1 = std::max(0, box.y);
		int x2 = std::max(0, box.br().x);
		int y2 = std::max(0, box.br().y);
		cv::Mat m2 = masks[index];
		cv::Mat m = m2 * mask1;
		for (int col = 0; col < m.cols; col++) {
			m.at<float>(0, col) = sigmoid_function(m.at<float>(0, col));
		}
		cv::Mat m1 = m.reshape(1, 160);
		int mx1 = std::max(0, int((x1 * sx) / x_factor));
		int mx2 = std::max(0, int((x2 * sx) / x_factor));
		int my1 = std::max(0, int((y1 * sy) / y_factor));
		int my2 = std::max(0, int((y2 * sy) / y_factor));
		cv::Mat mask_roi = m1(cv::Range(my1, my2), cv::Range(mx1, mx2));
		cv::Mat rm, det_mask;
		cv::resize(mask_roi, rm, cv::Size(x2 - x1, y2 - y1));
		for (int r = 0; r < rm.rows; r++) {
			for (int c = 0; c < rm.cols; c++) {
				float pv = rm.at<float>(r, c);
				if (pv > 0.5) {
					rm.at<float>(r, c) = 1.0;
				}
				else {
					rm.at<float>(r, c) = 0.0;
				}
			}
		}
		rm = rm * rng.uniform(0, 255);
		rm.convertTo(det_mask, CV_8UC1);
		if ((y1 + det_mask.rows) >= frame.rows) {
			y2 = frame.rows - 1;
		}
		if ((x1 + det_mask.cols) >= frame.cols) {
			x2 = frame.cols - 1;
		}
		// std::cout << "x1: " << x1 << " x2:" << x2 << " y1: " << y1 << " y2: " << y2 << std::endl;
		cv::Mat mask = cv::Mat::zeros(cv::Size(frame.cols, frame.rows), CV_8UC1);
		det_mask(cv::Range(0, y2 - y1), cv::Range(0, x2 - x1)).copyTo(mask(cv::Range(y1, y2), cv::Range(x1, x2)));
		add(rgb_mask, cv::Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255)), rgb_mask, mask);

		cv::rectangle(frame, boxes[index], cv::Scalar(0, 0, 255), 1, 8);
		cv::rectangle(frame, cv::Point(boxes[index].tl().x, boxes[index].tl().y - 20),
			cv::Point(boxes[index].br().x, boxes[index].tl().y), cv::Scalar(0, 255, 255), -1);
		results.push_back(dr);
	}

	// 计算FPS render it
	float t = (cv::getTickCount() - start) / static_cast<float>(cv::getTickFrequency());
	cv::addWeighted(frame, 0.6, rgb_mask, 0.4, 0, frame);
	putText(frame, cv::format("FPS: %.2f", 1.0 / t), cv::Point(20, 40), cv::FONT_HERSHEY_PLAIN, 2.0, cv::Scalar(255, 0, 0), 2, 8);
}
