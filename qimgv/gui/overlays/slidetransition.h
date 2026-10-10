#pragma once

#include <QWidget>
#include <QPixmap>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QColor>
#include <QPointF>
#include <QStringList>
#include <QVector>
#include "gui/overlays/slidetransitionstyle.h"

struct SlideTransitionParams {
    int style = TRANSITION_FADE;
    int durationMs = 400;
    QEasingCurve curve;                  // maps time -> progress, may overshoot
    qreal strength = 0.5;                // 0..1, main magnitude of the effect
    qreal softness = 0.3;                // 0..1, feathered edges (wipe / iris / blocks)
    int blockSize = 64;                  // px, dissolve tiles
    int directionMode = TRANSITION_DIR_AUTO;
    QColor dipColor = Qt::black;
};

// Draws the previous slide (and, when it could be grabbed, the new one) on top of the viewer and
// animates between them. arm() takes the old frame right before the switch (it also hides the
// half-loaded new slide), start() gets the new frame and runs the animation. Without a new frame
// every style only moves / fades the old one away, so the live viewer underneath shows through.
// Everything is freed as soon as the animation ends.
class SlideTransition : public QWidget {
    Q_OBJECT
public:
    explicit SlideTransition(QWidget *parent);
    static QStringList styleNames();
    // navDirection: +1 next, -1 previous (Auto direction: the slide leaves the other way)
    void arm(const QPixmap &snapshot, const SlideTransitionParams &params, int navDirection);
    void start(const QPixmap &newFrame = QPixmap());
    void finish(); // jump to the end state
    bool isArmed() const;
    // the styles that move / filter the new slide too need a grab of it (the others just uncover the live viewer)
    bool needsNewFrame() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPixmap mOld, mNew;
    QVector<QPixmap> mOldMips, mNewMips; // 1/2, 1/4 ... of the frame (blur / pixelate styles only)
    QVariantAnimation mAnimation;
    SlideTransitionParams mParams;
    QPointF mDir = QPointF(-1, 0);       // unit vector, where the old slide leaves to
    qreal mProgress = 0.0;               // eased
    int mTileCols = 0, mTileRows = 0, mTileSize = 0;
    QVector<float> mTileRand;            // dissolve: per tile moment to fade

    bool usesMips() const;
    qreal extent() const; // size of the viewer along the leaving direction
    void buildMips(const QPixmap &frame, QVector<QPixmap> &mips) const;
    void prepareTiles();

    void blit(QPainter &painter, const QPixmap &pixmap, const QRectF &target, qreal opacity = 1.0) const;
    void drawBlurred(QPainter &painter, const QPixmap &full, const QVector<QPixmap> &mips, qreal level, qreal opacity) const;
    void drawPixelated(QPainter &painter, const QPixmap &full, const QVector<QPixmap> &mips, qreal block, qreal opacity) const;
    void drawStreaked(QPainter &painter, const QPixmap &pixmap, const QPointF &offset, const QPointF &trail, qreal opacity) const;
    // the old frame inside region, its alpha multiplied by the (gradient) mask brush
    void drawMasked(QPainter &painter, const QRectF &region, const QBrush &mask) const;

    void paintFade(QPainter &painter, qreal p);
    void paintSlide(QPainter &painter, qreal p);
    void paintZoom(QPainter &painter, qreal p);
    void paintDip(QPainter &painter, qreal p);
    void paintBlurFade(QPainter &painter, qreal p);
    void paintMotionBlur(QPainter &painter, qreal p);
    void paintPixelMash(QPainter &painter, qreal p);
    void paintDissolve(QPainter &painter, qreal p);
    void paintPush(QPainter &painter, qreal p);
    void paintCover(QPainter &painter, qreal p);
    void paintWipe(QPainter &painter, qreal p);
    void paintIris(QPainter &painter, qreal p);
};
