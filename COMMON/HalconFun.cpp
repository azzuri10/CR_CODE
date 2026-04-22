#include "HalconFun.h"

void ConvertImageToHImage(const cv::Mat& cvImg, HObject* halconImg) {
    if (cvImg.empty()) {
        throw std::invalid_argument("Input image is empty");
    }

    // ��ȡͼ�����
    int width = cvImg.cols;
    int height = cvImg.rows;
    int channels = cvImg.channels();
    int type = cvImg.type() & CV_MAT_DEPTH_MASK;

    // ��֤֧�ֵ���������
    if (type != CV_8U && type != CV_16U && type != CV_16S) {
        throw std::runtime_error("Unsupported image data type");
    }

    cv::Mat src = cvImg;
    if (!src.isContinuous()) {
        src = src.clone();
    }

    // ����ͨ������
    std::vector<cv::Mat> cvChannels;
    if (channels > 1) {
        cv::split(src, cvChannels);
        // OpenCVĬ��BGR˳��ת��ΪRGB
        if (channels == 3) {
            std::swap(cvChannels[0], cvChannels[2]);
        }
    }
    else {
        cvChannels.push_back(cvImg);
    }

    // ����Halconͼ�����
    try {
        Hlong pixelType;
        switch (type) {
        case CV_8U:  pixelType = (channels == 1) ? (Hlong)"byte" : (Hlong)"vector"; break;
        case CV_16U: pixelType = Hlong("uint2"); break;
        case CV_16S: pixelType = Hlong("int2"); break;
        default: throw std::runtime_error("Unsupported pixel type");
        }

        // ��ͨ������
        if (channels > 1) {
            HObject ho_channels[4];
            for (int i = 0; i < channels; ++i) {
                GenImage1(&ho_channels[i],
                    pixelType,
                    width,
                    height,
                    (Hlong)cvChannels[i].data);
            }
            Compose3(ho_channels[0], ho_channels[1], ho_channels[2], halconImg);
        }
        // ��ͨ������
        else {
            GenImage1(halconImg,
                pixelType,
                width,
                height,
                (Hlong)(src.data));
        }
    }
    catch (HalconCpp::HException& ex) {
        throw std::runtime_error("Halcon error: " + std::string(ex.ErrorMessage()));
    }
}