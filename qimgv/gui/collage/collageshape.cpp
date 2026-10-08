#include "collageshape.h"
#include "collagelayout.h"
#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QtMath>

namespace CollageShape {

QPainterPath path(const QRectF &f, const Spec &spec, qreal cornerRadius) {
    QPainterPath p;
    const qreal l = f.left(), t = f.top(), r = f.right(), b = f.bottom();
    const qreal cx = f.center().x(), cy = f.center().y();
    switch(spec.shape) {
    case CollageLayout::SHAPE_TILES: {
        qreal radius = qMax(cornerRadius, qMin(f.width(), f.height()) * 0.08);
        p.addRoundedRect(f, radius, radius);
        break;
    }
    case CollageLayout::SHAPE_CIRCLE:
        p.addEllipse(f);
        break;
    case CollageLayout::SHAPE_HEXAGON: {
        // pointy top: rows of hexagons nest when shifted by half a cell (honeycomb)
        qreal q = f.height() / 4.0;
        p.addPolygon(QPolygonF({ QPointF(cx, t), QPointF(r, t + q), QPointF(r, b - q),
                                 QPointF(cx, b), QPointF(l, b - q), QPointF(l, t + q) }));
        p.closeSubpath();
        break;
    }
    case CollageLayout::SHAPE_DIAMOND:
        p.addPolygon(QPolygonF({ QPointF(cx, t), QPointF(r, cy), QPointF(cx, b), QPointF(l, cy) }));
        p.closeSubpath();
        break;
    case CollageLayout::SHAPE_TRIANGLE:
        if(spec.index % 2 == 0)
            p.addPolygon(QPolygonF({ QPointF(cx, t), QPointF(r, b), QPointF(l, b) }));
        else
            p.addPolygon(QPolygonF({ QPointF(l, t), QPointF(r, t), QPointF(cx, b) }));
        p.closeSubpath();
        break;
    case CollageLayout::SHAPE_PARALLELOGRAM: {
        qreal skew = f.width() * 0.2;
        p.addPolygon(QPolygonF({ QPointF(l + skew, t), QPointF(r, t), QPointF(r - skew, b), QPointF(l, b) }));
        p.closeSubpath();
        break;
    }
    case CollageLayout::SHAPE_POLYGON: {
        int sides = qBound(3, spec.sides, 12);
        int points = spec.star ? sides * 2 : sides;
        qreal depth = qBound<qreal>(0.1, spec.starDepth, 0.95);
        QPolygonF polygon;
        for(int i = 0; i < points; i++) {
            qreal angle = -M_PI / 2.0 + i * 2.0 * M_PI / points; // first corner at the top
            qreal radius = (spec.star && (i % 2)) ? depth : 1.0;
            polygon << QPointF(cx + qCos(angle) * radius * f.width() / 2.0,
                               cy + qSin(angle) * radius * f.height() / 2.0);
        }
        p.addPolygon(polygon);
        p.closeSubpath();
        break;
    }
    case CollageLayout::SHAPE_RECT:
    default:
        if(cornerRadius > 0)
            p.addRoundedRect(f, cornerRadius, cornerRadius);
        else
            p.addRect(f);
        break;
    }
    return p;
}

namespace {
QPainterPath stroke(const QPainterPath &path, qreal width, Qt::PenStyle style) {
    QPainterPathStroker stroker;
    stroker.setWidth(width);
    stroker.setJoinStyle(Qt::MiterJoin);
    stroker.setCapStyle(style == Qt::SolidLine ? Qt::SquareCap : Qt::FlatCap);
    if(style != Qt::SolidLine)
        stroker.setDashPattern(style);
    return stroker.createStroke(path);
}
}

void paintBorder(QPainter *painter, const QPainterPath &clip, qreal width, const QColor &color, int style) {
    if(width <= 0 || !color.isValid() || clip.isEmpty())
        return;
    painter->save();
    painter->setClipPath(clip, Qt::IntersectClip);
    painter->setPen(Qt::NoPen);
    // a stroke of 2w centred on the edge leaves exactly w inside the clip
    switch(style) {
    case BORDER_DOUBLE: {
        painter->fillPath(stroke(clip, width * 2.0 / 3.0, Qt::SolidLine), color);
        QPainterPath inner = stroke(clip, width * 2.0, Qt::SolidLine).subtracted(stroke(clip, width * 4.0 / 3.0, Qt::SolidLine));
        painter->fillPath(inner, color);
        break;
    }
    case BORDER_DASHED:
        painter->fillPath(stroke(clip, width * 2.0, Qt::DashLine), color);
        break;
    case BORDER_DOTTED:
        painter->fillPath(stroke(clip, width * 2.0, Qt::DotLine), color);
        break;
    case BORDER_DASH_DOT:
        painter->fillPath(stroke(clip, width * 2.0, Qt::DashDotLine), color);
        break;
    case BORDER_SOLID:
    default:
        painter->fillPath(stroke(clip, width * 2.0, Qt::SolidLine), color);
        break;
    }
    painter->restore();
}

}
