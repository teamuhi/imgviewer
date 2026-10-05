#include "colorareawidget.h"

ColorAreaWidget::ColorAreaWidget(Kind kind, QWidget *parent)
    : QWidget(parent),
      mKind(kind)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    if(mKind == KIND_HUE)
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    else
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ColorAreaWidget::setHue(int h) {
    h = qBound(0, h, 359);
    if(h == mHue)
        return;
    mHue = h;
    update();
}

void ColorAreaWidget::setSaturationValue(int s, int v) {
    s = qBound(0, s, 255);
    v = qBound(0, v, 255);
    if(s == mSat && v == mVal)
        return;
    mSat = s;
    mVal = v;
    update();
}

QSize ColorAreaWidget::sizeHint() const {
    return (mKind == KIND_HUE) ? QSize(24, 220) : QSize(220, 220);
}

QSize ColorAreaWidget::minimumSizeHint() const {
    return (mKind == KIND_HUE) ? QSize(24, 120) : QSize(120, 120);
}

// drawing area, leaves room for the focus ring / handle overshoot
QRect ColorAreaWidget::areaRect() const {
    return rect().adjusted(2, 2, -2, -2);
}

void ColorAreaWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRect area = areaRect();
    if(area.width() < 2 || area.height() < 2)
        return;

    if(mKind == KIND_SV) {
        // hue base -> white on the left -> black at the bottom
        p.fillRect(area, QColor::fromHsv(mHue, 255, 255));
        QLinearGradient white(area.topLeft(), area.topRight());
        white.setColorAt(0.0, Qt::white);
        white.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(area, white);
        QLinearGradient black(area.topLeft(), area.bottomLeft());
        black.setColorAt(0.0, QColor(0, 0, 0, 0));
        black.setColorAt(1.0, Qt::black);
        p.fillRect(area, black);
    } else {
        QLinearGradient grad(area.topLeft(), area.bottomLeft());
        for(int i = 0; i <= 6; i++)
            grad.setColorAt(i / 6.0, QColor::fromHsv(qMin(i * 60, 359), 255, 255));
        p.fillRect(area, grad);
    }

    // border (semi-transparent so it reads on any theme)
    p.setPen(QColor(128, 128, 128, 200));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(area).adjusted(-0.5, -0.5, 0.5, 0.5));

    // handle
    if(mKind == KIND_SV) {
        qreal x = area.left() + (mSat / 255.0) * (area.width() - 1);
        qreal y = area.top()  + ((255 - mVal) / 255.0) * (area.height() - 1);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::black, 2));
        p.drawEllipse(QPointF(x, y), 6.5, 6.5);
        p.setPen(QPen(Qt::white, 1.5));
        p.drawEllipse(QPointF(x, y), 5.5, 5.5);
    } else {
        qreal y = area.top() + (mHue / 359.0) * (area.height() - 1);
        QRectF marker(area.left() - 1, y - 3, area.width() + 2, 6);
        p.setPen(QPen(Qt::black, 2));
        p.drawRect(marker);
        p.setPen(QPen(Qt::white, 1));
        p.drawRect(marker);
    }

    if(hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(palette().color(QPalette::Highlight), 1));
        p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    }
}

void ColorAreaWidget::pickAt(const QPoint &pos) {
    QRect area = areaRect();
    if(area.width() < 2 || area.height() < 2)
        return;
    qreal fx = qBound<qreal>(0.0, (pos.x() - area.left()) / qreal(area.width() - 1), 1.0);
    qreal fy = qBound<qreal>(0.0, (pos.y() - area.top())  / qreal(area.height() - 1), 1.0);
    if(mKind == KIND_SV) {
        int s = qRound(fx * 255.0);
        int v = qRound((1.0 - fy) * 255.0);
        if(s == mSat && v == mVal)
            return;
        mSat = s;
        mVal = v;
        update();
        emit saturationValueChanged(mSat, mVal);
    } else {
        int h = qBound(0, qRound(fy * 359.0), 359);
        if(h == mHue)
            return;
        mHue = h;
        update();
        emit hueChanged(mHue);
    }
}

void ColorAreaWidget::mousePressEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        setFocus();
        pickAt(event->pos());
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void ColorAreaWidget::mouseMoveEvent(QMouseEvent *event) {
    if(event->buttons() & Qt::LeftButton) {
        pickAt(event->pos());
        event->accept();
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void ColorAreaWidget::keyPressEvent(QKeyEvent *event) {
    int step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
    int dx = 0, dy = 0;
    switch(event->key()) {
        case Qt::Key_Left:  dx = -step; break;
        case Qt::Key_Right: dx =  step; break;
        case Qt::Key_Up:    dy = -step; break;
        case Qt::Key_Down:  dy =  step; break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }
    event->accept();
    if(mKind == KIND_SV) {
        int s = qBound(0, mSat + dx, 255);
        int v = qBound(0, mVal - dy, 255); // up = brighter
        if(s != mSat || v != mVal) {
            mSat = s;
            mVal = v;
            update();
            emit saturationValueChanged(mSat, mVal);
        }
    } else {
        int h = qBound(0, mHue + dy + dx, 359);
        if(h != mHue) {
            mHue = h;
            update();
            emit hueChanged(mHue);
        }
    }
}
