#include "slidetransition.h"
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QEvent>
#include <QRandomGenerator>
#include <QtMath>

namespace {
const int MAX_TILES = 2000;
const int MOTION_BLUR_MIN_STEPS = 3, MOTION_BLUR_MAX_STEPS = 12;
const qreal MOTION_BLUR_STEP_PX = 12.0; // spacing between the copies
const qint64 MAX_MIP_PIXELS = 8192LL * 4608LL; // bigger frames skip the blur / pixelate styles

inline qreal clamp01(qreal v) {
    return qBound(qreal(0), v, qreal(1));
}
}

SlideTransition::SlideTransition(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    mAnimation.setStartValue(0.0);
    mAnimation.setEndValue(1.0);
    mAnimation.setEasingCurve(QEasingCurve::Linear); // the user curve is applied in valueChanged
    connect(&mAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        mProgress = mParams.curve.valueForProgress(value.toReal());
        update();
    });
    connect(&mAnimation, &QVariantAnimation::finished, this, &SlideTransition::finish);
    if(parent)
        parent->installEventFilter(this);
    hide();
}

QStringList SlideTransition::styleNames() {
    return { tr("No transition"), tr("Fade"), tr("Slide"), tr("Zoom"), tr("Dip to color"), tr("Blur fade"),
             tr("Motion blur"), tr("Pixel mash"), tr("Push"), tr("Cover"), tr("Wipe"),
             tr("Iris"), tr("Dissolve") };
}

bool SlideTransition::isArmed() const {
    return !mOld.isNull();
}

bool SlideTransition::needsNewFrame() const {
    if(!isArmed())
        return false;
    switch(mParams.style) {
    case TRANSITION_BLUR_FADE:
    case TRANSITION_MOTION_BLUR:
    case TRANSITION_PIXEL_MASH:
    case TRANSITION_PUSH:
    case TRANSITION_COVER:
        return true;
    default:
        return false;
    }
}

bool SlideTransition::usesMips() const {
    return mParams.style == TRANSITION_BLUR_FADE || mParams.style == TRANSITION_PIXEL_MASH;
}

qreal SlideTransition::extent() const {
    return qAbs(mDir.x()) * width() + qAbs(mDir.y()) * height();
}

void SlideTransition::arm(const QPixmap &snapshot, const SlideTransitionParams &params, int navDirection) {
    finish(); // a slide that is still leaving jumps to its end
    if(params.style <= TRANSITION_NONE || params.style >= TRANSITION_COUNT || snapshot.isNull())
        return;
    mOld = snapshot;
    mParams = params;
    const qreal nav = navDirection < 0 ? -1 : 1;
    switch(params.directionMode) {
    case TRANSITION_DIR_LEFT:  mDir = QPointF(-1, 0); break;
    case TRANSITION_DIR_RIGHT: mDir = QPointF(1, 0);  break;
    case TRANSITION_DIR_UP:    mDir = QPointF(0, -1); break;
    case TRANSITION_DIR_DOWN:  mDir = QPointF(0, 1);  break;
    default:                   mDir = QPointF(-nav, 0); break; // next: the old slide leaves to the left
    }
    mProgress = 0.0;
    if(parentWidget())
        setGeometry(parentWidget()->rect());
    raise();
    show();
    update();
}

void SlideTransition::start(const QPixmap &newFrame) {
    if(!isArmed() || mAnimation.state() == QAbstractAnimation::Running)
        return;
    // only a frame of the same size can replace the live viewer
    if(!newFrame.isNull() && newFrame.size() == mOld.size() && qFuzzyCompare(newFrame.devicePixelRatio(), mOld.devicePixelRatio()))
        mNew = newFrame;
    else
        mNew = QPixmap();
    if(usesMips()) {
        if(qint64(mOld.width()) * mOld.height() > MAX_MIP_PIXELS) {
            mParams.style = TRANSITION_FADE; // too big to filter smoothly
        } else {
            buildMips(mOld, mOldMips);
            if(!mNew.isNull())
                buildMips(mNew, mNewMips);
        }
    }
    if(mParams.style == TRANSITION_DISSOLVE)
        prepareTiles();
    mAnimation.setDuration(qMax(1, mParams.durationMs));
    mAnimation.start();
}

