#include "AnalyseMat.h"

AnalyseMat::AnalyseMat() {
    CAL = new CalcFun;
}

AnalyseMat::~AnalyseMat() {
    delete CAL;
}

bool AnalyseMat::IsRectOutOfBounds(const cv::Rect& rect, const cv::Mat& img) {

    if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height <= 0)
        return true;

    if (rect.x + rect.width > img.cols || rect.y + rect.height > img.rows)
        return true;

    return false;
}

cv::Rect AnalyseMat::AdjustROI(const cv::Rect& roi, const cv::Mat& img) {
    const int x = std::max(roi.x, 0);
    const int y = std::max(roi.y, 0);
    const int right = std::min(roi.x + roi.width, img.cols);
    const int bottom = std::min(roi.y + roi.height, img.rows);
    return { x, y, std::max(right - x, 0), std::max(bottom - y, 0) };
}

cv::RotatedRect AnalyseMat:: AdjustRotateROI(const cv::RotatedRect& rotated_rect, const cv::Mat& img) {
    // Step 1: ��ȡ��ת���ε��ĸ�����
    cv::Point2f vertices[4];
    rotated_rect.points(vertices);

    // Step 2: �������㵽ͼ��߽���
    for (int i = 0; i < 4; ++i) {
        vertices[i].x = std::clamp(vertices[i].x, 0.0f, static_cast<float>(img.cols - 1));
        vertices[i].y = std::clamp(vertices[i].y, 0.0f, static_cast<float>(img.rows - 1));
    }

    // Step 3: ���ڵ�����Ķ������¼�����С�����ת����
    cv::RotatedRect adjusted_rect = cv::minAreaRect(std::vector<cv::Point2f>(vertices, vertices + 4));

    // Step 4: ��֤�������ߴ�ͽǶ�
    cv::Size2f size = adjusted_rect.size;
    float angle = adjusted_rect.angle;

    // OpenCV �п���ʼ�մ��ڸ߶ȣ�����Ҫ������ԭʼ��ת����һ�£�������ǶȺͳߴ磩
    if (size.width < size.height) {
        std::swap(size.width, size.height);
        angle += 90.0f;
    }

    // �Ƕȹ�һ���� [0, 180)
    angle = std::fmod(angle, 180.0f);
    if (angle < 0) angle += 180.0f;

    return cv::RotatedRect(adjusted_rect.center, size, angle);
}

cv::Mat AnalyseMat::ExtractRotatedROI(const cv::RotatedRect& rotated_rect, const cv::Mat& img) {
    // Step 1: ��ȡ��ת���ζ��㲢�����߽�
    cv::Point2f vertices[4];
    rotated_rect.points(vertices);
    for (int i = 0; i < 4; ++i) {
        vertices[i].x = std::clamp(vertices[i].x, 0.0f, static_cast<float>(img.cols - 1));
        vertices[i].y = std::clamp(vertices[i].y, 0.0f, static_cast<float>(img.rows - 1));
    }

    // Step 2: ������Ĥ
    cv::Mat mask = cv::Mat::zeros(img.size(), CV_8UC1);
    std::vector<cv::Point> poly_pts;
    for (int i = 0; i < 4; ++i) {
        poly_pts.push_back(cv::Point(vertices[i].x, vertices[i].y));
    }
    cv::fillConvexPoly(mask, poly_pts, cv::Scalar(255));

    // Step 3: ��ȡ ROI
    cv::Mat result;
    img.copyTo(result, mask);
    return result;
}

bool AnalyseMat::ChangeRectBnd(cv::Rect rectIn, int bndW, int bndH, cv::Rect& rectOut) {
    rectOut = cv::Rect(rectIn.x - bndW,
        rectIn.y - bndH,
        rectIn.width + 2 * bndW,
        rectIn.height + 2 * bndH);
    return true;
}


cv::RotatedRect AnalyseMat::ChangeRotateBox(cv::RotatedRect box, int w, int h) {
    cv::RotatedRect boxNew = box;
    boxNew.size.height = box.size.height + w;
    boxNew.size.width = box.size.width + h;
    return boxNew;
}

bool AnalyseMat::DataToMat(void* data, int w, int h, int channels, cv::Mat& outImg) {
    if (!data || w <= 0 || h <= 0 || channels < 1 || channels > 4)
        return false;

    const int type = CV_MAKETYPE(CV_8U, channels);
    outImg = cv::Mat(h, w, type, data).clone(); 
    return true;
}


void AnalyseMat::MatToData(const cv::Mat& srcImg, unsigned char*& data)
{
    if (data == nullptr || srcImg.empty()) {
        return;
    }
    cv::Mat src = srcImg;
    cv::Mat tmp;
    if (src.channels() == 3) {
        cv::cvtColor(src, tmp, cv::COLOR_BGR2RGB);
        src = tmp;
    }
    if (!src.isContinuous()) {
        src = src.clone();
    }
    const size_t nBytes = src.total() * src.elemSize();
    memcpy(data, src.data, nBytes);
}


// ��ɢ��ɾ���Ż�:�ϲ�ѭ�������������ж�
int AnalyseMat::DeleteDiscretePts(const std::vector<cv::Point>& srcPts, std::vector<cv::Point>& dstPts) {
    constexpr size_t MIN_SIZE = 10;
    if (srcPts.size() < MIN_SIZE)
        return 0;

    std::vector<bool> valid(srcPts.size(), true);

    // �ϲ����������ж�
#pragma omp parallel for
    for (int i = 0; i < static_cast<int>(srcPts.size()); ++i) {
        const bool edgeCondition = (i < 2 || i >= srcPts.size() - 2);
        if (edgeCondition) {
            valid[i] = CheckEdgePoint(srcPts, i);
        }
        else {
            valid[i] = CheckInnerPoint(srcPts, i);
        }
    }

    dstPts.clear();
    dstPts.reserve(srcPts.size());
    for (size_t i = 0; i < srcPts.size(); ++i) {
        if (valid[i]) dstPts.push_back(srcPts[i]);
    }
    return 1;
}
bool AnalyseMat::CheckEdgePoint(const std::vector<cv::Point>& pts, int idx) {
    constexpr int Y_THRESH_1 = 1;  // ���ڵ�y����ֵ
    constexpr int Y_THRESH_2 = 2;  // ����y����ֵ

    // ��֤������Χ
    if (idx < 0 || idx >= static_cast<int>(pts.size()))
        return false;

    // ���x����������
    bool xValid = true;
    if (idx == 0) {
        // �׵������4��
        for (int k = 1; k <= 4 && (idx + k) < pts.size(); ++k) {
            if (pts[idx + k].x != pts[idx].x + k) {
                xValid = false;
                break;
            }
        }
    }
    else if (idx == 1) {
        // ���׵������3��
        for (int k = 1; k <= 3 && (idx + k) < pts.size(); ++k) {
            if (pts[idx + k].x != pts[idx].x + k) {
                xValid = false;
                break;
            }
        }
    }
    else if (idx == pts.size() - 1) {
        // ĩ����ǰ4��
        for (int k = 1; k <= 4 && (idx - k) >= 0; ++k) {
            if (pts[idx - k].x != pts[idx].x - k) {
                xValid = false;
                break;
            }
        }
    }
    else if (idx == pts.size() - 2) {
        // ��ĩ����ǰ3��
        for (int k = 1; k <= 3 && (idx - k) >= 0; ++k) {
            if (pts[idx - k].x != pts[idx].x - k) {
                xValid = false;
                break;
            }
        }
    }

    // ���y��������
    bool yValid = true;
    if (idx == 0) {
        if (pts.size() > 2) {
            if (abs(pts[2].y - pts[0].y) > Y_THRESH_2 ||
                abs(pts[1].y - pts[0].y) > Y_THRESH_1) {
                yValid = false;
            }
        }
    }
    else if (idx == 1) {
        if (pts.size() > 3) {
            if (abs(pts[3].y - pts[1].y) > Y_THRESH_2 ||
                abs(pts[2].y - pts[1].y) > Y_THRESH_1) {
                yValid = false;
            }
        }
    }
    else if (idx == pts.size() - 1) {
        if (pts.size() > 2) {
            if (abs(pts[idx - 2].y - pts[idx].y) > Y_THRESH_2 ||
                abs(pts[idx - 1].y - pts[idx].y) > Y_THRESH_1) {
                yValid = false;
            }
        }
    }
    else if (idx == pts.size() - 2) {
        if (pts.size() > 3) {
            if (abs(pts[idx - 2].y - pts[idx].y) > Y_THRESH_2 ||
                abs(pts[idx - 1].y - pts[idx].y) > Y_THRESH_1) {
                yValid = false;
            }
        }
    }

    return xValid && yValid;
}

