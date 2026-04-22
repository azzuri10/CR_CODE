//#pragma once
//#ifndef LOCATIONANDRECOGNITION_H
//#define LOCATIONANDRECOGNITION_H
//
//#include "HeaderDefine.h"
//#include <opencv2/opencv.hpp>
//#include <opencv2/dnn.hpp>
//#include <vector>
//#include <functional>
//#include <mutex>
//#include <atomic>
//
//using namespace cv;
//using namespace cv::dnn;
//using namespace std;
//
////struct FinsObjects {
////    vector<Rect> boxes;
////    vector<float> confidences;
////};
//
//class LocationAndRecognition {
//public:
//    LocationAndRecognition() = default;
//    ~LocationAndRecognition() = default;
//
//    bool InitModelYOLO(const string& modelWeights, const string& classesFile, int deviceType,
//        vector<string>& classes, Net& net, int netType, string& errlog);
//
//    void ProcessImageAsync(Mat img, int cameraId, Net& net, float confThreshold, float nmsThreshold,
//        ThreadPool& threadPool, vector<atomic<bool>>& state, function<void(FinsObjects&)> callback);
//
//private:
//    vector<vector<Mat>> outs;
//    mutex net_mutex;
//};
//
//#endif // LOCATIONANDRECOGNITION_H