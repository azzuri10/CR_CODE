//#include <opencv2/opencv.hpp>
//#include <iostream>
//#include <vector>
//#include <cmath>
//
//using namespace cv;
//using namespace std;
//
//// 曲线拟合和矫正函数（优化版）
//Mat straightenCurvedTextWithPointsOptimized(const Mat& input,
//    const Point& startPoint,
//    const Point& midPoint,
//    const Point& endPoint,
//    double samplingHeightRatio = 0.99) {
//
//    // 1. 使用三点拟合抛物线 (y = ax² + bx + c)
//    vector<Point2f> points = {
//        Point2f(startPoint.x, startPoint.y),
//        Point2f(midPoint.x, midPoint.y),
//        Point2f(endPoint.x, endPoint.y)
//    };
//
//    // 构建方程组矩阵
//    Mat A = (Mat_<float>(3, 3) <<
//        points[0].x * points[0].x, points[0].x, 1,
//        points[1].x * points[1].x, points[1].x, 1,
//        points[2].x * points[2].x, points[2].x, 1);
//
//    Mat B = (Mat_<float>(3, 1) <<
//        points[0].y,
//        points[1].y,
//        points[2].y);
//
//    Mat coeffs;
//    solve(A, B, coeffs, DECOMP_SVD);
//
//    float a = coeffs.at<float>(0);
//    float b = coeffs.at<float>(1);
//    float c = coeffs.at<float>(2);
//
//    // 2. 计算曲线上的点（使用浮点精度）
//    int minX = min(startPoint.x, endPoint.x);
//    int maxX = max(startPoint.x, endPoint.x);
//    int curveWidth = maxX - minX + 1;
//
//    vector<Point2f> curvePoints(curveWidth);
//    vector<float> derivatives(curveWidth);
//    vector<float> curveLengths(curveWidth);
//
//    // 预计算曲线点、导数和累积长度
//    curvePoints[0] = Point2f(minX, a * minX * minX + b * minX + c);
//    curveLengths[0] = 0.0f;
//
//    for (int i = 1; i < curveWidth; i++) {
//        int x = minX + i;
//        float y = a * x * x + b * x + c;
//        curvePoints[i] = Point2f(x, y);
//
//        // 计算导数
//        derivatives[i] = 2 * a * x + b;
//
//        // 计算累积长度
//        float segmentLength = norm(curvePoints[i] - curvePoints[i - 1]);
//        curveLengths[i] = curveLengths[i - 1] + segmentLength;
//    }
//
//    double curveLength = curveLengths[curveWidth - 1];
//
//    // 3. 计算采样高度（基于图像高度）
//    int samplingHeight = static_cast<int>(input.rows * samplingHeightRatio);
//
//    // 4. 创建输出图像（与输入尺寸相同）
//    Mat output = input.clone();
//
//    // 5. 创建曲线区域的ROI - 确保不超出边界
//    int centerY = (startPoint.y + endPoint.y) / 2;
//    int roiY = centerY - samplingHeight / 2;
//
//    // 调整ROI的y坐标，确保不超出图像边界
//    if (roiY < 0) roiY = 0;
//    if (roiY + samplingHeight > input.rows) {
//        samplingHeight = input.rows - roiY;
//    }
//
//    Rect curveROI(minX, roiY, curveWidth, samplingHeight);
//
//    // 6. 创建映射表（提高性能）
//    Mat mapX(curveROI.size(), CV_32F);
//    Mat mapY(curveROI.size(), CV_32F);
//
//    // 7. 预计算法向量
//    vector<Point2f> normals(curveWidth);
//    for (int i = 0; i < curveWidth; i++) {
//        Point2f tangent(1.0f, derivatives[i]);
//        float length = norm(tangent);
//        if (length > 1e-5) {
//            normals[i] = Point2f(-tangent.y / length, tangent.x / length);
//        }
//        else {
//            normals[i] = Point2f(0, 1); // 垂直方向
//        }
//    }
//
//    // 8. 并行计算映射关系
//    parallel_for_(Range(0, curveROI.height), [&](const Range& range) {
//        for (int y = range.start; y < range.end; y++) {
//            for (int x = 0; x < curveROI.width; x++) {
//                // 计算当前点在曲线上的位置
//                double ratio = static_cast<double>(x) / curveROI.width;
//                double targetLength = ratio * curveLength;
//
//                // 使用二分查找快速定位曲线段
//                int low = 0, high = curveWidth - 1;
//                while (low <= high) {
//                    int mid = low + (high - low) / 2;
//                    if (curveLengths[mid] < targetLength) {
//                        low = mid + 1;
//                    }
//                    else {
//                        high = mid - 1;
//                    }
//                }
//
//                // 确保索引在有效范围内
//                int index = min(max(high, 0), curveWidth - 2);
//
//                // 插值位置
//                double segRatio = (targetLength - curveLengths[index]) /
//                    (curveLengths[index + 1] - curveLengths[index]);
//                segRatio = max(0.0, min(1.0, segRatio));
//
//                Point2f curvePoint = curvePoints[index] +
//                    (curvePoints[index + 1] - curvePoints[index]) * segRatio;
//
//                // 插值法向量 - 修复归一化问题
//                Point2f normal = normals[index] +
//                    (normals[index + 1] - normals[index]) * segRatio;
//
//                // 手动归一化法向量
//                float n = norm(normal);
//                if (n > 1e-5) {
//                    normal.x /= n;
//                    normal.y /= n;
//                }
//                else {
//                    normal = Point2f(0, 1); // 垂直方向
//                }
//
//                // 计算采样点 - 沿法线方向
//                int offsetY = y - curveROI.height / 2;
//                Point2f srcPoint = curvePoint + normal * offsetY;
//
//                // 存储映射
//                mapX.at<float>(y, x) = srcPoint.x;
//                mapY.at<float>(y, x) = srcPoint.y;
//            }
//        }
//        });
//
//    // 9. 应用映射（使用OpenCV优化函数）
//    Mat roiOutput = output(curveROI);
//    remap(input(curveROI), roiOutput, mapX, mapY, INTER_LINEAR);
//
//    return output;
//}
//
//int main() {
//    // 读取输入图像
//    Mat input = imread("D:/CONFIG_CR_3.0/Code/ALL/BASIC/2025_08_20_08_05_41_483_13_11.jpg");
//    if (input.empty()) {
//        cerr << "无法读取图像" << endl;
//        return -1;
//    }
//
//    // 用户提供的三个点（起点、中间点、终点）
//    Point startPoint(0, input.rows * 0.45);  // 起点
//    Point midPoint(input.cols / 2, input.rows * 0.25);  // 中间点
//    Point endPoint(input.cols - 1, input.rows * 0.45);  // 终点
//
//    // 测量执行时间
//    int64 start = getTickCount();
//
//    // 矫正图像（保持原始尺寸）
//    Mat result = straightenCurvedTextWithPointsOptimized(input, startPoint, midPoint, endPoint);
//
//    double duration = (getTickCount() - start) / getTickFrequency();
//    cout << "处理时间: " << duration * 1000 << " ms" << endl;
//
//    // 显示结果
//    imshow("原始图像", input);
//    imshow("矫正结果", result);
//    waitKey(0);
//
//    // 保存结果
//    imwrite("straightened_code_optimized.jpg", result);
//
//    return 0;
//}