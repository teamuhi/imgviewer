#include "collagelayout.h"
#include <QtMath>
#include <QVector>

namespace {

using CollageLayout::Options;
using CollageLayout::Separator;

// extreme panoramas / slivers would produce unusable cells
qreal sanitize(qreal aspect) {
    if(!(aspect > 0.0))
        return 1.0;
    return qBound<qreal>(0.2, aspect, 5.0);
}

// closest two borders may get: a few percent of the span, but never so much that n cells can not fit
qreal minSpan(qreal length, int cells) {
    if(cells <= 0 || length <= 0)
        return 0.0;
    return qMin(length / cells * 0.6, qMax<qreal>(24.0, length * 0.04));
}

// b: n + 1 absolute positions along one axis (b.first() = start, b.last() = end of the span).
// The inner ones may be moved by dragged borders (splits[prefix + i]); all stay minSpan() apart.
void applySplits(QVector<qreal> &b, const QString &prefix, const Options &o) {
    int n = b.size() - 1;
    if(n < 2)
        return;
    qreal start = b.first(), length = b.last() - b.first();
    if(length <= 0)
        return;
    qreal minLen = minSpan(length, n);
    for(int i = 1; i < n; i++) {
        QString key = prefix + QString::number(i);
        if(o.splits.contains(key))
            b[i] = start + qBound<qreal>(0.0, o.splits.value(key), 1.0) * length;
    }
    for(int i = 1; i < n; i++)
        b[i] = qMax(b[i], b[i - 1] + minLen);
    for(int i = n - 1; i >= 1; i--)
        b[i] = qMin(b[i], b[i + 1] - minLen);
}

bool hasSplits(const Options &o, const QString &prefix) {
    for(auto it = o.splits.constBegin(); it != o.splits.constEnd(); ++it) {
        if(it.key().startsWith(prefix))
            return true;
    }
    return false;
}

// one separator per inner border of b (indices 1 .. lastIndex), spanning crossStart .. crossEnd
void addSeparators(const QVector<qreal> &b, const QString &prefix, bool verticalLine,
                   qreal crossStart, qreal crossEnd, QList<Separator> *seps, int lastIndex = -1) {
    int n = b.size() - 1;
    if(!seps || n < 2 || crossEnd <= crossStart)
        return;
    if(lastIndex < 0 || lastIndex > n - 1)
        lastIndex = n - 1;
    qreal start = b.first(), length = b.last() - b.first();
    if(length <= 0)
        return;
    qreal minLen = minSpan(length, n);
    for(int i = 1; i <= lastIndex; i++) {
        Separator s;
        s.key = prefix + QString::number(i);
        s.orientation = verticalLine ? Qt::Vertical : Qt::Horizontal;
        s.line = verticalLine ? QLineF(b[i], crossStart, b[i], crossEnd) : QLineF(crossStart, b[i], crossEnd, b[i]);
        s.parent = verticalLine ? QRectF(start, crossStart, length, crossEnd - crossStart)
                                : QRectF(crossStart, start, crossEnd - crossStart, length);
        s.fraction = (b[i] - start) / length;
        s.lo = (b[i - 1] - start) / length;
        s.hi = (b[i + 1] - start) / length;
        s.minSize = minLen / length;
        seps->append(s);
    }
}

// evenly spaced borders over [start, end], or fixed cells of `cell` (+ gap) centred on the span
QVector<qreal> spanBorders(int cells, qreal start, qreal end, qreal cell, qreal gap, qreal extraCells = 0.0) {
    QVector<qreal> b(cells + 1);
    qreal pitch;
    qreal origin;
    if(cell > 0) {
        pitch = cell + gap;
        origin = (start + end) / 2.0 - (cells + extraCells) * pitch / 2.0;
    } else {
        pitch = (end - start) / (cells + extraCells);
        origin = start;
    }
    for(int i = 0; i <= cells; i++)
        b[i] = origin + i * pitch;
    return b;
}

// borders that split [start, end] proportionally to `shares`
QVector<qreal> shareBorders(const QList<qreal> &shares, qreal start, qreal end) {
    QVector<qreal> b(shares.size() + 1);
    qreal total = 0;
    for(qreal s : shares)
        total += s;
    if(total <= 0)
        total = 1;
    b[0] = start;
    qreal pos = start;
    for(int i = 0; i < shares.size(); i++) {
        pos += (end - start) * shares.at(i) / total;
        b[i + 1] = pos;
    }
    b[shares.size()] = end; // no rounding drift at the far edge
    return b;
}

//------------------------------------------------------------------------------
// Mosaic: binary tree of horizontal / vertical splits. Every node knows the aspect ratio of the
// group of images below it and the index range it covers (stable key for dragged borders).
struct Node {
    qreal aspect = 1.0;
    bool horizontal = true; // true: children side by side, false: stacked
    qreal weight = 1.0;     // relative size, mean of the leaves for groups
    int leaf = -1;          // image index for leaves
    int a = -1, b = -1;     // child node indices
    int begin = 0, end = 0; // image index range [begin, end)
};

class Tree {
public:
    Tree(const QList<qreal> &aspects, const QList<qreal> &weights, qreal targetAspect, const Options &options)
        : mAspects(aspects), mWeights(weights), mTarget(targetAspect), mOptions(options) {}

