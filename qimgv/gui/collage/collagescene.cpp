#include "collagescene.h"
#include "settings.h"
#include "utils/imagelib.h"
#include <QPainter>
#include <QPainterPath>
#include <QImageWriter>
#include <QFileInfo>
#include <QtMath>

namespace {
const int MIN_CANVAS = 16;
const int MAX_CANVAS = 16384;
// refuse to allocate absurd export buffers (ARGB32 = 4 bytes / px)
const qint64 MAX_EXPORT_PIXELS = 150LL * 1000 * 1000;
}

CollageScene::CollageScene(QObject *parent) : QGraphicsScene(parent) {
    connect(&mLoader, &CollageImageLoader::loaded, this, &CollageScene::onLoaded);

    mChecker = QPixmap(16, 16);
    mChecker.fill(QColor(204, 204, 204));
    QPainter p(&mChecker);
    p.fillRect(0, 0, 8, 8, QColor(153, 153, 153));
    p.fillRect(8, 8, 8, 8, QColor(153, 153, 153));
    p.end();

    // the view background pattern follows the settings
    connect(settings, &Settings::settingsChanged, this, [this]() {
        refreshPattern();
        update();
    });
    refreshPattern();
    updateSceneRect();
    connect(this, &CollageScene::itemEdited, this, &CollageScene::storeFreeRects);

    mBorderWidth = settings->collageBorderWidth();
    mBorderColor = settings->collageBorderColor();
    mBorderStyle = settings->collageBorderStyle();
    mThemeBackground = settings->collageViewThemeBackground();
}

CollageMode CollageScene::mode() const { return mMode; }
bool CollageScene::editorInitialized() const { return mEditorInitialized; }

QSize CollageScene::canvasSize() const { return mCanvasSize; }
QRectF CollageScene::viewAreaRect() const {
    return QRectF(QPointF(0, 0), mViewArea);
}

// a canvas shape only applies to the automatic view layouts
bool CollageScene::hasViewShape() const {
    return mMode == CollageMode::View && mViewShape > 0.0 && mViewLayout != CollageLayout::MODE_FREEFORM;
}

QRectF CollageScene::canvasRect() const {
    if(mMode == CollageMode::View) {
        if(!hasViewShape())
            return viewAreaRect();
        // largest rect with the wanted aspect that fits the window, centred, with a small margin so the frame shows
        const qreal margin = 12.0;
        qreal availW = qMax(1.0, mViewArea.width() - 2 * margin);
        qreal availH = qMax(1.0, mViewArea.height() - 2 * margin);
        qreal w = availW, h = availW / mViewShape;
        if(h > availH) {
            h = availH;
            w = availH * mViewShape;
        }
        w = qMax(1.0, w);
        h = qMax(1.0, h);
        return QRectF((mViewArea.width() - w) / 2.0, (mViewArea.height() - h) / 2.0, w, h);
    }
    return QRectF(QPointF(0, 0), QSizeF(mCanvasSize));
}
QColor CollageScene::canvasColor() const { return mCanvasColor; }

void CollageScene::setCanvasColor(const QColor &color) {
    mCanvasColor = color;
    update();
}

void CollageScene::updateSceneRect() {
    if(mMode == CollageMode::View) {
        // room around the view area in every layout, so the collage can be moved freely (middle-drag);
        // CollageView grows it further on demand
        setSceneRect(freeWorldRect());
        return;
    }
    // room around the canvas so items can be parked outside of it
    qreal margin = qMax(mCanvasSize.width(), mCanvasSize.height()) * 0.5;
    setSceneRect(canvasRect().adjusted(-margin, -margin, margin, margin));
}

void CollageScene::setCanvasSize(const QSize &size) {
    QSize clamped(qBound(MIN_CANVAS, size.width(), MAX_CANVAS), qBound(MIN_CANVAS, size.height(), MAX_CANVAS));
    if(clamped == mCanvasSize)
        return;
    mCanvasSize = clamped;
    updateSceneRect();
    refreshAutoResolution();
    if(mMode == CollageMode::Edit && mStatic)
        relayoutStatic();
    update();
    emit collageChanged();
}

// "Auto" resolution follows the canvas / view area: refresh working copies that are now too small / too big
void CollageScene::refreshAutoResolution() {
    for(CollageItem *item : collageItems()) {
        if(item->resolution() != CollageRes::Auto || !item->isLoaded())
            continue;
        int originalLong = qMax(item->originalSize().width(), item->originalSize().height());
        int cap = longSideFor(item);
        int desired = cap > 0 ? qMin(cap, originalLong) : originalLong;
        int current = qMax(item->workingSize().width(), item->workingSize().height());
        if(current < desired * 0.9 || current > desired * 1.5)
            reloadItem(item);
    }
}

