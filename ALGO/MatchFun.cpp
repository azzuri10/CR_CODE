#include "MatchFun.h"
using namespace HalconCpp;

//generate keyPoints 
int minHessian = 1000;
cv::Ptr<cv::SIFT> detector_sift = cv::SIFT::create(minHessian);
static std::mutex halcon_mutex;

MatchFun::MatchFun() {

	ANA = new AnalyseMat;
	CAL = new CalcFun;
}

MatchFun::~MatchFun() {
	delete ANA;
	delete CAL;
}

const char* MatchFun::GetOptimizationString(int optimizationLevel) {
	switch (optimizationLevel) {
	case 1: return "none";                 // 不优化，保留所有点
	case 2: return "point_reduction_low";   // 低程度优化（减少10-20%）
	case 3: return "point_reduction_medium";// 中度优化（默认，减少30-40%）
	case 4: return "point_reduction_high";  // 高度优化（减少50-60%）
	case 5: return "auto";                  // 自动选择优化程度
	default: return "point_reduction_medium"; // 默认值
	}
}

const char* MatchFun::GetMetricString(int metricLevel) {
	switch (metricLevel) {
	case 1: return "use_polarity";                 // 严格匹配对比度极性 ,最快	稳定光照环境
	case 2: return "ignore_global_polarity";   // 允许光照反转,时间+10%	
	case 3: return "ignore_local_polarity";// 允许局部对比度变化，时间+50%
	case 4: return "ignore_color_polarity";  // 多通道颜色极性无视	，时间+100%
	default: return "ignore_global_polarity"; // 默认值
	}
}

const char* MatchFun::GetSubPixelString(int subPixelLevel) {
	switch (subPixelLevel) {
	case 1: return "none";                 // 整数像素精度（最快）±0.5
	case 2: return "least_squares,max_deformation 1"; // 允许模型边缘点有最大1像素的形变
	case 3: return "least_squares,max_deformation 2"; // 允许模型边缘点有最大2像素的形变
	case 4: return "least_squares,max_deformation 3"; // 允许模型边缘点有最大3像素的形变
	case 5: return "least_squares,max_deformation 4"; // 允许模型边缘点有最大4像素的形变
	case 6: return "least_squares,max_deformation 5";  // 允许模型边缘点有最大5像素的形变
	case 7: return "least_squares, max_deformation 6"; // 允许模型边缘点有最大6像素的形变
	case 11: return "interpolation";   // 双线性插值±0.1
	case 12: return "interpolation,max_deformation 1";   // 双线性插值±0.1
	case 13: return "interpolation,max_deformation 2";   // 双线性插值±0.1
	case 14: return "interpolation,max_deformation 3";   // 双线性插值±0.1
	case 15: return "interpolation,max_deformation 4";   // 双线性插值±0.1
	case 16: return "interpolation,max_deformation 5";   // 双线性插值±0.1
	case 17: return "interpolation,max_deformation 6";   // 双线性插值±0.1
	case 21: return "least_squares";   // 高斯-牛顿优化±0.01
	case 22: return "least_squares,max_deformation 1";   // 高斯-牛顿优化±0.01
	case 23: return "least_squares,max_deformation 2";   // 高斯-牛顿优化±0.01
	case 24: return "least_squares,max_deformation 3";   // 高斯-牛顿优化±0.01
	case 25: return "least_squares,max_deformation 4";   // 高斯-牛顿优化±0.01
	case 26: return "least_squares,max_deformation 5";   // 高斯-牛顿优化±0.01
	case 27: return "least_squares,max_deformation 6";   // 高斯-牛顿优化±0.01
	case 31: return "least_squares_high";   // 带Hessian矩阵优化±0.005
	case 32: return "least_squares_high,max_deformation 1";   // 带Hessian矩阵优化±0.005
	case 33: return "least_squares_high,max_deformation 2";   // 带Hessian矩阵优化±0.005
	case 34: return "least_squares_high,max_deformation 3";   // 带Hessian矩阵优化±0.005
	case 35: return "least_squares_high,max_deformation 4";   // 带Hessian矩阵优化±0.005
	case 36: return "least_squares_high,max_deformation 5";   // 带Hessian矩阵优化±0.005
	case 37: return "least_squares_high,max_deformation 6";   // 带Hessian矩阵优化±0.005
	case 41: return "least_squares_very_high";   // 高阶泰勒展开±0.001
	case 42: return "least_squares_very_high,max_deformation 1";   // 高阶泰勒展开±0.001
	case 43: return "least_squares_very_high,max_deformation 2";   // 高阶泰勒展开±0.001
	case 44: return "least_squares_very_high,max_deformation 3";   // 高阶泰勒展开±0.001
	case 45: return "least_squares_very_high,max_deformation 4";   // 高阶泰勒展开±0.001
	case 46: return "least_squares_very_high,max_deformation 5";   // 高阶泰勒展开±0.001
	case 47: return "least_squares_very_high,max_deformation 6";   // 高阶泰勒展开±0.01               // 高阶泰勒展开
	default: return "interpolation"; // 默认值
	}
}



bool MatchFun::MatchCounters(cv::Mat src, cv::Mat markImg, float thresh,
	double& score) {
	if (src.empty()) {
		std::cout << "src is empty!" << std::endl;
		return false;
	}
	if (markImg.empty()) {
		std::cout << "markImg is empty!" << std::endl;
		return false;
	}
	if (src.channels() == 3) {
		cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
	}
	if (markImg.channels() == 3) {
		cv::cvtColor(markImg, markImg, cv::COLOR_BGR2GRAY);
	}

	return true;
}

bool MatchFun::MatchOpenCV(cv::Mat src, cv::Mat markImg, cv::Rect& matchRect, float thresh,
	double& score) {
	if (src.empty()) {
		std::cout << "src is empty!" << std::endl;
		return false;
	}
	if (markImg.empty()) {
		std::cout << "markImg is empty!" << std::endl;
		return false;
	}
	if (src.cols < markImg.cols || src.rows < markImg.rows) {
		std::cout << "markImg size must smaller than src!" << std::endl;
		return false;
	}
	if (src.channels() == 3) {
		cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
	}
	if (markImg.channels() == 3) {
		cv::cvtColor(markImg, markImg, cv::COLOR_BGR2GRAY);
	}

	cv::Mat matchResult;
	bool matchFlag = false;
	for (int i = 0; i < 5; i++) {
		float resizeBnd = 1 + pow(-1, i) * 0.05 * i;
		if (i != 0) {
			resize(markImg, markImg,
				cv::Size(markImg.cols * resizeBnd, markImg.rows * resizeBnd));
			if (markImg.cols > src.cols || markImg.rows > src.rows) {
				continue;
			}
		}
		matchTemplate(src, markImg, matchResult, cv::TM_CCOEFF_NORMED);

		double minVal, maxVal;
		cv::Point minLoc, maxLoc, minLocGoal;
		minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);
		matchRect = cv::Rect(maxLoc.x, maxLoc.y, markImg.cols, markImg.rows);

		//找到大于匹配阈值时即返回
		score = maxVal;
		if (score > thresh) {
			matchFlag = true;
			return matchFlag;
		}
	}

	return matchFlag;
}

bool MatchFun::MatchOpenCV(cv::Mat src, cv::Rect roiRect, cv::Mat markImg, cv::Rect& matchRect,
	float thresh, double& score) {
	if (src.empty()) {
		std::cout << "src is empty!" << std::endl;
		return false;
	}
	if (markImg.empty()) {
		std::cout << "markImg is empty!" << std::endl;
		return false;
	}
	if (roiRect.x < 0 || roiRect.y < 0 || roiRect.x + roiRect.width > src.cols ||
		roiRect.y + roiRect.height > src.rows) {
		std::cout << "roi set error!" << std::endl;
		return false;
	}
	if (roiRect.width < markImg.cols || roiRect.height < markImg.rows) {
		std::cout << "markImg size must smaller than roiRect!" << std::endl;
		return false;
	}

	cv::Mat checkAreaImg = src(roiRect).clone();
	if (checkAreaImg.channels() == 3) {
		cv::cvtColor(checkAreaImg, checkAreaImg, cv::COLOR_BGR2GRAY);
	}
	if (markImg.channels() == 3) {
		cv::cvtColor(markImg, markImg, cv::COLOR_BGR2GRAY);
	}

	cv::Mat matchResult;
	bool matchFlag = false;
	for (int i = 0; i < 5; i++) {
		float resizeBnd = 1 + pow(-1, i) * 0.05 * i;
		if (i != 0) {
			resize(markImg, markImg,
				cv::Size(markImg.cols * resizeBnd, markImg.rows * resizeBnd));
			if (markImg.cols > checkAreaImg.cols ||
				markImg.rows > checkAreaImg.rows) {
				continue;
			}
		}
		matchTemplate(checkAreaImg, markImg, matchResult, cv::TM_CCOEFF_NORMED);

		double minVal, maxVal;
		cv::Point minLoc, maxLoc, minLocGoal;
		minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);
		matchRect = cv::Rect(maxLoc.x + roiRect.x, maxLoc.y + roiRect.y, markImg.cols,
			markImg.rows);

		//找到大于匹配阈值时即返回
		score = maxVal;
		if (score > thresh) {
			matchFlag = true;
			return matchFlag;
		}
	}

	return matchFlag;
}

bool MatchFun::MatchOpenCV(cv::Mat src, cv::Rect roiRect, std::vector<cv::Mat> markImgs, cv::Rect& matchRect,
	float thresh, double& score) {
	if (src.empty()) {
		std::cout << "src is empty!" << std::endl;
		return false;
	}
	if (roiRect.x < 0 || roiRect.y < 0 || roiRect.x + roiRect.width > src.cols ||
		roiRect.y + roiRect.height > src.rows) {
		std::cout << "roi set error!" << std::endl;
		return false;
	}

	cv::Mat checkAreaImg = src(roiRect).clone();
	if (checkAreaImg.channels() == 3) {
		cv::cvtColor(checkAreaImg, checkAreaImg, cv::COLOR_BGR2GRAY);
	}

	cv::Mat matchResult;
	bool matchFlag = false;
	for (int i = 0; i < markImgs.size(); i++) {
		if (markImgs.empty()) {
			std::cout << "markImg is empty!" << std::endl;
			return false;
		}
		if (roiRect.width < markImgs[i].cols || roiRect.height < markImgs[i].rows) {
			std::cout << "markImg size must smaller than roiRect!" << std::endl;
			return false;
		}
		if (markImgs[i].channels() == 3) {
			cv::cvtColor(markImgs[i], markImgs[i], cv::COLOR_BGR2GRAY);
		}
		matchTemplate(checkAreaImg, markImgs[i], matchResult, cv::TM_CCOEFF_NORMED);

		double minVal, maxVal;
		cv::Point minLoc, maxLoc, minLocGoal;
		minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);
		matchRect = cv::Rect(maxLoc.x + roiRect.x, maxLoc.y + roiRect.y, markImgs[i].cols,
			markImgs[i].rows);

		//找到大于匹配阈值时即返回
		score = maxVal * 100;
		if (score > thresh) {
			matchFlag = true;
			return matchFlag;
		}
	}

	return matchFlag;
}



