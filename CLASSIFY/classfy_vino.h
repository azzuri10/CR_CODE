#pragma once
#include "HeaderDefine.h"

void initConfig(std::string IR_path, float score_threshold, int input_w, int input_h);
int classfy_resnet(cv::Mat& frame, std::vector<std::string>& classNames);