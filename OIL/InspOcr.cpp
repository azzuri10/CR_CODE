#include "InspOcr.h"

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
        if (line.empty() || line[0] == '#') continue;
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());

        if (key == "OCR_SAVE_DEBUG_IMAGE") params.saveDebugImage = std::stoi(value) != 0;
        else if (key == "OCR_SAVE_RESULT_IMAGE") params.saveResultImage = std::stoi(value) != 0;
        else if (key == "OCR_SAVE_LOG_TXT") params.saveLogTxt = std::stoi(value) != 0;
        else if (key == "OCR_TIME_OUT") params.timeOut = std::stoi(value);
        else if (key == "OCR_ROI_X") params.basic.roi.x = std::stoi(value);
        else if (key == "OCR_ROI_Y") params.basic.roi.y = std::stoi(value);
        else if (key == "OCR_ROI_W") params.basic.roi.width = std::stoi(value);
        else if (key == "OCR_ROI_H") params.basic.roi.height = std::stoi(value);
        else if (key == "OCR_EXT_W") params.basic.extW = std::stoi(value);
        else if (key == "OCR_EXT_H") params.basic.extH = std::stoi(value);
        else if (key == "OCR_DET_MODEL") params.detModel = value;
        else if (key == "OCR_REC_MODEL") params.recModel = value;
        else if (key == "OCR_INFO_CONFIG") params.infoConfig = value;
        else if (key == "OCR_DET_CONF") params.detConf = static_cast<float>(std::stod(value));
        else if (key == "OCR_DET_NMS") params.detNms = static_cast<float>(std::stod(value));
        else if (key == "OCR_REC_CONF") params.recConf = static_cast<float>(std::stod(value));
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
        outInfo.status.errorMessage = "OCR_DET_MODEL is empty";
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

void InspOcr::Ocr_RecognizeText(InspOcrOut& outInfo) {
    if (outInfo.status.statusCode != OCR_RETURN_OK) return;
    if (m_params.recModel.empty()) {
        outInfo.status.statusCode = OCR_RETURN_CONFIG_ERR;
        outInfo.status.errorMessage = "OCR_REC_MODEL is empty";
        return;
    }

    std::vector<FinsObject> boxes = outInfo.ocr.detBoxes;
    ANA->RankFinsObjectByX(boxes);
    std::string merged;
    for (const auto& det : boxes) {
        cv::Rect r = ANA->AdjustROI(det.box, outInfo.images.roi);
        if (r.width <= 0 || r.height <= 0) continue;
        FinsClassification cls = InferenceWorker::RunClassification(
            outInfo.system.cameraId,
            m_params.recModel,
            m_params.recClassNames,
            outInfo.images.roi(r),
            m_params.recConf
        );
        outInfo.ocr.recTexts.push_back(cls.className);
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

