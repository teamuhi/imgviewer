#include "slideshowbar.h"
#include <QHBoxLayout>
#include <QLabel>
#include "settings.h"
#include "utils/imagelib.h"

namespace {
const int SLIDE_DURATION_MS = 180;
const int BUTTON_SIZE = 34;

QIcon tintedIcon(const QString &path, const QColor &color) {
    QPixmap pixmap(path);
    if(pixmap.isNull())
        return QIcon();
    pixmap.setDevicePixelRatio(2.0);
    ImageLib::recolor(pixmap, color);
    return QIcon(pixmap);
}
}

SlideshowBar::SlideshowBar(FloatingWidgetContainer *parent) : OverlayWidget(parent) {
    setAccessibleName("SlideshowBar");
    setAttribute(Qt::WA_NoMousePropagation, true);
    setPosition(FloatingWidgetPosition::BOTTOM);
    setVerticalMargin(24);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(8);

    prevButton = new QPushButton(this);
    pauseButton = new QPushButton(this);
    nextButton = new QPushButton(this);
    prevButton->setToolTip(tr("Previous (Left)"));
    pauseButton->setToolTip(tr("Pause / resume (Space)"));
    nextButton->setToolTip(tr("Next (Right)"));
    for(QPushButton *button : { prevButton, pauseButton, nextButton }) {
        button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
        button->setIconSize(QSize(20, 20));
        button->setFocusPolicy(Qt::NoFocus);
        button->setAccessibleName("SlideshowBarButton");
        layout->addWidget(button);
    }

    // options: hidden on narrow windows, the navigation always stays
    optionsBox = new QWidget(this);
    QHBoxLayout *options = new QHBoxLayout(optionsBox);
    options->setContentsMargins(6, 0, 0, 0);
    options->setSpacing(8);
    QLabel *timerLabel = new QLabel(tr("Timer"), optionsBox);
    timerSpin = new ScrubDoubleSpinBox(optionsBox);
    timerSpin->setRange(0.5, 120.0);
    timerSpin->setSingleStep(0.5);
    timerSpin->setDecimals(1);
    timerSpin->setSuffix(" s");
    timerSpin->setToolTip(tr("Time per slide. Drag sideways to change it (Shift = faster), or click to type."));
    transitionCombo = new QComboBox(optionsBox);
    transitionCombo->addItems({ tr("No transition"), tr("Fade"), tr("Slide"), tr("Zoom") });
    transitionCombo->setFocusPolicy(Qt::NoFocus);
    loopCheck = new QCheckBox(tr("Loop"), optionsBox);
    nameCheck = new QCheckBox(tr("Name"), optionsBox);
    dateCheck = new QCheckBox(tr("Date"), optionsBox);
    nameCheck->setToolTip(tr("Show the file name"));
    dateCheck->setToolTip(tr("Show the date the file was modified"));
    for(QCheckBox *check : { loopCheck, nameCheck, dateCheck })
        check->setFocusPolicy(Qt::NoFocus);
    options->addWidget(timerLabel);
    options->addWidget(timerSpin);
    options->addWidget(transitionCombo);
    options->addWidget(loopCheck);
    options->addWidget(nameCheck);
    options->addWidget(dateCheck);
    layout->addWidget(optionsBox);

    exitButton = new QPushButton(tr("Exit"), this);
    exitButton->setToolTip(tr("Leave the slideshow (Esc)"));
    exitButton->setFocusPolicy(Qt::NoFocus);
    exitButton->setFixedHeight(BUTTON_SIZE);
    layout->addWidget(exitButton);

    connect(prevButton, &QPushButton::clicked, this, &SlideshowBar::prevRequested);
    connect(nextButton, &QPushButton::clicked, this, &SlideshowBar::nextRequested);
    connect(pauseButton, &QPushButton::clicked, this, &SlideshowBar::pauseRequested);
    connect(exitButton, &QPushButton::clicked, this, &SlideshowBar::exitRequested);
    connect(timerSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &SlideshowBar::saveOptions);
    connect(transitionCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &SlideshowBar::saveOptions);
    for(QCheckBox *check : { loopCheck, nameCheck, dateCheck })
        connect(check, &QCheckBox::toggled, this, &SlideshowBar::saveOptions);

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

    connect(settings, &Settings::settingsChanged, this, &SlideshowBar::updateIcons);
    connect(settings, &Settings::settingsChanged, this, &SlideshowBar::readSettings);
    updateIcons();
    readSettings();
    if(parent)
        setContainerSize(parent->size());
    OverlayWidget::hide();
}