// ����ڲ��㣨�Ǳ߽�㣩
bool AnalyseMat::CheckInnerPoint(const std::vector<cv::Point>& pts, int idx) {
    constexpr int Y_THRESH_1 = 1;
    constexpr int Y_THRESH_2 = 2;
    constexpr int CHECK_RANGE = 2;

    if (idx < CHECK_RANGE || idx >= static_cast<int>(pts.size()) - CHECK_RANGE)
        return false;

    // ���x���������ԣ�ǰ���2�㣩
    bool xValid = true;
    for (int k = -CHECK_RANGE; k <= CHECK_RANGE; ++k) {
        if (k == 0) continue;
        if (pts[idx + k].x != pts[idx].x + k) {
            xValid = false;
            break;
        }
    }

    // ���y�������䣨ǰ���2�㣩
    bool yValid = true;
    for (int k = -CHECK_RANGE; k <= CHECK_RANGE; ++k) {
        if (k == 0) continue;
        const int delta = abs(pts[idx + k].y - pts[idx].y);
        if ((abs(k) == 1 && delta > Y_THRESH_1) ||
            (abs(k) == 2 && delta > Y_THRESH_2)) {
            yValid = false;
            break;
        }
    }

    return xValid && yValid;
}


int AnalyseMat::GetSubpixVal(cv::Mat img, float idx, float idy) {
    int idx_int = floor(idx);
    int idy_int = floor(idy);
    float delta_x = 1 - (idx - idx_int);
    float delta_y = 1 - (idy - idy_int);
    int val_lt = img.at<uchar>(idy_int, idx_int);
    int val_rt = img.at<uchar>(idy_int, idx_int + 1);
    int val_lb = img.at<uchar>(idy_int + 1, idx_int);
    int val_rb = img.at<uchar>(idy_int + 1, idx_int + 1);
    int val_left = delta_y * val_lt + (1 - delta_y) * val_lb;
    int val_right = delta_y * val_rt + (1 - delta_y) * val_rb;
    int val = delta_x * val_left + (1 - delta_x) * val_right;
    return val;
}

int AnalyseMat::FindDynamicLines(cv::Mat& bwIn, int bnd, std::vector<int>& leftList,
    std::vector<int>& rightList) {
    int len = 15;
    int nCols = bwIn.cols;
    int nRows = bwIn.rows;
    std::vector<int> rawLeftList;
    std::vector<int> rawRightList;
    rawLeftList.reserve(nCols * nRows);
    rawRightList.reserve(nCols * nRows);
    for (int i = 1; i < nRows - 1; i++) {
        uchar* data = bwIn.ptr<uchar>(i);
        for (int j = 1; j < nCols - 1; j++) {
            if (data[j] == 0) {
                continue;
            }
            if (data[j - 1] == 0) {
                rawLeftList.push_back(j);
                break;
            }
        }
    }
    for (int i = 1; i < nRows - 1; i++) {
        uchar* data = bwIn.ptr<uchar>(i);
        for (int j = nCols - 1; j > 1; j--) {
            if (data[j] == 0) {
                continue;
            }
            if (data[j - 1] == 255) {
                rawRightList.push_back(j);
                break;
            }
        }
    }
    if (rawLeftList.size() < 10 || rawRightList.size() < 10) {
        return 0;
    }
    sort(rawLeftList.begin(), rawLeftList.end());
    sort(rawRightList.begin(), rawRightList.end());
    int leftIdx = rawLeftList[rawLeftList.size() / 2];
    int rightIdx = rawRightList[rawRightList.size() / 2];
    int leftS = MAX(leftIdx - bnd, 1);
    int leftE = leftIdx + bnd;
    int rightS = rightIdx - bnd;
    int rightE = rightIdx + bnd;
    std::vector<int> fLeftList;
    std::vector<int> fRightList;
    for (int i = 0; i < bwIn.rows; i++) {
        int idx_l = leftIdx;
        int idx_r = rightIdx;
        uchar* data = bwIn.ptr<uchar>(i);
        for (int j = leftS; j <= leftE; j++) {
            if (data[j] == 255 && data[j - 1] == 0) {
                idx_l = j;
                break;
            }
        }
        for (int j = rightE; j >= rightS; j--) {
            if (data[j] == 255 && data[j + 1] == 0) {
                idx_r = j;
                break;
            }
        }
        fLeftList.push_back(idx_l);
        fRightList.push_back(idx_r);
    }
    if (fLeftList.size() < 10 || fRightList.size() < 10) {
        return 0;
    }
    leftList.clear();
    rightList.clear();
    leftList.reserve(nRows);
    rightList.reserve(nRows);
    for (int i = 0; i < nRows; i++) {
        int sy = i - len;
        int ey = i + len;
        if (sy < 0) {
            sy = 0;
            ey = len;
        }
        else if (ey >= nRows) {
            sy = nRows - 1 - len;
            ey = nRows - 1;
        }
        int sum_l = 0;
        int cnt = 0;
        int sum_r = 0;
        for (int k = sy; k < ey; k++) {
            sum_l += fLeftList[k];
            sum_r += fRightList[k];
            cnt++;
        }
        int avg_l = sum_l * 1.0 / cnt + 0.5;
        int avg_r = sum_r * 1.0 / cnt + 0.5;
        leftList.push_back(avg_l);
        rightList.push_back(avg_r);
    }
    if (leftList.size() < 10 || rightList.size() < 10) {
        return 0;
    }
    return 1;
}

int AnalyseMat::FindDynamicLinesNeck(cv::Mat& bwIn, int bnd, std::vector<int>& leftList,
    std::vector<int>& rightList) {
    int len = 3;
    int nCols = bwIn.cols;
    int nRows = bwIn.rows;
    std::vector<int> rawLeftList;
    std::vector<int> rawRightList;
    rawLeftList.reserve(nCols * nRows);
    rawRightList.reserve(nCols * nRows);
    for (int i = 1; i < nRows - 1; i++) {
        uchar* data = bwIn.ptr<uchar>(i);
        for (int j = 1; j < nCols - 1; j++) {
            if (data[j] == 0) {
                continue;
            }
            if (data[j - 1] == 0) {
                rawLeftList.push_back(j);
                break;
            }
        }
    }
    for (int i = 1; i < nRows - 1; i++) {
        uchar* data = bwIn.ptr<uchar>(i);
        for (int j = nCols - 1; j > 1; j--) {
            if (data[j] == 0) {
                continue;
            }
            if (data[j - 1] == 255) {
                rawRightList.push_back(j);
                break;
            }
        }
    }
    if (rawLeftList.size() < 10 || rawRightList.size() < 10) {
        return 0;
    }
    sort(rawLeftList.begin(), rawLeftList.end());
    sort(rawRightList.begin(), rawRightList.end());
    int leftIdx = rawLeftList[rawLeftList.size() / 2];
    int rightIdx = rawRightList[rawRightList.size() / 2];
    int leftS = MAX(leftIdx - bnd, 1);
    int leftE = leftIdx + bnd;
    int rightS = rightIdx - bnd;
    int rightE = rightIdx + bnd;
    std::vector<int> fLeftList;
    std::vector<int> fRightList;
    for (int i = 0; i < bwIn.rows; i++) {
        int idx_l = leftIdx;
        int idx_r = rightIdx;
        uchar* data = bwIn.ptr<uchar>(i);
        for (int j = leftS; j <= leftE; j++) {
            if (data[j] == 255 && data[j - 1] == 0) {
                idx_l = j;
                break;
            }
        }
        for (int j = rightE; j >= rightS; j--) {
            if (data[j] == 255 && data[j + 1] == 0) {
                idx_r = j;
                break;
            }
        }
        fLeftList.push_back(idx_l);
        fRightList.push_back(idx_r);
    }
    if (fLeftList.size() < 10 || fRightList.size() < 10) {
        return 0;
    }
    leftList.clear();
    rightList.clear();
    leftList.reserve(nRows);
    rightList.reserve(nRows);
    for (int i = 0; i < nRows; i++) {
        int sy = i - len;
        int ey = i + len;
        if (sy < 0) {
            sy = 0;
            ey = len;
        }
        else if (ey >= nRows) {
            sy = nRows - 1 - len;
            ey = nRows - 1;
        }
        int sum_l = 0;
        int cnt = 0;
        int sum_r = 0;
        for (int k = sy; k < ey; k++) {
            sum_l += fLeftList[k];
            sum_r += fRightList[k];
            cnt++;
        }
        int avg_l = sum_l * 1.0 / cnt + 0.5;
        int avg_r = sum_r * 1.0 / cnt + 0.5;
        leftList.push_back(avg_l);
        rightList.push_back(avg_r);
    }
    if (leftList.size() < 10 || rightList.size() < 10) {
        return 0;
    }
    return 1;
}

