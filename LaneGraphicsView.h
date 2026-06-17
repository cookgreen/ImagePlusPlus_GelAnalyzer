#ifndef LANEGRAPHICSVIEW_H
#define LANEGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QRectF>

class LaneGraphicsView : public QGraphicsView {
    Q_OBJECT
public:
    explicit LaneGraphicsView(QGraphicsScene *scene, QWidget *parent = nullptr);

signals:
    void laneRectCreated(QRectF rect);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    bool drawing = false;
    QPointF startPoint;
    QGraphicsRectItem *tempRect = nullptr;
    bool panMode = true; // 默认平移模式，按住Ctrl绘制矩形
};

#endif