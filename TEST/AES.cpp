//#include <opencv2/opencv.hpp>
//#include <iostream>
//
//using namespace cv;
//using namespace std;
//
///**
// * @brief 预处理：增强对比度 + 去噪
// */
//Mat preprocess(const Mat& src)
//{
//    Mat gray, blurImg, claheImg;
//
//    // 转灰度
//    cvtColor(src, gray, COLOR_BGR2GRAY);
//
//    // 中值滤波（抗反光噪声）
//    medianBlur(gray, blurImg, 5);
//
//    // CLAHE（解决局部对比度不均）
//    Ptr<CLAHE> clahe = createCLAHE(2.0, Size(8, 8));
//    clahe->apply(blurImg, claheImg);
//
//    return claheImg;
//}
//
///**
// * @brief 检测瓶口中心（使用霍夫圆）
// */
//Point2f detectBottleCenter(const Mat& img)
//{
//    vector<Vec3f> circles;
//
//    // 霍夫圆检测（只找大圆）
//    HoughCircles(img, circles, HOUGH_GRADIENT,
//        1.5,           // 分辨率
//        img.rows / 4,  // 最小距离
//        100, 30,       // Canny阈值
//        img.rows / 4,  // 最小半径
//        img.rows / 2); // 最大半径
//
//    if (!circles.empty())
//    {
//        return Point2f(circles[0][0], circles[0][1]);
//    }
//
//    // fallback：中心点
//    return Point2f(img.cols / 2.0f, img.rows / 2.0f);
//}
//
///**
// * @brief 极坐标展开
// */
//Mat polarTransform(const Mat& src, Point2f center)
//{
//    Mat polar;
//
//    int maxRadius = min(src.cols, src.rows) / 2;
//
//    // 极坐标变换
//    warpPolar(src, polar,
//        Size(360, maxRadius), // 宽=角度，高=半径
//        center,
//        maxRadius,
//        WARP_POLAR_LINEAR);
//
//    return polar;
//}
//
///**
// * @brief 检测圆环（核心算法）
// */
//vector<int> detectRings(const Mat& polar)
//{
//    Mat gradY;
//
//    // Sobel：检测“横向边缘”（对应圆环）
//    Sobel(polar, gradY, CV_32F, 0, 1, 3);
//
//    Mat absGrad;
//    convertScaleAbs(gradY, absGrad);
//
//    // 沿角度方向求平均（消除反光）
//    Mat projection;
//    reduce(absGrad, projection, 1, REDUCE_AVG);
//
//    // 平滑（去抖动）
//    GaussianBlur(projection, projection, Size(1, 9), 0);
//    vector<int> ringPositions;
//
//    // 峰值检测
//    for (int i = 5; i < projection.rows - 5; i++)
//    {
//        float val = projection.at<uchar>(i);
//
//        if (val > projection.at<uchar>(i - 1)&& 
//            val > 10) // 阈值（可调）
//        {
//            ringPositions.push_back(i);
//        }
//    }
//
//    return ringPositions;
//}
//
///**
// * @brief 主函数
// */
//int main()
//{
//    Mat src = imread("F:/IMG/NeckTop/天津/4#/NeckTop/110_2023_12_18_15_11_38_354.jpg");
//
//    if (src.empty())
//    {
//        cout << "图像加载失败" << endl;
//        return -1;
//    }
//
//    // Step1：预处理
//    Mat pre = preprocess(src);
//
//    // Step2：检测中心
//    Point2f center = detectBottleCenter(pre);
//
//    // Step3：极坐标展开
//    Mat polar = polarTransform(pre, center);
//
//    // Step4：检测圆环
//    vector<int> rings = detectRings(polar);
//
//    // 可视化
//    Mat result = src.clone();
//
//    for (int r : rings)
//    {
//        circle(result, center, r, Scalar(0, 0, 255), 2);
//    }
//
//    // 显示结果
//    imshow("原图", src);
//    imshow("极坐标", polar);
//    imshow("结果", result);
//
//    waitKey(0);
//    return 0;
//}