#include "ProfilePlot.h"
#include <algorithm>

ProfilePlot::ProfilePlot(const cv::Mat& img, const cv::Rect& roi, bool averageHorizontally)
{
    // 1. 安全边界检查：确保 ROI 不会越界
    cv::Rect safeRoi = roi & cv::Rect(0, 0, img.cols, img.rows);
    if (safeRoi.area() == 0) {
        return;
    }

    // 2. 提取 ROI 并确保它是单通道灰度图
    cv::Mat roiMat = ensureGrayscale(img(safeRoi));

    // 3. 计算剖面
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
    // 垂直泳道：逐行求平均。即把矩阵沿水平方向压缩为一列。
    // cv::reduce 参数含义： 1 表示按行压缩 (变成单列)，cv::REDUCE_AVG 表示求平均，CV_64F 表示输出类型为 double
    cv::Mat reduced;
    cv::reduce(roiMat, reduced, 1, cv::REDUCE_AVG, CV_64F);

    // 将结果直接复制给 std::vector
    profile.assign((double*)reduced.datastart, (double*)reduced.dataend);
}

void ProfilePlot::calculateColumnAverage(const cv::Mat& roiMat) {
    // 水平泳道：逐列求平均。即把矩阵沿垂直方向压缩为一行。
    // 参数 0 表示按列压缩 (变成单行)
    cv::Mat reduced;
    cv::reduce(roiMat, reduced, 0, cv::REDUCE_AVG, CV_64F);

    profile.assign((double*)reduced.datastart, (double*)reduced.dataend);
}

void ProfilePlot::calculateStraightLineProfile(const cv::Mat& img, const cv::Point& p1, const cv::Point& p2) {
    // 使用 OpenCV 原生的线段迭代器提取两点之间的像素
    cv::LineIterator it(img, p1, p2, 8); // 8连通

    profile.resize(it.count);

    for(int i = 0; i < it.count; ++i, ++it) {
        // 由于我们在外部保证了是灰度图 (单通道)，直接取第0个元素
        // 如果原始图是 CV_8U，取值就是 uchar；这里统一转为 double
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