bool AnalyseMat::FindPeak(const std::vector<float>& v,
    std::vector<int>& peakPositions,
    std::vector<int>& peakPosScore,
    float thresh)
{
    const size_t size = v.size();
    if (size < 3) return false;  // ������Ҫ3������ܼ���ֵ

    // ֱ�Ӵ洢���Ʒ��򣬱������α���
    bool prevIncreasing = false;
    peakPositions.clear();
    peakPosScore.clear();

    // ������һ����
    prevIncreasing = (v[1] > v[0]);

    // ���α�����ɲ�ֺͷ�ֵ���
    for (size_t i = 1; i < size - 1; ++i) {
        bool currentIncreasing = (v[i + 1] > v[i]);

        // ����ֵ�㣺���������½���ת�۵�
        if (prevIncreasing && !currentIncreasing) {
            const size_t peakIndex = i;  // ��ֵλ���ǵ�ǰ����
            if (v[peakIndex] > thresh) {
                peakPositions.push_back(peakIndex);
                peakPosScore.push_back(v[peakIndex]);
            }
        }

        // ��������״̬������ƽ̹����
        if (v[i + 1] != v[i]) {
            prevIncreasing = currentIncreasing;
        }
    }

    return !peakPositions.empty();
}

bool AnalyseMat::FindPointsMinMaxX(std::vector<cv::Point> points, int& minX, int& maxX) {
    if (points.empty()) {
        minX = 0;
        maxX = 0;
        return false;
    }
    minX = points[0].x;
    maxX = points[0].x;
    for (const auto& pt : points) {
        minX = std::min(minX, pt.x);
        maxX = std::max(maxX, pt.x);
    }
    return true;
}

bool AnalyseMat::FindPointsMinMaxY(std::vector<cv::Point> points, int& minY, int& maxY) {
    if (points.empty()) {
        minY = 0;
        maxY = 0;
        return false;
    }
    minY = points[0].y;
    maxY = points[0].y;
    for (const auto& pt : points) {
        minY = std::min(minY, pt.y);
        maxY = std::max(maxY, pt.y);
    }
    return true;
}


bool AnalyseMat::FindPointsXY(const std::vector<cv::Point>& points,
    int& minX, cv::Point& minXPoint,
    int& minY, cv::Point& minYPoint,
    int& maxX, cv::Point& maxXPoint,
    int& maxY, cv::Point& maxYPoint)
{
    if (points.empty()) {
        return false;
    }

    // ��ʼ����ֵΪ��һ�����ֵ
    const cv::Point& first = points.front();
    minX = maxX = first.x;
    minY = maxY = first.y;
    minXPoint = maxXPoint = minYPoint = maxYPoint = first;

    // ���α����Ż�
    for (const auto& pt : points) {  // ���ڷ�Χ��ѭ��
        // ����X����
        if (pt.x < minX) {
            minX = pt.x;
            minXPoint = pt;
        }
        else if (pt.x > maxX) {
            maxX = pt.x;
            maxXPoint = pt;
        }

        // ����Y����
        if (pt.y < minY) {
            minY = pt.y;
            minYPoint = pt;
        }
        else if (pt.y > maxY) {
            maxY = pt.y;
            maxYPoint = pt;
        }
    }

    return true;  // �޸�ԭ����ȱ�ٷ���ֵ������
}
bool AnalyseMat::FindPointUpToDownOne(cv::Mat img, int sx, int sy, int ex, int ey, std::vector<cv::Point>& points) {
    // ���ͼ����Ч��
    if (img.empty() || img.channels() != 1)
        return false;

    // ������Ч�Լ��
    if (sx < 0 || sy < 0 || ex > img.cols || ey > img.rows || sx >= ex || sy >= ey)
        return false;

    points.clear();
    points.reserve(ex - sx); // Ԥ�����ڴ���ٶ�̬����

    // �б����Ż�:ʹ��ָ�������������
    for (int col = sx; col < ex; ++col) {
        // �б����Ż�:��������ɨ��
        for (int row = sy; row < ey; ++row) {
            // ֱ�ӷ�����������
            if (img.ptr<uchar>(row)[col] == 255) {
                points.emplace_back(col, row); // ʹ��emplace������ʱ������
                break; // �ҵ���������ǰ��
            }
        }
    }

    return !points.empty(); // ֱ�ӷ��ؽ��״̬
}


// ����ɨ���Ż��棨����ԭ�в�����
bool AnalyseMat::FindPointLeftToRightOne(cv::Mat img, std::vector<cv::Point>& points) {
    // ͼ����Ч�Լ��
    if (img.empty() || img.channels() != 1)
        return false;

    points.clear();
    const int rows = img.rows;
    const int cols = img.cols;
    points.reserve(rows); // Ԥ�����ڴ�

    // �б����Ż��������ڴ����
    for (int row = 0; row < rows; ++row) {
        uchar* pRow = img.ptr<uchar>(row);

        // �б���������ɨ��ȫͼ
        for (int col = 0; col < cols; ++col) {
            if (pRow[col] == 255) {
                points.emplace_back(col, row);
                break; // �ҵ�����ת����һ��
            }
            // ����ԭ�߼�����ĩǿ�Ƽ�¼
            if (col == cols - 1) {
                points.emplace_back(col, row);
            }
        }
    }

    return !points.empty();
}

// �ҵ���ɨ���Ż��棨����ԭ�в�����
bool AnalyseMat::FindPointRightToLeftOne(cv::Mat img, std::vector<cv::Point>& points) {
    // ͼ����Ч�Լ��
    if (img.empty() || img.channels() != 1)
        return false;

    points.clear();
    const int rows = img.rows;
    const int cols = img.cols;
    points.reserve(rows); // Ԥ�����ڴ�

    // �б����Ż��������ڴ����
    for (int row = 0; row < rows; ++row) {
        uchar* pRow = img.ptr<uchar>(row);

        // �б������ҵ���ɨ��ȫͼ
        for (int col = cols - 1; col >= 0; --col) {
            if (pRow[col] == 255) {
                points.emplace_back(col, row);
                break; // �ҵ�����ת����һ��
            }
            // ����ԭ�߼�������ǿ�Ƽ�¼
            if (col == 0) {
                points.emplace_back(col, row);
            }
        }
    }

    return !points.empty();
}

