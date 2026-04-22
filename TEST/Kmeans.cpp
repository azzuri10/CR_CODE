//#include <opencv2/opencv.hpp>
//#include <opencv2/ml.hpp>
//#include <iostream>
//#include <vector>
//#include <filesystem>
//
//namespace fs = std::filesystem;
//
//// 计算颜色直方图
//cv::Mat extractColorHistogram(const cv::Mat& image) {
//    cv::Mat hsvImage;
//    cv::cvtColor(image, hsvImage, cv::COLOR_BGR2HSV);
//
//    // 设置直方图参数
//    int hBins = 180, sBins = 256;
//    int histSize[] = { hBins, sBins };
//    float hRanges[] = { 0, 180 };
//    float sRanges[] = { 0, 256 };
//    const float* ranges[] = { hRanges, sRanges };
//    int channels[] = { 0, 1 };
//
//    // 计算直方图
//    cv::Mat hist;
//    cv::calcHist(&hsvImage, 1, channels, cv::Mat(), hist, 2, histSize, ranges, true, false);
//    cv::normalize(hist, hist, 0, 1, cv::NORM_MINMAX);
//
//    return hist.reshape(1, 1); // 将直方图展平为一行
//}
//
//// 计算颜色矩（均值、标准差、偏度）
//cv::Mat extractColorMoments(const cv::Mat& image) {
//    cv::Mat hsvImage;
//    cv::cvtColor(image, hsvImage, cv::COLOR_BGR2HSV);
//
//    // 计算均值和标准差
//    cv::Scalar mean, stddev;
//    cv::meanStdDev(hsvImage, mean, stddev);
//
//    // 将 stddev 转换为 cv::Mat 类型
//    cv::Mat stddevMat = (cv::Mat_<float>(1, 2) << stddev[0], stddev[1]);
//
//    // 计算偏度
//    cv::Mat meanMat = cv::Mat::zeros(hsvImage.size(), hsvImage.type());
//    meanMat.setTo(mean);
//    cv::Mat diff = hsvImage - meanMat;
//    cv::Mat diffPow3;
//    cv::pow(diff, 3.0, diffPow3); // 计算 diff 的三次方
//    cv::Scalar skewnessMean = cv::mean(diffPow3); // 计算 skewness 的均值
//
//    // 计算偏度值
//    cv::Mat skewnessValue = (cv::Mat_<float>(1, 2) <<
//        skewnessMean[0] / (std::pow(stddev[0], 3) + 1e-6),
//        skewnessMean[1] / (std::pow(stddev[1], 3) + 1e-6)
//        );
//
//    // 组合颜色矩
//    cv::Mat moments(1, 6, CV_32F);
//    moments.at<float>(0) = mean[0]; // H 均值
//    moments.at<float>(1) = mean[1]; // S 均值
//    moments.at<float>(2) = stddev[0]; // H 标准差
//    moments.at<float>(3) = stddev[1]; // S 标准差
//    moments.at<float>(4) = skewnessValue.at<float>(0); // H 偏度
//    moments.at<float>(5) = skewnessValue.at<float>(1); // S 偏度
//
//    return moments;
//}
//
//// 提取颜色特征（直方图 + 颜色矩）
//cv::Mat extractColorFeatures(const cv::Mat& image) {
//    cv::Mat hist = extractColorHistogram(image);
//    cv::Mat moments = extractColorMoments(image);
//
//    // 组合特征
//    cv::Mat features(1, hist.cols + moments.cols, CV_32F);
//    cv::hconcat(hist, moments, features);
//    return features;
//}
//
//int main() {
//    std::string datasetPath = "E:/TRAIN_DATA/CAP_CLS_YH"; // 图像文件夹路径
//    int numClusters = 5; // 聚类数量
//
//    // 读取图像并提取特征
//    std::vector<cv::Mat> features;
//    std::vector<std::string> imagePaths;
//    for (const auto& entry : fs::directory_iterator(datasetPath)) {
//        if (fs::is_directory(entry.path())) {
//            for (const auto& imgEntry : fs::directory_iterator(entry.path())) {
//                cv::Mat image = cv::imread(imgEntry.path().string());
//                if (image.empty()) continue;
//
//                cv::Mat feature = extractColorFeatures(image);
//                features.push_back(feature);
//                imagePaths.push_back(imgEntry.path().string());
//            }
//        }
//    }
//
//    // 将特征转换为 OpenCV 的 Mat 格式
//    cv::Mat featureMatrix(features.size(), features[0].cols, features[0].type());
//    for (size_t i = 0; i < features.size(); i++) {
//        features[i].copyTo(featureMatrix.row(i));
//    }
//
//    // 使用 K-Means 聚类
//    cv::Mat labels, centers;
//    cv::kmeans(featureMatrix, numClusters, labels,
//        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 100, 0.01),
//        3, cv::KMEANS_PP_CENTERS, centers);
//
//    // 在图像上显示聚类结果
//    for (int i = 0; i < labels.rows; i++) {
//        int clusterLabel = labels.at<int>(i);
//        std::string imagePath = imagePaths[i];
//
//        // 加载图像
//        cv::Mat image = cv::imread(imagePath);
//        if (image.empty()) continue;
//
//        // 在图像上添加聚类标签
//        std::string labelText = "Cluster: " + std::to_string(clusterLabel);
//        cv::putText(image, labelText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
//
//        // 显示图像
//        cv::imshow("Clustered Image", image);
//        cv::waitKey(0); // 按任意键继续
//    }
//
//    return 0;
//}