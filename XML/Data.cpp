//#include "stdafx.h"
#include "Data.h"
#include "tinyxml.h"
#include <stdio.h>
#include <windows.h>
#include <winbase.h>
#include "math.h" 
#include <iostream>  
#include <iomanip>  
#include <fstream> 
#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>  

#define ACCESS _access  
#define MKDIR(a) _mkdir((a))  

int DT_CreatDir(char* pDir)
{
	int i = 0;
	int iRet;
	int iLen;
	char* pszDir;

	if (NULL == pDir)
	{
		return 0;
	}

	pszDir = strdup(pDir);
	iLen = strlen(pszDir);

	// 创建中间目录  
	for (i = 0; i < iLen; i++)
	{
		if (pszDir[i] == '\\' || pszDir[i] == '/')
		{
			pszDir[i] = '\0';

			//如果不存在,创建  
			iRet = ACCESS(pszDir, 0);
			if (iRet != 0)
			{
				iRet = MKDIR(pszDir);
				if (iRet != 0)
				{
					return -1;
				}
			}
			//支持linux,将所有\换成/  
			pszDir[i] = '/';
		}
	}

	iRet = MKDIR(pszDir);
	free(pszDir);
	return iRet;
}



std::string Int2String(int number)
{
	std::stringstream str;
	str << number;
	return str.str();
}
std::string Float2String(float number)
{
	std::stringstream str;
	str << number;
	return str.str();
}


int DT_WriteAnnoXml(std::string xmlPath, cv::Mat img, std::vector<cv::Rect>& rectList, std::vector<std::string>& classList)
{
	TiXmlDocument* writeDoc = new TiXmlDocument; //xml文档指针

	//文档格式声明
	TiXmlDeclaration* decl = new TiXmlDeclaration("", "", "annotation");
	//writeDoc->LinkEndChild(decl); //写入文档

	TiXmlElement* RootElement = new TiXmlElement("annotation");//根元素
	writeDoc->LinkEndChild(RootElement);

	//写folder 名称
	TiXmlElement* Element_folder = new TiXmlElement("folder");
	RootElement->LinkEndChild(Element_folder);
	TiXmlText* val_folder = new TiXmlText("testFolder");
	Element_folder->LinkEndChild(val_folder);

	/***********写image size 名称**********************************************************/
	TiXmlElement* Element_size = new TiXmlElement("size");
	RootElement->LinkEndChild(Element_size);

	TiXmlElement* Element_size_width = new TiXmlElement("width");
	Element_size->LinkEndChild(Element_size_width);
	TiXmlText* val_size_width = new TiXmlText(Int2String(img.cols).c_str());
	Element_size_width->LinkEndChild(val_size_width);

	TiXmlElement* Element_size_height = new TiXmlElement("height");
	Element_size->LinkEndChild(Element_size_height);
	TiXmlText* val_size_height = new TiXmlText(Int2String(img.rows).c_str());
	Element_size_height->LinkEndChild(val_size_height);

	TiXmlElement* Element_size_depth = new TiXmlElement("depth");
	Element_size->LinkEndChild(Element_size_depth);
	TiXmlText* val_size_depth = new TiXmlText(Int2String(img.channels()).c_str());
	Element_size_depth->LinkEndChild(val_size_depth);

	/***********写obj 名称**********************************************************/

	for (int i = 0; i < rectList.size(); i++)
	{
		TiXmlElement* Element_object = new TiXmlElement("object");
		RootElement->LinkEndChild(Element_object);

		//name
		TiXmlElement* Element_obj_name = new TiXmlElement("name");
		Element_object->LinkEndChild(Element_obj_name);
#ifdef USE_CLASS_MULTI
		TiXmlText* val_obj_name = new TiXmlText(classList[i].c_str());
#else
		//TiXmlText *val_obj_name = new TiXmlText("word");
		std::string keyWord = classList[i];

		TiXmlText* val_obj_name = new TiXmlText(keyWord.c_str());
#endif
		Element_obj_name->LinkEndChild(val_obj_name);

		//name
		TiXmlElement* Element_obj_box = new TiXmlElement("bndbox");
		Element_object->LinkEndChild(Element_obj_box);

		TiXmlElement* Element_obj_box_xm = new TiXmlElement("xmin");
		Element_obj_box->LinkEndChild(Element_obj_box_xm);
		TiXmlText* val_xm = new TiXmlText(Int2String(rectList[i].x).c_str());
		Element_obj_box_xm->LinkEndChild(val_xm);
		TiXmlElement* Element_obj_box_ym = new TiXmlElement("ymin");
		Element_obj_box->LinkEndChild(Element_obj_box_ym);
		TiXmlText* val_ym = new TiXmlText(Int2String(rectList[i].y).c_str());
		Element_obj_box_ym->LinkEndChild(val_ym);

		TiXmlElement* Element_obj_box_xmx = new TiXmlElement("xmax");
		Element_obj_box->LinkEndChild(Element_obj_box_xmx);
		TiXmlText* val_xmx = new TiXmlText(Int2String(rectList[i].x + rectList[i].width).c_str());
		Element_obj_box_xmx->LinkEndChild(val_xmx);

		TiXmlElement* Element_obj_box_ymx = new TiXmlElement("ymax");
		Element_obj_box->LinkEndChild(Element_obj_box_ymx);
		TiXmlText* val_ymx = new TiXmlText(Int2String(rectList[i].y + rectList[i].height).c_str());
		Element_obj_box_ymx->LinkEndChild(val_ymx);
}


	writeDoc->SaveFile(xmlPath.c_str());
	delete writeDoc;

	return 1;
}

