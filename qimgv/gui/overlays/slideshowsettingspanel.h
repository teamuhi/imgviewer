#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include "gui/customwidgets/overlaywidget.h"
#include "gui/customwidgets/scrubspinbox.h"
#include "gui/customwidgets/colorselectorbutton.h"
#include "gui/customwidgets/easecurveeditor.h"
#include "gui/overlays/slideshowbar.h"

// Popup of the slideshow bar's gear button: caption size, transition duration / easing graph /
// strength / softness / block size / direction / dip colour. Floats above the bar, scrolls on short
// windows. Every control writes straight to the settings (like the bar does).
class SlideshowSettingsPanel : public OverlayWidget {
    Q_OBJECT
public:
    SlideshowSettingsPanel(FloatingWidgetContainer *parent, SlideshowBar *bar);

public slots:
    void show();
    void hide();
    void readSettings();

signals:
    void optionsChanged();

protected:
    void recalculateGeometry() override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    SlideshowBar *mBar = nullptr;
    QScrollArea *mScroll = nullptr;
    QWidget *mContent = nullptr;
    QCheckBox *nameCheck = nullptr, *dateCheck = nullptr, *randomCheck = nullptr;
    QSlider *captionSlider = nullptr, *strengthSlider = nullptr, *softnessSlider = nullptr;
    ScrubDoubleSpinBox *durationSpin = nullptr;
    ScrubSpinBox *blockSpin = nullptr;
    QComboBox *presetCombo = nullptr, *directionCombo = nullptr;
    EaseCurveEditor *curveEditor = nullptr;
    ColorSelectorButton *dipButton = nullptr;
    // label + control of the rows that only make sense for some styles
    QWidget *strengthRow[2] = {}, *softnessRow[2] = {}, *blockRow[2] = {}, *directionRow[2] = {}, *dipRow[2] = {};
    bool syncing = false;

    QWidget *makeSlider(QSlider *&slider, int min, int max, const QString &suffix);
    void saveOptions();
    void updatePresetCombo();
    void updateEnabled();
};
