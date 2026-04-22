#pragma once

#include "postprocess_op.h"
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

class DBDetector {
public:
  DBDetector() = default;
  void LoadModel(const std::string &model_dir);
  void Run(cv::Mat &img, std::vector<std::vector<std::vector<int>>> &boxes,
           std::vector<double> *times);

  int max_side_len_ = 960;
  float det_db_thresh_ = 0.3f;
  float det_db_box_thresh_ = 0.6f;
  float det_db_unclip_ratio_ = 1.5f;
  bool use_polygon_score_ = false;
  bool visualize_ = false;
  bool use_gpu_ = true;
  bool use_tensorrt_ = true;
  bool use_mkldnn_ = false;
  int gpu_mem_ = 2048;
  int gpu_id_ = 0;
  int cpu_math_library_num_threads_ = 8;
  std::string precision_ = "fp16";
  std::vector<float> mean_ = {0.485f, 0.456f, 0.406f};
  std::vector<float> scale_ = {1.0f / 0.229f, 1.0f / 0.224f, 1.0f / 0.225f};
  bool is_scale_ = true;

private:
#if PROJECT_HAS_PADDLE_INFER
  std::shared_ptr<paddle_infer::Predictor> predictor_;
#endif
  ResizeImgType0 resize_op_;
  Normalize normalize_op_;
  Permute permute_op_;
  PostProcessor post_processor_;
};

} // namespace PaddleOCR

