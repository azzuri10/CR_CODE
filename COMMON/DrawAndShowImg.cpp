#include "DrawAndShowImg.h"

DrawAndShowImg::DrawAndShowImg() {
    ANA = new AnalyseMat;
}

DrawAndShowImg::~DrawAndShowImg() {
    delete ANA;
};

bool DrawAndShowImg::DAS_Rect(cv::Mat img,
    const cv::Rect& rect,
    const std::string& fileName,
    bool isSaveImg) {
    // 提前返回条件优先判断
    if (!isSaveImg) return true;

    // 强化参数校验
    if (img.empty() || fileName.empty())
        return false;

    // 验证矩形有效性（完全在图像范围内）
    if (rect.x < 0 || rect.y < 0 ||
        rect.width <= 0 || rect.height <= 0 ||
        (rect.x + rect.width) > img.cols ||
        (rect.y + rect.height) > img.rows) {
        return false;
    }

    // 统一图像处理流程
    try {
        // 优化通道转换逻辑
        if (img.channels() != 3) {
            cv::cvtColor(img, img, cv::COLOR_GRAY2BGR);
        }

        // 使用明确的绘图参数
        const int thickness = 1;
        const int lineType = cv::LINE_AA;  // 改用抗锯齿线型
        cv::rectangle(img, rect,
            Colors::GREEN,   // 确保是cv::Scalar(0,255,0)
            thickness,
            lineType);

        // 强化文件保存结果检查
        if (!cv::imwrite(fileName, img)) {
            std::cerr << "Failed to write: " << fileName << std::endl;
            return false;
        }
    }
    catch (const cv::Exception& e) {
        // 捕获OpenCV异常
        std::cerr << "OpenCV error: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        // 捕获其他异常
        std::cerr << "Unknown error occurred" << std::endl;
        return false;
    }

    return true;
}

bool DrawAndShowImg::DAS_FinsObject(const cv::Mat& img,
    const std::vector<FinsObject>& details,
    const std::string& fileName,
    bool isSaveImg) {
    // 提前返回条件优先判断
    if (!isSaveImg) return true;

    // 强化参数校验
    if (img.empty() || fileName.empty())
        return false;


    // 统一图像处理流程
    try {
        for (int i = 0; i < details.size(); i++)
        {
            // 验证矩形有效性（完全在图像范围内）
            if (details[i].box.x < 0 || details[i].box.y < 0 ||
                details[i].box.width <= 0 || details[i].box.height <= 0 ||
                (details[i].box.x + details[i].box.width) > img.cols ||
                (details[i].box.y + details[i].box.height) > img.rows) {
                return false;
            }

            // 优化通道转换逻辑
            if (img.channels() != 3) {
                cv::cvtColor(img, img, cv::COLOR_GRAY2BGR);
            }

            // 使用明确的绘图参数
            const int thickness = 1;
            const int lineType = cv::LINE_AA;  // 改用抗锯齿线型
            cv::rectangle(img, details[i].box,
                Colors::GREEN,   // 确保是cv::Scalar(0,255,0)
                thickness,
                lineType);

            // 强化文件保存结果检查
            if (!cv::imwrite(fileName, img)) {
                std::cerr << "Failed to write: " << fileName << std::endl;
                return false;
            }
        }
        
    }
    catch (const cv::Exception& e) {
        // 捕获OpenCV异常
        std::cerr << "OpenCV error: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        // 捕获其他异常
        std::cerr << "Unknown error occurred" << std::endl;
        return false;
    }

    return true;
}