bool MatchFun::MatchSurf(cv::Mat src, cv::Rect roiRect, std::vector<cv::KeyPoint>& srcKeypoints, cv::Mat& srcDescriptors,
	cv::Mat templateImg, std::vector<cv::KeyPoint> templateKeypoints, cv::Mat templateDescriptors,
	std::vector<cv::DMatch>& good_matches_ransac, std::vector<cv::Point2f>& srcPoints, std::vector<cv::Point2f>& templatePoints, int thresh, int& score) {
	srcKeypoints.clear();
	good_matches_ransac.clear();
	srcPoints.clear();
	templatePoints.clear();

	if (src.empty()) {
		std::cout << "src is empty!" << std::endl;
		return false;
	}
	if (templateImg.empty()) {
		std::cout << "templateImg is empty!" << std::endl;
		return false;
	}
	if (roiRect.x < 0 || roiRect.y < 0 || roiRect.x + roiRect.width > src.cols ||
		roiRect.y + roiRect.height > src.rows) {
		std::cout << "roi set error!" << std::endl;
		return false;
	}
	if (roiRect.width < templateImg.cols || roiRect.height < templateImg.rows) {
		std::cout << "templateImg size must smaller than roiRect!" << std::endl;
		return false;
	}
	if (templateKeypoints.size() < 4 || templateDescriptors.empty()) {
		std::cout << "templateImg feature error!" << std::endl;
		return false;
	}

	cv::Mat checkAreaImg = src(roiRect).clone();
	if (checkAreaImg.channels() == 3) {
		cv::cvtColor(checkAreaImg, checkAreaImg, cv::COLOR_BGR2GRAY);
	}
	if (templateImg.channels() == 3) {
		cv::cvtColor(templateImg, templateImg, cv::COLOR_BGR2GRAY);
	}

	bool matchFlag = false;
	detector_sift->detectAndCompute(checkAreaImg, cv::noArray(), srcKeypoints, srcDescriptors);


	cv::Ptr<cv::DescriptorMatcher> matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);
	std::vector< std::vector<cv::DMatch> > knn_matches;
	matcher->knnMatch(srcDescriptors, templateDescriptors, knn_matches, 2);

	//-- Filter matches using the Lowe's ratio test
	const float ratio_thresh = 0.7f;
	std::vector<cv::DMatch> good_matches;
	for (size_t i = 0; i < knn_matches.size(); i++)
	{
		if (knn_matches[i][0].distance < ratio_thresh * knn_matches[i][1].distance)
		{
			good_matches.push_back(knn_matches[i][0]);
		}
	}

	//-- Filter matches By YX
	RefineMatchY(checkAreaImg, templateImg, srcKeypoints, templateKeypoints, good_matches);
	RefineMatchX(checkAreaImg, templateImg, srcKeypoints, templateKeypoints, good_matches);

	//-- Localize the srcPointsect
	for (size_t i = 0; i < good_matches.size(); i++)
	{
		srcPoints.push_back(srcKeypoints[good_matches[i].queryIdx].pt);
		templatePoints.push_back(templateKeypoints[good_matches[i].trainIdx].pt);
	}
	if (srcPoints.size() < 4 || templatePoints.size() < 4)
	{
		score = 0;
		return matchFlag;
	}
	//matches with RANSAC
	std::vector<uchar>inliers;
	cv::Mat H = findHomography(srcPoints, templatePoints, inliers, cv::RANSAC);
	cv::Mat img_matches_ransac;
	for (size_t i = 0; i < inliers.size(); i++)
	{
		if (inliers[i])
		{
			good_matches_ransac.push_back(good_matches[i]);
		}
	}

	score = good_matches_ransac.size();
	if (score > thresh)
	{
		matchFlag = true;
	}

	return matchFlag;
}


bool MatchFun::MatchSurf(cv::Mat src, cv::Rect roiRect, std::vector<cv::KeyPoint>& srcKeypoints, cv::Mat& srcDescriptors,
	std::vector<cv::Mat> templateImg, std::vector<std::vector<cv::KeyPoint>> templateKeypoints, std::vector<cv::Mat> templateDescriptors,
	std::vector<cv::DMatch>& good_matches_ransac, std::vector<cv::Point2f>& srcPoints, std::vector<cv::Point2f>& templatePoints, int thresh, int& score, int& id) {
	srcKeypoints.clear();
	good_matches_ransac.clear();

	if (src.empty()) {
		std::cout << "src is empty!" << std::endl;
		return false;
	}
	if (roiRect.x < 0 || roiRect.y < 0 || roiRect.x + roiRect.width > src.cols ||
		roiRect.y + roiRect.height > src.rows) {
		std::cout << "roi set error!" << std::endl;
		return false;
	}

	cv::Mat checkAreaImg = src(roiRect).clone();
	if (checkAreaImg.channels() == 3) {
		cv::cvtColor(checkAreaImg, checkAreaImg, cv::COLOR_BGR2GRAY);
	}
	detector_sift->detectAndCompute(checkAreaImg, cv::noArray(), srcKeypoints, srcDescriptors);

	bool matchFlag = false;
	int maxScore = 4;
	for (int kk = 0; kk < templateImg.size(); kk++)
	{

		if (templateImg[kk].empty()) {
			std::cout << "templateImg is empty!" << std::endl;
			return false;
		}
		if (roiRect.width < templateImg[kk].cols || roiRect.height < templateImg[kk].rows) {
			std::cout << "templateImg size must smaller than roiRect!" << std::endl;
			return false;
		}
		if (templateKeypoints[kk].size() < 4 || templateDescriptors[kk].empty()) {
			std::cout << "templateImg feature error!" << std::endl;
			return false;
		}

		if (templateImg[kk].channels() == 3) {
			cv::cvtColor(templateImg[kk], templateImg[kk], cv::COLOR_BGR2GRAY);
		}

		cv::Ptr<cv::DescriptorMatcher> matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);
		std::vector< std::vector<cv::DMatch> > knn_matches;
		matcher->knnMatch(srcDescriptors, templateDescriptors[kk], knn_matches, 2);

		//-- Filter matches using the Lowe's ratio test
		const float ratio_thresh = 0.7f;
		std::vector<cv::DMatch> good_matches;
		for (size_t i = 0; i < knn_matches.size(); i++)
		{
			if (knn_matches[i][0].distance < ratio_thresh * knn_matches[i][1].distance)
			{
				good_matches.push_back(knn_matches[i][0]);
			}
		}

		/*auto matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::BRUTEFORCE);
		std::vector<cv::DMatch> good_matches;
		std::vector<cv::DMatch>  match_res;
		std::vector<cv::DMatch>  match_res_inv;
		matcher->match(srcDescriptors, templateDescriptors[kk], match_res);
		matcher->match(templateDescriptors[kk], srcDescriptors, match_res_inv);

		for (int i = 0; i < match_res.size(); i++)
		{
			bool isTrue = false;
			int train_idx = match_res[i].trainIdx;
			int query_idx = match_res[i].queryIdx;

			for (int j = 0; j < match_res_inv.size(); j++)
			{
				if (match_res_inv[j].trainIdx == query_idx
					&& match_res_inv[j].queryIdx == train_idx)
				{
					isTrue = true;
				}
			}
			if (isTrue)
			{
				good_matches.push_back(match_res[i]);
			}
		}*/

		//-- Filter matches By YX
		RefineMatchY(checkAreaImg, templateImg[kk], srcKeypoints, templateKeypoints[kk], good_matches);
		RefineMatchX(checkAreaImg, templateImg[kk], srcKeypoints, templateKeypoints[kk], good_matches);

		//-- Localize the srcPointsect
		srcPoints.clear();
		templatePoints.clear();
		for (size_t i = 0; i < good_matches.size(); i++)
		{
			srcPoints.push_back(srcKeypoints[good_matches[i].queryIdx].pt);
			templatePoints.push_back(templateKeypoints[kk][good_matches[i].trainIdx].pt);
		}
		if (srcPoints.size() < 4 || templatePoints.size() < 4)
		{
			score = 0;
			continue;
		}
		//matches with RANSAC
		std::vector<uchar>inliers;
		cv::Mat H = findHomography(srcPoints, templatePoints, inliers, cv::RANSAC, 1);
		cv::Mat img_matches_ransac;
		good_matches_ransac.clear();
		for (size_t i = 0; i < inliers.size(); i++)
		{
			if (inliers[i])
			{
				good_matches_ransac.push_back(good_matches[i]);
			}
		}

		score = good_matches_ransac.size();


		if (score > thresh)
		{
			matchFlag = true;
			id = kk;
			break;
		}
		else
		{
			if (maxScore < score)
			{
				maxScore = score;
				id = kk;
			}
		}
	}


	return matchFlag;
}

