#include "BarAndQR.h"

using namespace std::chrono;
using namespace HalconCpp;

BarAndQR::BarAndQR() {}
BarAndQR::~BarAndQR() {}

// ����Ԥ���������ٴ���
// ����Ԥ�������Ż���Ŀ��ٴ���
cv::Mat BarAndQR::standardPreprocess(const cv::Mat& img) {
    cv::Mat processed;
    const int width = img.cols;
    const int height = img.rows;

    // תΪ�Ҷ�ͼ
    if (img.channels() == 3) {
        cv::cvtColor(img, processed, cv::COLOR_BGR2GRAY);
    }
    else {
        processed = img.clone();
    }

    // ����ӦCLAHE - ����ͼ��ߴ��������
    double clipLimit = (width > 2000 || height > 2000) ? 1.8 : 2.0;
    cv::Size tileSize = (width > 2000 || height > 2000) ? cv::Size(4, 4) : cv::Size(8, 8);

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clipLimit, tileSize);
    clahe->apply(processed, processed);

    // ����ȥ����� - ����ͼ��ߴ�ѡ�񷽷�
    if (width * height > 1.5e6) { // 150����������
        // ��ͼ��ʹ�ÿ���˫���˲�
        cv::Mat filtered;
        cv::bilateralFilter(processed, filtered, 3, 25, 25);
        processed = filtered;
    }
    else if (width * height > 0.5e6) { // 50-150������
     // �е�ͼ��ʹ�ÿ��ٷǾֲ���ֵ�����ٲ�����
        cv::fastNlMeansDenoising(processed, processed, 3, 5, 11);
    }
    else { // 50����������
     // Сͼ��ʹ�ñ�׼�Ǿֲ���ֵ
        cv::fastNlMeansDenoising(processed, processed, 7, 7, 21);
    }

    return processed;
}

// ��ǿԤ�������Ż���ĸ���������
cv::Mat BarAndQR::enhancedPreprocess(const cv::Mat& img) {
    cv::Mat processed = img.clone();
    bool isColor = processed.channels() == 3;

    // ���Բ�ɫͼ����и߹��޸�
    if (isColor) {
        cv::Mat lab;
        cv::cvtColor(processed, lab, cv::COLOR_BGR2Lab);
        std::vector<cv::Mat> labChannels;
        cv::split(lab, labChannels);

        // �����߹�������ģ
        cv::Mat mask;
        cv::threshold(labChannels[0], mask, 220, 255, cv::THRESH_BINARY);

        // �����߹����򳬹�ͼ��1%ʱ�Ž����޸�
        if (cv::countNonZero(mask) > (mask.rows * mask.cols * 0.01)) {
            cv::inpaint(processed, mask, processed, 3, cv::INPAINT_TELEA);
        }
    }

    // תΪ�Ҷ�ͼ����ɫͼ��Ҫת�����Ҷ�ͼֱ��ʹ�ã�
    if (isColor) {
        cv::cvtColor(processed, processed, cv::COLOR_BGR2GRAY);
    }

    // �Ľ���CLAHE���� - �ӽ�standardPreprocess��������ǿ
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.2, cv::Size(8, 8)); // ΢��clipLimit
    clahe->apply(processed, processed);

    // ���º͵��񻯴���
    cv::Mat blurred;
    cv::GaussianBlur(processed, blurred, cv::Size(0, 0), 1.5);
    cv::addWeighted(processed, 1.2, blurred, -0.2, 0, processed);

    // ��ѡ����Ե�����˲����������ȥ����
    if (img.cols > 1000 || img.rows > 1000) {
        cv::Mat filtered;
        cv::bilateralFilter(processed, filtered, 5, 75, 75);
        processed = filtered;
    }
    else {
        // ���ȥ�루����standardPreprocess��
        cv::fastNlMeansDenoising(processed, processed, 7, 7, 15);
    }

    return processed;
}