//bool ValidateImage(const cv::Mat& img) {
//    return !img.empty() && img.channels() == 1;
//}
//
//bool ValidateROI(const cv::Mat& img, int sx, int sy, int ex, int ey) {
//    return sx >= 0 && sy >= 0 && ex <= img.cols && ey <= img.rows && ex > sx && ey > sy;
//}
//// ͨ��ɨ��ģ�壨֧�ַ����֪��
//template<typename Scanner>
//bool GenericScan(const cv::Mat& img, int sx, int sy, int ex, int ey,
//    std::vector<cv::Point>& points, Scanner scanner) {
//    // ������֤
//    if (!ValidateImage(img) || !ValidateROI(img, sx, sy, ex, ey))
//        return false;
//
//    points.clear();
//    points.reserve(scanner.GetReserveSize(sx, sy, ex, ey)); // ����Ԥ����
//
//    // ִ��ɨ�����
//    scanner(img, sx, sy, ex, ey, points);
//
//    return !points.empty();
//}
//
//
//// ˮƽɨ����ԣ�����ң�
//struct LeftToRightScanner : public  ScannerConfig {
//    int GetReserveSize(int sx, int sy, int ex, int ey) const {
//        return ey - sy; // ÿ�����һ����
//    }
//
//    void operator()(const cv::Mat& img, int sx, int sy, int ex, int ey,
//        std::vector<cv::Point>& points) const {
//        for (int y = sy; y < ey; ++y) {
//            const uchar* row = img.ptr<uchar>(y);
//            for (int x = sx; x < ex; ++x) {
//                if (row[x] == 255) {
//                    points.emplace_back(x, y);
//                    if (stop_at_first) break;
//                }
//            }
//        }
//    }
//};
//
//// ��ֱɨ����ԣ��ϡ��£�
//struct TopToBottomScanner : ScannerConfig {
//    int GetReserveSize(int sx, int sy, int ex, int ey) const {
//        return ex - sx; // ÿ�����һ����
//    }
//
//    void operator()(const cv::Mat& img, int sx, int sy, int ex, int ey,
//        std::vector<cv::Point>& points) const {
//        for (int x = sx; x < ex; ++x) {
//            for (int y = sy; y < ey; ++y) {
//                if (img.at<uchar>(y, x) == 255) {
//                    points.emplace_back(x, y);
//                    if (stop_at_first) break;
//                }
//            }
//        }
//    }
//};
//
//// ��Ե������
//struct EdgeScanner : ScannerConfig {
//    int GetReserveSize(int sx, int sy, int ex, int ey) const {
//        return (scan_direction == HORIZONTAL) ? (ey - sy) : (ex - sx);
//    }
//
//    void operator()(const cv::Mat& img, int sx, int sy, int ex, int ey,
//        std::vector<cv::Point>& points) const {
//        const int delta = (scan_direction == HORIZONTAL) ? 1 : img.cols;
//
//        for (int i = 0; i < ((scan_direction == HORIZONTAL) ? (ey - sy) : (ex - sx)); ++i) {
//            const int base_y = (scan_direction == HORIZONTAL) ? (sy + i) : sy;
//            const int base_x = (scan_direction == VERTICAL) ? (sx + i) : sx;
//
//            const uchar* ptr = img.ptr<uchar>(base_y) + base_x;
//            const int len = (scan_direction == HORIZONTAL) ? (ex - sx) : (ey - sy);
//
//            for (int j = 0; j < len - 1; ++j) {
//                const uchar curr = ptr[j * delta];
//                const uchar next = ptr[(j + 1) * delta];
//
//                bool condition = false;
//                switch (edge_type) {
//                case TO_WHITE: condition = (curr == 0 && next == 255); break;
//                case TO_BLACK: condition = (curr == 255 && next == 0); break;
//                case ANY_EDGE: condition = (curr != next); break;
//                }
//
//                if (condition) {
//                    const int x = (scan_direction == HORIZONTAL) ? (base_x + j) : base_x;
//                    const int y = (scan_direction == VERTICAL) ? (base_y + j) : base_y;
//                    points.emplace_back(x, y);
//                    if (stop_at_first) break;
//                }
//            }
//        }
//    }
//};


bool AnalyseMat::JudgeRectIn(cv::Rect maxRect, cv::Rect curRect) {
    if (maxRect.x > curRect.x || maxRect.y > curRect.y ||
        maxRect.x + maxRect.width < curRect.x + curRect.width ||
        maxRect.y + maxRect.height < curRect.y + curRect.height) {
        return false;
    }
    else {
        return true;
    }
}

bool AnalyseMat::JudgeCrossRect(cv::Rect rect_0, cv::Rect rect_1) {
    if (rect_0.x > rect_1.x + rect_1.width) {
        return false;
    }
    if (rect_0.y > rect_1.y + rect_1.height) {
        return false;
    }
    if (rect_0.x + rect_0.width < rect_1.x) {
        return false;
    }
    if (rect_0.y + rect_0.height < rect_1.y) {
        return false;
    }
    return true;
}

float AnalyseMat::JudgeCrossRectRate(cv::Rect rect_0, cv::Rect rect_1) {
    if (rect_0.x > rect_1.x + rect_1.width) {
        return 0.0;
    }
    if (rect_0.y > rect_1.y + rect_1.height) {
        return 0.0;
    }
    if (rect_0.x + rect_0.width < rect_1.x) {
        return 0.0;
    }
    if (rect_0.y + rect_0.height < rect_1.y) {
        return 0.0;
    }
    float colInt = std::min(rect_0.x + rect_0.width, rect_1.x + rect_1.width) -
        std::max(rect_0.x, rect_1.x);
    float rowInt = std::min(rect_0.y + rect_0.height, rect_1.y + rect_1.height) -
        std::max(rect_0.y, rect_1.y);
    float intersection = colInt * rowInt;
    float area1 = rect_0.width * rect_0.height;
    float area2 = rect_1.width * rect_1.height;
    if (area1 + area2 - intersection == 0) {
        std::cout << "Input rect error!" << std::endl;
        std::cout << "file = " << __FILE__ << "  line =" << __LINE__ << std::endl;
        return 0.0;
    }
    return intersection / (area1 + area2 - intersection);
}

void AnalyseMat::LabelRectGapXY(const cv::Mat& _binImgOri, std::vector<cv::Rect>& rectList, std::vector<int>& sizeList, int gapX, int gapY)
{

    cv::Mat _lableImg;
    // int  area_num = PS_BwLabel(_binImgOri,_lableImg)  ;
    int  area_num = LabelGapXY(_binImgOri, _lableImg, gapX, gapY);

    //cv::Mat _colorImg ;
    //icvprLabelColor(_lableImg,  _colorImg)   ;
    //imshow("_colorImg",_colorImg);
    //waitKey(0);

    std::vector<int>   minX;
    std::vector<int>   minY;
    std::vector<int>   maxX;
    std::vector<int>   maxY;
    std::vector<int>   sizeListTmp;
    for (int i = 0; i < area_num; i++)
    {

        minX.push_back(10000);
        minY.push_back(10000);
        maxX.push_back(0);
        maxY.push_back(0);

        sizeListTmp.push_back(0);
    }

    for (int i = 0; i < _lableImg.rows; i++)
        for (int j = 0; j < _lableImg.cols; j++)
        {

            int idx = _lableImg.at<int>(i, j);
            sizeListTmp[idx]++;
            minX[idx] = MIN(j, minX[idx]);
            minY[idx] = MIN(i, minY[idx]);
            maxX[idx] = MAX(j, maxX[idx]);
            maxY[idx] = MAX(i, maxY[idx]);
        }

    for (int i = 2; i < area_num; i++)
    {
        if (sizeListTmp[i] <= 0)
        {
            continue;
        }
        cv::Rect tmpRect(minX[i], minY[i], maxX[i] - minX[i] + 1, maxY[i] - minY[i] + 1);
        rectList.push_back(tmpRect);
        sizeList.push_back(sizeListTmp[i]);
    }
}

int AnalyseMat::LabelGapXY(const cv::Mat& _binImgOri, cv::Mat& _lableImg, int gapX, int gapY)
{
  
    if (_binImgOri.empty() ||
        _binImgOri.type() != CV_8UC1)
    {
        return 0;
    }

    cv::Mat _binImg = cv::Mat::zeros(_binImgOri.rows, _binImgOri.cols, CV_8UC1);

    for (int i = 0; i < _binImgOri.rows; i++)
        for (int j = 0; j < _binImgOri.cols; j++)
        {
            if (_binImgOri.at<uchar>(i, j) > 125)
            {
                _binImg.at<uchar>(i, j) = 1;
            }

        }
    // 1. first pass  

    _lableImg.release();
    _binImg.convertTo(_lableImg, CV_32SC1);

    int label = 1;  // start by 2  
    std::vector<int> labelSet;
    labelSet.push_back(0);   // background: 0  
    labelSet.push_back(1);   // foreground: 1  

    int rows = _binImg.rows - 1;
    int cols = _binImg.cols - 1;
    for (int i = 1; i < rows; i++)
    {
        int* data_curRow = _lableImg.ptr<int>(i);

        for (int j = 1; j < cols; j++)
        {
            if (data_curRow[j] == 1)
            {
                if (i > 300)
                {
                    i = i;
                }
                std::vector<int>   neighborLabels;
                std::vector<cv::Point> neighborPts;
                neighborLabels.reserve(2);

                for (int ii = i - gapY; ii <= i + gapY; ii++)
                    for (int jj = j - gapX; jj <= j + gapX; jj++)
                    {
                        if (ii < 0 || ii >= _lableImg.rows - 1 || jj < 0 || jj >= _lableImg.cols - 1)
                        {
                            continue;
                        }
                        int Pixel = _lableImg.at<int>(ii, jj);
                        if (Pixel > 1)
                        {
                            neighborLabels.push_back(Pixel);
                            cv::Point tmpPt(ii, jj);
                            neighborPts.push_back(tmpPt);
                        }
                    }

                if (neighborLabels.empty())
                {
                    labelSet.push_back(++label);  // assign to a new label  
                    data_curRow[j] = label;
                    labelSet[label] = label;
                    int kkk = 1;
                }
                else
                {
                    std::sort(neighborLabels.begin(), neighborLabels.end());
                    int maxLabel = neighborLabels[neighborLabels.size() - 1];
                    int smallestLabel = neighborLabels[0];
                    data_curRow[j] = smallestLabel;

                    if (maxLabel != smallestLabel)
                    {
                        SetImageVal(_lableImg, maxLabel, smallestLabel);
                    }

                }

                neighborLabels.clear();
                neighborPts.clear();
            }
        }
    }

    // update equivalent labels  
    // assigned with the smallest label in each equivalent label set  
    for (size_t i = 2; i < labelSet.size(); i++)
    {
        int curLabel = labelSet[i];
        int preLabel = labelSet[curLabel];
        while (preLabel != curLabel)
        {
            curLabel = preLabel;
            preLabel = labelSet[preLabel];
        }
        labelSet[i] = curLabel;
    }


    // 2. second pass  
    for (int i = 0; i < rows; i++)
    {
        int* data = _lableImg.ptr<int>(i);
        for (int j = 0; j < cols; j++)
        {
            int& pixelLabel = data[j];
            pixelLabel = labelSet[pixelLabel];
        }
    }

    return labelSet.size();
}

