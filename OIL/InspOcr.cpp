#include "InspOcr.h"

std::vector<std::string> InspOcr::SplitCsv(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item.erase(0, item.find_first_not_of(" \t\r\n"));
        item.erase(item.find_last_not_of(" \t\r\n") + 1);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

InspOcr::InspOcr(const cv::Mat& img, int cameraId, int jobId, int timeOut, InspOcrOut& outInfo)
    : LOG(std::make_unique<Log>()),
      COM(std::make_unique<Common>()),
      ANA(std::make_unique<AnalyseMat>()),
      DAS(std::make_unique<DrawAndShowImg>()),
      m_img(img.clone()) {
    m_startTime = std::chrono::high_resolution_clock::now();
    m_params.timeOut = timeOut;
    outInfo.system.cameraId = cameraId;
    outInfo.system.jobId = jobId;
    outInfo.system.startTime = COM->time_t2string_with_ms();
    outInfo.images.outputImg = m_img.clone();
}

bool InspOcr::CheckTimeout() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();
    return elapsed > m_params.timeOut;
}

bool InspOcr::readParams(const std::string& filePath, InspOcrIn& params, InspOcrOut& outInfo) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        outInfo.status.statusCode = OCR_RETURN_CONFIG_ERR;
        outInfo.status.errorMessage = "OCR config file missing";
        return false;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        const auto sharp = line.find("##");
        if (sharp != std::string::npos) continue;
        const auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        const size_t keyBegin = key.find_first_not_of(" \t\r\n");
        const size_t keyEnd = key.find_last_not_of(" \t\r\n");
        const size_t valBegin = value.find_first_not_of(" \t\r\n");
        const size_t valEnd = value.find_last_not_of(" \t\r\n");
        if (keyBegin == std::string::npos || keyEnd == std::string::npos) continue;
        key = key.substr(keyBegin, keyEnd - keyBegin + 1);
        if (valBegin == std::string::npos || valEnd == std::string::npos) value.clear();
        else value = value.substr(valBegin, valEnd - valBegin + 1);

        if (key == "CODE_OCR_SAVE_DEBUG_IMAGE") params.saveDebugImage = std::stoi(value) != 0;
        else if (key == "CODE_OCR_SAVE_RESULT_IMAGE") params.saveResultImage = std::stoi(value) != 0;
        else if (key == "CODE_OCR_SAVE_LOG_TXT") params.saveLogTxt = std::stoi(value) != 0;
        else if (key == "CODE_OCR_TIME_OUT") params.timeOut = std::stoi(value);
        else if (key == "CODE_OCR_ROI_X") params.basic.roi.x = std::stoi(value);
        else if (key == "CODE_OCR_ROI_Y") params.basic.roi.y = std::stoi(value);
        else if (key == "CODE_OCR_ROI_W") params.basic.roi.width = std::stoi(value);
        else if (key == "CODE_OCR_ROI_H") params.basic.roi.height = std::stoi(value);
        else if (key == "CODE_OCR_EXT_W") params.basic.extW = std::stoi(value);
        else if (key == "CODE_OCR_EXT_H") params.basic.extH = std::stoi(value);
        else if (key == "CODE_OCR_DET_MODEL") params.detModel = value;
        else if (key == "CODE_OCR_REC_MODEL") params.recModel = value;
        else if (key == "CODE_OCR_INFO_CHECK") params.infoConfig = value;
        else if (key == "CODE_OCR_DET_CONF") params.detConf = static_cast<float>(std::stod(value));
        else if (key == "CODE_OCR_DET_NMS") params.detNms = static_cast<float>(std::stod(value));
        else if (key == "CODE_OCR_REC_CONF") params.recConf = static_cast<float>(std::stod(value));
        else if (key == "CODE_OCR_DET_CLASS_NAMES") params.detClassNames = SplitCsv(value);
        else if (key == "CODE_OCR_REC_CLASS_NAMES") params.recClassNames = SplitCsv(value);
        else if (key == "CODE_OCR_REC_SCHED_CHUNK") params.recSchedChunk = static_cast<size_t>(std::max(1, std::stoi(value)));
        else if (key == "CODE_OCR_REC_INFER_CHUNK") params.recInferChunk = static_cast<size_t>(std::max(1, std::stoi(value)));
        else if (key == "CODE_OCR_REC_THREADS") params.recThreads = std::max(1, std::stoi(value));
        else if (key == "CODE_OCR_ENABLE_CLS") params.enableCls = std::stoi(value) != 0;
        else if (key == "CODE_OCR_CLS_MODEL") params.clsModel = value;
        else if (key == "CODE_OCR_CLS_CONF") params.clsConf = static_cast<float>(std::stod(value));
        else if (key == "CODE_OCR_CLS_BATCH") params.clsBatch = std::max(1, std::stoi(value));
        else if (key == "CODE_OCR_PREFER_PADDLE_NATIVE") params.preferPaddleNative = std::stoi(value) != 0;
        else if (key == "CODE_OCR_REC_DICT_PATH") params.recDictPath = value;
        else if (key == "CODE_OCR_USE_GPU") params.useGpu = std::stoi(value) != 0;
        else if (key == "CODE_OCR_USE_TRT") params.useTrt = std::stoi(value) != 0;
        else if (key == "CODE_OCR_USE_MKLDNN") params.useMkldnn = std::stoi(value) != 0;
        else if (key == "CODE_OCR_GPU_ID") params.gpuId = std::stoi(value);
        else if (key == "CODE_OCR_GPU_MEM_MB") params.gpuMemMB = std::max(256, std::stoi(value));
        else if (key == "CODE_OCR_CPU_THREADS") params.cpuThreads = std::max(1, std::stoi(value));
        else if (key == "CODE_OCR_PRECISION") params.precision = value;
    }

    if (!params.infoConfig.empty()) {
        params.inputInfo = readConfig(params.infoConfig);
    }
    return true;
}

