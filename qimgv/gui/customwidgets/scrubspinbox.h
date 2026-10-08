#pragma once

#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPoint>
#include <QPointer>
#include <QLineEdit>

// Figma-style number fields: a normal spin box (type, Enter, wheel, arrows) that can also be
// "scrubbed" - press the left button on the prefix (e.g. "W  ") or anywhere on a field that does
// not have the focus, then drag sideways. 1 step per pixel, Shift = x10, Alt / Ctrl = x0.1.
// A click without a drag focuses the field and selects the number; Esc during a drag restores it.
class ScrubController : public QObject {
    Q_OBJECT
public:
    explicit ScrubController(QAbstractSpinBox *box);
    bool isScrubbing() const { return mScrubbing; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QAbstractSpinBox *mBox;
    QPointer<QLineEdit> mEdit; // cached: never look it up while the box is being destroyed
    bool mPending = false, mScrubbing = false;
    QPoint mPressPos, mLastPos;
    double mStartValue = 0.0, mAccumulated = 0.0;

    bool inScrubZone(const QPoint &lineEditPos) const;
    void updateHoverCursor(const QPoint &lineEditPos);
    double value() const;
    void setValue(double value);
    double step() const;
    void begin();
    void finish(bool cancel);
};

class ScrubSpinBox : public QSpinBox {
    Q_OBJECT
public:
    explicit ScrubSpinBox(QWidget *parent = nullptr);
};

class ScrubDoubleSpinBox : public QDoubleSpinBox {
    Q_OBJECT
public:
    explicit ScrubDoubleSpinBox(QWidget *parent = nullptr);
};
