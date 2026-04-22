//#include <opencv2/opencv.hpp>
//#include <iostream>
//#include "DllExtern.h" 
//
//
//void TestTemplateCreation(const std::string& imagePath) {
//    // 1. 准备输入图像
//    cv::Mat inputImage = cv::imread(imagePath, cv::IMREAD_COLOR);
//    if (inputImage.empty()) {
//        std::cerr << "错误：无法加载图像 " << imagePath << std::endl;
//        return;
//    }
//
//
//    TemplateConfig config;
//    config.matchType = 1; // 形状匹配
//    config.roi = cv::Rect(257, 153, 769, 718); // 示例ROI区域
//    config.templateCenterX = 641;
//    config.templateCenterY = 512;
//    config.channels = 1; // 彩色图像
//    config.angleRange = { 0, 25.0 };
//    config.angleStep = 0.0;
//    config.scaleRange = { 0.9, 1.1 };
//    config.scaleStep = 0.0;
//    config.optimization = 1; // 平衡精度和速度[3]
//    config.metric = 1; // 使用极性
//    config.contrast = 40;
//    config.minContrast = 10;
//    config.numLevels = 10; 
//    config.extW = 100;
//    config.extH = 100; 
//    config.greediness = 0.8; 
//
//    // 3. 准备输出图像
//    cv::Mat outputImage;
//
//    // 4. 调用模板创建函数
//    int result = CR_DLL_CreatTemplate(inputImage, config, 0, &outputImage);
//
//    // 5. 处理结果
//    if (result == 1) {
//        std::cout << "模板创建成功！" << std::endl;
//
//        // 显示结果图像
//        cv::namedWindow("模板轮廓和ROI", cv::WINDOW_NORMAL);
//        cv::imshow("模板轮廓和ROI", outputImage);
//        cv::waitKey(0);
//
//        // 保存结果图像
//        cv::imwrite("Template_Result.jpg", outputImage);
//        std::cout << "结果已保存至 Template_Result.jpg" << std::endl;
//    }
//    else if (result == -1) {
//        std::cerr << "错误：无效输入参数" << std::endl;
//    }
//    else if (result == -2) {
//        std::cerr << "错误：模板创建失败（Halcon异常）" << std::endl;
//    }
//}
//
//int main() {
//    
//    TestTemplateCreation("C:/Users/admin/Desktop/1/12345.jpg");
//    return 0;
//}