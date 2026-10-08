#pragma once

#include <QPainterPath>
#include <QRectF>
#include <QColor>

class QPainter;

// Outline / clip shape of a collage tile. Used for painting, hit testing and the export,
// so what is on screen is what gets saved.
namespace CollageShape {

struct Spec {
    int shape = 0;          // CollageLayout::CellShape
    int sides = 6;          // SHAPE_POLYGON
    bool star = false;
    qreal starDepth = 0.5;
    int index = 0;          // position in the layout (triangles alternate)
    bool operator==(const Spec &o) const {
        return shape == o.shape && sides == o.sides && star == o.star
               && qFuzzyCompare(1.0 + starDepth, 1.0 + o.starDepth) && index == o.index;
    }
    bool operator!=(const Spec &o) const { return !(*this == o); }
};

// cornerRadius only applies to the plain rectangle
QPainterPath path(const QRectF &frame, const Spec &spec, qreal cornerRadius);

// tile outline styles
enum BorderStyle {
    BORDER_SOLID,
    BORDER_DASHED,
    BORDER_DOTTED,
    BORDER_DASH_DOT,
    BORDER_DOUBLE
};

// strokes `width` px along the inside of `clip` (nothing ever leaks outside the tile)
void paintBorder(QPainter *painter, const QPainterPath &clip, qreal width, const QColor &color, int style);

}
