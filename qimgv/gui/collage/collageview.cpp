#include "collageview.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QScrollBar>
#include <QLineF>
#include <QtMath>

namespace {
const qreal MIN_VIEW_SCALE = 0.01;
const qreal MAX_VIEW_SCALE = 50.0;
}

CollageView::CollageView(CollageScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent),
      mScene(scene)
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
}

bool CollageView::isAutoFit() const {
    return mAutoFit;
}

bool CollageView::isViewMode() const {
    return mViewMode;
}

void CollageView::setCropMode(bool on) {
    mCropMode = on;
}

// view mode: dragging empty space pans the (zoomed) collage, edit mode: rubber band selection
void CollageView::applyDragMode() {
    setDragMode(mViewMode ? QGraphicsView::ScrollHandDrag : QGraphicsView::RubberBandDrag);
    // the view-mode background is pinned to the viewport (not the scene), so Qt's scroll-by-blit
    // would smear it whenever the view pans; repaint everything instead
    setViewportUpdateMode(mViewMode ? QGraphicsView::FullViewportUpdate : QGraphicsView::SmartViewportUpdate);
}

void CollageView::setViewMode(bool mode) {
    mViewMode = mode;
    applyDragMode();
    if(mViewMode)
        updateViewArea();
    fitCanvas();
}

// the view scene is exactly as big as the viewport; re-flow whenever it changes
void CollageView::updateViewArea() {
    if(mViewMode)
        mScene->setViewArea(QSizeF(viewport()->size()), devicePixelRatioF());
}

CollageItem *CollageView::itemUnderCursor(const QPoint &pos) const {
    for(QGraphicsItem *graphicsItem : items(pos)) {
        if(CollageItem *item = qobject_cast<CollageItem*>(graphicsItem->toGraphicsObject()))
            return item;
    }
    return nullptr;
}

void CollageView::viewScaleChanged() {
    mScene->setViewScale(transform().m11());
}

// Freehand: keep the scene bigger than what is on screen so the canvas can be dragged anywhere, at any zoom
void CollageView::ensurePanRoom() {
    if(mGrowing || !mViewMode || !mScene->isFreeView())
        return;
    mGrowing = true;
    QPoint centerPx = viewport()->rect().center();
    QPointF before = mapToScene(centerPx);
    mScene->growSceneRect(mapToScene(viewport()->rect()).boundingRect());
    // a changed scene rect can shift the scroll bars: keep what the user is looking at where it is
    QPointF after = mapToScene(centerPx);
    if(QLineF(before, after).length() > 0.01)
        centerOn(before);
    mGrowing = false;
}

void CollageView::scrollContentsBy(int dx, int dy) {
    QGraphicsView::scrollContentsBy(dx, dy);
    ensurePanRoom();
}

void CollageView::fitCanvas() {
    mAutoFit = true;
    if(mViewMode) {
        // scene == viewport, identity shows the whole collage
        resetTransform();
        centerOn(mScene->canvasRect().center()); // Freehand's scene is larger than the window
        viewScaleChanged();
        ensurePanRoom();
        return;
    }
    QRectF target = mScene->canvasRect();
    qreal margin = qMax(target.width(), target.height()) * 0.03;
    fitInView(target.adjusted(-margin, -margin, margin, margin), Qt::KeepAspectRatio);
    viewScaleChanged();
}

void CollageView::zoomToActualSize() {
    mAutoFit = false;
    resetTransform();
    centerOn(mScene->canvasRect().center());
    viewScaleChanged();
}

void CollageView::zoomBy(qreal factor) {
    qreal current = transform().m11();
    qreal target = qBound(MIN_VIEW_SCALE, current * factor, MAX_VIEW_SCALE);
    if(qFuzzyCompare(target, current))
        return;
    mAutoFit = false;
    qreal applied = target / current;
    scale(applied, applied);
    viewScaleChanged();
    ensurePanRoom();
}

void CollageView::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    updateViewArea();
    if(mAutoFit)
        fitCanvas();
}

void CollageView::showEvent(QShowEvent *event) {
    QGraphicsView::showEvent(event);
    updateViewArea();
    if(mAutoFit)
        fitCanvas();
}

void CollageView::wheelEvent(QWheelEvent *event) {
    int delta = event->angleDelta().y();
    if(delta == 0) {
        event->ignore();
        return;
    }
    qreal steps = delta / 120.0;
    // Shift + wheel (or plain wheel while Crop is on): zoom the picture inside the frame under the cursor
    if(event->modifiers().testFlag(Qt::ShiftModifier) || mCropMode) {
        // view / crop: the frame under the cursor, edit: the single selected frame
        CollageItem *item = nullptr;
        if(mViewMode || mCropMode) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            item = itemUnderCursor(event->position().toPoint());
#else
            item = itemUnderCursor(event->pos());
#endif
        } else {
            const QList<CollageItem*> selected = mScene->selectedCollageItems();
            if(selected.size() == 1)
                item = selected.first();
        }
        if(item) {
            if(!item->isSelected()) {
                mScene->clearSelection();
                item->setSelected(true);
            }
            if(item->fit() != CollageFit::Fill)
                item->setFit(CollageFit::Fill); // zoom / crop only exist in Fill mode
            item->setContentZoom(item->contentZoom() * qPow(1.1, steps));
            emit mScene->itemEdited();
            event->accept();
            return;
        }
    }
    zoomBy(qPow(1.15, steps));
    event->accept();
}

