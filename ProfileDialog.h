#ifndef PROFILEDIALOG_H
#define PROFILEDIALOG_H

#include <QDialog>
#include <QVector>
#include <QPolygon>
#include <QLine>
#include <QPoint>
#include <vector>

class QToolBar;
class QAction;

struct PeakInfo {
    QPolygon polygon;
    double area;
    QPoint center; // 用于在 UI 上写数字
};

class ProfileDialog : public QDialog
{
    Q_OBJECT

public:
    ProfileDialog(const std::vector<std::vector<double>> &profiles,
                  double minVal, double maxVal,
                  int imageWidth,
                  QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    // 提取绘图逻辑，方便复用
    void drawPlotContent(QPainter *p, int width, int height);
    void applyWandTool(const QPoint& clickPos);

    std::vector<std::vector<double>> m_profiles;
    double m_min;
    double m_max;
    int m_imageWidth;
    int m_pixelsAveraged;

    // 布局常量
    const int topMargin = 50; // 加大 topMargin 给工具栏留出空间
    const int bottomMargin = 10;
    const int leftMargin = 40;
    const int rightMargin = 20;

    // --- 交互工具相关 ---
    enum ToolMode { None, LineTool, WandTool };
    ToolMode m_currentTool = None;

    QToolBar *m_toolBar;
    QAction *m_actionLine;
    QAction *m_actionWand;
    QAction *m_actionClear;

    // 存储用户画的直线
    QVector<QLine> m_userLines;
    bool m_isDrawingLine = false;
    QPoint m_lineStart;
    QPoint m_lineEnd;


    // 存储魔棒选中的多边形区域
    QVector<QPolygon> m_wandSelections;
    QVector<PeakInfo> m_peaks;
};

#endif // PROFILEDIALOG_H