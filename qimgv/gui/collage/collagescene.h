#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <QColor>
#include <QPixmap>
#include <functional>
#include "gui/collage/collageitem.h"
#include "gui/collage/collagelayout.h"
#include "gui/collage/collageimageloader.h"
#include "gui/collage/collageseparator.h"

enum class CollageMode {
    View, // tiles fill the viewer area and re-flow with it, dragging swaps tiles
    Edit  // fixed export canvas with free-floating frames
};

// The collage scene. In Edit mode scene units are canvas pixels: a frame that is 500 units
// wide ends up 500 px wide in the exported image. In View mode the scene is exactly the
// viewer area. Every image keeps a separate arrangement for each mode.
class CollageScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit CollageScene(QObject *parent = nullptr);

    CollageMode mode() const;
    void setMode(CollageMode mode);
    bool editorInitialized() const;

    // view mode
    void setViewArea(const QSizeF &size, qreal dpr);
    void setViewLayout(CollageLayout::Mode mode);
    CollageLayout::Mode viewLayout() const;
    // Freehand view layout: the user places / sizes the tiles, the scene only keeps them inside the window
    bool isFreeView() const { return mMode == CollageMode::View && mViewLayout == CollageLayout::MODE_FREEFORM; }
    QPointF clampToView(const CollageItem *item, const QPointF &pos) const;
    // Freehand is an endless canvas: the view area plus generous room on every side, grown further while panning
    QRectF freeWorldRect() const;
    // every layout can be panned freely (middle-drag): the scene rect grows with the visible part
    void growSceneRect(const QRectF &visible);
    void setViewGap(int gap);
    int viewGap() const;
    // view canvas shape: width / height of the frame the tiles are laid out in, 0 = fill the whole window.
    // Ignored by Freehand, which uses the whole window.
    void setViewShape(qreal aspect);
    qreal viewShape() const;
    void relayoutView();
    QList<CollageItem*> viewOrderItems() const;
    void swapTiles(quint64 a, quint64 b);
    void selectNeighbor(int delta);

    // Grid / Row / Column options + dragged borders, shared by the view and the editor's automatic layouts
    const CollageLayout::Options &layoutOptions() const;
    void setLayoutOptions(const CollageLayout::Options &options);
    // the automatic layout that is on screen, or MODE_FREEFORM when the tiles are placed by hand
    CollageLayout::Mode activeLayout() const;
    // drag handles of the borders follow the "Borders" setting (snap / free / locked)
    void refreshSeparators();
    // a tile's own size was set: its dragged borders would fight it, drop them
    void clearSplitsFor(CollageItem *item);
    // default outline of the tiles that have none of their own
    void setDefaultBorder(int width, const QColor &color, int style);
    // view background: theme colour + pattern, or the canvas colour
    void setThemeBackground(bool theme);
    // opacity (0-100 %) of the custom colour over the theme background + pattern
    void setBackgroundOpacity(int percent);

    // editor: frames follow an automatic layout instead of being placed by hand
    bool staticCanvas() const;
    void setStaticCanvas(bool enabled);
    void setStaticLayout(CollageLayout::Mode mode, int gap);

    QSize canvasSize() const;
    void setCanvasSize(const QSize &size);
    QRectF canvasRect() const;
    QColor canvasColor() const;
    void setCanvasColor(const QColor &color); // alpha 0 = transparent (png)

    // returns nullptr if the file can not be read as an image
    CollageItem *addImage(const QString &path, bool cascade);
    QList<CollageItem*> collageItems() const;         // bottom to top
    QList<CollageItem*> selectedCollageItems() const;
    int imageCount() const;
    void removeCollageItem(CollageItem *item);
    void removeSelected();
    void clearCollage();

    void applyLayout(CollageLayout::Mode mode, int gap);
    void bringToFront(CollageItem *item);
    void sendToBack(CollageItem *item);
    void fillCanvas(CollageItem *item);
    void reloadItem(CollageItem *item);

    qreal viewScale() const;
    void setViewScale(qreal scale);
    qreal handleSize() const;
    qreal snapThreshold() const;
    QPointF snapPosition(const CollageItem *item, const QPointF &pos) const;
    qreal snapValue(qreal value, bool horizontal) const;

    // animated images (gif / webp / apng)
    bool animationsEnabled() const;
    void setAnimationsEnabled(bool enabled); // user switch, persisted by the widget
    void setAnimationsActive(bool active);   // false while the collage is not on screen

    qint64 memoryBytes() const;
    int longSideFor(const CollageItem *item) const; // 0 = unlimited

    // progress(done, total) may return false to cancel
    bool exportImage(const QString &path, QString *error,
                     const std::function<bool(int, int)> &progress = nullptr);

