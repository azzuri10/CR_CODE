#pragma once

#include "preprocess_op.h"

#include <opencv2/opencv.hpp>

#include <memory>
#include <numeric>
#include <string>
#include <vector>

#if defined(__has_include)
#if __has_include(<paddle_inference_api.h>)
#include <paddle_inference_api.h>
#define PROJECT_HAS_PADDLE_INFER 1
#else
#define PROJECT_HAS_PADDLE_INFER 0
#endif
#else
#define PROJECT_HAS_PADDLE_INFER 0
#endif

namespace PaddleOCR {

class Classifier {
public:
  Classifier() = default;
  cv::Mat Run(cv::Mat &img);
  void LoadModel(const std::string &model_dir);

  bool use_gpu_ = true;
  bool use_tensorrt_ = true;
  bool use_mkldnn_ = false;
  int gpu_mem_ = 2048;
  int gpu_id_ = 0;
  int cpu_math_library_num_threads_ = 8;
  std::string precision_ = "fp16";
  float cls_thresh = 0.9f;
  std::vector<float> mean_ = {0.5f, 0.5f, 0.5f};
  std::vector<float> scale_ = {1.0f / 0.5f, 1.0f / 0.5f, 1.0f / 0.5f};
  bool is_scale_ = true;

private:
#if PROJECT_HAS_PADDLE_INFER
  std::shared_ptr<paddle_infer::Predictor> predictor_;
#endif
  ClsResizeImg resize_op_;
  Normalize normalize_op_;
  Permute permute_op_;
};

} // namespace PaddleOCR

