#include "MainWindow.h"
#include "LaneGraphicsView.h"
#include "ProfileDialog.h"

#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsPixmapItem>
#include <QPen>
#include <QDebug>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Gel Analyzer Test");
    resize(1000, 700);

    // 菜单
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Open Image...", this, &MainWindow::openImage);
    QAction *analyzeAct = fileMenu->addAction("Analyze", this, &MainWindow::analyze);
    analyzeAct->setShortcut(QKeySequence("Ctrl+A"));

    // 场景和视图
    scene = new QGraphicsScene(this);
    view = new LaneGraphicsView(scene, this);
    setCentralWidget(view);

    // 默认状态提示
    scene->addText("Open an image to begin")->setPos(10, 10);
}

MainWindow::~MainWindow() {}

void MainWindow::openImage() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open Image", "",
                                                    "Images (*.png *.jpg *.tif *.bmp)");
    if (fileName.isEmpty()) return;

    // 使用 OpenCV 读取为灰度图
    cvImage = cv::imread(fileName.toStdString(), cv::IMREAD_GRAYSCALE);
    if (cvImage.empty()) {
        QMessageBox::warning(this, "Error", "Cannot load image.");
        return;
    }

    // 转换为 Qt 显示的 QImage（灰度8位）
    qtImage = QImage(cvImage.data, cvImage.cols, cvImage.rows,
                     static_cast<int>(cvImage.step), QImage::Format_Grayscale8);
    qtImage = qtImage.copy(); // 深拷贝，避免 cv::Mat 数据被释放

    // 更新场景
    scene->clear();
    lanes.clear();
    QGraphicsPixmapItem *pixmapItem = scene->addPixmap(QPixmap::fromImage(qtImage));
    pixmapItem->setPos(0, 0);
    view->setSceneRect(0, 0, cvImage.cols, cvImage.rows);
    view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);

    // 连接视图信号，当用户绘制完矩形时添加泳道
    connect(view, &LaneGraphicsView::laneRectCreated, this, [this](QRectF rect) {
        QPen pen(Qt::red, 2);
        QGraphicsRectItem *rectItem = scene->addRect(rect, pen);
        rectItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
        // 右键点击删除泳道（通过视图的 contextMenuEvent 或直接在此处理）
        // 为简化，这里添加后需要在 LaneGraphicsView 中实现删除（见 LaneGraphicsView::contextMenuEvent）
        lanes.append(rectItem);
    });
}

void MainWindow::analyze() {
    if (cvImage.empty()) {
        QMessageBox::warning(this, "Warning", "No image loaded.");
        return;
    }
    if (lanes.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No lanes defined. Draw rectangles on the image.");
        return;
    }

    qDebug() << "Plotting" << lanes.size() << "lanes";

    // ==========================================
    // 1. OpenCV 图像全局预处理
    // ==========================================
    cv::Mat processedMat = cvImage.clone();

    // 转为单通道灰度图
    if (processedMat.channels() > 1) {
        cv::cvtColor(processedMat, processedMat, cv::COLOR_BGR2GRAY);
    }

    // 对应: if (uncalibratedOD && imp2.depth() > 8) new ImageConverter(imp2).convertToGray8();
    // 将 16位/32位 图像归一化并转换为 8位 (CV_8U)
    if (uncalibratedOD && processedMat.depth() != CV_8U) {
        cv::normalize(processedMat, processedMat, 0, 255, cv::NORM_MINMAX, CV_8U);
    }

    // 对应: if (invertPeaks) ip2.invert();
    if (invertPeaks) {
        // 只有 8位图 反相才有 255-x 的物理意义
        if (processedMat.depth() == CV_8U) {
            cv::bitwise_not(processedMat, processedMat);
        }
    }

    // ==========================================
    // 2. 提取轮廓与处理
    // ==========================================
    std::vector<std::vector<double>> allProfiles;
    double globalMin = std::numeric_limits<double>::max();
    double globalMax = -std::numeric_limits<double>::max();

    // 重置全局 OD 极值 (假设 odMin, odMax 是 MainWindow 的成员变量)
    if (uncalibratedOD) {
        odMin = std::numeric_limits<double>::max();
        odMax = -std::numeric_limits<double>::max();
    }

    int pixelsAveraged = 1; // 记录泳道厚度

    // 遍历用户在 UI 上画的所有矩形框
    for (const QGraphicsRectItem *item : lanes) {
        QRectF rf = item->rect();

        // 映射到图像真实坐标 (需要确保你的 QGraphicsRectItem 是基于图像原点放置的)
        cv::Rect cvRoi(rf.x(), rf.y(), rf.width(), rf.height());

        // 对应 ImageJ 的 isVertical 判断。
        bool averageHorizontally = (cvRoi.height > cvRoi.width);

        // 获取泳道的宽度(厚度)
        pixelsAveraged = averageHorizontally ? cvRoi.width : cvRoi.height;

        // 调用刚才写的 OpenCV 版 ProfilePlot
        ProfilePlot plot(processedMat, cvRoi, averageHorizontally);
        std::vector<double> profile = plot.getProfile();

        if (profile.empty()) continue;

        // 3. 应用 OD (光密度) 转换
        if (uncalibratedOD) {
            profile = calculateOD(profile);
        }

        // 更新这根泳道的最大最小值
        auto minmax = std::minmax_element(profile.begin(), profile.end());
        if (*minmax.first < globalMin) globalMin = *minmax.first;
        if (*minmax.second > globalMax) globalMax = *minmax.second;

        allProfiles.push_back(profile);
    }

    if (allProfiles.empty()) {
        QMessageBox::warning(this, "Warning", "No valid lane data.");
        return;
    }

    // 应用 OD 极值覆盖
    if (uncalibratedOD) {
        globalMin = odMin;
        globalMax = odMax;
    }

    // ==========================================
    // 4. 展示图表
    // ==========================================
    // 此处调用你的图表对话框，传入处理好的数据和极值
    ProfileDialog dlg(allProfiles, globalMin, globalMax, pixelsAveraged, this);
    dlg.exec();
}

std::vector<double> MainWindow::calculateOD(const std::vector<double>& profile) {
    std::vector<double> odProfile(profile.size());

    for (size_t i = 0; i < profile.size(); ++i) {
        // 安全处理：防止像素值达到255导致 std::log(0) 产生 Infinity
        double pixelValue = std::min(profile[i], 254.999);

        // 0.434294481 = 1 / ln(10)，相当于计算 log10
        double v = 0.434294481 * std::log(255.0 / (255.0 - pixelValue));

        // 更新类成员变量极值
        if (v < odMin) odMin = v;
        if (v > odMax) odMax = v;

        odProfile[i] = v;
    }

    return odProfile;
}