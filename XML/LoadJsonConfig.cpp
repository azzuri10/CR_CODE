#include "LoadJsonConfig.h"
#include "write_json.h"
#include <fstream>
#include <codecvt>
#include <locale>
#include <sstream>
#include <type_traits> // 添加类型特性支持

enum class LoadResult {
	SUCCESS = 1,
	FILE_ERROR = -1,
	JSON_ERROR = -2,
	EMPTY_CONFIG = -3
};

// UTF-8 转宽字符串
wstring utf8_to_wstring(const string& str) {
	if (str.empty()) return L"";
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
	wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
	return wstr;
}

// 宽字符串转 UTF-8
string wstring_to_utf8(const wstring& wstr) {
	if (wstr.empty()) return "";
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
	string str(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
	return str;
}

int LoadConfigMatch(
	const std::string& matchConfigFile,
	std::vector<MatchConfig>& details,
	const std::string& logFileName)
{
	Common COM;

	auto SafeLog = [&](const std::string& msg, LogRank level, bool console) {
		Log::WriteAsyncLog(msg, level, logFileName, console);
	};

	auto FileError = [&](const std::string& path) {
		SafeLog("无法打开文件: " + path, ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	};

	std::ifstream ifs(matchConfigFile, std::ios::binary | std::ios::ate);
	if (!ifs) return FileError(matchConfigFile);

	try {
		details.clear();

		const size_t fileSize = ifs.tellg();
		ifs.seekg(0);
		std::string fileContent(fileSize, '\0');
		ifs.read(&fileContent[0], fileSize);
		ifs.close();

		// 修复路径字符串中的反斜杠问题 (适配新字段名)
		auto fixPathString = [](std::string& jsonStr) {
			size_t pos = 0;
			while ((pos = jsonStr.find("templatePaths", pos)) != std::string::npos) {
				size_t arrayStart = jsonStr.find('[', pos + 13);
				if (arrayStart == std::string::npos) break;

				size_t arrayEnd = jsonStr.find(']', arrayStart);
				if (arrayEnd == std::string::npos) break;

				// 处理数组内的每个路径
				size_t strStart = arrayStart;
				while ((strStart = jsonStr.find('\"', strStart + 1)) != std::string::npos && strStart < arrayEnd) {
					size_t strEnd = jsonStr.find('\"', strStart + 1);
					if (strEnd == std::string::npos) break;

					std::string path = jsonStr.substr(strStart + 1, strEnd - strStart - 1);
					std::replace(path.begin(), path.end(), '\\', '/');
					jsonStr.replace(strStart + 1, strEnd - strStart - 1, path);
					strStart = strEnd + path.length() - (strEnd - strStart - 1);
				}
				pos = arrayEnd;
			}
		};

		fixPathString(fileContent);

		auto data = nlohmann::json::parse(fileContent);

		if (!data.contains("targets") || !data["targets"].is_array()) {
			throw std::runtime_error("JSON结构缺少'targets'数组");
		}

		auto parse_field = [](const auto& item, const std::string& jsonField, auto& targetVar, auto defaultValue) {
			if (item.contains(jsonField)) {
				try {
					targetVar = item[jsonField].get<std::decay_t<decltype(targetVar)>>();
				}
				catch (...) {
					targetVar = defaultValue;
				}
			}
			else {
				targetVar = defaultValue;
			}
		};

		auto parse_range = [](const auto& item, const std::string& key, auto& vec, const auto& defaultRange) {
			if (item.contains(key)) {
				auto& jval = item[key];
				if (!jval.is_array() || jval.size() != 2) {
					throw std::runtime_error(key + "格式错误，应为2元素数组");
				}

				// 确保容器有足够空间
				if (vec.size() < 2) vec.resize(2);

				// 直接赋值元素
				vec[0] = jval[0].get<typename std::decay_t<decltype(vec)>::value_type>();
				vec[1] = jval[1].get<typename std::decay_t<decltype(vec)>::value_type>();
			}
			else {
				vec = defaultRange;  // 已知类型相同，安全赋值
			}
		};

		const auto& targets = data["targets"];
		for (size_t i = 0; i < targets.size(); ++i) {
			try {
				const auto& item = targets[i];
				MatchConfig cfg;
				std::ostringstream itemPrefix;
				itemPrefix << "配置项[" << (i + 1) << "]";

				// 解析模板路径数组
				if (item.contains("templatePaths") && item["templatePaths"].is_array()) {
					for (const auto& path : item["templatePaths"]) {
						std::string utf8Path = path.get<std::string>();
						std::replace(utf8Path.begin(), utf8Path.end(), '\\', '/');
						cfg.templatePaths.push_back(COM.UTF8ToGBK(utf8Path));
					}
				}
				else {
					throw std::runtime_error("缺少必需字段 'templatePaths'");
				}

				// 解析ROI数组
				if (item.contains("roi") && item["roi"].is_array()) {
					for (const auto& roiItem : item["roi"]) {
						cfg.rois.push_back(cv::Rect(
							roiItem.value("x", 0),
							roiItem.value("y", 0),
							std::max(1, roiItem.value("w", 0)),
							std::max(1, roiItem.value("h", 0))
						));
					}
				}
				else {
					throw std::runtime_error("缺少'roi'字段");
				}

				// 检查路径和ROI数量匹配
				if (cfg.templatePaths.size() != cfg.rois.size()) {
					SafeLog("templatePaths与roi元素数量不匹配", INFO, true);
					return static_cast<int>(LoadResult::JSON_ERROR);
				}

				// 解析其他共享参数
				parse_field(item, "templateType", cfg.templateType, false);
				parse_field(item, "templateCenterX", cfg.templateCenterX, 0);
				parse_field(item, "templateCenterY", cfg.templateCenterY, 0);
				parse_field(item, "shiftHor", cfg.shiftHor, 0);
				parse_field(item, "shiftVer", cfg.shiftVer, 0);
				parse_field(item, "offset", cfg.offset, 0);
				parse_field(item, "extW", cfg.extW, 0);
				parse_field(item, "extH", cfg.extH, 0);
				parse_field(item, "channels", cfg.channels, 1);
				parse_field(item, "optimization", cfg.optimization, 1);
				parse_field(item, "angleStep", cfg.angleStep, 0);
				parse_field(item, "scaleStep", cfg.scaleStep, 0);
				parse_field(item, "matchType", cfg.matchType, 0);
				parse_field(item, "metric", cfg.metric, 0);
				parse_field(item, "numLevels", cfg.numLevels, 0);
				parse_field(item, "polarity", cfg.polarity, 0);
				parse_field(item, "contrast", cfg.contrast, 30);
				parse_field(item, "minContrast", cfg.minContrast, 8);
				parse_field(item, "subPixel", cfg.subPixel, 0);
				parse_field(item, "greediness", cfg.greediness, 0.8);
				parse_field(item, "numMatches", cfg.numMatches, 1);
				parse_field(item, "maxOverLap", cfg.maxOverlap, 0.0);
				parse_field(item, "scoreThreshold", cfg.scoreThreshold, 0.85);

				parse_range(item, "angleRange", cfg.angleRange, std::vector<double>{-10.0, 10.0});
				parse_range(item, "scaleRange", cfg.scaleRange, std::vector<double>{0.9, 1.1});
				parse_range(item, "angleThreshold", cfg.angleThreshold, std::vector<double>{-5.0, 5.0});
				parse_field(item, "warp", cfg.warp, 0);

				// 解析warpRange
				if (item.contains("warpRange") && item["warpRange"].is_array()) {  // 改为使用item
					cfg.warpRange.clear();
					for (const auto& warpItem : item["warpRange"]) {
						if (warpItem.is_object()) {
							std::vector<double> warpValues;

							// 安全提取各个参数，提供默认值
							warpValues.push_back(warpItem.value("h0", 1.0));
							warpValues.push_back(warpItem.value("h1", 1.0));
							warpValues.push_back(warpItem.value("w0", 1.0));
							warpValues.push_back(warpItem.value("w1", 1.0));

							cfg.warpRange.push_back(warpValues);
						}
					}
				}
				else {
					// 设置默认warpRange
					cfg.warpRange = { {0.95, 1.0, 0.7, 0.7}, {0.95, 1.0, 0.8, 0.8} };
				}
				details.push_back(std::move(cfg));
			}
			catch (const std::exception& e) {
				std::ostringstream errMsg;
				errMsg << "配置项[" << (i + 1) << "] 错误: " << e.what();
				SafeLog(errMsg.str(), ERR, true);
			}
		}

		if (details.empty()) {
			SafeLog("没有有效的模板配置", ERR, true);
			return static_cast<int>(LoadResult::EMPTY_CONFIG);
		}

		std::ostringstream successMsg;
		successMsg << "成功加载 " << details.size() << " 个模板组，共包含 ";
		size_t totalTemplates = 0;
		for (const auto& cfg : details) totalTemplates += cfg.templatePaths.size();
		successMsg << totalTemplates << " 个模板";
		SafeLog(successMsg.str(), INFO, true);

		return static_cast<int>(LoadResult::SUCCESS);
	}
	catch (const nlohmann::json::exception& e) {
		SafeLog(std::string("JSON解析错误: ") + e.what(), ERR, true);
		return static_cast<int>(LoadResult::JSON_ERROR);
	}
	catch (const std::exception& e) {
		SafeLog(std::string("配置处理异常: ") + e.what(), ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}
}

int LoadConfigMatchLocate(
	const std::string& matchConfigFile,
	MatchLocateConfig& details,
	const std::string& logFileName)
{
	Common COM;

	auto SafeLog = [&](const std::string& msg, LogRank level, bool console) {
		Log::WriteAsyncLog(msg, level, logFileName, console);
	};

	auto FileError = [&](const std::string& path) {
		SafeLog("无法打开文件: " + path, ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	};

	std::ifstream ifs(matchConfigFile, std::ios::binary);
	if (!ifs.is_open()) {
		return FileError(matchConfigFile);
	}

	try {
		// 清空现有配置
		details = MatchLocateConfig();

		// 读取文件内容
		std::string fileContent((std::istreambuf_iterator<char>(ifs)),
			std::istreambuf_iterator<char>());
		ifs.close();

		// 修复路径字符串中的反斜杠问题
		auto fixPathString = [](std::string& jsonStr) {
			size_t pos = 0;
			while ((pos = jsonStr.find("templateImgPath", pos)) != std::string::npos) {
				size_t strStart = jsonStr.find('\"', pos + 15);
				if (strStart == std::string::npos) break;

				size_t strEnd = jsonStr.find('\"', strStart + 1);
				if (strEnd == std::string::npos) break;

				std::string path = jsonStr.substr(strStart + 1, strEnd - strStart - 1);
				std::replace(path.begin(), path.end(), '\\', '/');
				jsonStr.replace(strStart + 1, strEnd - strStart - 1, path);
				pos = strEnd + 1;
			}
		};

		fixPathString(fileContent);

		// 解析JSON
		auto data = nlohmann::json::parse(fileContent);

		// 辅助函数：安全解析字段
		auto parse_field = [](const auto& item, const std::string& jsonField, auto& targetVar, auto defaultValue) {
			if (item.contains(jsonField) && !item[jsonField].is_null()) {
				try {
					targetVar = item[jsonField].get<std::decay_t<decltype(targetVar)>>();
				}
				catch (...) {
					targetVar = defaultValue;
				}
			}
			else {
				targetVar = defaultValue;
			}
		};

		// 辅助函数：解析范围数组
		auto parse_range = [](const auto& item, const std::string& key, std::vector<double>& vec, const std::vector<double>& defaultRange) {
			if (item.contains(key) && item[key].is_array() && item[key].size() == 2) {
				vec.resize(2);
				vec[0] = item[key][0].get<double>();
				vec[1] = item[key][1].get<double>();
			}
			else {
				vec = defaultRange;
			}
		};

		// 解析模板路径
		if (data.contains("templateImgPath")) {
			std::string utf8Path = data["templateImgPath"].get<std::string>();
			std::replace(utf8Path.begin(), utf8Path.end(), '\\', '/');
			details.templatePath = COM.UTF8ToGBK(utf8Path);
		}
		else {
			throw std::runtime_error("缺少必需字段 'templateImgPath'");
		}

		// 解析模板姿态
		if (data.contains("templatePose")) {
			auto& poseItem = data["templatePose"];
			details.templatePose = cv::Rect(
				poseItem.value("x", 0),
				poseItem.value("y", 0),
				std::max(1, poseItem.value("w", 0)),
				std::max(1, poseItem.value("h", 0))
			);

			// 计算模板中心坐标
			details.templateCenterX = details.templatePose.x + details.templatePose.width / 2;
			details.templateCenterY = details.templatePose.y + details.templatePose.height / 2;
		}
		else {
			throw std::runtime_error("缺少必需字段 'templatePose'");
		}

		// 解析目标ROI
		if (data.contains("targetRoi")) {
			auto& roiItem = data["targetRoi"];
			details.targetRoi = cv::Rect(
				roiItem.value("x", 0),
				roiItem.value("y", 0),
				std::max(1, roiItem.value("w", 0)),
				std::max(1, roiItem.value("h", 0))
			);
		}
		else {
			// 如果没有提供targetRoi，使用templatePose作为默认值
			details.targetRoi = details.templatePose;
		}

		// 解析其他参数
		parse_field(data, "extW", details.extW, 100);
		parse_field(data, "extH", details.extH, 100);
		parse_field(data, "shiftHor", details.shiftHor, 30);
		parse_field(data, "shiftVer", details.shiftVer, 100);
		parse_field(data, "offset", details.offset, 100);
		parse_field(data, "matchType", details.matchType, 1);
		parse_field(data, "numLevels", details.numLevels, 10);
		parse_field(data, "contrast", details.contrast, 40);
		parse_field(data, "minContrast", details.minContrast, 8);
		parse_field(data, "subPixel", details.subPixel, 1);
		parse_field(data, "greediness", details.greediness, 0.8);
		parse_field(data, "numMatches", details.numMatches, 1);
		parse_field(data, "maxOverLap", details.maxOverlap, 0.0);
		parse_field(data, "scoreThreshold", details.scoreThreshold, 0.65);
		parse_field(data, "angleStep", details.angleStep, 0.0);
		parse_field(data, "scaleStep", details.scaleStep, 0.0);
		parse_field(data, "optimization", details.optimization, 1);
		parse_field(data, "metric", details.metric, 1);

		// 设置默认值
		parse_field(data, "channels", details.channels, 1);     // 默认黑白
		parse_field(data, "polarity", details.polarity, 0);     // 默认值
		parse_field(data, "timeOut", details.timeOut, 0);       // 默认无超时

		// 解析范围参数
		parse_range(data, "angleRange", details.angleRange, { -25.0, 25.0 });
		parse_range(data, "scaleRange", details.scaleRange, { 0.9, 1.1 });
		parse_range(data, "angleThreshold", details.angleThreshold, { -5.0, 5.0 });



		SafeLog("成功加载定位配置", INFO, true);
		return static_cast<int>(LoadResult::SUCCESS);
	}
	catch (const nlohmann::json::exception& e) {
		SafeLog(std::string("JSON解析错误: ") + e.what(), ERR, true);
		return static_cast<int>(LoadResult::JSON_ERROR);
	}
	catch (const std::exception& e) {
		SafeLog(std::string("配置处理异常: ") + e.what(), ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}
}


int LoadConfigYOLO(
	const std::string detectConfigFile,
	std::vector<YoloConfig>& details,
	std::vector<std::string>& locateClassName,
	const std::string logFileName)
{
	Common COM;
	auto SafeLog = [&](const std::string& msg, LogRank level, bool console) {
		Log::WriteAsyncLog(msg, level, logFileName, console);
	};

	// 1. 验证文件路径
	if (detectConfigFile.empty()) {
		SafeLog("配置文件路径为空", ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}

	// 2. 打开文件
	std::ifstream ifs(detectConfigFile, std::ios::binary | std::ios::ate);
	if (!ifs) {
		SafeLog("无法打开文件: " + detectConfigFile, ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}

	// 3. 读取文件内容
	size_t fileSize = ifs.tellg();
	if (fileSize == 0) {
		SafeLog("配置文件为空: " + detectConfigFile, ERR, true);
		return static_cast<int>(LoadResult::EMPTY_CONFIG);
	}

	ifs.seekg(0);
	std::string fileContent(fileSize, '\0');
	ifs.read(&fileContent[0], fileSize);
	ifs.close();

	try {
		// 移除BOM
		if (fileSize >= 3 &&
			static_cast<unsigned char>(fileContent[0]) == 0xEF &&
			static_cast<unsigned char>(fileContent[1]) == 0xBB &&
			static_cast<unsigned char>(fileContent[2]) == 0xBF) {
			fileContent = fileContent.substr(3);
		}

		// 过滤控制字符
		auto removeControlChars = [](std::string& str) {
			str.erase(std::remove_if(str.begin(), str.end(), [](char c) {
				return (c >= 0 && c < 32) && c != '\n' && c != '\r' && c != '\t';
				}), str.end());
		};
		removeControlChars(fileContent);

		details.clear();
		locateClassName.clear();

		// 读取文件
		std::ifstream ifs(detectConfigFile);
		if (!ifs.is_open()) {
			SafeLog("无法打开文件: " + detectConfigFile, ERR, true);
			return static_cast<int>(LoadResult::FILE_ERROR);
		}

		// JSON解析
		auto data = nlohmann::json::parse(fileContent);
		ifs.close();

		// 验证JSON结构
		if (!data.contains("targets") || !data["targets"].is_array()) {
			throw std::runtime_error("JSON结构缺少'targets'数组");
		}

		const std::map<std::string, std::string> fieldMap = {
			{"widthRange", "widthRange"},
			{"heightRange", "heightRange"},
			{"areaRange", "areaRange"},
			{"aspectRange", "aspectRange"},
			{"countRange", "countRange"},
			{"angleRange", "angleRange"},
			{"maxOverlap", "maxOverLap"},
			{"scoreThreshold", "scoreThreshold"}
		};

		for (size_t i = 0; i < data["targets"].size(); ++i) {
			try {
				const auto& item = data["targets"][i];
				YoloConfig cfg;

				// 类型字段
				if (!item.contains("type") || !item["type"].is_string()) {
					throw std::runtime_error("缺少有效的'type'字段");
				}

#ifdef _WIN32
				cfg.className = COM.UTF8ToGBK(item["type"].get<std::string>());
#else
				cfg.className = item["type"].get<std::string>();
#endif

				// ROI处理
				if (item.contains("roi") && item["roi"].is_object()) {
					const auto& roi = item["roi"];
					cfg.roi = cv::Rect(
						roi.value("x", 0),
						roi.value("y", 0),
						std::max(1, roi.value("w", 0)),
						std::max(1, roi.value("h", 0))
					);
				}
				else {
					cfg.roi = cv::Rect(0, 0, 1280, 960); // 默认ROI
				}

				// 字段映射解析
				auto parse_field = [&](const std::string& jsonField, auto& targetVar) {
					std::string field = fieldMap.count(jsonField) ?
						fieldMap.at(jsonField) : jsonField;
					if (item.contains(jsonField)) {
						targetVar = item[jsonField].get<std::decay_t<decltype(targetVar)>>();
					}
				};

				// 解析所有字段
				parse_field("widthRange", cfg.widthRange);
				parse_field("heightRange", cfg.heightRange);
				parse_field("areaRange", cfg.areaRange);
				parse_field("aspectRange", cfg.aspectRange);
				parse_field("countRange", cfg.countRange);
				parse_field("angleRange", cfg.angleRange);
				parse_field("maxOverlap", cfg.maxOverLap);
				parse_field("scoreThreshold", cfg.confidenceThresh);

				// 处理use字段
				cfg.use = item.value("use", 1) != 0;

				// 参数验证
				if (cfg.widthRange[0] <= 0 || cfg.heightRange[0] <= 0) {
					throw std::runtime_error("尺寸范围必须大于0");
				}
				if (cfg.confidenceThresh < 0 || cfg.confidenceThresh > 1) {
					throw std::runtime_error("置信度必须大于0小于1");
				}
				if (cfg.areaRange[0] <= 0 || cfg.areaRange[1] <= cfg.areaRange[0]) {
					throw std::runtime_error("面积范围无效");
				}
				if (cfg.aspectRange[0] <= 0 || cfg.aspectRange[1] <= cfg.aspectRange[0]) {
					throw std::runtime_error("宽高比范围无效");
				}
				if (cfg.angleRange[0] > cfg.angleRange[1]) {
					throw std::runtime_error("角度范围无效");
				}

				details.push_back(cfg);
				if (cfg.use) {
					locateClassName.push_back(cfg.className);
				}
			}
			catch (const std::exception& e) {
				SafeLog("目标[" + std::to_string(i) + "]错误: " + e.what(), ERR, true);
			}
		}

		if (locateClassName.empty()) {
			SafeLog("未找到有效检测类别", ERR, true);
			return static_cast<int>(LoadResult::EMPTY_CONFIG);
		}

		SafeLog("成功加载" + std::to_string(locateClassName.size()) + "个类别", INFO, true);
		return static_cast<int>(LoadResult::SUCCESS);
	}
	catch (const nlohmann::json::exception& e) {
		// 详细错误日志
		std::ostringstream oss;
		oss << "JSON解析错误: " << e.what() << "\n"
			<< "文件: " << detectConfigFile << "\n"
			<< "错误ID: " << e.id << "\n";

		SafeLog(oss.str(), ERR, true);

		// 记录部分文件内容用于调试
		size_t debugSize = std::min(fileContent.size(), static_cast<size_t>(200));
		SafeLog("文件内容片段: " + fileContent.substr(0, debugSize), ERR, true);

		return static_cast<int>(LoadResult::JSON_ERROR);
	}
	catch (const std::exception& e) {
		SafeLog(std::string("配置处理异常: ") + e.what(), ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}
}


int LoadConfigBar(
	const std::string& configFile,
	std::vector<BarConfig>& barConfigs,
	const std::string& logFileName)
{

	Common COM; // 使用栈对象

	// 统一的日志处理函数
	auto SafeLog = [&](const std::string& msg, LogRank level, bool console) {
		Log::WriteAsyncLog(msg, level, logFileName, console);
	};

	auto FileError = [&](const std::string& path) {
		SafeLog("无法打开文件: " + path, ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	};

	std::ifstream ifs(configFile, std::ios::binary | std::ios::ate);
	if (!ifs) return FileError(configFile);

	try {
		barConfigs.clear();

		const size_t fileSize = ifs.tellg();
		ifs.seekg(0);
		std::string fileContent(fileSize, '\0');
		ifs.read(&fileContent[0], fileSize);
		ifs.close();

		// 修复路径字符串中的反斜杠问题
		auto fixPathString = [](std::string& jsonStr) {
			size_t pos = 0;
			while ((pos = jsonStr.find("modelPath", pos)) != std::string::npos) {
				size_t start = jsonStr.find('\"', pos + 10) + 1;
				if (start == std::string::npos) break;

				size_t end = jsonStr.find('\"', start);
				if (end == std::string::npos) break;

				std::string path = jsonStr.substr(start, end - start);
				std::replace(path.begin(), path.end(), '\\', '/'); // 关键修复

				jsonStr.replace(start, end - start, path);
				pos = end + path.length() - (end - start);
			}
		};

		fixPathString(fileContent);

		// JSON解析
		auto data = nlohmann::json::parse(fileContent);

		// 验证JSON结构
		if (!data.contains("targets") || !data["targets"].is_array()) {
			throw std::runtime_error("JSON结构缺少'targets'数组");
		}

		// 字段解析器（统一接口）
		auto parse_field = [](const auto& item, const std::string& jsonField, auto& targetVar, auto defaultValue) {
			if (item.contains(jsonField)) {
				try {
					targetVar = item[jsonField].get<std::decay_t<decltype(targetVar)>>();
				}
				catch (const nlohmann::json::exception&) {
					targetVar = defaultValue;
				}
			}
			else {
				targetVar = defaultValue;
			}
		};

		// 修复后的范围解析器 (直接操作容器元素)
		auto parse_range = [](const auto& item, const std::string& key, auto& vec, const auto& defaultRange) {
			if (item.contains(key)) {
				auto& jval = item[key];
				if (!jval.is_array() || jval.size() != 2) {
					throw std::runtime_error(key + "格式错误，应为2元素数组");
				}

				// 确保容器有足够空间
				if (vec.size() < 2) vec.resize(2);

				// 直接赋值元素
				vec[0] = jval[0].get<typename std::decay_t<decltype(vec)>::value_type>();
				vec[1] = jval[1].get<typename std::decay_t<decltype(vec)>::value_type>();
			}
			else {
				vec = defaultRange;  // 已知类型相同，安全赋值
			}
		};

		// 解析目标配置
		const auto& targets = data["targets"];
		for (size_t i = 0; i < targets.size(); ++i) {
			try {
				const auto& item = targets[i];
				BarConfig cfg;

				// 必需字段检查
				if (!item.contains("checkType")) {
					throw std::runtime_error("缺少必需字段 'checkType'");
				}
				if (!item.contains("roi")) {
					throw std::runtime_error("缺少必需字段 'roi'");
				}
				if (!item.contains("barType")) {
					throw std::runtime_error("缺少必需字段 'barType'");
				}

				// 解析基本字段
			   // cfg.checkType = item["checkType"].get<std::string>();

				cfg.checkType = COM.UTF8ToGBK(item["checkType"].get<std::string>());
				cfg.barType = item["barType"].get<std::string>();

				// 解析可选字段
				if (item.contains("info")) {
					cfg.info = item["info"].get<std::string>();
				}

				// 解析ROI
				const auto& roi = item["roi"];
				cfg.roi = cv::Rect(
					roi.value("x", 0),
					roi.value("y", 0),
					std::max(1, roi.value("w", 0)),
					std::max(1, roi.value("h", 0))
				);

				// 解析数量范围
				if (item.contains("countRange") && item["countRange"].is_array() && item["countRange"].size() == 2) {
					cfg.countRange = {
						item["countRange"][0].get<int>(),
						item["countRange"][1].get<int>()
					};
				}
				else {
					cfg.countRange = { 1, 1 }; // 默认范围
				}

				// 解析模型相关字段
				if (item.contains("checkModel")) {
					// 支持多种类型：bool, int, string
					if (item["checkModel"].is_boolean()) {
						cfg.checkModel = item["checkModel"].get<bool>();
					}
					else if (item["checkModel"].is_number()) {
						cfg.checkModel = item["checkModel"].get<int>() != 0;
					}
					else if (item["checkModel"].is_string()) {
						std::string val = item["checkModel"].get<std::string>();
						cfg.checkModel = (val == "true" || val == "1");
					}
				}
				else {
					cfg.checkModel = false; // 默认不检查模型
				}

				// 解析模型路径
				if (item.contains("modelPath")) {
					cfg.modelPath = COM.UTF8ToGBK(item["modelPath"].get<std::string>());
					// 修复路径分隔符
					std::replace(cfg.modelPath.begin(), cfg.modelPath.end(), '\\', '/');
				}
				else {
					cfg.modelPath = "";
				}

				// 成功日志
				SafeLog("成功加载配置项[" + std::to_string(i + 1) + "]: " + cfg.checkType + " - " + cfg.barType, INFO, true);
				// 添加配置
				barConfigs.push_back(std::move(cfg));

			}
			catch (const std::exception& e) {
				SafeLog("配置项[" + std::to_string(i + 1) + "] 错误: " + e.what(), INFO, true);
			}
		}

		// 结果检查
		if (barConfigs.empty()) {
			SafeLog("没有有效的一维码配置", INFO, true);
			return static_cast<int>(LoadResult::EMPTY_CONFIG);
		}

		// 成功日志
		SafeLog("成功加载 " + std::to_string(barConfigs.size()) + " 个一维码配置", INFO, true);
		return static_cast<int>(LoadResult::SUCCESS);
	}
	catch (const nlohmann::json::exception& e) {
		SafeLog("JSON解析错误: " + std::string(e.what()), INFO, true);
		return static_cast<int>(LoadResult::JSON_ERROR);
	}
	catch (const std::exception& e) {
		SafeLog("配置处理异常: " + std::string(e.what()), INFO, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}
}

int LoadConfigCodeBasic(
	const std::string codeBasicConfigFile,
	CodeBasic& details,
	const std::string logFileName)
{
	auto SafeLog = [&](const std::string& msg, LogRank level, bool console) {
		Log::WriteAsyncLog(msg, level, logFileName, console);
	};

	// 1. 验证文件路径
	if (codeBasicConfigFile.empty()) {
		SafeLog("配置文件路径为空", ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}

	// 2. 打开文件
	std::ifstream ifs(codeBasicConfigFile, std::ios::binary | std::ios::ate);
	if (!ifs) {
		SafeLog("无法打开文件: " + codeBasicConfigFile, ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}

	// 3. 读取文件内容
	size_t fileSize = ifs.tellg();
	if (fileSize == 0) {
		SafeLog("配置文件为空: " + codeBasicConfigFile, ERR, true);
		ifs.close();
		return static_cast<int>(LoadResult::EMPTY_CONFIG);
	}

	ifs.seekg(0);
	std::string fileContent(fileSize, '\0');
	ifs.read(&fileContent[0], fileSize);
	ifs.close();

	try {
		// 移除BOM
		if (fileSize >= 3 &&
			static_cast<unsigned char>(fileContent[0]) == 0xEF &&
			static_cast<unsigned char>(fileContent[1]) == 0xBB &&
			static_cast<unsigned char>(fileContent[2]) == 0xBF) {
			fileContent = fileContent.substr(3);
		}

		// 过滤控制字符
		auto removeControlChars = [](std::string& str) {
			str.erase(std::remove_if(str.begin(), str.end(), [](char c) {
				return (c >= 0 && c < 32) && c != '\n' && c != '\r' && c != '\t';
				}), str.end());
		};
		removeControlChars(fileContent);

		// JSON解析
		auto data = nlohmann::json::parse(fileContent);

		// 检查targets数组
		if (!data.contains("targets") || !data["targets"].is_array() || data["targets"].empty()) {
			throw std::runtime_error("'targets' must be a non-empty array");
		}

		const std::map<std::string, std::string> fieldMap = {
			{"charNumRange", "charNumRange"},
			{"codeWidthRange", "codeWidthRange"},
			{"codeHeightRange", "codeHeightRange"},
			{"codeAngleRange", "codeAngleRange"}
		};

		const auto& targets = data["targets"];
		const auto& item = targets[0];  // 取第一个目标配置
		CodeBasic cfg;

		// ROI处理
		if (item.contains("roi") && item["roi"].is_object()) {
			const auto& roi = item["roi"];
			cfg.roi = cv::Rect(
				roi.value("x", 0),
				roi.value("y", 0),
				std::max(1, roi.value("w", 0)),
				std::max(1, roi.value("h", 0))
			);
		}
		else {
			cfg.roi = cv::Rect(0, 0, 1280, 960); // 默认ROI
		}

		// 字段解析函数
		auto parse_field = [&](const std::string& jsonField, auto& targetVar) {
			std::string field = fieldMap.count(jsonField) ?
				fieldMap.at(jsonField) : jsonField;
			if (item.contains(jsonField)) {
				targetVar = item[jsonField].get<std::decay_t<decltype(targetVar)>>();
			}
		};

		// 解析所有字段
		parse_field("charNumRange", cfg.charNumRange);
		parse_field("codeWidthRange", cfg.codeWidthRange);
		parse_field("codeHeightRange", cfg.codeHeightRange);
		parse_field("codeAngleRange", cfg.codeAngleRange);

		// 解析其他参数
		cfg.rotateAngle = item.value("rotateAngle", 0);
		cfg.rotateCenter.x = item.value("rotateCenterX", 0);
		cfg.rotateCenter.y = item.value("rotateCenterY", 0);
		cfg.warp = item.value("warp", 0);
		cfg.warpL = item.value("warpL", 0.0);
		cfg.warpM = item.value("warpM", 0.0);
		cfg.warpR = item.value("warpR", 0.0);
		cfg.lineWordMinNum = item.value("lineCharMinNum", 2);
		cfg.charMaxDis = item.value("charMaxDis", 1000);
		cfg.lineDis = item.value("lineDis", 0);
		cfg.extW = item.value("extW", 20);
		cfg.extH = item.value("extH", 20);

		// 参数验证
		if (cfg.charNumRange.size() < 2 ||
			cfg.charNumRange[0] <= 0 ||
			cfg.charNumRange[1] < cfg.charNumRange[0]) {
			throw std::runtime_error("字符数量范围无效");
		}
		if (cfg.codeWidthRange.size() < 2 ||
			cfg.codeWidthRange[0] <= 0 ||
			cfg.codeWidthRange[1] <= cfg.codeWidthRange[0]) {
			throw std::runtime_error("长度范围无效");
		}
		if (cfg.codeHeightRange.size() < 2 ||
			cfg.codeHeightRange[0] <= 0 ||
			cfg.codeHeightRange[1] <= cfg.codeHeightRange[0]) {
			throw std::runtime_error("宽度范围无效");
		}
		if (cfg.codeAngleRange.size() < 2 ||
			cfg.codeAngleRange[0] > cfg.codeAngleRange[1]) {
			throw std::runtime_error("角度范围无效");
		}

		// 传递结果给输出参数
		details = cfg;
		return static_cast<int>(LoadResult::SUCCESS);
	}
	catch (const nlohmann::json::exception& e) {
		// 详细错误日志
		std::ostringstream oss;
		oss << "JSON解析错误: " << e.what() << "\n"
			<< "文件: " << codeBasicConfigFile << "\n"
			<< "错误ID: " << e.id << "\n";

		SafeLog(oss.str(), ERR, true);

		// 记录部分文件内容用于调试
		size_t debugSize = std::min(fileContent.size(), static_cast<size_t>(200));
		SafeLog("文件内容片段: " + fileContent.substr(0, debugSize), ERR, true);

		return static_cast<int>(LoadResult::JSON_ERROR);
	}
	catch (const std::exception& e) {
		SafeLog(std::string("配置处理异常: ") + e.what(), ERR, true);
		return static_cast<int>(LoadResult::JSON_ERROR);
	}
}

int LoadConfigCodeClassfy(
	const std::string codeClassfyConfigFile,
	std::vector<CodeClassfy>& details,
	std::vector<std::string>& ClassFyName,
	const std::string logFileName)
{
	auto SafeLog = [&](const std::string& msg, LogRank level, bool console) {
		Log::WriteAsyncLog(msg, level, logFileName, console);
	};

	// 1. 验证文件路径
	if (codeClassfyConfigFile.empty()) {
		SafeLog("配置文件路径为空", ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}

	// 2. 打开文件
	std::ifstream ifs(codeClassfyConfigFile, std::ios::binary | std::ios::ate);
	if (!ifs) {
		SafeLog("无法打开文件: " + codeClassfyConfigFile, ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}

	// 3. 读取文件内容
	size_t fileSize = ifs.tellg();
	if (fileSize == 0) {
		SafeLog("配置文件为空: " + codeClassfyConfigFile, ERR, true);
		ifs.close();
		return static_cast<int>(LoadResult::EMPTY_CONFIG);
	}

	ifs.seekg(0);
	std::string fileContent(fileSize, '\0');
	ifs.read(&fileContent[0], fileSize);
	ifs.close();

	try {
		// 移除BOM
		if (fileSize >= 3 &&
			static_cast<unsigned char>(fileContent[0]) == 0xEF &&
			static_cast<unsigned char>(fileContent[1]) == 0xBB &&
			static_cast<unsigned char>(fileContent[2]) == 0xBF) {
			fileContent = fileContent.substr(3);
		}

		// 过滤控制字符
		auto removeControlChars = [](std::string& str) {
			str.erase(std::remove_if(str.begin(), str.end(), [](char c) {
				return (c >= 0 && c < 32) && c != '\n' && c != '\r' && c != '\t';
				}), str.end());
		};
		removeControlChars(fileContent);

		// 修复路径字符串中的反斜杠问题
		auto fixPathString = [](std::string& jsonStr) {
			size_t pos = 0;
			std::replace(jsonStr.begin(), jsonStr.end(), '\\', '/');
		};

		fixPathString(fileContent);

		// JSON解析
		auto data = nlohmann::json::parse(fileContent);

		// 检查targets数组
		if (!data.contains("targets") || !data["targets"].is_array() || data["targets"].empty()) {
			throw std::runtime_error("'targets' must be a non-empty array");
		}

		details.clear(); // 清空输出容器
		ClassFyName.clear(); // 清空类型名称容器

		const auto& targets = data["targets"];

		Common COM; // 使用栈对象
		// 遍历所有目标配置
		for (const auto& target : targets) {
			CodeClassfy cfg;


			// 解析基础字段
			cfg.type = target.value("type", "");
			// 中文字符编码转换
			cfg.type = COM.UTF8ToGBK(cfg.type);

			// 添加到类型名称列表
			ClassFyName.push_back(cfg.type);

			cfg.classifyScoreThresh = target.value("classifyScoreThresh", 0.5);
			cfg.defectCheckMethod = target.value("defectCheckMethod", 0);
			cfg.defectScoreThresh = target.value("defectScoreThresh", 0.5);



			// 路径字段也需要编码转换
			cfg.charImgTemplate = COM.UTF8ToGBK(target.value("charImgTemplate", ""));
			cfg.charDefectModel = COM.UTF8ToGBK(target.value("charDefectModel", ""));


			// 解析范围数组
			auto parse_range = [&](const std::string& key, auto& range) {
				if (target.contains(key) && target[key].is_array() && target[key].size() >= 2) {
					range = { target[key][0], target[key][1] };
				}
			};

			parse_range("charWidthRange", cfg.charWidthRange);
			parse_range("charHeightRange", cfg.charHeightRange);

			// 验证范围有效性
			if (cfg.charWidthRange.size() < 2 || cfg.charWidthRange[0] < 0 || cfg.charWidthRange[1] < cfg.charWidthRange[0]) {
				throw std::runtime_error("无效的字符宽度范围: " + cfg.type);
			}
			if (cfg.charHeightRange.size() < 2 || cfg.charHeightRange[0] < 0 || cfg.charHeightRange[1] < cfg.charHeightRange[0]) {
				throw std::runtime_error("无效的字符高度范围: " + cfg.type);
			}

			details.push_back(std::move(cfg));
		}

		return static_cast<int>(LoadResult::SUCCESS);
	}
	catch (const nlohmann::json::exception& e) {
		std::ostringstream oss;
		oss << "JSON解析错误: " << e.what() << "\n"
			<< "文件: " << codeClassfyConfigFile << "\n"
			<< "错误ID: " << e.id << "\n";
		SafeLog(oss.str(), ERR, true);
		return static_cast<int>(LoadResult::JSON_ERROR);
	}
	catch (const std::exception& e) {
		SafeLog(std::string("配置处理异常: ") + e.what(), ERR, true);
		return static_cast<int>(LoadResult::JSON_ERROR);
	}
}

CodeInfo readConfig(const std::string& configPath) {
	auto SafeLog = [&](const std::string& msg, LogRank level, bool console) {
		Log::WriteAsyncLog(msg, level, "d://aoi_error_log.txt", console);
	};
	try {

		std::ifstream f(configPath);
		json j;
		f >> j;

		CodeInfo config;

		// 解析全局配置 
		config.extW = j["extW"];
		config.extH = j["extH"];
		config.checkType = j["checkType"];
		config.fuzzy = j["fuzzy"];
		config.fuzzyConfig = j["fuzzyConfig"].get<std::string>(); // 直接获取string
		config.infoRepeat = j["infoRepeat"];
		config.timeError = j["timeError"];
		config.expirationDateYear = j["expirationDateYear"];
		config.expirationDateMonth = j["expirationDateMonth"];
		config.expirationDateDay = j["expirationDateDay"];
		config.expirationDateError = j["expirationDateError"];
		config.ignoreRow = j["ignoreRow"];  // 读取全局ignoreRow

		// 解析目标配置
		Common COM; // 使用栈对象
		for (const auto& target : j["targets"]) {
			TargetConfig t;
			t.row = target["row"];
			t.part = target["part"];

			// 直接获取字符串，不再转换
			t.type = COM.UTF8ToGBK(target["type"].get<std::string>());
			t.info = COM.UTF8ToGBK(target["info"].get<std::string>());
			//t.type = target["type"].get<std::string>();
			//t.info = target["info"].get<std::string>();

			// 解析ROI - 使用cv::Rect
			const auto& roi = target["roi"];
			t.roi = cv::Rect(
				roi["x"].get<int>(),
				roi["y"].get<int>(),
				roi["w"].get<int>(),
				roi["h"].get<int>()
			);

			// 解析范围
			t.charWidthRange = {
				target["charWidthRange"][0].get<int>(),
				target["charWidthRange"][1].get<int>()
			};

			t.charHeightRange = {
				target["charHeightRange"][0].get<int>(),
				target["charHeightRange"][1].get<int>()
			};

			t.changeNum = target["changeNum"];

			config.targets.push_back(t);
		}
		return config;
	}
	catch (const nlohmann::json::exception& e) {
		SafeLog(configPath + std::string("配置处理异常: ") + e.what(), ERR, true);
	}
	catch (const std::exception& e) {
		SafeLog(configPath + std::string("配置处理异常: ") + e.what(), ERR, true);
	}
}

int LoadConfigBottleType(
	const std::string bottleTypeConfigFile,
	std::vector<BottleType>& details,
	const std::string logFileName)
{
	auto SafeLog = [&](const std::string& msg, LogRank level, bool console) {
		Log::WriteAsyncLog(msg, level, logFileName, console);
	};

	// 1. 验证文件路径
	if (bottleTypeConfigFile.empty()) {
		SafeLog("配置文件路径为空", ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}

	// 2. 打开文件
	std::ifstream ifs(bottleTypeConfigFile, std::ios::binary | std::ios::ate);
	if (!ifs) {
		SafeLog("无法打开文件: " + bottleTypeConfigFile, ERR, true);
		return static_cast<int>(LoadResult::FILE_ERROR);
	}

	// 3. 读取文件内容
	size_t fileSize = ifs.tellg();
	if (fileSize == 0) {
		SafeLog("配置文件为空: " + bottleTypeConfigFile, ERR, true);
		ifs.close();
		return static_cast<int>(LoadResult::EMPTY_CONFIG);
	}

	ifs.seekg(0);
	std::string fileContent(fileSize, '\0');
	ifs.read(&fileContent[0], fileSize);
	ifs.close();

	try {
		// 移除BOM
		if (fileSize >= 3 &&
			static_cast<unsigned char>(fileContent[0]) == 0xEF &&
			static_cast<unsigned char>(fileContent[1]) == 0xBB &&
			static_cast<unsigned char>(fileContent[2]) == 0xBF) {
			fileContent = fileContent.substr(3);
		}

		// 过滤控制字符
		auto removeControlChars = [](std::string& str) {
			str.erase(std::remove_if(str.begin(), str.end(), [](char c) {
				return (c >= 0 && c < 32) && c != '\n' && c != '\r' && c != '\t';
				}), str.end());
		};
		removeControlChars(fileContent);

		// JSON解析
		auto data = nlohmann::json::parse(fileContent);

		// 检查targets数组
		if (!data.contains("targets") || !data["targets"].is_array() || data["targets"].empty()) {
			throw std::runtime_error("'targets' must be a non-empty array");
		}

		const std::map<std::string, std::string> fieldMap = {
			{"capWidthRange", "capWidthRange"},
			{"capHeightRange", "capHeightRange"},
			{"handleWidthRange", "handleWidthRange"},
			{"handleHeightRange", "handleHeightRange"}
		};

		const auto& targets = data["targets"];

		Common COM; // 使用栈对象
		for (size_t i = 0; i < targets.size(); ++i) {
			const auto& item = targets[i];  // 取第一个目标配置
			BottleType cfg;
			cfg.capType = COM.UTF8ToGBK(item.value("capType", ""));
			cfg.handleType = COM.UTF8ToGBK(item.value("handleType", ""));

			// 字段解析函数
			auto parse_field = [&](const std::string& jsonField, auto& targetVar) {
				std::string field = fieldMap.count(jsonField) ?
					fieldMap.at(jsonField) : jsonField;
				if (item.contains(jsonField)) {
					targetVar = item[jsonField].get<std::decay_t<decltype(targetVar)>>();
				}
			};

			// 解析所有字段
			parse_field("capWidthRange", cfg.capWidthRange);
			parse_field("capHeightRange", cfg.capHeightRange);
			parse_field("handleWidthRange", cfg.handleWidthRange);
			parse_field("handleHeightRange", cfg.handleHeightRange);

			// 解析其他参数
			cfg.num = item.value("num", 0);

			// 参数验证
			if (cfg.capWidthRange.size() < 2 ||
				cfg.capWidthRange[0] <= 0 ||
				cfg.capWidthRange[1] < cfg.capWidthRange[0]) {
				throw std::runtime_error("瓶盖宽度范围无效");
			}
			if (cfg.capHeightRange.size() < 2 ||
				cfg.capHeightRange[0] <= 0 ||
				cfg.capHeightRange[1] <= cfg.capHeightRange[0]) {
				throw std::runtime_error("瓶盖长度范围无效");
			}
			if (cfg.handleWidthRange.size() < 2 ||
				cfg.handleWidthRange[0] <= 0 ||
				cfg.handleWidthRange[1] <= cfg.handleWidthRange[0]) {
				throw std::runtime_error("提手宽度范围无效");
			}
			if (cfg.handleHeightRange.size() < 2 ||
				cfg.handleHeightRange[0] <= 0 ||
				cfg.handleHeightRange[1] <= cfg.handleHeightRange[0]) {
				throw std::runtime_error("提手宽长范围无效");
			}
			cfg.bottleType = cfg.capType + cfg.handleType;
			// 传递结果给输出参数
			details.push_back(cfg);
		}

		return static_cast<int>(LoadResult::SUCCESS);
	}
	catch (const nlohmann::json::exception& e) {
		// 详细错误日志
		std::ostringstream oss;
		oss << "JSON解析错误: " << e.what() << "\n"
			<< "文件: " << bottleTypeConfigFile << "\n"
			<< "错误ID: " << e.id << "\n";

		SafeLog(oss.str(), ERR, true);

		// 记录部分文件内容用于调试
		size_t debugSize = std::min(fileContent.size(), static_cast<size_t>(200));
		SafeLog("文件内容片段: " + fileContent.substr(0, debugSize), ERR, true);

		return static_cast<int>(LoadResult::JSON_ERROR);
	}
	catch (const std::exception& e) {
		SafeLog(std::string("配置处理异常: ") + e.what(), ERR, true);
		return static_cast<int>(LoadResult::JSON_ERROR);
	}
}


