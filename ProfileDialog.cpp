#include "ProfileDialog.h"
#include <QPainter>
#include <QMouseEvent>
#include <QToolBar>
#include <QAction>
#include <QVBoxLayout>
#include <QDebug>
#include <opencv2/opencv.hpp>

ProfileDialog::ProfileDialog(const std::vector<std::vector<double>> &profiles,
                             double minVal, double maxVal,
                             int pixelsAveraged,
                             QWidget *parent)
    : QDialog(parent), m_profiles(profiles), m_min(minVal), m_max(maxVal), m_pixelsAveraged(pixelsAveraged)
{
    setWindowTitle("Lane Profiles Analyzer");
    setMinimumSize(650, 400);
    resize(800, 500);

    // 1. 初始化工具栏
    m_toolBar = new QToolBar(this);
    m_toolBar->move(0, 0); // 固定在顶部

    m_actionLine = m_toolBar->addAction("Straight Line");
    m_actionWand = m_toolBar->addAction("Wand Tool");
    m_toolBar->addSeparator();
    m_actionClear = m_toolBar->addAction("Clear All");

    m_actionLine->setCheckable(true);
    m_actionWand->setCheckable(true);

    // 互斥选中逻辑
    connect(m_actionLine, &QAction::triggered, this, [this](bool checked) {
        if (checked) { m_actionWand->setChecked(false); m_currentTool = LineTool; }
        else m_currentTool = None;
    });
    connect(m_actionWand, &QAction::triggered, this, [this](bool checked) {
        if (checked) { m_actionLine->setChecked(false); m_currentTool = WandTool; }
        else m_currentTool = None;
    });
    connect(m_actionClear, &QAction::triggered, this, [this]() {
        m_userLines.clear();
        m_wandSelections.clear();
        update();
    });
}

void ProfileDialog::paintEvent(QPaintEvent *) {
    m_toolBar->resize(width(), m_toolBar->height()); // 确保工具栏撑满宽度

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true); // 开启抗锯齿
    p.fillRect(rect(), Qt::white);

    if (m_profiles.empty()) return;

    int plotAreaWidth = width() - leftMargin - rightMargin;
    int plotAreaHeight = height() - topMargin - bottomMargin;
    if (plotAreaWidth <= 0 || plotAreaHeight <= 0) return;

    // 标题
    p.setPen(Qt::black);
    p.setFont(QFont("Sans", 9));
    p.drawText(leftMargin, topMargin - 15, QString("Gel Profiles (Image width: %1 px)").arg(m_imageWidth));

    // 转移坐标系到绘图区左上角
    p.save();
    p.translate(leftMargin, topMargin);

    // 调用核心绘制逻辑
    drawPlotContent(&p, plotAreaWidth, plotAreaHeight);

    // 绘制魔棒生成的选区 (类似 ImageJ 的黄色半透明高亮)
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 0, 100)); // 黄色，透明度 100
    for (const QPolygon& poly : m_wandSelections) {
        p.drawPolygon(poly);
    }

    for (int i = 0; i < m_peaks.size(); ++i) {
        const PeakInfo& peak = m_peaks[i];

        // 2. 在质心位置画出面积和编号
        p.setPen(Qt::black);
        p.setFont(QFont("Arial", 10, QFont::Bold));

        // 格式化文本：编号 + 面积大小
        QString text = QString("%1: %2").arg(i + 1).arg(static_cast<int>(peak.area));

        // 计算文本包围盒以实现完美居中
        QFontMetrics fm = p.fontMetrics();
        int textWidth = fm.horizontalAdvance(text);
        int textHeight = fm.height();

        QPoint textPos(peak.center.x() - textWidth / 2, peak.center.y() + textHeight / 4);

        // （可选）给文字画个白色半透明底色，防止波峰里的竖线干扰阅读
        QRect textBgRect(textPos.x() - 2, textPos.y() - textHeight + 3, textWidth + 4, textHeight + 2);
        p.fillRect(textBgRect, QColor(255, 255, 255, 180));

        p.drawText(textPos, text);
    }

    // 绘制正在拖拽的直线
    if (m_isDrawingLine) {
        p.setPen(QPen(Qt::blue, 1, Qt::DashLine));
        p.drawLine(m_lineStart, m_lineEnd);
    }

    p.restore();
}

