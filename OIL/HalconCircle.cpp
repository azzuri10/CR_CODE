//#include <opencv2/opencv.hpp>
//#include <cmath>
//
//using namespace cv;
//using namespace std;
//
//int main() {
//    // 读取图像
//    Mat src = imread("000.jpg");
//    if (src.empty()) {
//        cout << "无法读取图像" << endl;
//        return -1;
//    }
//
//    // 设置旋转角度（弧度）
//    double angle = -30.0 * CV_PI / 180.0; // 30度
//
//    // 创建输出图像
//    Mat dst = Mat::zeros(src.size(), src.type());
//
//    // 获取图像宽度和高度
//    int width = src.cols;
//    int height = src.rows;
//
//    // 创建透视变换后的图像
//    Mat warpedImage = Mat::zeros(src.size(), src.type());
//
//    // 对整个图像应用透视变换（反向映射）
//    for (int y = 0; y < height; y++) {
//        for (int x = 0; x < width; x++) {
//            // 计算当前点到中轴的距离（归一化）
//            double distFromCenter = (x - width / 2.0) / (width / 2.0);
//
//            // 计算原始坐标（反向映射）
//            double origX = width / 2.0 + (x - width / 2.0) / cos(angle);
//            double origY = y - (height / 2.0 - y) * distFromCenter * tan(angle);
//
//            // 确保坐标在图像范围内
//            if (origX >= 0 && origX < width && origY >= 0 && origY < height) {
//                // 双线性插值
//                int x1 = floor(origX);
//                int y1 = floor(origY);
//                int x2 = ceil(origX);
//                int y2 = ceil(origY);
//
//                // 边界检查
//                x2 = min(x2, width - 1);
//                y2 = min(y2, height - 1);
//
//                // 计算权重
//                double dx = origX - x1;
//                double dy = origY - y1;
//
//                // 获取四个相邻像素
//                Vec3b p11 = src.at<Vec3b>(y1, x1);
//                Vec3b p12 = src.at<Vec3b>(y1, x2);
//                Vec3b p21 = src.at<Vec3b>(y2, x1);
//                Vec3b p22 = src.at<Vec3b>(y2, x2);
//
//                // 双线性插值计算新像素值
//                Vec3b interpolated =
//                    p11 * (1 - dx) * (1 - dy) +
//                    p12 * dx * (1 - dy) +
//                    p21 * (1 - dx) * dy +
//                    p22 * dx * dy;
//
//                warpedImage.at<Vec3b>(y, x) = interpolated;
//            }
//        }
//    }
//
//    // 显示结果
//    imshow("Original", src);
//    imshow("Rotated Plane", warpedImage);
//    waitKey(0);
//
//    return 0;
//}