int DT_LoadAnnoXml_TEST(std::string xmlPath, std::vector<cv::Rect>& rectList, std::vector<std::string>& classList)
{


	try
	{
		TiXmlDocument mydoc(xmlPath.c_str());//xml文档对象
		bool loadOk = mydoc.LoadFile();//加载文档
		if (!loadOk)
		{
			//cout << "could not load the test file.Error:" << mydoc.ErrorDesc() << endl;
			return 0;
			//exit(1);
		}
		TiXmlElement* RootElement = mydoc.RootElement(); //根元素, SerializableDictionary
		//cout << "[root name]" << RootElement->Value() << "\n";

		//TiXmlElement * ItemElement = RootElement->FirstChildElement();//Item
		//cout << "[root name]" << ItemElement->Value() << "\n";

		TiXmlElement* pEle = RootElement;
		//遍历该结点
		for (TiXmlElement* StuElement = pEle->FirstChildElement();//第一个子元素
			StuElement != NULL;
			StuElement = StuElement->NextSiblingElement())//下一个兄弟元素
		{
			//cout << "[sub name]" << StuElement->Value() << "\n";
			std::string cur_str = StuElement->Value();
			std::string dst_str = "object";
			if (cur_str == dst_str)
			{

				for (TiXmlElement* objEle = StuElement->FirstChildElement();//第一个子元素
					objEle != NULL;
					objEle = objEle->NextSiblingElement())//下一个兄弟元素
				{

					//cout << "[objEle name]" << objEle->Value() << "\n";
					if (strcmp(objEle->Value(), "name") == 0)
					{
						const char* itemValue = objEle->GetText();
						std::string  name_str = itemValue;

					



						classList.push_back(name_str);
					}
					else if (strcmp(objEle->Value(), "bndbox") == 0)
					{
						int min_x = 0;
						int min_y = 0;
						int max_x = 0;
						int max_y = 0;

						std::vector<int> pos;
						for (TiXmlElement* boxEle = objEle->FirstChildElement();//第一个子元素
							boxEle != NULL;
							boxEle = boxEle->NextSiblingElement())//下一个兄弟元素
						{
							const char* itemValue = boxEle->GetText();
							int val = atoi(itemValue);

							std::string _cur_str = boxEle->Value();

							if (_cur_str == "xmax")
							{
								max_x = val;
							}
							else if (_cur_str == "xmin")
							{
								min_x = val;
							}
							else if (_cur_str == "ymax")
							{
								max_y = val;
							}
							else if (_cur_str == "ymin")
							{
								min_y = val;
							}
						}

						cv::Rect rectCur(min_x, min_y, max_x - min_x, max_y - min_y);
						rectList.push_back(rectCur);
					}
				}
			}

			int bbb = 0;
		}


	}
	catch (...)
	{

	}

	return 1;

}