// grid for dissolve, each tile gets a random moment to fade. The tile count is capped so tiny
// blocks stay cheap on big screens
void SlideTransition::prepareTiles() {
    const qreal minSize = qSqrt(qreal(width()) * height() / MAX_TILES);
    mTileSize = qMax(qMax(mParams.blockSize, int(qCeil(minSize))), 8);
    mTileCols = (width() + mTileSize - 1) / mTileSize;
    mTileRows = (height() + mTileSize - 1) / mTileSize;
    mTileRand.resize(mTileCols * mTileRows);
    for(float &value : mTileRand)
        value = float(QRandomGenerator::global()->generateDouble());
}

void SlideTransition::finish() {
    mAnimation.stop();
    mOld = QPixmap();
    mNew = QPixmap();
    mOldMips.clear();
    mNewMips.clear();
    mTileRand.clear();
    mTileCols = mTileRows = mTileSize = 0;
    mProgress = 0.0;
    hide();
}

bool SlideTransition::eventFilter(QObject *watched, QEvent *event) {
    // the snapshot was taken at the old size: a resize ends the transition
    if(watched == parentWidget() && event->type() == QEvent::Resize && isArmed())
        finish();
    return QWidget::eventFilter(watched, event);
}

void SlideTransition::buildMips(const QPixmap &frame, QVector<QPixmap> &mips) const {
    mips.clear();
    QPixmap previous = frame;
    for(int i = 0; i < 5; i++) {
        if(previous.width() <= 16 || previous.height() <= 16)
            break;
        previous = previous.scaled(previous.width() / 2, previous.height() / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        mips.append(previous);
    }
}


//------------------------------------------------------------------------------
// drawing helpers
void SlideTransition::blit(QPainter &painter, const QPixmap &pixmap, const QRectF &target, qreal opacity) const {
    painter.setOpacity(opacity);
    // source in device pixels: works for any pixel ratio and for the downscaled mips
    painter.drawPixmap(target, pixmap, QRectF(pixmap.rect()));
}

// level 0 = sharp, level n = the 1/2^n downscale stretched back up; fractions cross-fade two levels
void SlideTransition::drawBlurred(QPainter &painter, const QPixmap &full, const QVector<QPixmap> &mips, qreal level, qreal opacity) const {
    level = qBound(qreal(0), level, qreal(mips.size()));
    const int low = int(level);
    const qreal fraction = level - low;
    const QRectF area = rect();
    blit(painter, low == 0 ? full : mips[low - 1], area, opacity);
    if(fraction > 0.01 && low < mips.size())
        blit(painter, mips[low], area, opacity * fraction);
}

// block: size of one "pixel" in widget px. The (small) source is scaled down to the block grid and
// stretched back without smoothing
void SlideTransition::drawPixelated(QPainter &painter, const QPixmap &full, const QVector<QPixmap> &mips, qreal block, qreal opacity) const {
    const QRectF area = rect();
    if(block <= 1.5) {
        blit(painter, full, area, opacity);
        return;
    }
    const QSize target(qMax(1, int(area.width() / block)), qMax(1, int(area.height() / block)));
    const QPixmap *source = &full;
    for(const QPixmap &mip : mips) { // the smallest one that is still not below the grid
        if(mip.width() >= target.width() && mip.height() >= target.height())
            source = &mip;
        else
            break;
    }
    const QPixmap grid = source->scaled(target, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    blit(painter, grid, area, opacity);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
}

// copies spread over the path behind the moving frame: with 1/n opacity each they average out evenly
void SlideTransition::drawStreaked(QPainter &painter, const QPixmap &pixmap, const QPointF &offset, const QPointF &trail, qreal opacity) const {
    const int steps = qBound(MOTION_BLUR_MIN_STEPS, int(qCeil(QLineF(QPointF(), trail).length() / MOTION_BLUR_STEP_PX)), MOTION_BLUR_MAX_STEPS);
    for(int i = 0; i < steps; i++) {
        const qreal t = qreal(i) / (steps - 1);
        blit(painter, pixmap, QRectF(rect()).translated(offset - trail * t), opacity / (i + 1));
    }
}

void SlideTransition::drawMasked(QPainter &painter, const QRectF &region, const QBrush &mask) const {
    if(region.width() < 1 || region.height() < 1)
        return;
    const qreal ratio = devicePixelRatioF();
    QImage buffer(qCeil(region.width() * ratio), qCeil(region.height() * ratio), QImage::Format_ARGB32_Premultiplied);
    if(buffer.isNull())
        return;
    buffer.setDevicePixelRatio(ratio);
    buffer.fill(Qt::transparent);
    QPainter bufferPainter(&buffer);
    bufferPainter.setRenderHint(QPainter::SmoothPixmapTransform);
    bufferPainter.translate(-region.topLeft());
    blit(bufferPainter, mOld, rect());
    bufferPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    bufferPainter.fillRect(region, mask);
    bufferPainter.end();
    painter.setOpacity(1.0);
    painter.drawImage(region.topLeft(), buffer);
}

//------------------------------------------------------------------------------
void SlideTransition::paintEvent(QPaintEvent *) {
    if(mOld.isNull())
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal p = mProgress;
    switch(mParams.style) {
    case TRANSITION_FADE:         paintFade(painter, p); break;
    case TRANSITION_SLIDE:        paintSlide(painter, p); break;
    case TRANSITION_ZOOM:         paintZoom(painter, p); break;
    case TRANSITION_DIP_COLOR:    paintDip(painter, p); break;
    case TRANSITION_BLUR_FADE:    paintBlurFade(painter, p); break;
    case TRANSITION_MOTION_BLUR:  paintMotionBlur(painter, p); break;
    case TRANSITION_PIXEL_MASH:   paintPixelMash(painter, p); break;
    case TRANSITION_DISSOLVE:     paintDissolve(painter, p); break;
    case TRANSITION_PUSH:         paintPush(painter, p); break;
    case TRANSITION_COVER:        paintCover(painter, p); break;
    case TRANSITION_WIPE:         paintWipe(painter, p); break;
    case TRANSITION_IRIS:         paintIris(painter, p); break;
    default: break;
    }
}

// The new frame (when there is one) goes underneath, the old one is then taken apart on top of it.
// p is the eased progress: opacities and filter levels clamp it, movements use it as it is,
// so a curve that overshoots (back / elastic) visibly overshoots.
void SlideTransition::paintFade(QPainter &painter, qreal p) {
    if(!mNew.isNull())
        blit(painter, mNew, rect());
    blit(painter, mOld, rect(), 1.0 - clamp01(p));
}

// strength: how far the old slide travels (the default 50 % = the full width, less fades the rest)
void SlideTransition::paintSlide(QPainter &painter, qreal p) {
    if(!mNew.isNull())
        blit(painter, mNew, rect());
    const qreal travel = qMin(qreal(1), 0.25 + 1.5 * mParams.strength);
    const qreal opacity = travel >= 1.0 ? 1.0 : 1.0 - clamp01(p);
    blit(painter, mOld, QRectF(rect()).translated(mDir * (p * travel * extent())), opacity);
}

// strength: zoom amount of the leaving slide (up to +30 %)
void SlideTransition::paintZoom(QPainter &painter, qreal p) {
    if(!mNew.isNull())
        blit(painter, mNew, rect());
    const QRectF area = rect();
    const qreal scale = 1.0 + 0.3 * mParams.strength * clamp01(p);
    painter.save();
    painter.translate(area.center());
    painter.scale(scale, scale);
    painter.translate(-area.center());
    blit(painter, mOld, area, 1.0 - clamp01(p));
    painter.restore();
}

// old -> color -> new. strength: how long the color is held in the middle (up to 30 % of the time)
void SlideTransition::paintDip(QPainter &painter, qreal p) {
    const qreal cp = clamp01(p);
    const qreal hold = 0.3 * mParams.strength;
    const qreal fadeOutEnd = 0.5 - hold / 2, fadeInStart = 0.5 + hold / 2;
    qreal amount;
    if(cp < fadeOutEnd)
        amount = fadeOutEnd > 0 ? cp / fadeOutEnd : 1.0;
    else if(cp <= fadeInStart)
        amount = 1.0;
    else
        amount = (1.0 - cp) / (1.0 - fadeInStart);
    if(cp < 0.5)
        blit(painter, mOld, rect());
    else if(!mNew.isNull())
        blit(painter, mNew, rect()); // (otherwise the live viewer is below)
    QColor color = mParams.dipColor;
    color.setAlpha(255);
    painter.setOpacity(clamp01(amount));
    painter.fillRect(rect(), color);
}

// strength: how blurry it gets
void SlideTransition::paintBlurFade(QPainter &painter, qreal p) {
    const qreal cp = clamp01(p);
    const qreal maxLevel = 1.0 + 4.0 * mParams.strength;
    if(!mNew.isNull())
        drawBlurred(painter, mNew, mNewMips, maxLevel * (1.0 - cp), 1.0);
    drawBlurred(painter, mOld, mOldMips, maxLevel * cp, 1.0 - cp);
}

// both slides move along the direction (like push), smeared over a trail. strength: trail length
void SlideTransition::paintMotionBlur(QPainter &painter, qreal p) {
    const qreal cp = clamp01(p);
    const qreal length = extent();
    const QPointF trail = mDir * ((0.05 + 0.45 * mParams.strength) * length * qSin(M_PI * cp));
    if(!mNew.isNull())
        drawStreaked(painter, mNew, mDir * ((p - 1.0) * length), trail, 1.0);
    drawStreaked(painter, mOld, mDir * (p * length), trail, 1.0);
}

// both slides turn into big pixels, the old one is swapped for the new one at the coarsest point.
// strength: biggest pixel (8 .. 96 px)
void SlideTransition::paintPixelMash(QPainter &painter, qreal p) {
    const qreal cp = clamp01(p);
    const qreal maxBlock = 8.0 + 88.0 * mParams.strength;
    const qreal block = 1.0 + (maxBlock - 1.0) * qSin(M_PI * cp);
    if(mNew.isNull()) {
        drawPixelated(painter, mOld, mOldMips, block, 1.0 - cp);
        return;
    }
    drawPixelated(painter, mNew, mNewMips, block, 1.0);
    drawPixelated(painter, mOld, mOldMips, block, 1.0 - clamp01((cp - 0.4) / 0.2));
}

// tiles of the old slide fade out in a random order (strength: how long each one takes)
void SlideTransition::paintDissolve(QPainter &painter, qreal p) {
    const qreal cp = clamp01(p);
    const QRectF area = rect();
    if(!mNew.isNull())
        blit(painter, mNew, area);
    if(mTileSize <= 0)
        return;
    const qreal scaleX = mOld.width() / area.width(), scaleY = mOld.height() / area.height();
    const qreal window = 0.15 + 0.5 * mParams.strength;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for(int row = 0; row < mTileRows; row++) {
        for(int col = 0; col < mTileCols; col++) {
            const QRectF tile = QRectF(col * mTileSize, row * mTileSize, mTileSize, mTileSize).intersected(area);
            const qreal gone = clamp01((cp - mTileRand[row * mTileCols + col] * (1.0 - window)) / window);
            if(gone >= 1.0)
                continue;
            painter.setOpacity(1.0 - gone);
            painter.drawPixmap(tile, mOld,
                               QRectF(tile.x() * scaleX, tile.y() * scaleY, tile.width() * scaleX, tile.height() * scaleY));
        }
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
}

void SlideTransition::paintPush(QPainter &painter, qreal p) {
    const qreal length = extent();
    if(!mNew.isNull())
        blit(painter, mNew, QRectF(rect()).translated(mDir * ((p - 1.0) * length)));
    blit(painter, mOld, QRectF(rect()).translated(mDir * (p * length)));
}

// the new slide slides in over the old one, which dims (strength: how much)
void SlideTransition::paintCover(QPainter &painter, qreal p) {
    if(mNew.isNull()) { // nothing to slide in
        paintFade(painter, p);
        return;
    }
    const qreal cp = clamp01(p);
    blit(painter, mOld, rect());
    painter.setOpacity(1.2 * mParams.strength * cp);
    painter.fillRect(rect(), Qt::black);
    blit(painter, mNew, QRectF(rect()).translated(mDir * ((p - 1.0) * extent())));
}

// the old slide is wiped away by a feathered edge moving along the direction (softness: the feather)
void SlideTransition::paintWipe(QPainter &painter, qreal p) {
    const qreal cp = clamp01(p);
    const QRectF area = rect();
    const QPointF center = area.center();
    const qreal half = extent() / 2;
    const qreal feather = mParams.softness * extent() * 0.5;
    // u = position along the direction, from the centre. The old slide stays where u > edge + feather
    const qreal edge = (-half - feather) * (1.0 - cp) + half * cp;
    auto band = [&](qreal from, qreal to) { // from < to
        if(mDir.y() == 0) {
            const qreal a = center.x() + mDir.x() * from, b = center.x() + mDir.x() * to;
            return QRectF(QPointF(qMin(a, b), area.top()), QPointF(qMax(a, b), area.bottom())).intersected(area);
        }
        const qreal a = center.y() + mDir.y() * from, b = center.y() + mDir.y() * to;
        return QRectF(QPointF(area.left(), qMin(a, b)), QPointF(area.right(), qMax(a, b))).intersected(area);
    };
    if(!mNew.isNull())
        blit(painter, mNew, area);
    painter.save();
    painter.setClipRect(band(edge + feather, half + 1.0));
    blit(painter, mOld, area);
    painter.restore();
    if(feather >= 1.0) {
        QLinearGradient gradient(center + mDir * edge, center + mDir * (edge + feather));
        gradient.setColorAt(0.0, QColor(0, 0, 0, 0));
        gradient.setColorAt(1.0, QColor(0, 0, 0, 255));
        drawMasked(painter, band(edge, edge + feather), QBrush(gradient));
    }
}

// the old slide is eaten by a growing circle from the centre (softness: the feather)
void SlideTransition::paintIris(QPainter &painter, qreal p) {
    const qreal cp = clamp01(p);
    const QRectF area = rect();
    const QPointF center = area.center();
    const qreal reach = qSqrt(area.width() * area.width() + area.height() * area.height()) / 2;
    const qreal feather = mParams.softness * reach * 0.5;
    const qreal edge = (reach + feather) * cp - feather;   // radius where the old slide starts to come back
    const qreal outer = qMax(qreal(0), edge + feather);    // ... and where it is fully back
    if(!mNew.isNull())
        blit(painter, mNew, area);
    QPainterPath outside;
    outside.addRect(area);
    QPainterPath circle;
    circle.addEllipse(center, outer, outer);
    painter.save();
    painter.setClipPath(outside.subtracted(circle));
    blit(painter, mOld, area);
    painter.restore();
    if(feather >= 1.0 && outer >= 1.0) {
        QRadialGradient gradient(center, outer);
        gradient.setColorAt(0.0, QColor(0, 0, 0, 0));
        gradient.setColorAt(qBound(0.0, edge / outer, 0.999), QColor(0, 0, 0, 0));
        gradient.setColorAt(1.0, QColor(0, 0, 0, 255));
        drawMasked(painter, QRectF(center.x() - outer, center.y() - outer, 2 * outer, 2 * outer).intersected(area), QBrush(gradient));
    }
}
