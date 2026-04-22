#pragma once

#include "preprocess_op.h"
#include "utility.h"

#include <opencv2/opencv.hpp>

#include <cmath>
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

class CRNNRecognizer {
public:
  CRNNRecognizer() = default;
  void Run(std::vector<cv::Mat> img_list, std::vector<double> *times);
  void LoadModel(const std::string &model_dir);

  int rec_batch_num_ = 6;
  bool use_gpu_ = true;
  bool use_tensorrt_ = true;
  bool use_mkldnn_ = false;
  int gpu_mem_ = 2048;
  int gpu_id_ = 0;
  int cpu_math_library_num_threads_ = 8;
  std::string precision_ = "fp16";
  std::vector<float> mean_ = {0.5f, 0.5f, 0.5f};
  std::vector<float> scale_ = {1.0f / 0.5f, 1.0f / 0.5f, 1.0f / 0.5f};
  bool is_scale_ = true;
  std::vector<std::string> label_list_;

private:
#if PROJECT_HAS_PADDLE_INFER
  std::shared_ptr<paddle_infer::Predictor> predictor_;
#endif
  CrnnResizeImg resize_op_;
  Normalize normalize_op_;
  PermuteBatch permute_op_;
};

} // namespace PaddleOCR