void InspOcr::Ocr_SetROI(InspOcrOut& outInfo) {
    if (outInfo.status.statusCode != OCR_RETURN_OK) return;
    cv::Rect roi = m_params.basic.roi;
    roi = ANA->AdjustROI(roi, m_img);
    outInfo.images.roi = m_img(roi).clone();
}

void InspOcr::Ocr_DetectText(InspOcrOut& outInfo) {
    if (outInfo.status.statusCode != OCR_RETURN_OK) return;
    cv::Rect roi = m_params.basic.roi;
    roi.x -= m_params.basic.extW;
    roi.y -= m_params.basic.extH;
    roi.width += m_params.basic.extW * 2;
    roi.height += m_params.basic.extH * 2;
    roi = ANA->AdjustROI(roi, m_img);
    outInfo.images.roi = m_img(roi).clone();

    if (outInfo.images.roi.empty()) {
        outInfo.status.statusCode = OCR_RETURN_NO_TEXT;
        outInfo.status.errorMessage = "OCR ROI is empty";
        return;
    }

    if (m_params.detModel.empty()) {
        outInfo.status.statusCode = OCR_RETURN_CONFIG_ERR;
        outInfo.status.errorMessage = "CODE_OCR_DET_MODEL is empty";
        return;
    }

    if (m_params.preferPaddleNative && Ocr_DetectTextPaddleNative(outInfo)) {
        return;
    }

    outInfo.ocr.detBoxes = InferenceWorker::Run(
        outInfo.system.cameraId,
        m_params.detModel,
        m_params.detClassNames,
        outInfo.images.roi,
        m_params.detConf,
        m_params.detNms
    );
}

bool InspOcr::Ocr_DetectTextPaddleNative(InspOcrOut& outInfo) {
    (void)outInfo;
    return false;
}

void InspOcr::Ocr_RecognizeText(InspOcrOut& outInfo) {
    if (outInfo.status.statusCode != OCR_RETURN_OK) return;
    if (m_params.recModel.empty()) {
        outInfo.status.statusCode = OCR_RETURN_CONFIG_ERR;
        outInfo.status.errorMessage = "CODE_OCR_REC_MODEL is empty";
        return;
    }

    if (m_params.preferPaddleNative && Ocr_RecognizeTextPaddleNative(outInfo)) {
        return;
    }
    Ocr_RecognizeTextConcurrentFallback(outInfo);
}

bool InspOcr::Ocr_RecognizeTextPaddleNative(InspOcrOut& outInfo) {
    (void)outInfo;
    return false;
}

