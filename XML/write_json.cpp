#include "write_json.h"

std::string GbkToUtf8(const std::string& gbkStr) {
    int wideLen = MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, NULL, 0);
    wchar_t* wideBuf = new wchar_t[wideLen + 1];
    memset(wideBuf, 0, (wideLen + 1) * sizeof(wchar_t));
    MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, wideBuf, wideLen);

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideBuf, -1, NULL, 0, NULL, NULL);
    char* utf8Buf = new char[utf8Len + 1];
    memset(utf8Buf, 0, utf8Len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wideBuf, -1, utf8Buf, utf8Len, NULL, NULL);

    std::string result(utf8Buf);
    delete[] wideBuf;
    delete[] utf8Buf;
    return result;
}

json generateXAnyLabelingJSON(
    const std::vector<FinsObjectRotate>& details,
    const std::string& imagePath,
    int imageHeight,
    int imageWidth)
{
    json root;
    root["version"] = "2.4.4";
    root["flags"] = json::object();
    root["imagePath"] = imagePath;
    root["imageData"] = nullptr;
    root["imageHeight"] = imageHeight;
    root["imageWidth"] = imageWidth;
    root["description"] = "";

    json shapes = json::array();

    for (const auto& obj : details) {
        json shape;
        cv::Point2f vertices[4];
        obj.box.points(vertices);  // 获取旋转矩形四个顶点[6,7](@ref)

        // 坐标点处理（转换为二维数组）
        json pointsArray = json::array();
        for (int i = 0; i < 4; ++i) {
            pointsArray.push_back({ vertices[i].x, vertices[i].y });
        }

        // 角度转换（OpenCV角度转弧度）
        float direction = obj.box.angle * CV_PI / 180.0f;  // 转换为弧度[3,6](@ref)

        // 构建shape对象
        shape["kie_linking"] = json::array();
        shape["direction"] = direction;
        shape["label"] = GbkToUtf8(obj.className);
        shape["score"] = obj.confidence;  // 使用检测置信度
        shape["points"] = pointsArray;
        shape["group_id"] = nullptr;
        shape["description"] = "";
        shape["difficult"] = false;
        shape["shape_type"] = "rotation";
        shape["flags"] = json::object();
        shape["attributes"] = json::object();

        shapes.push_back(shape);
    }

    root["shapes"] = shapes;

    return root;
}

json generateXAnyLabelingJSON(
    const std::vector<FinsObject>& details,
    const std::string& imagePath,
    int imageHeight,
    int imageWidth)
{
    json root;
    root["version"] = "2.4.4";
    root["flags"] = json::object();
    root["imagePath"] = imagePath;
    root["imageData"] = nullptr;
    root["imageHeight"] = imageHeight;
    root["imageWidth"] = imageWidth;
    root["description"] = "";

    json shapes = json::array();

    for (const auto& obj : details) {
        json shape;
        // Rect转四点坐标（左上、右上、右下、左下）[1,2](@ref)
        const cv::Point2f tl = obj.box.tl();   // 左上角
        const cv::Point2f br = obj.box.br();   // 右下角
        const std::vector<cv::Point2f> vertices = {
            tl,                                  // 左上
            cv::Point2f(br.x, tl.y),            // 右上
            br,                                  // 右下
            cv::Point2f(tl.x, br.y)             // 左下
        };

        // 坐标点处理（转换为二维数组）
        json pointsArray = json::array();
        for (const auto& pt : vertices) {
            pointsArray.push_back({ pt.x, pt.y });
        }

        // 构建shape对象（Rect无旋转角度）
        shape["kie_linking"] = json::array();
        shape["direction"] = 0.0f;  // 无旋转角度[1,2](@ref)
        shape["label"] = GbkToUtf8(obj.className);
        shape["score"] = obj.confidence;
        shape["points"] = pointsArray;
        shape["group_id"] = nullptr;
        shape["description"] = "";
        shape["difficult"] = false;
        shape["shape_type"] = "rectangle";  // 修改形状类型[2,4](@ref)
        shape["flags"] = json::object();
        shape["attributes"] = json::object();

        shapes.push_back(shape);
    }

    root["shapes"] = shapes;
    return root;
}

json generateXAnyLabelingJSONMulty(
    const std::vector<std::vector<FinsObject>>& details,
    const std::string& imagePath,
    int imageHeight,
    int imageWidth)
{
    json root;
    root["version"] = "2.4.4";
    root["flags"] = json::object();
    root["imagePath"] = imagePath;
    root["imageData"] = nullptr;
    root["imageHeight"] = imageHeight;
    root["imageWidth"] = imageWidth;
    root["description"] = "";

    json shapes = json::array();

    // 遍历所有组
    for (const auto& group : details) {
        // 遍历组内每个目标
        for (const auto& obj : group) {
            json shape;

            // Rect 转四点坐标（左上、右上、右下、左下）
            const cv::Point2f tl = obj.box.tl();   // 左上角
            const cv::Point2f br = obj.box.br();   // 右下角
            const std::vector<cv::Point2f> vertices = {
                tl,                                // 左上
                cv::Point2f(br.x, tl.y),           // 右上
                br,                                // 右下
                cv::Point2f(tl.x, br.y)            // 左下
            };

            // 坐标点处理（转换为二维数组）
            json pointsArray = json::array();
            for (const auto& pt : vertices) {
                pointsArray.push_back({ pt.x, pt.y });
            }

            // 构建 shape 对象
            shape["kie_linking"] = json::array();
            shape["direction"] = 0.0f;  // 无旋转角度
            shape["label"] = GbkToUtf8(obj.className);
            shape["score"] = obj.confidence;
            shape["points"] = pointsArray;
            shape["group_id"] = nullptr;
            shape["description"] = "";
            shape["difficult"] = false;
            shape["shape_type"] = "rectangle";  // 矩形
            shape["flags"] = json::object();
            shape["attributes"] = json::object();

            shapes.push_back(shape);
        }
    }

    root["shapes"] = shapes;
    return root;
}


void saveJSONToFile(const json& jsonData, const std::string& filename) {
    std::ofstream outputFile(filename, std::ios::binary);
    if (outputFile.is_open()) {
        // 禁用BOM头 + 强制UTF-8编码
        outputFile << jsonData.dump(4, ' ', false, json::error_handler_t::replace);
        outputFile.close();
        //std::cout << "文件保存成功: " << filename << std::endl;
    }
    else {
        std::cerr << "文件打开失败: " << filename << std::endl;
    }
}

//void saveJSONToFile(const json& jsonData, const std::string& filename) {
//    std::ofstream outputFile(filename);
//    if (outputFile.is_open()) {
//        outputFile << jsonData.dump(4); // 带缩进的格式化输出
//        outputFile.close();
//        std::cout << "JSON saved to: " << filename << std::endl;
//    }
//    else {
//        std::cerr << "Failed to open file: " << filename << std::endl;
//    }
//}
