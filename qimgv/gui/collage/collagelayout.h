#pragma once

#include <QList>
#include <QRectF>
#include <QLineF>
#include <QHash>
#include <QString>

// Pure layout math for the collage - no widgets, no I/O.
namespace CollageLayout {

enum Mode {
    MODE_MOSAIC, // uneven justified mosaic, every cell sized by image aspect
    MODE_GRID,   // equal cells
    MODE_ROW,    // rows of tiles (one by default)
    MODE_COLUMN, // columns of tiles (one by default)
    MODE_FREEFORM // tiles are placed by hand (the view keeps its own rects; compute() falls back to a mosaic)
};

// clip shape of the cells (Grid / Row / Column "Style")
enum CellShape {
    SHAPE_RECT,          // plain (corner radius per tile)
    SHAPE_TILES,         // rounded tiles
    SHAPE_CIRCLE,        // ellipse inside the cell
    SHAPE_HEXAGON,
    SHAPE_DIAMOND,
    SHAPE_TRIANGLE,      // alternating up / down
    SHAPE_PARALLELOGRAM,
    SHAPE_POLYGON        // regular polygon with polygonSides corners, optionally a star
};

// Layout tweaks of the Grid / Row / Column layouts plus the dragged borders of every automatic layout.
struct Options {
    int columns = 0;          // grid: columns (0 = auto); column layout: number of columns (0 / 1 = one)
    int rows = 0;             // grid: rows (0 = auto); row layout: number of rows (0 / 1 = one)
    qreal cellWidth = 0.0;    // grid: cell width, column layout: column width (0 = share the canvas)
    qreal cellHeight = 0.0;   // grid: cell height, row layout: row height (0 = share the canvas)
    qreal rotation = 0.0;     // degrees, every tile turns around its centre
    int shape = SHAPE_RECT;
    int polygonSides = 6;
    bool star = false;
    qreal starDepth = 0.5;    // inner radius of a star, 0..1 of the outer one
    bool honeycomb = false;   // grid: every other row shifted by half a cell
    // dragged borders: key -> position of the border as a fraction (0..1) of the separator's parent rect
    QHash<QString, qreal> splits;
};

// A border between neighbouring cells that can be dragged (Snap / Free modes).
struct Separator {
    QString key;
    Qt::Orientation orientation = Qt::Vertical; // of the line: vertical = dragged sideways
    QLineF line;          // centre line of the gap between the cells
    QRectF parent;        // fractions are relative to this rect along the drag axis
    qreal fraction = 0.5; // current position
    qreal lo = 0.0, hi = 1.0; // neighbouring borders (fractions of parent)
    qreal minSize = 0.05;     // closest the border may get to lo / hi
};

struct Result {
    QList<QRectF> cells;
    QList<Separator> separators;
};

// true for the layouts that use the Options style / rotation / counts
inline bool usesCellOptions(Mode mode) {
    return mode == MODE_GRID || mode == MODE_ROW || mode == MODE_COLUMN;
}

// aspects: width / height of every image, in display order.
// weights (optional): relative size of every image, 1.0 = normal. Ignored by MODE_GRID.
// Returns one rect per image. `gap` is both the outer margin and the spacing between cells.
Result computeEx(Mode mode, const QList<qreal> &aspects, const QRectF &canvas, qreal gap,
                 const QList<qreal> &weights, const Options &options);

QList<QRectF> compute(Mode mode, const QList<qreal> &aspects, const QRectF &canvas, qreal gap,
                      const QList<qreal> &weights = QList<qreal>());

// index range of the mosaic node keys ("m<begin>-<end>") so a tile's ancestors can be found
bool mosaicKeyContains(const QString &key, int index);

}