// 核心数据绘制分离，为了供屏幕显示和 OpenCV 掩码提取共同调用
void ProfileDialog::drawPlotContent(QPainter *p, int width, int height) {
    int nLanes = static_cast<int>(m_profiles.size());
    int laneHeight = height / nLanes;
    if (laneHeight < 2) laneHeight = 2;

    double xScale = static_cast<double>(width) / (m_profiles[0].size() - 1);
    double yRange = m_max - m_min;
    if (yRange == 0.0) yRange = 1.0;
    double yScale = static_cast<double>(laneHeight) / yRange;

    // 画曲线和基线
    for (int i = 0; i < nLanes; ++i) {
        const auto &profile = m_profiles[i];
        int baseY = (i + 1) * laneHeight - 1;

        p->setPen(QPen(Qt::lightGray, 1));
        p->drawLine(0, baseY, width, baseY);

        p->setPen(QPen(Qt::black, 1)); // 曲线必须是黑色
        if (!profile.empty()) {
            QPolygon curvePoly;
            for (size_t j = 0; j < profile.size(); ++j) {
                int x = static_cast<int>(j * xScale);
                int y = baseY - static_cast<int>((profile[j] - m_min) * yScale);
                curvePoly << QPoint(x, y);
            }
            p->drawPolyline(curvePoly);
        }
    }

    // 画用户封口的直线
    p->setPen(QPen(Qt::black, 1));
    for (const QLine& line : m_userLines) {
        p->drawLine(line);
    }
}

// ----------------- 鼠标交互事件 -----------------

void ProfileDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // 映射到绘图区坐标
        QPoint plotPos = event->pos() - QPoint(leftMargin, topMargin);

        if (m_currentTool == LineTool) {
            m_isDrawingLine = true;
            m_lineStart = plotPos;
            m_lineEnd = plotPos;
        } else if (m_currentTool == WandTool) {
            applyWandTool(plotPos);
        }
    }
}

void ProfileDialog::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDrawingLine && (event->buttons() & Qt::LeftButton)) {
        m_lineEnd = event->pos() - QPoint(leftMargin, topMargin);
        update();
    }
}

void ProfileDialog::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_isDrawingLine) {
        m_isDrawingLine = false;
        m_lineEnd = event->pos() - QPoint(leftMargin, topMargin);
        if (m_lineStart != m_lineEnd) {
            m_userLines.append(QLine(m_lineStart, m_lineEnd));
        }
        update();
    }
}

// ----------------- OpenCV 魔法：魔棒工具 -----------------

