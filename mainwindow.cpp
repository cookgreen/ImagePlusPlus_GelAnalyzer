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

    double globalMin = std::numeric_limits<double>::max();
    double globalMax = -std::numeric_limits<double>::max();

    int pixelsAveraged = 1;

    std::vector<cv::Rect> roi_list;
    for (const QGraphicsRectItem *item : lanes) {
        QRectF rf = item->rect();
        cv::Rect cvRoi(rf.x(), rf.y(), rf.width(), rf.height());

        bool averageHorizontally = (cvRoi.height > cvRoi.width);
        pixelsAveraged = averageHorizontally ? cvRoi.width : cvRoi.height;

        roi_list.push_back(cvRoi);
    }

    GelAnalyzer gelAnalyzer;
    std::vector<std::vector<double>> allProfiles = gelAnalyzer.plotLanes(
        processedMat, roi_list, globalMin, globalMax);

    if (gelAnalyzer.uncalibratedOD)
    {
        globalMin = gelAnalyzer.odMin;
        globalMax = gelAnalyzer.odMax;
    }

    ProfileDialog dlg(allProfiles, globalMin, globalMax, pixelsAveraged, this);
    dlg.exec();
}