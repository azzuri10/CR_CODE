#pragma once
#include "HeaderDefine.h"
#pragma optimize
#include <json.hpp>

using json = nlohmann::json;
json generateXAnyLabelingJSON(
    const std::vector<FinsObjectRotate>& details,
    const std::string& imagePath,
    int imageHeight,
    int imageWidth);
json generateXAnyLabelingJSON(
    const std::vector<FinsObject>& details,
    const std::string& imagePath,
    int imageHeight,
    int imageWidth);

json generateXAnyLabelingJSONMulty(
    const std::vector<std::vector<FinsObject>>& details,
    const std::string& imagePath,
    int imageHeight,
    int imageWidth);


void saveJSONToFile(const json& jsonData, const std::string& filename);
std::string GbkToUtf8(const std::string& gbkStr);