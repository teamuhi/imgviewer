#include "collageseparator.h"
#include "settings.h"
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>

namespace {
const qreal SNAP_DISTANCE = 0.03; // of the parent span
}

CollageSeparator::CollageSeparator() {
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setZValue(1e7); // above every tile, also a lifted (dragged) one
}

void CollageSeparator::setSeparator(const CollageLayout::Separator &separator, qreal hitWidth) {
    prepareGeometryChange();
    mSep = separator;
    mHit = qMax<qreal>(1.0, hitWidth);
    setCursor(mSep.orientation == Qt::Vertical ? Qt::SizeHorCursor : Qt::SizeVerCursor);
    update();
}

const CollageLayout::Separator &CollageSeparator::separator() const {
    return mSep;
}

bool CollageSeparator::isDragging() const {
    return mDragging;
}

QRectF CollageSeparator::boundingRect() const {
    const QLineF &l = mSep.line;
    if(mSep.orientation == Qt::Vertical)
        return QRectF(l.x1() - mHit / 2.0, qMin(l.y1(), l.y2()), mHit, qAbs(l.y2() - l.y1()));
    return QRectF(qMin(l.x1(), l.x2()), l.y1() - mHit / 2.0, qAbs(l.x2() - l.x1()), mHit);
}

void CollageSeparator::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if(!mHover && !mDragging)
        return; // only shows up under the mouse
    QColor accent = settings->colorScheme().accent;
    QPen pen(accent, mSnapped ? 2 : 3);
    pen.setCosmetic(true);
    if(mSnapped)
        pen.setStyle(Qt::DashLine); // guide: the border sits on a snap position
    painter->setPen(pen);
    painter->drawLine(mSep.line);
}

qreal CollageSeparator::fractionAt(const QPointF &scenePos) const {
    if(mSep.orientation == Qt::Vertical)
        return mSep.parent.width() > 0 ? (scenePos.x() - mSep.parent.left()) / mSep.parent.width() : mSep.fraction;
    return mSep.parent.height() > 0 ? (scenePos.y() - mSep.parent.top()) / mSep.parent.height() : mSep.fraction;
}

void CollageSeparator::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    mHover = true;
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void CollageSeparator::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    mHover = false;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

void CollageSeparator::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if(event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    mDragging = true;
    mSnapped = false;
    update();
    event->accept();
}

void CollageSeparator::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if(!mDragging)
        return;
    qreal lo = mSep.lo + mSep.minSize, hi = mSep.hi - mSep.minSize;
    if(hi < lo)
        return;
    qreal f = qBound(lo, fractionAt(event->scenePos()), hi);
    mSnapped = false;
    // Snap mode: lock to simple ratios of the parent and to "both neighbours equal"; Alt = free
    if(settings->collageBorderMode() == 0 && !(event->modifiers() & Qt::AltModifier)) {
        const qreal targets[] = { 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.75, (mSep.lo + mSep.hi) / 2.0 };
        qreal best = SNAP_DISTANCE;
        for(qreal target : targets) {
            if(target < lo || target > hi)
                continue;
            if(qAbs(f - target) < best) {
                best = qAbs(f - target);
                f = target;
                mSnapped = true;
            }
        }
    }
    event->accept();
    emit moved(mSep.key, f);
}

void CollageSeparator::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    mDragging = false;
    mSnapped = false;
    update();
    event->accept();
}

void CollageSeparator::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    event->accept();
    emit resetRequested(mSep.key);
}