//------------------------------------------------------------------------------
// Mode switching. Each image keeps one arrangement per mode; switching saves the one we
// leave and restores the one we enter, so arranging in one mode never touches the other.
void CollageScene::setMode(CollageMode newMode) {
    if(newMode == mMode)
        return;
    const QList<CollageItem*> list = collageItems();
    for(CollageItem *item : list) {
        if(mMode == CollageMode::View)
            mViewStates.insert(item->id(), item->saveState());
        else
            mEditStates.insert(item->id(), item->saveState());
    }
    mMode = newMode;
    bool view = (mMode == CollageMode::View);
    if(!view)
        mEditorInitialized = true;
    for(CollageItem *item : list) {
        item->setViewMode(view || mStatic);
        item->setFreePlacement(isFreeView());
        const QHash<quint64, CollageItem::State> &states = view ? mViewStates : mEditStates;
        if(states.contains(item->id()))
            item->restoreState(states.value(item->id()));
    }
    updateSceneRect();
    if(view || mStatic) {
        relayoutView();
    } else {
        resetCellStyle();
        mSeparators.clear();
        syncSeparators();
    }
    update();
    emit collageChanged();
}

bool CollageScene::staticCanvas() const { return mStatic; }

void CollageScene::setStaticCanvas(bool enabled) {
    if(mStatic == enabled)
        return;
    mStatic = enabled;
    if(mMode != CollageMode::Edit)
        return;
    // frames behave like view tiles (no handles, drag pans the picture / swaps) while it is on
    for(CollageItem *item : mById)
        item->setViewMode(enabled);
    if(enabled) {
        relayoutView();
    } else {
        resetCellStyle(); // hand-placed frames: plain, upright, no border handles
        mSeparators.clear();
        syncSeparators();
        update();
        emit collageChanged();
    }
}

void CollageScene::setStaticLayout(CollageLayout::Mode mode, int gap) {
    mStaticLayout = mode;
    mStaticGap = qBound(0, gap, 400);
    if(mMode == CollageMode::Edit && mStatic)
        relayoutView();
}

// editor with a static canvas: same flow as the view, but on the export canvas
void CollageScene::relayoutStatic() {
    const QList<CollageItem*> list = viewOrderItems();
    if(list.isEmpty()) {
        emit collageChanged();
        return;
    }
    QList<qreal> aspects;
    for(CollageItem *item : list)
        aspects.append(item->aspect());
    CollageLayout::Result result = CollageLayout::computeEx(mStaticLayout, aspects, canvasRect(), mStaticGap,
                                                            QList<qreal>(), mLayoutOptions);
    applyLayoutResult(list, mStaticLayout, result);
}

// positions + frames, then the cell style (shape / rotation of Grid / Row / Column) and the border handles
void CollageScene::applyLayoutResult(const QList<CollageItem*> &list, CollageLayout::Mode mode,
                                     const CollageLayout::Result &result) {
    bool styled = CollageLayout::usesCellOptions(mode);
    for(int i = 0; i < list.size() && i < result.cells.size(); i++) {
        CollageItem *item = list.at(i);
        item->setPos(result.cells.at(i).topLeft());
        item->setFrameSize(result.cells.at(i).size());
        CollageShape::Spec spec;
        if(styled) {
            spec.shape = mLayoutOptions.shape;
            spec.sides = mLayoutOptions.polygonSides;
            spec.star = mLayoutOptions.star;
            spec.starDepth = mLayoutOptions.starDepth;
            spec.index = i;
        }
        item->setShapeSpec(spec);
        item->setLayoutRotation(styled ? mLayoutOptions.rotation : 0.0);
    }
    mSeparators = result.separators;
    syncSeparators();
    update();
    emit collageChanged();
}

void CollageScene::resetCellStyle() {
    for(CollageItem *item : mById) {
        item->setShapeSpec(CollageShape::Spec());
        item->setLayoutRotation(0.0);
    }
}

CollageLayout::Mode CollageScene::activeLayout() const {
    if(mMode == CollageMode::View)
        return mViewLayout;
    return mStatic ? mStaticLayout : CollageLayout::MODE_FREEFORM;
}

const CollageLayout::Options &CollageScene::layoutOptions() const {
    return mLayoutOptions;
}

