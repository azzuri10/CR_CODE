// AnalyseMat.h
#ifndef ANALYSEMAT_H
#define ANALYSEMAT_H
#include "HeaderDefine.h"
#include "CalcFun.h"
#include "HalconCpp.h"

struct ScannerConfig {
    enum Direction { HORIZONTAL, VERTICAL };
    enum Transition { TO_WHITE, TO_BLACK, ANY_EDGE };

    Direction scan_direction = HORIZONTAL;
    Transition edge_type = TO_WHITE;
    bool stop_at_first = true;
};

class AnalyseMat {
public:
    // ============== ɨ��������� ============== //
    struct ScannerConfig {
        enum Direction { HORIZONTAL, VERTICAL };
        enum Transition { TO_WHITE, TO_BLACK, ANY_EDGE };

        Direction scan_direction = HORIZONTAL;
        Transition edge_type = TO_WHITE;
        bool stop_at_first = true;
    };

    // ============== ����ģ�庯�� ============== //
    template<typename Scanner>
    bool GenericScan(const cv::Mat& img, int sx, int sy, int ex, int ey,
        std::vector<cv::Point>& points, Scanner scanner)
    {
        if (!ValidateImage(img) || !ValidateROI(img, sx, sy, ex, ey))
            return false;

        points.clear();
        points.reserve(scanner.GetReserveSize(sx, sy, ex, ey));
        scanner(img, sx, sy, ex, ey, points);
        return !points.empty();
    }

    // ============== Ԥ����ɨ����� ============== //
    struct LeftToRightScanner : ScannerConfig {
        int GetReserveSize(int sx, int sy, int ex, int ey) const {
            return ey - sy;
        }

        void operator()(const cv::Mat& img, int sx, int sy, int ex, int ey,
            std::vector<cv::Point>& points) const
        {
            for (int y = sy; y < ey; ++y) {
                const uchar* row = img.ptr<uchar>(y);
                for (int x = sx; x < ex; ++x) {
                    if (row[x] == 255) {
                        points.emplace_back(x, y);
                        if (stop_at_first) break;
                    }
                }
            }
        }
    };


    // ============== ���Ĺ��ܷ��� ============== //
    AnalyseMat();
    ~AnalyseMat();

    bool ValidateImage(const cv::Mat& img);
    bool ValidateROI(const cv::Mat& img, int sx, int sy, int ex, int ey);



    bool IsRectOutOfBounds(const cv::Rect& rect, const cv::Mat& img);
    cv::Rect AdjustROI(const cv::Rect& roi, const cv::Mat& img);
    cv::RotatedRect AdjustRotateROI(const cv::RotatedRect& rotated_rect, const cv::Mat& img);
    cv::Mat ExtractRotatedROI(const cv::RotatedRect& rotated_rect, const cv::Mat& img);
    bool ChangeRectBnd(cv::Rect rectIn, int bndW, int bndH, cv::Rect& rectOut);
    cv::RotatedRect ChangeRotateBox(cv::RotatedRect box, int w, int h);
    bool ConnectedComponentAnalysis(const cv::Mat& binImg, std::vector<BreakSize>& ccList);
    bool ConnectedComponentAnalysisConst(cv::Mat& _binImgOri, std::vector<BreakSize>& ccList, int gapX, int gapY);
    bool DataToMat(void* data, int nW, int nH, int nChannel, cv::Mat& outImg);
    void MatToData(const cv::Mat& srcImg, unsigned char*& data);
    int DeleteDiscretePts(const std::vector<cv::Point>& srcPts, std::vector<cv::Point>& dstPts);
    bool CheckEdgePoint(const std::vector<cv::Point>& pts, int idx);
    bool CheckInnerPoint(const std::vector<cv::Point>& pts, int idx);
    int FindDynamicLines(cv::Mat& bwIn, int bnd, std::vector<int>& leftList,
        std::vector<int>& rightList);
    int FindDynamicLinesNeck(cv::Mat& bwIn, int bnd, std::vector<int>& leftList,
        std::vector<int>& rightList);
    bool FindPeak(const std::vector<float>& v, std::vector<int>& peakPositions,
        std::vector<int>& peakPosScore, float thresh);