bool MatchFun::MatchSurf(cv::Mat src, cv::Rect roiRect, std::vector<cv::KeyPoint>& srcKeypoints, cv::Mat& srcDescriptors,
	std::vector<cv::Mat> templateImg, std::vector<std::vector<cv::KeyPoint>> templateKeypoints, std::vector<cv::Mat> templateDescriptors,
	std::vector<cv::DMatch>& good_matches_ransac, std::vector<cv::Point2f>& srcPoints, std::vector<cv::Point2f>& templatePoints, int thresh, int& score, int& id, float& angle, std::vector<cv::Point2f>& dstCorner) {
	srcKeypoints.clear();
	good_matches_ransac.clear();


	dstCorner.reserve(4);

	if (src.empty()) {
		std::cout << "src is empty!" << std::endl;
		return false;
	}
	if (roiRect.x < 0 || roiRect.y < 0 || roiRect.x + roiRect.width > src.cols ||
		roiRect.y + roiRect.height > src.rows) {
		std::cout << "roi set error!" << std::endl;
		return false;
	}

	cv::Mat checkAreaImg = src(roiRect).clone();
	if (checkAreaImg.channels() == 3) {
		cv::cvtColor(checkAreaImg, checkAreaImg, cv::COLOR_BGR2GRAY);
	}
	detector_sift->detectAndCompute(checkAreaImg, cv::noArray(), srcKeypoints, srcDescriptors);

	bool matchFlag = false;
	int maxScore = 4;
	for (int kk = 0; kk < templateImg.size(); kk++)
	{

		if (templateImg[kk].empty()) {
			std::cout << "templateImg is empty!" << std::endl;
			return false;
		}
		if (roiRect.width < templateImg[kk].cols || roiRect.height < templateImg[kk].rows) {
			std::cout << "templateImg size must smaller than roiRect!" << std::endl;
			return false;
		}
		if (templateKeypoints[kk].size() < 4 || templateDescriptors[kk].empty()) {
			std::cout << "templateImg feature error!" << std::endl;
			return false;
		}

		if (templateImg[kk].channels() == 3) {
			cv::cvtColor(templateImg[kk], templateImg[kk], cv::COLOR_BGR2GRAY);
		}

		cv::Ptr<cv::DescriptorMatcher> matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);
		std::vector< std::vector<cv::DMatch> > knn_matches;
		matcher->knnMatch(srcDescriptors, templateDescriptors[kk], knn_matches, 2);

		//-- Filter matches using the Lowe's ratio test
		const float ratio_thresh = 0.7f;
		std::vector<cv::DMatch> good_matches;
		for (size_t i = 0; i < knn_matches.size(); i++)
		{
			if (knn_matches[i][0].distance < ratio_thresh * knn_matches[i][1].distance)
			{
				good_matches.push_back(knn_matches[i][0]);
			}
		}


		//-- Filter matches By YX
		RefineMatchY(checkAreaImg, templateImg[kk], srcKeypoints, templateKeypoints[kk], good_matches);
		RefineMatchX(checkAreaImg, templateImg[kk], srcKeypoints, templateKeypoints[kk], good_matches);

		//-- Localize the srcPointsect
		srcPoints.clear();
		templatePoints.clear();
		for (size_t i = 0; i < good_matches.size(); i++)
		{
			srcPoints.push_back(srcKeypoints[good_matches[i].queryIdx].pt);
			templatePoints.push_back(templateKeypoints[kk][good_matches[i].trainIdx].pt);
		}
		if (srcPoints.size() < 4 || templatePoints.size() < 4)
		{
			score = 0;
			continue;
		}
		//matches with RANSAC
		std::vector<uchar>inliers;
		cv::Mat H = findHomography(templatePoints, srcPoints, inliers, cv::RANSAC, 1);
		cv::Mat img_matches_ransac;
		good_matches_ransac.clear();
		for (size_t i = 0; i < inliers.size(); i++)
		{
			if (inliers[i])
			{
				good_matches_ransac.push_back(good_matches[i]);
			}
		}

		score = good_matches_ransac.size();
		if (good_matches_ransac.size() < 4)
		{
			score = 0;
			continue;
		}

		cv::Mat R = H(cv::Rect(0, 0, 2, 2));  // 获取旋转矩阵部分
		angle = atan2(R.at<double>(1, 0), R.at<double>(0, 0)) * 180 / CV_PI;

		std::vector<cv::Point> srcCorner(4);
		srcCorner[0] = cv::Point(0, 0);
		srcCorner[1] = cv::Point(templateImg[kk].cols, 0);
		srcCorner[2] = cv::Point(templateImg[kk].cols, templateImg[kk].rows);
		srcCorner[3] = cv::Point(0, templateImg[kk].rows);


		double h[9];
		bool isFound = true;
		h[0] = H.at<double>(0, 0);
		h[1] = H.at<double>(0, 1);
		h[2] = H.at<double>(0, 2);
		h[3] = H.at<double>(1, 0);
		h[4] = H.at<double>(1, 1);
		h[5] = H.at<double>(1, 2);
		h[6] = H.at<double>(2, 0);
		h[7] = H.at<double>(2, 1);
		h[8] = H.at<double>(2, 2);
		for (int ii = 0; ii < 4; ii++)
		{
			float x = (float)srcCorner[ii].x;
			float y = (float)srcCorner[ii].y;
			float Z = (float)1. / (h[6] * x + h[7] * y + h[8]);
			float X = (float)(h[0] * x + h[1] * y + h[2]) * Z;
			float Y = (float)(h[3] * x + h[4] * y + h[5]) * Z;
			//if(X>=0&&X<width&&Y>=0&&Y<height)
			{
				dstCorner.push_back(cv::Point(int(X) + roiRect.x, int(Y) + roiRect.y));
			}
			//else
			{
				//isFound &= false;
			}
		}

		if (score > thresh)
		{
			matchFlag = true;
			id = kk;
			break;
		}
		else
		{
			if (maxScore < score)
			{
				maxScore = score;
				id = kk;
			}
		}
	}


	return matchFlag;
}

bool findShapeModelWithTimeout(HalconCpp::HObject& hImg, HTuple& templateImg, double minScore, int numMatch,
	double maxOverlap, int numLevels, HTuple& row, HTuple& col,
	HTuple& angle, HTuple& score, int timeout_ms) {
	// 用于表示匹配操作是否完成
	std::atomic<bool> isFinished(false);
	std::condition_variable cv;
	std::mutex mtx;

	// 启动新的线程执行 FindShapeModel
	std::thread matchThread([&]() {
		FindShapeModel(hImg, templateImg, -CV_PI / 6, CV_PI / 4, minScore, numMatch, maxOverlap,
			"least_squares", numLevels, 0.9, &row, &col, &angle, &score);
		// 标记完成
		isFinished = true;
		cv.notify_one();
		});

	// 等待任务完成或超时
	std::unique_lock<std::mutex> lock(mtx);
	if (!cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]() { return isFinished.load(); })) {
		std::cout << "匹配超时，强制跳出!" << std::endl;
		// 超时处理，强制停止线程
		matchThread.detach();  // 将线程分离，避免阻塞
		return false;
	}

	// 等待线程完成
	if (matchThread.joinable()) {
		matchThread.join();
	}
	return true;
}


int MatchFun::RefineMatchX(cv::Mat& img1, cv::Mat& img2, std::vector<cv::KeyPoint>& keypoints0, std::vector<cv::KeyPoint>& keypoints1, std::vector<cv::DMatch>& match_vec)
{
	if (img1.empty()) {
		std::cout << "img1 is empty!" << std::endl;
		return false;
	}
	if (img2.empty()) {
		std::cout << "img2 is empty!" << std::endl;
		return false;
	}
	if (keypoints0.size() < 4) {
		std::cout << "keypoints0 size less than 4!" << std::endl;
		return false;
	}
	if (keypoints1.size() < 4) {
		std::cout << "keypoints1 size less than 4!" << std::endl;
		return false;
	}
	if (match_vec.size() < 4) {
		std::cout << "match_vec size less than 4!" << std::endl;
		return false;
	}


	//filter 
	float w1 = img1.cols;
	float w2 = img2.cols;
	std::vector<float> x1_list;
	std::vector<float> x2_list;

	std::vector< int > idx_list;
	for (int i = 0; i < match_vec.size(); i++)
	{
		idx_list.push_back(i);
	}

	std::vector< cv::DMatch > x_matches;
	for (int i = 0; i < match_vec.size(); i++)
	{
		float x1 = keypoints0[match_vec[i].queryIdx].pt.x;
		float x2 = keypoints1[match_vec[i].trainIdx].pt.x;
		x1_list.push_back(x1 / w1);
		x2_list.push_back(x2 / w2);
	}

	for (int i = 0; i < x1_list.size(); i++)
		for (int j = i + 1; j < x1_list.size(); j++)
		{
			if (x1_list[i] > x1_list[j])
			{
				float tmp_x1 = x1_list[j];
				x1_list[j] = x1_list[i];
				x1_list[i] = tmp_x1;

				float tmp_x2 = x2_list[j];
				x2_list[j] = x2_list[i];
				x2_list[i] = tmp_x2;

				int tmp_idx = idx_list[j];
				idx_list[j] = idx_list[i];
				idx_list[i] = tmp_idx;
			}
		}

	std::vector<float> x2_list_smooth;
	std::vector<bool>   good_flg_list;
	float thresh_dis_x = 0.5;
	int bnd = 10;
	for (int i = 0; i < x2_list.size(); i++)
	{
		int sx = MAX(0, i - bnd);
		int ex = MIN(x2_list.size() - 1, i + bnd);

		std::vector<float> cut_list;
		for (int k = sx; k <= ex; k++)
		{
			cut_list.push_back(x2_list[k]);
		}

		sort(cut_list.begin(), cut_list.end());
		float med_val = cut_list[cut_list.size() / 2];
		x2_list_smooth.push_back(med_val);

		if (abs(x2_list[i] - med_val) < thresh_dis_x)
		{
			good_flg_list.push_back(true);
		}
		else
		{
			good_flg_list.push_back(false);
		}
	}

	for (int i = 0; i < good_flg_list.size(); i++)
	{
		if (good_flg_list[i])
		{
			int idx = idx_list[i];
			x_matches.push_back(match_vec[idx]);
		}
	}
	match_vec = x_matches;

	//DI_ShowKeyMatch(img1, img2, m_keypoints0, m_keypoints1, m_match_vec, LabelType360_GetName("3.3 matchX"), m_debugType);

	return 1;
}