// ������ OpenCV cv::Mat ת��Ϊ HALCON HObject
void AnalyseMat::Mat2HObject(const cv::Mat& matImg, HalconCpp::HObject& hImg)
{
    // ��ȡ cv::Mat ͼ��Ŀ��ߺ�ͨ����
    int height = matImg.rows;
    int width = matImg.cols;
    int channels = matImg.channels();

    // ����ͨ����������ͬ���͵� HALCON ͼ��
    if (channels == 1)  // ��ͨ���Ҷ�ͼ��
    {
        // �� cv::Mat ͼ������ת��Ϊ HALCON �ֽ�ͼ��
        HalconCpp::GenImage1(&hImg, "byte", width, height, (Hlong)matImg.data);
    }
    else if (channels == 3)  // ��ͨ�� BGR ��ɫͼ��
    {
        // �� cv::Mat ͼ��ָ������ͨ�� B, G, R
        std::vector<cv::Mat> bgr;
        split(matImg, bgr);  // ���� BGR ����ͨ��

        // �� B, G, R ����ͨ���ֱ�ת��Ϊ HALCON ͼ��
        HalconCpp::HObject hRed, hGreen, hBlue;
        GenImage1(&hBlue, "byte", width, height, (Hlong)bgr[0].data);
        GenImage1(&hGreen, "byte", width, height, (Hlong)bgr[1].data);
        GenImage1(&hRed, "byte", width, height, (Hlong)bgr[2].data);

        // �� R, G, B ����ͨ���ϳ�Ϊһ�� HALCON ͼ��
        Compose3(hRed, hGreen, hBlue, &hImg);
    }
    else
    {
        std::cerr << "Unsupported cv::Mat format!" << std::endl;
    }
}

cv::Mat AnalyseMat::HObject2Mat(const HalconCpp::HObject& hImg)
{
    HTuple ptr, width, height, type;
    GetImagePointer1(hImg, &ptr, &type, &width, &height);
    int imgWidth = width.I();
    int imgHeight = height.I();
    std::string imgType = type.S().Text();
    int cvType = CV_8UC1;
    if (imgType == "byte") cvType = CV_8UC1;
    else if (imgType == "uint2") cvType = CV_16UC1;
    else if (imgType == "real") cvType = CV_32FC1;
    else throw std::runtime_error("Unsupported HALCON image type");
    cv::Mat matImg(imgHeight, imgWidth, cvType, (void*)(ptr.L()));
    return matImg.clone();
}

bool AnalyseMat::MergeCrossRect(cv::Rect rect_0, cv::Rect rect_1, cv::Rect& mergeRect) {
    bool isCross = JudgeCrossRect(rect_0, rect_1);
    if (isCross) {
        int minX = std::min(rect_0.x, rect_1.x);
        int minY = std::min(rect_0.y, rect_1.y);
        int maxX = std::max(rect_0.x + rect_0.width, rect_1.x + rect_1.width);
        int maxY = std::max(rect_0.y + rect_0.height, rect_1.y + rect_1.height);
        mergeRect.x = minX;
        mergeRect.y = minY;
        mergeRect.width = maxX - minX;
        mergeRect.height = maxY - minY;
        return true;
    }
    else {
        return false;
    }
}

bool AnalyseMat::MergeCrossRects(std::vector<cv::Rect>& rects) {
    int updated_size = rects.size();
    for (int i = 0; i < updated_size; i++) {
        for (int j = i + 1; j < updated_size; j++) {
            float rc = JudgeCrossRectRate(rects[i], rects[j]);
            if (rc > 0.01) {
                cv::Rect mergeRect;
                MergeCrossRect(rects[i], rects[j], mergeRect);
                rects[i] = mergeRect;
                rects.erase(rects.begin() + j);
                updated_size = rects.size();
            }
        }
    }
    return true;
}

bool AnalyseMat::MergeCrossBreaks(std::vector<BreakSize>& defects) {
    int updated_size = defects.size();
    for (int i = 0; i < updated_size; i++) {
        for (int j = i + 1; j < updated_size; j++) {
            float rc = JudgeCrossRectRate(defects[i].rect, defects[j].rect);
            if (rc > 0.01) {
                cv::Rect mergeRect;
                MergeCrossRect(defects[i].rect, defects[j].rect, mergeRect);
                defects[i].rect = mergeRect;
                defects[i].size = 0.763 * (defects[i].size + defects[j].size);
                defects.erase(defects.begin() + j);
                updated_size = defects.size();
            }
        }
    }
    return true;
}

bool AnalyseMat::RankRectsByX(std::vector<cv::Rect>& rectList) {
    if (rectList.empty()) {
        return false;
    }
    for (int i = 0; i < rectList.size(); i++) {
        cv::Rect tmpRect;
        for (int j = i + 1; j < rectList.size(); j++) {
            if (rectList[i].x > rectList[j].x) {
                tmpRect = rectList[i];
                rectList[i] = rectList[j];
                rectList[j] = tmpRect;
            }
        }
    }
    return true;
}

bool AnalyseMat::RankRectStringScoreByX(std::vector<cv::Rect>& rectList,
    std::vector<std::string>& stringList,
    std::vector<float>& scoreList) {
    if (rectList.empty() || stringList.empty() || scoreList.empty()) {
        return false;
    }
    for (int i = 0; i < rectList.size(); i++) {
        cv::Rect tmpRect;
        std::string tmpString;
        float tmpScore;
        for (int j = i + 1; j < rectList.size(); j++) {
            if (rectList[i].x > rectList[j].x) {
                tmpRect = rectList[i];
                rectList[i] = rectList[j];
                rectList[j] = tmpRect;
                tmpString = stringList[i];
                stringList[i] = stringList[j];
                stringList[j] = tmpString;
                tmpScore = scoreList[i];
                scoreList[i] = scoreList[j];
                scoreList[j] = tmpScore;
            }
        }
    }
    return true;
}


bool AnalyseMat::RankFinsObjectByX(std::vector<FinsObject>& codeList) {
    if (codeList.empty()) {
        return false;
    }
    std::sort(codeList.begin(), codeList.end(),
        [](const FinsObject& a, const FinsObject& b) {
            return a.box.x < b.box.x;
        });
    return true;
}

bool AnalyseMat::FindRectsMinX(std::vector<cv::Rect> rectList, cv::Rect& rectMinX, int& id) {
    if (rectList.empty()) {
        return false;
    }
    int minX = 100000;
    for (int i = 0; i < rectList.size(); i++) {
        if (rectList[i].x < minX) {
            minX = rectList[i].x;
            rectMinX = rectList[i];
            id = i;
        }
    }
    return true;
}

bool AnalyseMat::FindRectsMaxX(std::vector<cv::Rect> rectList, cv::Rect& rectMaxX, int& id) {
    if (rectList.empty()) {
        return false;
    }
    int maxX = 0;
    for (int i = 0; i < rectList.size(); i++) {
        if (rectList[i].x > maxX) {
            maxX = rectList[i].x;
            rectMaxX = rectList[i];
            id = i;
        }
    }
    return true;
}

bool AnalyseMat::FindRectsMinY(std::vector<cv::Rect> rectList, cv::Rect& rectMinY, int& id) {
    if (rectList.empty()) {
        return false;
    }
    int minY = 100000;
    for (int i = 0; i < rectList.size(); i++) {
        if (rectList[i].y < minY) {
            minY = rectList[i].y;
            rectMinY = rectList[i];
            id = i;
        }
    }
    return true;
}

bool AnalyseMat::FindObjectMinY(std::vector<FinsObject> codeList, FinsObject& rectMinY, int& id) {
    if (codeList.empty()) {
        return false;
    }
    int minY = 100000;
    for (int i = 0; i < codeList.size(); i++) {
        if (codeList[i].box.y < minY) {
            minY = codeList[i].box.y;
            rectMinY = codeList[i];
            id = i;
        }
    }
    return true;
}

