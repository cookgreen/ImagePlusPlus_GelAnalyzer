#include "ProfilePlot.h"
#include <cmath>
#include <algorithm>
#include <QColor>

ProfilePlot::ProfilePlot(const QImage& img, const QRect& roi, bool averageHorizontally)
    : imp(img)
{
    // 确保 ROI 在图像边界内
    QRect safeRect = roi.intersected(imp.rect());
    if (safeRect.isEmpty()) {
        return;
    }

    if (averageHorizontally) {
        calculateRowAverage(safeRect);
    } else {
        calculateColumnAverage(safeRect);
    }
}

ProfilePlot::ProfilePlot(const QImage& img, const QLineF& line)
    : imp(img)
{
    calculateStraightLineProfile(line);
}

QVector<double> ProfilePlot::getProfile() const {
    return profile;
}

double ProfilePlot::getMin() {
    if (!minAndMaxCalculated) {
        findMinAndMax();
    }
    return minVal;
}

double ProfilePlot::getMax() {
    if (!minAndMaxCalculated) {
        findMinAndMax();
    }
    return maxVal;
}

void ProfilePlot::calculateRowAverage(const QRect& rect) {
    int width = rect.width();
    int height = rect.height();
    profile.resize(height);
    profile.fill(0.0);

    // 沿水平方向求平均 (对应 ImageJ 的 getRowAverageProfile)
    // 遍历每一行，计算该行中所有像素的平均值
    for (int y = 0; y < height; ++y) {
        double sum = 0.0;
        int imgY = rect.y() + y;
        for (int x = 0; x < width; ++x) {
            int imgX = rect.x() + x;
            sum += getPixelGrayValue(imgX, imgY);
        }
        profile[y] = sum / static_cast<double>(width);
    }
}

void ProfilePlot::calculateColumnAverage(const QRect& rect) {
    int width = rect.width();
    int height = rect.height();
    profile.resize(width);
    profile.fill(0.0);

    // 沿垂直方向求平均 (对应 ImageJ 的 getColumnAverageProfile)
    // 遍历每一列，计算该列中所有像素的平均值
    for (int x = 0; x < width; ++x) {
        double sum = 0.0;
        int imgX = rect.x() + x;
        for (int y = 0; y < height; ++y) {
            int imgY = rect.y() + y;
            sum += getPixelGrayValue(imgX, imgY);
        }
        profile[x] = sum / static_cast<double>(height);
    }
}

void ProfilePlot::calculateStraightLineProfile(const QLineF& line) {
    // 简单的 Bresenham 风格或等距采样算法
    double dx = line.dx();
    double dy = line.dy();
    double length = std::sqrt(dx * dx + dy * dy);

    int numPoints = static_cast<int>(std::round(length));
    if (numPoints <= 0) return;

    profile.resize(numPoints);

    double xInc = dx / numPoints;
    double yInc = dy / numPoints;

    double currentX = line.x1();
    double currentY = line.y1();

    for (int i = 0; i < numPoints; ++i) {
        // 使用最近邻采样，如果需要更高精度，可在此处实现双线性插值 (Bilinear Interpolation)
        int px = static_cast<int>(std::round(currentX));
        int py = static_cast<int>(std::round(currentY));

        profile[i] = getPixelGrayValue(px, py);

        currentX += xInc;
        currentY += yInc;
    }
}

void ProfilePlot::findMinAndMax() {
    if (profile.isEmpty()) {
        minVal = maxVal = 0.0;
        minAndMaxCalculated = true;
        return;
    }

    minVal = profile[0];
    maxVal = profile[0];

    for (double value : profile) {
        if (value < minVal) minVal = value;
        if (value > maxVal) maxVal = value;
    }

    minAndMaxCalculated = true;
}

double ProfilePlot::getPixelGrayValue(int x, int y) const {
    // 边界保护
    if (x < 0 || x >= imp.width() || y < 0 || y >= imp.height()) {
        return 0.0;
    }

    // 根据你的 QImage 格式提取灰度值
    // 如果图像已经是 Format_Grayscale8：
    if (imp.format() == QImage::Format_Grayscale8) {
        return static_cast<double>(imp.pixelIndex(x, y));
    }

    // 如果是 RGB 图像，将其转换为灰度值 (使用 qGray 公式)
    QRgb pixel = imp.pixel(x, y);
    return static_cast<double>(qGray(pixel));
}