    bool FindPointsMinMaxX(std::vector<cv::Point> points, int& minX, int& maxX);
    bool FindPointsMinMaxY(std::vector<cv::Point> points, int& minY, int& maxY);
    bool FindPointsXY(const std::vector<cv::Point>& points,
        int& minX, cv::Point& minXPoint,
        int& minY, cv::Point& minYPoint,
        int& maxX, cv::Point& maxXPoint,
        int& maxY, cv::Point& maxYPoint);
    bool FindPointUpToDownOne(cv::Mat img, int sx, int sy, int ex, int ey, std::vector<cv::Point>& points);

    bool FindPointLeftToRightOne(cv::Mat img, std::vector<cv::Point>& points);
    bool FindPointRightToLeftOne(cv::Mat img, std::vector<cv::Point>& points);

    bool FindRectsBoundary(std::vector<cv::Rect> rectList, cv::Rect& rectBoundary);
    bool FindRectsMinX(std::vector<cv::Rect> rectList, cv::Rect& rectMinX, int& id);
    bool FindRectsMaxX(std::vector<cv::Rect> rectList, cv::Rect& rectMaxX, int& id);
    bool FindRectsMinY(std::vector<cv::Rect> rectList, cv::Rect& rectMinY, int& id);
    bool FindRectsMaxY(std::vector<cv::Rect> rectList, cv::Rect& rectMaxY, int& id);
    bool FindObjectMinY(std::vector<FinsObject> codeList, FinsObject& rectMinY, int& id);
    int GetSubpixVal(cv::Mat img, float idx, float idy);
    bool JudgeRectIn(cv::Rect maxRect, cv::Rect curRect);
    bool JudgeCrossRect(cv::Rect rect_0, cv::Rect rect_1);
    float JudgeCrossRectRate(cv::Rect rext_0, cv::Rect rext_1);
    void LabelRectGapXY(const cv::Mat& _binImgOri, std::vector<cv::Rect>& rectList, std::vector<int>& sizeList, int gapX, int gapY);
    int LabelGapXY(const cv::Mat& _binImgOri, cv::Mat& _lableImg, int gapX, int gapY);
    void Mat2HObject(const cv::Mat& matImg, HalconCpp::HObject& hImg);
    cv::Mat HObject2Mat(const HalconCpp::HObject& hImg);
    bool MergeCrossRect(cv::Rect rect_0, cv::Rect rect_1, cv::Rect& mergeRect);
    bool MergeCrossRects(std::vector<cv::Rect>& defects);
    bool MergeCrossBreaks(std::vector<BreakSize>& defects);
    bool RankRectsByX(std::vector<cv::Rect>& rectList);
    bool RankRectStringScoreByX(std::vector<cv::Rect>& rectList, std::vector<std::string>& strList,
        std::vector<float>& probList);
    bool RankFinsObjectByX(std::vector<FinsObject>& codeList);

    bool Project(cv::Mat img, std::vector<int>& projectRes, bool isVer);
    bool ProjectSmooth(std::vector<int>& projectList, int smoothBnd, int& peak,
        int& valley);
    bool ProjectSmooth(std::vector<int>& projectList, std::vector<float>& projectListOut,
        int smoothBnd, float& peak, float& valley);
    bool RemoveEdgeBySize(cv::Mat imgEdge, cv::Mat& imgEdgeFiltedge, int minW, int minH,
        int maxW, int maxH, int minPixelNum, int maxPixelNum);
    bool RemoveEdgeByDir(cv::Mat srcImg, cv::Mat edgeImg, bool isHor, float bnd, cv::Mat& outImg);
    bool RemoveEdgeByDir(cv::Mat srcImg, cv::Mat edgeImg, int orientation, float bnd, cv::Mat& outImg);
    bool RemoveEdgeByDir(cv::Mat srcImg, cv::Mat edgeImg, int orientation, bool isHor, int polar, float bnd, cv::Mat& outImg);
    bool CheckPointOnLine(cv::Mat srcImg, cv::Mat bwImg, cv::Point point, int orientation, bool isHor, int polar, float angleBnd);
    bool RestrainRect(cv::Rect maxRect, cv::Rect curRect, cv::Rect& resRect);
    void RotateImg(cv::Mat src, cv::Mat& dst, cv::Point2f center, float angle);
    bool SetImageVal(cv::Mat& labelImg, int compareVal, int dstVal);
    int TwoPass(const cv::Mat& _binImgOri, cv::Mat& _labelImg, int gapX, int gapY);
    bool WarpAnnulus2Rectangle(cv::Mat img, int w, int h, float radius, cv::Point center,
        float step, cv::Mat& imgOut);

private:
    CalcFun* CAL;
};

#endif  // ANALYSEMAT_H