signals:
    void collageChanged(); // items added / removed / loaded / re-laid out
    void itemEdited();     // user dragging, resizing or panning
    void itemActivated(CollageItem *item);        // view: tile double-clicked
    void itemContextRequested(CollageItem *item); // view: tile right-clicked

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

private slots:
    void onLoaded(quint64 id, int generation, QImage image, QSize originalSize, bool animated);
    void onSwapRequested(CollageItem *item, const QPointF &scenePos);
    void onSeparatorMoved(const QString &key, qreal fraction);
    void onSeparatorReset(const QString &key);
    void storeFreeRects(); // remembers where the user put the selected tiles (Freehand)

private:
    // Freehand tile placement, independent of the window size: centre is 0..1 of the view area per
    // axis, size is in units of the shorter view side (tiles keep their shape when the window changes)
    struct FreeRect {
        QPointF center = QPointF(0.5, 0.5);
        QSizeF size = QSizeF(0.3, 0.3);
    };

    CollageImageLoader mLoader;
    QHash<quint64, CollageItem*> mById;
    QSize mCanvasSize = QSize(1920, 1080);
    QColor mCanvasColor = QColor(Qt::white);
    QPixmap mChecker;
    qreal mViewScale = 1.0;
    quint64 mNextId = 0;
    int mTopZ = 0, mBottomZ = 0;

    CollageMode mMode = CollageMode::View;
    bool mEditorInitialized = false;
    QSizeF mViewArea = QSizeF(1280, 720);
    qreal mDpr = 1.0;
    QList<quint64> mViewOrder; // display order of the tiles in view mode
    QHash<quint64, CollageItem::State> mEditStates, mViewStates;
    CollageLayout::Mode mViewLayout = CollageLayout::MODE_MOSAIC;
    int mViewGap = 6;
    qreal mViewShape = 0.0;
    QPixmap mPatternTile;
    bool mAnimEnabled = true, mAnimActive = true;
    bool mStatic = false;
    CollageLayout::Mode mStaticLayout = CollageLayout::MODE_MOSAIC;
    int mStaticGap = 12;
    QHash<quint64, FreeRect> mFreeRects;
    int mFreeCascade = 0;
    CollageLayout::Options mLayoutOptions;
    QList<CollageLayout::Separator> mSeparators;   // of the last automatic layout
    QList<CollageSeparator*> mSeparatorItems;     // pooled: a dragged one must survive the re-flows it causes
    int mBorderWidth = 0;
    QColor mBorderColor;
    int mBorderStyle = 0;
    bool mThemeBackground = true;
    int mBackgroundOpacity = 100;

    void applyLayoutResult(const QList<CollageItem*> &list, CollageLayout::Mode mode, const CollageLayout::Result &result);
    void resetCellStyle();
    void syncSeparators();
    void clearSplits();

    void storeFreeRect(const CollageItem *item);
    void seedFreeRects();
    void placeNewFreeTile(CollageItem *item);
    void relayoutFree();
    void updateSceneRect();
    QRectF viewAreaRect() const;
    bool hasViewShape() const;
    void applyAnimationState();
    void relayoutStatic();
    void refreshPattern();
    void refreshAutoResolution();
    void forget(quint64 id);
    void drawItem(QPainter *painter, CollageItem *item) const;
};
