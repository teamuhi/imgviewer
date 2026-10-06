#include "collageitem.h"
#include "collagescene.h"
#include "settings.h"
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QStyleOptionGraphicsItem>
#include <QFileInfo>
#include <QCursor>
#include <QGuiApplication>
#include <QApplication>
#include <QLineF>
#include <QMovie>
#include <QPolygonF>

namespace {
const qreal MIN_FRAME_SIZE = 24.0;
const qreal MAX_ZOOM = 8.0;
}

CollageItem::CollageItem(const QString &path, quint64 id, const QSize &originalSize)
    : mId(id),
      mPath(path),
      mOriginal(originalSize),
      mFrame(200, 200)
{
    setFlag(ItemIsMovable, true);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setToolTip(QFileInfo(path).fileName());
}

CollageItem::State CollageItem::saveState() const {
    State state;
    state.pos = pos();
    state.frame = mFrame;
    state.fit = mFit;
    state.zoom = mZoom;
    state.center = mCenter;
    state.z = zValue();
    return state;
}

void CollageItem::restoreState(const State &state) {
    setPos(state.pos);
    setFrameSize(state.frame);
    mFit = state.fit;
    mZoom = state.zoom;
    mCenter = state.center;
    setZValue(state.z);
    update();
}

void CollageItem::setViewMode(bool mode) {
    if(mViewMode == mode)
        return;
    prepareGeometryChange(); // shape() / handles differ
    mViewMode = mode;
    setFlag(ItemIsMovable, editLike());
    mDrag = DRAG_NONE;
    update();
}

bool CollageItem::isViewMode() const { return mViewMode; }

void CollageItem::setFreePlacement(bool free) {
    if(mFree == free)
        return;
    prepareGeometryChange(); // shape() / handles differ
    mFree = free;
    setFlag(ItemIsMovable, editLike());
    mDrag = DRAG_NONE;
    update();
}

bool CollageItem::isFreePlacement() const { return mFree; }
qreal CollageItem::viewAspect() const { return mViewAspect; }
void CollageItem::setViewAspect(qreal aspect) { mViewAspect = qMax<qreal>(0.0, aspect); }
qreal CollageItem::viewWeight() const { return mViewWeight; }
void CollageItem::setViewWeight(qreal weight) { mViewWeight = qBound<qreal>(0.5, weight, 3.0); }
qreal CollageItem::layoutAspect() const { return mViewAspect > 0.0 ? mViewAspect : aspect(); }

quint64 CollageItem::id() const { return mId; }
QString CollageItem::path() const { return mPath; }
QString CollageItem::displayName() const { return QFileInfo(mPath).fileName(); }
QSize CollageItem::originalSize() const { return mOriginal; }

qreal CollageItem::aspect() const {
    if(mOriginal.isValid() && mOriginal.height() > 0)
        return static_cast<qreal>(mOriginal.width()) / mOriginal.height();
    return 1.0;
}

QSizeF CollageItem::frameSize() const { return mFrame; }

void CollageItem::setFrameSize(const QSizeF &size) {
    QSizeF clamped(qMax(1.0, size.width()), qMax(1.0, size.height()));
    if(clamped == mFrame)
        return;
    prepareGeometryChange();
    mFrame = clamped;
    update();
}

QRectF CollageItem::frameRect() const {
    return QRectF(QPointF(0, 0), mFrame);
}

CollageFit CollageItem::fit() const { return mFit; }

void CollageItem::setFit(CollageFit fit) {
    mFit = fit;
    update();
}

qreal CollageItem::contentZoom() const { return mZoom; }

void CollageItem::setContentZoom(qreal zoom) {
    mZoom = qBound<qreal>(1.0, zoom, MAX_ZOOM);
    update();
}

QPointF CollageItem::contentCenter() const { return mCenter; }

void CollageItem::setContentCenter(const QPointF &center) {
    mCenter = QPointF(qBound<qreal>(0.0, center.x(), 1.0), qBound<qreal>(0.0, center.y(), 1.0));
    update();
}

void CollageItem::resetCrop() {
    mZoom = 1.0;
    mCenter = QPointF(0.5, 0.5);
    update();
}

qreal CollageItem::cornerRadius() const { return mRadius; }

void CollageItem::setCornerRadius(qreal radius) {
    mRadius = qMax<qreal>(0.0, radius);
    update();
}

