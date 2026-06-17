#ifndef GELANALYZER_H
#define GELANALYZER_H

#include <QImage>
#include <QString>
#include <QRect>
#include <QVector>
#include <QObject>

class GelAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit GelAnalyzer(QObject *parent = nullptr);
    ~GelAnalyzer() override = default;

    // 核心入口，对应 Java 的 run(String arg)
    void run(const QString& arg);

    // 获取当前正在处理的凝胶图像
    QImage getGelImage() const;

private:
    // --- 常量定义 ---
    static constexpr int MAX_LANES = 100;

    // --- 状态变量 (原Java中的 static 变量) ---
    int saveID = 0;
    int nLanes = 0;
    int saveNLanes = 0;
    QRect firstRect;
    QVector<int> xPositions; // 对应原代码的 int[] x

    QImage currentImage; // 对应 imp
    QImage gel;          // 对应 gel

    int plotHeight = 0;
    bool uncalibratedOD = true;
    bool labelWithPercentages = false;
    bool outlineLanesFlag = false;
    bool invertPeaks = true;
    double verticalScaleFactor = 1.0;
    double horizontalScaleFactor = 1.0;

    bool invertedLut = false;
    double odMin = std::numeric_limits<double>::max();
    double odMax = -std::numeric_limits<double>::max();
    bool isVertical = false;
    bool showLaneDialog = true;

    // --- 内部方法 ---
    void showDialog();
    void selectFirstLane(const QRect& rect);
    void selectNextLane(QRect rect);
    void updateRoiList(const QRect& rect);
    void plotLanes(const QImage& imp, bool replot);
    void outlineLanes();
    void showMessage(const QString& msg);

    // --- 算法辅助方法 ---
    QVector<double> od(const QVector<double>& profile);
    QVector<double> getProfile(const QImage& img, const QRect& roi);

    // --- 平台/框架相关的抽象 ---
    QImage getCurrentImageFromWindowManager(); // 模拟 WindowManager.getCurrentImage()
};

#endif // GELANALYZER_H