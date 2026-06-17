#include "LaneGraphicsView.h"
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QDebug>

LaneGraphicsView::LaneGraphicsView(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::ScrollHandDrag); // 默认平移
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    // 设置视图接受鼠标右键等
    setContextMenuPolicy(Qt::CustomContextMenu);
    // 不要和默认菜单冲突，手动处理右键
}

void LaneGraphicsView::mousePressEvent(QMouseEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl+左键开始画矩形
        if (event->button() == Qt::LeftButton) {
            drawing = true;
            startPoint = mapToScene(event->pos());
            tempRect = new QGraphicsRectItem(QRectF(startPoint, QSizeF(0,0)));
            tempRect->setPen(QPen(Qt::green, 2));
            scene()->addItem(tempRect);
            setDragMode(QGraphicsView::NoDrag); // 禁止平移
        }
    } else {
        QGraphicsView::mousePressEvent(event); // 正常平移
    }
}

void LaneGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (drawing && tempRect) {
        QPointF endPoint = mapToScene(event->pos());
        QRectF rect(startPoint, endPoint);
        rect = rect.normalized(); // 确保非负
        tempRect->setRect(rect);
    } else {
        QGraphicsView::mouseMoveEvent(event);
    }
}

void LaneGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    if (drawing && tempRect) {
        QRectF finalRect = tempRect->rect();
        scene()->removeItem(tempRect);
        delete tempRect;
        tempRect = nullptr;
        drawing = false;
        setDragMode(QGraphicsView::ScrollHandDrag); // 恢复平移
        if (finalRect.width() > 5 && finalRect.height() > 5) {
            emit laneRectCreated(finalRect);
        }
    } else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

void LaneGraphicsView::contextMenuEvent(QContextMenuEvent *event) {
    // 右键菜单：删除选中的泳道
    QGraphicsItem *item = scene()->itemAt(mapToScene(event->pos()), QTransform());
    if (item && item->type() == QGraphicsRectItem::Type) {
        QMenu menu;
        QAction *deleteAction = menu.addAction("Remove Lane");
        if (menu.exec(event->globalPos()) == deleteAction) {
            scene()->removeItem(item);
            delete item;
            // 同时从 MainWindow 的 lanes 列表中删除？无法直接访问，可发送信号或通过父窗口查找
            // 简单处理：由 MainWindow 连接 item 的销毁信号？
        }
    }
}