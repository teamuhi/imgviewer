#include "slideshowsettingspanel.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include "settings.h"
#include "gui/overlays/slidetransition.h"

namespace {
const int PANEL_WIDTH = 390;
const int GUTTER = 16;      // min distance to the viewer edges
const int BAR_GAP = 8;      // between the panel and the bar
}

SlideshowSettingsPanel::SlideshowSettingsPanel(FloatingWidgetContainer *parent, SlideshowBar *bar)
    : OverlayWidget(parent), mBar(bar) {
    setAccessibleName("SlideshowSettingsPanel");
    setAttribute(Qt::WA_NoMousePropagation, true);
    setPosition(FloatingWidgetPosition::BOTTOM);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    mScroll = new QScrollArea(this);
    mScroll->setFrameShape(QFrame::NoFrame);
    mScroll->setWidgetResizable(true);
    mScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(mScroll);

    mContent = new QWidget(mScroll);
    mScroll->setWidget(mContent);
    QFormLayout *form = new QFormLayout(mContent);
    form->setContentsMargins(10, 6, 12, 6);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(13);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto heading = [this](const QString &text) {
        QLabel *label = new QLabel(text, mContent);
        label->setAccessibleName("SlideshowPanelHeading");
        return label;
    };
    // empty row: extra air between the groups
    auto gap = [&](int height) {
        QWidget *spacer = new QWidget(mContent);
        spacer->setFixedHeight(height);
        form->addRow(spacer);
    };
    // adds "label | field" and remembers both, so the pair can be greyed out together
    auto addRow = [&](const QString &text, QWidget *field, QWidget *pair[2] = nullptr) {
        QLabel *label = new QLabel(text, mContent);
        form->addRow(label, field);
        if(pair) {
            pair[0] = label;
            pair[1] = field;
        }
    };

    // --- captions --------------------------------------------------------------
    form->addRow(heading(tr("Captions")));
    QWidget *shown = new QWidget(mContent);
    QHBoxLayout *shownLayout = new QHBoxLayout(shown);
    shownLayout->setContentsMargins(0, 0, 0, 0);
    shownLayout->setSpacing(22);
    nameCheck = new QCheckBox(tr("Name"), shown);
    dateCheck = new QCheckBox(tr("Date"), shown);
    nameCheck->setToolTip(tr("Show the file name"));
    dateCheck->setToolTip(tr("Show the date the file was modified"));
    shownLayout->addWidget(nameCheck);
    shownLayout->addWidget(dateCheck);
    shownLayout->addStretch(1);
    addRow(tr("Show"), shown);
    addRow(tr("Text size"), makeSlider(captionSlider, 75, 300, " %"));
    captionSlider->setToolTip(tr("Size of the file name / date text"));

    // --- transition ---------------------------------------------------------------
    gap(10);
    form->addRow(heading(tr("Transition")));
    durationSpin = new ScrubDoubleSpinBox(mContent);
    durationSpin->setRange(0.08, 3.0);
    durationSpin->setSingleStep(0.05);
    durationSpin->setDecimals(2);
    durationSpin->setSuffix(" s");
    durationSpin->setToolTip(tr("How long a transition takes (never longer than the time per slide). Drag sideways to change it, or click to type."));
    addRow(tr("Duration"), durationSpin);

    QWidget *easing = new QWidget(mContent);
    QVBoxLayout *easingLayout = new QVBoxLayout(easing);
    easingLayout->setContentsMargins(0, 0, 0, 0);
    easingLayout->setSpacing(10);
    presetCombo = new QComboBox(easing);
    presetCombo->setFocusPolicy(Qt::NoFocus);
    for(const EaseCurveEditor::Preset &preset : EaseCurveEditor::presets())
        presetCombo->addItem(preset.name);
    presetCombo->addItem(tr("Custom")); // last
    curveEditor = new EaseCurveEditor(easing);
    curveEditor->setToolTip(tr("Drag the two handles to shape how the transition eases in and out.\nDouble-click a handle to reset it."));
    easingLayout->addWidget(presetCombo);
    easingLayout->addWidget(curveEditor);
    addRow(tr("Easing"), easing);

    addRow(tr("Strength"), makeSlider(strengthSlider, 0, 100, " %"), strengthRow);
    strengthSlider->setToolTip(tr("Main amount of the effect: blur, pixel size, zoom, travel distance..."));
    addRow(tr("Softness"), makeSlider(softnessSlider, 0, 100, " %"), softnessRow);
    softnessSlider->setToolTip(tr("Feathered edge of the wipe / iris"));
    blockSpin = new ScrubSpinBox(mContent);
    blockSpin->setRange(16, 256);
    blockSpin->setSingleStep(8);
    blockSpin->setSuffix(" px");
    blockSpin->setToolTip(tr("Size of the tiles (dissolve)"));
    addRow(tr("Block size"), blockSpin, blockRow);
    directionCombo = new QComboBox(mContent);
    directionCombo->addItems({ tr("Auto (next / previous)"), tr("Left"), tr("Right"), tr("Up"), tr("Down") });
    directionCombo->setFocusPolicy(Qt::NoFocus);
    directionCombo->setToolTip(tr("The way the old slide leaves"));
    addRow(tr("Direction"), directionCombo, directionRow);

    QWidget *dip = new QWidget(mContent);
    QHBoxLayout *dipLayout = new QHBoxLayout(dip);
    dipLayout->setContentsMargins(0, 0, 0, 0);
    dipLayout->setSpacing(8);
    dipButton = new ColorSelectorButton(dip);
    dipButton->setDescription(tr("Dip color"));
    dipButton->setFixedSize(44, 24);
    dipButton->setToolTip(tr("Click to pick any color"));
    dipLayout->addWidget(dipButton);
    struct Quick { QString text; int kind; }; // 0 black, 1 white, 2 accent
    for(const Quick &quick : { Quick{ tr("Black"), 0 }, Quick{ tr("White"), 1 }, Quick{ tr("Accent"), 2 } }) {
        QPushButton *button = new QPushButton(quick.text, dip);
        button->setFocusPolicy(Qt::NoFocus);
        dipLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, quick]() {
            const QColor color = quick.kind == 0 ? QColor(Qt::black) : quick.kind == 1 ? QColor(Qt::white) : settings->colorScheme().accent;
            dipButton->setColor(color);
            saveOptions();
        });
    }
    dipLayout->addStretch(1);
    addRow(tr("Dip color"), dip, dipRow);

    randomCheck = new QCheckBox(tr("Random style for every slide"), mContent);
    randomCheck->setFocusPolicy(Qt::NoFocus);
    randomCheck->setToolTip(tr("Picks another transition style each time (the style above is ignored)"));
    gap(4);
    form->addRow(randomCheck);
    QPushButton *resetButton = new QPushButton(tr("Reset to defaults"), mContent);
    resetButton->setFocusPolicy(Qt::NoFocus);
    gap(2);
    form->addRow(resetButton);

    for(QCheckBox *check : { nameCheck, dateCheck })
        check->setFocusPolicy(Qt::NoFocus);

    connect(nameCheck, &QCheckBox::toggled, this, &SlideshowSettingsPanel::saveOptions);
    connect(dateCheck, &QCheckBox::toggled, this, &SlideshowSettingsPanel::saveOptions);
    connect(randomCheck, &QCheckBox::toggled, this, &SlideshowSettingsPanel::saveOptions);
    connect(durationSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &SlideshowSettingsPanel::saveOptions);
    connect(blockSpin, qOverload<int>(&QSpinBox::valueChanged), this, &SlideshowSettingsPanel::saveOptions);
    connect(directionCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &SlideshowSettingsPanel::saveOptions);
    connect(dipButton, &ColorSelectorButton::colorChanged, this, &SlideshowSettingsPanel::saveOptions);
    connect(curveEditor, &EaseCurveEditor::curveChanged, this, [this]() {
        updatePresetCombo();
        saveOptions();
    });
    connect(presetCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        const QList<EaseCurveEditor::Preset> presets = EaseCurveEditor::presets();
        if(syncing || index < 0 || index >= presets.size())
            return; // "Custom" does nothing by itself
        curveEditor->setCurve(presets[index].c1, presets[index].c2);
        saveOptions();
    });
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        settings->resetSlideshowTransitionOptions();
        readSettings();
        emit optionsChanged();
    });
    connect(settings, &Settings::settingsChanged, this, &SlideshowSettingsPanel::readSettings);

    if(mBar)
        mBar->installEventFilter(this); // follows the bar (resize, narrow layout)
    if(parent)
        setContainerSize(parent->size());
    readSettings();
    OverlayWidget::hide();
}