bool DrawAndShowImg::DAS_FinsObjectObb(const cv::Mat& img,
    const std::vector<FinsObjectRotate>& details,
    const std::string& fileName,
    bool isSaveImg) {
    // 提前返回条件优先判断
    if (!isSaveImg) return true;

    // 强化参数校验
    if (img.empty() || fileName.empty())
        return false;


    // 统一图像处理流程
    try {
        for (int i = 0; i < details.size(); i++)
        {
            // 验证矩形有效性（完全在图像范围内）
           /* if (details[i].box.x < 0 || details[i].box.y < 0 ||
                details[i].box.width <= 0 || details[i].box.height <= 0 ||
                (details[i].box.x + details[i].box.width) > img.cols ||
                (details[i].box.y + details[i].box.height) > img.rows) {
                return false;
            }*/

            // 优化通道转换逻辑
            if (img.channels() != 3) {
                cv::cvtColor(img, img, cv::COLOR_GRAY2BGR);
            }

            // 使用明确的绘图参数
            const int thickness = 1;
            const int lineType = cv::LINE_AA;  // 改用抗锯齿线型
            
            cv::Point2f vertices[4];
            details[i].box.points(vertices);
            for (int i = 0; i < 4; i++) {
                cv::line(img, vertices[i], vertices[(i + 1) % 4], Colors::GREEN,
                    thickness,
                    lineType);
            }

            // 强化文件保存结果检查
            if (!cv::imwrite(fileName, img)) {
                std::cerr << "Failed to write: " << fileName << std::endl;
                return false;
            }
        }

    }
    catch (const cv::Exception& e) {
        // 捕获OpenCV异常
        std::cerr << "OpenCV error: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        // 捕获其他异常
        std::cerr << "Unknown error occurred" << std::endl;
        return false;
    }

    return true;
}

bool DrawAndShowImg::DAS_HoughLineP(cv::Mat img, cv::Vec4i lineP, std::string fileName, bool isSaveImg)
{
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }


    line(imgCopy, cv::Point(lineP[0], lineP[1]), cv::Point(lineP[2], lineP[3]), Colors::GREEN, 3, cv::LINE_AA);

    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_HoughLinesP(cv::Mat img, std::vector<cv::Vec4i> linesP, std::string fileName, bool isSaveImg)
{
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }

    for (size_t i = 0; i < linesP.size(); i++)
    {
        cv::Vec4f hline = linesP[i];
        line(imgCopy, cv::Point(hline[0], hline[1]), cv::Point(hline[2], hline[3]), Colors::GREEN, 3, cv::LINE_AA);
    }

    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_HoughLinesP0(cv::Mat img, cv::Vec4f linesP, std::string fileName, bool isSaveImg)
{
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }

    line(imgCopy, cv::Point(linesP[0], linesP[1]), cv::Point(linesP[2], linesP[3]), Colors::GREEN, 3, cv::LINE_AA);

    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_HoughLinesP(cv::Mat img, std::vector<cv::Vec4f> linesP, std::string fileName, bool isSaveImg)
{
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }

    for (size_t i = 0; i < linesP.size(); i++)
    {
        cv::Vec4f hline = linesP[i];
        line(imgCopy, cv::Point(hline[0], hline[1]), cv::Point(hline[2], hline[3]), Colors::GREEN, 3, cv::LINE_AA);
    }

    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_Img(cv::Mat img, std::string fileName, bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    imwrite(fileName, img);
    return true;
}