int MatchFun::RefineMatchY(cv::Mat& img1, cv::Mat& img2, std::vector<cv::KeyPoint>& keypoints0, std::vector<cv::KeyPoint>& keypoints1, std::vector<cv::DMatch>& match_vec)
{
	if (img1.empty()) {
		std::cout << "img1 is empty!" << std::endl;
		return false;
	}
	if (img2.empty()) {
		std::cout << "img2 is empty!" << std::endl;
		return false;
	}
	if (keypoints0.size() < 4) {
		std::cout << "keypoints0 size less than 4!" << std::endl;
		return false;
	}
	if (keypoints1.size() < 4) {
		std::cout << "keypoints1 size less than 4!" << std::endl;
		return false;
	}
	if (match_vec.size() < 4) {
		std::cout << "match_vec size less than 4!" << std::endl;
		return false;
	}

	std::sort(match_vec.begin(), match_vec.end());

	std::vector<float> y1_list;
	std::vector<float> y2_list;

	float h1 = img1.rows;
	float h2 = img2.rows;

	for (int i = 0; i < keypoints0.size(); i++)
	{
		y1_list.push_back(keypoints0[i].pt.y / h1);

	}
	for (int i = 0; i < keypoints1.size(); i++)
	{
		y2_list.push_back(keypoints1[i].pt.y / h2);
	}

	std::vector< cv::DMatch > y_matches;
	float thresh_dis_y = 0.6;
	for (int i = 0; i < match_vec.size(); i++)
	{
		float y1 = y1_list[match_vec[i].queryIdx];
		float y2 = y2_list[match_vec[i].trainIdx];
		float dis = abs(y1 - y2);
		if (dis < thresh_dis_y)
		{
			y_matches.push_back(match_vec[i]);
		}
	}

	match_vec = y_matches;

	// resort !! 0 to 1
	y_matches.clear();
	y1_list.clear();
	y2_list.clear();

	std::vector< int > idx_list;
	for (int i = 0; i < match_vec.size(); i++)
	{
		idx_list.push_back(i);
	}


	for (int i = 0; i < match_vec.size(); i++)
	{
		float y1 = keypoints0[match_vec[i].queryIdx].pt.y;
		float y2 = keypoints1[match_vec[i].trainIdx].pt.y;
		y1_list.push_back(y1 / h1);
		y2_list.push_back(y2 / h2);
	}

	for (int i = 0; i < y1_list.size(); i++)
		for (int j = i + 1; j < y1_list.size(); j++)
		{
			if (y1_list[i] > y1_list[j])
			{
				float tmp_y1 = y1_list[j];
				y1_list[j] = y1_list[i];
				y1_list[i] = tmp_y1;

				float tmp_y2 = y2_list[j];
				y2_list[j] = y2_list[i];
				y2_list[i] = tmp_y2;

				int tmp_idx = idx_list[j];
				idx_list[j] = idx_list[i];
				idx_list[i] = tmp_idx;
			}
		}

	std::vector<float> y2_list_smooth;
	std::vector<bool>   good_flg_list;
	thresh_dis_y = 0.05;
	int bnd = 10;
	for (int i = 0; i < y2_list.size(); i++)
	{
		int sy = MAX(0, i - bnd);
		int ey = MIN(y2_list.size() - 1, i + bnd);

		std::vector<float> cut_list;
		for (int k = sy; k <= ey; k++)
		{
			cut_list.push_back(y2_list[k]);
		}

		sort(cut_list.begin(), cut_list.end());
		float med_val = cut_list[cut_list.size() / 2];
		y2_list_smooth.push_back(med_val);

		if (abs(y2_list[i] - med_val) < thresh_dis_y)
		{
			good_flg_list.push_back(true);
		}
		else
		{
			good_flg_list.push_back(false);
		}
	}

	for (int i = 0; i < good_flg_list.size(); i++)
	{
		if (good_flg_list[i])
		{
			int idx = idx_list[i];
			y_matches.push_back(match_vec[idx]);
		}
	}
	match_vec = y_matches;

	return 1;
}

int MatchFun::MatchHalcon(cv::Mat src, MatchConfig matchCfg, MatchResult& resultFinal)
{
	// 1. 输入验证
	if (src.empty()) return -1;
	if (matchCfg.labelAllTemplateHObjects.empty() || matchCfg.templateMats.empty()) {
		return -3; // 模板为空
	}
	if (matchCfg.labelAllTemplateHObjects.size() != matchCfg.templateMats.size()) {
		return -4; // 模板数量不匹配
	}
	for (int i = 0; i < matchCfg.templatePaths.size(); i++) {
		if (matchCfg.rois[i].x < 0 || matchCfg.rois[i].y < 0 ||
			matchCfg.rois[i].x + matchCfg.rois[i].width > src.cols ||
			matchCfg.rois[i].y + matchCfg.rois[i].height > src.rows) {
			return -2; // ROI无效
		}
	}

	// 2. 多模板并行匹配
	std::vector<std::future<MatchResult>> futures;
	std::vector<MatchResult> results(matchCfg.labelAllTemplateHObjects.size());

	for (int kk = 0; kk < matchCfg.labelAllTemplateHObjects.size(); ++kk) {
		futures.emplace_back(std::async(std::launch::async, [&, kk]() {
			// 准备ROI区域
			cv::Rect originalProposedRoi = matchCfg.rois[kk]; // 原始提议的ROI
			originalProposedRoi.x -= matchCfg.extW;
			originalProposedRoi.y -= matchCfg.extH;
			originalProposedRoi.width += 2 * matchCfg.extW;
			originalProposedRoi.height += 2 * matchCfg.extH;

			// 调整ROI以确保其在图像范围内
			cv::Rect roiCur = ANA->AdjustROI(originalProposedRoi, src);
			cv::Mat roi = src(roiCur);
			if (roi.channels() == 3) {
				cv::cvtColor(roi, roi, cv::COLOR_BGR2GRAY);
			}

			MatchResult result;
			result.id = kk;
			result.valid = 0; // 默认匹配失败
			result.adjustedRoiX = roiCur.x;
			result.adjustedRoiY = roiCur.y;

			try {
				HalconCpp::HTuple hv_ModelID;
				{
					std::lock_guard<std::mutex> lock(halcon_mutex);
					HalconResourceGuard guard(hv_ModelID);

					// 转换图像
					ANA->Mat2HObject(roi, result.ho_Image);

					// 执行匹配
					switch (matchCfg.matchType) {
					case 0:
						HalconCpp::FindShapeModel(
							result.ho_Image,
							matchCfg.labelAllTemplateHTuple[kk],
							matchCfg.angleRange[0] * CV_PI / 180.0,
							(matchCfg.angleRange[1] - matchCfg.angleRange[0]) * CV_PI / 180.0,
							0.3,
							matchCfg.numMatches,
							matchCfg.maxOverlap,
							GetSubPixelString(matchCfg.subPixel),
							matchCfg.numLevels,
							matchCfg.greediness,
							&result.hv_Row,
							&result.hv_Column,
							&result.hv_Angle,
							&result.hv_Score);
						result.hv_Scale = HalconCpp::HTuple(1.0);
						break;
					case 1:
						HalconCpp::FindScaledShapeModel(
							result.ho_Image,
							matchCfg.labelAllTemplateHTuple[kk],
							matchCfg.angleRange[0] * CV_PI / 180.0,
							(matchCfg.angleRange[1] - matchCfg.angleRange[0]) * CV_PI / 180.0,
							matchCfg.scaleRange[0],
							matchCfg.scaleRange[1],
							0.3,
							matchCfg.numMatches,
							matchCfg.maxOverlap,
							GetSubPixelString(matchCfg.subPixel),
							matchCfg.numLevels,
							matchCfg.greediness,
							&result.hv_Row,
							&result.hv_Column,
							&result.hv_Angle,
							&result.hv_Scale,
							&result.hv_Score);
						break;
					}

					// 统一处理匹配结果
					if (result.hv_Score.Length() > 0 && result.hv_Score[0].D() >= 0) {
						result.score = result.hv_Score[0].D();
						result.angle = -result.hv_Angle[0].D() * 180.0 / CV_PI;
						// 角度规范化
						if (result.angle > 180) result.angle -= 360;
						else if (result.angle < -180) result.angle += 360;

						double scale = (matchCfg.matchType == 1) ? result.hv_Scale[0].D() : 1.0;

						// 关键：计算匹配点在原图中的坐标
						double matchedCenterX_inSrc = result.hv_Column[0].D() + result.adjustedRoiX;
						double matchedCenterY_inSrc = result.hv_Row[0].D() + result.adjustedRoiY;

						result.center.x = matchedCenterX_inSrc;
						result.center.y = matchedCenterY_inSrc;

						// 计算偏移量（相对于模板预设的中心点）
						result.shiftHor = matchedCenterX_inSrc - matchCfg.templateCenterX;
						result.shiftVer = matchedCenterY_inSrc - matchCfg.templateCenterY;
						result.offset = std::sqrt(result.shiftHor * result.shiftHor + result.shiftVer * result.shiftVer);

						// 计算包围盒（在原图中计算）
						result.boundingRect = CAL->CALC_RotatedRect(
							matchedCenterX_inSrc,
							matchedCenterY_inSrc,
							result.angle,
							scale * matchCfg.templateMats[kk].cols,
							scale * matchCfg.templateMats[kk].rows);

						cv::Point2f corners[4];
						result.boundingRect.points(corners);
						result.corners.assign(corners, corners + 4);

						// ============ 新增：生成变换后的轮廓 ============
						// 获取模板轮廓
						HObject ho_ModelContours;
						HalconCpp::GetShapeModelContours(&ho_ModelContours,
							matchCfg.labelAllTemplateHTuple[kk], 1);

						if (ho_ModelContours.IsInitialized()) {
							// 计算变换矩阵
							HTuple hv_HomMat2D;
							if (matchCfg.matchType == 0) {
								// 形状匹配：只有旋转和平移
								HalconCpp::VectorAngleToRigid(0, 0, 0,
									result.hv_Row[0], result.hv_Column[0], result.hv_Angle[0],
									&hv_HomMat2D);
							}
							else {
								// 缩放形状匹配：包含缩放
								HalconCpp::HomMat2dIdentity(&hv_HomMat2D);
								HalconCpp::HomMat2dScale(hv_HomMat2D, result.hv_Scale[0], result.hv_Scale[0],
									0, 0, &hv_HomMat2D);
								HalconCpp::HomMat2dRotate(hv_HomMat2D, result.hv_Angle[0], 0, 0, &hv_HomMat2D);
								HalconCpp::HomMat2dTranslate(hv_HomMat2D, result.hv_Row[0], result.hv_Column[0],
									&hv_HomMat2D);
							}

							// 应用变换到轮廓
							HalconCpp::AffineTransContourXld(ho_ModelContours, &result.ho_TransContours, hv_HomMat2D);

							// 检查变换后的轮廓是否有效
							if (!result.ho_TransContours.IsInitialized()) {
								//LOG->WriteLog("变换后的轮廓初始化失败", WARNING, "", true);
							}
						}
						// ============ 新增代码结束 ============

						// 统一状态处理
						if (matchCfg.templateType) {
							if (result.score < matchCfg.scoreThreshold) {
								result.valid = 2; // 匹配得分过低
							}
							else if (result.angle < matchCfg.angleThreshold[0] || result.angle > matchCfg.angleThreshold[1]) {
								result.valid = 3; // 歪斜
							}
							else if (std::abs(result.shiftHor) > matchCfg.shiftHor) {
								result.valid = 5; // 水平偏移过大
							}
							else if (std::abs(result.shiftVer) > matchCfg.shiftVer) {
								result.valid = 6; // 垂直偏移过大
							}
							else if (result.offset > matchCfg.offset) {
								result.valid = 7; // 距离偏移过大
							}
							else {
								result.valid = 1; // 匹配成功 
							}
						}
						else {
							if (result.score >= matchCfg.scoreThreshold) {
								result.valid = 4; // 错误特征
							}
						}
					}
					if (result.valid > 0) {
						guard.release();
					}
				}
			}
			catch (HalconCpp::HException& e) {
				if (e.ErrorCode() == H_ERR_TIMEOUT) {
					result.timeout = true;
				}
				else {
					std::cerr << "Halcon error [" << kk << "]: " << e.ErrorMessage() << std::endl;
				}
			}
			catch (...) {
				// 其他异常处理
			}
			return result;
			}));
	}

	// 3. 结果收集与处理
	int bestIndex = -1;
	double maxScore = -1.0;
	bool timeoutOccurred = false;
	bool anyMatchFound = false;

	for (int i = 0; i < futures.size(); ++i) {
		auto status = futures[i].wait_for(std::chrono::milliseconds(matchCfg.timeOut + 500));
		if (status == std::future_status::ready) {
			try {
				auto result = futures[i].get();
				results[i] = result;
				if (result.timeout) {
					timeoutOccurred = true;
				}
				else if (result.valid > 0) {
					anyMatchFound = true;
					if (result.score > maxScore) {
						maxScore = result.score;
						bestIndex = i;
					}
				}
			}
			catch (const std::exception& e) {
				std::cerr << "Exception when getting result from future [" << i << "]: " << e.what() << std::endl;
				results[i].id = i;
				results[i].valid = 0;
			}
		}
		else {
			results[i].id = i;
			results[i].timeout = true;
			timeoutOccurred = true;
		}
	}

	// 4. 返回结果
	if (timeoutOccurred) {
		return 8; // 超时状态码
	}

	if (bestIndex >= 0) {
		resultFinal = results[bestIndex];
		return resultFinal.valid; // 直接返回具体的匹配状态码
	}

	if (anyMatchFound) {
		return 0; // 匹配失败（找到了匹配但未满足条件）
	}
	else {
		return 9; // 无匹配
	}
}


