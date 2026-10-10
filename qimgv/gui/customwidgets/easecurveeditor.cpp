#include "easecurveeditor.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QtMath>
#include "settings.h"

namespace {
const qreal Y_MIN = -0.6, Y_MAX = 1.6; // room for overshoot above and below the 0..1 square
const int MARGIN = 10;
const int HANDLE_RADIUS = 6;
const int HIT_RADIUS = 12;
const int PREVIEW_MS = 1000;
}

EaseCurveEditor::EaseCurveEditor(QWidget *parent) : QWidget(parent), mC1(0.65, 0), mC2(0.35, 1) {
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);
    setAccessibleName("EaseCurveEditor");
    mPreview.setStartValue(0.0);
    mPreview.setEndValue(1.0);
    mPreview.setDuration(PREVIEW_MS);
    mPreview.setEasingCurve(QEasingCurve::Linear);
    connect(&mPreview, &QVariantAnimation::valueChanged, this, [this]() { update(); });
    connect(settings, &Settings::settingsChanged, this, [this]() { update(); });
}

QList<EaseCurveEditor::Preset> EaseCurveEditor::presets() {
    return {
        { tr("Linear"),       QPointF(0.0, 0.0),   QPointF(1.0, 1.0) },
        { tr("Ease in"),      QPointF(0.42, 0.0),  QPointF(1.0, 1.0) },
        { tr("Ease out"),     QPointF(0.0, 0.0),   QPointF(0.58, 1.0) },
        { tr("Ease in-out"),  QPointF(0.65, 0.0),  QPointF(0.35, 1.0) },
        { tr("Smooth step"),  QPointF(0.45, 0.05), QPointF(0.55, 0.95) },
        { tr("Back (overshoot)"), QPointF(0.34, 1.56), QPointF(0.64, 1.0) },
        { tr("Back in-out"),  QPointF(0.68, -0.55), QPointF(0.27, 1.55) },
        { tr("Snappy"),       QPointF(0.16, 1.0),  QPointF(0.3, 1.0) }
    };
}

QEasingCurve EaseCurveEditor::toEasing(const QPointF &c1, const QPointF &c2) {
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(c1, c2, QPointF(1.0, 1.0));
    return curve;
}

QString EaseCurveEditor::serialize(const QPointF &c1, const QPointF &c2) {
    return QString("%1,%2,%3,%4").arg(c1.x(), 0, 'f', 3).arg(c1.y(), 0, 'f', 3).arg(c2.x(), 0, 'f', 3).arg(c2.y(), 0, 'f', 3);
}

bool EaseCurveEditor::parse(const QString &text, QPointF &c1, QPointF &c2) {
    const QStringList parts = text.split(',');
    if(parts.size() != 4)
        return false;
    qreal v[4];
    for(int i = 0; i < 4; i++) {
        bool ok = false;
        v[i] = parts[i].trimmed().toDouble(&ok);
        if(!ok || !qIsFinite(v[i]))
            return false;
    }
    c1 = QPointF(qBound(0.0, v[0], 1.0), qBound(Y_MIN, v[1], Y_MAX));
    c2 = QPointF(qBound(0.0, v[2], 1.0), qBound(Y_MIN, v[3], Y_MAX));
    return true;
}

void EaseCurveEditor::setCurve(const QPointF &c1, const QPointF &c2) {
    if(c1 == mC1 && c2 == mC2)
        return;
    mC1 = c1;
    mC2 = c2;
    restartPreview();
    update();
}

QSize EaseCurveEditor::sizeHint() const {
    return QSize(300, 210);
}

QSize EaseCurveEditor::minimumSizeHint() const {
    return QSize(180, 150);
}

QRectF EaseCurveEditor::plotRect() const {
    return QRectF(rect()).adjusted(MARGIN, MARGIN, -MARGIN, -MARGIN);
}

QPointF EaseCurveEditor::toWidget(const QPointF &c) const {
    const QRectF r = plotRect();
    return QPointF(r.left() + c.x() * r.width(), r.bottom() - (c.y() - Y_MIN) / (Y_MAX - Y_MIN) * r.height());
}

QPointF EaseCurveEditor::toCurve(const QPointF &w) const {
    const QRectF r = plotRect();
    return QPointF((w.x() - r.left()) / r.width(), Y_MIN + (r.bottom() - w.y()) / r.height() * (Y_MAX - Y_MIN));
}

int EaseCurveEditor::handleAt(const QPoint &pos) const {
    const QPointF p(pos);
    const qreal d1 = QLineF(p, toWidget(mC1)).length(), d2 = QLineF(p, toWidget(mC2)).length();
    if(d1 > HIT_RADIUS && d2 > HIT_RADIUS)
        return 0;
    return d1 <= d2 ? 1 : 2;
}

void EaseCurveEditor::moveHandle(int handle, const QPointF &c) {
    const QPointF clamped(qBound(0.0, c.x(), 1.0), qBound(Y_MIN, c.y(), Y_MAX));
    QPointF &target = handle == 1 ? mC1 : mC2;
    if(clamped == target)
        return;
    target = clamped;
    restartPreview();
    update();
    emit curveChanged(mC1, mC2);
}