CollageRes CollageItem::resolution() const { return mRes; }
void CollageItem::setResolution(CollageRes res) { mRes = res; }
bool CollageItem::cropMode() const { return mCropMode; }

void CollageItem::setCropMode(bool mode) {
    if(mCropMode == mode)
        return;
    prepareGeometryChange(); // shape() drops the handles while cropping
    mCropMode = mode;
    update();
}

//------------------------------------------------------------------------------
int CollageItem::generation() const { return mGeneration; }
int CollageItem::nextGeneration() { return ++mGeneration; }

void CollageItem::setWorkingPixmap(const QPixmap &pixmap) {
    mPixmap = pixmap;
    mFailed = pixmap.isNull();
    update();
}

void CollageItem::setLoadFailed() {
    dropMovie();
    mAnimated = false;
    mPixmap = QPixmap();
    mFailed = true;
    update();
}

//------------------------------------------------------------------------------
// Animation. The movie decodes straight to the working size of the pixmap, so a tile never
// holds more than its current frame in memory.
bool CollageItem::isAnimated() const { return mAnimated; }
bool CollageItem::animationEnabled() const { return mAnimEnabled; }
bool CollageItem::animationLoop() const { return mAnimLoop; }
int CollageItem::animationSpeed() const { return mAnimSpeed; }

bool CollageItem::isPlaying() const {
    return mMovie && mMovie->state() == QMovie::Running;
}

// called after every (re)load: the movie has to be rebuilt for the new working size
void CollageItem::setAnimated(bool animated) {
    dropMovie();
    mAnimated = animated && !mPixmap.isNull();
    mAnimFinished = false;
    mLastFrame = -1;
    updatePlayback();
    update();
}

void CollageItem::setAnimationEnabled(bool enabled) {
    if(mAnimEnabled == enabled && !(enabled && mAnimFinished))
        return;
    mAnimEnabled = enabled;
    if(enabled && mAnimFinished) { // "play once" already ran out: play again from the start
        mAnimFinished = false;
        mLastFrame = -1;
        if(mMovie)
            mMovie->stop();
    }
    updatePlayback();
    update();
}

void CollageItem::setAnimationLoop(bool loop) {
    if(mAnimLoop == loop)
        return;
    mAnimLoop = loop;
    if(loop && mAnimFinished) {
        mAnimFinished = false;
        mLastFrame = -1;
        if(mMovie)
            mMovie->stop();
    }
    updatePlayback();
}

void CollageItem::setAnimationSpeed(int percent) {
    mAnimSpeed = qBound(10, percent, 800);
    if(mMovie)
        mMovie->setSpeed(mAnimSpeed);
}

void CollageItem::setAnimationsAllowed(bool allowed) {
    if(mAnimAllowed == allowed)
        return;
    mAnimAllowed = allowed;
    updatePlayback();
    update();
}

void CollageItem::createMovie() {
    mMovie = new QMovie(this);
    mMovie->setFileName(mPath);
    mMovie->setCacheMode(QMovie::CacheNone);
    if(mPixmap.size().isValid())
        mMovie->setScaledSize(mPixmap.size());
    if(!mMovie->isValid()) { // unreadable as an animation: keep showing the still frame
        dropMovie();
        mAnimated = false;
        return;
    }
    mMovie->setSpeed(mAnimSpeed);
    connect(mMovie, &QMovie::frameChanged, this, &CollageItem::onMovieFrame);
    connect(mMovie, &QMovie::finished, this, [this]() {
        // files that loop only a few times: keep going unless the tile is set to play once
        if(mAnimLoop && mMovie && mAnimAllowed && mAnimEnabled)
            mMovie->start();
        else
            mAnimFinished = true;
    });
}

void CollageItem::dropMovie() {
    if(!mMovie)
        return;
    mMovie->stop();
    mMovie->disconnect(this);
    delete mMovie;
    mMovie = nullptr;
}

void CollageItem::updatePlayback() {
    bool wanted = mAnimated && mAnimAllowed && mAnimEnabled && !mFailed && !mPixmap.isNull();
    if(!wanted) {
        if(mMovie && mMovie->state() == QMovie::Running)
            mMovie->setPaused(true); // keeps the current frame on screen
        return;
    }
    if(!mMovie)
        createMovie();
    if(!mMovie || mAnimFinished)
        return;
    if(mMovie->state() == QMovie::Paused)
        mMovie->setPaused(false);
    else if(mMovie->state() == QMovie::NotRunning)
        mMovie->start();
}

