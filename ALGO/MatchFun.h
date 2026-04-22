#ifndef MATCHFUN_H
#define MATCHFUN_H
#include "HeaderDefine.h"
//#include "GeoMatch.h"
#include "AnalyseMat.h"
#include "HalconCpp.h"
#include "HalconFun.h"
#include <thread>

#include <future>
#include <mutex>


// RAII 资源管理类
class HalconResourceGuard {
public:
	HalconResourceGuard(HalconCpp::HTuple& model) : model_(model) {}
	~HalconResourceGuard() {
		if (!released_) {
			try {
				HalconCpp::ClearShapeModel(model_);
			}
			catch (...) {
				// 异常时静默处理
			}
		}
	}
	void release() { released_ = true; }
private:
	HalconCpp::HTuple& model_;
	bool released_ = false;
};


class MatchFun {
public:
	MatchFun();
	~MatchFun(void);

	//  bool MatchEdgeGeo(cv::Mat src, cv::Mat markImg, double minScore, double
	//  greediness,
	//                    Point& result, double& score);


	const char* GetOptimizationString(int optimizationLevel);
	const char* GetMetricString(int optimizationLevel);
	const char* GetSubPixelString(int optimizationLevel);
	bool MatchCounters(cv::Mat src, cv::Mat markImg, float thresh, double& score);
	bool MatchOpenCV(cv::Mat src, cv::Mat markImg, cv::Rect& matchRect, float thresh,
		double& score);
	bool MatchOpenCV(cv::Mat src, cv::Rect roiRect, cv::Mat markImg, cv::Rect& matchRect,
		float thresh, double& score);
	bool MatchOpenCV(cv::Mat src, cv::Rect roiRect, std::vector<cv::Mat> markImgs, cv::Rect& matchRect,
		float thresh, double& score);
	bool MatchSurf(cv::Mat src, cv::Rect roiRect, std::vector<cv::KeyPoint>& srcKeypoints, cv::Mat& srcDescriptors,
		cv::Mat markImg, std::vector<cv::KeyPoint> templatesKeypoints, cv::Mat templatesDescriptors,
		std::vector<cv::DMatch>& good_matches_ransac, std::vector<cv::Point2f>& obj, std::vector<cv::Point2f>& scene, int thresh, int& score);
	bool MatchSurf(cv::Mat src, cv::Rect roiRect, std::vector<cv::KeyPoint>& srcKeypoints, cv::Mat& srcDescriptors,
		std::vector<cv::Mat> templateImg, std::vector<std::vector<cv::KeyPoint>> templateKeypoints, std::vector<cv::Mat> templateDescriptors,
		std::vector<cv::DMatch>& good_matches_ransac, std::vector<cv::Point2f>& srcPoints, std::vector<cv::Point2f>& templatePoints, int thresh, int& score, int& id);
	bool MatchSurf(cv::Mat src, cv::Rect roiRect, std::vector<cv::KeyPoint>& srcKeypoints, cv::Mat& srcDescriptors,
		std::vector<cv::Mat> templateImg, std::vector<std::vector<cv::KeyPoint>> templateKeypoints, std::vector<cv::Mat> templateDescriptors,
		std::vector<cv::DMatch>& good_matches_ransac, std::vector<cv::Point2f>& srcPoints, std::vector<cv::Point2f>& templatePoints, int thresh, int& score, int& id, float& angle, std::vector<cv::Point2f>& dstCorner);
	int MatchNCC(cv::Mat src, MatchConfig matchCfg, MatchResult& result);
	int MatchSIFT(cv::Mat src, MatchConfig matchCfg, MatchResult& result);
	std::vector<cv::KeyPoint> MultiScaleFeatureDetection(const cv::Mat& input);
	float FixRotatedRectAngle(float angle);
	float CalculateAngleFromFeatureDirections(
		const std::vector<cv::KeyPoint>& srcKeypoints,
		const std::vector<cv::KeyPoint>& templateKeypoints,
		const std::vector<cv::DMatch>& matches);
	float NormalizeAngleTo_90_90(float angle);
	float NormalizeRotatedRectAngleTo_90_90(float angle);

	float CalculateRobustAngleFromMatches(
		const std::vector<cv::KeyPoint>& srcKeypoints,
		const std::vector<cv::KeyPoint>& templateKeypoints,
		const std::vector<cv::DMatch>& matches,
		const std::vector<cv::Point2f>& transformedCorners);
	float CalculateAngleFromHomography(
		const std::vector<cv::DMatch>& matches,
		const std::vector<cv::KeyPoint>& srcKeypoints,
		const std::vector<cv::KeyPoint>& templateKeypoints);
	float CalculateAngleFromMatchVectors(
		const std::vector<cv::KeyPoint>& srcKeypoints,
		const std::vector<cv::KeyPoint>& templateKeypoints,
		const std::vector<cv::DMatch>& matches);
	float CombineAngles(float angle1, float angle2, float angle3);
	float CalculateAngleConsistency(const std::vector<float>& angles);
	float SelectMostLikelyAngle(const std::vector<float>& angles);

	cv::Ptr<cv::SIFT> CreateSIFTDetectorFromHalconConfig(const MatchConfig& matchCfg);
	int MatchHalcon(cv::Mat src, MatchConfig matchCfg, MatchResult& results);
	int RefineMatchX(cv::Mat& img1, cv::Mat& img2, std::vector<cv::KeyPoint>& keypoints0, std::vector<cv::KeyPoint>& keypoints1, std::vector<cv::DMatch>& match_vec);
	int RefineMatchY(cv::Mat& img1, cv::Mat& img2, std::vector<cv::KeyPoint>& keypoints0, std::vector<cv::KeyPoint>& keypoints1, std::vector<cv::DMatch>& match_vec);
	
	int MatchLocateHalcon(cv::Mat src, MatchLocateConfig matchCfg, MatchResult& resultFinal);

private:
	//  GeoMatch* GM;
	AnalyseMat* ANA;
	CalcFun* CAL;

	mutable std::mutex template_mutex_; // 保护模型字典
	std::mutex init_mutex_;           // 保护 LoadModels() 初始化
	bool is_initialized_ = false;     // 标记是否已经加载过

	std::unordered_map<std::string, cv::dnn::Net> cv_template_;
	//std::unordered_map<std::string, ov::CompiledModel> halcon_template_;

};

#endif  // MATCHFUN_H
#pragma once
