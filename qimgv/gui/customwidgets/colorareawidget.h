#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QLinearGradient>

// Interactive HSV picking surface. Two flavours:
//   KIND_SV  - saturation (x) / value (y) square for the current hue
//   KIND_HUE - vertical hue strip
// h: 0..359, s/v: 0..255
class ColorAreaWidget : public QWidget {
    Q_OBJECT
public:
    enum Kind {
        KIND_SV,
        KIND_HUE
    };

    explicit ColorAreaWidget(Kind kind, QWidget *parent = nullptr);

    void setHue(int h);
    void setSaturationValue(int s, int v);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void hueChanged(int h);
    void saturationValueChanged(int s, int v);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    Kind mKind;
    int mHue = 0, mSat = 255, mVal = 255;

    QRect areaRect() const;
    void pickAt(const QPoint &pos);
};