    int build(int begin, int end) {
        if(end - begin == 1) {
            Node leaf;
            leaf.leaf = begin;
            leaf.aspect = mAspects.at(begin);
            leaf.weight = mWeights.at(begin);
            leaf.begin = begin;
            leaf.end = end;
            mNodes.append(leaf);
            return mNodes.size() - 1;
        }
        int mid = begin + (end - begin) / 2;
        int a = build(begin, mid);
        int b = build(mid, end);
        qreal aa = mNodes.at(a).aspect;
        qreal ba = mNodes.at(b).aspect;
        qreal sideBySide = aa + ba;
        qreal stacked = 1.0 / (1.0 / aa + 1.0 / ba);
        Node node;
        node.a = a;
        node.b = b;
        node.begin = begin;
        node.end = end;
        // pick whichever orientation lands closer to the canvas shape
        node.horizontal = qAbs(qLn(sideBySide / mTarget)) <= qAbs(qLn(stacked / mTarget));
        node.aspect = node.horizontal ? sideBySide : stacked;
        node.weight = (mNodes.at(a).weight + mNodes.at(b).weight) / 2.0;
        mNodes.append(node);
        return mNodes.size() - 1;
    }

    void assign(int index, const QRectF &rect, QList<QRectF> &out, QList<Separator> *seps) const {
        const Node &node = mNodes.at(index);
        if(node.leaf >= 0) {
            out[node.leaf] = rect;
            return;
        }
        qreal aa = mNodes.at(node.a).aspect;
        qreal ba = mNodes.at(node.b).aspect;
        qreal wa_ = mNodes.at(node.a).weight;
        qreal wb_ = mNodes.at(node.b).weight;
        qreal length = node.horizontal ? rect.width() : rect.height();
        qreal fraction;
        if(node.horizontal) {
            fraction = (aa * wa_) / (aa * wa_ + ba * wb_);
        } else {
            qreal ia = wa_ / aa, ib = wb_ / ba;
            fraction = ia / (ia + ib);
        }
        QString key = QString("m%1-%2").arg(node.begin).arg(node.end);
        if(mOptions.splits.contains(key))
            fraction = mOptions.splits.value(key);
        qreal minF = length > 0 ? qBound<qreal>(0.05, 48.0 / length, 0.45) : 0.45;
        fraction = qBound(minF, fraction, 1.0 - minF);
        if(seps && length > 0) {
            Separator s;
            s.key = key;
            s.parent = rect;
            s.fraction = fraction;
            s.lo = 0.0;
            s.hi = 1.0;
            s.minSize = minF;
            if(node.horizontal) {
                qreal x = rect.left() + rect.width() * fraction;
                s.orientation = Qt::Vertical;
                s.line = QLineF(x, rect.top(), x, rect.bottom());
            } else {
                qreal y = rect.top() + rect.height() * fraction;
                s.orientation = Qt::Horizontal;
                s.line = QLineF(rect.left(), y, rect.right(), y);
            }
            seps->append(s);
        }
        if(node.horizontal) {
            qreal wa = rect.width() * fraction;
            assign(node.a, QRectF(rect.left(), rect.top(), wa, rect.height()), out, seps);
            assign(node.b, QRectF(rect.left() + wa, rect.top(), rect.width() - wa, rect.height()), out, seps);
        } else {
            qreal ha = rect.height() * fraction;
            assign(node.a, QRectF(rect.left(), rect.top(), rect.width(), ha), out, seps);
            assign(node.b, QRectF(rect.left(), rect.top() + ha, rect.width(), rect.height() - ha), out, seps);
        }
    }

