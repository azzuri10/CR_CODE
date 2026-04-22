#include "DllExtern.h"
#include <atomic>
#include <future>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <sstream>

using namespace std;
using namespace cv;

// 添加超时控制结构
struct TimeoutControl {
	std::atomic<bool> timed_out{ false };
	std::atomic<bool> completed{ false };
	std::mutex mutex;
	std::condition_variable cv;
};

// 每个相机的处理上下文
struct CameraContext {
	int cameraId;
	Log* logger;
};

// 线程局部存储（TLS）用于每个线程的上下文
static thread_local std::unique_ptr<CameraContext> tls_context;

// 相机上下文管理器（线程安全）
class CameraContextManager {
private:
	std::map<int, std::shared_ptr<CameraContext>> contexts;  // 相机ID到上下文的映射
	std::mutex mtx;                                          // 保护映射的互斥锁

public:
	// 获取单例实例
	static CameraContextManager& Instance() {
		static CameraContextManager instance;  // C++11保证线程安全初始化
		return instance;
	}

	// 获取或创建相机上下文
	std::shared_ptr<CameraContext> GetContext(int cameraId) {
		std::lock_guard<std::mutex> lock(mtx);  // 加锁保护

		// 查找现有上下文
		auto it = contexts.find(cameraId);
		if (it != contexts.end()) {
			return it->second;  // 返回现有上下文
		}

		// 创建新的上下文
		auto ctx = std::make_shared<CameraContext>();
		ctx->cameraId = cameraId;
		ctx->logger = new Log();  // 创建日志记录器

		contexts[cameraId] = ctx;  // 存入映射
		return ctx;               // 返回新上下文
	}

	// 清理所有上下文
	void Cleanup() {
		std::lock_guard<std::mutex> lock(mtx);
		contexts.clear();  // 清空所有上下文
	}

	// 析构函数，确保资源释放
	~CameraContextManager() {
		Cleanup();
	}
};


// 全局容器：按相机ID存储耗时
std::map<int, double> g_cameraDurations;  // 记录每个相机的平均处理时间
std::mutex g_durationMutex;               // 保护耗时记录的互斥锁

// 全局资源互斥锁
static std::mutex global_resource_mutex;

// DLL导出函数：释放Mat内存
extern "C" __declspec(dllexport) void FreeMatBuffer(cv::Mat * mat) {
	if (mat) {
		mat->release();  // 调用OpenCV的release()释放内存
	}
}

// DLL导出函数：释放所有神经网络资源
extern "C" __declspec(dllexport) void ReleaseNetResources() {
	ModelManager::ReleaseAllNetResources();  // 调用ModelManager的静态清理函数
}


// DLL导出函数：主检测接口
extern "C" __declspec(dllexport) int CR_DLL_InspBottleNeck(
	cv::Mat img,           // 输入图像（BGR格式）
	int cameraId,          // 相机ID（0-9）
	int jobId,             // 任务ID（必须大于0）
	const char* configPath, // 配置文件目录路径
	bool loadConfig,       // 是否重新加载配置
	int timeOut,           // 超时时间（毫秒）
	InspBottleNeckResult * result) // 返回结果结构体指针
{
	// 保证最小超时时间为100毫秒，避免过短的超时
	timeOut = MAX(100, timeOut);

	int rv = CODE_RETURN_OK;  // 默认返回成功
	auto start = std::chrono::high_resolution_clock::now();  // 记录开始时间

	// 使用unique_ptr自动管理Common对象，避免内存泄露
	auto COM = std::make_unique<Common>();
	InspBottleNeckOut outInfo;  // 输出信息结构体

	// 初始化输出信息
	outInfo.system.startTime = COM->time_t2string_with_ms();  // 获取当前时间字符串
	outInfo.system.jobId = jobId;        // 设置任务ID
	outInfo.system.cameraId = cameraId;  // 设置相机ID

	// 使用stringstream构建文件路径，避免缓冲区溢出
	std::stringstream ss;

	// 构建日志目录
	ss.str("");
	ss << "BottleNeck/camera_" << cameraId << "/";
	std::string logDir = ProjectConstants::LOG_PATH + ss.str();

	// 构建配置文件路径
	ss.str("");
	ss.clear();
	ss << "/InspBottleNeckConfig_" << cameraId << ".txt";
	std::string configFile = std::string(configPath) + ss.str();

	char bufLog[100];
	sprintf(bufLog, "BottleNeck/camera_%d/", outInfo.system.cameraId);
	char bufConfig[100];
	sprintf(bufConfig, "/InspBottleNeckConfig_%d.txt", outInfo.system.cameraId);
	outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
	outInfo.paths.intermediateImagesDir =
		ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
	outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
	outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
	outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
	outInfo.paths.configFile = configPath + std::string(bufConfig);
	outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

	// 初始化状态信息
	outInfo.status.errorMessage = "OK";
	outInfo.status.statusCode = BOTTLENECK_RETURN_OK;
	outInfo.status.logs.reserve(100);  // 预分配日志空间

	// 记录任务开始日志
	Log::WriteAsyncLog("********** Start Inspection JobID = ", INFO, outInfo.paths.logFile, true, jobId, " ***********");

	// 获取相机上下文（线程安全）
	auto cameraCtx = CameraContextManager::Instance().GetContext(cameraId);
	Log* LOG = cameraCtx->logger;  // 获取日志记录器

	// 输入参数校验
	if (img.empty()) {
		Log::WriteAsyncLog("DLL_InspBottleNeck: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
		rv = CODE_RETURN_INPUT_PARA_ERR;
	}
	else if (jobId < 0) {
		Log::WriteAsyncLog("DLL_InspBottleNeck: jobId < 0", ERR, "D://aoi_error_log.txt", true);
		rv = CODE_RETURN_INPUT_PARA_ERR;
	}
	else if (cameraId < 0 || cameraId > 9) {
		Log::WriteAsyncLog("DLL_InspBottleNeck: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
		rv = CODE_RETURN_INPUT_PARA_ERR;
	}
	else if (img.channels() != 1)
	{
		Log::WriteAsyncLog("DLL_InspBottleNeck: img.channels() != 1", ERR, "D://aoi_error_log.txt", true);
		rv = CODE_RETURN_INPUT_PARA_ERR;
	}



	// 如果参数校验失败，直接返回错误
	if (rv != BOTTLENECK_RETURN_OK) {
		if (result) {
			result->statusCode = rv;
			strncpy_s(result->errorMessage, "输入参数错误", sizeof(result->errorMessage) - 1);
			result->imgOut = img.clone();  // 返回原始图像
		}
		return rv;
	}

	// 创建超时控制对象（使用智能指针自动管理）
	auto control = std::make_shared<TimeoutControl>();

	// 创建算法对象（使用智能指针避免内存泄露）
	std::unique_ptr<InspBottleNeck> pInspBottleNeck;
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);  // 保护全局资源

		try {
			// 创建InspBottleNeck对象，第三个参数为false表示不重新加载配置（如果已经加载过）
			pInspBottleNeck = std::make_unique<InspBottleNeck>(
				std::string(configPath), img, cameraId, jobId, false, timeOut, outInfo);

			// 设置超时控制
			if (pInspBottleNeck) {
				pInspBottleNeck->SetTimeoutFlagRef(control->timed_out);
				pInspBottleNeck->SetStartTimePoint(start);
			}
			else {
				throw std::runtime_error("Failed to create InspBottleNeck object");
			}
		}
		catch (const std::exception& e) {
			Log::WriteAsyncLog(std::string("创建InspBottleNeck对象失败: ") + e.what(),
				ERR, "D://aoi_error_log.txt", true);
			rv = CODE_RETURN_ALGO_ERR;

			if (result) {
				result->statusCode = rv;
				strncpy_s(result->errorMessage, "算法对象创建失败", sizeof(result->errorMessage) - 1);
				result->imgOut = img.clone();
			}
			return rv;
		}
	}

	try {
		// 创建异步任务执行算法
		auto future = std::async(std::launch::async,
			[&pInspBottleNeck, &outInfo, control]() -> int {
				try {
					// 检查对象指针
					if (!pInspBottleNeck) {
						Log::WriteAsyncLog("InspBottleNeck指针为空!", ERR, "D://aoi_error_log.txt", true);
						return CODE_RETURN_ALGO_ERR;
					}

					// 执行主算法
					int algoResult = pInspBottleNeck->BottleNeck_Main(outInfo);

					// 设置完成标志
					{
						std::lock_guard<std::mutex> lock(control->mutex);
						control->completed = true;
					}
					control->cv.notify_one();  // 通知等待线程

					return algoResult;
				}
				catch (const std::exception& e) {
					Log::WriteAsyncLog(std::string("算法执行异常: ") + e.what(),
						ERR, "D://aoi_error_log.txt", true);

					// 异常情况下也设置完成标志
					{
						std::lock_guard<std::mutex> lock(control->mutex);
						control->completed = true;
					}
					control->cv.notify_one();
					return CODE_RETURN_ALGO_ERR;
				}
			}
		);

		bool timeout_occurred = false;  // 标记是否超时

		// 等待算法完成或超时
		{
			std::unique_lock<std::mutex> lock(control->mutex);

			// 等待条件变量，最多等待timeOut毫秒
			if (!control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
				[control]() { return control->completed.load(); })) {

				// 超时处理
				timeout_occurred = true;
				control->timed_out = true;

				// 记录超时日志
				Log::WriteAsyncLog("算法执行超时! jobId = ",
					ERR, "D://aoi_error_log.txt", true, jobId);
			}
		}

		// 处理结果
		if (!timeout_occurred) {
			// 正常完成，获取结果
			rv = future.get();  // 这里会阻塞直到算法线程完成

			// 填充结果对象
			if (result) {
				result->jobId = jobId;
				result->cameraId = cameraId;
				result->statusCode = rv;

				// 复制时间信息
				if (!outInfo.system.startTime.empty()) {
					strncpy_s(result->startTime,
						outInfo.system.startTime.c_str(),
						sizeof(result->startTime) - 1);
				}

				// 复制错误信息
				if (!outInfo.status.errorMessage.empty()) {
					strncpy_s(result->errorMessage,
						outInfo.status.errorMessage.c_str(),
						sizeof(result->errorMessage) - 1);
				}

				// 复制输出图像
				if (!outInfo.images.outputImg.empty()) {
					result->imgOut = outInfo.images.outputImg.clone();
				}
				else {
					result->imgOut = img.clone();
				}
			}
		}
		else {
			// 超时处理
			rv = CODE_RETURN_TIMEOUT;

			// 等待算法线程结束（最多100毫秒）
			if (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
				Log::WriteAsyncLog("警告：算法线程未能在超时后及时退出",
					WARNING, "D://aoi_error_log.txt", true);
			}

			if (result) {
				result->statusCode = CODE_RETURN_TIMEOUT;
				strncpy_s(result->errorMessage,
					"算法执行超时",
					sizeof(result->errorMessage) - 1);

				// 克隆输入图像作为输出
				result->imgOut = img.clone();

				// 在输出图像上标记超时
				if (!result->imgOut.empty()) {
					putTextZH(result->imgOut,
						"算法执行超时",
						cv::Point(50, 50),
						Colors::RED, 55, FW_BOLD);
				}
			}
		}
	}
	catch (const std::exception& e) {
		// 捕获并记录异常
		Log::WriteAsyncLog(std::string("CR_DLL_InspBottleNeck异常: ") + e.what(),
			ERR, "D://aoi_error_log.txt", true);
		rv = CODE_RETURN_ALGO_ERR;

		if (result) {
			result->statusCode = rv;
			strncpy_s(result->errorMessage, "算法执行异常", sizeof(result->errorMessage) - 1);
			result->imgOut = img.clone();
		}
	}

	// 自动清理资源（智能指针自动管理）
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		// 智能指针会自动释放资源，不需要手动delete
	}

	// 记录处理时间
	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;  // 记录本次耗时
	}

	// 记录完成日志
	Log::WriteAsyncLog("********** End Inspection JobID = ",
		INFO, outInfo.paths.logFile, true, jobId,
		", Duration = ", duration, "ms ***********");

	return rv;  // 返回结果代码
}

