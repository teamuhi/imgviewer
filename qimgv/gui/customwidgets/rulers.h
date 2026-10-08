#pragma once

#include <QObject>
#include <QWidget>
#include <QPoint>
#include <QTransform>
#include <QVector>
#include <QString>
#include <QAbstractScrollArea>
#include <QPointer>
#include <QDialog>
#include <QSizeF>
#include <functional>

// Rulers + draggable guide lines over a scroll-area based canvas (image viewer, collage view).
//  - the owner supplies a doc -> viewport transform (image pixels / collage canvas pixels)
//  - the rulers live in viewport margins, so they never cover the content
//  - guides are kept in document pixels, so they follow zoom and pan; they are saved on release
struct RulerHost {
    QAbstractScrollArea *view = nullptr;
    // document pixels -> viewport pixels (scale + translate only). false = nothing to measure
    std::function<bool(QTransform&)> docToViewport;
    // wraps the (protected) QAbstractScrollArea::setViewportMargins(left, top, 0, 0)
    std::function<void(int left, int top)> setMargins;
    // size of the measured document in document pixels (for "percent" guides); empty = unknown
    std::function<QSizeF()> docSize;
};

struct RulerGuide {
    Qt::Orientation orientation; // Horizontal = a horizontal line (y = pos)
    double pos;                  // document pixels
};

class RulerController;

// top / left ruler and the corner square
class RulerBar : public QWidget {
    Q_OBJECT
public:
    enum Kind { Top, Left, Corner };
    RulerBar(RulerController *controller, Kind kind, QWidget *parent);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    RulerController *mCtl;
    Kind mKind;
    void drawBand(QPainter &p, bool horizontal, int len, qreal scale, qreal origin,
                  int unit, int b0, int b1) const;
};

// guide lines + the drag read-out; covers the viewport, transparent for the mouse
class GuideOverlay : public QWidget {
    Q_OBJECT
public:
    GuideOverlay(RulerController *controller, QWidget *parent);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    RulerController *mCtl;
};

class RulerController : public QObject {
    Q_OBJECT
public:
    // guidesKey: "image" / "collage" (own saved set of guides)
    RulerController(const RulerHost &host, const QString &guidesKey, QObject *parent = nullptr);

    // DPI of the shown image, 0 = unknown (the Settings fallback is used)
    void setImageDpi(qreal dpi);
    // hide without touching the setting (video, slideshow)
    void setSuppressed(bool suppressed);
    void clearGuides();
    bool isActive() const { return mActive; }
    int thickness() const { return mActive ? mThickness : 0; }

    // --- used by the bar / overlay widgets ---
    bool transform(QTransform &t) const;
    int unit() const { return mUnit; }
    int mixedUnit() const { return mMixedUnit; }
    qreal dpi() const;
    // viewport pixels per one unit (px = 1, cm = dpi / 2.54, in = dpi)
    qreal pixelsPerUnit(int unit) const;
    const QVector<RulerGuide>& guides() const { return mGuides; }
    QPoint cursorPos() const { return mCursorVp; }
    bool hasCursor() const { return mCursorVp.x() >= 0 && mCursorVp.y() >= 0; }
    bool isDragging() const { return mDrag.active; }
    int dragIndex() const { return mDrag.index; }
    QString formatDoc(double docPixels) const;
    QPoint toViewport(const QPoint &local, const QWidget *from) const;
    QRect viewportRect() const;

    void beginBarDrag(Qt::Orientation orientation);
    void updateDrag(const QPoint &viewportPos);
    void endDrag(const QPoint &viewportPos);
    void showMenu(const QPoint &globalPos);
    // "Ruler settings" window: add a guide at an exact position, snap step
    void showSettingsDialog(Qt::Orientation orientation);
    bool addGuide(Qt::Orientation orientation, double docPixels);
    void refresh();

signals:
    void marginChanged(); // the room reserved for the rulers changed (owners re-place their floating bars)

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void readSettings();

private:
    struct Drag {
        bool active = false;
        int index = -1; // -1: dragged out of a ruler, the guide is created once it enters the canvas
        Qt::Orientation orientation = Qt::Horizontal;
    };
    RulerHost mHost;
    QString mKey;
    RulerBar *mTop, *mLeft, *mCorner;
    GuideOverlay *mOverlay;
    QVector<RulerGuide> mGuides;
    Drag mDrag;
    QPoint mCursorVp = QPoint(-1, -1);
    QTransform mLastTransform;
    bool mLastValid = false;

    bool mEnabled = false, mSuppressed = false, mActive = false;
    int mUnit = 0, mMixedUnit = 1, mFallbackDpi = 96, mThickness = 18, mAppliedMargin = 0;
    bool mUseImageDpi = true;
    bool mSnap = false;
    int mSnapStep = 10;
    qreal mImageDpi = 0.0;
    QPointer<QDialog> mDialog;

    bool mHoverCursorSet = false, mHadCursor = false;
    QCursor mSavedCursor;

    void layoutChildren();
    void loadGuides();
    void saveGuides();
    int guideAt(const QPoint &viewportPos) const;
    void setHoverCursor(Qt::Orientation orientation);
    void clearHoverCursor();
    void startDrag(int index, Qt::Orientation orientation);
    void setCursorMarker(const QPoint &pos);
};