    int root() const { return mNodes.size() - 1; }

private:
    QList<qreal> mAspects;
    QList<qreal> mWeights;
    qreal mTarget;
    const Options &mOptions;
    QVector<Node> mNodes;
};

//------------------------------------------------------------------------------
// Grid: columns x rows (auto from the canvas shape by default), optional fixed cell size,
// honeycomb offset and dragged column / row borders ("gc<i>", "gr<i>")
QList<QRectF> grid(int count, const QRectF &area, qreal gap, const Options &o, QList<Separator> *seps) {
    QList<QRectF> out;
    qreal canvasAspect = area.height() > 0 ? area.width() / area.height() : 1.0;
    int cols, rows;
    if(o.columns > 0) {
        cols = o.columns;
        rows = qMax(o.rows > 0 ? o.rows : 0, (count + cols - 1) / cols);
    } else if(o.rows > 0) {
        rows = o.rows;
        cols = qMax(1, (count + rows - 1) / rows);
        rows = qMax(rows, (count + cols - 1) / cols);
    } else {
        cols = qMax(1, qRound(qSqrt(count * canvasAspect)));
        cols = qMin(cols, count);
        rows = (count + cols - 1) / cols;
        cols = (count + rows - 1) / rows; // tighten: fewer empty cells in the last row
    }
    cols = qMax(1, cols);
    rows = qMax(1, rows);
    int usedRows = qMax(1, (count + cols - 1) / cols);

    bool fixedW = o.cellWidth > 0, fixedH = o.cellHeight > 0;
    bool honeycomb = o.honeycomb && usedRows > 1;
    QVector<qreal> xs = spanBorders(cols, area.left(), area.right(), o.cellWidth, gap, honeycomb ? 0.5 : 0.0);
    QVector<qreal> ys = spanBorders(rows, area.top(), area.bottom(), o.cellHeight, gap);
    if(!fixedW && !honeycomb)
        applySplits(xs, "gc", o);
    if(!fixedH)
        applySplits(ys, "gr", o);
    qreal pitch = (xs.last() - xs.first()) / cols;

    // honeycomb of (pointy) hexagons: rows interlock by a quarter of the cell height
    bool nest = honeycomb && o.shape == CollageLayout::SHAPE_HEXAGON;
    QVector<qreal> rowTop(rows), rowHeight(rows);
    if(nest) {
        qreal cell = fixedH ? o.cellHeight + gap : area.height() / (rows * 0.75 + 0.25);
        qreal total = cell * (rows * 0.75 + 0.25);
        qreal top = fixedH ? area.center().y() - total / 2.0 : area.top();
        for(int r = 0; r < rows; r++) {
            rowTop[r] = top + r * 0.75 * cell;
            rowHeight[r] = cell;
        }
    } else {
        for(int r = 0; r < rows; r++) {
            rowTop[r] = ys[r];
            rowHeight[r] = ys[r + 1] - ys[r];
        }
    }

    int inLastRow = count - (usedRows - 1) * cols;
    // an incomplete last row is centred, unless the columns were dragged or shifted
    bool centerLast = !honeycomb && inLastRow < cols && !hasSplits(o, "gc");
    for(int i = 0; i < count; i++) {
        int r = i / cols;
        int c = i % cols;
        qreal x = xs[c], w = xs[c + 1] - xs[c];
        if(honeycomb && (r % 2))
            x += pitch / 2.0;
        if(centerLast && r == usedRows - 1)
            x += (cols - inLastRow) * pitch / 2.0;
        out.append(QRectF(x, rowTop[r], w, rowHeight[r]));
    }
    if(seps) {
        int fullRows = centerLast ? usedRows - 1 : usedRows;
        if(!fixedW && !honeycomb && cols > 1 && fullRows > 0)
            addSeparators(xs, "gc", true, ys[0], ys[fullRows], seps);
        if(!fixedH && !nest && usedRows > 1)
            addSeparators(ys, "gr", false, xs.first(), xs.last(), seps, usedRows - 1);
    }
    return out;
}

// Row / Column: tiles flow into `lines` lines (sequential chunks), each line split by aspect * weight.
// Lines: "rl<i>" / "cl<i>"; borders inside line j: "r<j>:<i>" / "c<j>:<i>".
QList<QRectF> lines(bool horizontalLines, const QList<qreal> &aspects, const QList<qreal> &weights,
                    const QRectF &area, qreal gap, const Options &o, QList<Separator> *seps) {
    int count = aspects.size();
    int wanted = horizontalLines ? o.rows : o.columns;
    int lineCount = qBound(1, wanted, count);
    int perLine = (count + lineCount - 1) / lineCount;
    lineCount = (count + perLine - 1) / perLine; // no empty lines
    qreal fixed = horizontalLines ? o.cellHeight : o.cellWidth;

    // borders between the lines (across the flow)
    QVector<qreal> lb = horizontalLines ? spanBorders(lineCount, area.top(), area.bottom(), fixed, gap)
                                        : spanBorders(lineCount, area.left(), area.right(), fixed, gap);
    const QString lineKey = horizontalLines ? "rl" : "cl";
    if(fixed <= 0)
        applySplits(lb, lineKey, o);

    QList<QRectF> out;
    for(int j = 0; j < lineCount; j++) {
        int first = j * perLine;
        int last = qMin(count, first + perLine);
        QList<qreal> shares;
        for(int i = first; i < last; i++)
            shares.append(horizontalLines ? aspects.at(i) * weights.at(i) : weights.at(i) / aspects.at(i));
        QVector<qreal> b = horizontalLines ? shareBorders(shares, area.left(), area.right())
                                           : shareBorders(shares, area.top(), area.bottom());
        QString prefix = QString(horizontalLines ? "r%1:" : "c%1:").arg(j);
        applySplits(b, prefix, o);
        for(int k = 0; k < shares.size(); k++) {
            if(horizontalLines)
                out.append(QRectF(b[k], lb[j], b[k + 1] - b[k], lb[j + 1] - lb[j]));
            else
                out.append(QRectF(lb[j], b[k], lb[j + 1] - lb[j], b[k + 1] - b[k]));
        }
        addSeparators(b, prefix, horizontalLines, lb[j], lb[j + 1], seps);
    }
    if(fixed <= 0 && lineCount > 1) {
        if(horizontalLines)
            addSeparators(lb, lineKey, false, area.left(), area.right(), seps);
        else
            addSeparators(lb, lineKey, true, area.top(), area.bottom(), seps);
    }
    return out;
}

} // namespace

