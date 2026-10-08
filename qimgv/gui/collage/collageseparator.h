#pragma once

#include <QGraphicsObject>
#include "gui/collage/collagelayout.h"

// Draggable border between two cells of an automatic layout. Lives above the tiles, invisible
// until hovered. Drag = move the border (Snap mode locks to 1/4, 1/3, 1/2, 2/3, 3/4 and "equal";
// Alt bypasses it), double-click = back to the automatic position.
class CollageSeparator : public QGraphicsObject {
    Q_OBJECT
public:
    CollageSeparator();

    // hitWidth: thickness of the grab area in scene units
    void setSeparator(const CollageLayout::Separator &separator, qreal hitWidth);
    const CollageLayout::Separator &separator() const;
    bool isDragging() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    void moved(const QString &key, qreal fraction);
    void resetRequested(const QString &key);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    CollageLayout::Separator mSep;
    qreal mHit = 10.0;
    bool mHover = false, mDragging = false, mSnapped = false;

    qreal fractionAt(const QPointF &scenePos) const;
};