void CollageScene::setLayoutOptions(const CollageLayout::Options &options) {
    mLayoutOptions = options;
    relayoutView();
}

void CollageScene::clearSplits() {
    mLayoutOptions.splits.clear();
}

// the tile's own size (W / H in the panel) wins over dragged borders around it
void CollageScene::clearSplitsFor(CollageItem *item) {
    int index = item ? mViewOrder.indexOf(item->id()) : -1;
    if(index < 0)
        return;
    for(auto it = mLayoutOptions.splits.begin(); it != mLayoutOptions.splits.end();) {
        const QString key = it.key();
        bool lineInner = key.size() > 1 && (key.at(0) == QChar('r') || key.at(0) == QChar('c')) && key.at(1).isDigit();
        if(CollageLayout::mosaicKeyContains(key, index) || lineInner)
            it = mLayoutOptions.splits.erase(it);
        else
            ++it;
    }
}

void CollageScene::refreshSeparators() {
    syncSeparators();
}

// one handle per border of the automatic layout; reused between re-flows (one may be in the middle of a drag)
void CollageScene::syncSeparators() {
    bool show = settings->collageBorderMode() != 2 && activeLayout() != CollageLayout::MODE_FREEFORM && imageCount() > 1;
    int needed = show ? mSeparators.size() : 0;
    qreal gap = (mMode == CollageMode::View) ? mViewGap : mStaticGap;
    qreal hit = gap + 10.0 / mViewScale; // the gap plus ~10 screen px
    while(mSeparatorItems.size() < needed) {
        CollageSeparator *separator = new CollageSeparator();
        QGraphicsScene::addItem(separator);
        connect(separator, &CollageSeparator::moved, this, &CollageScene::onSeparatorMoved);
        connect(separator, &CollageSeparator::resetRequested, this, &CollageScene::onSeparatorReset);
        mSeparatorItems.append(separator);
    }
    for(int i = 0; i < mSeparatorItems.size(); i++) {
        CollageSeparator *separator = mSeparatorItems.at(i);
        if(i < needed) {
            separator->setSeparator(mSeparators.at(i), hit);
            separator->setVisible(true);
        } else {
            separator->setVisible(false);
        }
    }
}

void CollageScene::onSeparatorMoved(const QString &key, qreal fraction) {
    mLayoutOptions.splits.insert(key, fraction);
    relayoutView();
}

void CollageScene::onSeparatorReset(const QString &key) {
    mLayoutOptions.splits.remove(key);
    relayoutView();
}

void CollageScene::setDefaultBorder(int width, const QColor &color, int style) {
    mBorderWidth = width;
    mBorderColor = color;
    mBorderStyle = style;
    for(CollageItem *item : mById) {
        if(!item->hasBorderOverride())
            item->setBorder(width, color, style);
    }
}

void CollageScene::setThemeBackground(bool theme) {
    mThemeBackground = theme;
    update();
}

void CollageScene::setViewArea(const QSizeF &size, qreal dpr) {
    if(size.width() < 1 || size.height() < 1)
        return;
    bool dprChanged = !qFuzzyCompare(mDpr, dpr);
    if(size == mViewArea && !dprChanged)
        return;
    mViewArea = size;
    mDpr = dpr;
    if(dprChanged)
        refreshPattern();
    updateSceneRect();
    refreshAutoResolution();
    relayoutView();
    update();
}

void CollageScene::setViewLayout(CollageLayout::Mode mode) {
    // first switch to Freehand starts from the arrangement that is on screen right now
    if(mode == CollageLayout::MODE_FREEFORM && mViewLayout != CollageLayout::MODE_FREEFORM && mMode == CollageMode::View)
        seedFreeRects();
    mViewLayout = mode;
    bool free = isFreeView();
    for(CollageItem *item : mById)
        item->setFreePlacement(free);
    updateSceneRect();
    relayoutView();
}

CollageLayout::Mode CollageScene::viewLayout() const { return mViewLayout; }

void CollageScene::setViewGap(int gap) {
    mViewGap = qBound(0, gap, 200);
    relayoutView();
}

int CollageScene::viewGap() const { return mViewGap; }

void CollageScene::setViewShape(qreal aspect) {
    aspect = aspect > 0.0 ? qBound(0.1, aspect, 10.0) : 0.0;
    if(qFuzzyCompare(1.0 + mViewShape, 1.0 + aspect))
        return;
    mViewShape = aspect;
    updateSceneRect();
    refreshAutoResolution();
    relayoutView();
    update();
}

qreal CollageScene::viewShape() const { return mViewShape; }