namespace CollageLayout {

Result computeEx(Mode mode, const QList<qreal> &rawAspects, const QRectF &canvas, qreal gap,
                 const QList<qreal> &rawWeights, const Options &options) {
    Result result;
    int count = rawAspects.size();
    if(count == 0 || canvas.isEmpty())
        return result;

    QList<qreal> aspects;
    for(qreal a : rawAspects)
        aspects.append(sanitize(a));

    QList<qreal> weights;
    for(int i = 0; i < count; i++) {
        qreal w = i < rawWeights.size() ? rawWeights.at(i) : 1.0;
        weights.append(w > 0.0 ? qBound<qreal>(0.2, w, 5.0) : 1.0);
    }

    // keep margins sane on tiny canvases
    gap = qBound<qreal>(0.0, gap, qMin(canvas.width(), canvas.height()) / 4.0);
    QRectF area = canvas.adjusted(gap / 2.0, gap / 2.0, -gap / 2.0, -gap / 2.0);

    QList<QRectF> cells;
    QList<Separator> *seps = &result.separators;
    switch(mode) {
        case MODE_GRID:
            cells = grid(count, area, gap, options, seps);
            break;
        case MODE_ROW:
            cells = lines(true, aspects, weights, area, gap, options, seps);
            break;
        case MODE_COLUMN:
            cells = lines(false, aspects, weights, area, gap, options, seps);
            break;
        case MODE_FREEFORM:
        case MODE_MOSAIC:
        default: {
            qreal target = canvas.width() / canvas.height();
            Tree tree(aspects, weights, target, options);
            tree.build(0, count);
            for(int i = 0; i < count; i++)
                cells.append(QRectF());
            tree.assign(tree.root(), area, cells, mode == MODE_FREEFORM ? nullptr : seps);
            break;
        }
    }

    // spacing between neighbours = gap (each cell gives up half on every side)
    for(const QRectF &cell : cells) {
        QRectF r = cell.adjusted(gap / 2.0, gap / 2.0, -gap / 2.0, -gap / 2.0);
        if(r.width() < 1.0)
            r.setWidth(1.0);
        if(r.height() < 1.0)
            r.setHeight(1.0);
        result.cells.append(r);
    }
    return result;
}

QList<QRectF> compute(Mode mode, const QList<qreal> &aspects, const QRectF &canvas, qreal gap,
                      const QList<qreal> &weights) {
    return computeEx(mode, aspects, canvas, gap, weights, Options()).cells;
}

bool mosaicKeyContains(const QString &key, int index) {
    if(!key.startsWith('m'))
        return false;
    int dash = key.indexOf('-');
    if(dash < 0)
        return false;
    bool okBegin = false, okEnd = false;
    int begin = key.mid(1, dash - 1).toInt(&okBegin);
    int end = key.mid(dash + 1).toInt(&okEnd);
    return okBegin && okEnd && index >= begin && index < end;
}

}