int MatchFun::MatchNCC(cv::Mat src, MatchConfig matchCfg, MatchResult& result) {
	// 初始化结果
	result.id = -1;
	result.score = 0;
	result.angle = 0;
	result.valid = 0;
	result.timeout = false;
	result.shiftHor = 0;
	result.shiftVer = 0;
	result.offset = 0;
	result.adjustedRoiX = 0;
	result.adjustedRoiY = 0;

	if (src.empty() || matchCfg.templateMats.empty()) { 
		return 0; // 匹配失败
	}

	try {
		double bestScore = -1;
		cv::Rect bestRect;
		int bestId = -1;
		cv::Rect bestAdjustedRoi;

		// 对每个模板进行匹配
		for (int i = 0; i < matchCfg.templateMats.size(); i++) {
			if (matchCfg.rois.empty() || i >= matchCfg.rois.size()) {
				continue;
			}

			cv::Rect originalRoi = matchCfg.rois[i];
			// 扩展ROI以考虑边界情况
			cv::Rect extendedRoi = originalRoi;
			extendedRoi.x -= matchCfg.extW;
			extendedRoi.y -= matchCfg.extH;
			extendedRoi.width += 2 * matchCfg.extW;
			extendedRoi.height += 2 * matchCfg.extH;

			// 调整ROI确保在图像范围内
			cv::Rect adjustedRoi = ANA->AdjustROI(extendedRoi, src);
			cv::Mat roiImg = src(adjustedRoi);

			// 转换为灰度图
			if (roiImg.channels() == 3) {
				cv::cvtColor(roiImg, roiImg, cv::COLOR_BGR2GRAY);
			}

			cv::Mat templ = matchCfg.templateMats[i];
			if (templ.channels() == 3) {
				cv::cvtColor(templ, templ, cv::COLOR_BGR2GRAY);
			}

			// 检查模板尺寸
			if (roiImg.cols < templ.cols || roiImg.rows < templ.rows) {
				continue;
			}

			// 执行NCC匹配
			cv::Mat resultMat;
			cv::matchTemplate(roiImg, templ, resultMat, cv::TM_CCOEFF_NORMED);

			// 找到最佳匹配位置
			double minVal, maxVal;
			cv::Point minLoc, maxLoc;
			cv::minMaxLoc(resultMat, &minVal, &maxVal, &minLoc, &maxLoc);

			double currentScore = maxVal;

			if (currentScore > bestScore) {
				bestScore = currentScore;
				bestId = i;
				bestRect = cv::Rect(maxLoc.x + adjustedRoi.x, maxLoc.y + adjustedRoi.y,
					templ.cols, templ.rows);
				bestAdjustedRoi = adjustedRoi;
			}
		}

		if (bestId != -1 && bestScore >= matchCfg.scoreThreshold) {
			result.id = bestId;
			result.score = bestScore;
			result.rect = bestRect;
			result.center = cv::Point2d(bestRect.x + bestRect.width / 2.0,
				bestRect.y + bestRect.height / 2.0);

			// 计算旋转矩形（NCC没有角度信息，所以角度为0）
			result.boundingRect = cv::RotatedRect(result.center,
				cv::Size2f(bestRect.width, bestRect.height), 0);

			// 计算四个角点
			cv::Point2f corners[4];
			result.boundingRect.points(corners);
			result.corners.assign(corners, corners + 4);

			// 计算偏移量
			result.shiftHor = result.center.x - matchCfg.templateCenterX;
			result.shiftVer = result.center.y - matchCfg.templateCenterY;
			result.offset = std::sqrt(result.shiftHor * result.shiftHor +
				result.shiftVer * result.shiftVer);

			result.adjustedRoiX = bestAdjustedRoi.x;
			result.adjustedRoiY = bestAdjustedRoi.y;

			// 根据阈值验证匹配结果
			if (result.score < matchCfg.scoreThreshold) {
				result.valid = 2; // 匹配得分过低
			}
			else if (std::abs(result.shiftHor) > matchCfg.shiftHor) {
				result.valid = 5; // 水平偏移过大
			}
			else if (std::abs(result.shiftVer) > matchCfg.shiftVer) {
				result.valid = 6; // 垂直偏移过大
			}
			else if (result.offset > matchCfg.offset) {
				result.valid = 7; // 距离偏移过大
			}
			else {
				result.valid = 1; // 匹配成功
			}

			return result.valid;
		}

		return 0; // 匹配失败

	}
	catch (const std::exception& e) {
		std::cerr << "NCC匹配异常: " << e.what() << std::endl;
		return 0;
	}
}

// 修复旋转矩形角度偏差问题
float MatchFun::FixRotatedRectAngle(float angle) {
	// 标准化到-90到90度范围
	angle = NormalizeRotatedRectAngleTo_90_90(angle);

	// 确保角度在-90到90度范围内
	if (angle < -90.0f) angle = -90.0f;
	if (angle >= 90.0f) angle = 89.9f; // 避免等于90

	return angle;
}



// 多尺度特征检测
std::vector<cv::KeyPoint> MatchFun::MultiScaleFeatureDetection(const cv::Mat& input) {
	std::vector<cv::KeyPoint> allKeypoints;
	cv::Ptr<cv::SIFT> detector = cv::SIFT::create();

	try {
		// 在不同尺度下检测特征
		std::vector<double> scales = { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 };

		for (double scale : scales) {
			cv::Mat resized;
			cv::resize(input, resized, cv::Size(), scale, scale);

			std::vector<cv::KeyPoint> keypoints;
			cv::Mat descriptors;
			detector->detect(resized, keypoints);

			// 将关键点坐标转换回原始尺度
			for (auto& kp : keypoints) {
				kp.pt.x /= scale;
				kp.pt.y /= scale;
				kp.size /= scale;
			}

			// 添加到总关键点集合
			allKeypoints.insert(allKeypoints.end(), keypoints.begin(), keypoints.end());
		}

		// 去除重复的关键点
		std::sort(allKeypoints.begin(), allKeypoints.end(),
			[](const cv::KeyPoint& a, const cv::KeyPoint& b) {
				return a.response > b.response;
			});

		// 使用空间哈希去除邻近的重复点
		std::vector<cv::KeyPoint> filteredKeypoints;
		const float minDistance = 5.0f;

		for (const auto& kp : allKeypoints) {
			bool isDuplicate = false;
			for (const auto& existingKp : filteredKeypoints) {
				float dist = cv::norm(kp.pt - existingKp.pt);
				if (dist < minDistance) {
					isDuplicate = true;
					break;
				}
			}
			if (!isDuplicate) {
				filteredKeypoints.push_back(kp);
			}
		}

		return filteredKeypoints;

	}
	catch (const std::exception& e) {
		std::cout << "多尺度特征检测异常!" << std::endl;
		return allKeypoints;
	}
}

