#pragma once

#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QPropertyAnimation>
#include "gui/customwidgets/overlaywidget.h"
#include "gui/customwidgets/scrubspinbox.h"

// Control bar of the slideshow mode, docked at the bottom centre of the viewer.
// Slides in on mouse movement, slides out when idle. The options write straight to the settings.
class SlideshowBar : public OverlayWidget {
    Q_OBJECT
    Q_PROPERTY(int slideOffset READ slideOffset WRITE setSlideOffset)

public:
    explicit SlideshowBar(FloatingWidgetContainer *parent = nullptr);

    int slideOffset() const;
    void setSlideOffset(int offset);
    void setPaused(bool paused);
    bool isSliding() const;

public slots:
    void show();
    void hide();
    void readSettings();

signals:
    void prevRequested();
    void nextRequested();
    void pauseRequested();
    void exitRequested();
    void optionsChanged();

protected:
    void recalculateGeometry() override;

private slots:
    void updateIcons();

private:
    QPushButton *prevButton = nullptr, *pauseButton = nullptr, *nextButton = nullptr, *exitButton = nullptr;
    QWidget *optionsBox = nullptr; // built after the base class already asked for a geometry update
    ScrubDoubleSpinBox *timerSpin = nullptr;
    QComboBox *transitionCombo = nullptr;
    QCheckBox *loopCheck = nullptr, *nameCheck = nullptr, *dateCheck = nullptr;
    QPropertyAnimation *slideAnimation = nullptr;
    int mSlideOffset = 0;
    bool hiding = false, paused = false, syncing = false;
    QColor iconColor;

    int hiddenOffset();
    void saveOptions();
};