bool DrawAndShowImg::DAS_BreakArea(cv::Mat img, BreakSize breakArea, int bnd,
    std::string fileName, bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    cv::Rect showRect;
    ANA->ChangeRectBnd(breakArea.rect, bnd, bnd, showRect);
    rectangle(imgCopy, showRect, Colors::GREEN, 1, 8, 0);
    char text[100];
    sprintf(text, "W%d H%d S%d", breakArea.rect.width, breakArea.rect.height,
        breakArea.size);
    putText(imgCopy, text, cv::Point2f(showRect.x, showRect.y - 10),
        cv::FONT_HERSHEY_SIMPLEX, 0.7, Colors::GREEN, 2, 8, false);
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_BreakAreas(cv::Mat img, std::vector<BreakSize> breakAreas,
    int bnd, std::string fileName, bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < breakAreas.size(); i++) {
        cv::Rect showRect;
        ANA->ChangeRectBnd(breakAreas[i].rect, bnd, bnd, showRect);
        rectangle(imgCopy, showRect, Colors::GREEN, 1, 8, 0);
        char text_H[100];
        char text_W[100];
        char text_S[100];
        sprintf(text_W, "W%d", breakAreas[i].rect.width);
        sprintf(text_H, "H%d", breakAreas[i].rect.height);
        sprintf(text_S, "S%d", breakAreas[i].size);
        putText(imgCopy, text_W, cv::Point2f(showRect.x, showRect.y - 25),
            cv::FONT_HERSHEY_SIMPLEX, 0.35, Colors::GREEN, 1, 8, false);
        putText(imgCopy, text_H, cv::Point2f(showRect.x, showRect.y - 15),
            cv::FONT_HERSHEY_SIMPLEX, 0.35, Colors::GREEN, 1, 8, false);
        putText(imgCopy, text_S, cv::Point2f(showRect.x, showRect.y - 5),
            cv::FONT_HERSHEY_SIMPLEX, 0.35, Colors::GREEN, 1, 8, false);
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_BreakAreasPoints(cv::Mat img, std::vector <int> points, std::vector<BreakSize> breakAreas,
    int bnd, std::string fileName, bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }

    for (int i = 0; i < points.size(); i++) {
        imgCopy.at<cv::Vec3b>(i, points[i])[0] = 0;
        imgCopy.at<cv::Vec3b>(i, points[i])[1] = 0;
        imgCopy.at<cv::Vec3b>(i, points[i])[2] = 255;
    }

    for (int i = 0; i < breakAreas.size(); i++) {
        cv::Rect showRect;
        ANA->ChangeRectBnd(breakAreas[i].rect, bnd, bnd, showRect);
        rectangle(imgCopy, showRect, Colors::GREEN, 1, 8, 0);
        char text_H[100];
        char text_W[100];
        char text_S[100];
        sprintf(text_W, "W%d", breakAreas[i].rect.width);
        sprintf(text_H, "H%d", breakAreas[i].rect.height);
        sprintf(text_S, "S%d", breakAreas[i].size);
        putText(imgCopy, text_W, cv::Point2f(showRect.x, showRect.y - 25),
            cv::FONT_HERSHEY_SIMPLEX, 0.35, Colors::GREEN, 1, 8, false);
        putText(imgCopy, text_H, cv::Point2f(showRect.x, showRect.y - 15),
            cv::FONT_HERSHEY_SIMPLEX, 0.35, Colors::GREEN, 1, 8, false);
        putText(imgCopy, text_S, cv::Point2f(showRect.x, showRect.y - 5),
            cv::FONT_HERSHEY_SIMPLEX, 0.35, Colors::GREEN, 1, 8, false);
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_Circle(cv::Mat img, cv::Vec3f drawCircle, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    circle(imgCopy, cv::Point2f(drawCircle[0], drawCircle[1]), drawCircle[2], Colors::RED, 2,
        8, 0);
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_Circles(cv::Mat img, std::vector<cv::Vec4f> drawCircles,
    std::string fileName, bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < drawCircles.size(); i++) {
        cv::Vec4f val = drawCircles[i];
        circle(imgCopy, cv::Point2f(val[0], val[1]), val[2], Colors::RED, 2, 8, 0);
        char radius[100];
        sprintf(radius, "R %.1f", val[2]);
        putText(imgCopy, radius,
            cv::Point2f(val[0] - val[2] * 0.3, val[1] - val[2] * 0.6),
            cv::FONT_HERSHEY_SIMPLEX, 0.5, Colors::BLUE, 1);
        char score[100];
        sprintf(score, "V %.0f", val[3]);
        putText(imgCopy, score,
            cv::Point2f(val[0] - val[2] * 0.3, val[1] + val[2] * 0.6),
            cv::FONT_HERSHEY_SIMPLEX, 0.5, Colors::BLUE, 1);
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_CirclesScore(cv::Mat img, std::vector<cv::Vec4f> drawCircles,
    std::vector<float> scores, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < drawCircles.size(); i++) {
        cv::Vec4f val = drawCircles[i];
        circle(imgCopy, cv::Point2f(val[0], val[1]), val[2], Colors::RED, 2, 8, 0);
        char radius[100];
        sprintf(radius, "R %.1f", val[2]);
        putText(imgCopy, radius,
            cv::Point2f(val[0] - val[2] * 0.3, val[1] - val[2] * 0.6),
            cv::FONT_HERSHEY_SIMPLEX, 0.5, Colors::BLUE, 1);
        char vote[100];
        sprintf(vote, "V %.0f", val[3]);
        putText(imgCopy, vote, cv::Point2f(val[0] - val[2] / 3, val[1]),
            cv::FONT_HERSHEY_SIMPLEX, 0.5, Colors::BLUE, 1);
        char score[100];
        sprintf(score, "C %.1f", scores[i]);
        putText(imgCopy, score,
            cv::Point2f(val[0] - val[2] * 0.3, val[1] + val[2] * 0.6),
            cv::FONT_HERSHEY_SIMPLEX, 0.5, Colors::BLUE, 1);
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_Counters(cv::Mat img, std::vector<std::vector<cv::Point>> contours,
    std::string fileName, bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    if (contours.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    drawContours(imgCopy, contours, -1, Colors::GREEN, 1);
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_DynamicLines(cv::Mat img, std::vector<int> leftList,
    std::vector<int> rightList, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    if (leftList.empty() || rightList.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < leftList.size(); i++) {
        imgCopy.at<cv::Vec3b>(i, leftList[i])[0] = 0;
        imgCopy.at<cv::Vec3b>(i, leftList[i])[1] = 0;
        imgCopy.at<cv::Vec3b>(i, leftList[i])[2] = 255;
    }
    for (int i = 0; i < rightList.size(); i++) {
        imgCopy.at<cv::Vec3b>(i, rightList[i])[0] = 0;
        imgCopy.at<cv::Vec3b>(i, rightList[i])[1] = 0;
        imgCopy.at<cv::Vec3b>(i, rightList[i])[2] = 255;
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_Line(cv::Mat img, CALC_LinePara drawLine, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    circle(imgCopy, drawLine.pt0, 2, Colors::GREEN, 2, 8, 0);
    circle(imgCopy, drawLine.pt1, 2, Colors::GREEN, 2, 8, 0);
    line(imgCopy, drawLine.pt0, drawLine.pt1, Colors::RED, 2, 8, 0);
    cv::Point2f ptMed((drawLine.pt0.x + drawLine.pt1.x) / 2,
        (drawLine.pt0.y + drawLine.pt1.y) / 2);
    char text[100];
    sprintf(text, "%.1f", drawLine.angle);
    putText(imgCopy, text, ptMed, cv::FONT_HERSHEY_SIMPLEX, 0.5, Colors::GREEN, 1, 8, false);
    imwrite(fileName, imgCopy);
    return true;
}

bool DAS_Line(cv::Mat img, cv::Point pt0, cv::Point pt1, std::string fileName, bool isSaveImg)
{
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }

    line(imgCopy, pt0, pt1, Colors::GREEN, 1, 8, 0);

    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_Lines(cv::Mat img, std::vector<CALC_LinePara> drawLines,
    std::string fileName, bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < drawLines.size(); i++) {
        circle(imgCopy, drawLines[i].pt0, 2, Colors::GREEN, 2, 8, 0);
        circle(imgCopy, drawLines[i].pt1, 2, Colors::GREEN, 2, 8, 0);
        line(imgCopy, drawLines[i].pt0, drawLines[i].pt1, Colors::RED, 2, 8, 0);
        cv::Point2f ptMed((drawLines[i].pt0.x + drawLines[i].pt1.x) / 2,
            (drawLines[i].pt0.y + drawLines[i].pt1.y) / 2);
        char text[100];
        sprintf(text, "%.1f", drawLines[i].angle);
        putText(imgCopy, text, ptMed, cv::FONT_HERSHEY_SIMPLEX, 0.5, Colors::GREEN, 1, 8,
            false);
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_Lines(cv::Mat img, std::vector<cv::Vec2f> lines, std::string fileName, bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    
    for (int i = 0; i < lines.size(); i++)
    {
        float rho = lines[i][0], theta = lines[i][1];
        cv::Point pt1, pt2;
        double a = cos(theta), b = sin(theta);
        double x0 = a * rho, y0 = b * rho;
        pt1.x = cvRound(x0 + 2000 * (-b));  //把浮点数转化成整数
        pt1.y = cvRound(y0 + 2000 * (a));
        pt2.x = cvRound(x0 - 2000 * (-b));
        pt2.y = cvRound(y0 - 2000 * (a));

        line(imgCopy, pt1, pt2, Colors::GREEN, 1, 8, 0);
    }
   

    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_Match(cv::Mat img0, std::vector<cv::KeyPoint> point0, cv::Mat img1,
    std::vector<cv::KeyPoint> point1, std::vector<cv::DMatch> matches, std::string fileName, bool isSaveImg)
{
    if (!isSaveImg) {
        return true;
    }
    if (img0.empty() || img1.empty()) {
        return false;
    }
    if (point1.empty() || matches.empty()) {
        return false;
    }

    cv::Mat imgMatch;
    drawMatches(img0, point0, img1, point1, matches, imgMatch, cv::Scalar::all(-1),
        cv::Scalar::all(-1), std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
    imwrite(fileName, imgMatch);
    return true;
}

bool DrawAndShowImg::DAS_MatchHalcon(cv::Mat img, std::vector<cv::Point2f> dstCorners, double score, double angle, std::string fileName, bool isSaveImg)
{
    if (!isSaveImg) {
        return true;
    }
    if (img.empty() || dstCorners.size() != 4) {
        return false;
    }

    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }

    std::stringstream matchScores, matchAngle;
    matchScores << std::fixed << std::setprecision(2) << score; // 设置小数点后的位数为两位
    matchAngle << std::fixed << std::setprecision(2) << angle;

    cv::putText(imgCopy, "Score: " + matchScores.str(), cv::Point(dstCorners[0].x, dstCorners[0].y - 8), cv::FONT_HERSHEY_SIMPLEX, 0.8, Colors::GREEN, 2);
    cv::putText(imgCopy, "Angle: " + matchAngle.str(), cv::Point(dstCorners[0].x, dstCorners[0].y - 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, Colors::GREEN, 2);
    cv::line(imgCopy, dstCorners[0], dstCorners[1], Colors::GREEN, 3);
    cv::line(imgCopy, dstCorners[1], dstCorners[2], Colors::GREEN, 3);
    cv::line(imgCopy, dstCorners[2], dstCorners[3], Colors::GREEN, 3);
    cv::line(imgCopy, dstCorners[3], dstCorners[0], Colors::GREEN, 3);

    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_Point(cv::Mat img, cv::Point point, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    circle(imgCopy, point, 3, Colors::GREEN, -1, 8, 0);
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_Points(cv::Mat img, std::vector<cv::Point> points, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < points.size(); i++) {
        circle(imgCopy, points[i], 1, Colors::GREEN, -1, 8, 0);
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_Points(cv::Mat img, std::vector<std::vector<cv::Point>> points,
    std::string fileName, bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < points.size(); i++) {
        for (int j = 0; j < points.size(); j++) {
            circle(imgCopy, points[i][j], 3, Colors::GREEN, -1, 8, 0);
        }
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_PointsState(cv::Mat img, std::vector<cv::Point> points,
    std::vector<bool> states, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    if (points.size() != states.size()) {
        return false;
    }
    for (int i = 0; i < points.size(); i++) {
        if (states[i]) {
            circle(imgCopy, points[i], 2, Colors::GREEN, -1, 8, 0);
        }
        else {
            circle(imgCopy, points[i], 2, Colors::RED, -1, 8, 0);
        }
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_PosAndScore(cv::Mat img, std::vector<int> posList,
    std::vector<double> grayDif, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < posList.size(); i++) {
        line(imgCopy, cv::Point(0, posList[i]), cv::Point(imgCopy.cols - 1, posList[i]),
            Colors::GREEN, 2, 8, 0);
        char text[100];
        sprintf(text, "%.2f", grayDif[i]);
        putText(imgCopy, text, cv::Point2f(imgCopy.cols / 2, posList[i] - 5),
            cv::FONT_HERSHEY_SIMPLEX, 0.7, Colors::GREEN, 2, 8, false);
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_ProjectVer(std::vector<int> valList, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    int len = valList.size();
    int height = 100;
    cv::Mat img = cv::Mat::zeros(height, len, CV_8UC3);
    int maxVal = 0;
    for (int i = 0; i < len; i++) {
        maxVal = MAX(valList[i], maxVal);
    }
    for (int i = 0; i < len; i++) {
        float rate = valList[i] * 1.0 / maxVal;
        int sy = height * (1 - rate);
        cv::Point spt(i, sy);
        cv::Point ept(i, height);
        line(img, spt, ept, Colors::RED, 1, 8, 0);
    }
    imwrite(fileName, img);
}

bool DrawAndShowImg::DAS_ProjectVer(std::vector<float> valList, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    int len = valList.size();
    int height = 100;
    cv::Mat img = cv::Mat::zeros(height, len, CV_8UC3);
    int maxVal = 0;
    for (int i = 0; i < len; i++) {
        maxVal = MAX(valList[i], maxVal);
    }
    for (int i = 0; i < len; i++) {
        float rate = valList[i] * 1.0 / maxVal;
        int sy = height * (1 - rate);
        cv::Point spt(i, sy);
        cv::Point ept(i, height);
        line(img, spt, ept, Colors::RED, 1, 8, 0);
    }
    imwrite(fileName, img);
}

bool DrawAndShowImg::DAS_ProjectHor(std::vector<int> valList, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    int len = valList.size();
    int width = 100;
    cv::Mat img = cv::Mat::zeros(len, width, CV_8UC3);
    int maxVal = 0;
    for (int i = 0; i < len; i++) {
        maxVal = MAX(valList[i], maxVal);
    }
    for (int i = 0; i < len; i++) {
        float rate = valList[i] * 1.0 / maxVal;
        int ex = width * (rate);
        cv::Point spt(0, i);
        cv::Point ept(ex, i);
        line(img, spt, ept, Colors::RED, 1, 8, 0);
    }
    imwrite(fileName, img);
    return true;
}

bool DrawAndShowImg::DAS_ProjectHor(std::vector<float> valList, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    int len = valList.size();
    int width = 100;
    cv::Mat img = cv::Mat::zeros(len, width, CV_8UC3);
    int maxVal = 0;
    for (int i = 0; i < len; i++) {
        maxVal = MAX(valList[i], maxVal);
    }
    for (int i = 0; i < len; i++) {
        float rate = valList[i] * 1.0 / maxVal;
        int ex = width * (rate);
        cv::Point spt(0, i);
        cv::Point ept(ex, i);
        line(img, spt, ept, Colors::RED, 1, 8, 0);
    }
    imwrite(fileName, img);
    return true;
}

bool DrawAndShowImg::DAS_Rects(cv::Mat img, std::vector<cv::Rect> rects, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < rects.size(); i++) {
        rectangle(imgCopy, rects[i], Colors::GREEN, 1, 8, 0);
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_RectsScore(cv::Mat img, std::vector<cv::Rect> rects, std::vector<float> scores, std::string fileName, bool isSaveImg) {
	if (!isSaveImg) {
		return true;
	}
	if (img.empty()) {
		return false;
	}
	cv::Mat imgCopy;
	if (img.channels() == 3) {
		img.copyTo(imgCopy);
	}
	else {
		cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
	}
	for (int i = 0; i < rects.size(); i++) {
		rectangle(imgCopy, rects[i], Colors::GREEN, 1, 8, 0);

		char text_prob[100];
		sprintf(text_prob, "%.2f", scores[i]);
		putText(imgCopy, text_prob, cv::Point(rects[i].x, rects[i].y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, Colors::GREEN, 1, 8, 0);
	}
	imwrite(fileName, imgCopy);

	return true;
}

bool DrawAndShowImg::DAS_RectsScoreType(cv::Mat img, std::vector<cv::Rect> rects, std::vector<float> scores, std::vector<std::string> types, std::string fileName, bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < rects.size(); i++) {
        rectangle(imgCopy, rects[i], Colors::GREEN, 1, 8, 0);

        char text_prob[100];
        sprintf(text_prob, "%.2f", scores[i]);
        putText(imgCopy, text_prob, cv::Point(rects[i].x, rects[i].y + 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, Colors::GREEN, 1, 8, 0);


        putTextZH(imgCopy, types[i].c_str(), cv::Point(rects[i].x, rects[i].y + 25), Colors::GREEN, 15, 8);

    }
    imwrite(fileName, imgCopy);
    return true;
}


bool DrawAndShowImg::DAS_RectsScoreType(cv::Mat img, std::vector < std::vector<cv::Rect>> rects, std::vector < std::vector<float>> scores, std::vector < std::vector<std::string>> types, std::string fileName, bool isSaveImg)
{
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < rects.size(); i++) {
        for (int j = 0; j < rects[i].size(); j++) {
            rectangle(imgCopy, rects[i][j], Colors::GREEN, 1, 8, 0);

            char text_prob[100];
            sprintf(text_prob, "%.2f", scores[i][j]);
            putText(imgCopy, text_prob, cv::Point(rects[i][j].x, rects[i][j].y + 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, Colors::GREEN, 1, 8, 0);


            putTextZH(imgCopy, types[i][j].c_str(), cv::Point(rects[i][j].x, rects[i][j].y + 25), Colors::GREEN, 15, 8);

        }
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_RectsState(cv::Mat img, std::vector<cv::Rect> rects,
    std::vector<bool> states, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < rects.size(); i++) {
        if (states[i]) {
            rectangle(imgCopy, rects[i], Colors::GREEN, 1, 8, 0);
        }
        else {
            rectangle(imgCopy, rects[i], Colors::RED, 1, 8, 0);
        }
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_RotateRect(cv::Mat img, cv::RotatedRect box, std::string fileName,
    bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    ellipse(imgCopy, box, Colors::RED, 1);
    char text[100];
    sprintf(text, "x %.2f, y %.2f, w %.2f, h %.2f", box.center.x, box.center.y,
        box.size.width, box.size.height);
    putText(imgCopy, text, cv::Point2f(imgCopy.cols * 0.2, box.center.y),
        cv::FONT_HERSHEY_SIMPLEX, 0.6, Colors::RED, 1, 8, false);
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_RotateRects(cv::Mat img, std::vector<cv::RotatedRect> boxes,
    std::string fileName, bool isSaveImg) {
    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }
    for (int i = 0; i < boxes.size(); i++) {
        ellipse(imgCopy, boxes[i], Colors::RED, 1);
    }
    imwrite(fileName, imgCopy);
    return true;
}

bool DrawAndShowImg::DAS_String(
    cv::Mat img,
    std::string info,
    std::string fileName,
    bool isSaveImg)
{

    if (!isSaveImg) {
        return true;
    }
    if (img.empty()) {
        return false;
    }
    cv::Mat imgCopy;
    if (img.channels() == 3) {
        img.copyTo(imgCopy);
    }
    else {
        cv::cvtColor(img, imgCopy, cv::COLOR_GRAY2BGR);
    }

    putTextZH(imgCopy, info.c_str(), cv::Point(img.cols/3, img.rows/2), Colors::GREEN, 25, FW_BOLD);
    imwrite(fileName, imgCopy);
    return true;
}