#pragma once

#include <QGraphicsObject>
#include <QPixmap>
#include <QPointF>
#include <QSize>
#include <QSizeF>
#include <QString>

class QMovie;

enum class CollageFit {
    Fill = 0,    // cover the frame, overflow is cropped (zoom / pan adjust the crop)
    Contain = 1, // whole image inside the frame
    Stretch = 2  // whole image, distorted to the frame
};

// Per-image resolution cap (longest side). Auto = longest side of the canvas.
enum class CollageRes {
    Auto = 0,
    Original = 1,
    Res4K = 2,
    Res2K = 3,
    Res1080 = 4,
    Res720 = 5
};

struct CollageMapping {
    QRectF source;  // normalized (0..1) part of the image that is visible
    QRectF target;  // where it lands, in frame coordinates
    qreal scale = 1.0; // frame px per original image px
};

// One image on the collage canvas. The item's position is the top-left corner of its
// frame; the frame size is independent of the image size (see CollageFit).
class CollageItem : public QGraphicsObject {
    Q_OBJECT
public:
    enum Handle { H_NONE, H_TL, H_T, H_TR, H_R, H_BR, H_B, H_BL, H_L, H_BODY };

    // geometry + crop of one mode (the editor and the view keep separate arrangements)
    struct State {
        QPointF pos;
        QSizeF frame = QSizeF(200, 200);
        CollageFit fit = CollageFit::Fill;
        qreal zoom = 1.0;
        QPointF center = QPointF(0.5, 0.5);
        qreal z = 0.0; // stacking order
    };

    CollageItem(const QString &path, quint64 id, const QSize &originalSize);

    State saveState() const;
    void restoreState(const State &state);

    // view mode: tiles can not be resized by hand, dragging one swaps it with another
    void setViewMode(bool mode);
    bool isViewMode() const;
    // view mode, Freehand layout: the tile behaves like an editor frame (move, resize handles, crop drag)
    void setFreePlacement(bool free);
    bool isFreePlacement() const;
    // layout hints used by the view mosaic
    qreal viewAspect() const;        // 0 = use the image aspect
    void setViewAspect(qreal aspect);
    qreal viewWeight() const;        // relative tile size, 1.0 = normal
    void setViewWeight(qreal weight);
    qreal layoutAspect() const;

    quint64 id() const;
    QString path() const;
    QString displayName() const;
    QSize originalSize() const;
    qreal aspect() const;

    QSizeF frameSize() const;
    void setFrameSize(const QSizeF &size);
    QRectF frameRect() const;

    CollageFit fit() const;
    void setFit(CollageFit fit);
    qreal contentZoom() const;
    void setContentZoom(qreal zoom);
    QPointF contentCenter() const;
    void setContentCenter(const QPointF &center);
    void resetCrop();
    qreal cornerRadius() const;
    void setCornerRadius(qreal radius);
    CollageRes resolution() const;
    void setResolution(CollageRes res);
    bool cropMode() const;
    void setCropMode(bool mode);

    // async loading bookkeeping
    int generation() const;
    int nextGeneration();
    void setWorkingPixmap(const QPixmap &pixmap);
    void setLoadFailed();
    bool isLoaded() const;
    bool hasFailed() const;
    QSize workingSize() const;
    qint64 memoryBytes() const;

    // animation (gif / animated webp / apng). The still pixmap is the first frame; once the
    // item is flagged animated a QMovie plays at the working size of the pixmap.
    bool isAnimated() const;
    void setAnimated(bool animated);
    bool isPlaying() const;
    bool animationEnabled() const;        // per tile "play" switch
    void setAnimationEnabled(bool enabled);
    bool animationLoop() const;           // false: play once, stop on the last frame
    void setAnimationLoop(bool loop);
    int animationSpeed() const;           // percent, 100 = as authored
    void setAnimationSpeed(int percent);
    void setAnimationsAllowed(bool allowed); // scene-wide switch (user setting / widget visibility)

    // handle size in scene units (depends on the view zoom)
    void setHandleSize(qreal size);

    static CollageMapping computeMapping(const QSize &image, const QSizeF &frame, CollageFit fit,
                                         qreal zoom, const QPointF &center);
    CollageMapping mapping() const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    // emitted while the user moves / resizes / pans the item
    void edited();
    // view mode: tile was dropped at scenePos (finding the swap target is up to the scene)
    void swapRequested(CollageItem *item, const QPointF &scenePos);
    void activated();        // double-click in view mode
    void contextRequested(); // right click in view mode

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    enum DragMode { DRAG_NONE, DRAG_MOVE, DRAG_RESIZE, DRAG_PAN, DRAG_SWAP };

    quint64 mId;
    QString mPath;
    QSize mOriginal;
    QSizeF mFrame;
    CollageFit mFit = CollageFit::Fill;
    qreal mZoom = 1.0;
    QPointF mCenter = QPointF(0.5, 0.5);
    qreal mRadius = 0.0;
    CollageRes mRes = CollageRes::Auto;
    bool mCropMode = false;
    bool mViewMode = false;
    bool mFree = false;
    qreal mViewAspect = 0.0;
    qreal mViewWeight = 1.0;

    QPixmap mPixmap;
    QMovie *mMovie = nullptr;
    bool mAnimated = false;
    bool mAnimEnabled = true;
    bool mAnimLoop = true;
    bool mAnimAllowed = true;
    bool mAnimFinished = false;
    int mAnimSpeed = 100;
    int mLastFrame = -1;
    int mGeneration = 0;
    bool mFailed = false;
    qreal mHandle = 10.0;

    DragMode mDrag = DRAG_NONE;
    Handle mDragHandle = H_NONE;
    bool mSnapping = false;
    QPointF mPressScenePos, mStartPos, mStartCenter;
    QSizeF mStartSize;
    bool mSwapActive = false;
    qreal mSavedOpacity = 1.0, mSavedZ = 0.0;

    // frames can be moved / resized by hand (editor, or the Freehand view layout)
    bool editLike() const { return !mViewMode || mFree; }
    QRectF handleRect(Handle handle) const;
    Handle handleAt(const QPointF &local) const;
    void resizeTo(const QPointF &scenePos, Qt::KeyboardModifiers modifiers);
    void panTo(const QPointF &scenePos);
    QCursor cursorFor(Handle handle, Qt::KeyboardModifiers modifiers) const;
    void createMovie();
    void dropMovie();
    void updatePlayback();
    void onMovieFrame(int frame);
};
