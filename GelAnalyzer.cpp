#include "GelAnalyzer.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPainter>
#include <QTransform>
#include <QGuiApplication>
#include <QScreen>
#include <cmath>
#include <algorithm>
#include <QDebug>

GelAnalyzer::GelAnalyzer(QObject *parent)
    : QObject(parent), xPositions(MAX_LANES + 1, 0)
{
}

void GelAnalyzer::run(const QString& arg) {
    if (arg == "options") {
        showDialog();
        return;
    }

    currentImage = getCurrentImageFromWindowManager();
    if (currentImage.isNull()) {
        showMessage("No image available.");
        return;
    }

    if (arg == "reset") {
        nLanes = 0;
        saveNLanes = 0;
        saveID = 0;
        // 清理绘图画布和Overlay的逻辑 (视你的UI实现而定)
        return;
    }

    if (arg == "percent") {
        // plotsCanvas.displayPercentages();
        showMessage("Display Percentages not implemented yet.");
        return;
    }

    if (arg == "label") {
        // plotsCanvas.labelPeaks();
        showMessage("Label Peaks not implemented yet.");
        return;
    }

    // 假设用 QImage 的 cacheKey() 来模拟图像的 ID
    if (currentImage.cacheKey() != saveID) {
        nLanes = 0;
        saveID = 0;
    }

    if (arg == "replot") {
        if (saveNLanes == 0) {
            showMessage("The data needed to re-plot the lanes is not available");
            return;
        }
        nLanes = saveNLanes;
        plotLanes(gel, true);
        return;
    }

    if (arg == "draw") {
        outlineLanes();
        return;
    }

    // 模拟获取用户当前在图像上框选的 ROI
    // 在实际 Qt 应用中，这通常由你的自定义 QGraphicsView/QGraphicsScene 提供
    QRect rect; /* = yourGraphicsView->getCurrentRoi(); */
    if (rect.isNull()) {
        showMessage("Rectangular selection required.");
        return;
    }

    if (arg == "first") {
        selectFirstLane(rect);
        return;
    }

    if (nLanes == 0) {
        showMessage("You must first use the \"Select First Lane\" command.");
        return;
    }

    if (arg == "next") {
        selectNextLane(rect);
        return;
    }

    if (arg == "plot") {
        if ((isVertical && (rect.x() != xPositions[nLanes])) ||
            (!isVertical && (rect.y() != xPositions[nLanes]))) {
            selectNextLane(rect);
        }
        plotLanes(gel, false);
        return;
    }
}

void GelAnalyzer::showDialog() {
    // 模拟 ImageJ 的 GenericDialog
    QDialog dialog;
    dialog.setWindowTitle("Gel Analyzer Options");
    QFormLayout form(&dialog);

    QDoubleSpinBox vScaleBox;
    vScaleBox.setValue(verticalScaleFactor);
    QDoubleSpinBox hScaleBox;
    hScaleBox.setValue(horizontalScaleFactor);
    QCheckBox odBox("Uncalibrated OD");
    odBox.setChecked(uncalibratedOD);
    QCheckBox percentBox("Label with percentages");
    percentBox.setChecked(labelWithPercentages);
    QCheckBox invertBox("Invert peaks");
    invertBox.setChecked(invertPeaks);

    form.addRow("Vertical scale factor:", &vScaleBox);
    form.addRow("Horizontal scale factor:", &hScaleBox);
    form.addRow(&odBox);
    form.addRow(&percentBox);
    form.addRow(&invertBox);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        verticalScaleFactor = vScaleBox.value() == 0.0 ? 1.0 : vScaleBox.value();
        horizontalScaleFactor = hScaleBox.value() == 0.0 ? 1.0 : hScaleBox.value();
        uncalibratedOD = odBox.isChecked();
        labelWithPercentages = percentBox.isChecked();
        invertPeaks = invertBox.isChecked();
    }
}

void GelAnalyzer::selectFirstLane(const QRect& rect) {
    if (rect.width() / (double)rect.height() >= 2.0 /* || 是否按下Alt键 */) {
        if (showLaneDialog) {
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(nullptr, "Gel Analyzer",
                                          "Are the lanes really horizontal?\n\nImageJ assumes the lanes are\nhorizontal if the selection is more\nthan twice as wide as it is tall.",
                                          QMessageBox::Yes|QMessageBox::No);
            if (reply == QMessageBox::No) return;
            showLaneDialog = false;
        }
        isVertical = false;
    } else {
        isVertical = true;
    }

    firstRect = rect;
    nLanes = 1;
    saveNLanes = 0;

    if (isVertical)
        xPositions[1] = rect.x();
    else
        xPositions[1] = rect.y();

    gel = currentImage;
    saveID = currentImage.cacheKey();

    updateRoiList(rect);
    // IJ.showStatus("Lane 1 selected..."); (状态栏更新)
}

