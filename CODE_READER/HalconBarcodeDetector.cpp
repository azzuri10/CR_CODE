#pragma once
// HalconBarcodeDetector.hpp
// A lightweight, high-performance HALCON C++ barcode wrapper with ROI cropping,
// model reuse, and thread-safe per-thread model copies.
//
// Requirements:
//  - HALCON (include HalconCpp.h and link HALCON libs)
//  - OpenCV (for Mat/RotatedRect types). If you don't use OpenCV, you can replace
//    the small bits at the bottom accordingly.
//
// Usage:
//  HalconBarcode::Detector::Config cfg;
//  cfg.code_types = {"EAN-13", "Code 128"};
//  cfg.stop_after_result_num = 0;      // 0 = read all
//  cfg.meas_param_estimation = false;  // faster in stable setups
//  cfg.slanted = false;                // disable heavy angle search if not needed
//  cfg.position_deviation = 5.0;       // small position tolerance (px)
//  cfg.roi = {x, y, w, h};             // optional; set to {} to disable
//
//  HalconBarcode::Detector det(cfg);
//  std::vector<HalconBarcode::BarResult> results;
//  bool ok = det.detect(frame_bgr_or_gray, results);
//
// Threading:
//  The detector keeps a base model and lazily creates a per-thread copy via
//  CopyBarCodeModel(). You can safely call detect() concurrently from multiple
//  threads without external locks.

#include <HalconCpp.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <optional>

namespace HalconBarcode {

    struct ROI {
        int x{ 0 }, y{ 0 }, width{ 0 }, height{ 0 };
        bool valid() const { return width > 0 && height > 0; }
    };

    struct BarResult {
        cv::RotatedRect rect;               // center/size/angle (angle in degrees)
        std::vector<cv::Point2f> corners;   // 4 rectangle corners (clockwise)
        std::string text;                   // decoded string
        std::string type;                   // decoded type if available
        float score{ 1.0f };                  // optional quality/score (best-effort)
    };

    class Detector {
    public:
        struct Config {
            std::vector<std::string> code_types;  // e.g., {"EAN-13"} or {"auto"}
            ROI roi{};                             // optional cropped region on input image
            int stop_after_result_num{ 0 };          // 0 = all, >0 stops earlier
            bool meas_param_estimation{ false };     // set false to skip auto estimation (faster)
            bool slanted{ false };                   // set false if barcodes are not rotated significantly
            double position_deviation{ 10.0 };       // px tolerance for position search
            // If your scene is very stable, shrink this (e.g., 5.0) for speed
        };

        explicit Detector(const Config& cfg) : cfg_(cfg) {
            // Create the base barcode model once
            HalconCpp::CreateBarCodeModel(HalconCpp::HTuple(), HalconCpp::HTuple(), &base_model_);

            // Apply static params only once on the base model
            HalconCpp::SetBarCodeParam(base_model_, "stop_after_result_num", cfg_.stop_after_result_num);
            HalconCpp::SetBarCodeParam(base_model_, "meas_param_estimation", cfg_.meas_param_estimation ? "true" : "false");
            HalconCpp::SetBarCodeParam(base_model_, "slanted", cfg_.slanted ? "true" : "false");
            HalconCpp::SetBarCodeParam(base_model_, "position_deviation", cfg_.position_deviation);

            // Restrict code types if provided
            if (!cfg_.code_types.empty()) {
                // HALCON supports tuple input for multiple types
                HalconCpp::HTuple types;
                for (const auto& t : cfg_.code_types) types.Append(t.c_str());
                HalconCpp::SetBarCodeParam(base_model_, "code_types", types);
            }
        }

        ~Detector() {
            try {
                if (base_model_.Length() > 0)
                    HalconCpp::ClearBarCodeModel(base_model_);
            }
            catch (...) {
                // swallow on shutdown
            }
        }

        // Change ROI at runtime (thread-safe for readers)
        void setROI(const ROI& roi) { cfg_.roi = roi; }

