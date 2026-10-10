#pragma once

#include <QWidget>
#include <QPointF>
#include <QEasingCurve>
#include <QVariantAnimation>
#include <QList>

// Cubic bezier easing graph (like CSS cubic-bezier(x1, y1, x2, y2)): drag the two handles to shape
// how a transition eases in and out. x stays in 0..1, y may leave 0..1 for an overshoot.
// Follows the colour scheme. Arrow keys nudge the active handle (Shift = bigger steps).
class EaseCurveEditor : public QWidget {
    Q_OBJECT
public:
    struct Preset {
        QString name;
        QPointF c1, c2;
    };

    explicit EaseCurveEditor(QWidget *parent = nullptr);

    void setCurve(const QPointF &c1, const QPointF &c2); // does not emit curveChanged
    QPointF c1() const { return mC1; }
    QPointF c2() const { return mC2; }

    static QList<Preset> presets();
    static QEasingCurve toEasing(const QPointF &c1, const QPointF &c2);
    static QString serialize(const QPointF &c1, const QPointF &c2);
    static bool parse(const QString &text, QPointF &c1, QPointF &c2);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void curveChanged(QPointF c1, QPointF c2); // only for edits made by the user

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QPointF mC1, mC2;
    int mActive = 0;  // 0 = none, 1 / 2 = handle that is dragged / gets the arrow keys
    int mHover = 0;
    bool mDragging = false;
    QVariantAnimation mPreview;

    QRectF plotRect() const;
    QPointF toWidget(const QPointF &curvePoint) const;
    QPointF toCurve(const QPointF &widgetPoint) const;
    int handleAt(const QPoint &pos) const;
    void moveHandle(int handle, const QPointF &curvePoint);
    void restartPreview();
};