void CollageItem::onMovieFrame(int frame) {
    if(!mMovie)
        return;
    // the frame number going back means the animation wrapped around
    if(!mAnimLoop && mLastFrame > 0 && frame <= mLastFrame) {
        mAnimFinished = true;
        mMovie->setPaused(true);
        update();
        return;
    }
    mLastFrame = frame;
    QPixmap pixmap = mMovie->currentPixmap();
    if(pixmap.isNull())
        return;
    mPixmap = pixmap;
    update(frameRect());
}

bool CollageItem::isLoaded() const { return !mPixmap.isNull(); }
bool CollageItem::hasFailed() const { return mFailed; }
QSize CollageItem::workingSize() const { return mPixmap.size(); }

qint64 CollageItem::memoryBytes() const {
    return static_cast<qint64>(mPixmap.width()) * mPixmap.height() * 4;
}

void CollageItem::setHandleSize(qreal size) {
    size = qMax<qreal>(6.0, size);
    if(qFuzzyCompare(size, mHandle))
        return;
    prepareGeometryChange(); // bounding rect includes the handles
    mHandle = size;
}

//------------------------------------------------------------------------------
// Maps the image into the frame. Shared by painting and export so both agree.
CollageMapping CollageItem::computeMapping(const QSize &image, const QSizeF &frame, CollageFit fit,
                                           qreal zoom, const QPointF &center)
{
    CollageMapping m;
    m.source = QRectF(0, 0, 1, 1);
    m.target = QRectF(QPointF(0, 0), frame);
    qreal iw = image.width(), ih = image.height();
    qreal fw = frame.width(), fh = frame.height();
    if(iw <= 0 || ih <= 0 || fw <= 0 || fh <= 0)
        return m;

    switch(fit) {
        case CollageFit::Stretch:
            m.scale = fw / iw;
            break;
        case CollageFit::Contain: {
            qreal s = qMin(fw / iw, fh / ih);
            qreal tw = iw * s, th = ih * s;
            m.target = QRectF((fw - tw) / 2.0, (fh - th) / 2.0, tw, th);
            m.scale = s;
            break;
        }
        case CollageFit::Fill:
        default: {
            qreal s = qMax(fw / iw, fh / ih) * qMax<qreal>(1.0, zoom);
            qreal sw = qMin<qreal>(1.0, fw / (s * iw));
            qreal sh = qMin<qreal>(1.0, fh / (s * ih));
            qreal cx = qBound(sw / 2.0, center.x(), 1.0 - sw / 2.0);
            qreal cy = qBound(sh / 2.0, center.y(), 1.0 - sh / 2.0);
            m.source = QRectF(cx - sw / 2.0, cy - sh / 2.0, sw, sh);
            m.scale = s;
            break;
        }
    }
    return m;
}

CollageMapping CollageItem::mapping() const {
    return computeMapping(mOriginal, mFrame, mFit, mZoom, mCenter);
}

//------------------------------------------------------------------------------
QRectF CollageItem::boundingRect() const {
    qreal m = mHandle + 2.0; // room for the selection handles
    return frameRect().adjusted(-m, -m, m, m);
}

QPainterPath CollageItem::shape() const {
    QPainterPath path;
    path.addRect(frameRect());
    if(handlesActive()) {
        for(int h = H_TL; h <= H_L; h++)
            path.addRect(handleRect(static_cast<Handle>(h)));
    }
    return path;
}

QRectF CollageItem::handleRect(Handle handle) const {
    QRectF f = frameRect();
    QPointF c;
    switch(handle) {
        case H_TL: c = f.topLeft(); break;
        case H_T:  c = QPointF(f.center().x(), f.top()); break;
        case H_TR: c = f.topRight(); break;
        case H_R:  c = QPointF(f.right(), f.center().y()); break;
        case H_BR: c = f.bottomRight(); break;
        case H_B:  c = QPointF(f.center().x(), f.bottom()); break;
        case H_BL: c = f.bottomLeft(); break;
        case H_L:  c = QPointF(f.left(), f.center().y()); break;
        default: return QRectF();
    }
    return QRectF(c.x() - mHandle / 2.0, c.y() - mHandle / 2.0, mHandle, mHandle);
}