void EaseCurveEditor::restartPreview() {
    if(!isVisible())
        return;
    mPreview.stop();
    mPreview.start();
}

void EaseCurveEditor::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    restartPreview();
}

void EaseCurveEditor::hideEvent(QHideEvent *event) {
    mPreview.stop(); // no animation ticks while the panel is closed
    QWidget::hideEvent(event);
}

void EaseCurveEditor::mousePressEvent(QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return;
    const int handle = handleAt(event->pos());
    if(handle) {
        mActive = handle;
        mDragging = true;
        setFocus();
        update();
    }
    event->accept();
}

void EaseCurveEditor::mouseMoveEvent(QMouseEvent *event) {
    if(mDragging) {
        moveHandle(mActive, toCurve(event->pos()));
    } else {
        const int handle = handleAt(event->pos());
        if(handle != mHover) {
            mHover = handle;
            setCursor(handle ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
    }
    event->accept();
}

void EaseCurveEditor::mouseReleaseEvent(QMouseEvent *event) {
    mDragging = false;
    event->accept();
}

// reset the handle under the cursor to the linear position
void EaseCurveEditor::mouseDoubleClickEvent(QMouseEvent *event) {
    const int handle = handleAt(event->pos());
    if(handle)
        moveHandle(handle, handle == 1 ? QPointF(0.33, 0.33) : QPointF(0.66, 0.66));
    event->accept();
}

void EaseCurveEditor::keyPressEvent(QKeyEvent *event) {
    const qreal step = (event->modifiers() & Qt::ShiftModifier) ? 0.1 : 0.02;
    QPointF delta;
    switch(event->key()) {
    case Qt::Key_Left:  delta = QPointF(-step, 0); break;
    case Qt::Key_Right: delta = QPointF(step, 0);  break;
    case Qt::Key_Up:    delta = QPointF(0, step);  break;
    case Qt::Key_Down:  delta = QPointF(0, -step); break;
    case Qt::Key_Tab:   // jump between the two handles
        mActive = mActive == 1 ? 2 : 1;
        update();
        event->accept();
        return;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    if(!mActive)
        mActive = 1;
    moveHandle(mActive, (mActive == 1 ? mC1 : mC2) + delta);
    event->accept();
}

void EaseCurveEditor::paintEvent(QPaintEvent *) {
    const ColorScheme &colors = settings->colorScheme();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF plot = plotRect();

    p.fillRect(rect(), colors.widget);
    // the 0..1 square, grid and the straight (linear) reference
    const QRectF unit(toWidget(QPointF(0, 1)), toWidget(QPointF(1, 0)));
    QColor faint = colors.text;
    faint.setAlpha(40);
    p.setPen(QPen(faint, 1));
    for(int i = 1; i < 4; i++) {
        const qreal x = unit.left() + unit.width() * i / 4, y = unit.top() + unit.height() * i / 4;
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }
    p.setPen(QPen(colors.widget_border.lighter(160), 1));
    p.drawRect(unit);
    p.setPen(QPen(faint, 1, Qt::DashLine));
    p.drawLine(unit.bottomLeft(), unit.topRight());

    // the handle arms
    const QPointF start = toWidget(QPointF(0, 0)), end = toWidget(QPointF(1, 1));
    const QPointF h1 = toWidget(mC1), h2 = toWidget(mC2);
    QColor arm = colors.text;
    arm.setAlpha(110);
    p.setPen(QPen(arm, 1));
    p.drawLine(start, h1);
    p.drawLine(end, h2);

    // the curve (sampled through the same easing the transition uses)
    const QEasingCurve easing = toEasing(mC1, mC2);
    QPainterPath path;
    const int samples = qMax(32, int(plot.width() / 3));
    for(int i = 0; i <= samples; i++) {
        const qreal t = qreal(i) / samples;
        const QPointF point = toWidget(QPointF(t, easing.valueForProgress(t)));
        i ? path.lineTo(point) : path.moveTo(point);
    }
    p.setClipRect(plot.adjusted(-2, -2, 2, 2));
    p.setPen(QPen(colors.accent, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);
    p.setClipping(false);

    // preview dot travelling along the curve
    if(mPreview.state() == QAbstractAnimation::Running) {
        const qreal t = mPreview.currentValue().toReal();
        const QPointF dot = toWidget(QPointF(t, easing.valueForProgress(t)));
        p.setPen(Qt::NoPen);
        p.setBrush(colors.text);
        p.drawEllipse(dot, 3.5, 3.5);
    }

    // handles
    for(int handle = 1; handle <= 2; handle++) {
        const bool active = (mActive == handle && (mDragging || hasFocus())) || mHover == handle;
        p.setPen(QPen(colors.accent, 2));
        p.setBrush(active ? colors.accent : colors.widget);
        p.drawEllipse(handle == 1 ? h1 : h2, HANDLE_RADIUS, HANDLE_RADIUS);
    }
}