cv::Ptr<cv::SIFT> MatchFun::CreateSIFTDetectorFromHalconConfig(const MatchConfig& matchCfg) {
	try {
		// 将Halcon参数映射到SIFT参数
		int nfeatures = 0; // 保留所有特征点

		// 使用Halcon的金字塔层数作为SIFT的八度层数
		int nOctaveLayers = std::max(3, std::min(5, matchCfg.numLevels));

		// 将Halcon的对比度参数转换为SIFT的对比度阈值
		// Halcon的minContrast通常在5-50范围，SIFT的contrastThreshold通常在0.01-0.1范围
		double contrastThreshold = std::max(0.005, std::min(0.1, matchCfg.minContrast / 500.0));

		// 将Halcon的对比度参数转换为SIFT的边缘阈值
		// Halcon的contrast通常在10-30范围，SIFT的edgeThreshold通常在5-15范围
		double edgeThreshold = std::max(5.0, std::min(15.0, matchCfg.contrast / 2.0));

		// 基于贪婪度调整模糊参数
		// Halcon的greediness在0.0-1.0范围，SIFT的sigma通常在1.0-2.0范围
		double sigma = 1.4 + (matchCfg.greediness * 0.6);

		
		return cv::SIFT::create(nfeatures, nOctaveLayers, contrastThreshold, edgeThreshold, sigma);
	}
	catch (const std::exception& e) {	
		return cv::SIFT::create(0, 3, 0.04, 10, 1.6);
	}
}

int MatchFun::MatchSIFT(cv::Mat src, MatchConfig matchCfg, MatchResult& result) {
	// 初始化结果
	result.id = -1;
	result.score = 0;
	result.angle = 0;
	result.valid = 0;
	result.timeout = false;
	result.shiftHor = 0;
	result.shiftVer = 0;
	result.offset = 0;
	result.adjustedRoiX = 0;
	result.adjustedRoiY = 0;

	if (src.empty() || matchCfg.roiTemplates.empty()) {
		return 0; // 匹配失败
	}

	try {
		double bestScore = -1;
		int bestId = -1;
		std::vector<cv::KeyPoint> bestSrcKeypoints;
		cv::Mat bestSrcDescriptors;
		std::vector<cv::DMatch> bestMatches;
		cv::Rect bestAdjustedRoi;

		cv::Ptr<cv::SIFT> detector = CreateSIFTDetectorFromHalconConfig(matchCfg);

		// 对每个ROI模板进行匹配
		for (int i = 0; i < matchCfg.roiTemplates.size(); i++) {
			if (matchCfg.rois.empty() || i >= matchCfg.rois.size() ||
				matchCfg.templateMatsKeypoints[i].empty() ||
				matchCfg.templateMatsDescriptors[i].empty()) {
				continue;
			}

			cv::Rect originalRoi = matchCfg.rois[i];

			// 扩展ROI以考虑边界情况
			cv::Rect extendedRoi = originalRoi;
			extendedRoi.x -= matchCfg.extW;
			extendedRoi.y -= matchCfg.extH;
			extendedRoi.width += 2 * matchCfg.extW;
			extendedRoi.height += 2 * matchCfg.extH;

			// 调整ROI确保在图像范围内
			cv::Rect adjustedRoi = ANA->AdjustROI(extendedRoi, src);
			cv::Mat roiImg = src(adjustedRoi);

			if (roiImg.channels() == 3) {
				cv::cvtColor(roiImg, roiImg, cv::COLOR_BGR2GRAY);
			}

			// 提取源图像ROI区域的特征
			std::vector<cv::KeyPoint> srcKeypoints;
			cv::Mat srcDescriptors;
			detector->detectAndCompute(roiImg, cv::noArray(), srcKeypoints, srcDescriptors);

			if (srcKeypoints.empty()) {
				continue;
			}

			// 特征匹配
			cv::Ptr<cv::DescriptorMatcher> matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);
			std::vector<std::vector<cv::DMatch>> knnMatches;
			matcher->knnMatch(srcDescriptors, matchCfg.templateMatsDescriptors[i], knnMatches, 2);

			// Lowe's ratio test
			std::vector<cv::DMatch> goodMatches;
			const float ratioThresh = 0.7f;
			for (size_t j = 0; j < knnMatches.size(); j++) {
				if (knnMatches[j].size() >= 2 &&
					knnMatches[j][0].distance < ratioThresh * knnMatches[j][1].distance) {
					goodMatches.push_back(knnMatches[j][0]);
				}
			}

			double currentScore = goodMatches.size();

			if (currentScore > bestScore) {
				bestScore = currentScore;
				bestId = i;
				bestSrcKeypoints = srcKeypoints;
				bestSrcDescriptors = srcDescriptors;
				bestMatches = goodMatches;
				bestAdjustedRoi = adjustedRoi;
			}
		}

		if (bestId != -1 && bestScore >= matchCfg.scoreThreshold) {
			result.id = bestId;

			// 修复1: 得分限制在100以内
			double maxPossibleScore = matchCfg.templateMatsKeypoints[bestId].size();
			result.score = std::min(1.0, (bestScore * 1.0 / maxPossibleScore) );

			// 计算匹配点对
			std::vector<cv::Point2f> srcPoints, templatePoints;
			for (const auto& match : bestMatches) {
				// 源图像特征点（需要转换到原图坐标系）
				cv::Point2f srcPt = bestSrcKeypoints[match.queryIdx].pt;
				srcPt.x += bestAdjustedRoi.x;
				srcPt.y += bestAdjustedRoi.y;
				srcPoints.push_back(srcPt);

				// 模板特征点（在ROI模板中的坐标）
				templatePoints.push_back(matchCfg.templateMatsKeypoints[bestId][match.trainIdx].pt);
			}

			// 计算单应性矩阵来估计变换
			if (srcPoints.size() >= 4) {
				std::vector<uchar> inliers;
				// 使用RANSAC计算单应性矩阵
				cv::Mat H = cv::findHomography(templatePoints, srcPoints, cv::RANSAC, 3.0, inliers);

				if (!H.empty()) {
					// 使用模板ROI的四个角点进行变换
					cv::Rect templateRoi = matchCfg.templateRois[bestId];
					std::vector<cv::Point2f> templateCorners(4);
					templateCorners[0] = cv::Point2f(0, 0);  // 模板ROI内的相对坐标
					templateCorners[1] = cv::Point2f(templateRoi.width, 0);
					templateCorners[2] = cv::Point2f(templateRoi.width, templateRoi.height);
					templateCorners[3] = cv::Point2f(0, templateRoi.height);


					//templateCorners[0] = cv::Point2f(matchCfg.rois[bestId].x, matchCfg.rois[bestId].y);  // 模板ROI内的相对坐标
					//templateCorners[1] = cv::Point2f(matchCfg.rois[bestId].x + matchCfg.rois[bestId].width, matchCfg.rois[bestId].y);
					//templateCorners[2] = cv::Point2f(matchCfg.rois[bestId].x + matchCfg.rois[bestId].width, matchCfg.rois[bestId].y + matchCfg.rois[bestId].height);
					//templateCorners[3] = cv::Point2f(matchCfg.rois[bestId].x, matchCfg.rois[bestId].y + templateRoi.height);

					std::vector<cv::Point2f> transformedCorners(4);
					cv::perspectiveTransform(templateCorners, transformedCorners, H);

					// 将变换后的角点转换到原图坐标系
					/*for (auto& corner : transformedCorners) {
						corner.x -= bestAdjustedRoi.x;
						corner.y -= bestAdjustedRoi.y;
					}*/

					// 检查变换后的角点是否在合理范围内
					bool validTransform = true;
					cv::Rect imageBounds(0, 0, src.cols, src.rows);

					for (const auto& corner : transformedCorners) {
						if (corner.x < -imageBounds.width * 0.1 || corner.x > imageBounds.width * 1.1 ||
							corner.y < -imageBounds.height * 0.1 || corner.y > imageBounds.height * 1.1) {
							validTransform = false;
							break;
						}
					}

					if (!validTransform) {
						// 使用替代方法：基于匹配点计算边界框
						cv::Rect boundingBox = cv::boundingRect(srcPoints);
						result.boundingRect = cv::RotatedRect(
							cv::Point2f(boundingBox.x + boundingBox.width / 2.0f,
								boundingBox.y + boundingBox.height / 2.0f),
							cv::Size2f(boundingBox.width, boundingBox.height),
							0.0f
						);
						result.angle = 0;
						result.center = result.boundingRect.center;
						result.rect = boundingBox;
						result.corners.clear();
						for (int i = 0; i < 4; i++) {
							cv::Point2f pt;
							pt.x = (i == 0 || i == 3) ? boundingBox.x : boundingBox.x + boundingBox.width;
							pt.y = (i < 2) ? boundingBox.y : boundingBox.y + boundingBox.height;
							result.corners.push_back(pt);
						}
					}
					else {
						// 计算最小外接矩形
						cv::RotatedRect rotatedRect = cv::minAreaRect(transformedCorners);

						// 修复2: 处理角度偏差问题
						result.angle = CalculateRobustAngleFromMatches(
							bestSrcKeypoints,
							matchCfg.templateMatsKeypoints[bestId],
							bestMatches,
							transformedCorners
						);
						result.center = rotatedRect.center;
						

						// 处理边界框尺寸
						cv::Size2f size = cv::Size2f(templateRoi.width, templateRoi.height);
						/*if (size.width <= 0 || size.height <= 0) {
							size = cv::Size2f(templateRoi.width, templateRoi.height);
						}*/

						// 处理宽高交换问题（当角度接近±90度时）
						if (std::abs(result.angle) > 45.0f) {
							// 角度在±45度以上，确保宽高合理
							if (size.width < size.height) {
								std::swap(size.width, size.height);
								// 调整角度，保持在-90到90度范围内
								result.angle = NormalizeAngleTo_90_90(result.angle + 90.0f);
							}
						}

						result.boundingRect = cv::RotatedRect(result.center, size, result.angle);

						// 计算矩形边界
						result.rect = cv::boundingRect(transformedCorners);
						result.corners = transformedCorners;
					}

					// 计算偏移量（相对于模板预设的中心点）
					result.shiftHor = result.center.x - matchCfg.templateCenterX;
					result.shiftVer = result.center.y - matchCfg.templateCenterY;
					result.offset = std::sqrt(result.shiftHor * result.shiftHor +
						result.shiftVer * result.shiftVer);

					result.adjustedRoiX = bestAdjustedRoi.x;
					result.adjustedRoiY = bestAdjustedRoi.y;

					// 根据阈值验证匹配结果
					if (result.score < matchCfg.scoreThreshold) {
						result.valid = 2; // 匹配得分过低
					}
					else if (result.angle < matchCfg.angleThreshold[0] ||
						result.angle > matchCfg.angleThreshold[1]) {
						result.valid = 3; // 歪斜
					}
					else if (std::abs(result.shiftHor) > matchCfg.shiftHor) {
						result.valid = 5; // 水平偏移过大
					}
					else if (std::abs(result.shiftVer) > matchCfg.shiftVer) {
						result.valid = 6; // 垂直偏移过大
					}
					else if (result.offset > matchCfg.offset) {
						result.valid = 7; // 距离偏移过大
					}
					else {
						result.valid = 1; // 匹配成功
					}

					return result.valid;
				}
			}

			// 如果单应性矩阵计算失败，使用基于匹配点的方法
			if (!srcPoints.empty()) {
				cv::Rect boundingBox = cv::boundingRect(srcPoints);
				result.boundingRect = cv::RotatedRect(
					cv::Point2f(boundingBox.x + boundingBox.width / 2.0f,
						boundingBox.y + boundingBox.height / 2.0f),
					cv::Size2f(boundingBox.width, boundingBox.height),
					0.0f
				);
				result.angle = 0;
				result.center = result.boundingRect.center;
				result.rect = boundingBox;
				result.corners.clear();
				for (int i = 0; i < 4; i++) {
					cv::Point2f pt;
					pt.x = (i == 0 || i == 3) ? boundingBox.x : boundingBox.x + boundingBox.width;
					pt.y = (i < 2) ? boundingBox.y : boundingBox.y + boundingBox.height;
					result.corners.push_back(pt);
				}

				// 计算偏移量
				result.shiftHor = result.center.x - matchCfg.templateCenterX;
				result.shiftVer = result.center.y - matchCfg.templateCenterY;
				result.offset = std::sqrt(result.shiftHor * result.shiftHor +
					result.shiftVer * result.shiftVer);

				result.adjustedRoiX = bestAdjustedRoi.x;
				result.adjustedRoiY = bestAdjustedRoi.y;

				result.valid = 1; // 匹配成功
				return result.valid;
			}
		}

		return 0; // 匹配失败

	}
	catch (const std::exception& e) {
		std::cerr << "SIFT匹配异常: " << e.what() << std::endl;
		return 0;
	}
}

