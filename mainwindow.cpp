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
    setWindowTitle("ImagePlusPlus - Gel Analyzer");
    resize(1000, 700);

    // 菜单
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Open Image...", this, &MainWindow::openImage);
    QAction *analyzeAct = fileMenu->addAction("Analyze", this, &MainWindow::analyze);
    analyzeAct->setShortcut(QKeySequence("Ctrl+A"));

    scene = new QGraphicsScene(this);
    view = new LaneGraphicsView(scene, this);
    setCentralWidget(view);

    scene->addText("Open an image to begin")->setPos(10, 10);
}

MainWindow::~MainWindow() {}

void MainWindow::openImage() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open Image", "",
                                                    "Images (*.png *.jpg *.tif *.bmp)");
    if (fileName.isEmpty()) return;

    cvImage = cv::imread(fileName.toStdString(), cv::IMREAD_GRAYSCALE);
    if (cvImage.empty()) {
        QMessageBox::warning(this, "Error", "Cannot load image.");
        return;
    }

    qtImage = QImage(cvImage.data, cvImage.cols, cvImage.rows,
                     static_cast<int>(cvImage.step), QImage::Format_Grayscale8);
    qtImage = qtImage.copy();

    scene->clear();
    lanes.clear();
    QGraphicsPixmapItem *pixmapItem = scene->addPixmap(QPixmap::fromImage(qtImage));
    pixmapItem->setPos(0, 0);
    view->setSceneRect(0, 0, cvImage.cols, cvImage.rows);
    view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);

    connect(view, &LaneGraphicsView::laneRectCreated, this, [this](QRectF rect) {
        QPen pen(Qt::red, 2);
        QGraphicsRectItem *rectItem = scene->addRect(rect, pen);
        rectItem->setFlag(QGraphicsItem::ItemIsSelectable, true);

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

    cv::Mat processedMat = cvImage.clone();

    // 转为单通道灰度图
    if (processedMat.channels() > 1) {
        cv::cvtColor(processedMat, processedMat, cv::COLOR_BGR2GRAY);
    }

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

    std::vector<std::vector<double>> allProfiles;
    double globalMin = std::numeric_limits<double>::max();
    double globalMax = -std::numeric_limits<double>::max();

    // 重置全局 OD 极值 (假设 odMin, odMax 是 MainWindow 的成员变量)
    if (uncalibratedOD) {
        odMin = std::numeric_limits<double>::max();
        odMax = -std::numeric_limits<double>::max();
    }

    int pixelsAveraged = 1; // 记录泳道厚度

    for (const QGraphicsRectItem *item : lanes) {
        QRectF rf = item->rect();

        cv::Rect cvRoi(rf.x(), rf.y(), rf.width(), rf.height());

        bool averageHorizontally = (cvRoi.height > cvRoi.width);

        pixelsAveraged = averageHorizontally ? cvRoi.width : cvRoi.height;

        ProfilePlot plot(processedMat, cvRoi, averageHorizontally);
        std::vector<double> profile = plot.getProfile();

        if (profile.empty()) continue;

        if (uncalibratedOD) {
            profile = calculateOD(profile);
        }

        auto minmax = std::minmax_element(profile.begin(), profile.end());
        if (*minmax.first < globalMin) globalMin = *minmax.first;
        if (*minmax.second > globalMax) globalMax = *minmax.second;

        allProfiles.push_back(profile);
    }

    if (allProfiles.empty()) {
        QMessageBox::warning(this, "Warning", "No valid lane data.");
        return;
    }

    if (uncalibratedOD) {
        globalMin = odMin;
        globalMax = odMax;
    }

    ProfileDialog dlg(allProfiles, globalMin, globalMax, pixelsAveraged, this);
    dlg.exec();
}

std::vector<double> MainWindow::calculateOD(const std::vector<double>& profile) {
    std::vector<double> odProfile(profile.size());

    for (size_t i = 0; i < profile.size(); ++i) {
        double pixelValue = std::min(profile[i], 254.999);

        double v = 0.434294481 * std::log(255.0 / (255.0 - pixelValue));

        if (v < odMin) odMin = v;
        if (v > odMax) odMax = v;

        odProfile[i] = v;
    }

    return odProfile;
}