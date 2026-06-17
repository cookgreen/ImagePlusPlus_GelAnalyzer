#ifndef PROFILEPLOT_H
#define PROFILEPLOT_H

#include <opencv2/opencv.hpp>
#include <vector>

class ProfilePlot
{
public:
    // 构造函数1：基于矩形区域 ROI
    // 如果 averageHorizontally 为 true，则按行计算平均值（垂直泳道，返回长度为 height 的数组）
    // 如果 averageHorizontally 为 false，则按列计算平均值（水平泳道，返回长度为 width 的数组）
    ProfilePlot(const cv::Mat& img, const cv::Rect& roi, bool averageHorizontally);

    // 构造函数2：基于简单直线
    ProfilePlot(const cv::Mat& img, const cv::Point& p1, const cv::Point& p2);

    // 获取剖面数据
    std::vector<double> getProfile() const;

    // 获取极值
    double getMin();
    double getMax();

private:
    std::vector<double> profile;

    double minVal = 0.0;
    double maxVal = 0.0;
    bool minAndMaxCalculated = false;

    // 核心提取算法
    void calculateRowAverage(const cv::Mat& roiMat);
    void calculateColumnAverage(const cv::Mat& roiMat);
    void calculateStraightLineProfile(const cv::Mat& img, const cv::Point& p1, const cv::Point& p2);

    // 辅助工具：确保图像是单通道灰度图
    cv::Mat ensureGrayscale(const cv::Mat& input) const;
    void findMinAndMax();
};

#endif // PROFILEPLOT_H