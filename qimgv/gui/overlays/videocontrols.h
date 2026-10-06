#pragma once

#include "gui/customwidgets/overlaywidget.h"
#include "settings.h"
#include <QPushButton>
#include <QPropertyAnimation>

namespace Ui {
class VideoControls;
}

enum PlaybackMode {
    PLAYBACK_ANIMATION,
    PLAYBACK_VIDEO
};

class VideoControls : public OverlayWidget
{
    Q_OBJECT
    Q_PROPERTY(int slideOffset READ slideOffset WRITE setSlideOffset)

public:
    explicit VideoControls(FloatingWidgetContainer *parent = nullptr);
    ~VideoControls();

    int slideOffset() const;
    void setSlideOffset(int offset);

public slots:
    // slide in from / out to the screen edge the bar is docked to
    void show();
    void hide();
    void setPlaybackDuration(int);
    void setPlaybackPosition(int);
    void onPlaybackPaused(bool);
    void onVideoMuted(bool);
    void setMode(PlaybackMode _mode);

signals:
    void seek(int pos);
    void seekForward();
    void seekBackward();

private slots:
    void readSettings();

protected:
    void recalculateGeometry() override;

private:
    int hiddenOffset();
    Ui::VideoControls *ui;
    QPropertyAnimation *slideAnimation;
    int mSlideOffset = 0; // px the bar is currently shifted towards its edge (0 = resting place)
    bool hiding = false;
    int lastPosition;
    PlaybackMode mode;
};
