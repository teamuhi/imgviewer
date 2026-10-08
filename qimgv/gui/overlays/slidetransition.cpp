#include "slidetransition.h"
#include <QPainter>
#include <QEvent>

SlideTransition::SlideTransition(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    mAnimation.setStartValue(0.0);
    mAnimation.setEndValue(1.0);
    mAnimation.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&mAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        mProgress = value.toReal();
        update();
    });
    connect(&mAnimation, &QVariantAnimation::finished, this, &SlideTransition::finish);
    if(parent)
        parent->installEventFilter(this);
    hide();
}

bool SlideTransition::isArmed() const {
    return !mSnapshot.isNull();
}

void SlideTransition::arm(const QPixmap &snapshot, int style, int direction) {
    finish(); // a slide that is still leaving jumps to its end
    if(style == TRANSITION_NONE || snapshot.isNull())
        return;
    mSnapshot = snapshot;
    mStyle = style;
    mDirection = direction < 0 ? -1 : 1;
    mProgress = 0.0;
    if(parentWidget())
        setGeometry(parentWidget()->rect());
    raise();
    show();
    update();
}

void SlideTransition::start(int durationMs) {
    if(!isArmed() || mAnimation.state() == QAbstractAnimation::Running)
        return;
    mAnimation.setDuration(qMax(1, durationMs));
    mAnimation.start();
}

void SlideTransition::finish() {
    mAnimation.stop();
    mSnapshot = QPixmap();
    mProgress = 0.0;
    hide();
}

bool SlideTransition::eventFilter(QObject *watched, QEvent *event) {
    // the snapshot was taken at the old size: a resize ends the transition
    if(watched == parentWidget() && event->type() == QEvent::Resize && isArmed())
        finish();
    return QWidget::eventFilter(watched, event);
}

void SlideTransition::paintEvent(QPaintEvent *) {
    if(mSnapshot.isNull())
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF area = rect();
    switch(mStyle) {
    case TRANSITION_FADE:
        painter.setOpacity(1.0 - mProgress);
        painter.drawPixmap(area.topLeft(), mSnapshot);
        break;
    case TRANSITION_SLIDE:
        // the old slide moves out, the new one is already underneath
        painter.drawPixmap(QPointF(-mDirection * mProgress * area.width(), 0), mSnapshot);
        break;
    case TRANSITION_ZOOM: {
        qreal scale = 1.0 + 0.08 * mProgress;
        painter.setOpacity(1.0 - mProgress);
        painter.translate(area.center());
        painter.scale(scale, scale);
        painter.translate(-area.center());
        painter.drawPixmap(area.topLeft(), mSnapshot);
        break;
    }
    default:
        break;
    }
}