// middle button pans: re-dispatch it as a left-drag in hand-drag mode
void CollageView::mousePressEvent(QMouseEvent *event) {
    if(event->button() == Qt::MiddleButton) {
        mPanning = true;
        mAutoFit = false;
        setDragMode(QGraphicsView::ScrollHandDrag);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(event->pos()), Qt::LeftButton, Qt::LeftButton, event->modifiers());
        QGraphicsView::mousePressEvent(&press);
        event->accept();
        return;
    }
    setFocus();
    QGraphicsView::mousePressEvent(event);
}

void CollageView::mouseMoveEvent(QMouseEvent *event) {
    if(mPanning) {
        QMouseEvent move(QEvent::MouseMove, QPointF(event->pos()), Qt::NoButton, Qt::LeftButton, event->modifiers());
        QGraphicsView::mouseMoveEvent(&move);
        event->accept();
        return;
    }
    // dragging empty space pans: from now on a resize keeps the position instead of re-fitting
    if(mViewMode && (event->buttons() & Qt::LeftButton) && dragMode() == QGraphicsView::ScrollHandDrag
       && !mScene->mouseGrabberItem())
        mAutoFit = false;
    QGraphicsView::mouseMoveEvent(event);
}

void CollageView::mouseReleaseEvent(QMouseEvent *event) {
    if(mPanning && event->button() == Qt::MiddleButton) {
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(event->pos()), Qt::LeftButton, Qt::NoButton, event->modifiers());
        QGraphicsView::mouseReleaseEvent(&release);
        applyDragMode();
        mPanning = false;
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void CollageView::nudgeSelection(int dx, int dy) {
    const QList<CollageItem*> selected = mScene->selectedCollageItems();
    for(CollageItem *item : selected)
        item->moveBy(dx, dy);
    if(!selected.isEmpty())
        emit mScene->itemEdited();
}

void CollageView::keyPressEvent(QKeyEvent *event) {
    int step = event->modifiers().testFlag(Qt::ShiftModifier) ? 10 : 1;
    bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);
    // Freehand layout: arrows / PgUp / PgDn work like in the editor
    bool tileKeys = mViewMode ? !mScene->isFreeView() : mScene->staticCanvas();
    switch(event->key()) {
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            mScene->removeSelected();
            break;
        case Qt::Key_A:
            if(!ctrl) { event->ignore(); return; }
            for(CollageItem *item : mScene->collageItems())
                item->setSelected(true);
            break;
        case Qt::Key_Escape:
            if(mScene->selectedItems().isEmpty()) { event->ignore(); return; }
            mScene->clearSelection();
            break;
        case Qt::Key_Left:
            if(tileKeys) mScene->selectNeighbor(-1); else nudgeSelection(-step, 0);
            break;
        case Qt::Key_Right:
            if(tileKeys) mScene->selectNeighbor(1); else nudgeSelection(step, 0);
            break;
        case Qt::Key_Up:
            if(tileKeys) { event->ignore(); return; }
            nudgeSelection(0, -step);
            break;
        case Qt::Key_Down:
            if(tileKeys) { event->ignore(); return; }
            nudgeSelection(0, step);
            break;
        case Qt::Key_PageUp:
            if(tileKeys) { event->ignore(); return; }
            for(CollageItem *item : mScene->selectedCollageItems())
                mScene->bringToFront(item);
            break;
        case Qt::Key_PageDown:
            if(tileKeys) { event->ignore(); return; }
            for(CollageItem *item : mScene->selectedCollageItems())
                mScene->sendToBack(item);
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            // view: open the selected tile in the normal viewer
            const QList<CollageItem*> selected = mScene->selectedCollageItems();
            if(!mViewMode || selected.isEmpty()) { event->ignore(); return; }
            emit mScene->itemActivated(selected.first());
            break;
        }
        case Qt::Key_Space: {
            // play / pause the selected animated tiles, or every animation when none is selected
            QList<CollageItem*> animated;
            for(CollageItem *item : mScene->selectedCollageItems()) {
                if(item->isAnimated())
                    animated.append(item);
            }
            if(animated.isEmpty()) {
                emit animationToggleRequested();
                break;
            }
            bool play = !animated.first()->isPlaying();
            for(CollageItem *item : animated)
                item->setAnimationEnabled(play);
            if(play && !mScene->animationsEnabled())
                emit animationToggleRequested(); // a paused collage would ignore the tile
            emit mScene->itemEdited();
            break;
        }
        case Qt::Key_E:
            if(ctrl) { event->ignore(); return; }
            emit modeToggleRequested();
            break;
        case Qt::Key_0:
            if(!ctrl) { event->ignore(); return; }
            fitCanvas();
            break;
        case Qt::Key_1:
            if(!ctrl) { event->ignore(); return; }
            zoomToActualSize();
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomBy(1.15);
            break;
        case Qt::Key_Minus:
            zoomBy(1.0 / 1.15);
            break;
        default:
            // let shortcuts (fullscreen etc.) reach the main window
            event->ignore();
            return;
    }
    event->accept();
}

//------------------------------------------------------------------------------
void CollageView::dragEnterEvent(QDragEnterEvent *event) {
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        event->ignore();
}

void CollageView::dragMoveEvent(QDragMoveEvent *event) {
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        event->ignore();
}

void CollageView::dropEvent(QDropEvent *event) {
    QStringList paths;
    for(const QUrl &url : event->mimeData()->urls()) {
        if(url.isLocalFile())
            paths << url.toLocalFile();
    }
    if(paths.isEmpty()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    emit filesDropped(paths);
}
