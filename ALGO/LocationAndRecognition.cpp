//#include "LocationAndRecognition.h"
//#include <opencv2/opencv.hpp>
//#include <opencv2/dnn.hpp>
//#include <iostream>
//#include <vector>
//#include <future>
//
//using namespace cv;
//using namespace cv::dnn;
//using namespace std;
//
//bool LocationAndRecognition::InitModelYOLO(const string& modelWeights, const string& classesFile, int deviceType,
//	vector<string>& classes, Net& net, int netType, string& errlog) {
//	classes.clear();
//
//	ifstream ifs(classesFile.c_str());
//	if (!ifs.is_open()) {
//		errlog = "classesFile ²»´æÔÚ£¡";
//		return false;
//	}
//	string line;
//	while (getline(ifs, line)) classes.push_back(line);
//
//	try {
//		net = readNetFromONNX(modelWeights);
//		if (deviceType == 0) {
//			net.setPreferableBackend(DNN_BACKEND_CUDA);
//			net.setPreferableTarget(DNN_TARGET_CUDA);
//		}
//		else {
//			net.setPreferableBackend(DNN_BACKEND_OPENCV);
//			net.setPreferableTarget(DNN_TARGET_CPU);
//		}
//	}
//	catch (const cv::Exception& e) {
//		errlog = e.what();
//		return false;
//	}
//
//	return true;
//}
//
//void LocationAndRecognition::ProcessImageAsync(Mat img, int cameraId, Net& net, float confThreshold,
//	float nmsThreshold, ThreadPool& threadPool, vector<atomic<bool>>& state,
//	function<void(FinsObjects&)> callback) {
//	if (state[cameraId]) return;
//	state[cameraId] = true;
//
//	threadPool.enqueue([=, &net, &state, &callback]() mutable {
//		vector<int> classIds;
//		vector<float> confidences;
//		vector<Rect> boxes;
//		Mat blob;
//		blobFromImage(img, blob, 1 / 255.0, Size(640, 640), Scalar(0, 0, 0), true, false);
//
//		{
//			lock_guard<mutex> lock(net_mutex);
//			net.setInput(blob);
//			net.forward(outs[cameraId], net.getUnconnectedOutLayersNames());
//		}
//
//		for (size_t i = 0; i < outs[cameraId].size(); ++i) {
//			float* data = (float*)outs[cameraId][i].data;
//			for (int j = 0; j < outs[cameraId][i].rows; ++j, data += outs[cameraId][i].cols) {
//				Mat scores = outs[cameraId][i].row(j).colRange(5, outs[cameraId][i].cols);
//				Point classIdPoint;
//				double confidence;
//				minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
//				if (confidence > confThreshold) {
//					int centerX = (int)(data[0] * img.cols);
//					int centerY = (int)(data[1] * img.rows);
//					int width = (int)(data[2] * img.cols);
//					int height = (int)(data[3] * img.rows);
//					int left = centerX - width / 2;
//					int top = centerY - height / 2;
//
//					classIds.push_back(classIdPoint.x);
//					confidences.push_back((float)confidence);
//					boxes.push_back(Rect(left, top, width, height));
//				}
//			}
//		}
//
//		vector<int> indices;
//		NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);
//
//		FinsObjects result;
//		for (size_t i = 0; i < indices.size(); ++i) {
//			int idx = indices[i];
//			result.boxes.push_back(boxes[idx]);
//			result.confidences.push_back(confidences[idx]);
//		}
//
//		state[cameraId] = false;
//		callback(result);
//		});
//}