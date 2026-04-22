//#include "HeaderDefine.h"
//#include "Data.h"
//#include "Common.h"
//#include "Log.h"
//
//std::string getFileName(const std::string& filePath) {
//	size_t pos = filePath.find_last_of("\\");
//	if (pos != std::string::npos) {
//		return filePath.substr(pos + 1);
//	}
//	return filePath;  // 如果找不到分隔符，直接返回原始路径
//}
//
//
//int main()
//{
//	Common* COM = new Common;
//	std::string IMG_PATH;
//	std::cout << "图像文件夹路径: ";
//	//cin >> IMG_PATH;
//	std::getline(std::cin, IMG_PATH);
//
//	std::vector<std::string> imgPath;
//	cv::glob(IMG_PATH, imgPath, false);
//
//	std::string IMG_NAME;
//	for (int SS = 0; SS < imgPath.size(); SS++)
//	{
//		std::cout << imgPath[SS] << std::endl;
//		size_t found = imgPath[SS].find(".jpg");
//		std::string xmlpath;
//		std::string imgPathRe;
//		if (found != std::string::npos) {
//			xmlpath = imgPath[SS].substr(0, imgPath[SS].find_last_of(".jpg") - 3) + ".xml";
//
//			IMG_NAME = imgPath[SS].substr(0, imgPath[SS].find_last_of(".jpg") - 3)  + ".jpg";
//		}
//		else {
//			continue;
//		}
//
//
//
//		cv::Mat m_img = cv::imread(imgPath[SS], 1);
//
//		std::vector<cv::Rect> rectList;
//		std::vector<std::string> classList;
//		DT_LoadAnnoXml_TEST(xmlpath, rectList, classList);
//
//
//		bool find0 = false;
//		bool find1 = false;
//		bool find2 = false;
//		bool find3 = false;
//		bool find4 = false;
//		std::vector<cv::Rect> rectListRe;
//		std::vector<std::string> classListRe;
//		std::string savePath;
//		
//		for (int i = 0; i < classList.size(); i++)
//		{
//
//
//			if (classList[i] == "num_0")
//			{
//				find0 = true;
//				savePath = "F://TRAIN_SAMPLE//缝包线//RE//0//";
//			}
//			else if (classList[i] == "num_3")
//			{
//				find3 = true;
//				savePath = "F://TRAIN_SAMPLE//缝包线//RE//3//";
//			}
//			else if (classList[i] == "num_2")
//			{
//				find2 = true;
//				savePath = "F://TRAIN_SAMPLE//缝包线//RE//2//";
//			}
//			else if (classList[i] == "num_4")
//			{
//				find4 = true;
//				savePath = "F://TRAIN_SAMPLE//缝包线//RE//4//";
//			}
//			else if (classList[i] == "num_1")
//			{
//				find1 = true;
//				savePath = "F://TRAIN_SAMPLE//缝包线//RE//1//";
//			}
//
//			COM->CreateDir(savePath);
//			rectListRe.push_back(rectList[i]);
//			classListRe.push_back(classList[i]);
//		}
//		if (classList.empty())
//		{
//			savePath = "F://TRAIN_SAMPLE//缝包线//RE//5//";
//			COM->CreateDir(savePath);
//		}
//
//		std::string imgFileName = getFileName(imgPath[SS]);
//		std::string saveXmlPath;
//		std::string saveImgPath;
//		if (found != std::string::npos) {
//			saveXmlPath = savePath + imgFileName.substr(0, imgFileName.find_last_of(".jpg") - 3) + ".xml";
//			saveImgPath = savePath + imgFileName;
//
//		}
//		else {
//			continue;
//		}
//
//		DT_WriteAnnoXml_TEST(saveXmlPath, m_img, rectListRe, classListRe);
//		cv::imwrite(saveImgPath, m_img);
//	}
//	delete COM;
//}