QWidget *SlideshowSettingsPanel::makeSlider(QSlider *&slider, int min, int max, const QString &suffix) {
    QWidget *row = new QWidget(mContent);
    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(min, max);
    slider->setFocusPolicy(Qt::NoFocus);
    QLabel *value = new QLabel(QString::number(slider->value()) + suffix, row);
    value->setMinimumWidth(QFontMetrics(value->font()).horizontalAdvance(QString::number(max) + suffix) + 4);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(slider, 1);
    layout->addWidget(value);
    connect(slider, &QSlider::valueChanged, this, [this, value, suffix](int v) {
        value->setText(QString::number(v) + suffix);
        saveOptions();
    });
    return row;
}

void SlideshowSettingsPanel::readSettings() {
    syncing = true;
    nameCheck->setChecked(settings->slideshowShowName());
    dateCheck->setChecked(settings->slideshowShowDate());
    captionSlider->setValue(settings->slideshowCaptionScale());
    durationSpin->setValue(settings->slideshowTransitionDuration() / 1000.0);
    QPointF c1(0.65, 0), c2(0.35, 1);
    EaseCurveEditor::parse(settings->slideshowTransitionEase(), c1, c2); // keeps the defaults when it is broken
    curveEditor->setCurve(c1, c2);
    updatePresetCombo();
    strengthSlider->setValue(settings->slideshowTransitionStrength());
    softnessSlider->setValue(settings->slideshowTransitionSoftness());
    blockSpin->setValue(settings->slideshowTransitionBlockSize());
    directionCombo->setCurrentIndex(settings->slideshowTransitionDirection());
    dipButton->setColor(settings->slideshowTransitionDipColor());
    randomCheck->setChecked(settings->slideshowTransitionRandom());
    syncing = false;
    updateEnabled();
}