// 更完善的角度标准化函数
float MatchFun::NormalizeAngleTo_90_90(float angle) {
	// 先转换到-180到180度范围
	angle = fmod(angle, 360.0f);
	if (angle > 180.0f) angle -= 360.0f;
	if (angle < -180.0f) angle += 360.0f;

	// 再转换到-90到90度范围（利用180度对称性）
	if (angle > 90.0f) {
		angle = angle - 180.0f;
	}
	else if (angle < -90.0f) {
		angle = angle + 180.0f;
	}

	return angle;
}

// 专门处理旋转矩形的角度
float MatchFun::NormalizeRotatedRectAngleTo_90_90(float angle) {
	// OpenCV的minAreaRect返回[-90, 0)范围
	// 直接使用这个范围，不需要转换
	return angle;
}

// 基于特征点匹配的稳健角度计算方法
float MatchFun::CalculateRobustAngleFromMatches(
	const std::vector<cv::KeyPoint>& srcKeypoints,
	const std::vector<cv::KeyPoint>& templateKeypoints,
	const std::vector<cv::DMatch>& matches,
	const std::vector<cv::Point2f>& transformedCorners) {

	if (matches.size() < 4) {
		return 0.0f; // 匹配点太少，返回0度
	}

	try {
		// 方法1: 使用单应性矩阵分解计算角度
		float angleFromHomography = CalculateAngleFromHomography(matches, srcKeypoints, templateKeypoints);

		// 方法2: 使用特征点方向计算角度
		float angleFromFeatures = CalculateAngleFromFeatureDirections(srcKeypoints, templateKeypoints, matches);

		// 方法3: 使用匹配点对的方向变化计算角度
		float angleFromMatches = CalculateAngleFromMatchVectors(srcKeypoints, templateKeypoints, matches);

		// 综合三种方法，选择最一致的角度
		return CombineAngles(angleFromHomography, angleFromFeatures, angleFromMatches);

	}
	catch (const std::exception& e) {
		std::cerr << "角度计算异常: " << e.what() << std::endl;
		return 0.0f;
	}
}

// 从单应性矩阵计算角度
float MatchFun::CalculateAngleFromHomography(
	const std::vector<cv::DMatch>& matches,
	const std::vector<cv::KeyPoint>& srcKeypoints,
	const std::vector<cv::KeyPoint>& templateKeypoints) {

	if (matches.size() < 4) return 0.0f;

	// 提取匹配点
	std::vector<cv::Point2f> srcPoints, templatePoints;
	for (const auto& match : matches) {
		srcPoints.push_back(srcKeypoints[match.queryIdx].pt);
		templatePoints.push_back(templateKeypoints[match.trainIdx].pt);
	}

	// 计算单应性矩阵
	cv::Mat H = cv::findHomography(templatePoints, srcPoints, cv::RANSAC);
	if (H.empty()) return 0.0f;

	// 从单应性矩阵提取旋转角度
	// H = K * [R|t] * K^-1，但这里我们简化处理
	// 提取旋转矩阵的近似角度
	double angle = atan2(H.at<double>(1, 0), H.at<double>(0, 0)) * 180.0 / CV_PI;

	return NormalizeAngleTo_90_90(static_cast<float>(angle));
}

// 使用匹配点对的向量方向计算角度
float MatchFun::CalculateAngleFromMatchVectors(
	const std::vector<cv::KeyPoint>& srcKeypoints,
	const std::vector<cv::KeyPoint>& templateKeypoints,
	const std::vector<cv::DMatch>& matches) {

	if (matches.size() < 2) return 0.0f;

	std::vector<float> angles;

	// 随机选择多对匹配点计算角度
	const int numPairs = std::min(10, static_cast<int>(matches.size() / 2));
	cv::RNG rng(12345);

	for (int i = 0; i < numPairs; i++) {
		// 随机选择两个不同的匹配点
		int idx1 = rng.uniform(0, static_cast<int>(matches.size()));
		int idx2;
		do {
			idx2 = rng.uniform(0, static_cast<int>(matches.size()));
		} while (idx2 == idx1);

		const auto& match1 = matches[idx1];
		const auto& match2 = matches[idx2];

		// 计算源图像中的向量
		cv::Point2f srcVec = srcKeypoints[match1.queryIdx].pt - srcKeypoints[match2.queryIdx].pt;
		float srcAngle = atan2(srcVec.y, srcVec.x) * 180.0f / CV_PI;

		// 计算模板图像中的向量
		cv::Point2f templateVec = templateKeypoints[match1.trainIdx].pt - templateKeypoints[match2.trainIdx].pt;
		float templateAngle = atan2(templateVec.y, templateVec.x) * 180.0f / CV_PI;

		// 计算角度差
		float angleDiff = srcAngle - templateAngle;
		angles.push_back(NormalizeAngleTo_90_90(angleDiff));
	}

	// 使用中值作为最终角度
	if (angles.empty()) return 0.0f;

	std::sort(angles.begin(), angles.end());
	return angles[angles.size() / 2];
}

// 综合多种角度计算方法
float MatchFun::CombineAngles(float angle1, float angle2, float angle3) {
	std::vector<float> angles = { angle1, angle2, angle3 };

	// 计算角度的一致性
	float consistency = CalculateAngleConsistency(angles);

	// 如果角度一致性高，使用加权平均
	if (consistency > 0.8f) {
		// 根据方法可靠性加权
		return (angle1 * 0.4f + angle2 * 0.4f + angle3 * 0.2f);
	}

	// 否则选择最可能的角度
	return SelectMostLikelyAngle(angles);
}

// 计算角度一致性
float MatchFun::CalculateAngleConsistency(const std::vector<float>& angles) {
	if (angles.size() < 2) return 1.0f;

	float maxDiff = 0.0f;
	for (size_t i = 0; i < angles.size(); i++) {
		for (size_t j = i + 1; j < angles.size(); j++) {
			float diff = std::abs(angles[i] - angles[j]);
			// 考虑角度循环特性
			if (diff > 90.0f) diff = 180.0f - diff;
			maxDiff = std::max(maxDiff, diff);
		}
	}

	// 最大差异越小，一致性越高
	return 1.0f - (maxDiff / 90.0f);
}

// 选择最可能的角度
float MatchFun::SelectMostLikelyAngle(const std::vector<float>& angles) {
	if (angles.empty()) return 0.0f;

	// 简单实现：选择绝对值最小的角度（通常更可靠）
	float bestAngle = angles[0];
	float minAbs = std::abs(angles[0]);

	for (size_t i = 1; i < angles.size(); i++) {
		if (std::abs(angles[i]) < minAbs) {
			minAbs = std::abs(angles[i]);
			bestAngle = angles[i];
		}
	}

	return bestAngle;
}

// 基于特征点方向的角度计算（确保返回-90到90度）
float MatchFun::CalculateAngleFromFeatureDirections(
	const std::vector<cv::KeyPoint>& srcKeypoints,
	const std::vector<cv::KeyPoint>& templateKeypoints,
	const std::vector<cv::DMatch>& matches) {

	if (matches.empty()) return 0.0f;

	std::vector<float> angleDiffs;

	for (const auto& match : matches) {
		float srcAngle = srcKeypoints[match.queryIdx].angle;
		float templateAngle = templateKeypoints[match.trainIdx].angle;

		// 计算角度差异（考虑360度循环）
		float angleDiff = srcAngle - templateAngle;

		// 标准化到 [-180, 180] 范围
		if (angleDiff > 180.0f) angleDiff -= 360.0f;
		if (angleDiff < -180.0f) angleDiff += 360.0f;

		angleDiffs.push_back(angleDiff);
	}

	// 使用中值减少异常值影响
	std::sort(angleDiffs.begin(), angleDiffs.end());
	float medianAngle = angleDiffs[angleDiffs.size() / 2];

	// 转换为-90到90度范围
	return NormalizeAngleTo_90_90(medianAngle);
}