void GelAnalyzer::selectNextLane(QRect rect) {
    if (rect.width() != firstRect.width() || rect.height() != firstRect.height()) {
        showMessage("Selections must all be the same size.");
        return;
    }

    if (nLanes < MAX_LANES) {
        nLanes += 1;
    }

    if (isVertical)
        xPositions[nLanes] = rect.x();
    else
        xPositions[nLanes] = rect.y();

    // 避免重复添加
    if (xPositions[nLanes] == xPositions[nLanes - 1]) {
        nLanes--;
        return;
    }

    // 对齐到第一条Lane的坐标
    if (isVertical && rect.y() != firstRect.y()) {
        rect.moveTop(firstRect.y());
    } else if (!isVertical && rect.x() != firstRect.x()) {
        rect.moveLeft(firstRect.x());
    }

    updateRoiList(rect);
}

void GelAnalyzer::updateRoiList(const QRect& rect) {
    if (gel.isNull()) return;
    // 在 Qt 中，这里应该触发信号，通知 QGraphicsScene 增加一个矩形框图元 (QGraphicsRectItem)
    qDebug() << "Added ROI:" << rect;
}

void GelAnalyzer::plotLanes(const QImage& imp, bool replot) {
    int topMargin = 16;
    int bottomMargin = 2;
    double minVal = std::numeric_limits<double>::max();
    double maxVal = -std::numeric_limits<double>::max();
    int plotWidth;

    // C++ 中推荐使用 0-based 索引，所以大小设为 nLanes
    QVector<QVector<double>> profiles(nLanes);

    qDebug() << "Plotting" << nLanes << "lanes";

    // 1. 图像预处理 (旋转、格式转换、反相)
    QImage ipRotated = imp;
    if (isVertical) {
        QTransform transform;
        transform.rotate(-90); // 对应 rotateLeft()
        ipRotated = ipRotated.transformed(transform);
    }

    QImage imp2 = ipRotated;
    // 如果是未标定光密度且图像位深大于8位，转为8位灰度
    if (uncalibratedOD && imp2.depth() > 8) {
        imp2 = imp2.convertToFormat(QImage::Format_Grayscale8);
    }

    if (invertPeaks) {
        imp2.invertPixels();
    }

    // 2. 提取每个Lane的轮廓数据 (Profile)
    for (int i = 0; i < nLanes; ++i) {
        QRect roi;
        if (isVertical) {
            // 注意：Qt中图像的原点(0,0)在左上角
            roi = QRect(firstRect.y(),
                        ipRotated.height() - x[i] - firstRect.width(),
                        firstRect.height(),
                        firstRect.width());
        } else {
            roi = QRect(firstRect.x(), x[i], firstRect.width(), firstRect.height());
        }

        // 调用自定义函数获取该ROI的投影剖面线 (类似 ImageJ 的 ProfilePlot)
        profiles[i] = getProfile(imp2, roi);

        // 更新全局最大最小值
        auto [pMin, pMax] = std::minmax_element(profiles[i].begin(), profiles[i].end());
        if (*pMin < minVal) minVal = *pMin;
        if (*pMax > maxVal) maxVal = *pMax;

        if (uncalibratedOD) {
            profiles[i] = od(profiles[i]); // 调用自定义的OD转换函数
        }
    }

    if (uncalibratedOD) {
        minVal = odMin;
        maxVal = odMax;
    }

    // 3. 计算图表尺寸
    plotWidth = isVertical ? firstRect.height() : firstRect.width();
    if (plotWidth < 650) plotWidth = 650;

    int maxWidth = isVertical ? 4 * firstRect.height() : 4 * firstRect.width();
    if (plotWidth > maxWidth) plotWidth = maxWidth;

    if (verticalScaleFactor == 0.0) verticalScaleFactor = 1.0;
    if (horizontalScaleFactor == 0.0) horizontalScaleFactor = 1.0;

    // 获取屏幕尺寸
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int screenW = screenGeometry.width();

    if (plotWidth > screenW - screenW / 6) {
        plotWidth = screenW - screenW / 6;
    }

    plotWidth = static_cast<int>(plotWidth * horizontalScaleFactor);
    int plotHeight = plotWidth / 2;
    if (plotHeight < 250) plotHeight = 250;
    plotHeight = static_cast<int>(plotHeight * verticalScaleFactor);

    // 4. 创建画布并使用 QPainter 绘制
    int imgHeight = topMargin + nLanes * plotHeight + bottomMargin;
    QImage plotImage(plotWidth, imgHeight, QImage::Format_RGB32);
    plotImage.fill(Qt::white); // 填充白底

    QPainter painter(&plotImage);
    painter.setPen(Qt::black);
    painter.setRenderHint(QPainter::Antialiasing); // 开启抗锯齿让曲线平滑

    // 绘制外边框
    int h = plotImage.height();
    painter.drawRect(0, 0, plotWidth - 1, h - 1);
    painter.drawLine(0, h - 2, plotWidth - 1, h - 2);

    // 绘制标题字符串
    QString s = "Image Title; "; // 这里替换为你的实际图像标题逻辑
    if (uncalibratedOD) {
        s += "Uncalibrated OD";
    } else {
        s += "Uncalibrated";
    }
    painter.drawText(5, topMargin, s);

    // 5. 绘制数据曲线
    double xScale = static_cast<double>(plotWidth) / profiles[0].size();
    double yScale = (maxVal - minVal == 0.0) ? 1.0 : (plotHeight / (maxVal - minVal));

    for (int i = 0; i < nLanes; ++i) {
        const QVector<double>& profile = profiles[i];
        int top = i * plotHeight + topMargin;
        int base = top + plotHeight;

        // 绘制基线
        painter.drawLine(0, base, static_cast<int>(profile.size() * xScale), base);

        // 构建数据曲线多边形路径 (比单次循环 drawLine 更高效)
        QPolygonF polygon;
        for (int j = 0; j < profile.size(); ++j) {
            double px = j * xScale;
            double py = base - (profile[j] - minVal) * yScale;
            polygon << QPointF(px, py);
        }

        painter.drawPolyline(polygon);
    }

    painter.end();

    // 6. 展示与后续清理逻辑
    // 在Qt中，通常将 QImage 转换为 QPixmap 显示在 QLabel 中，或者直接发出信号
    showPlotWindow(plotImage);

    // 重置变量
    // saveNLanes = nLanes;
    // nLanes = 0;
    // saveID = 0;


    qDebug() << "Plotting lanes...";
    saveNLanes = nLanes;
    nLanes = 0;
    saveID = 0;
}