// DLL导出函数：主检测接口
extern "C" __declspec(dllexport) int CR_DLL_InspCode(
	cv::Mat img,           // 输入图像（BGR格式）
	int cameraId,          // 相机ID（0-9）
	int jobId,             // 任务ID（必须大于0）
	const char* configPath, // 配置文件目录路径
	bool loadConfig,       // 是否重新加载配置
	int timeOut,           // 超时时间（毫秒）
	InspCodeResult * result) // 返回结果结构体指针
{
	// 保证最小超时时间为100毫秒，避免过短的超时
	timeOut = MAX(100, timeOut);

	int rv = CODE_RETURN_OK;  // 默认返回成功
	auto start = std::chrono::high_resolution_clock::now();  // 记录开始时间

	// 使用unique_ptr自动管理Common对象，避免内存泄露
	auto COM = std::make_unique<Common>();
	InspCodeOut outInfo;  // 输出信息结构体

	// 初始化输出信息
	outInfo.system.startTime = COM->time_t2string_with_ms();  // 获取当前时间字符串
	outInfo.system.jobId = jobId;        // 设置任务ID
	outInfo.system.cameraId = cameraId;  // 设置相机ID

	// 使用stringstream构建文件路径，避免缓冲区溢出
	std::stringstream ss;

	// 构建日志目录
	ss.str("");
	ss << "Code/camera_" << cameraId << "/";
	std::string logDir = ProjectConstants::LOG_PATH + ss.str();

	// 构建配置文件路径
	ss.str("");
	ss.clear();
	ss << "/InspCodeConfig_" << cameraId << ".txt";
	std::string configFile = std::string(configPath) + ss.str();

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

	// 初始化状态信息
	outInfo.status.errorMessage = "OK";
	outInfo.status.statusCode = CODE_RETURN_OK;
	outInfo.status.logs.reserve(100);  // 预分配日志空间

	// 记录任务开始日志
	Log::WriteAsyncLog("********** Start Inspection JobID = ", INFO, outInfo.paths.logFile, true, jobId, " ***********");

	// 获取相机上下文（线程安全）
	auto cameraCtx = CameraContextManager::Instance().GetContext(cameraId);
	Log* LOG = cameraCtx->logger;  // 获取日志记录器

	// 输入参数校验
	if (img.empty()) {
		Log::WriteAsyncLog("DLL_InspCode: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
		rv = CODE_RETURN_INPUT_PARA_ERR;
	}
	else if (jobId < 0) {
		Log::WriteAsyncLog("DLL_InspCode: jobId < 0", ERR, "D://aoi_error_log.txt", true);
		rv = CODE_RETURN_INPUT_PARA_ERR;
	}
	else if (cameraId < 0 || cameraId > 9) {
		Log::WriteAsyncLog("DLL_InspCode: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
		rv = CODE_RETURN_INPUT_PARA_ERR;
	}

	// 如果参数校验失败，直接返回错误
	if (rv != CODE_RETURN_OK) {
		if (result) {
			result->statusCode = rv;
			strncpy_s(result->errorMessage, "输入参数错误", sizeof(result->errorMessage) - 1);
			result->imgOut = img.clone();  // 返回原始图像
		}
		return rv;
	}

	// 创建超时控制对象（使用智能指针自动管理）
	auto control = std::make_shared<TimeoutControl>();

	// 创建算法对象（使用智能指针避免内存泄露）
	std::unique_ptr<InspCode> pInspCode;
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);  // 保护全局资源

		try {
			// 创建InspCode对象，第三个参数为false表示不重新加载配置（如果已经加载过）
			pInspCode = std::make_unique<InspCode>(
				std::string(configPath), img, cameraId, jobId, false, timeOut, outInfo);

			// 设置超时控制
			if (pInspCode) {
				pInspCode->SetTimeoutFlagRef(control->timed_out);
				pInspCode->SetStartTimePoint(start);
			}
			else {
				throw std::runtime_error("Failed to create InspCode object");
			}
		}
		catch (const std::exception& e) {
			Log::WriteAsyncLog(std::string("创建InspCode对象失败: ") + e.what(),
				ERR, "D://aoi_error_log.txt", true);
			rv = CODE_RETURN_ALGO_ERR;

			if (result) {
				result->statusCode = rv;
				strncpy_s(result->errorMessage, "算法对象创建失败", sizeof(result->errorMessage) - 1);
				result->imgOut = img.clone();
			}
			return rv;
		}
	}

	try {
		// 创建异步任务执行算法
		auto future = std::async(std::launch::async,
			[&pInspCode, &outInfo, control]() -> int {
				try {
					// 检查对象指针
					if (!pInspCode) {
						Log::WriteAsyncLog("InspCode指针为空!", ERR, "D://aoi_error_log.txt", true);
						return CODE_RETURN_ALGO_ERR;
					}

					// 执行主算法
					int algoResult = pInspCode->Code_Main(outInfo);

					// 设置完成标志
					{
						std::lock_guard<std::mutex> lock(control->mutex);
						control->completed = true;
					}
					control->cv.notify_one();  // 通知等待线程

					return algoResult;
				}
				catch (const std::exception& e) {
					Log::WriteAsyncLog(std::string("算法执行异常: ") + e.what(),
						ERR, "D://aoi_error_log.txt", true);

					// 异常情况下也设置完成标志
					{
						std::lock_guard<std::mutex> lock(control->mutex);
						control->completed = true;
					}
					control->cv.notify_one();
					return CODE_RETURN_ALGO_ERR;
				}
			}
		);

		bool timeout_occurred = false;  // 标记是否超时

		// 等待算法完成或超时
		{
			std::unique_lock<std::mutex> lock(control->mutex);

			// 等待条件变量，最多等待timeOut毫秒
			if (!control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
				[control]() { return control->completed.load(); })) {

				// 超时处理
				timeout_occurred = true;
				control->timed_out = true;

				// 记录超时日志
				Log::WriteAsyncLog("算法执行超时! jobId = ",
					ERR, "D://aoi_error_log.txt", true, jobId);
			}
		}

		// 处理结果
		if (!timeout_occurred) {
			// 正常完成，获取结果
			rv = future.get();  // 这里会阻塞直到算法线程完成

			// 填充结果对象
			if (result) {
				result->jobId = jobId;
				result->cameraId = cameraId;
				result->statusCode = rv;

				// 复制时间信息
				if (!outInfo.system.startTime.empty()) {
					strncpy_s(result->startTime,
						outInfo.system.startTime.c_str(),
						sizeof(result->startTime) - 1);
				}

				// 复制错误信息
				if (!outInfo.status.errorMessage.empty()) {
					strncpy_s(result->errorMessage,
						outInfo.status.errorMessage.c_str(),
						sizeof(result->errorMessage) - 1);
				}

				// 复制输出图像
				if (!outInfo.images.outputImg.empty()) {
					result->imgOut = outInfo.images.outputImg.clone();
				}
				else {
					result->imgOut = img.clone();
				}
			}
		}
		else {
			// 超时处理
			rv = CODE_RETURN_TIMEOUT;

			// 等待算法线程结束（最多100毫秒）
			if (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
				Log::WriteAsyncLog("警告：算法线程未能在超时后及时退出",
					WARNING, "D://aoi_error_log.txt", true);
			}

			if (result) {
				result->statusCode = CODE_RETURN_TIMEOUT;
				strncpy_s(result->errorMessage,
					"算法执行超时",
					sizeof(result->errorMessage) - 1);

				// 克隆输入图像作为输出
				result->imgOut = img.clone();

				// 在输出图像上标记超时
				if (!result->imgOut.empty()) {
					putTextZH(result->imgOut,
						"算法执行超时",
						cv::Point(50, 50),
						Colors::RED, 55, FW_BOLD);
				}
			}
		}
	}
	catch (const std::exception& e) {
		// 捕获并记录异常
		Log::WriteAsyncLog(std::string("CR_DLL_InspCode异常: ") + e.what(),
			ERR, "D://aoi_error_log.txt", true);
		rv = CODE_RETURN_ALGO_ERR;

		if (result) {
			result->statusCode = rv;
			strncpy_s(result->errorMessage, "算法执行异常", sizeof(result->errorMessage) - 1);
			result->imgOut = img.clone();
		}
	}

	// 自动清理资源（智能指针自动管理）
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		// 智能指针会自动释放资源，不需要手动delete
	}

	// 记录处理时间
	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;  // 记录本次耗时
	}

	// 记录完成日志
	Log::WriteAsyncLog("********** End Inspection JobID = ",
		INFO, outInfo.paths.logFile, true, jobId,
		", Duration = ", duration, "ms ***********");

	return rv;  // 返回结果代码
}

