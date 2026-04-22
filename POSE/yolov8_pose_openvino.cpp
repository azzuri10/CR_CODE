#include <yolov8_pose_openvino.h>

void YOLOv8PoseDetector::initConfig(std::string onnxpath, float scored) {
	this->score_threshold = scored;
	ov::Core ie;
	std::vector<std::string> devices = ie.get_available_devices();
	for (std::string name : devices) {
		std::cout << "device name: " << name << std::endl;
	}

	ov::CompiledModel compiled_model = ie.compile_model(onnxpath, "GPU");
	this->infer_request = compiled_model.create_infer_request();

	cv::RNG rng;
	for (int i = 0; i < 17; i++) {
		int a = rng.uniform(0, 255);
		int b = rng.uniform(0, 255);
		int c = rng.uniform(0, 255);
		this->color_tables.push_back(cv::Scalar(a, b, c));
	}
}

void YOLOv8PoseDetector::detect(cv::Mat &frame) {
	// 获取输入格式
	int64 start = cv::getTickCount();
	// 请求网络输入
	ov::Tensor input_tensor = infer_request.get_input_tensor();
	ov::Shape tensor_shape = input_tensor.get_shape();
	size_t num_channels = tensor_shape[1];
	size_t input_h = tensor_shape[2];
	size_t input_w = tensor_shape[3];

	// 图象预处理 - 格式化操作
	int w = frame.cols;
	int h = frame.rows;
	int _max = std::max(h, w);
	cv::Mat image = cv::Mat::zeros(cv::Size(_max, _max), CV_8UC3);
	cv::Rect roi(0, 0, w, h);
	frame.copyTo(image(roi));

	float x_factor = image.cols / static_cast<float>(input_w);
	float y_factor = image.rows / static_cast<float>(input_h);
	cv::Mat m1 = cv::Mat::zeros(cv::Size(3, 17), CV_32FC1);
	for (int i = 0; i < 17; i++) {
		m1.at<float>(i, 0) = x_factor;
		m1.at<float>(i, 1) = y_factor;
		m1.at<float>(i, 2) = 1.0f;
	}

	size_t image_size = input_w * input_h;
	cv::Mat blob_image;
	resize(image, blob_image, cv::Size(input_w, input_h));
	blob_image.convertTo(blob_image, CV_32F);
	blob_image = blob_image / 255.0;

	// NCHW 设置输入图象数据
	float* data = input_tensor.data<float>();
	for (size_t row = 0; row < input_h; row++) {
		for (size_t col = 0; col < input_w; col++) {
			for (size_t ch = 0; ch < num_channels; ch++) {
				data[image_size*ch + row * input_w + col] = blob_image.at<cv::Vec3f>(row, col)[ch];
			}
		}
	}

	// 推理与返回结果
	this->infer_request.infer();

	auto output = this->infer_request.get_output_tensor();
	const float* prob = (float*)output.data();
	const ov::Shape outputDims = output.get_shape();
	size_t numRows = outputDims[1];
	size_t numCols = outputDims[2];
	
	// 8400x56
	std::vector<cv::Rect> boxes;
	std::vector<cv::Mat> kypts;
	std::vector<float> confidences;
	cv::Mat dout(numRows, numCols, CV_32F, (float*)prob);
	cv::Mat det_output = dout.t();

	for (int i = 0; i < det_output.rows; i++) {
		double conf = det_output.at<float>(i, 4);
		// 置信度 0～1之间
		if (conf > this->score_threshold)
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
			boxes.push_back(box);
			confidences.push_back(conf);
			cv::Mat pts = det_output.row(i).colRange(5, det_output.cols);
			kypts.push_back(pts);
		}
	}

	// NMS
	std::vector<int> indexes;
	cv::dnn::NMSBoxes(boxes, confidences, 0.25, 0.45, indexes);

	// 计算FPS render it
	// cv::Mat black = cv::Mat(frame.size(), frame.type());
	float t = (cv::getTickCount() - start) / static_cast<float>(cv::getTickFrequency());
	cv::putText(frame, cv::format("FPS: %.2f", 1.0 / t), cv::Point(20, 40), cv::FONT_HERSHEY_PLAIN, 2.0, cv::Scalar(255, 0, 0), 2, 8);
	// black.copyTo(frame);
	for (size_t i = 0; i < indexes.size(); i++) {
		int idx = indexes[i];
		cv::rectangle(frame, boxes[idx], cv::Scalar(0, 0, 255), 1, 8);
		cv::rectangle(frame, cv::Point(boxes[idx].tl().x, boxes[idx].tl().y - 20),
			cv::Point(boxes[idx].br().x, boxes[idx].tl().y), cv::Scalar(0, 255, 255), -1);
		cv::Mat one_kypts = kypts[idx];
		cv::Mat m2 = one_kypts.reshape(0, 17);
		cv::Mat kpts; // 17x3
		cv::multiply(m2, m1, kpts);

		cv::line(frame, cv::Point(kpts.at<float>(0, 0), kpts.at<float>(0, 1)), cv::Point(kpts.at<float>(1, 0), kpts.at<float>(1, 1)), color_tables[0], 2, 8, 0);
		cv::line(frame, cv::Point(kpts.at<float>(1, 0), kpts.at<float>(1, 1)), cv::Point(kpts.at<float>(3, 0), kpts.at<float>(3, 1)), color_tables[1], 2, 8, 0);

		// nose->right_eye->right_ear.(0, 2), (2, 4)
		cv::line(frame, cv::Point(kpts.at<float>(0, 0), kpts.at<float>(0, 1)), cv::Point(kpts.at<float>(2, 0), kpts.at<float>(2, 1)), color_tables[2], 2, 8, 0);
		cv::line(frame, cv::Point(kpts.at<float>(2, 0), kpts.at<float>(2, 1)), cv::Point(kpts.at<float>(4, 0), kpts.at<float>(4, 1)), color_tables[3], 2, 8, 0);

		// nose->left_shoulder->left_elbow->left_wrist.(0, 5), (5, 7), (7, 9)
		cv::line(frame, cv::Point(kpts.at<float>(0, 0), kpts.at<float>(0, 1)), cv::Point(kpts.at<float>(5, 0), kpts.at<float>(5, 1)), color_tables[4], 2, 8, 0);
		cv::line(frame, cv::Point(kpts.at<float>(5, 0), kpts.at<float>(5, 1)), cv::Point(kpts.at<float>(7, 0), kpts.at<float>(7, 1)), color_tables[5], 2, 8, 0);
		cv::line(frame, cv::Point(kpts.at<float>(7, 0), kpts.at<float>(7, 1)), cv::Point(kpts.at<float>(9, 0), kpts.at<float>(9, 1)), color_tables[6], 2, 8, 0);

		// nose->right_shoulder->right_elbow->right_wrist.(0, 6), (6, 8), (8, 10)
		cv::line(frame, cv::Point(kpts.at<float>(0, 0), kpts.at<float>(0, 1)), cv::Point(kpts.at<float>(6, 0), kpts.at<float>(6, 1)), color_tables[7], 2, 8, 0);
		cv::line(frame, cv::Point(kpts.at<float>(6, 0), kpts.at<float>(6, 1)), cv::Point(kpts.at<float>(8, 0), kpts.at<float>(8, 1)), color_tables[8], 2, 8, 0);
		cv::line(frame, cv::Point(kpts.at<float>(8, 0), kpts.at<float>(8, 1)), cv::Point(kpts.at<float>(10, 0), kpts.at<float>(10, 1)), color_tables[9], 2, 8, 0);

		// left_shoulder->left_hip->left_knee->left_ankle.(5, 11), (11, 13), (13, 15)
		cv::line(frame, cv::Point(kpts.at<float>(5, 0), kpts.at<float>(5, 1)), cv::Point(kpts.at<float>(11, 0), kpts.at<float>(11, 1)), color_tables[10], 2, 8, 0);
		cv::line(frame, cv::Point(kpts.at<float>(11, 0), kpts.at<float>(11, 1)), cv::Point(kpts.at<float>(13, 0), kpts.at<float>(13, 1)), color_tables[11], 2, 8, 0);
		cv::line(frame, cv::Point(kpts.at<float>(13, 0), kpts.at<float>(13, 1)), cv::Point(kpts.at<float>(15, 0), kpts.at<float>(15, 1)), color_tables[12], 2, 8, 0);

		// right_shoulder->right_hip->right_knee->right_ankle.(6, 12), (12, 14), (14, 16)
		cv::line(frame, cv::Point(kpts.at<float>(6, 0), kpts.at<float>(6, 1)), cv::Point(kpts.at<float>(12, 0), kpts.at<float>(12, 1)), color_tables[13], 2, 8, 0);
		cv::line(frame, cv::Point(kpts.at<float>(12, 0), kpts.at<float>(12, 1)), cv::Point(kpts.at<float>(14, 0), kpts.at<float>(14, 1)), color_tables[14], 2, 8, 0);
		cv::line(frame, cv::Point(kpts.at<float>(14, 0), kpts.at<float>(14, 1)), cv::Point(kpts.at<float>(16, 0), kpts.at<float>(16, 1)), color_tables[16], 2, 8, 0);

		for (int row = 0; row < kpts.rows; row++) {
			int x = static_cast<int>(kpts.at<float>(row, 0));
			int y = static_cast<int>(kpts.at<float>(row, 1));
			cv::circle(frame, cv::Size(x, y), 3, cv::Scalar(255, 0, 255), 4, 8, 0);
		}
	}
}