int DT_WriteAnnoXml_TEST(std::string xmlPath, cv::Mat img, std::vector<cv::Rect>& rectList, std::vector<std::string>& classList)
{

	TiXmlDocument* writeDoc = new TiXmlDocument; //xml文档指针

	//文档格式声明
	TiXmlDeclaration* decl = new TiXmlDeclaration("", "", "annotation");
	//writeDoc->LinkEndChild(decl); //写入文档

	TiXmlElement* RootElement = new TiXmlElement("annotation");//根元素
	writeDoc->LinkEndChild(RootElement);

	//写folder 名称
	TiXmlElement* Element_folder = new TiXmlElement("folder");
	RootElement->LinkEndChild(Element_folder);
	TiXmlText* val_folder = new TiXmlText("testFolder");
	Element_folder->LinkEndChild(val_folder);

	/***********写image size 名称**********************************************************/
	TiXmlElement* Element_size = new TiXmlElement("size");
	RootElement->LinkEndChild(Element_size);

	TiXmlElement* Element_size_width = new TiXmlElement("width");
	Element_size->LinkEndChild(Element_size_width);
	TiXmlText* val_size_width = new TiXmlText(Int2String(img.cols).c_str());
	Element_size_width->LinkEndChild(val_size_width);

	TiXmlElement* Element_size_height = new TiXmlElement("height");
	Element_size->LinkEndChild(Element_size_height);
	TiXmlText* val_size_height = new TiXmlText(Int2String(img.rows).c_str());
	Element_size_height->LinkEndChild(val_size_height);

	TiXmlElement* Element_size_depth = new TiXmlElement("depth");
	Element_size->LinkEndChild(Element_size_depth);
	TiXmlText* val_size_depth = new TiXmlText(Int2String(img.channels()).c_str());
	Element_size_depth->LinkEndChild(val_size_depth);

	/***********写obj 名称**********************************************************/

	for (int i = 0; i < rectList.size(); i++)
	{
		rectList[i].x = MAX(0, rectList[i].x);
		rectList[i].y = MAX(0, rectList[i].y);
		rectList[i].width = MIN(rectList[i].width, img.cols - 1 - rectList[i].x);
		rectList[i].height = MIN(rectList[i].height, img.rows - 1 - rectList[i].y);

		TiXmlElement* Element_object = new TiXmlElement("object");
		RootElement->LinkEndChild(Element_object);

		//name
		TiXmlElement* Element_obj_name = new TiXmlElement("name");
		Element_object->LinkEndChild(Element_obj_name);
#ifdef USE_CLASS_MULTI
		TiXmlText* val_obj_name = new TiXmlText(classList[i].c_str());
#else
		//TiXmlText *val_obj_name = new TiXmlText("word");
		std::string keyWord = classList[i];
		



		TiXmlText* val_obj_name = new TiXmlText(keyWord.c_str());
#endif
		Element_obj_name->LinkEndChild(val_obj_name);

		//name
		TiXmlElement* Element_obj_box = new TiXmlElement("bndbox");
		Element_object->LinkEndChild(Element_obj_box);

		TiXmlElement* Element_obj_box_xm = new TiXmlElement("xmin");
		Element_obj_box->LinkEndChild(Element_obj_box_xm);
		TiXmlText* val_xm = new TiXmlText(Int2String(rectList[i].x).c_str());
		Element_obj_box_xm->LinkEndChild(val_xm);
		TiXmlElement* Element_obj_box_ym = new TiXmlElement("ymin");
		Element_obj_box->LinkEndChild(Element_obj_box_ym);
		TiXmlText* val_ym = new TiXmlText(Int2String(rectList[i].y).c_str());
		Element_obj_box_ym->LinkEndChild(val_ym);

		TiXmlElement* Element_obj_box_xmx = new TiXmlElement("xmax");
		Element_obj_box->LinkEndChild(Element_obj_box_xmx);
		TiXmlText* val_xmx = new TiXmlText(Int2String(rectList[i].x + rectList[i].width).c_str());
		Element_obj_box_xmx->LinkEndChild(val_xmx);

		TiXmlElement* Element_obj_box_ymx = new TiXmlElement("ymax");
		Element_obj_box->LinkEndChild(Element_obj_box_ymx);
		TiXmlText* val_ymx = new TiXmlText(Int2String(rectList[i].y + rectList[i].height).c_str());
		Element_obj_box_ymx->LinkEndChild(val_ymx);
	}


	writeDoc->SaveFile(xmlPath.c_str());
	delete writeDoc;

	return 1;
}

//写XML文件
void writeImageXml(std::string outPath, std::string itemName, cv::Mat img, int id, std::vector <std::vector<cv::Rect>> rectList, std::vector <std::vector<std::string>> classList)
{
	char buf_id[100];
	sprintf_s(buf_id, "_%d", id);
	std::string  xml_path = outPath + "\\" + itemName + buf_id + ".xml";
	std::string  img_path = outPath + "\\" + itemName + buf_id + ".jpg";


	imwrite(img_path.c_str(), img);

	std::vector<cv::Rect> rectListTmp;
	std::vector<std::string> classListTmp;
	for (int i = 0; i < rectList.size(); i++)
	{
		for (int j = 0; j < rectList[i].size(); j++)
		{
			rectListTmp.push_back(rectList[i][j]);
			classListTmp.push_back(classList[i][j]);
		}
	}

	DT_WriteAnnoXml(xml_path, img, rectListTmp, classListTmp);
}