extern "C" __declspec(dllexport) int CR_DLL_InspOcr(
	cv::Mat img,
	int cameraId,
	int jobId,
	const char* configPath,
	bool loadConfig,
	int timeOut,
	InspOcrResult * result)
{
	(void)loadConfig;
	timeOut = MAX(100, timeOut);

	int rv = OCR_RETURN_OK;
	auto start = std::chrono::high_resolution_clock::now();
	auto COM = std::make_unique<Common>();
	InspOcrOut outInfo;
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;

	char bufLog[100];
	sprintf(bufLog, "Ocr/camera_%d/", outInfo.system.cameraId);
	char bufConfig[100];
	sprintf(bufConfig, "/InspCodeConfig_%d.txt", outInfo.system.cameraId);
	outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
	outInfo.paths.intermediateImagesDir =
		ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
	outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
	outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
	outInfo.paths.configFile = std::string(configPath) + std::string(bufConfig);
	outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

	outInfo.status.errorMessage = "OK";
	outInfo.status.statusCode = OCR_RETURN_OK;

	if (img.empty()) {
		Log::WriteAsyncLog("DLL_InspOcr: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
		rv = OCR_RETURN_INPUT_PARA_ERR;
	}
	else if (jobId < 0) {
		Log::WriteAsyncLog("DLL_InspOcr: jobId < 0", ERR, "D://aoi_error_log.txt", true);
		rv = OCR_RETURN_INPUT_PARA_ERR;
	}
	else if (cameraId < 0 || cameraId > 9) {
		Log::WriteAsyncLog("DLL_InspOcr: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
		rv = OCR_RETURN_INPUT_PARA_ERR;
	}

	if (rv != OCR_RETURN_OK) {
		if (result) {
			result->statusCode = rv;
			strncpy_s(result->errorMessage, "输入参数错误", sizeof(result->errorMessage) - 1);
			result->imgOut = img.clone();
		}
		return rv;
	}

	try {
		InspOcr ocr(img, cameraId, jobId, timeOut, outInfo);
		rv = ocr.Ocr_Main(outInfo);

		if (result) {
			result->jobId = jobId;
			result->cameraId = cameraId;
			result->statusCode = rv;
			if (!outInfo.system.startTime.empty()) {
				strncpy_s(result->startTime, outInfo.system.startTime.c_str(), sizeof(result->startTime) - 1);
			}
			if (!outInfo.status.errorMessage.empty()) {
				strncpy_s(result->errorMessage, outInfo.status.errorMessage.c_str(), sizeof(result->errorMessage) - 1);
			}
			result->mergedText = outInfo.ocr.mergedText;
			result->compareResults = outInfo.ocr.compareResults;
			if (!outInfo.images.outputImg.empty()) result->imgOut = outInfo.images.outputImg.clone();
			else result->imgOut = img.clone();
		}
	}
	catch (const std::exception& e) {
		Log::WriteAsyncLog(std::string("CR_DLL_InspOcr异常: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
		rv = OCR_RETURN_ALGO_ERR;
		if (result) {
			result->statusCode = rv;
			strncpy_s(result->errorMessage, "算法执行异常", sizeof(result->errorMessage) - 1);
			result->imgOut = img.clone();
		}
	}

	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;
	}
	Log::WriteAsyncLog("********** End OCR JobID = ", INFO, outInfo.paths.logFile, true, jobId, ", Duration = ", duration, "ms ***********");
	return rv;
}

extern "C" __declspec(dllexport) void FreeInspCapResult(InspCapOmniResult * result) {
	if (!result) return;

	// 释放图像内存
	if (!result->imgOut.empty()) {
		result->imgOut.release();
	}

	// 释放details中的字符串内存
	for (auto& detail : result->defect) {
		// 字符串由std::string管理，无需手动释放
	}
}

extern "C" __declspec(dllexport) int CR_DLL_InspCapOmni(
	cv::Mat img,
	int cameraId,
	int jobId,
	const char* configPath,
	bool loadConfig,
	int timeOut,
	InspCapOmniResult * result)
{
	timeOut = MAX(100, timeOut);

	int rv = PRESSCAP_RETURN_OK;
	auto start = std::chrono::high_resolution_clock::now();
	Common* COM = new Common;
	InspPressCapOut outInfo;
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


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

	// 初始化线程局部上下文
	if (!tls_context) {
		tls_context = std::make_unique<CameraContext>();
		tls_context->cameraId = cameraId;
		tls_context->logger = new Log();
	}

	Log* LOG = tls_context->logger;

	if (img.empty()) {
		Log::WriteAsyncLog("DLL_InspCapOmni: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
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


	// 创建超时控制对象
	auto control = std::make_shared<TimeoutControl>();
	outInfo.images.outputImg.data = std::make_shared<cv::Mat>(img.clone());

	// 创建算法对象
	InspPressCap* pInspPressCap = nullptr;
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		pInspPressCap = new InspPressCap(std::string(configPath), img, cameraId, jobId, false, timeOut, outInfo);
		pInspPressCap->SetTimeoutFlagRef(control->timed_out);
		pInspPressCap->SetStartTimePoint(start);
	}

	try {
		// 启动异步任务执行算法
		auto future = std::async(std::launch::async, [&]() {
			try {
				int algoResult = pInspPressCap->PressCap_Main(outInfo);

				// 设置完成标志
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

		// 等待结果或超时
		std::unique_lock<std::mutex> lock(control->mutex);
		if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
			[control] { return control->completed.load(); }))
		{
			// 正常完成
			rv = future.get();

			// 填充结果对象
			if (result) {
				// 填充基础信息
				result->jobId = jobId;
				result->cameraId = cameraId;
				result->statusCode = rv;

				// 复制时间信息
				if (!outInfo.system.startTime.empty()) {
					strncpy_s(result->startTime, outInfo.system.startTime.c_str(), sizeof(result->startTime) - 1);
				}

				// 复制错误信息
				if (!outInfo.status.errorMessage.empty()) {
					strncpy_s(result->errorMessage, outInfo.status.errorMessage.c_str(), sizeof(result->errorMessage) - 1);
				}

				// 几何数据
				result->capHeight = outInfo.geometry.capHeight;
				result->capHeightDeviation = outInfo.geometry.capHeighttDeviation;
				result->topAngle = outInfo.geometry.topAngle;
				result->bottomAngle = outInfo.geometry.bottomAngle;
				result->topBottomAngleDif = outInfo.geometry.topBottomAngleDif;
				result->capTopType = outInfo.classification.topType.className;
				result->capBottomType = outInfo.classification.bottomType.className;
				// 图像数据
				if (!outInfo.images.outputImg.mat().empty()) {
					result->imgOut = outInfo.images.outputImg.mat().clone();
				}

				// 缺陷详情
				for (const auto& defect : outInfo.locate.details) {
					FinsObject finsObj;
					finsObj.box = defect.box;
					finsObj.confidence = defect.confidence;
					finsObj.className = defect.className;
					result->locates.push_back(finsObj);
				}

				for (const auto& defect : outInfo.defects.details) {
					FinsObject finsObj;
					finsObj.box = defect.box;
					finsObj.confidence = defect.confidence;
					finsObj.className = defect.className;
					result->defect.push_back(finsObj);
				}
			}
		}
		else {
			// 超时处理
			control->timed_out = true;
			Log::WriteAsyncLog("算法执行超时!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);

			if (result) {
				result->statusCode = PRESSCAP_RETURN_TIMEOUT;
				strncpy_s(result->errorMessage, "算法执行超时", sizeof(result->errorMessage) - 1);

				// 克隆输入图像作为输出
				result->imgOut = img.clone();

				// 在输出图像上标记超时
				if (!result->imgOut.empty()) {
					putTextZH(result->imgOut,
						"算法执行超时",
						cv::Point(50, 50),
						Colors::RED, 55, FW_BOLD);
				}
			}
			rv = PRESSCAP_RETURN_TIMEOUT;
		}
	}
	catch (const std::exception& e) {
		delete COM;
		Log::WriteAsyncLog(std::string("CR_DLL_InspCapOmni exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
		rv = PRESSCAP_RETURN_ALGO_ERR;
	}

	// 清理资源
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		delete pInspPressCap;
		delete COM;
		pInspPressCap = nullptr;
	}

	// 记录处理时间
	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;
	}

	return rv;
}

extern "C" __declspec(dllexport) int CR_DLL_InspLevel(
	cv::Mat img,
	int cameraId,
	int jobId,
	const char* configPath,
	bool loadConfig,
	int timeOut,
	InspLevelResult * result)
{
	timeOut = MAX(100, timeOut);

	int rv = LEVEL_RETURN_OK;
	auto start = std::chrono::high_resolution_clock::now();
	Common* COM = new Common;
	InspLevelOut outInfo;
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


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


	// 初始化线程局部上下文
	if (!tls_context) {
		tls_context = std::make_unique<CameraContext>();
		tls_context->cameraId = cameraId;
		tls_context->logger = new Log();
	}

	Log* LOG = tls_context->logger;

	if (img.empty()) {
		Log::WriteAsyncLog("DLL_InspLevel: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
		return -1;
	}
	else if (jobId < 0) {
		Log::WriteAsyncLog("DLL_InspLevel: jobId < 0", ERR, "D://aoi_error_log.txt", true);
		return -1;
	}
	else if (cameraId < 0 || cameraId > 9) {
		Log::WriteAsyncLog("DLL_InspLevel: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
		return -1;
	}



	// 创建超时控制对象
	auto control = std::make_shared<TimeoutControl>();

	// 创建算法对象
	InspLevel* pInspLevel = nullptr;
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		pInspLevel = new InspLevel(std::string(configPath), img, cameraId, jobId, false, timeOut, outInfo);
		pInspLevel->SetTimeoutFlagRef(control->timed_out);
		pInspLevel->SetStartTimePoint(start);
	}

	try {
		// 启动异步任务执行算法
		auto future = std::async(std::launch::async, [&]() {
			try {
				int algoResult = pInspLevel->Level_Main(outInfo);

				// 设置完成标志
				{
					std::lock_guard<std::mutex> lock(control->mutex);
					control->completed = true;
				}
				control->cv.notify_one();

				return algoResult;
			}
			catch (const std::exception& e) {
				Log::WriteAsyncLog(std::string("Algorithm exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
				return -1;
			}
			});

		// 等待结果或超时
		std::unique_lock<std::mutex> lock(control->mutex);
		if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
			[control] { return control->completed.load(); }))
		{
			// 正常完成
			rv = future.get();

			// 填充结果对象
			if (result) {
				// 填充基础信息
				result->jobId = jobId;
				result->cameraId = cameraId;
				result->statusCode = rv;

				// 复制时间信息
				if (!outInfo.system.startTime.empty()) {
					strncpy_s(result->startTime, outInfo.system.startTime.c_str(), sizeof(result->startTime) - 1);
				}

				// 复制错误信息
				if (!outInfo.status.errorMessage.empty()) {
					strncpy_s(result->errorMessage, outInfo.status.errorMessage.c_str(), sizeof(result->errorMessage) - 1);
				}

				// 几何数据
				result->levelY = outInfo.geometry.levelY;
				result->grayDis = outInfo.geometry.grayDis;
				result->project = outInfo.geometry.project;
				// 图像数据
				if (!outInfo.images.outputImg.empty()) {
					result->imgOut = outInfo.images.outputImg.clone();
				}

				//// 缺陷详情
				//for (const auto& defect : outInfo.locate.details) {
				//    FinsObject finsObj;
				//    finsObj.box = defect.box;
				//    finsObj.confidence = defect.confidence;
				//    finsObj.className = defect.className;
				//    result->locates.push_back(finsObj);
				//}

				//for (const auto& defect : outInfo.defects.details) {
				//    FinsObject finsObj;
				//    finsObj.box = defect.box;
				//    finsObj.confidence = defect.confidence;
				//    finsObj.className = defect.className;
				//    result->defect.push_back(finsObj);
				//}
			}
		}
		else {
			// 超时处理
			control->timed_out = true;
			Log::WriteAsyncLog("算法执行超时!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);

			if (result) {
				result->statusCode = LEVEL_RETURN_TIMEOUT;
				strncpy_s(result->errorMessage, "算法执行超时", sizeof(result->errorMessage) - 1);

				// 克隆输入图像作为输出
				result->imgOut = img.clone();

				// 在输出图像上标记超时
				if (!result->imgOut.empty()) {
					putTextZH(result->imgOut,
						"算法执行超时",
						cv::Point(50, 50),
						Colors::RED, 55, FW_BOLD);
				}
			}
			rv = LEVEL_RETURN_TIMEOUT;
		}
	}
	catch (const std::exception& e) {
		delete COM;
		Log::WriteAsyncLog(std::string("CR_DLL_InspLevel exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
		rv = -1;
	}

	// 清理资源
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		delete pInspLevel;
		delete COM;
		pInspLevel = nullptr;
	}

	// 记录处理时间
	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;
	}

	return rv;
}


extern "C" __declspec(dllexport) int CR_DLL_InspHandle(
	cv::Mat img,
	int cameraId,
	int jobId,
	const char* configPath,
	bool loadConfig,
	int timeOut,
	InspHandleResult * result)
{
	timeOut = MAX(100, timeOut);

	int rv = HANDLE_RETURN_OK;
	auto start = std::chrono::high_resolution_clock::now();
	Common* COM = new Common;
	InspHandleOut outInfo;
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


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

	// 初始化线程局部上下文
	if (!tls_context) {
		tls_context = std::make_unique<CameraContext>();
		tls_context->cameraId = cameraId;
		tls_context->logger = new Log();
	}

	Log* LOG = tls_context->logger;

	if (img.empty()) {
		Log::WriteAsyncLog("DLL_InspHandle: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
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


	// 创建超时控制对象
	auto control = std::make_shared<TimeoutControl>();
	outInfo.images.outputImg.data = std::make_shared<cv::Mat>(img.clone());

	// 创建算法对象
	InspHandle* pInspHandle = nullptr;
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		pInspHandle = new InspHandle(std::string(configPath), img, cameraId, jobId, false, timeOut, outInfo);
		pInspHandle->SetTimeoutFlagRef(control->timed_out);
		pInspHandle->SetStartTimePoint(start);
	}

	try {
		// 启动异步任务执行算法
		auto future = std::async(std::launch::async, [&]() {
			try {
				int algoResult = pInspHandle->Handle_Main(outInfo);

				// 设置完成标志
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

		// 等待结果或超时
		std::unique_lock<std::mutex> lock(control->mutex);
		if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
			[control] { return control->completed.load(); }))
		{
			// 正常完成
			rv = future.get();

			// 填充结果对象
			if (result) {
				// 填充基础信息
				result->jobId = jobId;
				result->cameraId = cameraId;
				result->statusCode = rv;

				// 复制时间信息
				if (!outInfo.system.startTime.empty()) {
					strncpy_s(result->startTime, outInfo.system.startTime.c_str(), sizeof(result->startTime) - 1);
				}

				// 复制错误信息
				if (!outInfo.status.errorMessage.empty()) {
					strncpy_s(result->errorMessage, outInfo.status.errorMessage.c_str(), sizeof(result->errorMessage) - 1);
				}

				// 几何数据
				result->filmType = outInfo.classification.filmType.className;
				result->handleType = outInfo.classification.handleType.className;
				// 图像数据
				if (!outInfo.images.outputImg.mat().empty()) {
					result->imgOut = outInfo.images.outputImg.mat().clone();
				}

				// 目标详情
				for (const auto& defect : outInfo.locate.details) {
					FinsObject finsObj;
					finsObj.box = defect.box;
					finsObj.confidence = defect.confidence;
					finsObj.className = defect.className;
					result->locates.push_back(finsObj);
				}
			}
		}
		else {
			// 超时处理
			control->timed_out = true;
			Log::WriteAsyncLog("算法执行超时!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);

			if (result) {
				result->statusCode = HANDLE_RETURN_TIMEOUT;
				strncpy_s(result->errorMessage, "算法执行超时", sizeof(result->errorMessage) - 1);

				// 克隆输入图像作为输出
				result->imgOut = img.clone();

				// 在输出图像上标记超时
				if (!result->imgOut.empty()) {
					putTextZH(result->imgOut,
						"算法执行超时",
						cv::Point(50, 50),
						Colors::RED, 55, FW_BOLD);
				}
			}
			rv = HANDLE_RETURN_TIMEOUT;
		}
	}
	catch (const std::exception& e) {
		delete COM;
		Log::WriteAsyncLog(std::string("CR_DLL_InspHandle exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
		rv = HANDLE_RETURN_ALGO_ERR;
	}

	// 清理资源
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		delete pInspHandle;
		delete COM;
		pInspHandle = nullptr;
	}

	// 记录处理时间
	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;
	}

	return rv;
}



extern "C" __declspec(dllexport) int CR_DLL_InspLabelAll(
	cv::Mat img,
	int cameraId,
	int jobId,
	const char* configPath,
	bool loadConfig,
	int timeOut,
	InspLabelAllResult * result)
{
	timeOut = MAX(100, timeOut);

	int rv = LABELALL_RETURN_OK;
	auto start = std::chrono::high_resolution_clock::now();
	Common* COM = new Common;
	InspLabelAllOut outInfo;
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


	char bufLog[100];
	sprintf(bufLog, "LabelAll/camera_%d/", outInfo.system.cameraId);
	char bufConfig[100];
	sprintf(bufConfig, "/InspLabelAllConfig_%d.txt", outInfo.system.cameraId);
	outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
	outInfo.paths.intermediateImagesDir =
		ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
	outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
	outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
	outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
	outInfo.paths.configFile = configPath + std::string(bufConfig);
	outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

	outInfo.status.errorMessage = "OK";
	outInfo.status.statusCode = LABELALL_RETURN_OK;
	outInfo.status.logs.reserve(100);

	// 初始化线程局部上下文
	if (!tls_context) {
		tls_context = std::make_unique<CameraContext>();
		tls_context->cameraId = cameraId;
		tls_context->logger = new Log();
	}

	Log* LOG = tls_context->logger;

	if (img.empty()) {
		Log::WriteAsyncLog("DLL_InspLabelAll: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
		rv = LABELALL_RETURN_INPUT_PARA_ERR;
	}
	else if (jobId < 0) {
		Log::WriteAsyncLog("DLL_InspLabelAll: jobId < 0", ERR, "D://aoi_error_log.txt", true);
		rv = LABELALL_RETURN_INPUT_PARA_ERR;
	}
	else if (cameraId < 0 || cameraId > 9) {
		Log::WriteAsyncLog("DLL_InspLabelAll: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
		rv = LABELALL_RETURN_INPUT_PARA_ERR;
	}



	// 创建超时控制对象
	auto control = std::make_shared<TimeoutControl>();
	outInfo.images.outputImg.data = std::make_shared<cv::Mat>(img.clone());

	// 创建算法对象
	InspLabelAll* pInspLabelAll = nullptr;
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		pInspLabelAll = new InspLabelAll(std::string(configPath), img, cameraId, jobId, false, timeOut, outInfo);
		pInspLabelAll->SetTimeoutFlagRef(control->timed_out);
		pInspLabelAll->SetStartTimePoint(start);
	}

	try {
		// 启动异步任务执行算法
		auto future = std::async(std::launch::async, [&]() {
			try {
				int algoResult = pInspLabelAll->LabelAll_Main(outInfo);

				// 设置完成标志
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

		// 等待结果或超时
		std::unique_lock<std::mutex> lock(control->mutex);
		if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
			[control] { return control->completed.load(); }))
		{
			// 正常完成
			rv = future.get();

			// 填充结果对象
			if (result) {
				// 填充基础信息
				result->jobId = jobId;
				result->cameraId = cameraId;
				result->statusCode = rv;

				// 复制时间信息
				if (!outInfo.system.startTime.empty()) {
					strncpy_s(result->startTime, outInfo.system.startTime.c_str(), sizeof(result->startTime) - 1);
				}

				// 复制错误信息
				if (!outInfo.status.errorMessage.empty()) {
					strncpy_s(result->errorMessage, outInfo.status.errorMessage.c_str(), sizeof(result->errorMessage) - 1);
				}


				// 图像数据
				if (!outInfo.images.outputImg.mat().empty()) {
					result->imgOut = outInfo.images.outputImg.mat().clone();
				}

				// 目标详情
				for (const auto& defect : outInfo.locate.details) {
					FinsObject finsObj;
					finsObj.box = defect.box;
					finsObj.confidence = defect.confidence;
					finsObj.className = defect.className;
					result->locates.push_back(finsObj);
				}
			}
		}
		else {
			// 超时处理
			control->timed_out = true;
			Log::WriteAsyncLog("算法执行超时!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);

			if (result) {
				result->statusCode = LABELALL_RETURN_TIMEOUT;
				strncpy_s(result->errorMessage, "算法执行超时", sizeof(result->errorMessage) - 1);

				// 克隆输入图像作为输出
				result->imgOut = img.clone();

				// 在输出图像上标记超时
				if (!result->imgOut.empty()) {
					putTextZH(result->imgOut,
						"算法执行超时",
						cv::Point(50, 50),
						Colors::RED, 55, FW_BOLD);
				}
			}
			rv = LABELALL_RETURN_TIMEOUT;
		}
	}
	catch (const std::exception& e) {
		delete COM;
		Log::WriteAsyncLog(std::string("CR_DLL_InspLabelAll exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
		rv = LABELALL_RETURN_ALGO_ERR;
	}

	// 清理资源
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		delete pInspLabelAll;
		delete COM;
		pInspLabelAll = nullptr;
	}

	// 记录处理时间
	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;
	}

	return rv;
}



extern "C" __declspec(dllexport) int CR_DLL_InspLabelAllMulty(
	std::vector<cv::Mat>&imgs, // 改为接收多张图像
	int cameraId,
	int jobId,
	const char* configPath,
	bool loadConfig,
	int timeOut,
	InspLabelAllResult * result)
{
	timeOut = MAX(100, timeOut);

	int rv = LABELALL_RETURN_OK;
	auto start = std::chrono::high_resolution_clock::now();
	Common* COM = new Common;

	// 检查输入图像数量是否为9
	int num = imgs.size();
	if (imgs.empty()) {
		Log::WriteAsyncLog("DLL_InspLabelAll: 图像数量为0!!!!", ERR, "D://aoi_error_log.txt", true);
		delete COM;
		return LABELALL_RETURN_INPUT_PARA_ERR;
	}
	if (imgs.size() != 9) {
		Log::WriteAsyncLog("DLL_InspLabelAll: 图像数量为 ", ERR,  "D://aoi_error_log.txt", true, imgs.size(), " 张!");
		delete COM;
		return LABELALL_RETURN_INPUT_PARA_ERR;
	}

	// 存储每个图像的处理结果
	std::vector<InspLabelAllResult> individualResults(num);
	std::vector<int> returnCodes(num, LABELALL_RETURN_OK);
	std::vector<std::future<int>> futures;

	// 并行处理所有9张图像
	for (int i = 0; i < num; i++) {
		futures.push_back(std::async(std::launch::async, [&, i]() {
			// 为每个图像创建独立的配置和结果对象
			InspLabelAllResult singleResult;
			const char* singleConfigPath = configPath; // 可根据需要为每个相机调整

			// 调用单图像处理函数（需要实现或调整现有函数）
			int singleRv = CR_DLL_InspLabelAll(
				imgs[i],
				cameraId,
				jobId,
				singleConfigPath,
				loadConfig,
				timeOut,
				&singleResult
			);

			individualResults[i] = singleResult;
			return singleRv;
			}));
	}

	// 等待所有处理完成
	for (int i = 0; i < num; i++) {
		returnCodes[i] = futures[i].get();
	}

	// 综合判断逻辑（按优先级排序）
	if (std::any_of(returnCodes.begin(), returnCodes.end(), [](int code) {
		return code == 16043; // 读码-一维码信息错误
		})) {
		rv = 16043;
	}
	else if (std::all_of(returnCodes.begin(), returnCodes.end(), [](int code) {
		return code == 16020; // 全部模板匹配-匹配失败
		})) {
		rv = 16020;
	}
	else if (std::any_of(returnCodes.begin(), returnCodes.end(), [](int code) {
		return code == 16022; // 任意模板匹配-歪斜
		})) {
		rv = 16022;
	}
	else if (std::any_of(returnCodes.begin(), returnCodes.end(), [](int code) {
		return code == 16021; // 任意模板匹配-错误特征
		})) {
		rv = 16021;
	}
	else if (std::any_of(returnCodes.begin(), returnCodes.end(), [](int code) {
		return code == 1; // 至少一个OK
		})) {
		rv = 1;
	}
	else {
		rv = LABELALL_RETURN_ALGO_ERR; // 默认错误
	}

	// 填充最终结果（这里需要根据你的需求确定如何整合多个结果）
	if (result) {
		// 初始化一个标志，用于记录是否找到匹配的结果
		bool foundMatch = false;

		// 遍历所有单个图像的处理结果
		for (int i = 0; i < num; i++) {
			// 找到状态码与综合判断结果rv一致的那个结果
			if (individualResults[i].statusCode == rv) {
				// 复制该结果到最终输出
				*result = individualResults[i];
				foundMatch = true;
				break; // 找到第一个匹配的即可退出循环
			}
		}

		// 如果没有找到完全匹配的结果（理论上应该不会发生，但安全起见）
		if (!foundMatch) {
			// 可以选择第一个结果，但覆盖其状态码为综合状态码
			*result = individualResults[num/2];
			result->statusCode = rv;
			// 你也可以根据需要记录日志或进行其他错误处理
		}
	}

	// 清理资源
	delete COM;

	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;
	}

	return rv;
}

extern "C" __declspec(dllexport) int CR_DLL_InspLabelAllMulty1(
	std::vector<cv::Mat>&imgs, // 接收多张图像
	int cameraId,
	int jobId,
	const char* configPath,
	bool loadConfig,
	int timeOut,
	std::vector<InspLabelAllResult>*results) // 改为返回每个图像的结果向量
{
	timeOut = MAX(100, timeOut);
	int rv = LABELALL_RETURN_OK;
	auto start = std::chrono::high_resolution_clock::now();
	Common* COM = new Common;

	// 检查输入图像数量是否为9
	const int num = imgs.size();
	if (num == 0) {
		Log::WriteAsyncLog("DLL_InspLabelAllMulty: 图像数量为0!!!!", ERR, "D://aoi_error_log.txt", true);
		delete COM;
		return LABELALL_RETURN_INPUT_PARA_ERR;
	}
	if (num != 9) {
		Log::WriteAsyncLog("DLL_InspLabelAllMulty: 图像数量为 " + std::to_string(num) + " 张!", ERR, "D://aoi_error_log.txt", true);
		delete COM;
		return LABELALL_RETURN_INPUT_PARA_ERR;
	}

	// 安全处理 results 指针
	std::vector<InspLabelAllResult> individualResults(num);
	results->reserve(num);
	std::vector<int> returnCodes(num, LABELALL_RETURN_OK);
	std::vector<std::future<int>> futures;

	// 并行处理所有9张图像
	for (int i = 0; i < num; i++) {
		futures.push_back(std::async(std::launch::async, [&, i]() {
			// 为每个图像创建独立的结果对象
			InspLabelAllResult singleResult;
			const char* singleConfigPath = configPath;

			// 调用单图像处理函数
			int singleRv = CR_DLL_InspLabelAll(
				imgs[i],
				cameraId,
				jobId,
				singleConfigPath,
				loadConfig,
				timeOut,
				&singleResult
			);

			individualResults[i] = singleResult;
			return singleRv;
			}));
	}

	// 等待所有处理完成并收集结果
	for (int i = 0; i < num; i++) {
		returnCodes[i] = futures[i].get();
	}

	// 安全地将结果复制到输出向量
	if (results) {
		try {
			*results = individualResults;
		}
		catch (const std::exception& e) {
			Log::WriteAsyncLog("复制结果时发生异常: " + std::string(e.what()), ERR, "D://aoi_error_log.txt", true);
			rv = LABELALL_RETURN_ALGO_ERR;
		}
	}

	// 综合判断逻辑（按优先级排序）
	if (std::any_of(returnCodes.begin(), returnCodes.end(), [](int code) {
		return code == 16043; // 读码-一维码信息错误
		})) {
		rv = 16043;
	}
	else if (std::all_of(returnCodes.begin(), returnCodes.end(), [](int code) {
		return code == 16020; // 全部模板匹配-匹配失败
		})) {
		rv = 16020;
	}
	else if (std::any_of(returnCodes.begin(), returnCodes.end(), [](int code) {
		return code == 16022; // 任意模板匹配-歪斜
		})) {
		rv = 16022;
	}
	else if (std::any_of(returnCodes.begin(), returnCodes.end(), [](int code) {
		return code == 16021; // 任意模板匹配-错误特征
		})) {
		rv = 16021;
	}
	else if (std::any_of(returnCodes.begin(), returnCodes.end(), [](int code) {
		return code == 1; // 至少一个OK
		})) {
		rv = 1;
	}
	else {
		rv = LABELALL_RETURN_ALGO_ERR; // 默认错误
	}

	// 清理资源
	delete COM;

	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;
	}

	return rv;
}


extern "C" __declspec(dllexport) int CR_DLL_CreatTemplate(
	cv::Mat img,
	TemplateConfig matchCfg,
	int cameraId,
	cv::Mat * imgOut)
{
	Log::WriteAsyncLog("开始创建模板！", ERR, "D://aoi_error_log.txt", true);
	Common* COM = new Common;
	AnalyseMat* ANA = new AnalyseMat;
	MatchFun* MF = new MatchFun;
	InspLabelAllOut outInfo;
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.system.cameraId = cameraId;

	char bufLog[100];
	sprintf(bufLog, "CreatTemplate/camera_%d/", outInfo.system.cameraId);
	outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog);
	outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + string(bufLog);
	outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

	outInfo.status.errorMessage = "OK";
	outInfo.status.statusCode = LABELALL_RETURN_OK;
	outInfo.status.logs.reserve(100);

	// 初始化线程局部上下文
	if (!tls_context) {
		tls_context = std::make_unique<CameraContext>();
		tls_context->cameraId = cameraId;
		tls_context->logger = new Log();
	}

	Log* LOG = tls_context->logger;

	if (img.empty()) {
		Log::WriteAsyncLog("CR_DLL_CreatTemplate: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
		return -1;
	}
	else if (cameraId < 0 || cameraId > 9) {
		Log::WriteAsyncLog("CR_DLL_CreatTemplate: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
		return -1;
	}
	if (img.channels() == 1)
	{
		Mat imgColor;
		cv::cvtColor(img, imgColor, COLOR_GRAY2BGR);
		*imgOut = imgColor.clone();
	}
	else if (img.channels() == 3)
	{
		*imgOut = img.clone();
	}
	else
	{
		Log::WriteAsyncLog("CR_DLL_CreatTemplate: channels != 1 || channels != 3", ERR, "D://aoi_error_log.txt", true);
		return -1;
	}

	try {
		Log::WriteAsyncLog("step1！", ERR, "D://aoi_error_log.txt", true);

		if (ANA->IsRectOutOfBounds(matchCfg.roi, img))
		{
			Log::WriteAsyncLog("CR_DLL_CreatTemplate: 模版roi超出图像范围!", ERR, "D://aoi_error_log.txt", true);
			return -3;
		}

		Mat templateRoi = img(matchCfg.roi).clone();
		Mat templateRoiGray;
		cv::cvtColor(templateRoi, templateRoiGray, COLOR_BGR2GRAY);

		Log::WriteAsyncLog("matchType:  ", ERR, "D://aoi_error_log.txt", matchCfg.matchType, true);
		switch (matchCfg.matchType) {
		case 0: // Halcon形状匹配
		case 1: // Halcon缩放形状匹配
			// 原有的Halcon处理代码
			try {
				// 读取图像
				HObject ho_Image;
				ANA->Mat2HObject(templateRoiGray, ho_Image);

				// 创建形状模型
				HTuple hv_ModelID;
				// 创建匹配模型
				switch (matchCfg.matchType) {
				case 0: // 形状匹配
					HalconCpp::CreateShapeModel(
						ho_Image,
						matchCfg.numLevels,
						matchCfg.angleRange[0],
						matchCfg.angleRange[1] - matchCfg.angleRange[0],
						matchCfg.angleStep,   // 角度步长
						HalconCpp::HTuple(MF->GetOptimizationString(matchCfg.optimization)),
						HalconCpp::HTuple(MF->GetMetricString(matchCfg.metric)),
						matchCfg.contrast,
						matchCfg.minContrast,
						&hv_ModelID);
					break;
				case 1: // 缩放匹配（修正参数）
					HalconCpp::CreateScaledShapeModel(
						ho_Image,
						matchCfg.numLevels,
						matchCfg.angleRange[0],
						matchCfg.angleRange[1] - matchCfg.angleRange[0],
						matchCfg.angleStep,
						matchCfg.scaleRange[0],
						matchCfg.scaleRange[1],
						matchCfg.scaleStep,
						HalconCpp::HTuple(MF->GetOptimizationString(matchCfg.optimization)),
						HalconCpp::HTuple(MF->GetMetricString(matchCfg.metric)),
						matchCfg.contrast,
						matchCfg.minContrast,
						&hv_ModelID);
					break;
				}

				// 获取模型轮廓
				HObject ho_ModelContours;
				HalconCpp::GetShapeModelContours(&ho_ModelContours, hv_ModelID, 1);

				double roiCenterX = matchCfg.roi.width / 2.0;
				double roiCenterY = matchCfg.roi.height / 2.0;
				HTuple hv_HomMat2D;
				HalconCpp::VectorAngleToRigid(0, 0, 0,
					roiCenterY,
					roiCenterX,
					0,
					&hv_HomMat2D);

				// 应用变换到轮廓
				HObject ho_TransContours;
				HalconCpp::AffineTransContourXld(ho_ModelContours, &ho_TransContours, hv_HomMat2D);

				// 检查轮廓对象
				if (!ho_TransContours.IsInitialized()) {
					throw std::runtime_error("轮廓对象未初始化");
				}

				// 获取轮廓对象数量
				HTuple hv_NumContours;
				HalconCpp::CountObj(ho_TransContours, &hv_NumContours);

				if (hv_NumContours[0].I() == 0) {
					throw std::runtime_error("轮廓对象为空");
				}

				// 遍历所有轮廓对象
				for (int contourIdx = 1; contourIdx <= hv_NumContours[0].I(); contourIdx++)
				{
					HObject ho_SingleContour;
					SelectObj(ho_TransContours, &ho_SingleContour, contourIdx);

					// 获取轮廓点坐标
					HTuple hv_Rows, hv_Cols;
					GetContourXld(ho_SingleContour, &hv_Rows, &hv_Cols);

					// 将轮廓点转换为OpenCV格式
					std::vector<cv::Point> currentContour;
					bool inContour = false;

					for (int k = 0; k < hv_Rows.Length(); k++)
					{
						double rowVal = hv_Rows[k].D();
						double colVal = hv_Cols[k].D();

						// 检查是否为有效数字
						if (std::isfinite(rowVal) && std::isfinite(colVal))
						{
							currentContour.push_back(cv::Point(
								static_cast<int>(colVal) + matchCfg.roi.x,
								static_cast<int>(rowVal) + matchCfg.roi.y
							));
							inContour = true;
						}
						else if (inContour)
						{
							// 遇到分隔点，结束当前轮廓
							if (!currentContour.empty())
							{
								if (currentContour.size() > 1)
								{
									cv::polylines(*imgOut, currentContour, false, Colors::GREEN, 2, cv::LINE_AA);
								}
								currentContour.clear();
							}
							inContour = false;
						}
					}

					// 绘制最后一个轮廓
					if (!currentContour.empty() && currentContour.size() > 1)
					{
						cv::polylines(*imgOut,
							currentContour,
							false,
							Colors::GREEN,
							2,
							cv::LINE_AA);
					}

					// 绘制roi
					rectangle(*imgOut, matchCfg.roi, Colors::YELLOW, 3, cv::LINE_AA);

					// 绘制中心
					cv::circle(*imgOut,
						cv::Point(matchCfg.templateCenterX, matchCfg.templateCenterY),
						7,
						Colors::BLUE, // 使用OK/NG颜色
						-1); // 半径为5的实心圆
				}

				imwrite(outInfo.paths.resultsOKDir + outInfo.system.startTime + ".jpg", *imgOut);
			}
			catch (HException& e) {
				imwrite(outInfo.paths.resultsOKDir + outInfo.system.startTime + ".jpg", img);
				LOG->WriteLog("Halcon模型创建失败: " + string(e.ErrorMessage().Text()),
					ERR, outInfo.paths.logFile, true);
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "模板模型创建失败!";
				return -2;
			}
			break;

		case 2: 
			return 1;
			break;
		

		case 3: // SIFT匹配
			try {
				// 创建SIFT检测器
				cv::Ptr<cv::SIFT> detector = cv::SIFT::create();

				// 提取SIFT特征
				std::vector<cv::KeyPoint> keypoints;
				cv::Mat descriptors;
				detector->detectAndCompute(templateRoiGray, cv::noArray(), keypoints, descriptors);

				

				// 绘制ROI矩形
				cv::rectangle(*imgOut, matchCfg.roi, Colors::BLUE, 3, cv::LINE_AA);

				// 绘制特征点
				cv::Mat keypointImg;
				cv::drawKeypoints(*imgOut, keypoints, keypointImg, Colors::GREEN,
					cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
				*imgOut = keypointImg;


				// 绘制特征点数量信息
				std::string infoText = "特征点数量: " + std::to_string(keypoints.size());
				cv::putText(*imgOut, infoText,
					cv::Point(matchCfg.roi.x, matchCfg.roi.y - 10),
					cv::FONT_HERSHEY_SIMPLEX, 0.7, Colors::BLUE, 2);
				if (keypoints.size() < 10)
				{
					LOG->WriteLog(infoText + "，特征点数量过少！", ERR, outInfo.paths.logFile, true);
					outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
					outInfo.status.errorMessage = "模板模型创建失败!";
					return -2;
				}
				else
				{
					LOG->WriteLog("模板创建成功，特征点数: ", INFO, outInfo.paths.logFile, true, descriptors.size());
				}
				
			}
			catch (const std::exception& e) {
				LOG->WriteLog("模板创建失败: " + std::string(e.what()),
					ERR, outInfo.paths.logFile, true);
				outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
				outInfo.status.errorMessage = "模板创建失败!";
				return -2;
			}
			break;

		default:
			LOG->WriteLog("未知的匹配类型: " + std::to_string(matchCfg.matchType),
				ERR, outInfo.paths.logFile, true);
			outInfo.status.statusCode = LABELALL_RETURN_CONFIG_ERR;
			outInfo.status.errorMessage = "未知的匹配类型!";
			return -2;
		}
	}
	catch (const std::exception& e) {
		imwrite(outInfo.paths.resultsOKDir + outInfo.system.startTime + ".jpg", img);
		Log::WriteAsyncLog(std::string("Algorithm exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
		return -2;
	}

	delete COM;
	delete ANA;
	delete MF;

	return 1;
}



extern "C" __declspec(dllexport) int CR_DLL_InspBottleNum(
	cv::Mat img,
	int cameraId,
	int jobId,
	const char* configPath,
	bool loadConfig,
	int timeOut,
	InspBottleNumResult * result)
{
	timeOut = MAX(100, timeOut);

	int rv = BOTTLENUM_RETURN_OK;
	auto start = std::chrono::high_resolution_clock::now();
	Common* COM = new Common;
	InspBottleNumOut outInfo;
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


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

	// 初始化线程局部上下文
	if (!tls_context) {
		tls_context = std::make_unique<CameraContext>();
		tls_context->cameraId = cameraId;
		tls_context->logger = new Log();
	}

	Log* LOG = tls_context->logger;

	if (img.empty()) {
		Log::WriteAsyncLog("DLL_InspBottleNum: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
		rv = BOTTLENUM_RETURN_INPUT_PARA_ERR;
	}
	else if (img.channels() != 3) {
		Log::WriteAsyncLog("DLL_InspBottleNum: img.channels() != 3", ERR, "D://aoi_error_log.txt", true);
		rv = BOTTLENUM_RETURN_INPUT_PARA_ERR;
	}
	else if (jobId < 0) {
		Log::WriteAsyncLog("DLL_InspBottleNum: jobId < 0", ERR, "D://aoi_error_log.txt", true);
		rv = BOTTLENUM_RETURN_INPUT_PARA_ERR;
	}
	else if (cameraId < 0 || cameraId > 9) {
		Log::WriteAsyncLog("DLL_InspBottleNum: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
		rv = BOTTLENUM_RETURN_INPUT_PARA_ERR;
	}



	// 创建超时控制对象
	auto control = std::make_shared<TimeoutControl>();
	outInfo.images.outputImg.data = std::make_shared<cv::Mat>(img.clone());

	// 创建算法对象
	InspBottleNum* pInspBottleNum = nullptr;
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		pInspBottleNum = new InspBottleNum(std::string(configPath), img, cameraId, jobId, false, timeOut, outInfo);
		pInspBottleNum->SetTimeoutFlagRef(control->timed_out);
		pInspBottleNum->SetStartTimePoint(start);
	}

	try {
		// 启动异步任务执行算法
		auto future = std::async(std::launch::async, [&]() {
			try {
				int algoResult = pInspBottleNum->BottleNum_Main(outInfo);

				// 设置完成标志
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

		// 等待结果或超时
		std::unique_lock<std::mutex> lock(control->mutex);
		if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
			[control] { return control->completed.load(); }))
		{
			// 正常完成
			rv = future.get();

			// 填充结果对象
			if (result) {
				// 填充基础信息
				result->jobId = jobId;
				result->cameraId = cameraId;
				result->statusCode = rv;

				// 复制时间信息
				if (!outInfo.system.startTime.empty()) {
					strncpy_s(result->startTime, outInfo.system.startTime.c_str(), sizeof(result->startTime) - 1);
				}

				// 复制错误信息
				if (!outInfo.status.errorMessage.empty()) {
					strncpy_s(result->errorMessage, outInfo.status.errorMessage.c_str(), sizeof(result->errorMessage) - 1);
				}


				// 图像数据
				if (!outInfo.images.outputImg.mat().empty()) {
					result->imgOut = outInfo.images.outputImg.mat().clone();
				}


			}
		}
		else {
			// 超时处理
			control->timed_out = true;
			Log::WriteAsyncLog("算法执行超时!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);

			if (result) {
				result->statusCode = BOTTLENUM_RETURN_TIMEOUT;
				strncpy_s(result->errorMessage, "算法执行超时", sizeof(result->errorMessage) - 1);

				// 克隆输入图像作为输出
				result->imgOut = img.clone();

				// 在输出图像上标记超时
				if (!result->imgOut.empty()) {
					putTextZH(result->imgOut,
						"算法执行超时",
						cv::Point(50, 50),
						Colors::RED, 55, FW_BOLD);
				}
			}
			rv = BOTTLENUM_RETURN_TIMEOUT;
		}
	}
	catch (const std::exception& e) {
		delete COM;
		Log::WriteAsyncLog(std::string("CR_DLL_InspBottleNum exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
		rv = BOTTLENUM_RETURN_ALGO_ERR;
	}

	// 清理资源
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		delete pInspBottleNum;
		delete COM;
		pInspBottleNum = nullptr;
	}

	// 记录处理时间
	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;
	}

	return rv;
}

extern "C" __declspec(dllexport) int CR_DLL_InspSew(
	cv::Mat img,
	int cameraId,
	int jobId,
	const char* configPath,
	bool loadConfig,
	int timeOut,
	InspSewResult * result)
{
	timeOut = MAX(100, timeOut);

	int rv = BOTTLENUM_RETURN_OK;
	auto start = std::chrono::high_resolution_clock::now();
	Common* COM = new Common;
	InspSewOut outInfo;
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


	char bufLog[100];
	sprintf(bufLog, "Sew/camera_%d/", outInfo.system.cameraId);
	char bufConfig[100];
	sprintf(bufConfig, "/InspSewConfig_%d.txt", outInfo.system.cameraId);
	outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
	outInfo.paths.intermediateImagesDir =
		ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
	outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
	outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
	outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
	outInfo.paths.configFile = configPath + std::string(bufConfig);
	outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

	outInfo.status.errorMessage = "OK";
	outInfo.status.statusCode = SEW_RETURN_OK;
	outInfo.status.logs.reserve(100);

	// 初始化线程局部上下文
	if (!tls_context) {
		tls_context = std::make_unique<CameraContext>();
		tls_context->cameraId = cameraId;
		tls_context->logger = new Log();
	}

	Log* LOG = tls_context->logger;

	if (img.empty()) {
		Log::WriteAsyncLog("DLL_InspSew: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
		rv = SEW_RETURN_INPUT_PARA_ERR;
	}
	else if (jobId < 0) {
		Log::WriteAsyncLog("DLL_InspSew: jobId < 0", ERR, "D://aoi_error_log.txt", true);
		rv = SEW_RETURN_INPUT_PARA_ERR;
	}
	else if (cameraId < 0 || cameraId > 9) {
		Log::WriteAsyncLog("DLL_InspSew: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
		rv = SEW_RETURN_INPUT_PARA_ERR;
	}



	// 创建超时控制对象
	auto control = std::make_shared<TimeoutControl>();
	outInfo.images.outputImg = img.clone();

	// 创建算法对象
	InspSew* pInspSew = nullptr;
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		pInspSew = new InspSew(std::string(configPath), img, cameraId, jobId, false, timeOut, outInfo);
		pInspSew->SetTimeoutFlagRef(control->timed_out);
		pInspSew->SetStartTimePoint(start);
	}

	try {
		// 启动异步任务执行算法
		auto future = std::async(std::launch::async, [&]() {
			try {
				int algoResult = pInspSew->Sew_Main(outInfo);

				// 设置完成标志
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

		// 等待结果或超时
		std::unique_lock<std::mutex> lock(control->mutex);
		if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
			[control] { return control->completed.load(); }))
		{
			// 正常完成
			rv = future.get();

			// 填充结果对象
			if (result) {
				// 填充基础信息
				result->jobId = jobId;
				result->cameraId = cameraId;
				result->statusCode = rv;

				// 复制时间信息
				if (!outInfo.system.startTime.empty()) {
					strncpy_s(result->startTime, outInfo.system.startTime.c_str(), sizeof(result->startTime) - 1);
				}

				// 复制错误信息
				if (!outInfo.status.errorMessage.empty()) {
					strncpy_s(result->errorMessage, outInfo.status.errorMessage.c_str(), sizeof(result->errorMessage) - 1);
				}


				// 图像数据
				if (!outInfo.images.outputImg.empty()) {
					result->imgOut = outInfo.images.outputImg.clone();
				}


			}
		}
		else {
			// 超时处理
			control->timed_out = true;
			Log::WriteAsyncLog("算法执行超时!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);

			if (result) {
				result->statusCode = SEW_RETURN_TIMEOUT;
				strncpy_s(result->errorMessage, "算法执行超时", sizeof(result->errorMessage) - 1);

				// 克隆输入图像作为输出
				result->imgOut = img.clone();

				// 在输出图像上标记超时
				if (!result->imgOut.empty()) {
					putTextZH(result->imgOut,
						"算法执行超时",
						cv::Point(50, 50),
						Colors::RED, 55, FW_BOLD);
				}
			}
			rv = SEW_RETURN_TIMEOUT;
		}
	}
	catch (const std::exception& e) {
		delete COM;
		Log::WriteAsyncLog(std::string("CR_DLL_InspSew exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
		rv = SEW_RETURN_ALGO_ERR;
	}

	// 清理资源
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		delete pInspSew;
		delete COM;
		pInspSew = nullptr;
	}

	// 记录处理时间
	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;
	}

	return rv;
}


extern "C" __declspec(dllexport) int CR_DLL_InspBoxBag(
	cv::Mat img,
	int cameraId,
	int jobId,
	const char* configPath,
	bool loadConfig,
	int timeOut,
	InspBoxBagResult * result)
{
	timeOut = MAX(100, timeOut);

	int rv = BOTTLENUM_RETURN_OK;
	auto start = std::chrono::high_resolution_clock::now();
	Common* COM = new Common;
	InspBoxBagOut outInfo;
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


	char bufLog[100];
	sprintf(bufLog, "BoxBag/camera_%d/", outInfo.system.cameraId);
	char bufConfig[100];
	sprintf(bufConfig, "/InspBoxBagConfig_%d.txt", outInfo.system.cameraId);
	outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
	outInfo.paths.intermediateImagesDir =
		ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
	outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
	outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
	outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
	outInfo.paths.configFile = configPath + std::string(bufConfig);
	outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

	outInfo.status.errorMessage = "OK";
	outInfo.status.statusCode = BOXBAG_RETURN_OK;
	outInfo.status.logs.reserve(100);

	// 初始化线程局部上下文
	if (!tls_context) {
		tls_context = std::make_unique<CameraContext>();
		tls_context->cameraId = cameraId;
		tls_context->logger = new Log();
	}

	Log* LOG = tls_context->logger;

	if (img.empty()) {
		Log::WriteAsyncLog("DLL_InspBoxBag: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
		rv = BOXBAG_RETURN_INPUT_PARA_ERR;
	}
	else if (jobId < 0) {
		Log::WriteAsyncLog("DLL_InspBoxBag: jobId < 0", ERR, "D://aoi_error_log.txt", true);
		rv = BOXBAG_RETURN_INPUT_PARA_ERR;
	}
	else if (cameraId < 0 || cameraId > 9) {
		Log::WriteAsyncLog("DLL_InspBoxBag: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
		rv = BOXBAG_RETURN_INPUT_PARA_ERR;
	}



	// 创建超时控制对象
	auto control = std::make_shared<TimeoutControl>();
	outInfo.images.outputImg = img.clone();

	// 创建算法对象
	InspBoxBag* pInspBoxBag = nullptr;
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		pInspBoxBag = new InspBoxBag(std::string(configPath), img, cameraId, jobId, false, timeOut, outInfo);
		pInspBoxBag->SetTimeoutFlagRef(control->timed_out);
		pInspBoxBag->SetStartTimePoint(start);
	}

	try {
		// 启动异步任务执行算法
		auto future = std::async(std::launch::async, [&]() {
			try {
				int algoResult = pInspBoxBag->BoxBag_Main(outInfo);

				// 设置完成标志
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

		// 等待结果或超时
		std::unique_lock<std::mutex> lock(control->mutex);
		if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
			[control] { return control->completed.load(); }))
		{
			// 正常完成
			rv = future.get();

			// 填充结果对象
			if (result) {
				// 填充基础信息
				result->jobId = jobId;
				result->cameraId = cameraId;
				result->statusCode = rv;

				// 复制时间信息
				if (!outInfo.system.startTime.empty()) {
					strncpy_s(result->startTime, outInfo.system.startTime.c_str(), sizeof(result->startTime) - 1);
				}

				// 复制错误信息
				if (!outInfo.status.errorMessage.empty()) {
					strncpy_s(result->errorMessage, outInfo.status.errorMessage.c_str(), sizeof(result->errorMessage) - 1);
				}


				// 图像数据
				if (!outInfo.images.outputImg.empty()) {
					result->imgOut = outInfo.images.outputImg.clone();
				}


			}
		}
		else {
			// 超时处理
			control->timed_out = true;
			Log::WriteAsyncLog("算法执行超时!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);

			if (result) {
				result->statusCode = BOXBAG_RETURN_TIMEOUT;
				strncpy_s(result->errorMessage, "算法执行超时", sizeof(result->errorMessage) - 1);

				// 克隆输入图像作为输出
				result->imgOut = img.clone();

				// 在输出图像上标记超时
				if (!result->imgOut.empty()) {
					putTextZH(result->imgOut,
						"算法执行超时",
						cv::Point(50, 50),
						Colors::RED, 55, FW_BOLD);
				}
			}
			rv = BOXBAG_RETURN_TIMEOUT;
		}
	}
	catch (const std::exception& e) {
		delete COM;
		Log::WriteAsyncLog(std::string("CR_DLL_InspBoxBag exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
		rv = BOXBAG_RETURN_ALGO_ERR;
	}

	// 清理资源
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		delete pInspBoxBag;
		delete COM;
		pInspBoxBag = nullptr;
	}

	// 记录处理时间
	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;
	}

	return rv;
}


extern "C" __declspec(dllexport) int CR_DLL_InspHeatSeal(
	cv::Mat img,
	int cameraId,
	int jobId,
	const char* configPath,
	bool loadConfig,
	int timeOut,
	InspHeatSealResult * result)
{
	timeOut = MAX(100, timeOut);

	int rv = BOTTLENUM_RETURN_OK;
	auto start = std::chrono::high_resolution_clock::now();
	Common* COM = new Common;
	InspHeatSealOut outInfo;
	outInfo.system.startTime = COM->time_t2string_with_ms();
	outInfo.system.jobId = jobId;
	outInfo.system.cameraId = cameraId;
	std::cout << "cameraId_" << outInfo.system.cameraId << "  m_jobId_" << outInfo.system.jobId << std::endl;


	char bufLog[100];
	sprintf(bufLog, "HeatSeal/camera_%d/", outInfo.system.cameraId);
	char bufConfig[100];
	sprintf(bufConfig, "/InspHeatSealConfig_%d.txt", outInfo.system.cameraId);
	outInfo.paths.logDirectory = ProjectConstants::LOG_PATH + std::string(bufLog);
	outInfo.paths.intermediateImagesDir =
		ProjectConstants::LOG_PATH + std::string(bufLog) + "IMG/" + std::to_string(outInfo.system.jobId) + "/";
	outInfo.paths.resultsOKDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "OK/";
	outInfo.paths.resultsNGDir = ProjectConstants::LOG_PATH + std::string(bufLog) + "NG/";
	outInfo.paths.trainDir = ProjectConstants::TRAIN_PATH + std::string(bufLog);
	outInfo.paths.configFile = configPath + std::string(bufConfig);
	outInfo.paths.logFile = outInfo.paths.logDirectory + "log_" + g_logSysTime_YMD + ".txt";

	outInfo.status.errorMessage = "OK";
	outInfo.status.statusCode = HEATSEAL_RETURN_OK;
	outInfo.status.logs.reserve(100);

	// 初始化线程局部上下文
	if (!tls_context) {
		tls_context = std::make_unique<CameraContext>();
		tls_context->cameraId = cameraId;
		tls_context->logger = new Log();
	}

	Log* LOG = tls_context->logger;

	if (img.empty()) {
		Log::WriteAsyncLog("DLL_InspHeatSeal: 输入图像为空!", ERR, "D://aoi_error_log.txt", true);
		rv = HEATSEAL_RETURN_INPUT_PARA_ERR;
	}
	else if (jobId < 0) {
		Log::WriteAsyncLog("DLL_InspHeatSeal: jobId < 0", ERR, "D://aoi_error_log.txt", true);
		rv = HEATSEAL_RETURN_INPUT_PARA_ERR;
	}
	else if (cameraId < 0 || cameraId > 9) {
		Log::WriteAsyncLog("DLL_InspHeatSeal: cameraId < 0 || cameraId > 9", ERR, "D://aoi_error_log.txt", true);
		rv = HEATSEAL_RETURN_INPUT_PARA_ERR;
	}



	// 创建超时控制对象
	auto control = std::make_shared<TimeoutControl>();
	outInfo.images.outputImg = img.clone();

	// 创建算法对象
	InspHeatSeal* pInspHeatSeal = nullptr;
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		pInspHeatSeal = new InspHeatSeal(std::string(configPath), img, cameraId, jobId, false, timeOut, outInfo);
		pInspHeatSeal->SetTimeoutFlagRef(control->timed_out);
		pInspHeatSeal->SetStartTimePoint(start);
	}

	try {
		// 启动异步任务执行算法
		auto future = std::async(std::launch::async, [&]() {
			try {
				int algoResult = pInspHeatSeal->HeatSeal_Main(outInfo);

				// 设置完成标志
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

		// 等待结果或超时
		std::unique_lock<std::mutex> lock(control->mutex);
		if (control->cv.wait_for(lock, std::chrono::milliseconds(timeOut),
			[control] { return control->completed.load(); }))
		{
			// 正常完成
			rv = future.get();

			// 填充结果对象
			if (result) {
				// 填充基础信息
				result->jobId = jobId;
				result->cameraId = cameraId;
				result->statusCode = rv;

				// 复制时间信息
				if (!outInfo.system.startTime.empty()) {
					strncpy_s(result->startTime, outInfo.system.startTime.c_str(), sizeof(result->startTime) - 1);
				}

				// 复制错误信息
				if (!outInfo.status.errorMessage.empty()) {
					strncpy_s(result->errorMessage, outInfo.status.errorMessage.c_str(), sizeof(result->errorMessage) - 1);
				}


				// 图像数据
				if (!outInfo.images.outputImg.empty()) {
					result->imgOut = outInfo.images.outputImg.clone();
				}


			}
		}
		else {
			// 超时处理
			control->timed_out = true;
			Log::WriteAsyncLog("算法执行超时!", ERR, "D://aoi_error_log.txt", true, "jobId = ", jobId);

			if (result) {
				result->statusCode = HEATSEAL_RETURN_TIMEOUT;
				strncpy_s(result->errorMessage, "算法执行超时", sizeof(result->errorMessage) - 1);

				// 克隆输入图像作为输出
				result->imgOut = img.clone();

				// 在输出图像上标记超时
				if (!result->imgOut.empty()) {
					putTextZH(result->imgOut,
						"算法执行超时",
						cv::Point(50, 50),
						Colors::RED, 55, FW_BOLD);
				}
			}
			rv = HEATSEAL_RETURN_TIMEOUT;
		}
	}
	catch (const std::exception& e) {
		delete COM;
		Log::WriteAsyncLog(std::string("CR_DLL_InspHeatSeal exception: ") + e.what(), ERR, "D://aoi_error_log.txt", true);
		rv = HEATSEAL_RETURN_ALGO_ERR;
	}

	// 清理资源
	{
		std::lock_guard<std::mutex> lock(global_resource_mutex);
		delete pInspHeatSeal;
		delete COM;
		pInspHeatSeal = nullptr;
	}

	// 记录处理时间
	auto end = std::chrono::high_resolution_clock::now();
	double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	{
		std::lock_guard<std::mutex> lock(g_durationMutex);
		g_cameraDurations[cameraId] = duration;
	}

	return rv;
}