bool AnalyseMat::FindRectsMaxY(std::vector<cv::Rect> rectList, cv::Rect& rectMaxY, int& id) {
    if (rectList.empty()) {
        return false;
    }
    int maxY = 0;
    for (int i = 0; i < rectList.size(); i++) {
        if (rectList[i].y > maxY) {
            maxY = rectList[i].y;
            rectMaxY = rectList[i];
            id = i;
        }
    }
    return true;
}

bool AnalyseMat::FindRectsBoundary(std::vector<cv::Rect> rectList, cv::Rect& rectBoundary) {
    if (rectList.empty()) {
        return false;
    }
    int minX = 100000;
    int minY = 100000;
    int maxX = 0;
    int maxY = 0;
    for (int i = 0; i < rectList.size(); i++) {
        if (rectList[i].x < minX) {
            minX = rectList[i].x;
        }
        if (rectList[i].y < minY) {
            minY = rectList[i].y;
        }
        if (rectList[i].x + rectList[i].width > maxX) {
            maxX = rectList[i].x + rectList[i].width;
        }
        if (rectList[i].y + rectList[i].height > maxY) {
            maxY = rectList[i].y + rectList[i].height;
        }
    }
    rectBoundary = cv::Rect(minX, minY, maxX - minX, maxY - minY);
    return true;
}

bool AnalyseMat::Project(cv::Mat img, std::vector<int>& projectRes, bool isVer) {
    if (img.empty() || img.channels() != 1) {
        return false;
    }
    projectRes.clear();
    if (!isVer) {
        for (int i = 0; i < img.rows; i++) {
            int cnt = 0;
            for (int j = 0; j < img.cols; j++) {
                if (img.at<uchar>(i, j) == 255) {
                    cnt++;
                }
            }
            projectRes.push_back(cnt);
        }
    }
    else {
        for (int j = 0; j < img.cols; j++) {
            int cnt = 0;
            for (int i = 0; i < img.rows; i++) {
                if (img.at<uchar>(i, j) == 255) {
                    cnt++;
                }
            }
            projectRes.push_back(cnt);
        }
    }
    return true;
}

bool AnalyseMat::ProjectSmooth(std::vector<int>& projectList, int smoothBnd,
    int& peak, int& valley) {
    if (projectList.empty()) {
        return false;
    }
    peak = 0;
    valley = 1000000;
    std::vector<int> projectListSmooth(projectList.size());
    for (int i = smoothBnd; i < projectList.size() - smoothBnd; i++) {
        int cnt = 0;
        int avg = 0;
        for (int k = i - smoothBnd; k < i + smoothBnd; k++) {
            avg += projectList[k];
            cnt += 1;
        }
        projectListSmooth[i] = avg / cnt;
        peak = MAX(peak, projectListSmooth[i]);
        valley = MIN(valley, projectListSmooth[i]);
    }
    projectList = projectListSmooth;
    return true;
}

bool AnalyseMat::ProjectSmooth(std::vector<int>& projectList,
    std::vector<float>& projectListOut, int smoothBnd,
    float& peak, float& valley) {
    if (projectList.empty()) {
        return false;
    }
    std::vector<float> projectListSmooth(projectList.size());
    peak = 0;
    valley = 1000000;
    int len = smoothBnd * 2 + 1;
    float minW = 0.5;
    float maxW = 1.0;
    std::vector<float> weightVal(len);
    for (int i = 0; i < smoothBnd; i++) {
        weightVal[i] = 0.5 + (maxW - minW) * i / smoothBnd;
    }
    weightVal[smoothBnd] = 1.0;
    for (int i = smoothBnd + 1; i < len; i++) {
        weightVal[i] = 1.0 + (maxW - minW) * (smoothBnd - i) / smoothBnd;
    }
    for (int i = 0; i < projectList.size(); i++) {
        int si = MAX(i - smoothBnd, 0);
        int ei = MIN(i + smoothBnd, projectList.size() - 1);
        int avg = 0;
        float sumW = 0;
        for (int k = si; k < ei; k++) {
            int idx = k - si;
            avg += projectList[k] * weightVal[idx];
            sumW += weightVal[idx];
        }
        projectListSmooth[i] = avg * 1.0 / sumW;
        peak = MAX(peak, projectListSmooth[i]);
        valley = MIN(valley, projectListSmooth[i]);
    }
    projectListOut = projectListSmooth;
    return true;
}

bool AnalyseMat::RemoveEdgeBySize(cv::Mat imgEdge, cv::Mat& imgEdgeFiltedge, int minW,
    int minH, int maxW, int maxH, int minPixelNum,
    int maxPixelNum) {
    if (imgEdge.empty() || imgEdge.channels() != 1) {
        return false;
    }
    std::vector<std::vector<cv::Point>> contours;
    findContours(imgEdge,
        contours,            // a std::vector of contours
        cv::RETR_LIST,           // retrieve the external contours
        cv::CHAIN_APPROX_NONE);  // retrieve all pixels of each contours
    imgEdgeFiltedge = cv::Mat::zeros(imgEdge.rows, imgEdge.cols, CV_8UC1);
    int contoursSize = contours.size();
    for (int i = 0; i < contoursSize; i++) {
        if (contours[i].size() < minPixelNum || contours[i].size() > maxPixelNum) {
            continue;
        }
        cv::Rect tmpRect = boundingRect(contours[i]);
        if ((tmpRect.width < minW && tmpRect.height < minH) ||
            (tmpRect.width > maxW && tmpRect.height > maxH)) {
            continue;
        }
        int contourSize = contours[i].size();
        for (int j = 0; j < contourSize; j++) {
            cv::Point pt = contours[i][j];
            imgEdgeFiltedge.at<uchar>(pt.y, pt.x) = 255;
        }
    }
    imgEdgeFiltedge.copyTo(imgEdge);
    contours.clear();
    return true;
}

bool AnalyseMat::RemoveEdgeByDir(cv::Mat srcImg, cv::Mat edgeImg, bool isHor, float bnd,
    cv::Mat& outImg) {
    if (srcImg.empty() || edgeImg.empty()) {
        return false;
    }
    outImg = cv::Mat::zeros(srcImg.size(), CV_8UC1);
    float edgeVal;
    for (int i = 1; i < srcImg.rows - 1; i++) {
        uchar* data0 = srcImg.ptr<uchar>(i - 1);
        uchar* data1 = srcImg.ptr<uchar>(i);
        uchar* data2 = srcImg.ptr<uchar>(i + 1);
        for (int j = 1; j < srcImg.cols - 1; j++) {
            if (edgeImg.at<uchar>(i, j) == 0) {
                continue;
            }
            float dx = data0[j + 1] + (data1[j + 1] << 1) + data2[j + 1] -
                (data0[j - 1] + (data1[j - 1] << 1) + data2[j - 1]);
            float dy = data2[j - 1] + (data2[j] << 1) + data2[j + 1] -
                (data0[j - 1] + (data0[j] << 1) + data0[j + 1]);
            edgeVal = CAL->CALC_Angle180(dx, dy);
            if (isHor) {
                if (edgeVal < bnd || edgeVal > 180 - bnd) {
                    outImg.at<uchar>(i, j) = 255;
                }
            }
            else {
                if (edgeVal > 90 - bnd && edgeVal < 90 + bnd) {
                    outImg.at<uchar>(i, j) = 255;
                }
            }
        }
    }
    return true;
}

