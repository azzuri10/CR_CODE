#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

namespace PaddleOCR {

class Permute {
public:
  void Run(const cv::Mat *im, float *data);
};

class PermuteBatch {
public:
  void Run(const std::vector<cv::Mat> imgs, float *data);
};

class Normalize {
public:
  void Run(cv::Mat *im, const std::vector<float> &mean,
           const std::vector<float> &scale, bool is_scale);
};

class ResizeImgType0 {
public:
  void Run(const cv::Mat &img, cv::Mat &resize_img, int max_size_len,
           float &ratio_h, float &ratio_w, bool use_tensorrt);
};

class CrnnResizeImg {
public:
  void Run(const cv::Mat &img, cv::Mat &resize_img, float wh_ratio,
           bool use_tensorrt, const std::vector<int> &rec_image_shape = {3, 32, 320});
};

class ClsResizeImg {
public:
  void Run(const cv::Mat &img, cv::Mat &resize_img, bool use_tensorrt,
           const std::vector<int> &rec_image_shape = {3, 48, 192});
};

} // namespace PaddleOCR