void SlideshowSettingsPanel::saveOptions() {
    if(syncing)
        return;
    settings->setSlideshowShowName(nameCheck->isChecked());
    settings->setSlideshowShowDate(dateCheck->isChecked());
    settings->setSlideshowCaptionScale(captionSlider->value());
    settings->setSlideshowTransitionDuration(qRound(durationSpin->value() * 1000.0));
    settings->setSlideshowTransitionEase(EaseCurveEditor::serialize(curveEditor->c1(), curveEditor->c2()));
    settings->setSlideshowTransitionStrength(strengthSlider->value());
    settings->setSlideshowTransitionSoftness(softnessSlider->value());
    settings->setSlideshowTransitionBlockSize(blockSpin->value());
    settings->setSlideshowTransitionDirection(directionCombo->currentIndex());
    settings->setSlideshowTransitionDipColor(dipButton->color());
    settings->setSlideshowTransitionRandom(randomCheck->isChecked());
    updateEnabled();
    emit optionsChanged();
}

// the preset that matches the graph, "Custom" when there is none
void SlideshowSettingsPanel::updatePresetCombo() {
    const QList<EaseCurveEditor::Preset> presets = EaseCurveEditor::presets();
    int index = presets.size(); // Custom
    for(int i = 0; i < presets.size(); i++) {
        if(QLineF(presets[i].c1, curveEditor->c1()).length() < 0.01 && QLineF(presets[i].c2, curveEditor->c2()).length() < 0.01) {
            index = i;
            break;
        }
    }
    const bool wasSyncing = syncing;
    syncing = true; // picking a preset must not feed back into the graph
    presetCombo->setCurrentIndex(index);
    syncing = wasSyncing;
}