bool AnalyseMat::RemoveEdgeByDir(cv::Mat srcImg, cv::Mat edgeImg, int orientation, float bnd, cv::Mat& outImg) {
    if (srcImg.empty() || edgeImg.empty()) {
        outImg = edgeImg.clone();
        return false;
    }
    if (srcImg.cols != edgeImg.cols || srcImg.rows != edgeImg.rows) {
        outImg = edgeImg.clone();
        return false;
    }
    if (orientation < 0 || orientation > 3) {
        outImg = edgeImg.clone();
        return false;
    }
    outImg = cv::Mat::zeros(srcImg.size(), CV_8UC1);
    float edgeVal;
    for (int i = 1; i < srcImg.rows - 1; i++) {
        uchar* data0 = srcImg.ptr<uchar>(i - 1);
        uchar* data1 = srcImg.ptr<uchar>(i);
        uchar* data2 = srcImg.ptr<uchar>(i + 1);
        for (int j = 1; j < srcImg.cols - 1; j++) {
            if (edgeImg.at<uchar>(i, j) == 0) {
                continue;
            }
            float dx = data0[j + 1] + (data1[j + 1] << 1) + data2[j + 1] -
                (data0[j - 1] + (data1[j - 1] << 1) + data2[j - 1]);
            float dy = data2[j - 1] + (data2[j] << 1) + data2[j + 1] -
                (data0[j - 1] + (data0[j] << 1) + data0[j + 1]);
            edgeVal = CAL->CALC_Angle180(dx, dy);
            if (orientation == 0)  //������������
            {
                if (data0[j + 1] < data0[j - 1] || data1[j + 1] < data1[j - 1] || data2[j + 1] < data2[j - 1])
                {
                    continue;
                }
                if (edgeVal < bnd || edgeVal > 180 - bnd) {
                    outImg.at<uchar>(i, j) = 255;
                }
            }
            else if (orientation == 1) //������������
            {
                if (data0[j + 1] > data0[j - 1] || data1[j + 1] > data1[j - 1] || data2[j + 1] > data2[j - 1])
                {
                    continue;
                }
                if (edgeVal < bnd || edgeVal > 180 - bnd) {
                    outImg.at<uchar>(i, j) = 255;
                }
            }
            else if (orientation == 2) //������������
            {
                if (data2[j - 1] < data0[j - 1] || data2[j] < data0[j] || data2[j + 1] < data0[j + 1])
                {
                    continue;
                }
                if (edgeVal > 90 - bnd && edgeVal < 90 + bnd) {
                    outImg.at<uchar>(i, j) = 255;
                }
            }
            else if (orientation == 3) //������������
            {
                if (data2[j - 1] > data0[j - 1] || data2[j] > data0[j] || data2[j + 1] > data0[j + 1])
                {
                    continue;
                }
                if (edgeVal > 90 - bnd && edgeVal < 90 + bnd) {
                    outImg.at<uchar>(i, j) = 255;
                }
            }
        }
    }
    return true;
}

bool AnalyseMat::RemoveEdgeByDir(cv::Mat srcImg, cv::Mat edgeImg, int orientation, bool isHor, int polar, float bnd, cv::Mat& outImg) {
    if (srcImg.empty() || edgeImg.empty()) {
        outImg = edgeImg.clone();
        return false;
    }
    if (srcImg.cols != edgeImg.cols || srcImg.rows != edgeImg.rows) {
        outImg = edgeImg.clone();
        return false;
    }
    
    outImg = cv::Mat::zeros(srcImg.size(), CV_8UC1);
    float edgeVal;
    for (int i = 1; i < srcImg.rows - 1; i++) {
        uchar* data0 = srcImg.ptr<uchar>(i - 1);
        uchar* data1 = srcImg.ptr<uchar>(i);
        uchar* data2 = srcImg.ptr<uchar>(i + 1);
        for (int j = 1; j < srcImg.cols - 1; j++) {
            if (edgeImg.at<uchar>(i, j) == 0) {
                continue;
            }
            float dx = data0[j + 1] + (data1[j + 1] << 1) + data2[j + 1] -
                (data0[j - 1] + (data1[j - 1] << 1) + data2[j - 1]);
            float dy = data2[j - 1] + (data2[j] << 1) + data2[j + 1] -
                (data0[j - 1] + (data0[j] << 1) + data0[j + 1]);
            edgeVal = CAL->CALC_Angle180(dx, dy);
           
            if (orientation == 0)  //������������
            {
                if (polar == 0)
                {

                }
                else if (polar == 1)
				{
					if (data0[j + 1] < data0[j - 1] || data1[j + 1] < data1[j - 1] || data2[j + 1] < data2[j - 1])
					{
						continue;
					}
				}
				else if (polar == 2)
				{
					if (data0[j + 1] > data0[j - 1] || data1[j + 1] > data1[j - 1] || data2[j + 1] > data2[j - 1])
					{
						continue;
					}
				}
				else
				{
					return false;
				}
				if (edgeVal < bnd || edgeVal > 180 - bnd) {
					outImg.at<uchar>(i, j) = 255;
				}
            }
            else if (orientation == 1) //������������
            {
                if (polar == 0)
                {

                }
                else if (polar == 1)
                {
                    if (data0[j + 1] > data0[j - 1] || data1[j + 1] > data1[j - 1] || data2[j + 1] > data2[j - 1])
                    {
                        continue;
                    }
                }
                else if (polar == 2)
                {
                    if (data0[j + 1] < data0[j - 1] || data1[j + 1] < data1[j - 1] || data2[j + 1] < data2[j - 1])
                    {
                        continue;
                    }
                }
                else
                {
                    return false;
                }
                if (edgeVal < bnd || edgeVal > 180 - bnd) {
                    outImg.at<uchar>(i, j) = 255;
                }
            }
            else if (orientation == 2) //������������
            {
                if (polar == 0)
                {
                    
                }
                else if (polar == 1)
                {
                    if (data2[j - 1] < data0[j - 1] || data2[j] < data0[j] || data2[j + 1] < data0[j + 1])
                    {
                        continue;
                    }
                }
                else if (polar == 2)
                {
                    if (data2[j - 1] > data0[j - 1] || data2[j] > data0[j] || data2[j + 1] > data0[j + 1])
                    {
                        continue;
                    }
                }
                else
                {
                    return false;
                }
                if (edgeVal > 90 - bnd && edgeVal < 90 + bnd) {
                    outImg.at<uchar>(i, j) = 255;
                }
            }
            else if (orientation == 3) //������������
            {
                if (polar == 0)
                {

                }
                else if (polar == 1)
                {
                    if (data2[j - 1] > data0[j - 1] || data2[j] > data0[j] || data2[j + 1] > data0[j + 1])
                    {
                        continue;
                    }
                }
                else if (polar == 2)
                {
                    if (data2[j - 1] < data0[j - 1] || data2[j] < data0[j] || data2[j + 1] < data0[j + 1])
                    {
                        continue;
                    }
                }
                else
                {
                    return false;
                }
                
                if (edgeVal > 90 - bnd && edgeVal < 90 + bnd) {
                    outImg.at<uchar>(i, j) = 255;
                }
            }
        }
    }
    return true;
}

