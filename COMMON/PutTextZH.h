#pragma once
#ifndef PUTTEXT_H_
#define PUTTEXT_H_

#include "HeaderDefine.h"



void GetStringSize(HDC hDC, const char* str, int* w, int* h);
void putTextZH(const cv::Mat& dst, const char* str, cv::Point org, cv::Scalar color, int fontSize, int lfWeight,
	const char* fn = "Arial", bool italic = false, bool underline = false);

#endif // PUTTEXT_H_

