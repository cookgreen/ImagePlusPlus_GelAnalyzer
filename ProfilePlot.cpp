#include "ProfilePlot.h"
#include <algorithm>

ProfilePlot::ProfilePlot(const cv::Mat& img, const cv::Rect& roi, bool averageHorizontally)
{
    // 安全边界检查：确保 ROI 不会越界
    cv::Rect safeRoi = roi & cv::Rect(0, 0, img.cols, img.rows);
    if (safeRoi.area() == 0) {
        return;
    }

    // 提取 ROI 并确保它是单通道灰度图
    cv::Mat roiMat = ensureGrayscale(img(safeRoi));

    // 计算剖面
    if (averageHorizontally) {
        calculateRowAverage(roiMat);
    } else {
        calculateColumnAverage(roiMat);
    }
}

ProfilePlot::ProfilePlot(const cv::Mat& img, const cv::Point& p1, const cv::Point& p2)
{
    calculateStraightLineProfile(ensureGrayscale(img), p1, p2);
}

std::vector<double> ProfilePlot::getProfile() const {
    return profile;
}

double ProfilePlot::getMin() {
    if (!minAndMaxCalculated) findMinAndMax();
    return minVal;
}

double ProfilePlot::getMax() {
    if (!minAndMaxCalculated) findMinAndMax();
    return maxVal;
}

void ProfilePlot::calculateRowAverage(const cv::Mat& roiMat) {
    cv::Mat reduced;
    cv::reduce(roiMat, reduced, 1, cv::REDUCE_AVG, CV_64F);

    // 将结果直接复制给 std::vector
    profile.assign((double*)reduced.datastart, (double*)reduced.dataend);
}

void ProfilePlot::calculateColumnAverage(const cv::Mat& roiMat) {
    cv::Mat reduced;
    cv::reduce(roiMat, reduced, 0, cv::REDUCE_AVG, CV_64F);

    profile.assign((double*)reduced.datastart, (double*)reduced.dataend);
}

void ProfilePlot::calculateStraightLineProfile(const cv::Mat& img, const cv::Point& p1, const cv::Point& p2) {
    cv::LineIterator it(img, p1, p2, 8); // 8连通

    profile.resize(it.count);

    for(int i = 0; i < it.count; ++i, ++it) {
        profile[i] = static_cast<double>((*it)[0]);
    }
}

cv::Mat ProfilePlot::ensureGrayscale(const cv::Mat& input) const {
    if (input.channels() == 1) {
        return input;
    }
    cv::Mat gray;
    if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    } else if (input.channels() == 4) {
        cv::cvtColor(input, gray, cv::COLOR_BGRA2GRAY);
    } else {
        gray = input.clone(); // Fallback
    }
    return gray;
}

void ProfilePlot::findMinAndMax() {
    if (profile.empty()) {
        minVal = maxVal = 0.0;
        minAndMaxCalculated = true;
        return;
    }

    auto minmax = std::minmax_element(profile.begin(), profile.end());
    minVal = *minmax.first;
    maxVal = *minmax.second;
    minAndMaxCalculated = true;
}