bool AnalyseMat::CheckPointOnLine(cv::Mat srcImg, cv::Mat bwImg, cv::Point point, int orientation, bool isHor, int polar, float angleBnd) {
    if (srcImg.empty()) {
        return false;
    }
    if (point.x <= 0 || point.y <= 0 || point.x >= srcImg.cols - 1 || point.y >= srcImg.rows - 1) {
        return false;
    }
    if (bwImg.at<uchar>(point.y, point.x) == 255 ||
        bwImg.at<uchar>(point.y, point.x - 1) == 255 ||
        bwImg.at<uchar>(point.y, point.x + 1) == 255 ||
        bwImg.at<uchar>(point.y - 1, point.x) == 255 ||
        bwImg.at<uchar>(point.y - 1, point.x - 1) == 255 ||
        bwImg.at<uchar>(point.y - 1, point.x + 1) == 255 ||
        bwImg.at<uchar>(point.y + 1, point.x) == 255 ||
        bwImg.at<uchar>(point.y + 1, point.x - 1) == 255 ||
        bwImg.at<uchar>(point.y + 1, point.x + 1) == 255
        ) 
    {
        return false;
    }

  

    if (orientation == 0)  //������������
    {
        if (polar == 0)
        {
            return true;
        }
        else if (polar == 1)
        {
            if (srcImg.at<uchar>(point.x - 1, point.y) < srcImg.at<uchar>(point.x, point.y) && srcImg.at<uchar>(point.x - 1, point.y) < srcImg.at<uchar>(point.x + 1, point.y))
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        else if (polar == 2)
        {
            if (srcImg.at<uchar>(point.x - 1, point.y) > srcImg.at<uchar>(point.x, point.y) && srcImg.at<uchar>(point.x - 1, point.y) > srcImg.at<uchar>(point.x + 1, point.y))
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        else
        {
            return false;
        }
        /*if (edgeVal < angleBnd || edgeVal > 180 - angleBnd) {
            return false;
        }
        else
        {
            return true;
        }*/
    }
    else if (orientation == 1) //������������
    {
        if (polar == 0)
        {
            return true;
        }
        else if (polar == 1)
        {
            if (srcImg.at<uchar>(point.x - 1, point.y) > srcImg.at<uchar>(point.x, point.y) && srcImg.at<uchar>(point.x - 1, point.y) > srcImg.at<uchar>(point.x + 1, point.y))
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        else if (polar == 2)
        {
            if (srcImg.at<uchar>(point.x - 1, point.y) < srcImg.at<uchar>(point.x, point.y) && srcImg.at<uchar>(point.x - 1, point.y) < srcImg.at<uchar>(point.x + 1, point.y))
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        else
        {
            return false;
        }
        /*if (edgeVal < angleBnd || edgeVal > 180 - angleBnd) {
            return false;
        }
        else
        {
            return true;
        }*/
    }
    else if (orientation == 2) //������������
    {
        if (polar == 0)
        {
            return true;
        }
        else if (polar == 1)
        {
            if (srcImg.at<uchar>(point.y - 1, point.x) > srcImg.at<uchar>(point.y, point.x) && srcImg.at<uchar>(point.y - 1, point.x) > srcImg.at<uchar>(point.y + 1, point.x))
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        else if (polar == 2)
        {
            if (srcImg.at<uchar>(point.y - 1, point.x) < srcImg.at<uchar>(point.y, point.x) && srcImg.at<uchar>(point.y - 1, point.x) < srcImg.at<uchar>(point.y + 1, point.x))
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        else
        {
            return false;
        }
       /* if (edgeVal < angleBnd || edgeVal > 180 - angleBnd) {
            return false;
        }
        else
        {
            return true;
        }*/
    }
    else if (orientation == 3) //������������
    {
        if (polar == 0)
        {
            return true;
        }
        else if (polar == 1)
        {
            if (srcImg.at<uchar>(point.x, point.y - 1) < srcImg.at<uchar>(point.x, point.y + 1) && srcImg.at<uchar>(point.x, point.y - 1) < srcImg.at<uchar>(point.x, point.y + 2))
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        else if (polar == 2)
        {
            if (srcImg.at<uchar>(point.x, point.y - 1) > srcImg.at<uchar>(point.x, point.y + 1) && srcImg.at<uchar>(point.x, point.y - 1) > srcImg.at<uchar>(point.x, point.y + 2))
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        else
        {
            return false;
        }
        /*if (edgeVal < angleBnd || edgeVal > 180 - angleBnd) {
            return false;
        }
        else
        {
            return true;
        }*/
    }

    return true;
}

bool AnalyseMat::RestrainRect(cv::Rect maxRect, cv::Rect curRect, cv::Rect& resRect) {
    if (!JudgeCrossRect(maxRect, curRect)) {
        resRect = curRect;
        std::cout << "cv::Rect error!" << std::endl;
        return false;
    }
    int maxX = 0;
    int maxY = 0;
    resRect = curRect;
    resRect.x = MAX(resRect.x, maxRect.x);
	resRect.y = MAX(resRect.y, maxRect.y);
	maxX = MIN(curRect.x + curRect.width, maxRect.x + maxRect.width);
    maxY = MIN(curRect.y + curRect.height, maxRect.y + maxRect.height);
    resRect.width = maxX - resRect.x;
    resRect.height = maxY - resRect.y;
    if (resRect.width <= 0 || resRect.height <= 0)
    {
        return false;
    }
    return true;
}

void AnalyseMat::RotateImg(cv::Mat src, cv::Mat& dst, cv::Point2f center, float angle) {
    if (src.empty()) {
        std::cout << "Input rotate img error!" << std::endl;
        std::cout << "file = " << __FILE__ << "  line =" << __LINE__ << std::endl;
        return;
    }
    float radian = (float)(angle / 180.0 * CV_PI);

    cv::Mat affine_matrix = getRotationMatrix2D(center, angle, 1.0);  //�����ת����
    warpAffine(src, dst, affine_matrix, dst.size());

}

bool AnalyseMat::SetImageVal(cv::Mat& labelImg, int compareVal, int dstVal) {
    if (labelImg.empty()) {
        return false;
    }
    for (int i = 0; i < labelImg.rows; i++) {
        uchar* data = labelImg.ptr<uchar>(i);
        for (int j = 0; j < labelImg.cols; j++) {
            if (data[j] == compareVal) {
                labelImg.at<int>(i, j) = dstVal;
            }
        }
    }
    return true;
}

int AnalyseMat::TwoPass(const cv::Mat& _binImgOri, cv::Mat& _lableImg, int gapX,
    int gapY) {
    // connected component analysis (4-component)
    // use two-pass algorithm
    // 1. first pass: label each foreground pixel with a label
    // 2. second pass: visit each labeled pixel and merge neighbor labels
    //
    // foreground pixel: _binImg(x,y) = 1
    // background pixel: _binImg(x,y) = 0
    if (_binImgOri.empty() || _binImgOri.type() != CV_8UC1) {
        return 0;
    }
    cv::Mat _binImg = cv::Mat::zeros(_binImgOri.rows, _binImgOri.cols, CV_8UC1);
    for (int i = 0; i < _binImgOri.rows; i++)
        for (int j = 0; j < _binImgOri.cols; j++) {
            if (_binImgOri.at<uchar>(i, j) > 125) {
                _binImg.at<uchar>(i, j) = 1;
            }
        }
    // 1. first pass
    _lableImg.release();
    _binImg.convertTo(_lableImg, CV_32SC1);
    int label = 1;  // start by 2
    std::vector<int> labelSet;
    labelSet.push_back(0);  // background: 0
    labelSet.push_back(1);  // foreground: 1
    int rows = _binImg.rows - 1;
    int cols = _binImg.cols - 1;
    for (int i = 1; i < rows; i++) {
        int* data_curRow = _lableImg.ptr<int>(i);
        for (int j = 1; j < cols; j++) {
            if (data_curRow[j] == 1) {
                if (i > 300) {
                    i = i;
                }
                std::vector<int> neighborLabels;
                std::vector<cv::Point> neighborPts;
                neighborLabels.reserve(2);
                for (int ii = i - gapY; ii <= i + gapY; ii++)
                    for (int jj = j - gapX; jj <= j + gapX; jj++) {
                        if (ii < 0 || ii >= _lableImg.rows - 1 || jj < 0 ||
                            jj >= _lableImg.cols - 1) {
                            continue;
                        }
                        int Pixel = _lableImg.at<int>(ii, jj);
                        if (Pixel > 1) {
                            neighborLabels.push_back(Pixel);
                            cv::Point tmpPt(ii, jj);
                            neighborPts.push_back(tmpPt);
                        }
                    }
                if (neighborLabels.empty()) {
                    labelSet.push_back(++label);  // assign to a new label
                    data_curRow[j] = label;
                    labelSet[label] = label;
                    int kkk = 1;
                }
                else {
                    std::sort(neighborLabels.begin(), neighborLabels.end());
                    int maxLabel = neighborLabels[neighborLabels.size() - 1];
                    int smallestLabel = neighborLabels[0];
                    data_curRow[j] = smallestLabel;
                    if (maxLabel != smallestLabel) {
                        SetImageVal(_lableImg, maxLabel, smallestLabel);
                    }
                }
                neighborLabels.clear();
                neighborPts.clear();
            }
        }
    }
    // update equivalent labels
    // assigned with the smallest label in each equivalent label set
    for (size_t i = 2; i < labelSet.size(); i++) {
        int curLabel = labelSet[i];
        int preLabel = labelSet[curLabel];
        while (preLabel != curLabel) {
            curLabel = preLabel;
            preLabel = labelSet[preLabel];
        }
        labelSet[i] = curLabel;
    }
    // 2. second pass
    for (int i = 0; i < rows; i++) {
        int* data = _lableImg.ptr<int>(i);
        for (int j = 0; j < cols; j++) {
            int& pixelLabel = data[j];
            pixelLabel = labelSet[pixelLabel];
        }
    }
    return labelSet.size();
}

bool AnalyseMat::WarpAnnulus2Rectangle(cv::Mat img, int w, int h, float radius,
    cv::Point center, float step, cv::Mat& imgOut) {
    for (int i = 0; i < h; i++) {
        float curAngle = step * i;
        float x = sin(curAngle) * radius + center.x;
        float y = -cos(curAngle) * radius + center.y;
        for (int j = 0; j < w; j++) {
            /********** first area !******/
            float delta = j * 1.0 / radius;
            float delta_x = (x - center.x) * delta;
            float delta_y = (y - center.y) * delta;
            float cur_x = x + delta_x;
            float cur_y = y + delta_y;
            int idx_x = cur_x + 0.5;
            int idx_y = cur_y + 0.5;
            if (idx_x >= img.cols - 1 || idx_x < 1 || idx_y >= img.rows - 1 ||
                idx_y < 1) {
                continue;
            }
            int val = GetSubpixVal(img, cur_x, cur_y);
            imgOut.at<uchar>(i, j) = val;
        }
    }
    return true;
}
