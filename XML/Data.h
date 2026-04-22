#pragma once
#pragma once


#include "HeaderDefine.h"
#include "Common.h"


int DT_CreatDir(char* pDir);
int DT_WriteAnnoXml(std::string xmlPath, cv::Mat img, std::vector<cv::Rect>& rectList, std::vector<std::string>& classList);
int DT_LoadAnnoXml_TEST(std::string xmlPath, std::vector<cv::Rect>& rectList, std::vector<std::string>& classList);
int DT_WriteAnnoXml_TEST(std::string xmlPath, cv::Mat img, std::vector<cv::Rect>& rectList, std::vector<std::string>& classList);
void writeImageXml(std::string outPath, std::string itemName, cv::Mat img, int id, std::vector <std::vector<cv::Rect>> rectList, std::vector <std::vector<std::string>> classList);
void writeImageXml(std::string outPath, std::string itemName, cv::Mat img, int id, std::vector<cv::Rect> rectList, std::vector<std::string> classList);
void writeImageXml(std::string outPath, std::string itemName, cv::Mat img, int id, std::vector<FinsObjects> typeTargets);
void writeImageXml(std::string outPath, std::string itemName, cv::Mat img, int id, FinsObjects typeTargets);
void writeImageXml(std::string outPath, std::string itemName, cv::Mat img, int cameraId, int id, std::vector<FinsObject> typeTargets);