// only the controls the current style uses are active (greyed out, not hidden: the panel keeps its size)
void SlideshowSettingsPanel::updateEnabled() {
    const int style = settings->slideshowTransition();
    const bool any = settings->slideshowTransitionRandom();
    auto setPair = [](QWidget *pair[2], bool enabled) {
        pair[0]->setEnabled(enabled);
        pair[1]->setEnabled(enabled);
    };
    const bool animated = style != TRANSITION_NONE || any;
    presetCombo->setEnabled(animated);
    curveEditor->setEnabled(animated);
    durationSpin->setEnabled(animated);
    setPair(strengthRow, any || style == TRANSITION_SLIDE || style == TRANSITION_ZOOM || style == TRANSITION_DIP_COLOR
                         || style == TRANSITION_BLUR_FADE || style == TRANSITION_MOTION_BLUR || style == TRANSITION_PIXEL_MASH
                         || style == TRANSITION_COVER || style == TRANSITION_DISSOLVE);
    setPair(softnessRow, any || style == TRANSITION_WIPE || style == TRANSITION_IRIS);
    setPair(blockRow, any || style == TRANSITION_DISSOLVE);
    setPair(directionRow, any || style == TRANSITION_SLIDE || style == TRANSITION_MOTION_BLUR
                          || style == TRANSITION_PUSH || style == TRANSITION_COVER || style == TRANSITION_WIPE);
    setPair(dipRow, any || style == TRANSITION_DIP_COLOR);
    captionSlider->setEnabled(nameCheck->isChecked() || dateCheck->isChecked());
}

//------------------------------------------------------------------------------
void SlideshowSettingsPanel::show() {
    readSettings();
    OverlayWidget::show();
    recalculateGeometry();
    raise();
}

void SlideshowSettingsPanel::hide() {
    OverlayWidget::hide();
}

bool SlideshowSettingsPanel::eventFilter(QObject *watched, QEvent *event) {
    if(watched == mBar && isVisible()) {
        switch(event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::LayoutRequest:
            recalculateGeometry();
            break;
        default:
            break;
        }
    }
    return OverlayWidget::eventFilter(watched, event);
}

// above the bar, its right edge lined up with the gear button; shrinks / scrolls on small viewers
void SlideshowSettingsPanel::recalculateGeometry() {
    if(!mContent || !mBar)
        return;
    const QSize container = containerSize();
    const int width = qMin(PANEL_WIDTH, container.width() - 2 * GUTTER);
    if(width < 140)
        return;
    const int barTop = mBar->geometry().top() - mBar->slideOffset(); // where the bar rests
    const int bottom = qMin(container.height() - GUTTER, barTop - BAR_GAP);
    const int maxHeight = qMax(120, bottom - GUTTER);
    const int wanted = mContent->sizeHint().height() + layout()->contentsMargins().top() + layout()->contentsMargins().bottom() + 2;
    const int height = qMin(wanted, maxHeight);
    int right = container.width() / 2 + width / 2; // centred when there is no anchor
    if(QWidget *anchor = mBar->settingsAnchor())
        right = anchor->mapTo(parentWidget(), QPoint(anchor->width(), 0)).x();
    const int x = qBound(GUTTER, right - width, qMax(GUTTER, container.width() - GUTTER - width));
    setGeometry(x, bottom - height, width, height);
}
