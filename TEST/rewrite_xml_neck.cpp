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
//#define NUM2NUM 1
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
//			IMG_NAME = imgPath[SS].substr(0, imgPath[SS].find_last_of(".jpg") - 3) + ".jpg";
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
//		bool findNC = false;
//		bool findNCT = false;
//		bool findCRIMP = false;
//		bool findSCRAP = false;
//		bool findBARERR = false;
//		bool findBARB = false;
//		bool findBARD = false;
//		bool findLEAKM = false;
//		bool findTOPB = false;
//		int findLRP = 0;
//		std::vector<cv::Rect> rectListRe;
//		std::vector<std::string> classListRe;
//		std::string savePath;
//		for (int i = 0; i < classList.size(); i++)
//		{
//			/*if (classList[i] == "word_SCRAP")
//			{
//				continue;
//			}*/
//			rectListRe.push_back(rectList[i]);
//			classListRe.push_back(classList[i]);
//		}
//
//		for (int i = 0; i < classListRe.size(); i++)
//		{
//
//
//			if (classListRe[i] == "word_IN2")
//			{
//				findBARB = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_IN2//";
//				break;
//			}
//			else if (classListRe[i] == "word_TOP3")
//			{
//				findBARD = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_TOP3//";
//				break;
//			}
//			else if (classListRe[i] == "word_TOP2")
//			{
//				findTOPB = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_TOP2//";
//			}
//			else if (classListRe[i] == "word_TOP1")
//			{
//				findNC = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_TOP1//";
//				break;
//			}
//			else if (classListRe[i] == "word_TOP0")
//			{
//				findNCT = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_TOP0//";
//				break;
//			}
//			else if (classListRe[i] == "word_OUT1")
//			{
//				findCRIMP = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_OUT1//";
//				break;
//			}
//			else if (classListRe[i] == "word_OUT0")
//			{
//				findSCRAP = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_OUT0//";
//				break;
//			}
//			else if (classListRe[i] == "word_OTHER")
//			{
//				findBARERR = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_OTHER//";
//				break;
//			}
//			else if (classListRe[i] == "word_IN1")
//			{
//				findLEAKM = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_IN1//";
//			}
//			else if (classListRe[i] == "word_IN0")
//			{
//				findLEAKM = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_IN0//";
//			}
//			else if (classListRe[i] == "word_BURR0")
//			{
//				findLEAKM = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_BURR0//";
//			}
//			else if (classListRe[i] == "word_BURR1")
//			{
//				findLEAKM = true;
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//word_BURR1//";
//			}
//			else 
//			{
//				savePath = "F://TRAIN_SAMPLE//瓶口//昆明//new//1//ok//";
//			}
//		}
//		COM->CreateDir(savePath);
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