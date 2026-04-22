#pragma once
#ifndef LEVEL_STRUCT_H
#define LEVEL_STRUCT_H

#include "HeaderDefine.h"


enum LEVEL_RETURN_VAL {
    LEVEL_RETURN_ALGO_ERR = -2, //�㷨�쳣
    LEVEL_RETURN_INPUT_PARA_ERR = -1, //������������쳣
    LEVEL_RETURN_TIMEOUT = 0, //�㷨��ʱ
    LEVEL_RETURN_OK = 1,                // OK
    LEVEL_RETURN_CONFIG_ERR = 13002,    //���ô���
    LEVEL_RETURN_NO_LEVEL = 13003,      //��Һλ
    LEVEL_RETURN_LOW_LEVEL = 13004,     //Һλ����
    LEVEL_RETURN_HIGH_LEVEL = 13005,    //Һλ����
    LEVEL_RETURN_OUT = 13006,           //��Ŀ��
    LEVEL_RETURN_EMPTY = 13007,         //��ƿ
    LEVEL_RETURN_FULL = 13008,          //��ƿ

    LEVEL_RETURN_OTHER = 13100,         //����
    LEVEL_RETURN_THREAD_CONTENTION = 13101,   //��������쳣
    
};


struct InspLevelIn {
    bool saveDebugImage = false;
    bool saveResultImage = false;
    bool saveLogTxt = false;
    bool drawResult = false;
    int saveTrain = 0;
    int timeOut;

    cv::Rect roiRect;
    int angleThresh = 40;
    int blurThresh = 10;
    int edgeThresh = 100;
    int projectThresh = 10;
    int grayDis = 10;


    int minY = 0;
    int maxY = 0;

    int hardwareType = 0;
    int modelType = 0;
    std::string locateWeightsFile;   // Ŀ����ģ��
    std::vector<std::string> locateClassName;



};

struct InspLevelOut {
    // ============== ������Ϣ ==============
    struct SystemInfo {
        int jobId = -1;                  // ����ID 
        int cameraId = 0;                // ������
        std::string startTime;           // ��ʼʱ��(YYYY-MM-DD HH:mm:ss)
        std::chrono::milliseconds elapsedTime; // ������ʱ(ms)
        std::atomic<bool> timeoutFlag{ false };
    } system;

    // ============== ·������ ==============
    struct OutputConfig {
        std::string logDirectory;        // ��־��Ŀ¼
        std::string intermediateImagesDir; // �м�ͼ��Ŀ¼
        std::string resultsOKDir;          // OK���Ŀ¼
        std::string resultsNGDir;          // NG���Ŀ¼
        std::string trainDir;          // NG���Ŀ¼
        std::string configFile;            // �㷨�����ļ�
        std::string logFile;               // ��־�ļ�
    } paths{};

    // ============== ͼ������� ==============
    struct {
        cv::Mat roi;                       // ԭʼROI����
        cv::Mat cannyImg;                 // ��λ��Һλ����


        cv::Mat roiLog;                    // ԭʼROI����
        cv::Mat levelRegionDetectFilLog;
        cv::Mat outputImg;           // ����ע�Ľ��ͼ

    } images{};

    // ============== ���β������ ==============
    struct Geometry {
        std::vector<cv::Point> contourLevel;  // �Ƕ������㼯
        
        cv::Rect levelRect;           // Һλ����

        int levelY = 0;              // Һλ�߶�
        int grayDis = 0;                //Һλ���»ҶȲ���ֵ
        int project = 0;                //ͶӰ�÷�

    } geometry{};

    // ============== ����λ����� ==============
    struct LocateInfo {
        bool findLevel = false;         // Һλ
        bool findFull = false;          // ��ƿ
        bool findEmpty = false;          // ��ƿ

        std::vector<FinsObject> details; // ��λ��ϸ��Ϣ
    } locate{};

   

    // ============== ״̬���� ==============
    struct RuntimeInfo {
        LEVEL_RETURN_VAL statusCode = LEVEL_RETURN_OK;
        std::string errorMessage;       // ��������
        std::vector<std::string> logs;  // ��־��Ϣ
    } status{};
};

#endif // LEVEL_STRUCT_H