QList<CollageItem*> CollageScene::viewOrderItems() const {
    QList<CollageItem*> result;
    for(quint64 id : mViewOrder) {
        if(CollageItem *item = mById.value(id, nullptr))
            result.append(item);
    }
    return result;
}

// Flows the tiles into the view area. Only position and frame change, so per-tile
// crop (fit / zoom / center) survives re-flows.
void CollageScene::relayoutView() {
    if(mMode == CollageMode::Edit) {
        if(mStatic)
            relayoutStatic();
        return;
    }
    const QList<CollageItem*> list = viewOrderItems();
    if(list.isEmpty()) {
        emit collageChanged();
        return;
    }
    if(isFreeView()) {
        relayoutFree();
        return;
    }
    QList<qreal> aspects, weights;
    for(CollageItem *item : list) {
        aspects.append(item->layoutAspect());
        weights.append(item->viewWeight());
    }
    CollageLayout::Result result = CollageLayout::computeEx(mViewLayout, aspects, canvasRect(), mViewGap, weights, mLayoutOptions);
    applyLayoutResult(list, mViewLayout, result);
}

//------------------------------------------------------------------------------
// Freehand layout
void CollageScene::storeFreeRect(const CollageItem *item) {
    qreal w = mViewArea.width(), h = mViewArea.height();
    qreal unit = qMin(w, h);
    if(!item || unit < 1.0)
        return;
    QSizeF size = item->frameSize();
    FreeRect rect;
    rect.center = QPointF((item->pos().x() + size.width() / 2.0) / w, (item->pos().y() + size.height() / 2.0) / h);
    rect.size = QSizeF(size.width() / unit, size.height() / unit);
    mFreeRects.insert(item->id(), rect);
}

void CollageScene::seedFreeRects() {
    for(CollageItem *item : viewOrderItems()) {
        if(!mFreeRects.contains(item->id()))
            storeFreeRect(item);
    }
}

// new tile in Freehand: about a third of the window, centred, each one nudged a bit so they don't hide each other
void CollageScene::placeNewFreeTile(CollageItem *item) {
    qreal w = mViewArea.width(), h = mViewArea.height();
    qreal unit = qMin(w, h);
    qreal longSide = 0.35 * unit;
    qreal aspect = item->layoutAspect();
    QSizeF size = aspect >= 1.0 ? QSizeF(longSide, longSide / aspect) : QSizeF(longSide * aspect, longSide);
    qreal step = (mFreeCascade++ % 8) * 24.0;
    FreeRect rect;
    rect.center = QPointF(0.5 + step / w, 0.5 + step / h);
    rect.size = QSizeF(size.width() / unit, size.height() / unit);
    mFreeRects.insert(item->id(), rect);
}

void CollageScene::relayoutFree() {
    qreal w = mViewArea.width(), h = mViewArea.height();
    qreal unit = qMin(w, h);
    resetCellStyle();
    mSeparators.clear();
    syncSeparators();
    for(CollageItem *item : viewOrderItems()) {
        if(!mFreeRects.contains(item->id()))
            placeNewFreeTile(item);
        const FreeRect rect = mFreeRects.value(item->id());
        QSizeF size(rect.size.width() * unit, rect.size.height() * unit);
        // size first: the position clamp (itemChange) depends on the frame
        item->setFrameSize(size);
        item->setPos(rect.center.x() * w - item->frameSize().width() / 2.0,
                     rect.center.y() * h - item->frameSize().height() / 2.0);
    }
    update();
    emit collageChanged();
}

void CollageScene::storeFreeRects() {
    if(!isFreeView())
        return;
    for(CollageItem *item : selectedCollageItems())
        storeFreeRect(item);
}

QRectF CollageScene::freeWorldRect() const {
    qreal room = qMax(mViewArea.width(), mViewArea.height()) * 2.0;
    return QRectF(QPointF(0, 0), mViewArea).adjusted(-room, -room, room, room);
}

// keeps the scene a full window larger than the visible part on every side, so panning never hits an edge
void CollageScene::growSceneRect(const QRectF &visible) {
    if(visible.isEmpty())
        return;
    qreal w = visible.width(), h = visible.height();
    if(sceneRect().contains(visible.adjusted(-w, -h, w, h)))
        return;
    setSceneRect(sceneRect().united(visible.adjusted(-2 * w, -2 * h, 2 * w, 2 * h)));
}

