#include "HikCamera.h"

#include <cstring>
#include <vector>

#ifdef HAVE_HIK_MVS
#include <MvCameraControl.h>
#endif

HikCamera::HikCamera() : handle_(nullptr) {}

HikCamera::~HikCamera() {
    close();
}

bool HikCamera::isOpened() const {
    return handle_ != nullptr;
}

bool HikCamera::openFirst(std::string* errMsg) {
#ifndef HAVE_HIK_MVS
    if (errMsg) *errMsg = "Hik MVS SDK not enabled in build.";
    return false;
#else
    if (handle_) return true;

    int nRet = MV_CC_Initialize();
    if (nRet != MV_OK) {
        if (errMsg) *errMsg = "MV_CC_Initialize failed";
        return false;
    }

    MV_CC_DEVICE_INFO_LIST devList;
    std::memset(&devList, 0, sizeof(devList));
    nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &devList);
    if (nRet != MV_OK || devList.nDeviceNum == 0) {
        MV_CC_Finalize();
        if (errMsg) *errMsg = "No Hik camera found";
        return false;
    }

    void* localHandle = nullptr;
    nRet = MV_CC_CreateHandle(&localHandle, devList.pDeviceInfo[0]);
    if (nRet != MV_OK || !localHandle) {
        MV_CC_Finalize();
        if (errMsg) *errMsg = "MV_CC_CreateHandle failed";
        return false;
    }

    nRet = MV_CC_OpenDevice(localHandle);
    if (nRet != MV_OK) {
        MV_CC_DestroyHandle(localHandle);
        MV_CC_Finalize();
        if (errMsg) *errMsg = "MV_CC_OpenDevice failed";
        return false;
    }

    MV_CC_SetEnumValue(localHandle, "TriggerMode", 0);
    nRet = MV_CC_StartGrabbing(localHandle);
    if (nRet != MV_OK) {
        MV_CC_CloseDevice(localHandle);
        MV_CC_DestroyHandle(localHandle);
        MV_CC_Finalize();
        if (errMsg) *errMsg = "MV_CC_StartGrabbing failed";
        return false;
    }

    handle_ = localHandle;
    return true;
#endif
}

bool HikCamera::grab(cv::Mat* frame, std::string* errMsg, int timeoutMs) {
    if (!frame) {
        if (errMsg) *errMsg = "Output frame is null";
        return false;
    }

#ifndef HAVE_HIK_MVS
    if (errMsg) *errMsg = "Hik MVS SDK not enabled in build.";
    return false;
#else
    if (!handle_) {
        if (errMsg) *errMsg = "Camera not opened";
        return false;
    }

    MV_FRAME_OUT outFrame;
    std::memset(&outFrame, 0, sizeof(outFrame));
    int nRet = MV_CC_GetImageBuffer(handle_, &outFrame, timeoutMs);
    if (nRet != MV_OK) {
        if (errMsg) *errMsg = "MV_CC_GetImageBuffer timeout or failed";
        return false;
    }

    const unsigned int width = outFrame.stFrameInfo.nWidth;
    const unsigned int height = outFrame.stFrameInfo.nHeight;
    const unsigned int pixelType = outFrame.stFrameInfo.enPixelType;

    if (pixelType == PixelType_Gvsp_Mono8) {
        cv::Mat gray(static_cast<int>(height), static_cast<int>(width), CV_8UC1, outFrame.pBufAddr);
        *frame = gray.clone();
    } else if (pixelType == PixelType_Gvsp_RGB8_Packed) {
        cv::Mat rgb(static_cast<int>(height), static_cast<int>(width), CV_8UC3, outFrame.pBufAddr);
        cv::cvtColor(rgb, *frame, cv::COLOR_RGB2BGR);
    } else if (pixelType == PixelType_Gvsp_BGR8_Packed) {
        cv::Mat bgr(static_cast<int>(height), static_cast<int>(width), CV_8UC3, outFrame.pBufAddr);
        *frame = bgr.clone();
    } else {
        std::vector<unsigned char> bgrBuffer(width * height * 3);
        MV_CC_PIXEL_CONVERT_PARAM cvtParam;
        std::memset(&cvtParam, 0, sizeof(cvtParam));
        cvtParam.nWidth = width;
        cvtParam.nHeight = height;
        cvtParam.pSrcData = outFrame.pBufAddr;
        cvtParam.nSrcDataLen = outFrame.stFrameInfo.nFrameLen;
        cvtParam.enSrcPixelType = outFrame.stFrameInfo.enPixelType;
        cvtParam.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
        cvtParam.pDstBuffer = bgrBuffer.data();
        cvtParam.nDstBufferSize = static_cast<unsigned int>(bgrBuffer.size());
        nRet = MV_CC_ConvertPixelType(handle_, &cvtParam);
        if (nRet != MV_OK) {
            MV_CC_FreeImageBuffer(handle_, &outFrame);
            if (errMsg) *errMsg = "MV_CC_ConvertPixelType failed";
            return false;
        }
        cv::Mat bgr(static_cast<int>(height), static_cast<int>(width), CV_8UC3, bgrBuffer.data());
        *frame = bgr.clone();
    }

    MV_CC_FreeImageBuffer(handle_, &outFrame);
    return !frame->empty();
#endif
}

void HikCamera::close() {
#ifdef HAVE_HIK_MVS
    if (handle_) {
        MV_CC_StopGrabbing(handle_);
        MV_CC_CloseDevice(handle_);
        MV_CC_DestroyHandle(handle_);
        handle_ = nullptr;
        MV_CC_Finalize();
    }
#else
    handle_ = nullptr;
#endif
}

