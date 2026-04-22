#pragma once

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>

namespace PaddleOCR {

class Utility {
public:
  static std::vector<std::string> ReadDict(const std::string &path);
  static void VisualizeBboxes(
      const cv::Mat &srcimg,
      const std::vector<std::vector<std::vector<int>>> &boxes);
  static void GetAllFiles(const char *dir_name,
                          std::vector<std::string> &all_inputs);
  static cv::Mat GetRotateCropImage(const cv::Mat &srcimage,
                                    std::vector<std::vector<int>> box);
  static std::vector<int> argsort(const std::vector<float>& array);

  template <typename T>
  static int argmax(T begin, T end) {
    return static_cast<int>(std::distance(begin, std::max_element(begin, end)));
  }
};

} // namespace PaddleOCR

