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
//
//		std::vector<cv::Rect> rectList;
//		std::vector<std::string> classList;
//		DT_LoadAnnoXml_TEST(xmlpath, rectList, classList);
//
//
//		std::vector<cv::Rect> rectListRe;
//		std::vector<std::string> classListRe;
//		if (NUM2NUM == 1)
//		{
//			/*if (classList.size() != 30)
//			{
//				std::cout << xmlpath << "!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
//				cv::waitKey(10000);
//			}
//			else if (classList[1] != "charB_Z")
//				{
//					std::cout << xmlpath << "!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
//					cv::waitKey(10000);
//				}*/
//			/*classList[0] = "charB_J";
//			classList[1] = "charB_Z";*/
//			classList[24] = "charB_G";
//		//	classList[3] = "num_5";
//		//	classList[4] = "num_0";
//		//	classList[5] = "num_4";
//		//	classList[6] = "num_1";
//		//	classList[7] = "num_4";
//		//	classList[8] = "charB_A";
//		//	classList[9] = "num_7";
//		///*	classList[9] = "num_7";
//		//	classList[9] = "num_7";
//		//	classList[9] = "num_7";
//		//	classList[9] = "num_7";
//		//	classList[9] = "num_7";
//		//	classList[9] = "num_7";*/
//		//	classList[16] = "word_he";
//		//	classList[17] = "word_ge";
//		//	classList[18] = "charB_C";
//		//	classList[19] = "charB_H";
//		//	classList[20] = "charB_Q";
//			/*classList[21] = "charB_P";
//			classList[22] = "charB_B";
//			classList[23] = "num_4";*/
//			for (int i = 0; i < classList.size(); i++) 
//			{
//				/*rectList[i].x += -50;
//				rectList[i].y += -30;
//				rectList[i].width += 100;
//				rectList[i].height += 40;*/
//				//if (classList[i] != "word_LRP" )
//				//{
//				//	continue;
//				//	//classList[i] = "word_BARB";
//				//}
//				
//				/*if (classList[i] == "word_BARD")
//				{
//					continue;
//				}*/
//				/*if (classList[i] == "word_IN1")
//				{
//					classList[i] = "word_IN0";
//				}*/
//				/*if (classList[i] == "word_HandleRed" ||
//					classList[i] == "word_HandleGolden" ||
//					classList[i] == "word_HandleYellow" || 
//					classList[i] == "word_HandlePurple" || 
//					classList[i] == "word_HandleGreen" || 
//					classList[i] == "word_HandleOrange" || 
//					classList[i] == "word_HandleWhite" ||
//					classList[i] == "word_HanddleRed")
//				{
//					classList[i] = "word_HandleWhite";
//				}*/
//
//				/*if (classList[i] == "word_CapRed" ||
//					classList[i] == "word_CapGolden" ||
//					classList[i] == "word_CapYellow" ||
//					classList[i] == "word_CapPurple" ||
//					classList[i] == "word_CapGreen" ||
//					classList[i] == "word_CapOrange")
//				{
//					classList[i] = "word_Cap";
//				}*/
//				/*if (classList[i] == "word_HandleNone" )
//				{
//					continue;
//				}*
//				
//				/*if (classList[i] == "word_IN2")
//				{
//					classList[i] = "word_IN1";
//				}
//				if (classList[i] == "word_TOP3")
//				{
//					classList[i] = "word_TOP2";
//				}*/
//				//if (classList[i] == "word_HandleGolden")
//				//{
//				//	//continue;
//				//	classList[i] = "word_HandleWhite";
//				//}
//				//else if(classList[i] == "word_HandleWhite"|| classList[i] == "word_HandleGolden" || classList[i] == "word_HandleYellow")
//				//{
//				//	//continue;
//				//	classList[i] = "word_HandlePurple";
//				//}
//				rectListRe.push_back(rectList[i]);
//				classListRe.push_back(classList[i]);
//
//			}
//		}
//
//
//		DT_WriteAnnoXml_TEST(xmlpath, m_img, rectListRe, classListRe);
//	}
//
//}