void writeImageXml(std::string outPath, std::string itemName, cv::Mat img, int id, std::vector<cv::Rect> rectList, std::vector<std::string> classList)
{
	char buf_id[100];
	sprintf_s(buf_id, "_%d", id);
	std::string  xml_path = outPath + "\\" + itemName + buf_id + ".xml";
	std::string  img_path = outPath + "\\" + itemName + buf_id + ".jpg";


	imwrite(img_path.c_str(), img);

	std::vector<cv::Rect> rectListTmp;
	std::vector<std::string> classListTmp;
	for (int i = 0; i < rectList.size(); i++)
	{
		rectListTmp.push_back(rectList[i]);
		classListTmp.push_back(classList[i]);
	}

	DT_WriteAnnoXml(xml_path, img, rectListTmp, classListTmp);
}

void writeImageXml(std::string outPath, std::string itemName, cv::Mat img, int id, std::vector<FinsObjects> typeTargets)
{
	char buf_id[100];
	sprintf_s(buf_id, "_%d", id);
	std::string  xml_path = outPath + "\\" + itemName + buf_id + ".xml";
	std::string  img_path = outPath + "\\" + itemName + buf_id + ".jpg";


	imwrite(img_path.c_str(), img);

	std::vector<cv::Rect> rectListTmp;
	std::vector<std::string> classListTmp;
	for (int i = 0; i < typeTargets.size(); i++)
	{
		for (int j = 0; j < typeTargets[i].boxes.size(); j++)
		{
			rectListTmp.push_back(typeTargets[i].boxes[j]);
			classListTmp.push_back(typeTargets[i].classNames[j]);
		}
	}

	DT_WriteAnnoXml(xml_path, img, rectListTmp, classListTmp);
}

void writeImageXml(std::string outPath, std::string itemName, cv::Mat img, int id, FinsObjects typeTargets)
{
	char buf_id[100];
	sprintf_s(buf_id, "_%d", id);
	std::string  xml_path = outPath + "\\" + itemName + buf_id + ".xml";
	std::string  img_path = outPath + "\\" + itemName + buf_id + ".jpg";


	imwrite(img_path.c_str(), img);

	std::vector<cv::Rect> rectListTmp;
	std::vector<std::string> classListTmp;
	for (int j = 0; j < typeTargets.boxes.size(); j++)
	{
		rectListTmp.push_back(typeTargets.boxes[j]);
		classListTmp.push_back(typeTargets.classNames[j]);
	}

	DT_WriteAnnoXml(xml_path, img, rectListTmp, classListTmp);
}
void writeImageXml(std::string outPath, std::string itemName, cv::Mat img, int cameraId, int id, std::vector<FinsObject> typeTargets)
{
	std::string  xml_path = outPath + "\\" + itemName  +"_" + std::to_string(cameraId) + "_" + std::to_string(id) + ".xml";
	std::string  img_path = outPath + "\\" + itemName + "_" + std::to_string(cameraId) + "_" + std::to_string(id) + ".jpg";


	imwrite(img_path.c_str(), img);

	std::vector<cv::Rect> rectListTmp;
	std::vector<std::string> classListTmp;
	for (int i = 0; i < typeTargets.size(); i++)
	{
		rectListTmp.push_back(typeTargets[i].box);
		classListTmp.push_back(typeTargets[i].className);
	}

	DT_WriteAnnoXml(xml_path, img, rectListTmp, classListTmp);
}
//void writeImageXml(std::string outPath, std::string itemName, cv::Mat img, int id, std::vector<Detection> typeTargets)
//{
//	char buf_id[100];
//	sprintf_s(buf_id, "_%d", id);
//	std::string  xml_path = outPath + "\\" + itemName + buf_id + ".xml";
//	std::string  img_path = outPath + "\\" + itemName + buf_id + ".jpg";
//
//
//	imwrite(img_path.c_str(), img);
//
//	std::vector<cv::Rect> rectListTmp;
//	std::vector<std::string> classListTmp;
//	for (int i = 0; i < typeTargets.size(); i++)
//	{
//		rectListTmp.push_back(typeTargets[i].box);
//		classListTmp.push_back(std::to_string(typeTargets[i].class_id));
//	}
//
//	DT_WriteAnnoXml(xml_path, img, rectListTmp, classListTmp);
//}