// ����Ӧ��������
void BarAndQR::applyAdaptiveParameters(HalconCpp::HTuple barCodeHandle, const cv::Mat& img) {
    // ����ͼ������ָ��
    cv::Scalar mean, stddev;
    cv::meanStdDev(img, mean, stddev);
    double contrast = stddev[0];
    double brightness = mean[0];

    if (brightness < 50) {
        HalconCpp::SetBarCodeParam(barCodeHandle, "contrast_min", 15);
    }
    else if (brightness > 200) {
        HalconCpp::SetBarCodeParam(barCodeHandle, "contrast_min", 15);
    }

    // �Աȶ�����Ӧ
    if (contrast < 20) {
        HalconCpp::SetBarCodeParam(barCodeHandle, "element_size_min", 1.2);
        // ��persistence����Ϊ1��������
        HalconCpp::SetBarCodeParam(barCodeHandle, "persistence", 1);
    }
    else if (img.cols > 2000) {
        HalconCpp::SetBarCodeParam(barCodeHandle, "element_size_min", 2.5);
    }

    // ����ͨ��Ĭ������
    HalconCpp::SetBarCodeParam(barCodeHandle, "check_char", "present");
    HalconCpp::SetBarCodeParam(barCodeHandle, "num_scanlines", 10);
}

cv::RotatedRect BarAndQR::CreateRotatedRect(double col, double row,
    double phiRad,
    double width, double height) {
    return cv::RotatedRect(
        cv::Point2f(static_cast<float>(col), static_cast<float>(row)),
        cv::Size2f(static_cast<float>(width), static_cast<float>(height)),
        static_cast<float>(phiRad * 180.0 / CV_PI)  // ����ת�Ƕ�
    );
}
bool BarAndQR::ProcessSymbolRegions(
    HalconCpp::HObject& symbolRegions,
    HalconCpp::HTuple& codeStrings,
    bool isBarCode,
    std::vector<BarResult>& barResults,
    HalconCpp::HTuple handle,
    const std::string& currentType)
{
    if (codeStrings.Length() <= 0) return false;
    if (barResults.capacity() < static_cast<size_t>(codeStrings.Length())) {
        barResults.reserve(barResults.size() + static_cast<size_t>(codeStrings.Length()));
    }

    for (int i = 0; i < codeStrings.Length(); i++) {
        BarResult result;
        result.barType = currentType;
        result.infoResult = std::string(codeStrings[i].S());

        // ��ȡ�������
        HalconCpp::HObject singleRegion;
        HalconCpp::SelectObj(symbolRegions, &singleRegion, i + 1);

        // ��ȡ���Ŷȷ���
        double confidence = 1.0;
        if (isBarCode && handle.Length() > 0) {
            try {
                HalconCpp::HTuple confValue;
                HalconCpp::GetBarCodeResult(handle, i, "confidence", &confValue);
                confidence = confValue.D();
                result.detectScore = confidence * 100; // ת��Ϊ�ٷֱ�
                result.analysisScore = confidence * 100;
            }
            catch (...) {
                // ���ŶȻ�ȡʧ��ʱʹ��Ĭ��ֵ
                result.detectScore = 80;
                result.analysisScore = 80;
            }
        }
        else {
            // ��ά��Ĭ�����Ŷ�
            result.detectScore = 90;
            result.analysisScore = 90;
        }

        // ������ת���κͽǶ�
        double phiRad = 0.0;
        if (isBarCode) {
            // һά�봦��
            // === ͳһ���������ʼ��ʹ��SmallestRectangle2 ===
            HalconCpp::HTuple row, col, phi, length1, length2;
            HalconCpp::SmallestRectangle2(singleRegion, &row, &col, &phi, &length1, &length2);
            double phiRad = phi[0].D();

            // === �ؼ��޸����ǶȺ�����ϵת�� ===
            // Halcon��ʱ�뻡�� �� OpenCV˳ʱ��Ƕ�
            double opencvAngle = -phiRad * 180.0 / CV_PI; // ����ת

            // �Ƕȹ�һ����[0,360)��
            opencvAngle = fmod(opencvAngle, 360.0);
            if (opencvAngle < 0) opencvAngle += 360.0;

            // OpenCVҪ��Ƕ���[0,180)��Χ
            if (opencvAngle >= 180.0) opencvAngle -= 180.0;

            // ������ת���Σ�ע������ϵת����
            result.rect = cv::RotatedRect(
                cv::Point2f(static_cast<float>(col[0].D()), // x = col
                    static_cast<float>(row[0].D())), // y = row
                cv::Size2f(static_cast<float>(length1[0].D() * 2),
                    static_cast<float>(length2[0].D() * 2)),
                static_cast<float>(opencvAngle)
            );

            result.barAngle = opencvAngle; // �洢������ĽǶ�
        }
        else {
            // ��ά�봦��
            HalconCpp::HTuple row, col, phi, length1, length2;
            HalconCpp::SmallestRectangle2(singleRegion, &row, &col, &phi, &length1, &length2);
            phiRad = phi[0].D();

            result.rect = CreateRotatedRect(
                col[0].D(), row[0].D(), phiRad,
                length1[0].D() * 2, length2[0].D() * 2);
            result.barAngle = phiRad * 180.0 / CV_PI;
        }

        barResults.push_back(result);
    }
    return !barResults.empty();
}

