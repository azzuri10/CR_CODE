//#include <iostream>
//#include <vector>
//#include <opencv2/opencv.hpp>
//#include <opencv2/calib3d.hpp>
//
//using namespace cv;
//using namespace std;
//
//// 预设的固定变换矩阵（基于环形阵列相机的物理位置）
//vector<Mat> getFixedHomographies(const vector<Mat>& images) {
//    vector<Mat> homographies(4);
//    const float scale = 0.95f;  // 透视缩放因子
//    const float horizontal_overlap = 0.3f;  // 水平重叠比例
//
//    // 相机0（基准）
//    homographies[0] = (Mat_<double>(3, 3) <<
//        1.0, 0.0, 0.0,
//        0.0, 1.0, 0.0,
//        0.0, 0.0, 1.0);
//
//    // 相机1（右侧相机）
//    homographies[1] = (Mat_<double>(3, 3) <<
//        scale, 0.0, -images[0].cols * horizontal_overlap,
//        0.0, scale, 0.0,
//        0.0, 0.0, 1.0);
//
//    // 相机2（后方相机）
//    homographies[2] = (Mat_<double>(3, 3) <<
//        scale, 0.0, -images[0].cols * horizontal_overlap * 2,
//        0.0, scale, 0.0,
//        0.0, 0.0, 1.0);
//
//    // 相机3（左侧相机 - 针对圆柱形容器优化）
//    homographies[3] = (Mat_<double>(3, 3) <<
//        scale, 0.0, images[0].cols * horizontal_overlap,
//        0.0, scale, 0.0,
//        0.0, 0.0, 1.0);
//
//    return homographies;
//}
//
//// 简化的圆柱形容器保护掩码
//Mat createCylinderMask(const Mat& img) {
//    Mat mask = Mat::zeros(img.size(), CV_8U);
//    Point center(img.cols / 2, img.rows / 2);
//    int radius = min(img.cols, img.rows) * 0.4;
//
//    // 创建圆形区域掩码
//    circle(mask, center, radius, Scalar(255), -1);
//
//    // 矩形区域作为备选
//    if (countNonZero(mask) < mask.total() * 0.3) {
//        mask.setTo(0);
//        Rect rect(center.x - radius, center.y - radius, radius * 2, radius * 2);
//        rectangle(mask, rect, Scalar(255), -1);
//    }
//    return mask;
//}
//
//// 固定参数拼接
//Mat fixedStitching(const vector<Mat>& images) {
//    // 1. 获取预设变换矩阵
//    vector<Mat> homographies = getFixedHomographies(images);
//
//    // 2. 计算全景图尺寸
//    vector<Point2f> corners;
//    for (int i = 0; i < images.size(); i++) {
//        vector<Point2f> img_corners = {
//            Point2f(0, 0),
//            Point2f(images[i].cols, 0),
//            Point2f(images[i].cols, images[i].rows),
//            Point2f(0, images[i].rows)
//        };
//
//        vector<Point2f> transformed_corners;
//        perspectiveTransform(img_corners, transformed_corners, homographies[i]);
//
//        for (const auto& pt : transformed_corners) {
//            corners.push_back(pt);
//        }
//    }
//
//    // 3. 计算全景图边界
//    Rect bbox = boundingRect(corners);
//    Mat panorama = Mat::zeros(bbox.height, bbox.width, CV_8UC3);
//    Mat weight_map = Mat::zeros(bbox.height, bbox.width, CV_32F);
//
//    // 4. 处理每张图像
//    for (int i = 0; i < images.size(); i++) {
//        // 调整变换矩阵
//        Mat H = homographies[i].clone();
//        H.at<double>(0, 2) -= bbox.x;
//        H.at<double>(1, 2) -= bbox.y;
//
//        // 特别处理圆柱形容器（相机3）
//        Mat mask;
//        if (i == 3) { // 相机3的圆柱形容器
//            mask = createCylinderMask(images[i]);
//        }
//        else {
//            mask = Mat::ones(images[i].size(), CV_8U) * 255;
//        }
//
//        // 变换图像和掩码
//        Mat warped_img, warped_mask;
//        warpPerspective(images[i], warped_img, H, panorama.size());
//        warpPerspective(mask, warped_mask, H, panorama.size(), INTER_NEAREST);
//
//        // 累积混合
//        for (int y = 0; y < panorama.rows; y++) {
//            for (int x = 0; x < panorama.cols; x++) {
//                // 仅处理warped_mask有值的区域
//                if (warped_mask.at<uchar>(y, x) > 0) {
//                    Vec3b color = warped_img.at<Vec3b>(y, x);
//                    float weight = warped_mask.at<uchar>(y, x) / 255.0f;
//
//                    // 对于容器区域直接覆盖（相机3）
//                    if (i == 3) {
//                        panorama.at<Vec3b>(y, x) = color;
//                        weight_map.at<float>(y, x) = 1.0f;
//                    }
//                    // 其他区域进行混合
//                    else {
//                        float old_weight = weight_map.at<float>(y, x);
//                        Vec3b old_color = panorama.at<Vec3b>(y, x);
//
//                        // 混合权重计算
//                        float total_weight = old_weight + weight;
//                        Vec3b new_color;
//                        new_color[0] = (old_color[0] * old_weight + color[0] * weight) / total_weight;
//                        new_color[1] = (old_color[1] * old_weight + color[1] * weight) / total_weight;
//                        new_color[2] = (old_color[2] * old_weight + color[2] * weight) / total_weight;
//
//                        panorama.at<Vec3b>(y, x) = new_color;
//                        weight_map.at<float>(y, x) = total_weight;
//                    }
//                }
//            }
//        }
//    }
//
//    return panorama;
//}
//
//int main() {
//    // 1. 读取4个相机图像
//    vector<Mat> images;
//    for (int i = 0; i < 4; i++) {
//        string filename = "C:/Users/admin/Desktop/LABEL360/" + to_string(i) + ".jpg";
//        Mat img = imread(filename);
//        if (img.empty()) {
//            cerr << "错误: 无法读取图像 " << filename << endl;
//            return -1;
//        }
//        images.push_back(img);
//
//        // 为调试保存输入图像
//        imwrite("input_" + to_string(i) + ".jpg", img);
//    }
//
//    // 2. 固定参数拼接
//    Mat panorama = fixedStitching(images);
//
//    // 3. 保存结果
//    if (!panorama.empty()) {
//        // 裁剪黑边
//        Mat gray;
//        cvtColor(panorama, gray, COLOR_BGR2GRAY);
//        Rect bbox = boundingRect(gray > 1);
//        Mat cropped = panorama(bbox).clone();
//
//        imwrite("fixed_panorama_result.jpg", cropped);
//        cout << "全景图已保存为 fixed_panorama_result.jpg" << endl;
//
//        // 显示结果
//        namedWindow("Fixed Panorama Result", WINDOW_NORMAL);
//        imshow("Fixed Panorama Result", cropped);
//        waitKey(0);
//    }
//    else {
//        cerr << "错误: 未能生成全景图" << endl;
//        return -1;
//    }
//
//    return 0;
//}