void InspOcr::Ocr_RecognizeTextConcurrentFallback(InspOcrOut& outInfo) {
    std::vector<FinsObject> boxes = outInfo.ocr.detBoxes;
    ANA->RankFinsObjectByX(boxes);
    if (boxes.empty()) {
        outInfo.status.statusCode = OCR_RETURN_NO_TEXT;
        outInfo.status.errorMessage = "OCR detBoxes is empty";
        return;
    }

    std::vector<cv::Mat> roiImgs;
    roiImgs.reserve(boxes.size());
    for (const auto& det : boxes) {
        cv::Rect r = ANA->AdjustROI(det.box, outInfo.images.roi);
        if (r.width <= 0 || r.height <= 0) roiImgs.emplace_back();
        else roiImgs.push_back(outInfo.images.roi(r).clone());
    }

    std::vector<FinsClassification> allResults(roiImgs.size(), { "", 0.0f });
    const size_t schedChunk = std::max<size_t>(1, m_params.recSchedChunk);
    const size_t inferChunk = std::max<size_t>(1, m_params.recInferChunk);
    std::vector<std::future<void>> futures;

    for (size_t start = 0; start < roiImgs.size(); start += schedChunk) {
        const size_t end = std::min(start + schedChunk, roiImgs.size());
        futures.emplace_back(std::async(std::launch::async, [&, start, end]() {
            std::vector<cv::Mat> local(roiImgs.begin() + start, roiImgs.begin() + end);
            std::vector<FinsClassification> chunkResults = InferenceWorker::RunClassificationBatch(
                outInfo.system.cameraId, m_params.recModel, m_params.recClassNames, local, m_params.recConf, inferChunk);
            for (size_t i = 0; i < chunkResults.size(); ++i) {
                allResults[start + i] = chunkResults[i];
            }
        }));

        if (futures.size() >= static_cast<size_t>(std::max(1, m_params.recThreads))) {
            futures.front().get();
            futures.erase(futures.begin());
        }
    }
    for (auto& f : futures) f.get();

    std::string merged;
    for (const auto& cls : allResults) {
        outInfo.ocr.recTexts.push_back(cls.className);
        outInfo.ocr.recScores.push_back(cls.confidence);
        merged += cls.className;
    }
    outInfo.ocr.mergedText = merged;
    if (merged.empty()) {
        outInfo.status.statusCode = OCR_RETURN_NO_TEXT;
        outInfo.status.errorMessage = "OCR text is empty";
    }
}

void InspOcr::Ocr_CompareTargets(InspOcrOut& outInfo) {
    if (outInfo.status.statusCode != OCR_RETURN_OK) return;

    bool allOk = true;
    for (const auto& target : m_params.inputInfo.targets) {
        DetectionResult dr;
        dr.row = target.row;
        dr.part = target.part;
        dr.type = target.type;
        dr.expectedInfo = target.info;
        dr.actualInfo = outInfo.ocr.mergedText;
        if (outInfo.ocr.mergedText.find(target.info) != std::string::npos) {
            dr.status = "OK";
            dr.message = "matched";
        }
        else {
            dr.status = "NG";
            dr.message = "not matched";
            allOk = false;
        }
        outInfo.ocr.compareResults.push_back(dr);
    }

    if (!allOk) {
        outInfo.status.statusCode = OCR_RETURN_TEXT_MISMATCH;
        outInfo.status.errorMessage = "OCR content mismatch";
    }
}

void InspOcr::Ocr_DrawResult(InspOcrOut& outInfo) {
    if (outInfo.images.outputImg.empty()) {
        outInfo.images.outputImg = m_img.clone();
    }
    cv::putText(outInfo.images.outputImg, outInfo.ocr.mergedText, cv::Point(20, 60),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);
}

int InspOcr::Ocr_Main(InspOcrOut& outInfo) {
    if (m_img.empty()) {
        outInfo.status.statusCode = OCR_RETURN_INPUT_PARA_ERR;
        outInfo.status.errorMessage = "input image is empty";
        return outInfo.status.statusCode;
    }

    if (!readParams(outInfo.paths.configFile, m_params, outInfo)) {
        return outInfo.status.statusCode;
    }

    Ocr_DetectText(outInfo);
    if (CheckTimeout()) {
        outInfo.status.statusCode = OCR_RETURN_TIMEOUT;
        outInfo.status.errorMessage = "timeout";
        return outInfo.status.statusCode;
    }
    Ocr_RecognizeText(outInfo);
    if (CheckTimeout()) {
        outInfo.status.statusCode = OCR_RETURN_TIMEOUT;
        outInfo.status.errorMessage = "timeout";
        return outInfo.status.statusCode;
    }
    Ocr_CompareTargets(outInfo);
    Ocr_DrawResult(outInfo);

    return outInfo.status.statusCode;
}