// keeps a minimum part of the frame inside the world so a tile can never get lost
QPointF CollageScene::clampToView(const CollageItem *item, const QPointF &pos) const {
    QRectF area = isFreeView() ? freeWorldRect() : canvasRect();
    QSizeF size = item->frameSize();
    qreal keepX = qMin<qreal>(40.0 / mViewScale, size.width());
    qreal keepY = qMin<qreal>(40.0 / mViewScale, size.height());
    qreal x = qMax(area.left() - size.width() + keepX, qMin(pos.x(), area.right() - keepX));
    qreal y = qMax(area.top() - size.height() + keepY, qMin(pos.y(), area.bottom() - keepY));
    return QPointF(x, y);
}

//------------------------------------------------------------------------------
void CollageScene::swapTiles(quint64 a, quint64 b) {
    int ia = mViewOrder.indexOf(a);
    int ib = mViewOrder.indexOf(b);
    if(ia >= 0 && ib >= 0 && ia != ib)
        qSwap(mViewOrder[ia], mViewOrder[ib]);
    relayoutView();
}

// a dragged tile was dropped: swap with the tile under the cursor, otherwise snap back
void CollageScene::onSwapRequested(CollageItem *item, const QPointF &scenePos) {
    for(CollageItem *other : viewOrderItems()) {
        if(other != item && QRectF(other->pos(), other->frameSize()).contains(scenePos)) {
            swapTiles(item->id(), other->id());
            return;
        }
    }
    relayoutView();
}

// keyboard navigation in view mode
void CollageScene::selectNeighbor(int delta) {
    const QList<CollageItem*> list = viewOrderItems();
    if(list.isEmpty())
        return;
    const QList<CollageItem*> selected = selectedCollageItems();
    int index = selected.isEmpty() ? (delta > 0 ? -1 : list.size()) : list.indexOf(selected.first());
    index = qBound(0, index + delta, list.size() - 1);
    clearSelection();
    list.at(index)->setSelected(true);
}

void CollageScene::forget(quint64 id) {
    mById.remove(id);
    mViewOrder.removeAll(id);
    mEditStates.remove(id);
    mViewStates.remove(id);
    mFreeRects.remove(id);
}

void CollageScene::refreshPattern() {
    mPatternTile = ImageLib::backgroundPatternTile(mDpr);
}

//------------------------------------------------------------------------------
CollageItem *CollageScene::addImage(const QString &path, bool cascade) {
    QSize size = CollageImageLoader::probeSize(path);
    if(!size.isValid()) {
        // header probe is not supported by every plugin: fall back to a tiny decode
        QImage small = CollageImageLoader::readScaled(path, 256, &size);
        if(small.isNull() || !size.isValid())
            return nullptr;
    }

    CollageItem *item = new CollageItem(path, ++mNextId, size);
    qreal base = qMax(mCanvasSize.width(), mCanvasSize.height()) / 3.0;
    qreal aspect = item->aspect();
    item->setFrameSize(aspect >= 1.0 ? QSizeF(base, base / aspect) : QSizeF(base * aspect, base));
    int step = (imageCount() % 10) * 30;
    if(cascade)
        item->setPos(40 + step, 40 + step);
    item->setZValue(++mTopZ);
    item->setHandleSize(handleSize());
    item->setBorder(mBorderWidth, mBorderColor, mBorderStyle);
    clearSplits(); // a new tile changes the structure the dragged borders were made for
    item->setAnimationsAllowed(mAnimEnabled && mAnimActive);
    // the arrangement of the mode we are *not* in starts from the default frame
    CollageItem::State otherState = item->saveState();
    QGraphicsScene::addItem(item);
    mById.insert(item->id(), item);
    mViewOrder.append(item->id());
    bool view = (mMode == CollageMode::View);
    item->setViewMode(view || mStatic);
    item->setFreePlacement(isFreeView());
    if(view) {
        mEditStates.insert(item->id(), otherState);
    } else {
        CollageItem::State viewState;
        viewState.z = item->zValue(); // on top, like in the editor
        mViewStates.insert(item->id(), viewState);
    }
    connect(item, &CollageItem::edited, this, &CollageScene::itemEdited);
    connect(item, &CollageItem::swapRequested, this, &CollageScene::onSwapRequested);
    connect(item, &CollageItem::activated, this, [this, item]() { emit itemActivated(item); });
    connect(item, &CollageItem::contextRequested, this, [this, item]() { emit itemContextRequested(item); });

    reloadItem(item);
    if(view || mStatic)
        relayoutView();
    else
        emit collageChanged();
    return item;
}