void SlideshowBar::updateIcons() {
    const QColor color = settings->colorScheme().icons;
    if(color == iconColor)
        return;
    iconColor = color;
    const QString dir = ":/res/icons/common/buttons/videocontrols/";
    prevButton->setIcon(tintedIcon(dir + "skip-backwards24@2x.png", color));
    nextButton->setIcon(tintedIcon(dir + "skip-forward24@2x.png", color));
    pauseButton->setIcon(tintedIcon(dir + (paused ? "play24@2x.png" : "pause24@2x.png"), color));
}

void SlideshowBar::setPaused(bool mode) {
    if(paused == mode)
        return;
    paused = mode;
    iconColor = QColor(); // force a refresh of the play / pause icon
    updateIcons();
}

void SlideshowBar::readSettings() {
    syncing = true;
    timerSpin->setValue(settings->slideshowInterval() / 1000.0);
    transitionCombo->setCurrentIndex(settings->slideshowTransition());
    loopCheck->setChecked(settings->loopSlideshow());
    nameCheck->setChecked(settings->slideshowShowName());
    dateCheck->setChecked(settings->slideshowShowDate());
    syncing = false;
}

void SlideshowBar::saveOptions() {
    if(syncing)
        return;
    settings->setSlideshowInterval(qRound(timerSpin->value() * 1000.0));
    settings->setSlideshowTransition(transitionCombo->currentIndex());
    settings->setLoopSlideshow(loopCheck->isChecked());
    settings->setSlideshowShowName(nameCheck->isChecked());
    settings->setSlideshowShowDate(dateCheck->isChecked());
    emit optionsChanged();
}

//------------------------------------------------------------------------------
int SlideshowBar::hiddenOffset() {
    return height() + verticalMargin();
}

int SlideshowBar::slideOffset() const {
    return mSlideOffset;
}

void SlideshowBar::setSlideOffset(int offset) {
    move(x(), y() + offset - mSlideOffset);
    mSlideOffset = offset;
    const int total = qAbs(hiddenOffset());
    setProperty("opacity", total ? 1.0 - qMin(1.0, qAbs(offset) / qreal(total)) : 1.0);
}

bool SlideshowBar::isSliding() const {
    return slideAnimation && slideAnimation->state() == QAbstractAnimation::Running;
}

void SlideshowBar::recalculateGeometry() {
    // narrow window: keep only the navigation + exit
    if(optionsBox) {
        const int available = containerSize().width() - 2 * horizontalMargin();
        optionsBox->setVisible(true);
        if(sizeHint().width() > available)
            optionsBox->setVisible(false);
    }
    OverlayWidget::recalculateGeometry();
    if(mSlideOffset)
        move(x(), y() + mSlideOffset);
}

void SlideshowBar::show() {
    if(!isHidden() && !hiding)
        return;
    const bool wasHidden = isHidden();
    hiding = false;
    slideAnimation->stop();
    readSettings(); // the settings dialog may have changed something
    OverlayWidget::show();
    recalculateGeometry();
    if(wasHidden)
        setSlideOffset(hiddenOffset());
    slideAnimation->setStartValue(mSlideOffset);
    slideAnimation->setEndValue(0);
    slideAnimation->start();
}

void SlideshowBar::hide() {
    if(isHidden() || hiding)
        return;
    slideAnimation->stop();
    if(!isVisible()) {
        mSlideOffset = 0;
        OverlayWidget::hide();
        return;
    }
    hiding = true;
    slideAnimation->setStartValue(mSlideOffset);
    slideAnimation->setEndValue(hiddenOffset());
    slideAnimation->start();
}
