#include "videocontrols.h"
#include "ui_videocontrols.h"

static const int SLIDE_DURATION_MS = 180;

VideoControls::VideoControls(FloatingWidgetContainer *parent) :
    OverlayWidget(parent),
    ui(new Ui::VideoControls)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_NoMousePropagation, true);
    hide();
    ui->pauseButton->setIconPath(":res/icons/common/buttons/videocontrols/play24.png");
    ui->pauseButton->setAction("pauseVideo");
    ui->prevFrameButton->setIconPath(":res/icons/common/buttons/videocontrols/skip-backwards24.png");
    ui->prevFrameButton->setAction("frameStepBack");
    ui->nextFrameButton->setIconPath(":res/icons/common/buttons/videocontrols/skip-forward24.png");
    ui->nextFrameButton->setAction("frameStep");
    ui->muteButton->setIconPath(":/res/icons/common/buttons/videocontrols/mute-on24.png");
    ui->muteButton->setAction("toggleMute");

    lastPosition = -1;

    slideAnimation = new QPropertyAnimation(this, "slideOffset", this);
    slideAnimation->setDuration(SLIDE_DURATION_MS);
    slideAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(slideAnimation, &QPropertyAnimation::finished, this, [this]() {
        if(!hiding)
            return;
        hiding = false;
        OverlayWidget::hide();
        mSlideOffset = 0;
        recalculateGeometry();
    });

    readSettings();
    connect(settings, &Settings::settingsChanged, this, &VideoControls::readSettings);

    connect(ui->seekBar, &VideoSlider::sliderMovedX, this, &VideoControls::seek);

    if(parent)
        setContainerSize(parent->size());
}

void VideoControls::readSettings() {
    if(settings->panelEnabled() && settings->panelPosition() == PanelPosition::PANEL_BOTTOM)
        setPosition(FloatingWidgetPosition::TOP);
    else
        setPosition(FloatingWidgetPosition::BOTTOM);
}

VideoControls::~VideoControls() {
    delete ui;
}

// distance that moves the bar completely out of the container, towards the edge it is docked to
int VideoControls::hiddenOffset() {
    const int travel = height() + verticalMargin();
    return (position == FloatingWidgetPosition::TOP) ? -travel : travel;
}

int VideoControls::slideOffset() const {
    return mSlideOffset;
}

// moves the bar and fades it with the slide: fully transparent at the edge, opaque at rest
void VideoControls::setSlideOffset(int offset) {
    move(x(), y() + offset - mSlideOffset);
    mSlideOffset = offset;
    const int total = qAbs(hiddenOffset());
    setProperty("opacity", total ? 1.0 - qMin(1.0, qAbs(offset) / qreal(total)) : 1.0);
}

void VideoControls::recalculateGeometry() {
    OverlayWidget::recalculateGeometry();
    if(mSlideOffset)
        move(x(), y() + mSlideOffset);
}

// called on every mouse move over the control area: only the first call starts the animation
void VideoControls::show() {
    if(!isHidden() && !hiding)
        return;
    const bool wasHidden = isHidden();
    hiding = false; // before stop(): a cancelled hide must not finish
    slideAnimation->stop();
    OverlayWidget::show();
    if(wasHidden)
        setSlideOffset(hiddenOffset());
    slideAnimation->setStartValue(mSlideOffset);
    slideAnimation->setEndValue(0);
    slideAnimation->start();
}

void VideoControls::hide() {
    if(isHidden() || hiding)
        return;
    slideAnimation->stop();
    if(!isVisible()) { // parent is not on screen, nothing to animate
        mSlideOffset = 0;
        OverlayWidget::hide();
        return;
    }
    hiding = true;
    slideAnimation->setStartValue(mSlideOffset);
    slideAnimation->setEndValue(hiddenOffset());
    slideAnimation->start();
}

void VideoControls::setMode(PlaybackMode _mode) {
    mode = _mode;
    ui->muteButton->setVisible( (mode == PLAYBACK_VIDEO) );
}

void VideoControls::setPlaybackDuration(int duration) {
    QString durationStr;
    if(mode == PLAYBACK_VIDEO) {
        int _time = duration;
        int hours = _time / 3600;
        _time -= hours * 3600;
        int minutes = _time / 60;
        int seconds = _time - minutes * 60;
        durationStr = QString("%1").arg(minutes, 2, 10, QChar('0')) + ":" +
                      QString("%1").arg(seconds, 2, 10, QChar('0'));
        if(hours)
            durationStr.prepend(QString("%1").arg(hours, 2, 10, QChar('0')) + ":");
    } else {
        durationStr = QString::number(duration);
    }
    ui->seekBar->setRange(0, duration - 1);
    ui->durationLabel->setText(durationStr);
    ui->positionLabel->setText(durationStr);
    recalculateGeometry();
    ui->positionLabel->setText("");
}

void VideoControls::setPlaybackPosition(int position) {
    if(position == lastPosition)
        return;
    QString positionStr;
    if(mode == PLAYBACK_VIDEO) {
        int _time = position;
        int hours = _time / 3600;
        _time -= hours * 3600;
        int minutes = _time / 60;
        int seconds = _time - minutes * 60;
        positionStr = QString("%1").arg(minutes, 2, 10, QChar('0')) + ":" +
                      QString("%1").arg(seconds, 2, 10, QChar('0'));
        if(hours)
            positionStr.prepend(QString("%1").arg(hours, 2, 10, QChar('0')) + ":");
    } else {
        positionStr = QString::number(position + 1);
    }
    ui->positionLabel->setText(positionStr);
    ui->seekBar->blockSignals(true);
    ui->seekBar->setValue(position);
    ui->seekBar->blockSignals(false);
    lastPosition = position;
}

void VideoControls::onPlaybackPaused(bool mode) {
    if(mode)
        ui->pauseButton->setIconPath(":res/icons/common/buttons/videocontrols/play24.png");
    else
        ui->pauseButton->setIconPath(":res/icons/common/buttons/videocontrols/pause24.png");
}

void VideoControls::onVideoMuted(bool mode) {
    if(mode)
        ui->muteButton->setIconPath(":res/icons/common/buttons/videocontrols/mute-on24.png");
    else
        ui->muteButton->setIconPath(":res/icons/common/buttons/videocontrols/mute-off24.png");
}