QList<CollageItem*> CollageScene::collageItems() const {
    QList<CollageItem*> result;
    for(QGraphicsItem *graphicsItem : items(Qt::AscendingOrder)) {
        QGraphicsObject *object = graphicsItem->toGraphicsObject();
        if(CollageItem *item = qobject_cast<CollageItem*>(object))
            result.append(item);
    }
    return result;
}

QList<CollageItem*> CollageScene::selectedCollageItems() const {
    QList<CollageItem*> result;
    for(QGraphicsItem *graphicsItem : selectedItems()) {
        QGraphicsObject *object = graphicsItem->toGraphicsObject();
        if(CollageItem *item = qobject_cast<CollageItem*>(object))
            result.append(item);
    }
    return result;
}

int CollageScene::imageCount() const {
    return mById.size();
}

void CollageScene::removeCollageItem(CollageItem *item) {
    if(!item)
        return;
    forget(item->id());
    removeItem(item);
    delete item;
    clearSplits();
    if(mMode == CollageMode::View || mStatic)
        relayoutView();
    else
        emit collageChanged();
}

void CollageScene::removeSelected() {
    const QList<CollageItem*> selected = selectedCollageItems();
    if(selected.isEmpty())
        return;
    for(CollageItem *item : selected) {
        forget(item->id());
        removeItem(item);
        delete item;
    }
    clearSplits();
    if(mMode == CollageMode::View || mStatic)
        relayoutView();
    else
        emit collageChanged();
}

void CollageScene::clearCollage() {
    const QList<CollageItem*> all = collageItems();
    for(CollageItem *item : all) {
        forget(item->id());
        removeItem(item);
        delete item;
    }
    mTopZ = 0;
    mBottomZ = 0;
    mFreeCascade = 0;
    mFreeRects.clear();
    mEditorInitialized = false;
    clearSplits();
    mSeparators.clear();
    syncSeparators();
    emit collageChanged();
}

//------------------------------------------------------------------------------
void CollageScene::applyLayout(CollageLayout::Mode mode, int gap) {
    if(mMode != CollageMode::Edit)
        return; // the view re-flows itself (setViewLayout)
    const QList<CollageItem*> list = collageItems();
    if(list.isEmpty())
        return;
    QList<qreal> aspects;
    for(CollageItem *item : list)
        aspects.append(item->aspect());
    QList<QRectF> rects = CollageLayout::compute(mode, aspects, canvasRect(), gap);
    for(int i = 0; i < list.size() && i < rects.size(); i++) {
        list.at(i)->setPos(rects.at(i).topLeft());
        list.at(i)->setFrameSize(rects.at(i).size());
        list.at(i)->setFit(CollageFit::Fill);
        list.at(i)->resetCrop();
    }
    update();
    emit collageChanged();
}

void CollageScene::bringToFront(CollageItem *item) {
    if(item)
        item->setZValue(++mTopZ);
}

void CollageScene::sendToBack(CollageItem *item) {
    if(item)
        item->setZValue(--mBottomZ);
}

void CollageScene::fillCanvas(CollageItem *item) {
    if(!item)
        return;
    item->setPos(canvasRect().topLeft());
    item->setFrameSize(canvasRect().size());
    emit collageChanged();
}

int CollageScene::longSideFor(const CollageItem *item) const {
    switch(item->resolution()) {
        case CollageRes::Original: return 0;
        case CollageRes::Res4K:    return 3840;
        case CollageRes::Res2K:    return 2560;
        case CollageRes::Res1080:  return 1920;
        case CollageRes::Res720:   return 1280;
        case CollageRes::Auto:
        default: {
            // big enough for both the export canvas and the screen the view is shown on
            int canvasLong = qMax(mCanvasSize.width(), mCanvasSize.height());
            int viewLong = qRound(qMax(mViewArea.width(), mViewArea.height()) * mDpr);
            return qMax(canvasLong, viewLong);
        }
    }
}

void CollageScene::reloadItem(CollageItem *item) {
    if(!item)
        return;
    int generation = item->nextGeneration();
    mLoader.load(item->id(), generation, item->path(), longSideFor(item));
}

void CollageScene::onLoaded(quint64 id, int generation, QImage image, QSize originalSize, bool animated) {
    Q_UNUSED(originalSize)
    CollageItem *item = mById.value(id, nullptr);
    // item removed meanwhile, or a newer request superseded this one
    if(!item || item->generation() != generation)
        return;
    if(image.isNull()) {
        item->setLoadFailed();
    } else {
        item->setWorkingPixmap(QPixmap::fromImage(image));
        item->setAnimated(animated);
    }
    emit collageChanged();
}

