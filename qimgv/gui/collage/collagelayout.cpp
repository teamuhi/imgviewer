#include "collagelayout.h"
#include <QtMath>
#include <QVector>

namespace {

// extreme panoramas / slivers would produce unusable cells
qreal sanitize(qreal aspect) {
    if(!(aspect > 0.0))
        return 1.0;
    return qBound<qreal>(0.2, aspect, 5.0);
}

// Binary tree of horizontal / vertical splits. Every node knows the aspect
// ratio of the group of images below it.
struct Node {
    qreal aspect = 1.0;
    bool horizontal = true; // true: children side by side, false: stacked
    qreal weight = 1.0;     // relative size, mean of the leaves for groups
    int leaf = -1;          // image index for leaves
    int a = -1, b = -1;     // child node indices
};

class Tree {
public:
    Tree(const QList<qreal> &aspects, const QList<qreal> &weights, qreal targetAspect)
        : mAspects(aspects), mWeights(weights), mTarget(targetAspect) {}

    int build(int begin, int end) {
        if(end - begin == 1) {
            Node leaf;
            leaf.leaf = begin;
            leaf.aspect = mAspects.at(begin);
            leaf.weight = mWeights.at(begin);
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
        // pick whichever orientation lands closer to the canvas shape
        node.horizontal = qAbs(qLn(sideBySide / mTarget)) <= qAbs(qLn(stacked / mTarget));
        node.aspect = node.horizontal ? sideBySide : stacked;
        node.weight = (mNodes.at(a).weight + mNodes.at(b).weight) / 2.0;
        mNodes.append(node);
        return mNodes.size() - 1;
    }

    void assign(int index, const QRectF &rect, QList<QRectF> &out) const {
        const Node &node = mNodes.at(index);
        if(node.leaf >= 0) {
            out[node.leaf] = rect;
            return;
        }
        qreal aa = mNodes.at(node.a).aspect;
        qreal ba = mNodes.at(node.b).aspect;
        qreal wa_ = mNodes.at(node.a).weight;
        qreal wb_ = mNodes.at(node.b).weight;
        if(node.horizontal) {
            qreal wa = rect.width() * (aa * wa_) / (aa * wa_ + ba * wb_);
            assign(node.a, QRectF(rect.left(), rect.top(), wa, rect.height()), out);
            assign(node.b, QRectF(rect.left() + wa, rect.top(), rect.width() - wa, rect.height()), out);
        } else {
            qreal ia = wa_ / aa, ib = wb_ / ba;
            qreal ha = rect.height() * ia / (ia + ib);
            assign(node.a, QRectF(rect.left(), rect.top(), rect.width(), ha), out);
            assign(node.b, QRectF(rect.left(), rect.top() + ha, rect.width(), rect.height() - ha), out);
        }
    }

    int root() const { return mNodes.size() - 1; }

private:
    QList<qreal> mAspects;
    QList<qreal> mWeights;
    qreal mTarget;
    QVector<Node> mNodes;
};

QList<QRectF> grid(int count, const QRectF &area) {
    QList<QRectF> out;
    qreal canvasAspect = area.height() > 0 ? area.width() / area.height() : 1.0;
    int cols = qMax(1, qRound(qSqrt(count * canvasAspect)));
    cols = qMin(cols, count);
    int rows = (count + cols - 1) / cols;
    cols = (count + rows - 1) / rows; // tighten: fewer empty cells in the last row
    qreal cw = area.width() / cols;
    qreal ch = area.height() / rows;
    for(int i = 0; i < count; i++) {
        int row = i / cols;
        int col = i % cols;
        int inRow = qMin(cols, count - row * cols);
        // center an incomplete last row
        qreal offset = (cols - inRow) * cw / 2.0;
        out.append(QRectF(area.left() + offset + col * cw, area.top() + row * ch, cw, ch));
    }
    return out;
}

QList<QRectF> row(const QList<qreal> &aspects, const QList<qreal> &weights, const QRectF &area) {
    QList<QRectF> out;
    qreal total = 0;
    for(int i = 0; i < aspects.size(); i++)
        total += aspects.at(i) * weights.at(i);
    qreal x = area.left();
    for(int i = 0; i < aspects.size(); i++) {
        qreal w = area.width() * aspects.at(i) * weights.at(i) / total;
        out.append(QRectF(x, area.top(), w, area.height()));
        x += w;
    }
    return out;
}

QList<QRectF> column(const QList<qreal> &aspects, const QList<qreal> &weights, const QRectF &area) {
    QList<qreal> inverse;
    qreal total = 0;
    for(int i = 0; i < aspects.size(); i++) {
        qreal v = weights.at(i) / aspects.at(i);
        inverse.append(v);
        total += v;
    }
    QList<QRectF> out;
    qreal y = area.top();
    for(qreal ia : inverse) {
        qreal h = area.height() * ia / total;
        out.append(QRectF(area.left(), y, area.width(), h));
        y += h;
    }
    return out;
}

} // namespace

namespace CollageLayout {

QList<QRectF> compute(Mode mode, const QList<qreal> &rawAspects, const QRectF &canvas, qreal gap,
                      const QList<qreal> &rawWeights) {
    int count = rawAspects.size();
    if(count == 0 || canvas.isEmpty())
        return QList<QRectF>();

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
    switch(mode) {
        case MODE_GRID:
            cells = grid(count, area);
            break;
        case MODE_ROW:
            cells = row(aspects, weights, area);
            break;
        case MODE_COLUMN:
            cells = column(aspects, weights, area);
            break;
        case MODE_FREEFORM:
        case MODE_MOSAIC:
        default: {
            qreal target = canvas.width() / canvas.height();
            Tree tree(aspects, weights, target);
            tree.build(0, count);
            cells = QList<QRectF>();
            for(int i = 0; i < count; i++)
                cells.append(QRectF());
            tree.assign(tree.root(), area, cells);
            break;
        }
    }

    // spacing between neighbours = gap (each cell gives up half on every side)
    QList<QRectF> result;
    for(const QRectF &cell : cells)
        result.append(cell.adjusted(gap / 2.0, gap / 2.0, -gap / 2.0, -gap / 2.0));
    return result;
}

}