bool BarAndQR::BAQ_CheckBar(cv::Mat img, BarConfig barConfig, std::vector<BarResult>& barResult)
{
    if (img.empty()) {
        std::cerr << "Input image is empty!" << std::endl;
        return false;
    }

    auto start = high_resolution_clock::now();
    barResult.clear(); 

    // ����ģʽѡ��Ԥ��������
    cv::Mat processedImg;
    if (barConfig.checkModel == 0) {
       //processedImg = standardPreprocess(img);
       processedImg = img;
    }
    else if (barConfig.checkModel == 1) {
        processedImg = enhancedPreprocess(img);
    }
    else if (barConfig.checkModel == 2) {
        processedImg = enhancedPreprocess(img);
    }

    // ����֧�ֵ�����
    


    bool success = false;
    std::vector<double> scales;

    // ����ģʽȷ��������
    if (barConfig.checkModel == 0) {
        scales = { 1.0 }; // ����ģʽ�����߶ȼ��
    }
    else if (barConfig.checkModel == 1) {
        scales = { 0.8, 1.0, 1.2 }; // �Ż�ģʽ����߶ȼ��
    }
    else if (barConfig.checkModel == 2) {
        scales = { 0.8, 1.0, 1.2 }; // �Ż�ģʽ����߶ȼ��
    }

    for (const auto& type : barConfig.targetTypes) {
        for (double scale : scales) {
            cv::Mat resized;
            if (scale != 1.0) {
                cv::resize(processedImg, resized, cv::Size(), scale, scale);
            }
            else {
                resized = processedImg;
            }

            try {
                HalconCpp::HImage halcon_image;
                halcon_image.GenImage1("byte", resized.cols, resized.rows, resized.data);

                // ����һά��ģ��
                HalconCpp::HTuple barCodeHandle;
                HalconCpp::CreateBarCodeModel(HTuple(), HTuple(), &barCodeHandle);
                HalconHandleGuard barCodeGuard(barCodeHandle, HalconCpp::ClearBarCodeModel);
                 
                //// ����ͨ�ò���
                HalconCpp::SetBarCodeParam(barCodeHandle, "stop_after_result_num", 0);
                ///*HalconCpp::SetBarCodeParam(barCodeHandle, "persistence",
                //    (barConfig.checkModel == 1) ? 2 : 1);*/

                // ����һά��
                HalconCpp::HObject symbolRegions;
                HalconCpp::HTuple codeStrings;
                HalconCpp::FindBarCode(halcon_image, &symbolRegions, barCodeHandle,
                    type.c_str(), &codeStrings);

                // �����������
                std::vector<BarResult> tempResults;
                if (ProcessSymbolRegions(symbolRegions, codeStrings, true,
                    tempResults, barCodeHandle, type)) {

                    // Ӧ�����ű�������
                    for (auto& result : tempResults) {
                        result.rect.center.x /= scale;
                        result.rect.center.y /= scale;
                        result.rect.size.width /= scale;
                        result.rect.size.height /= scale;
                    }
                    // Ӧ�����ű�������
                    for (auto& result : tempResults) {
                        result.rect.center.x += barConfig.roi.x;
                        result.rect.center.y += barConfig.roi.y;
                    }
                   
                    // �ϲ����
                    barResult.insert(barResult.end(),
                        tempResults.begin(),
                        tempResults.end());
                    success = true;
                    break;
                }
            }
            catch (const HalconCpp::HException& e) {
                // �Ż�ģʽ�¼������ԣ�����ģʽ�¼�¼����
                if (barConfig.checkModel == 0) {
                    std::cerr << "HALCON error: " << e.ErrorMessage() << std::endl;
                }
            }
        }
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    //if (success) {
    //    std::cout << "Barcode detected in " << duration.count() << " ms" << std::endl;
    //}
    //else {
    //    std::cout << "No barcode found after " << duration.count() << " ms" << std::endl;
    //}

    return success;
}


bool BarAndQR::BAQ_CheckQR(cv::Mat img, BarConfig barConfig, std::vector<BarResult>& barResult)
{
    if (img.empty()) return false;

    auto start = high_resolution_clock::now();
    barResult.clear();

    // ����ģʽѡ��Ԥ����
    cv::Mat processedImg;
    if (barConfig.checkModel == 0) {
        processedImg = standardPreprocess(img);
    }
    else if (barConfig.checkModel == 1) {
        processedImg = standardPreprocess(img);
    } 
    else  if (barConfig.checkModel == 2) {
        processedImg = enhancedPreprocess(img);
    }

   

    bool success = false;
    std::vector<double> scales;

    // ����ģʽȷ��������
    if (barConfig.checkModel == 0) {
        scales = { 1.0 }; // ����ģʽ�����߶ȼ��
    }
    else {
        scales = { 0.9, 1.0, 1.1 }; // �Ż�ģʽ����߶ȼ��
    }

    for (const auto& type : barConfig.targetTypes) {
        for (double scale : scales) {
            cv::Mat resized;
            if (scale != 1.0) {
                cv::resize(processedImg, resized, cv::Size(), scale, scale);
            }
            else {
                resized = processedImg;
            }

            try {
                HalconCpp::HImage halcon_image;
                halcon_image.GenImage1("byte", resized.cols, resized.rows, resized.data);

                // ������ά��ģ��
                HalconCpp::HTuple dataCodeHandle;
                HalconCpp::CreateDataCode2dModel(type.c_str(),
                    HTuple(), HTuple(),
                    &dataCodeHandle);
                HalconHandleGuard dataCodeGuard(
                    dataCodeHandle,
                    HalconCpp::ClearDataCode2dModel
                );

                // ���ò���
                if (barConfig.checkModel == 1) {
                    // ʹ��Halcon18+���ݵĲ������÷�ʽ
                    HalconCpp::SetDataCode2dParam(dataCodeHandle, "contrast_tolerance", "high");
                    HalconCpp::SetDataCode2dParam(dataCodeHandle, "timeout", 1000); // ���ӳ�ʱʱ��
                }

                // ���Ҷ�ά��
                HalconCpp::HObject symbolRegions;
                HalconCpp::HTuple resultHandles, codeStrings;
                HalconCpp::FindDataCode2d(halcon_image, &symbolRegions, dataCodeHandle,
                    HTuple(), HTuple(),
                    &resultHandles, &codeStrings);

                // �����������
                std::vector<BarResult> tempResults;
                if (ProcessSymbolRegions(symbolRegions, codeStrings, false,
                    tempResults, dataCodeHandle, type)) {

                    // Ӧ�����ű�������
                    for (auto& result : tempResults) {
                        result.rect.center.x /= scale;
                        result.rect.center.y /= scale;
                        result.rect.size.width /= scale;
                        result.rect.size.height /= scale;
                    }

                    // �ϲ����
                    barResult.insert(barResult.end(),
                        tempResults.begin(),
                        tempResults.end());
                    success = true;
                }
            }
            catch (const HalconCpp::HException& e) {
                // �Ż�ģʽ�¼������ԣ�����ģʽ�¼�¼����
                if (barConfig.checkModel == 0) {
                    std::cerr << "HALCON error: " << e.ErrorMessage() << std::endl;
                }
            }
        }
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    /*if (success) {
        std::cout << "QR code detected in " << duration.count() << " ms" << std::endl;
    }
    else {
        std::cout << "No QR code found after " << duration.count() << " ms" << std::endl;
    }*/

    return success;
}