bool CollageScene::animationsEnabled() const { return mAnimEnabled; }

void CollageScene::setAnimationsEnabled(bool enabled) {
    mAnimEnabled = enabled;
    applyAnimationState();
}

void CollageScene::setAnimationsActive(bool active) {
    mAnimActive = active;
    applyAnimationState();
}

void CollageScene::applyAnimationState() {
    for(CollageItem *item : mById)
        item->setAnimationsAllowed(mAnimEnabled && mAnimActive);
}

qint64 CollageScene::memoryBytes() const {
    qint64 total = 0;
    for(CollageItem *item : mById)
        total += item->memoryBytes();
    return total;
}

//------------------------------------------------------------------------------
qreal CollageScene::viewScale() const { return mViewScale; }

void CollageScene::setViewScale(qreal scale) {
    mViewScale = qBound<qreal>(0.001, scale, 100.0);
    for(CollageItem *item : mById)
        item->setHandleSize(handleSize());
    syncSeparators(); // the grab area is a few screen px wide
    update();
}

// handles are ~10 screen px regardless of zoom
qreal CollageScene::handleSize() const { return 10.0 / mViewScale; }
qreal CollageScene::snapThreshold() const { return 8.0 / mViewScale; }

qreal CollageScene::snapValue(qreal value, bool horizontal) const {
    QRectF c = canvasRect();
    const qreal targets[3] = {
        horizontal ? c.left() : c.top(),
        horizontal ? c.center().x() : c.center().y(),
        horizontal ? c.right() : c.bottom()
    };
    qreal th = snapThreshold();
    for(qreal target : targets) {
        if(qAbs(value - target) < th)
            return target;
    }
    return value;
}

// snaps the frame's edges / center to the canvas edges / center
QPointF CollageScene::snapPosition(const CollageItem *item, const QPointF &pos) const {
    QRectF c = canvasRect();
    QSizeF s = item->frameSize();
    qreal th = snapThreshold();
    QPointF p = pos;

    if(qAbs(p.x() - c.left()) < th)
        p.setX(c.left());
    else if(qAbs(p.x() + s.width() - c.right()) < th)
        p.setX(c.right() - s.width());
    else if(qAbs(p.x() + s.width() / 2.0 - c.center().x()) < th)
        p.setX(c.center().x() - s.width() / 2.0);

    if(qAbs(p.y() - c.top()) < th)
        p.setY(c.top());
    else if(qAbs(p.y() + s.height() - c.bottom()) < th)
        p.setY(c.bottom() - s.height());
    else if(qAbs(p.y() + s.height() / 2.0 - c.center().y()) < th)
        p.setY(c.center().y() - s.height() / 2.0);
    return p;
}

//------------------------------------------------------------------------------
void CollageScene::drawBackground(QPainter *painter, const QRectF &rect) {
    if(mMode == CollageMode::View && !mThemeBackground) {
        // the collage canvas colour: everywhere when the canvas is the window, inside the frame otherwise
        auto fillCanvas = [this, painter](const QRectF &area) {
            if(mCanvasColor.alpha() < 255) {
                QBrush checker(mChecker);
                checker.setTransform(QTransform::fromScale(1.0 / mViewScale, 1.0 / mViewScale));
                painter->fillRect(area, checker);
            }
            painter->fillRect(area, mCanvasColor);
        };
        if(!hasViewShape()) {
            fillCanvas(rect);
            return;
        }
        painter->fillRect(rect, settings->colorScheme().background);
        fillCanvas(canvasRect().intersected(rect));
        return;
    }
    if(mMode == CollageMode::View) {
        // like the image viewer: theme background + pattern that stays put while zooming / panning
        painter->fillRect(rect, settings->colorScheme().background);
        if(!mPatternTile.isNull()) {
            painter->save();
            painter->resetTransform();
            painter->drawTiledPixmap(painter->viewport(), mPatternTile);
            painter->restore();
        }
        return;
    }
    // area around the canvas follows the app theme
    painter->fillRect(rect, settings->colorScheme().background);

    QRectF canvas = canvasRect();
    if(mCanvasColor.alpha() < 255) {
        // transparent canvas: show a checkerboard that keeps its on-screen size
        QBrush checker(mChecker);
        checker.setTransform(QTransform::fromScale(1.0 / mViewScale, 1.0 / mViewScale));
        painter->fillRect(canvas, checker);
    }
    painter->fillRect(canvas, mCanvasColor);
}