void ProfileDialog::applyWandTool(const QPoint& clickPos) {
    int w = width() - leftMargin - rightMargin;
    int h = height() - topMargin - bottomMargin;
    if (w <= 0 || h <= 0 || clickPos.x() < 0 || clickPos.x() >= w || clickPos.y() < 0 || clickPos.y() >= h)
        return;

    // 1. 在内存中生成一张用于分析的二值化图像 (白底黑线)
    QImage maskImg(w, h, QImage::Format_Grayscale8);
    maskImg.fill(Qt::white);
    QPainter p(&maskImg);
    drawPlotContent(&p, w, h);
    p.end();

    // 2. 将 QImage 转为 OpenCV 的 Mat (避免拷贝)
    cv::Mat mat(h, w, CV_8UC1, maskImg.bits(), maskImg.bytesPerLine());

    // 如果用户直接点在了黑线上，无视
    if (mat.at<uchar>(clickPos.y(), clickPos.x()) == 0) return;

    // 3. 使用 cv::floodFill 提取连通区域掩码
    // OpenCV 要求 mask 尺寸比原图大 2 个像素
    cv::Mat mask = cv::Mat::zeros(h + 2, w + 2, CV_8UC1);

    // floodFill 标志：填充值设为 255 (写入mask)，并只返回掩码
    int flags = 4 | (255 << 8) | cv::FLOODFILL_MASK_ONLY;
    cv::Rect boundingBox;
    cv::floodFill(mat, mask, cv::Point(clickPos.x(), clickPos.y()),
                  cv::Scalar(255), &boundingBox, cv::Scalar(0), cv::Scalar(0), flags);

    // 如果封闭区域过大（比如点击了曲线外部的背景区），则忽略
    if (boundingBox.area() > (w * h * 0.8)) {
        return;
    }

    // 4. 使用 cv::findContours 从掩码中提取多边形边界
    // 裁剪掉扩大的 1px 边界，恢复真实坐标
    cv::Mat maskROI = mask(cv::Rect(1, 1, w, h));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(maskROI, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return;

    // 取最大的轮廓 (排除噪点)
    size_t largestIdx = 0;
    for(size_t i = 1; i < contours.size(); i++) {
        if(cv::contourArea(contours[i]) > cv::contourArea(contours[largestIdx]))
            largestIdx = i;
    }

    const auto& bestContour = contours[largestIdx];



    double pixelCount = cv::contourArea(bestContour);

    double perimeter = cv::arcLength(bestContour, true);

    // 完美复刻 ImageJ 的边界补偿算法
    double rawArea = pixelCount + perimeter / 2.0;

    // ==========================================
    // 2. 获取校准比例 (完美对齐 cal.pixelWidth * cal.pixelHeight)
    // ==========================================
    int ijPlotWidth = m_pixelsAveraged; // 对应 firstRect.width
    if (ijPlotWidth < 650) ijPlotWidth = 650;
    int ijPlotHeight = ijPlotWidth / 2;
    if (ijPlotHeight < 250) ijPlotHeight = 250;

    double dataPoints = static_cast<double>(m_profiles[0].size() - 1);
    if (dataPoints <= 0) dataPoints = 1.0;
    double yRange = m_max - m_min;
    if (yRange == 0.0) yRange = 1.0;

    // ImageJ 的静态画布比例
    double ijXScale = ijPlotWidth / dataPoints;
    double ijYScale = ijPlotHeight / yRange;

    // Qt 的动态画布比例
    int nLanes = static_cast<int>(m_profiles.size());
    int laneHeight = h / nLanes;
    if (laneHeight < 2) laneHeight = 2;

    double qtXScale = static_cast<double>(w) / dataPoints;
    double qtYScale = static_cast<double>(laneHeight) / yRange;

    // 计算校准乘子 (相当于 cal.pixelWidth 和 cal.pixelHeight)
    double calPixelWidth = ijXScale / qtXScale;
    double calPixelHeight = ijYScale / qtYScale;

    // ==========================================
    // 3. 最终校准面积
    // ==========================================
    double realArea = rawArea * calPixelWidth * calPixelHeight;

    // 2. 计算轮廓的“质心”(Center of Mass)，用于在波峰正中心画上数字
    cv::Moments M = cv::moments(bestContour);
    int cx = 0, cy = 0;
    if (M.m00 != 0) { // 避免除以 0
        cx = static_cast<int>(M.m10 / M.m00);
        cy = static_cast<int>(M.m01 / M.m00);
    }

    // 5. 转换为 Qt 的 QPolygon 进行绘制
    QPolygon poly;
    for (const auto& pt : bestContour) {
        poly << QPoint(pt.x, pt.y);
    }

    m_wandSelections.append(poly);

    // TODO: 这里可以直接通过 cv::contourArea(contours[largestIdx])
    // 或者统计 maskROI 中的白色像素个数，计算出该波峰的积分面积！

    // 保存波峰信息
    m_peaks.append({poly, realArea, QPoint(cx, cy)});

    // 更新 UI
    update();
}