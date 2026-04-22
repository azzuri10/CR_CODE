#pragma once

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace PaddleOCR {

class PostProcessor {
public:
  void GetContourArea(const std::vector<std::vector<float>> &box,
                      float unclip_ratio, float &distance);
  cv::RotatedRect UnClip(std::vector<std::vector<float>> box,
                         const float &unclip_ratio);
  float **Mat2Vec(cv::Mat mat);
  std::vector<std::vector<int>>
  OrderPointsClockwise(std::vector<std::vector<int>> pts);
  std::vector<std::vector<float>> Mat2Vector(cv::Mat mat);
  static bool XsortFp32(std::vector<float> a, std::vector<float> b);
  static bool XsortInt(std::vector<int> a, std::vector<int> b);
  std::vector<std::vector<float>> GetMiniBoxes(cv::RotatedRect box, float &ssid);
  float PolygonScoreAcc(std::vector<cv::Point> contour, cv::Mat pred);
  float BoxScoreFast(std::vector<std::vector<float>> box_array, cv::Mat pred);
  std::vector<std::vector<std::vector<int>>> BoxesFromBitmap(
      cv::Mat pred, cv::Mat bitmap, const float &box_thresh,
      const float &det_db_unclip_ratio, const bool &use_polygon_score);
  std::vector<std::vector<std::vector<int>>>
  FilterTagDetRes(std::vector<std::vector<std::vector<int>>> boxes, float ratio_h,
                  float ratio_w, cv::Mat srcimg);
};

template <class T>
inline T clamp(T value, T low, T high) {
  return std::max(low, std::min(value, high));
}

inline float clampf(float value, float low, float high) {
  return std::max(low, std::min(value, high));
}

} // namespace PaddleOCR

