#include "DllInvoker.h"

#include <windows.h>

namespace {
template<typename T>
T GetProcChecked(HMODULE module, const char* name) {
    return reinterpret_cast<T>(GetProcAddress(module, name));
}
}

DllInvoker::DllInvoker()
    : module_(nullptr), fnCapOmni_(nullptr), fnLevel_(nullptr), fnHandle_(nullptr), fnBox_(nullptr), fnCode_(nullptr) {
}

DllInvoker::~DllInvoker() {
    if (module_) {
        FreeLibrary(static_cast<HMODULE>(module_));
        module_ = nullptr;
    }
}

bool DllInvoker::load(const QString& dllPath, QString* errMsg) {
    if (module_) {
        FreeLibrary(static_cast<HMODULE>(module_));
        module_ = nullptr;
    }

    HMODULE mod = LoadLibraryW(reinterpret_cast<LPCWSTR>(dllPath.utf16()));
    if (!mod) {
        if (errMsg) *errMsg = "DLL 加载失败";
        return false;
    }

    fnCapOmni_ = GetProcChecked<FnInspectCapOmni>(mod, "CR_DLL_InspCapOmni");
    fnLevel_ = GetProcChecked<FnInspectLevel>(mod, "CR_DLL_InspLevel");
    fnHandle_ = GetProcChecked<FnInspectHandle>(mod, "CR_DLL_InspHandle");
    fnBox_ = GetProcChecked<FnInspectBox>(mod, "CR_DLL_InspBoxBag");
    fnCode_ = GetProcChecked<FnInspectCode>(mod, "CR_DLL_InspCode");

    if (!fnCapOmni_ || !fnLevel_ || !fnHandle_ || !fnBox_ || !fnCode_) {
        FreeLibrary(mod);
        if (errMsg) *errMsg = "DLL 缺少必要导出函数";
        return false;
    }

    module_ = mod;
    return true;
}

bool DllInvoker::isLoaded() const {
    return module_ != nullptr;
}

int DllInvoker::inspect(InspectType type, const cv::Mat& input, int jobId, int cameraId, cv::Mat* output, QString* errMsg) {
    if (!isLoaded()) {
        if (errMsg) *errMsg = "请先加载 DLL";
        return -1;
    }
    if (input.empty()) {
        if (errMsg) *errMsg = "输入图像为空";
        return -1;
    }

    const char* configPath = "D:/CONFIG_CR_3.0/";
    const bool loadConfig = (jobId <= 1);
    const int timeoutMs = 30000;
    int channels = 3;
    switch (type) {
        case InspectType::CapOmni: channels = 3; break;
        case InspectType::Level: channels = 1; break;
        case InspectType::Handle: channels = 1; break;
        case InspectType::Box: channels = 3; break;
        case InspectType::Code: channels = 1; break;
    }

    cv::Mat in;
    if (channels == 1) {
        if (input.channels() == 1) in = input.clone();
        else cv::cvtColor(input, in, cv::COLOR_BGR2GRAY);
    } else {
        if (input.channels() == 3) in = input.clone();
        else cv::cvtColor(input, in, cv::COLOR_GRAY2BGR);
    }

    int rv = -1;
    cv::Mat out = in.clone();
    std::string detail;

    switch (type) {
        case InspectType::CapOmni: {
            InspCapOmniResult result;
            rv = fnCapOmni_(in, cameraId, jobId, configPath, loadConfig, timeoutMs, &result);
            if (!result.imgOut.empty()) out = result.imgOut.clone();
            detail = result.errorMessage;
            break;
        }
        case InspectType::Level: {
            InspLevelResult result;
            rv = fnLevel_(in, cameraId, jobId, configPath, loadConfig, timeoutMs, &result);
            if (!result.imgOut.empty()) out = result.imgOut.clone();
            detail = result.errorMessage;
            break;
        }
        case InspectType::Handle: {
            InspHandleResult result;
            rv = fnHandle_(in, cameraId, jobId, configPath, loadConfig, timeoutMs, &result);
            if (!result.imgOut.empty()) out = result.imgOut.clone();
            detail = result.errorMessage;
            break;
        }
        case InspectType::Box: {
            InspBoxBagResult result;
            rv = fnBox_(in, cameraId, jobId, configPath, loadConfig, timeoutMs, &result);
            if (!result.imgOut.empty()) out = result.imgOut.clone();
            detail = result.errorMessage;
            break;
        }
        case InspectType::Code: {
            InspCodeResult result;
            rv = fnCode_(in, cameraId, jobId, configPath, loadConfig, timeoutMs, &result);
            if (!result.imgOut.empty()) out = result.imgOut.clone();
            detail = result.errorMessage;
            break;
        }
    }

    if (output) *output = out;
    if (rv < 0 && errMsg) {
        QString suffix = detail.empty() ? QString() : QString(" (%1)").arg(QString::fromStdString(detail));
        *errMsg = QString("Inspect failed, code=%1%2").arg(rv).arg(suffix);
    }
    return rv;
}