CollageItem::Handle CollageItem::handleAt(const QPointF &local) const {
    if(handlesActive()) {
        for(int h = H_TL; h <= H_L; h++) {
            if(handleRect(static_cast<Handle>(h)).contains(local))
                return static_cast<Handle>(h);
        }
    }
    return frameRect().contains(local) ? H_BODY : H_NONE;
}

void CollageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(widget)
    qreal lod = option->levelOfDetailFromTransform(painter->worldTransform());
    if(lod <= 0)
        lod = 1.0;
    QRectF frame = frameRect();

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setRenderHint(QPainter::Antialiasing, true);

    if(!mPixmap.isNull() && mOriginal.isValid()) {
        painter->save();
        if(mRadius > 0) {
            QPainterPath clip;
            clip.addRoundedRect(frame, mRadius, mRadius);
            painter->setClipPath(clip, Qt::IntersectClip);
        }
        CollageMapping m = mapping();
        QRectF src(m.source.left() * mPixmap.width(),  m.source.top() * mPixmap.height(),
                   m.source.width() * mPixmap.width(), m.source.height() * mPixmap.height());
        painter->drawPixmap(m.target, mPixmap, src);
        painter->restore();
    } else {
        painter->fillRect(frame, QColor(128, 128, 128, 70));
        QFont font = painter->font();
        font.setPixelSize(qMax(8, qRound(14.0 / lod)));
        painter->setFont(font);
        painter->setPen(QColor(200, 200, 200));
        painter->drawText(frame.adjusted(4, 4, -4, -4), Qt::AlignCenter | Qt::TextWordWrap,
                          mFailed ? tr("Failed to load\n%1").arg(displayName()) : tr("Loading..."));
    }

    // animated but not running: small play badge so it is clear that the tile can be started
    if(mAnimated && !mPixmap.isNull() && !isPlaying()) {
        qreal size = qBound<qreal>(8.0, 22.0 / lod, qMin(frame.width(), frame.height()) / 2.0);
        QRectF badge(frame.left() + size * 0.4, frame.bottom() - size * 1.4, size, size);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 150));
        painter->drawEllipse(badge);
        QPolygonF triangle;
        triangle << QPointF(badge.left() + size * 0.38, badge.top() + size * 0.28)
                 << QPointF(badge.left() + size * 0.38, badge.bottom() - size * 0.28)
                 << QPointF(badge.right() - size * 0.26, badge.center().y());
        painter->setBrush(Qt::white);
        painter->drawPolygon(triangle);
    }

    if(isSelected()) {
        QColor accent = settings->colorScheme().accent;
        QPen pen(accent, 2);
        pen.setCosmetic(true);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(frame);

        QPen handlePen(accent, 1);
        handlePen.setCosmetic(true);
        painter->setPen(handlePen);
        painter->setBrush(Qt::white);
        if(handlesActive()) {
            for(int h = H_TL; h <= H_L; h++)
                painter->drawRect(handleRect(static_cast<Handle>(h)));
        }
        if(mCropMode) {
            // marker so it is obvious that dragging pans the picture instead of moving the frame
            painter->setBrush(QColor(accent.red(), accent.green(), accent.blue(), 60));
            painter->setPen(Qt::NoPen);
            painter->drawRect(frame);
        }
    }
}

//------------------------------------------------------------------------------
QVariant CollageItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    if(change == ItemPositionChange && scene()) {
        CollageScene *cs = qobject_cast<CollageScene*>(scene());
        if(cs) {
            QPointF p = value.toPointF();
            if(mSnapping && cs->selectedItems().size() == 1 && !(QGuiApplication::keyboardModifiers() & Qt::ControlModifier))
                p = cs->snapPosition(this, p);
            // Freehand view: never lose a tile outside of the window
            if(cs->isFreeView())
                p = cs->clampToView(this, p);
            return p;
        }
    }
    if(change == ItemSelectedHasChanged)
        prepareGeometryChange(); // shape() depends on the selection
    return QGraphicsObject::itemChange(change, value);
}

