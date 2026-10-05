#pragma once

#include <QList>
#include <QRectF>

// Pure layout math for the collage editor - no widgets, no I/O.
namespace CollageLayout {

enum Mode {
    MODE_MOSAIC, // uneven justified mosaic, every cell sized by image aspect
    MODE_GRID,   // equal cells
    MODE_ROW,    // single row
    MODE_COLUMN, // single column
    MODE_FREEFORM // tiles are placed by hand (the view keeps its own rects; compute() falls back to a mosaic)
};

// aspects: width / height of every image, in display order.
// weights (optional): relative size of every image, 1.0 = normal. Ignored by MODE_GRID.
// Returns one rect per image. `gap` is both the outer margin and the spacing between cells.
QList<QRectF> compute(Mode mode, const QList<qreal> &aspects, const QRectF &canvas, qreal gap,
                      const QList<qreal> &weights = QList<qreal>());

}