QVector<double> GelAnalyzer::od(const QVector<double>& profile) {
    QVector<double> odProfile(profile.size());
    for (int i = 0; i < profile.size(); ++i) {
        double pixelValue = std::min(profile[i], 254.999);
        double v = 0.434294481 * std::log(255.0 / (255.0 - pixelValue));
        if (v < odMin) odMin = v;
        if (v > odMax) odMax = v;
        odProfile[i] = v;
    }
    return odProfile;
}

void GelAnalyzer::outlineLanes() {
    if (gel.isNull() /* || overlay为空 */) {
        showMessage("Data needed to outline lanes is no longer available.");
        return;
    }

    // 复制原图作为画布
    QImage lanesImg = gel.convertToFormat(QImage::Format_RGB32);
    QPainter painter(&lanesImg);

    // 配置画笔
    QPen pen(Qt::red); // Qt 中直接用红色画笔代替 ImageJ 的 CustomLUT 修改
    pen.setWidth(1); // 可以根据缩放比例调整线宽
    painter.setPen(pen);

    QFont font("Helvetica", 12);
    painter.setFont(font);

    // 遍历所有记录的 ROI 进行绘制 (此处假设你已将添加的 ROI 存入一个容器中，如 QList<QRect> overlays)
    // 这里用推导的位置作为演示
    for (int i = 1; i <= saveNLanes; i++) {
        QRect r = firstRect;
        if (isVertical) {
            r.moveLeft(xPositions[i]);
        } else {
            r.moveTop(xPositions[i]);
        }

        painter.drawRect(r);

        // 绘制编号文本
        QString s = QString::number(i);
        QFontMetrics fm(font);
        int textWidth = fm.horizontalAdvance(s);

        if (isVertical) {
            int yloc = r.y() + 14;
            painter.drawText(r.x() + r.width() / 2 - textWidth / 2, yloc, s);
        } else {
            int xloc = r.x() - textWidth - 2;
            painter.drawText(xloc, r.y() + r.height() / 2 + 6, s);
        }
    }

    painter.end();

    // 显示绘制了轮廓的图像
    // emits showImageSignal(lanesImg);
}

void GelAnalyzer::showMessage(const QString& msg) {
    QMessageBox::information(nullptr, "Gel Analyzer", msg);
}

QImage GelAnalyzer::getGelImage() const {
    return gel;
}

QVector<double> GelAnalyzer::getProfile(const QImage& img, const QRect& roi) {
    // 【需自行实现】
    // 对应 ImageJ 的 ProfilePlot 功能
    // 需要沿 ROI 区域计算每行/每列像素的平均灰度值
    return QVector<double>();
}