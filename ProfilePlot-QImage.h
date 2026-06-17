#ifndef PROFILEPLOT_H
#define PROFILEPLOT_H

#include <QImage>
#include <QRect>
#include <QLineF>
#include <QPolygonF>
#include <QVector>

class ProfilePlot
{
public:
    // 构造函数1：基于矩形区域 (对应原版的 getRowAverageProfile 和 getColumnAverageProfile)
    // 如果 averageHorizontally 为 true，则按行计算平均值（返回长度为 height 的数组）
    // 如果 averageHorizontally 为 false，则按列计算平均值（返回长度为 width 的数组）
    ProfilePlot(const QImage& img, const QRect& roi, bool averageHorizontally);

    // 构造函数2：基于简单直线 (对应原版的 getStraightLineProfile)
    ProfilePlot(const QImage& img, const QLineF& line);

    // 获取剖面数据
    QVector<double> getProfile() const;

    // 获取极值
    double getMin();
    double getMax();

private:
    QImage imp;
    QVector<double> profile;

    double minVal = 0.0;
    double maxVal = 0.0;
    bool minAndMaxCalculated = false;

    // 核心提取算法
    void calculateRowAverage(const QRect& rect);
    void calculateColumnAverage(const QRect& rect);
    void calculateStraightLineProfile(const QLineF& line);

    // 辅助计算
    void findMinAndMax();

    // 安全获取灰度值的辅助函数
    double getPixelGrayValue(int x, int y) const;
};

#endif // PROFILEPLOT_H