int MatchFun::MatchLocateHalcon(cv::Mat src, MatchLocateConfig matchCfg, MatchResult& resultFinal)
{
	// 1. 输入验证（保持不变）
	if (src.empty()) return -1;
	if (matchCfg.labelAllTemplateHObjects.empty() || matchCfg.templateMats.empty()) {
		return -3; // 模板为空
	}
	if (matchCfg.labelAllTemplateHObjects.size() != matchCfg.templateMats.size()) {
		return -4; // 模板数量不匹配
	}
	if (matchCfg.templatePose.x < 0 || matchCfg.templatePose.y < 0 ||
		matchCfg.templatePose.x + matchCfg.templatePose.width > src.cols ||
		matchCfg.templatePose.y + matchCfg.templatePose.height > src.rows) {
		return -2; // ROI无效
	}



	// 针对 contrast 过高时的长尾耗时，采用更保守的参数避免搜索空间爆炸
	const bool aggressiveContrast = (matchCfg.contrast >= 60);
	const int tunedNumLevels = aggressiveContrast ? std::min(matchCfg.numLevels, 3) : matchCfg.numLevels;
	const double tunedGreediness = aggressiveContrast ? std::max(matchCfg.greediness, 0.92) : matchCfg.greediness;
	const char* tunedSubPixel = aggressiveContrast ? "none" : GetSubPixelString(matchCfg.subPixel);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(matchCfg.timeOut + 500);

	// 3. 多模板并行匹配（优化valid状态处理）
	std::vector<std::future<MatchResult>> futures;
	std::vector<MatchResult> results(matchCfg.labelAllTemplateHObjects.size());

	for (int kk = 0; kk < matchCfg.labelAllTemplateHObjects.size(); ++kk) {
		futures.emplace_back(std::async(std::launch::async, [&, kk]() {
			// 2. 准备ROI区域
			cv::Rect roiCur = matchCfg.templatePose;
			roiCur.x -= matchCfg.extW;
			roiCur.y -= matchCfg.extH;
			roiCur.width += 2 * matchCfg.extW;
			roiCur.height += 2 * matchCfg.extH;
			roiCur = ANA->AdjustROI(roiCur, src);

			cv::Mat roi = src(roiCur);
			if (roi.channels() == 3) {
				cv::cvtColor(roi, roi, cv::COLOR_BGR2GRAY);
			}

			MatchResult result;
			result.id = kk;
			result.valid = 0; // 默认匹配失败

			try {
				HalconCpp::HTuple hv_ModelID;
				{
					HalconResourceGuard guard(hv_ModelID);

					// 转换图像
					ANA->Mat2HObject(roi, result.ho_Image);

					// 执行匹配（保持不变）
					HalconCpp::HTuple coarseRow, coarseColumn, coarseAngle, coarseScale, coarseScore;
					const double coarseMinScore = std::max(0.6, matchCfg.scoreThreshold * 0.75);
					const int coarseNumMatches = 1;
					const int coarseLevels = std::max(0, tunedNumLevels - 1);
					switch (matchCfg.matchType) {
					case 0:
						HalconCpp::FindShapeModel(
							result.ho_Image,
							matchCfg.labelAllTemplateHTuple[kk],
							matchCfg.angleRange[0] * CV_PI / 180.0,
							(matchCfg.angleRange[1] - matchCfg.angleRange[0]) * CV_PI / 180.0,
							coarseMinScore,
							coarseNumMatches,
							matchCfg.maxOverlap,
							"none",
							coarseLevels,
							tunedGreediness,
							&coarseRow,
							&coarseColumn,
							&coarseAngle,
							&coarseScore);
						if (coarseScore.Length() <= 0 || coarseScore[0].D() < coarseMinScore) {
							break;
						}
						HalconCpp::FindShapeModel(
							result.ho_Image,
							matchCfg.labelAllTemplateHTuple[kk],
							matchCfg.angleRange[0] * CV_PI / 180.0,
							(matchCfg.angleRange[1] - matchCfg.angleRange[0]) * CV_PI / 180.0,
							0.5,
							matchCfg.numMatches,
							matchCfg.maxOverlap,
							tunedSubPixel,
							tunedNumLevels,
							tunedGreediness,
							&result.hv_Row,
							&result.hv_Column,
							&result.hv_Angle,
							&result.hv_Score);
						result.hv_Scale = HalconCpp::HTuple(1.0);
						break;
					case 1:
						HalconCpp::FindScaledShapeModel(
							result.ho_Image,
							matchCfg.labelAllTemplateHTuple[kk],
							matchCfg.angleRange[0] * CV_PI / 180.0,
							(matchCfg.angleRange[1] - matchCfg.angleRange[0]) * CV_PI / 180.0,
							matchCfg.scaleRange[0],
							matchCfg.scaleRange[1],
							coarseMinScore,
							coarseNumMatches,
							matchCfg.maxOverlap,
							"none",
							coarseLevels,
							tunedGreediness,
							&coarseRow,
							&coarseColumn,
							&coarseAngle,
							&coarseScale,
							&coarseScore);
						if (coarseScore.Length() <= 0 || coarseScore[0].D() < coarseMinScore) {
							break;
						}
						HalconCpp::FindScaledShapeModel(
							result.ho_Image,
							matchCfg.labelAllTemplateHTuple[kk],
							matchCfg.angleRange[0] * CV_PI / 180.0,
							(matchCfg.angleRange[1] - matchCfg.angleRange[0]) * CV_PI / 180.0,
							matchCfg.scaleRange[0],
							matchCfg.scaleRange[1],
							0.5,
							matchCfg.numMatches,
							matchCfg.maxOverlap,
							tunedSubPixel,
							tunedNumLevels,
							tunedGreediness,
							&result.hv_Row,
							&result.hv_Column,
							&result.hv_Angle,
							&result.hv_Scale,
							&result.hv_Score);
						break;
					}

					// 统一处理匹配结果
					if (result.hv_Score.Length() > 0 && result.hv_Score[0].D() >= 0) {
						result.score = result.hv_Score[0].D();
						result.angle = -result.hv_Angle[0].D() * 180.0 / CV_PI;
						while (result.angle >= 180.0) result.angle -= 360.0;
						while (result.angle < -180.0) result.angle += 360.0;
						double scale = (matchCfg.matchType == 1) ? result.hv_Scale[0].D() : 1.0;
						result.boundingRect = CAL->CALC_RotatedRect(
							result.hv_Column[0].D() + matchCfg.templatePose.x - matchCfg.extW,
							result.hv_Row[0].D() + matchCfg.templatePose.y - matchCfg.extH,
							result.angle,
							scale * matchCfg.templateMats[kk].cols,
							scale * matchCfg.templateMats[kk].rows);

						cv::Point2f corners[4];
						result.boundingRect.points(corners);
						result.corners.assign(corners, corners + 4);

						// 计算匹配中心点（原图坐标）
						result.center.x = result.hv_Column[0].D() + matchCfg.templatePose.x - matchCfg.extW;
						result.center.y = result.hv_Row[0].D() + matchCfg.templatePose.y - matchCfg.extH;

						// 计算偏移量
						result.shiftHor = result.center.x - matchCfg.templateCenterX;
						result.shiftVer = result.center.y - matchCfg.templateCenterY;
						result.offset = std::sqrt(
							result.shiftHor * result.shiftHor +
							result.shiftVer * result.shiftVer
						);

						// 检查分数是否达到阈值
						if (result.score < matchCfg.scoreThreshold) {
							result.valid = 2; // 匹配得分过低
						}
						// 检查角度约束
						else if (result.angle < matchCfg.angleThreshold[0] ||
							result.angle > matchCfg.angleThreshold[1]) {
							result.valid = 3; // 歪斜
						}
						// 检查偏移约束
						else if (std::abs(result.shiftHor) > matchCfg.shiftHor) {
							result.valid = 5; // 水平偏移过大
						}
						else if (std::abs(result.shiftVer) > matchCfg.shiftVer) {
							result.valid = 6; // 垂直偏移过大
						}
						else if (result.offset > matchCfg.offset) {
							result.valid = 7; // 距离偏移过大
						}
						else {
							result.valid = 1; // 匹配成功
						}
					}
					// 匹配成功则释放资源
					if (result.valid > 0) {
						guard.release();
					}
				}
			}
			catch (HalconCpp::HException& e) {
				if (e.ErrorCode() == H_ERR_TIMEOUT) {
					result.timeout = true;
				}
				else {
					std::cerr << "Halcon error [" << kk << "]: " << e.ErrorMessage() << std::endl;
				}
			}
			catch (...) {
				// 其他异常处理
			}

			return result;
			}));
	}

	// 4. 结果收集与处理（优化）
	int bestIndex = -1;
	double maxScore = -1.0; // 初始化为最小值
	bool timeoutOccurred = false;
	bool anyMatchFound = false;

	for (int i = 0; i < futures.size(); ++i) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline) {
			results[i].id = i;
			results[i].timeout = true;
			timeoutOccurred = true;
			continue;
		}
		auto status = futures[i].wait_until(deadline);

		if (status == std::future_status::ready) {
			auto result = futures[i].get();
			results[i] = result;

			if (result.timeout) {
				timeoutOccurred = true;
			}
			else if (result.valid > 0) {  // 只考虑有效匹配结果
				anyMatchFound = true;
				if (result.score > maxScore) {
					maxScore = result.score;
					bestIndex = i;
				}
			}
		}
		else {
			results[i].id = i;
			results[i].timeout = true;
			timeoutOccurred = true;
		}
	}

	// 5. 返回结果（重构状态码逻辑）
	if (timeoutOccurred) {
		return 8; // 超时状态码
	}

	if (bestIndex >= 0) {
		resultFinal = results[bestIndex];

		// 返回具体的匹配状态
		switch (resultFinal.valid) {
		case 1: return 1; // 匹配成功
		case 2: return 2; // 得分过低
		case 3: return 3; // 歪斜
		case 4: return 4; // 错误特征
		case 5: return 5; // 水平偏移
		case 6: return 6; // 垂直偏移
		case 7: return 7; // 距离偏移
		default: return 0; // 匹配失败
		}
	}

	// 处理无有效匹配的情况
	if (anyMatchFound) {
		// 虽然找到匹配但未满足条件
		return 0; // 匹配失败
	}
	else {
		// 完全未找到匹配
		return 9; // 新增状态码：无匹配
	}
}