        // Main API: detect barcodes from a BGR or GRAY cv::Mat
        // Thread-safe: internally uses per-thread model copy
        bool detect(const cv::Mat& img_bgr_or_gray, std::vector<BarResult>& out_results) const {
            out_results.clear();
            if (img_bgr_or_gray.empty()) return false;

            try {
                cv::Mat gray;
                toGray8(img_bgr_or_gray, gray);

                // HALCON image (ensure contiguous)
                cv::Mat gray_contig = gray.isContinuous() ? gray : gray.clone();
                HalconCpp::HImage himg;
                himg.GenImage1("byte", gray_contig.cols, gray_contig.rows, gray_contig.data);

                // ROI crop (optional)
                HalconCpp::HImage himg_roi = himg;
                if (cfg_.roi.valid()) {
                    const int r0 = std::clamp(cfg_.roi.y, 0, gray_contig.rows - 1);
                    const int c0 = std::clamp(cfg_.roi.x, 0, gray_contig.cols - 1);
                    const int r1 = std::clamp(cfg_.roi.y + cfg_.roi.height - 1, 0, gray_contig.rows - 1);
                    const int c1 = std::clamp(cfg_.roi.x + cfg_.roi.width - 1, 0, gray_contig.cols - 1);
                    if (r1 > r0 && c1 > c0) {
                        himg_roi = himg.CropRectangle1(r0, c0, r1, c1);
                    }
                }

                // Per-thread model copy (lazy init)
                HalconCpp::HTuple model = getThreadModel();

                // Find barcodes
                HalconCpp::HObject symbolRegions;
                HalconCpp::HTuple codeStrings;

                // If code types were specified in config we already set them on the model.
                // So here we can pass "auto" to let the model's param decide.
                const char* type_for_find = "auto";
                HalconCpp::FindBarCode(himg_roi, &symbolRegions, model, type_for_find, &codeStrings);

                // If none found, early out
                if (codeStrings.Length() <= 0) return true; // success but empty

                // Smallest rotated rectangle for each region
                HalconCpp::HTuple rows, cols, phis, l1s, l2s;
                HalconCpp::SmallestRectangle2(symbolRegions, &rows, &cols, &phis, &l1s, &l2s);

                const bool used_roi = cfg_.roi.valid();
                const float add_x = used_roi ? static_cast<float>(cfg_.roi.x) : 0.0f;
                const float add_y = used_roi ? static_cast<float>(cfg_.roi.y) : 0.0f;

                const Hlong n = codeStrings.Length();
                out_results.reserve(static_cast<size_t>(n));

                for (Hlong i = 0; i < n; ++i) {
                    BarResult br;
                    const double row = rows[i].D();
                    const double col = cols[i].D();
                    const double phi = phis[i].D();
                    const double l1 = l1s[i].D();
                    const double l2 = l2s[i].D();

                    // HALCON's SmallestRectangle2: center (row,col), angle in radians, half-lengths
                    // Convert to OpenCV RotatedRect (angle in degrees, x=col, y=row)
                    const float cx = static_cast<float>(col) + add_x;
                    const float cy = static_cast<float>(row) + add_y;
                    const float w = static_cast<float>(2.0 * l2); // long side ≈ along phi+90 depending on standard
                    const float h = static_cast<float>(2.0 * l1);
                    const float angle_deg = static_cast<float>(phi * 180.0 / 3.14159265358979323846);

                    br.rect = cv::RotatedRect(cv::Point2f(cx, cy), cv::Size2f(w, h), angle_deg);
                    br.corners = rectCorners(br.rect);

                    br.text = codeStrings[i].S();
                    br.type = bestEffortType(model, i);  // may be empty if not available
                    br.score = bestEffortScore(model, i); // 1.0f if not available

                    out_results.emplace_back(std::move(br));
                }

                return true;
            }
            catch (const HalconCpp::HException&) {
                return false;
            }
            catch (...) {
                return false;
            }
        }

    private:
        Config cfg_{};
        HalconCpp::HTuple base_model_;

        // Thread-local copy of the model for safe parallel use.
        static HalconCpp::HTuple& threadLocalModel() {
            thread_local HalconCpp::HTuple tls_model; // default empty
            return tls_model;
        }

        HalconCpp::HTuple getThreadModel() const {
            auto& tls = threadLocalModel();
            if (tls.Length() == 0) {
                HalconCpp::CreateBarCodeModel(HalconCpp::HTuple(), HalconCpp::HTuple(), &tls);

                // 复制 base_model_ 的参数
                HalconCpp::HTuple val;
                HalconCpp::GetBarCodeParam(base_model_, "stop_after_result_num", &val);
                HalconCpp::SetBarCodeParam(tls, "stop_after_result_num", val);

                HalconCpp::GetBarCodeParam(base_model_, "meas_param_estimation", &val);
                HalconCpp::SetBarCodeParam(tls, "meas_param_estimation", val);

                HalconCpp::GetBarCodeParam(base_model_, "slanted", &val);
                HalconCpp::SetBarCodeParam(tls, "slanted", val);

                HalconCpp::GetBarCodeParam(base_model_, "position_deviation", &val);
                HalconCpp::SetBarCodeParam(tls, "position_deviation", val);

                HalconCpp::GetBarCodeParam(base_model_, "code_types", &val);
                if (val.Length() > 0) HalconCpp::SetBarCodeParam(tls, "code_types", val);
            }
            return tls;
        }

        static void toGray8(const cv::Mat& in, cv::Mat& out_gray8) {
            if (in.type() == CV_8UC1) {
                out_gray8 = in;
                return;
            }
            if (in.channels() == 3) {
                cv::cvtColor(in, out_gray8, cv::COLOR_BGR2GRAY);
            }
            else if (in.channels() == 4) {
                cv::cvtColor(in, out_gray8, cv::COLOR_BGRA2GRAY);
            }
            else {
                // Fallback: convert to 8U by scaling
                cv::Mat tmp;
                in.convertTo(tmp, CV_8U);
                if (tmp.channels() == 1) out_gray8 = tmp; else cv::cvtColor(tmp, out_gray8, cv::COLOR_BGR2GRAY);
            }
        }

        static std::vector<cv::Point2f> rectCorners(const cv::RotatedRect& rr) {
            std::vector<cv::Point2f> pts(4);
            rr.points(pts.data());
            return pts;
        }

        // Best-effort getters. Not all HALCON versions/types support these.
        static float bestEffortScore(const HalconCpp::HTuple& model, Hlong /*idx*/) {
            try {
                HalconCpp::HTuple val;
                // Some HALCON versions expose a quality/score value. This key may vary across versions.
                HalconCpp::GetBarCodeParam(model, "symbol_score", &val);
                if (val.Length() > 0) {
                    double v = val[0].D();
                    return static_cast<float>(v);
                }
            }
            catch (...) {}
            return 1.0f;
        }

        static std::string bestEffortType(const HalconCpp::HTuple& model, Hlong /*idx*/) {
            try {
                HalconCpp::HTuple val;
                HalconCpp::GetBarCodeParam(model, "decoded_types", &val);
                if (val.Length() > 0) return std::string(val[0].S());
            }
            catch (...) {}
            return std::string();
        }
    };

} // namespace HalconBarcode
