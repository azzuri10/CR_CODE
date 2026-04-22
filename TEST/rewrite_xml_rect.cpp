//#include "HeaderDefine.h"
//#include "Data.h"
//
//std::string getFileName(const std::string& filePath) {
//	size_t pos = filePath.find_last_of("/\\");
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
//		cv::Rect roi0(300, 370, 1388, 706);
//		cv::Rect roi1(500, 300, 1248, 706);
//		cv::Rect roi2(350, 330, 1230, 700);
//
//
//		cv::Mat m_imgRoi = m_img(roi2).clone();//*********************
//
//		std::vector<cv::Rect> rectList;
//		std::vector<std::string> classList;
//		DT_LoadAnnoXml_TEST(xmlpath, rectList, classList);
//
//
//		std::vector<cv::Rect> rectListRe;
//		std::vector<std::string> classListRe;
//
//		for (int i = 0; i < classList.size(); i++)
//		{
//			rectList[i].x -= roi2.x;//*********************
//			rectList[i].y -= roi2.y;//*********************
//
//			//if (classList[i] != "word_LRP")
//			//{
//			//	continue;
//			//	//classList[i] = "word_BARB";
//			//}
//			rectListRe.push_back(rectList[i]);
//			classListRe.push_back(classList[i]);
//
//		}
//
//
//		DT_WriteAnnoXml_TEST(xmlpath, m_imgRoi, rectListRe, classListRe);
//		cv::imwrite(IMG_NAME, m_imgRoi);
//	}
//
//}