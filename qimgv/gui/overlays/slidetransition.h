#pragma once

#include <QWidget>
#include <QPixmap>
#include <QVariantAnimation>

enum SlideTransitionStyle {
    TRANSITION_NONE,
    TRANSITION_FADE,
    TRANSITION_SLIDE,
    TRANSITION_ZOOM
};

// Draws the previous slide on top of the viewer and animates it away once the next one is on screen.
// arm() takes the snapshot right before the switch (it also hides the half-loaded new slide),
// start() runs the animation. The snapshot is freed as soon as the animation ends.
class SlideTransition : public QWidget {
    Q_OBJECT
public:
    explicit SlideTransition(QWidget *parent);
    // direction: +1 next, -1 previous (the slide leaves the other way)
    void arm(const QPixmap &snapshot, int style, int direction);
    void start(int durationMs);
    void finish(); // jump to the end state
    bool isArmed() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPixmap mSnapshot;
    QVariantAnimation mAnimation;
    qreal mProgress = 0.0;
    int mStyle = TRANSITION_NONE, mDirection = 1;
};
