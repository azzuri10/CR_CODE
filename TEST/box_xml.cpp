//#include "HeaderDefine.h"
//#include "Data.h"
//#include <random>
//
//string getFileName(const string& filePath) {
//	size_t pos = filePath.find_last_of("/\\");
//	if (pos != string::npos) {
//		return filePath.substr(pos + 1);
//	}
//	return filePath;  // 如果找不到分隔符，直接返回原始路径
//}
//
//// 调整矩形区域，确保不超出图像边界
//void adjustRectToImageBoundary(Rect& rect, const Mat& image) {
//	int imgWidth = image.cols;
//	int imgHeight = image.rows;
//
//	// 确保矩形的左上角坐标不超出图像边界
//	rect.x = std::max(0, std::min(rect.x, imgWidth - 1));
//	rect.y = std::max(0, std::min(rect.y, imgHeight - 1));
//
//	// 确保矩形的宽度和高度不超出图像边界
//	rect.width = std::min(rect.width, imgWidth - rect.x);
//	rect.height = std::min(rect.height, imgHeight - rect.y);
//}
//
//
//#define NUM2NUM 1
//
//int main()
//{ 
//	string IMG_PATH;
//	cout << "图像文件夹路径: ";
//	//cin >> IMG_PATH;
//	getline(cin, IMG_PATH);
//
//	vector<string> imgPath;
//	glob(IMG_PATH, imgPath, false);
//
//	string IMG_NAME;
//	for (int SS = 0; SS < imgPath.size(); SS++)
//	{
//		cout << imgPath[SS] << endl;
//		size_t found = imgPath[SS].find(".jpg");
//		string xmlpath;
//		string imgPathRe;
//		string xmlpathRe;
//		if (found != string::npos) {
//			xmlpath = imgPath[SS].substr(0, imgPath[SS].find_last_of(".jpg") - 3) + ".xml";
//
//			IMG_NAME = imgPath[SS].substr(0, imgPath[SS].find_last_of(".jpg") - 3) + "_RE" + ".jpg";
//			imgPathRe = imgPath[SS].substr(0, imgPath[SS].find_last_of(".jpg") - 3)  + ".jpg";
//			xmlpathRe = imgPath[SS].substr(0, imgPath[SS].find_last_of(".jpg") - 3)  + ".xml";
//		}
//		else {
//			continue;
//		}
//		 
//
//		bool find2 = false;
//		Mat m_img = imread(imgPath[SS], 1);
//
//		
//		vector<Rect> rectList;
//		vector<string> classList;
//		DT_LoadAnnoXml_TEST(xmlpath, rectList, classList);
//
//		// 调整矩形区域，确保不超出图像边界
//		for (Rect& rect : rectList) {
//			adjustRectToImageBoundary(rect, m_img);
//		}
//
//		//2num
//		vector<Rect> rectListRe;
//		vector<string> classListRe;
//
//		vector<Rect> rectListUD;
//		vector<string> classListUD;
//
//
//		vector<Rect> rectList180;
//		vector<string> classList180;
//		if (NUM2NUM == 1)
//		{
//			for (int i = 0; i < classList.size(); i++)
//			{
//				Rect new_tmp(m_img.cols - rectList[i].x - rectList[i].width, rectList[i].y, rectList[i].width, rectList[i].height);
//				Rect UD_tmp(rectList[i].x, m_img.rows - rectList[i].y - rectList[i].height, rectList[i].width, rectList[i].height);
//				Rect tmp180(m_img.cols - rectList[i].x - rectList[i].width, m_img.rows - rectList[i].y - rectList[i].height, rectList[i].width, rectList[i].height);
//
//
//				rectListRe.push_back(new_tmp);
//				classListRe.push_back(classList[i]);
//
//				rectListUD.push_back(UD_tmp);
//				classListUD.push_back(classList[i]);
//
//				rectList180.push_back(tmp180);
//				classList180.push_back(classList[i]);
//
//				/*if (classList[i] == "word_HandleGreen" ||
//					classList[i] == "word_HandleGolden" ||
//					classList[i] == "word_HandlePurple" ||
//					classList[i] == "word_HandleYellow" ||
//					classList[i] == "word_HandleRed" ||
//					classList[i] == "word_HandleWhite"
//					)
//				{
//					classList[i] = "word_HandleWhite";
//				}
//
//				if (classList[i] == "word_CapGreen" ||
//					classList[i] == "word_CapGolden" ||
//					classList[i] == "word_CapPurple" ||
//					classList[i] == "word_CapYellow" ||
//					classList[i] == "word_CapRed"
//					)
//				{
//					classList[i] = "word_CapYellow";
//				}*/
//
//			}
//			
//
//			DT_WriteAnnoXml_TEST(xmlpathRe, m_img, rectList, classList);
//			imwrite(imgPathRe, m_img);
//
//
//
//			string path = "E://1//";
//			string imgFileName = getFileName(imgPath[SS]);
//			string saveXmlPath;
//			string saveImgPath;
//			string saveXmlPathUD;
//			string saveImgPathUD;
//			string saveXmlPath180;
//			string saveImgPath180;
//			if (found != string::npos) {
//				saveXmlPath = path + imgFileName.substr(0, imgFileName.find_last_of(".jpg") - 3) + "_LR.xml";
//				saveImgPath = path + imgFileName.substr(0, imgFileName.find_last_of(".jpg") - 3) + "_LR.jpg";
//				saveXmlPathUD = path + imgFileName.substr(0, imgFileName.find_last_of(".jpg") - 3) + "_UD.xml";
//				saveImgPathUD = path + imgFileName.substr(0, imgFileName.find_last_of(".jpg") - 3) + "_UD.jpg";
//				saveXmlPath180 = path + imgFileName.substr(0, imgFileName.find_last_of(".jpg") - 3) + "_180.xml";
//				saveImgPath180 = path + imgFileName.substr(0, imgFileName.find_last_of(".jpg") - 3) + "_180.jpg";
//
//			}
//			else {
//				continue;
//			}
//
//			Mat flipped_image;
//			flip(m_img, flipped_image, 1); // 参数 1 表示左右翻转
//			DT_WriteAnnoXml_TEST(saveXmlPath, flipped_image, rectListRe, classListRe);
//			imwrite(saveImgPath, flipped_image);
//
//			Mat flipped_imageUD;
//			flip(m_img, flipped_imageUD, 0); // 参数 0 表示垂直翻转
//			DT_WriteAnnoXml_TEST(saveXmlPathUD, flipped_imageUD, rectListUD, classListUD);
//			imwrite(saveImgPathUD, flipped_imageUD);
//
//			Mat flipped_image180;
//			flip(m_img, flipped_image180, -1); // 参数 0 表示垂直翻转
//			DT_WriteAnnoXml_TEST(saveXmlPath180, flipped_image180, rectList180, classList180);
//			imwrite(saveImgPath180, flipped_image180);
//		}
//		
//		
//	}
//
//}