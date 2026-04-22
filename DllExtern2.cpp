#include "DllExtern.h"
#include <atomic>
#include <future>
#include <condition_variable>
#include <chrono>

using namespace std;
using namespace cv;

// ???????????
struct TimeoutControl {
    std::atomic<bool> timed_out{ false };
    std::atomic<bool> completed{ false };
    std::mutex mutex;
    std::condition_variable cv;
};

// ?????????????????
struct CameraContext {
    int cameraId;
    Log* logger;
};
//
//// ??????��??TLS???????????????????
static thread_local std::unique_ptr<CameraContext> tls_context1;
static std::mutex global_resource_mutex1;
//
// ??????????????ID?��???
std::map<int, double> g_cameraDurations1;
std::mutex g_durationMutex1;



extern "C" __declspec(dllexport) int DLL_InspCapOmni(
    unsigned char* imageData,   // ???????????
    int imgWidth,               // ??????
    int imgHeight,               // ??????
    int jobId,                  // ???id
    int cameraId,               // ???id
    unsigned char* imageDataOut// ??????????
)
{

    bool isLoadConfig = true;
    if (jobId > 1)
    {
        isLoadConfig = false;
    }
    int rv = PRESSCAP_RETURN_OK;
    auto start = std::chrono::high_resolution_clock::now();
    Common* COM = new Common;
    InspPressCapOut outInfo;
    outInfo.system.startTime = COM->time_t2string_with_ms();
    outInfo.system.jobId = jobId;
    outInfo.system.cameraId = cameraId;
    std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


    int timeOut = 30000;
    const int nChannel = 3;
    std::unique_ptr<AnalyseMat> ANA(new AnalyseMat);
    cv::Mat img;
    ANA->DataToMat(imageData, imgWidth, imgHeight, nChannel, img);
    string configPath = "D:/config/";

    char bufLog[100];
    sprintf(bufLog, "PressCap/camera_%d/", outInfo.system.cameraId);
    char bufConfig[100];
    sprintf(bufConfig, "/InspPressCapConfig_%d.txt", outInfo.system.cameraId);
    outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
    outInfo.paths.intermediateImagesDir =
        ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
    outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
    outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
    outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
    outInfo.paths.configFile = configPath + std::string(bufConfig);
    outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

    outInfo.status.errorMessage = "OK";
    outInfo.status.statusCode = PRESSCAP_RETURN_OK;
    outInfo.status.logs.reserve(100);

    // ????????????????
    if (!tls_context1) {
        tls_context1 = std::make_unique<CameraContext>();
        tls_context1->cameraId = cameraId;
        tls_context1->logger = new Log();
    }

    Log* LOG = tls_context1->logger;

    if (img.empty()) {
        Log::WriteAsyncLog("DLL_InspCapOmni: ??????????!", ERR, "D://aoi_error_log.txt", true);
        return PRESSCAP_RETURN_INPUT_PARA_ERR;
    }
    else if (img.channels() != 3) {
        Log::WriteAsyncLog("DLL_InspCapOmni: img.channels() != 3", ERR, "D://aoi_error_log.txt", true);
        return PRESSCAP_RETURN_INPUT_PARA_ERR;
    }
    else if (jobId < 0) {
        Log::WriteAsyncLog("DLL_InspCapOmni: jobId < 0", ERR, "D://aoi_error_log.txt", true);
        return PRESSCAP_RETURN_INPUT_PARA_ERR;
    }
    else if (cameraId < 0 || cameraId > 9) {
        Log::WriteAsyncLog("DLL_InspCapOmni: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
        return PRESSCAP_RETURN_INPUT_PARA_ERR;
    }


    // ??????????????
    auto control = std::make_shared<TimeoutControl>();
    outInfo.images.outputImg.data = std::make_shared<cv::Mat>(img.clone());

    // ??????????
    InspPressCap* pInspPressCap = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_resource_mutex1);
        pInspPressCap = new InspPressCap(std::string(configPath), img, cameraId, jobId, isLoadConfig, timeOut, outInfo);
        pInspPressCap->SetTimeoutFlagRef(control->timed_out);
        pInspPressCap->SetStartTimePoint(start);
    }

    try {
        // ???????????????
        auto future = std::async(std::launch::async, [&]() {
            try {
                int algoResult = pInspPressCap->PressCap_Main(outInfo);

                // ?????????
                {
                    std::lock_guard<std::mutex> lock(control->mutex);
                    control->completed = true;
                }
                control->cv.notify_one();

                return algoResult;
            }
            catch (const std::exception& e) {
                Log::WriteAsyncLog(std::string("Algorithm exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
                return -2;
            }
            });

        // ?????????
        std::unique_lock<std::mutex> lock(control->mutex);
        if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
            [control] { return control->completed.load(); }))
        {
            // ???????
            rv = future.get();

            // ???????
            if (!outInfo.images.outputImg.mat().empty()) {
                ANA->MatToData(outInfo.images.outputImg.mat(), imageDataOut);
            }
        }
        else {
            // ???????
            control->timed_out = true;
            Log::WriteAsyncLog("????��??!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);          
            ANA->MatToData(img, imageDataOut);

        }
    }
    catch (const std::exception& e) {
        delete COM;
        Log::WriteAsyncLog(std::string("CR_DLL_InspCapOmni exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
        rv = PRESSCAP_RETURN_ALGO_ERR;
    }

    // ???????
    {
        std::lock_guard<std::mutex> lock(global_resource_mutex1);
        delete pInspPressCap;
        delete COM;
        pInspPressCap = nullptr;
    }

    // ??????????
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    {
        std::lock_guard<std::mutex> lock(g_durationMutex1);
        g_cameraDurations1[cameraId] = duration;
    }


    return rv;
}

extern "C" __declspec(dllexport) int DLL_InspLev(
    unsigned char* imageData,   // ???????????
    int imgWidth,               // ??????
    int imgHeight,               // ??????
    int jobId,                  // ???id
    int cameraId,               // ???id
    unsigned char* imageDataOut// ??????????
)
{

    bool isLoadConfig = true;
    if (jobId > 1)
    {
        isLoadConfig = false;
    }
    int rv = PRESSCAP_RETURN_OK;
    auto start = std::chrono::high_resolution_clock::now();
    Common* COM = new Common;
    InspLevelOut outInfo;
    outInfo.system.startTime = COM->time_t2string_with_ms();
    outInfo.system.jobId = jobId;
    outInfo.system.cameraId = cameraId;
    std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


    int timeOut = 30000;
    const int nChannel = 1;
    std::unique_ptr<AnalyseMat> ANA(new AnalyseMat);
    cv::Mat img;
    ANA->DataToMat(imageData, imgWidth, imgHeight, nChannel, img);
    string configPath = "D:/config/";

    char bufLog[100];
    sprintf(bufLog, "Level/camera_%d/", outInfo.system.cameraId);
    char bufConfig[100];
    sprintf(bufConfig, "/InspLevelConfig_%d.txt", outInfo.system.cameraId);
    outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
    outInfo.paths.intermediateImagesDir =
        ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
    outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
    outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
    outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
    outInfo.paths.configFile = configPath + std::string(bufConfig);
    outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

    outInfo.status.errorMessage = "OK";
    outInfo.status.statusCode = LEVEL_RETURN_OK;
    outInfo.status.logs.reserve(100);

    // ????????????????
    if (!tls_context1) {
        tls_context1 = std::make_unique<CameraContext>();
        tls_context1->cameraId = cameraId;
        tls_context1->logger = new Log();
    }

    Log* LOG = tls_context1->logger;

    if (img.empty()) {
        Log::WriteAsyncLog("DLL_InspLevel: ??????????!", ERR, "D://aoi_error_log.txt", true);
        return LEVEL_RETURN_INPUT_PARA_ERR;
    }
    else if (img.channels() != 1) {
        Log::WriteAsyncLog("DLL_InspLevel: img.channels() != 1", ERR, "D://aoi_error_log.txt", true);
        return LEVEL_RETURN_INPUT_PARA_ERR;
    }
    else if (jobId < 0) {
        Log::WriteAsyncLog("DLL_InspLevel: jobId < 0", ERR, "D://aoi_error_log.txt", true);
        return LEVEL_RETURN_INPUT_PARA_ERR;
    }
    else if (cameraId < 0 || cameraId > 9) {
        Log::WriteAsyncLog("DLL_InspLevel: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
        return LEVEL_RETURN_INPUT_PARA_ERR;
    }


    // ??????????????
    auto control = std::make_shared<TimeoutControl>();
    outInfo.images.outputImg= img.clone();

    // ??????????
    InspLevel* pInspLevel = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_resource_mutex1);
        pInspLevel = new InspLevel(std::string(configPath), img, cameraId, jobId, isLoadConfig, timeOut, outInfo);
        pInspLevel->SetTimeoutFlagRef(control->timed_out);
        pInspLevel->SetStartTimePoint(start);
    }

    try {
        // ???????????????
        auto future = std::async(std::launch::async, [&]() {
            try {
                int algoResult = pInspLevel->Level_Main(outInfo);

                // ?????????
                {
                    std::lock_guard<std::mutex> lock(control->mutex);
                    control->completed = true;
                }
                control->cv.notify_one();

                return algoResult;
            }
            catch (const std::exception& e) {
                Log::WriteAsyncLog(std::string("Algorithm exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
                return -2;
            }
            });

        // ?????????
        std::unique_lock<std::mutex> lock(control->mutex);
        if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
            [control] { return control->completed.load(); }))
        {
            // ???????
            rv = future.get();

            // ???????
            if (!outInfo.images.outputImg.empty()) {
                ANA->MatToData(outInfo.images.outputImg, imageDataOut);
            }
        }
        else {
            // ???????
            control->timed_out = true;
            Log::WriteAsyncLog("????��??!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);
            ANA->MatToData(img, imageDataOut);

        }
    }
    catch (const std::exception& e) {
        delete COM;
        Log::WriteAsyncLog(std::string("CR_DLL_InspLevel exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
        rv = LEVEL_RETURN_ALGO_ERR;
    }

    // ???????
    {
        std::lock_guard<std::mutex> lock(global_resource_mutex1);
        delete pInspLevel;
        delete COM;
        pInspLevel = nullptr;
    }

    // ??????????
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    {
        std::lock_guard<std::mutex> lock(g_durationMutex1);
        g_cameraDurations1[cameraId] = duration;
    }

    return rv;
}

extern "C" __declspec(dllexport) int DLL_InspHandle(
    unsigned char* imageData,   // ???????????
    int imgWidth,               // ??????
    int imgHeight,               // ??????
    int jobId,                  // ???id
    int cameraId,               // ???id
    unsigned char* imageDataOut// ??????????
)
{

    bool isLoadConfig = true;
    if (jobId > 1)
    {
        isLoadConfig = false;
    }
    int rv = HANDLE_RETURN_OK;
    auto start = std::chrono::high_resolution_clock::now();
    Common* COM = new Common;
    InspHandleOut outInfo;
    outInfo.system.startTime = COM->time_t2string_with_ms();
    outInfo.system.jobId = jobId;
    outInfo.system.cameraId = cameraId;
    std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


    int timeOut = 30000;
    const int nChannel = 1;
    std::unique_ptr<AnalyseMat> ANA(new AnalyseMat);
    cv::Mat img;
    ANA->DataToMat(imageData, imgWidth, imgHeight, nChannel, img);
    string configPath = "D:/config/";

    char bufLog[100];
    sprintf(bufLog, "Handle/camera_%d/", outInfo.system.cameraId);
    char bufConfig[100];
    sprintf(bufConfig, "/InspHandleConfig_%d.txt", outInfo.system.cameraId);
    outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
    outInfo.paths.intermediateImagesDir =
        ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
    outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
    outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
    outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
    outInfo.paths.configFile = configPath + std::string(bufConfig);
    outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

    outInfo.status.errorMessage = "OK";
    outInfo.status.statusCode = HANDLE_RETURN_OK;
    outInfo.status.logs.reserve(100);

    // ????????????????
    if (!tls_context1) {
        tls_context1 = std::make_unique<CameraContext>();
        tls_context1->cameraId = cameraId;
        tls_context1->logger = new Log();
    }

    Log* LOG = tls_context1->logger;

    if (img.empty()) {
        Log::WriteAsyncLog("DLL_InspHandle: ??????????!", ERR, "D://aoi_error_log.txt", true);
        return HANDLE_RETURN_INPUT_PARA_ERR;
    }
    else if (img.channels() != 1) {
        Log::WriteAsyncLog("DLL_InspHandle: img.channels() != 1", ERR, "D://aoi_error_log.txt", true);
        return HANDLE_RETURN_INPUT_PARA_ERR; 
    }
    else if (jobId < 0) {
        Log::WriteAsyncLog("DLL_InspHandle: jobId < 0", ERR, "D://aoi_error_log.txt", true);
        return HANDLE_RETURN_INPUT_PARA_ERR;
    }
    else if (cameraId < 0 || cameraId > 9) {
        Log::WriteAsyncLog("DLL_InspHandle: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
        return HANDLE_RETURN_INPUT_PARA_ERR;
    }


    // ??????????????
    auto control = std::make_shared<TimeoutControl>();
    outInfo.images.outputImg.data = std::make_shared<cv::Mat>(img.clone());

    // ??????????
    InspHandle* pInspHandle = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_resource_mutex1);
        pInspHandle = new InspHandle(std::string(configPath), img, cameraId, jobId, isLoadConfig, timeOut, outInfo);
        pInspHandle->SetTimeoutFlagRef(control->timed_out);
        pInspHandle->SetStartTimePoint(start);
    }

    try {
        // ???????????????
        auto future = std::async(std::launch::async, [&]() {
            try {
                int algoResult = pInspHandle->Handle_Main(outInfo);

                // ?????????
                {
                    std::lock_guard<std::mutex> lock(control->mutex);
                    control->completed = true;
                }
                control->cv.notify_one();

                return algoResult;
            }
            catch (const std::exception& e) {
                Log::WriteAsyncLog(std::string("Algorithm exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
                return -2;
            }
            });

        // ?????????
        std::unique_lock<std::mutex> lock(control->mutex);
        if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
            [control] { return control->completed.load(); }))
        {
            // ???????
            rv = future.get();

            // ???????
            if (!outInfo.images.outputImg.mat().empty()) {
                ANA->MatToData(outInfo.images.outputImg.mat(), imageDataOut);
            }
        }
        else {
            // ???????
            control->timed_out = true;
            Log::WriteAsyncLog("????��??!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);
            ANA->MatToData(img, imageDataOut);

        }
    }
    catch (const std::exception& e) {
        delete COM;
        Log::WriteAsyncLog(std::string("CR_DLL_InspHandle exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
        rv = HANDLE_RETURN_ALGO_ERR;
    }

    // ???????
    {
        std::lock_guard<std::mutex> lock(global_resource_mutex1);
        delete pInspHandle;
        delete COM;
        pInspHandle = nullptr;
    }

    // ??????????
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    {
        std::lock_guard<std::mutex> lock(g_durationMutex1);
        g_cameraDurations1[cameraId] = duration;
    }

    return rv;
}

extern "C" __declspec(dllexport) int DLL_InspBox(
    unsigned char* imageData,   // ???????????
    int imgWidth,               // ??????
    int imgHeight,               // ??????
    int jobId,                  // ???id
    int cameraId,               // ???id
    unsigned char* imageDataOut// ??????????
)
{

    bool isLoadConfig = true;
    if (jobId > 1)
    {
        isLoadConfig = false;
    }
    int rv = BOTTLENUM_RETURN_OK;
    auto start = std::chrono::high_resolution_clock::now();
    Common* COM = new Common;
    InspBottleNumOut outInfo;
    outInfo.system.startTime = COM->time_t2string_with_ms();
    outInfo.system.jobId = jobId;
    outInfo.system.cameraId = cameraId;
    std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


    int timeOut = 30000;
    const int nChannel = 3;
    std::unique_ptr<AnalyseMat> ANA(new AnalyseMat);
    cv::Mat img;
    ANA->DataToMat(imageData, imgWidth, imgHeight, nChannel, img);
    string configPath = "D:/config/";

    char bufLog[100];
    sprintf(bufLog, "BottleNum/camera_%d/", outInfo.system.cameraId);
    char bufConfig[100];
    sprintf(bufConfig, "/InspBottleNumConfig_%d.txt", outInfo.system.cameraId);
    outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
    outInfo.paths.intermediateImagesDir =
        ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
    outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
    outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
    outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
    outInfo.paths.configFile = configPath + std::string(bufConfig);
    outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

    outInfo.status.errorMessage = "OK";
    outInfo.status.statusCode = BOTTLENUM_RETURN_OK;
    outInfo.status.logs.reserve(100);

    // ????????????????
    if (!tls_context1) {
        tls_context1 = std::make_unique<CameraContext>();
        tls_context1->cameraId = cameraId;
        tls_context1->logger = new Log();
    }

    Log* LOG = tls_context1->logger;

    if (img.empty()) {
        Log::WriteAsyncLog("DLL_InspBox: ??????????!", ERR, "D://aoi_error_log.txt", true);
        return BOTTLENUM_RETURN_INPUT_PARA_ERR;
    }
    else if (img.channels() != 3) {
        Log::WriteAsyncLog("DLL_InspBox: img.channels() != 3", ERR, "D://aoi_error_log.txt", true);
        return BOTTLENUM_RETURN_INPUT_PARA_ERR;
    }
    else if (jobId < 0) {
        Log::WriteAsyncLog("DLL_InspBox: jobId < 0", ERR, "D://aoi_error_log.txt", true);
        return BOTTLENUM_RETURN_INPUT_PARA_ERR;
    }
    else if (cameraId < 0 || cameraId > 9) {
        Log::WriteAsyncLog("DLL_InspBox: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
        return BOTTLENUM_RETURN_INPUT_PARA_ERR;
    }


    // ??????????????
    auto control = std::make_shared<TimeoutControl>();
    outInfo.images.outputImg.data = std::make_shared<cv::Mat>(img.clone());

    // ??????????
    InspBottleNum* pInspBottleNum = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_resource_mutex1);
        pInspBottleNum = new InspBottleNum(std::string(configPath), img, cameraId, jobId, isLoadConfig, timeOut, outInfo);
        pInspBottleNum->SetTimeoutFlagRef(control->timed_out);
        pInspBottleNum->SetStartTimePoint(start);
    }

    try {
        // ???????????????
        auto future = std::async(std::launch::async, [&]() {
            try {
                int algoResult = pInspBottleNum->BottleNum_Main(outInfo);

                // ?????????
                {
                    std::lock_guard<std::mutex> lock(control->mutex);
                    control->completed = true;
                }
                control->cv.notify_one();

                return algoResult;
            }
            catch (const std::exception& e) {
                Log::WriteAsyncLog(std::string("Algorithm exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
                return -2;
            }
            });

        // ?????????
        std::unique_lock<std::mutex> lock(control->mutex);
        if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
            [control] { return control->completed.load(); }))
        {
            // ???????
            rv = future.get();

            // ???????
            if (!outInfo.images.outputImg.mat().empty()) {
                ANA->MatToData(outInfo.images.outputImg.mat(), imageDataOut);
            }
        }
        else {
            // ???????
            control->timed_out = true;
            Log::WriteAsyncLog("????��??!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);
            ANA->MatToData(img, imageDataOut);

        }
    }
    catch (const std::exception& e) {
        delete COM;
        Log::WriteAsyncLog(std::string("CR_DLL_InspBox exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
        rv = BOTTLENUM_RETURN_ALGO_ERR;
    }

    // ???????
    {
        std::lock_guard<std::mutex> lock(global_resource_mutex1);
        delete pInspBottleNum;
        delete COM;
        pInspBottleNum = nullptr;
    }

    // ??????????
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    {
        std::lock_guard<std::mutex> lock(g_durationMutex1);
        g_cameraDurations1[cameraId] = duration;
    }


    return rv;
}


extern "C" __declspec(dllexport) int DLL_InspCodePET(
    unsigned char* imageData,   // ???????????
    int imgWidth,               // ??????
    int imgHeight,               // ??????
    int jobId,                  // ???id
    int cameraId,                  //cameraId
    unsigned char* imageDataOut// ??????????
)
{

    bool isLoadConfig = true;
    if (jobId > 1)
    {
        isLoadConfig = false;
    }
    int rv = CODE_RETURN_OK;
    auto start = std::chrono::high_resolution_clock::now();
    Common* COM = new Common;
    InspCodeOut outInfo;
    outInfo.system.startTime = COM->time_t2string_with_ms();
    outInfo.system.jobId = jobId;
    outInfo.system.cameraId = cameraId;
    std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


    int timeOut = 300000;
    const int nChannel = 1;
    std::unique_ptr<AnalyseMat> ANA(new AnalyseMat);
    cv::Mat img;
    ANA->DataToMat(imageData, imgWidth, imgHeight, nChannel, img);
    string configPath = "D:/config/";

    char bufLog[100];
    sprintf(bufLog, "Code/camera_%d/", outInfo.system.cameraId);
    char bufConfig[100];
    sprintf(bufConfig, "/InspCodeConfig_%d.txt", outInfo.system.cameraId);
    outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
    outInfo.paths.intermediateImagesDir =
        ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
    outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
    outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
    outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
    outInfo.paths.configFile = configPath + std::string(bufConfig);
    outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

    outInfo.status.errorMessage = "OK";
    outInfo.status.statusCode = CODE_RETURN_OK;
    outInfo.status.logs.reserve(100);

    // ????????????????
    if (!tls_context1) {
        tls_context1 = std::make_unique<CameraContext>();
        tls_context1->cameraId = cameraId;
        tls_context1->logger = new Log();
    }

    Log* LOG = tls_context1->logger;

    if (img.empty()) {
        Log::WriteAsyncLog("DLL_InspCode: ??????????!", ERR, "D://aoi_error_log.txt", true);
        return CODE_RETURN_INPUT_PARA_ERR;
    }
    else if (img.channels() != 1) {
        Log::WriteAsyncLog("DLL_InspCode: img.channels() != 1", ERR, "D://aoi_error_log.txt", true); 
        return CODE_RETURN_INPUT_PARA_ERR;
    }
    else if (jobId < 0) {
        Log::WriteAsyncLog("DLL_InspCode: jobId < 0", ERR, "D://aoi_error_log.txt", true);
        return CODE_RETURN_INPUT_PARA_ERR;
    }
    else if (cameraId < 0 || cameraId > 9) {
        Log::WriteAsyncLog("DLL_InspCode: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
        return CODE_RETURN_INPUT_PARA_ERR;
    }


    // ??????????????
    auto control = std::make_shared<TimeoutControl>();
    outInfo.images.outputImg = img.clone();

    // ??????????
    InspCode* pInspCode = nullptr;
    {
        std::lock_guard<std::mutex> lock(global_resource_mutex1);
        pInspCode = new InspCode(std::string(configPath), img, cameraId, jobId, isLoadConfig, timeOut, outInfo);
        pInspCode->SetTimeoutFlagRef(control->timed_out);
        pInspCode->SetStartTimePoint(start);
    }

    try {
        // ???????????????
        auto future = std::async(std::launch::async, [&]() {
            try {
                int algoResult = pInspCode->Code_Main(outInfo);

                // ?????????
                {
                    std::lock_guard<std::mutex> lock(control->mutex);
                    control->completed = true;
                }
                control->cv.notify_one();

                return algoResult;
            }
            catch (const std::exception& e) {
                Log::WriteAsyncLog(std::string("Algorithm exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
                return -2;
            }
            });

        // ?????????
        std::unique_lock<std::mutex> lock(control->mutex);
        if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
            [control] { return control->completed.load(); }))
        {
            // ???????
            rv = future.get();

            // ???????
            if (!outInfo.images.outputImg.empty()) {
                ANA->MatToData(outInfo.images.outputImg, imageDataOut);
            }
        }
        else {
            // ???????
            control->timed_out = true;
            Log::WriteAsyncLog("????��??!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);
            ANA->MatToData(img, imageDataOut);

        }
    }
    catch (const std::exception& e) {
        delete COM;
        Log::WriteAsyncLog(std::string("CR_DLL_InspCode exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
        rv = CODE_RETURN_ALGO_ERR;
    }

    // ???????
    {
        std::lock_guard<std::mutex> lock(global_resource_mutex1);
        delete pInspCode;
        delete COM;
        pInspCode = nullptr;
    }

    // ??????????
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    {
        std::lock_guard<std::mutex> lock(g_durationMutex1);
        g_cameraDurations1[cameraId] = duration;
    }


    return rv;
}