QCursor CollageItem::cursorFor(Handle handle, Qt::KeyboardModifiers modifiers) const {
    if((modifiers & Qt::AltModifier) || mCropMode)
        return QCursor(Qt::OpenHandCursor);
    if(mViewMode && !mFree) {
        if(modifiers.testFlag(Qt::ControlModifier))
            return QCursor(Qt::SizeAllCursor); // swap
        return QCursor(mFit == CollageFit::Fill ? Qt::OpenHandCursor : Qt::SizeAllCursor);
    }
    switch(handle) {
        case H_TL:
        case H_BR: return QCursor(Qt::SizeFDiagCursor);
        case H_TR:
        case H_BL: return QCursor(Qt::SizeBDiagCursor);
        case H_T:
        case H_B:  return QCursor(Qt::SizeVerCursor);
        case H_L:
        case H_R:  return QCursor(Qt::SizeHorCursor);
        default:   return QCursor(Qt::SizeAllCursor);
    }
}

void CollageItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    setCursor(cursorFor(handleAt(event->pos()), event->modifiers()));
    QGraphicsObject::hoverMoveEvent(event);
}

void CollageItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}

void CollageItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if(mViewMode && mFree) {
        // Freehand view: right click opens the tile menu, a left press lifts the tile to the front
        // and continues as a normal editor drag (move / resize handles / Alt+drag crop)
        if(event->button() == Qt::RightButton) {
            if(!isSelected() && scene()) {
                scene()->clearSelection();
                setSelected(true);
            }
            event->accept();
            emit contextRequested();
            return;
        }
        if(event->button() == Qt::LeftButton) {
            if(CollageScene *cs = qobject_cast<CollageScene*>(scene()))
                cs->bringToFront(this);
        }
    } else if(mViewMode) {
        if(!isSelected() && scene()) {
            scene()->clearSelection();
            setSelected(true);
        }
        if(event->button() == Qt::RightButton) {
            event->accept();
            emit contextRequested();
            return;
        }
        if(event->button() != Qt::LeftButton) {
            event->ignore();
            return;
        }
        mPressScenePos = event->scenePos();
        mStartPos = pos();
        mStartCenter = mCenter;
        mSwapActive = false;
        // plain drag frames the picture inside its tile, Ctrl+drag moves the whole tile (swap).
        // Contain / Stretch show the whole image, so there is nothing to pan: dragging swaps.
        bool swap = event->modifiers().testFlag(Qt::ControlModifier);
        mDrag = (!swap && mFit == CollageFit::Fill) ? DRAG_PAN : DRAG_SWAP;
        if(mDrag == DRAG_PAN)
            setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if(event->button() != Qt::LeftButton) {
        QGraphicsObject::mousePressEvent(event);
        return;
    }
    mDragHandle = handleAt(event->pos());
    mPressScenePos = event->scenePos();
    mStartPos = pos();
    mStartSize = mFrame;
    mStartCenter = mCenter;

    bool wantsPan = (event->modifiers() & Qt::AltModifier) || mCropMode;
    // cropping needs the picture to overflow the frame: Contain / Stretch switch to Fill first
    if(wantsPan && mFit != CollageFit::Fill)
        setFit(CollageFit::Fill);
    bool wantsResize = (mDragHandle != H_NONE && mDragHandle != H_BODY);
    if((wantsPan && mFit == CollageFit::Fill) || wantsResize) {
        if(!isSelected()) {
            if(scene())
                scene()->clearSelection();
            setSelected(true);
        }
        mDrag = (wantsPan && mFit == CollageFit::Fill && !wantsResize) ? DRAG_PAN : DRAG_RESIZE;
        event->accept();
        return;
    }
    mDrag = DRAG_MOVE;
    mSnapping = true;
    QGraphicsObject::mousePressEvent(event);
}

void CollageItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    switch(mDrag) {
        case DRAG_RESIZE:
            resizeTo(event->scenePos(), event->modifiers());
            break;
        case DRAG_PAN:
            panTo(event->scenePos());
            break;
        case DRAG_SWAP: {
            QPointF delta = event->scenePos() - mPressScenePos;
            if(!mSwapActive) {
                CollageScene *cs = qobject_cast<CollageScene*>(scene());
                qreal threshold = QApplication::startDragDistance() / (cs ? cs->viewScale() : 1.0);
                if(QLineF(QPointF(0, 0), delta).length() < threshold)
                    break;
                // lift the tile so it floats above the others while it is dragged
                mSwapActive = true;
                mSavedOpacity = opacity();
                mSavedZ = zValue();
                setOpacity(0.75);
                setZValue(1e6);
                setCursor(Qt::ClosedHandCursor);
            }
            setPos(mStartPos + delta);
            break;
        }
        case DRAG_MOVE:
            QGraphicsObject::mouseMoveEvent(event);
            emit edited();
            break;
        default:
            QGraphicsObject::mouseMoveEvent(event);
            break;
    }
}

void CollageItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    DragMode mode = mDrag;
    mDrag = DRAG_NONE;
    mSnapping = false;
    if(mode == DRAG_RESIZE || mode == DRAG_PAN) {
        event->accept();
        if(mViewMode)
            setCursor(cursorFor(H_BODY, event->modifiers()));
        emit edited();
        return;
    }
    if(mode == DRAG_SWAP) {
        event->accept();
        if(mSwapActive) {
            mSwapActive = false;
            setOpacity(mSavedOpacity);
            setZValue(mSavedZ);
            unsetCursor();
            emit swapRequested(this, event->scenePos());
        }
        return;
    }
    QGraphicsObject::mouseReleaseEvent(event);
    emit edited();
}

void CollageItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if(mViewMode) {
        event->accept();
        if(event->button() == Qt::LeftButton)
            emit activated();
        return;
    }
    QGraphicsObject::mouseDoubleClickEvent(event);
}

void CollageItem::resizeTo(const QPointF &scenePos, Qt::KeyboardModifiers modifiers) {
    QRectF r0(mStartPos, mStartSize);
    QPointF d = scenePos - mPressScenePos;
    qreal l = r0.left(), t = r0.top(), r = r0.right(), b = r0.bottom();

    bool L = (mDragHandle == H_TL || mDragHandle == H_L || mDragHandle == H_BL);
    bool R = (mDragHandle == H_TR || mDragHandle == H_R || mDragHandle == H_BR);
    bool T = (mDragHandle == H_TL || mDragHandle == H_T || mDragHandle == H_TR);
    bool B = (mDragHandle == H_BL || mDragHandle == H_B || mDragHandle == H_BR);

    CollageScene *cs = qobject_cast<CollageScene*>(scene());
    bool snap = cs && !(modifiers & Qt::ControlModifier);
    if(L) { l += d.x(); if(snap) l = cs->snapValue(l, true); }
    if(R) { r += d.x(); if(snap) r = cs->snapValue(r, true); }
    if(T) { t += d.y(); if(snap) t = cs->snapValue(t, false); }
    if(B) { b += d.y(); if(snap) b = cs->snapValue(b, false); }

    if(r - l < MIN_FRAME_SIZE) {
        if(L) l = r - MIN_FRAME_SIZE;
        else  r = l + MIN_FRAME_SIZE;
    }
    if(b - t < MIN_FRAME_SIZE) {
        if(T) t = b - MIN_FRAME_SIZE;
        else  b = t + MIN_FRAME_SIZE;
    }

    // Shift on a corner: keep the frame proportions, anchored at the opposite corner
    if((L || R) && (T || B) && (modifiers & Qt::ShiftModifier) && r0.width() > 0 && r0.height() > 0) {
        qreal s = qMax((r - l) / r0.width(), (b - t) / r0.height());
        s = qMax(s, MIN_FRAME_SIZE / qMin(r0.width(), r0.height()));
        qreal w = r0.width() * s, h = r0.height() * s;
        if(L) l = r - w; else r = l + w;
        if(T) t = b - h; else b = t + h;
    }

    setPos(l, t);
    setFrameSize(QSizeF(r - l, b - t));
    emit edited();
}

// drag inside the frame moves the picture under it (crop)
void CollageItem::panTo(const QPointF &scenePos) {
    CollageMapping m = mapping();
    if(m.scale <= 0 || !mOriginal.isValid())
        return;
    QPointF d = scenePos - mPressScenePos;
    setContentCenter(QPointF(mStartCenter.x() - d.x() / (m.scale * mOriginal.width()),
                             mStartCenter.y() - d.y() / (m.scale * mOriginal.height())));
    emit edited();
}
