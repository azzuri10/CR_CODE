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
//			if (classListRe[i] == "word_BARB")
//			{
//			findBARB = true;
//			savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//BARB//";
//			break;
//			}
//			else if (classListRe[i] == "word_BARD")
//			{
//			findBARD = true;
//			savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//BARD//";
//			break;
//			}
//			else if (classListRe[i] == "word_TOPB")
//			{
//				findTOPB = true;
//				savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//word_TOPB//";
//			}
//			else if (classListRe[i] == "word_NC")
//			{
//				findNC = true;
//				savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//word_NC//";
//				break;
//			}
//			else if (classListRe[i] == "word_NCT")
//			{
//				findNCT = true;
//				savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//word_NCT//";
//				break;
//			}
//			else if (classListRe[i] == "word_CRIMP")
//			{
//				findCRIMP = true;
//				savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//word_CRIMP//";
//				break;
//			}
//			else if (classListRe[i] == "word_SCRAP")
//			{
//				findSCRAP = true;
//				savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//word_SCRAP//";
//				break;
//			}
//			else if (classListRe[i] == "word_BARERR")
//			{
//				findBARERR = true;
//				savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//BARERR//";
//				break;
//			}
//			else if (classListRe[i] == "word_LEAKM")
//			{
//				findLEAKM = true;
//				savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//word_LEAKM//";
//			}
//			else if (classListRe[i] == "word_LRP")
//			{
//				findLRP++;
//			}
//		}
//		if (findNC ||
//			findNCT ||
//			findCRIMP ||
//			findSCRAP ||
//			findBARERR ||
//			findBARB ||
//			findBARD ||
//			findLEAKM)
//		{
//			COM->CreateDir(savePath);
//		}
//		else if (findLRP < 2)
//		{
//			savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//out//";
//			COM->CreateDir(savePath);
//		}
//		else if (findLRP == 2)
//		{
//			savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//ok//";
//			COM->CreateDir(savePath);
//		}
//		else if (findLRP > 2)
//		{
//			savePath = "F://TRAIN_SAMPLE//CAP_NEW//defect//KMYH20//new//1//mult//";
//			COM->CreateDir(savePath);
//		}
//		std::string imgFileName = getFileName(imgPath[SS]);
//		std::string saveXmlPath;
//		std::string saveImgPath;
//		if (found != std::string::npos) {
//			saveXmlPath = savePath + imgFileName.substr(0,imgFileName.find_last_of(".jpg") - 3) + ".xml";
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