void CollageScene::drawForeground(QPainter *painter, const QRectF &rect) {
    if(mMode == CollageMode::View && !hasViewShape())
        return;
    QRectF canvas = canvasRect();
    // dim everything outside the canvas (in the editor: what is cut off on export; in the view: the shape frame)
    QPainterPath outside;
    outside.addRect(rect);
    QPainterPath inside;
    inside.addRect(canvas);
    painter->fillPath(outside.subtracted(inside), QColor(0, 0, 0, mMode == CollageMode::View ? 70 : 100));

    QPen pen(QColor(255, 255, 255, 90), 1);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(canvas);
}

//------------------------------------------------------------------------------
// Renders one item at the resolution its slot actually needs, then releases the pixels.
// This keeps export memory bounded no matter how many images the collage has.
void CollageScene::drawItem(QPainter *painter, CollageItem *item) const {
    QRectF frame(item->pos(), item->frameSize());
    if(!frame.intersects(canvasRect()))
        return;

    QSize original = item->originalSize();
    if(!original.isValid())
        return;
    CollageMapping m = item->mapping();

    // size of the whole image needed so that the visible part is drawn at 1:1
    qreal fullW = m.target.width()  / m.source.width();
    qreal fullH = m.target.height() / m.source.height();
    qreal factor = qMin<qreal>(1.0, qMax(fullW / original.width(), fullH / original.height()));
    int longSide = qMax(1, qCeil(factor * qMax(original.width(), original.height())));
    int cap = longSideFor(item);
    if(cap > 0)
        longSide = qMin(longSide, cap);

    QImage image = CollageImageLoader::readScaled(item->path(), longSide);
    if(image.isNull())
        return;

    QRectF source(m.source.left() * image.width(),  m.source.top() * image.height(),
                  m.source.width() * image.width(), m.source.height() * image.height());
    // same as on screen: frame-local coordinates, layout rotation around the centre, shape clip, outline
    painter->save();
    painter->translate(frame.topLeft());
    if(!qFuzzyIsNull(item->rotation())) {
        QPointF center(frame.width() / 2.0, frame.height() / 2.0);
        painter->translate(center);
        painter->rotate(item->rotation());
        painter->translate(-center);
    }
    painter->setOpacity(item->opacity());
    QPainterPath clip = item->clipPath();
    painter->save();
    if(item->cornerRadius() > 0 || item->shapeSpec().shape != 0)
        painter->setClipPath(clip, Qt::IntersectClip);
    painter->drawImage(m.target, image, source);
    painter->restore();
    if(item->borderWidth() > 0)
        CollageShape::paintBorder(painter, clip, item->borderWidth(), item->effectiveBorderColor(), item->borderStyle());
    painter->restore();
}

bool CollageScene::exportImage(const QString &path, QString *error, const std::function<bool(int, int)> &progress) {
    auto fail = [error](const QString &message) {
        if(error)
            *error = message;
        return false;
    };

    if(mMode != CollageMode::Edit)
        return fail(tr("Switch to the editor to export."));

    if(static_cast<qint64>(mCanvasSize.width()) * mCanvasSize.height() > MAX_EXPORT_PIXELS)
        return fail(tr("The canvas is too large to export (%1 x %2).").arg(mCanvasSize.width()).arg(mCanvasSize.height()));

    QString suffix = QFileInfo(path).suffix().toLower();
    bool opaqueFormat = (suffix == "jpg" || suffix == "jpeg" || suffix == "bmp");

    QImage out(mCanvasSize, QImage::Format_ARGB32_Premultiplied);
    if(out.isNull())
        return fail(tr("Not enough memory for a %1 x %2 image.").arg(mCanvasSize.width()).arg(mCanvasSize.height()));

    QColor background = mCanvasColor;
    if(opaqueFormat) // these formats have no alpha channel
        background = (background.alpha() == 0) ? QColor(Qt::white) : QColor(background.red(), background.green(), background.blue());
    out.fill(background);

    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QList<CollageItem*> list = collageItems();
    int total = list.size();
    for(int i = 0; i < total; i++) {
        if(progress && !progress(i, total))
            return fail(tr("Export cancelled."));
        drawItem(&painter, list.at(i));
    }
    painter.end();
    if(progress)
        progress(total, total);

    QImage result = opaqueFormat ? out.convertToFormat(QImage::Format_RGB32) : out;
    QImageWriter writer(path);
    if(suffix == "jpg" || suffix == "jpeg")
        writer.setQuality(95);
    if(!writer.write(result))
        return fail(writer.errorString());
    return true;
}
