#include "settingsdialog.h"
#include "gui/overlays/slidetransition.h"
#include "ui_settingsdialog.h"

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    this->setWindowTitle(tr("Preferences — ") + qApp->applicationName());

    ui->shortcutsTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);   
    ui->aboutAppTextBrowser->viewport()->setAutoFillBackground(false);
    ui->versionLabel->setText("" + QApplication::applicationVersion());
    ui->qtVersionLabel->setText(qVersion());
    ui->appIconLabel->setPixmap(QIcon(":/res/icons/common/logo/app/22.png").pixmap(22,22));
    ui->qtIconLabel->setPixmap(QIcon(":/res/icons/common/logo/3rdparty/qt22.png").pixmap(22,16));

    // fake combobox that acts as a menu button
    // less code than using pushbutton with menu
    // will be replaced with something custom later
    connect(ui->themeSelectorComboBox, qOverload<int>(&QComboBox::currentIndexChanged), [this](int index) {
        ui->themeSelectorComboBox->blockSignals(true);
        ui->themeSelectorComboBox->setCurrentIndex(index);
        ui->themeSelectorComboBox->blockSignals(false);
        switch(index) {
            case 0: setColorScheme(ThemeStore::colorScheme(COLORS_BLACK));    settings->setColorTid(COLORS_BLACK);    break;
            case 1: setColorScheme(ThemeStore::colorScheme(COLORS_DARK));     settings->setColorTid(COLORS_DARK);     break;
            case 2: setColorScheme(ThemeStore::colorScheme(COLORS_DARKBLUE)); settings->setColorTid(COLORS_DARKBLUE); break;
            case 3: setColorScheme(ThemeStore::colorScheme(COLORS_LIGHT));    settings->setColorTid(COLORS_LIGHT);    break;
        }
    });

    // the readout lives in the top bar: nothing to show without it
    connect(ui->showTopBar, &QCheckBox::toggled, ui->showTopBarPerformance, &QWidget::setEnabled);

    connect(ui->useSystemColorsCheckBox, &QCheckBox::toggled, [this](bool useSystemTheme) {
        if(useSystemTheme) {
            ui->themeSelectorComboBox->setCurrentIndex(-1);
            setColorScheme(ThemeStore::colorScheme(COLORS_SYSTEM));
            settings->setColorTid(COLORS_SYSTEM);
        }
        else {
            readColorScheme();
            settings->setColorTid(COLORS_CUSTOMIZED);
        }
        ui->themeSelectorComboBox->setEnabled(!useSystemTheme);
        ui->colorConfigSubgroup->setEnabled(!useSystemTheme);
        ui->modifySystemSchemeLabel->setVisible(useSystemTheme);
    });

    connect(ui->modifySystemSchemeLabel, &ClickableLabel::clicked, [this]() {
        ui->useSystemColorsCheckBox->setChecked(false);
        setColorScheme(ThemeStore::colorScheme(COLORS_CUSTOMIZED));
        settings->setColorTid(COLORS_CUSTOMIZED);
    });

    ui->colorSelectorAccent->setDescription(tr("Accent color"));
    ui->colorSelectorBackground->setDescription(tr("Windowed mode background"));
    ui->colorSelectorFullscreen->setDescription(tr("Fullscreen mode background"));
    ui->colorSelectorFolderview->setDescription(tr("FolderView background"));
    ui->colorSelectorFolderviewPanel->setDescription(tr("FolderView top panel"));
    ui->colorSelectorText->setDescription(tr("Text color"));
    ui->colorSelectorWidget->setDescription(tr("Widget background"));
    ui->colorSelectorWidgetBorder->setDescription(tr("Widget border"));
    ui->colorSelectorOverlay->setDescription(tr("Overlay background"));
    ui->colorSelectorOverlayText->setDescription(tr("Overlay text"));
    ui->colorSelectorScrollbar->setDescription(tr("Scrollbars"));
    ui->patternColorButton->setDescription(tr("Background pattern color"));

    connect(ui->patternSizeSlider, &QSlider::valueChanged, [this](int value) {
        ui->patternSizeValueLabel->setText(QString::number(value) + " px");
    });
    connect(ui->patternOpacitySlider, &QSlider::valueChanged, [this](int value) {
        ui->patternOpacityValueLabel->setText(QString::number(value) + "%");
    });
    connect(ui->patternComboBox, qOverload<int>(&QComboBox::currentIndexChanged), [this](int index) {
        bool enabled = (index != BG_PATTERN_NONE);
        ui->patternSizeSlider->setEnabled(enabled);
        ui->patternOpacitySlider->setEnabled(enabled);
        ui->patternAutoColorCheckBox->setEnabled(enabled);
        ui->patternColorButton->setEnabled(enabled && !ui->patternAutoColorCheckBox->isChecked());
    });
    connect(ui->patternAutoColorCheckBox, &QCheckBox::toggled, [this](bool useTheme) {
        ui->patternColorButton->setEnabled(ui->patternComboBox->currentIndex() != BG_PATTERN_NONE && !useTheme);
    });

#ifndef USE_KDE_BLUR
    ui->blurBackgroundCheckBox->setEnabled(false);
#endif

#ifndef USE_MPV
    // the choice is still stored (and togglable): it takes effect in a build with a video player
    {
        QLabel *noVideoLabel = new QLabel(tr("This build has no video player (libmpv). The setting is saved, "
                                             "but videos only play in a build made with video support."), ui->videoPlaybackGroup);
        noVideoLabel->setWordWrap(true);
        noVideoLabel->setAccessibleName("SettingsNote");
        ui->videoPlaybackGroup->layout()->addWidget(noVideoLabel);
    }
#endif
    setupExtraControls();

#ifdef USE_OPENCV
    ui->scalingQualityComboBox->addItem("Bilinear+sharpen (OpenCV)");
    ui->scalingQualityComboBox->addItem("Bicubic (OpenCV)");
    ui->scalingQualityComboBox->addItem("Bicubic+sharpen (OpenCV)");
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    ui->memoryLimitSpinBox->setEnabled(false);
    ui->memoryLimitLabel->setEnabled(false);
#endif

    if(!settings->supportedFormats().contains("jxl"))
        ui->animatedJxlCheckBox->hide();

    setupSidebar();

    // setup radioBtn groups
    fitModeGrp.addButton(ui->fitModeWindow);
    fitModeGrp.addButton(ui->fitModeWidth);
    fitModeGrp.addButton(ui->fitMode1to1);
    fitModeGrp.addButton(ui->fitModeWindowStretch);
    folderEndGrp.addButton(ui->folderEndSwitchFolder);
    folderEndGrp.addButton(ui->folderEndNoAction);
    folderEndGrp.addButton(ui->folderEndLoop);
    zoomIndGrp.addButton(ui->zoomIndicatorAuto);
    zoomIndGrp.addButton(ui->zoomIndicatorOff);
    zoomIndGrp.addButton(ui->zoomIndicatorOn);

    // readable language names
    langs.insert("de_DE", "Deutsch");
    langs.insert("en_US", "English");
    langs.insert("es_ES", "Español");
    langs.insert("fr_FR", "Français");
    langs.insert("ja_JP", "日本語");
    langs.insert("tr_TR", "Türkçe");
    langs.insert("uk_UA", "Українська");
    langs.insert("zh_CN", "简体中文");
    // fill langs combobox, sorted by locale
    ui->langComboBox->addItems(langs.values());
    // insert system language entry manually at the beginning
    langs.insert("system", "System language");
    ui->langComboBox->insertItem(0, "System language");

    connect(this, &SettingsDialog::settingsChanged, settings, &Settings::sendChangeNotification);
    readSettings();

    adjustSizeToContents();
}
//------------------------------------------------------------------------------
SettingsDialog::~SettingsDialog() {
    delete ui;
}
//------------------------------------------------------------------------------
// an attempt to force minimum width to fit contents
void SettingsDialog::adjustSizeToContents() {
    // general tab
    ui->gridLayout->activate();
    ui->horizontalLayout_28->activate();
    ui->horizontalLayout_19->activate();
    ui->gridLayout_3->activate();
    ui->horizontalLayout_18->activate();
    ui->gridLayout_4->activate();
    ui->horizontalLayout_24->activate();
    ui->gridLayout_5->activate();
    ui->slideshowGroupContents->activate();
    ui->scrollAreaWidgetContents->layout()->activate();
    ui->scrollArea->setMinimumWidth(ui->scrollAreaWidgetContents->minimumSizeHint().width());
    // view tab
    ui->horizontalLayout_29->activate();
    ui->horizontalLayout_31->activate();
    ui->widget->layout()->activate();
    ui->scrollAreaWidgetContents_3->layout()->activate();
    ui->scrollArea_3->setMinimumWidth(ui->scrollAreaWidgetContents_3->minimumSizeHint().width());
    // container
    //ui->stackedWidget->layout()->activate();
    this->setMinimumWidth(sizeHint().width() + 22);

    //qDebug() << "window:" << this->sizeHint() << this->minimumSizeHint() << this->size();
    //qDebug() << "stackedwidget:" << ui->stackedWidget->sizeHint() << ui->stackedWidget->minimumSizeHint() << ui->stackedWidget->size();
    //qDebug() << "scrollarea:" << ui->scrollArea->sizeHint() << ui->scrollArea->minimumSizeHint() << ui->scrollArea->size();
    //qDebug() << "scrollareawidget:" << ui->scrollAreaWidgetContents->sizeHint() << ui->scrollAreaWidgetContents->minimumSizeHint() << ui->scrollAreaWidgetContents->size();
    //qDebug() << "grid" << ui->gridLayout_15->sizeHint();
    //qDebug() << "wtf" << ui->startInFolderViewCheckBox->sizeHint() << ui->startInFolderViewCheckBox->minimumSizeHint();
}
//------------------------------------------------------------------------------
void SettingsDialog::resetToDesktopTheme() {
    settings->setColorScheme(ThemeStore::colorScheme(ColorSchemes::COLORS_SYSTEM));
    this->readColorScheme();
}
//------------------------------------------------------------------------------
void SettingsDialog::setupSidebar() {

}
//------------------------------------------------------------------------------
// General page additions that are easier to build in code than in the .ui:
// slideshow transition / caption options and the interface font group
void SettingsDialog::setupExtraControls() {
    // --- slideshow -----------------------------------------------------------
    ui->slideshowIntervalSpinBox->setRange(500, 120000);
    ui->slideshowIntervalSpinBox->setSingleStep(500);
    ui->slideshowIntervalSpinBox->setValue(3000);
    QHBoxLayout *slideRow = new QHBoxLayout();
    slideRow->setContentsMargins(0, 0, 0, 0);
    slideRow->setSpacing(7);
    slideRow->addWidget(new QLabel(tr("Transition:"), ui->slideshowGroup));
    slideTransitionComboBox = new QComboBox(ui->slideshowGroup);
    slideTransitionComboBox->addItems(SlideTransition::styleNames());
    slideRow->addWidget(slideTransitionComboBox);
    slideRow->addSpacing(10);
    slideShowNameCheckBox = new QCheckBox(tr("Show file name"), ui->slideshowGroup);
    slideShowDateCheckBox = new QCheckBox(tr("Show date"), ui->slideshowGroup);
    slideRow->addWidget(slideShowNameCheckBox);
    slideRow->addWidget(slideShowDateCheckBox);
    slideRow->addStretch(1);
    if(QBoxLayout *box = qobject_cast<QBoxLayout*>(ui->slideshowGroup->layout()))
        box->addLayout(slideRow);

    // --- interface font --------------------------------------------------------
    QWidget *fontGroup = new QWidget(ui->scrollAreaWidgetContents);
    fontGroup->setAccessibleName("SGroup");
    QVBoxLayout *fontLayout = new QVBoxLayout(fontGroup);
    fontLayout->setContentsMargins(13, 10, 13, 10);
    fontLayout->setSpacing(7);
    QLabel *fontTitle = new QLabel(tr("Interface font"), fontGroup);
    QFont bold = fontTitle->font();
    bold.setBold(true);
    fontTitle->setFont(bold);
    fontLayout->addWidget(fontTitle);

    QHBoxLayout *fontRow = new QHBoxLayout();
    fontRow->setSpacing(7);
    fontComboBox = new QFontComboBox(fontGroup);
    fontComboBox->setEditable(false);
    fontComboBox->setMinimumWidth(160);
    fontComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fontMonospaceCheckBox = new QCheckBox(tr("Monospaced only"), fontGroup);
    QPushButton *fontResetButton = new QPushButton(tr("Reset"), fontGroup);
    fontResetButton->setToolTip(tr("Back to the default font (Consolas)"));
    fontRow->addWidget(fontComboBox, 1);
    fontRow->addWidget(fontMonospaceCheckBox);
    fontRow->addWidget(fontResetButton);
    fontLayout->addLayout(fontRow);

    fontPreviewLabel = new QLabel(tr("The quick brown fox jumps over the lazy dog  0123456789"), fontGroup);
    fontPreviewLabel->setWordWrap(true);
    fontLayout->addWidget(fontPreviewLabel);

    connect(fontComboBox, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) {
        QFont preview = fontPreviewLabel->font();
        preview.setFamily(font.family());
        fontPreviewLabel->setFont(preview);
    });
    connect(fontMonospaceCheckBox, &QCheckBox::toggled, this, [this](bool mono) {
        QString current = fontComboBox->currentFont().family();
        fontComboBox->setFontFilters(mono ? QFontComboBox::MonospacedFonts : QFontComboBox::AllFonts);
        fontComboBox->setCurrentFont(QFont(current));
    });
    connect(fontResetButton, &QPushButton::clicked, this, [this]() {
        fontComboBox->setCurrentFont(QFont(Settings::defaultFontFamilies().first()));
    });

    // right after the slideshow group (keeps the page order: behaviour first, looks last)
    if(QBoxLayout *page = qobject_cast<QBoxLayout*>(ui->scrollAreaWidgetContents->layout())) {
        int index = page->indexOf(ui->slideshowGroup);
        if(index < 0)
            index = page->count() - 1;
        page->insertSpacing(index + 1, 12);
        page->insertWidget(index + 2, fontGroup);
    }

    // --- rulers & guides (View page) ----------------------------------------------
    QWidget *rulerGroup = new QWidget(ui->scrollAreaWidgetContents_3);
    rulerGroup->setAccessibleName("SGroup");
    QVBoxLayout *rulerLayout = new QVBoxLayout(rulerGroup);
    rulerLayout->setContentsMargins(13, 10, 13, 10);
    rulerLayout->setSpacing(7);
    QLabel *rulerTitle = new QLabel(tr("Rulers & guides"), rulerGroup);
    rulerTitle->setFont(bold);
    rulerLayout->addWidget(rulerTitle);

    rulersCheckBox = new QCheckBox(tr("Show rulers (image view, collage view and collage editor)"), rulerGroup);
    rulersCheckBox->setToolTip(tr("Drag from a ruler onto the canvas to add a guide line. Drag a guide back onto a ruler to remove it."));
    rulerLayout->addWidget(rulersCheckBox);

    QWidget *rulerOptions = new QWidget(rulerGroup);
    QVBoxLayout *rulerOptionsLayout = new QVBoxLayout(rulerOptions);
    rulerOptionsLayout->setContentsMargins(0, 0, 0, 0);
    rulerOptionsLayout->setSpacing(7);

    QHBoxLayout *unitRow = new QHBoxLayout();
    unitRow->setSpacing(7);
    unitRow->addWidget(new QLabel(tr("Units:"), rulerOptions));
    rulerUnitComboBox = new QComboBox(rulerOptions);
    rulerUnitComboBox->addItems({ tr("Pixels"), tr("Centimeters"), tr("Inches"), tr("Mixed") });
    rulerUnitComboBox->setToolTip(tr("Mixed: pixels on the outer scale, centimeters or inches on the inner scale"));
    unitRow->addWidget(rulerUnitComboBox);
    unitRow->addSpacing(10);
    QLabel *mixedLabel = new QLabel(tr("Mixed inner scale:"), rulerOptions);
    unitRow->addWidget(mixedLabel);
    rulerMixedComboBox = new QComboBox(rulerOptions);
    rulerMixedComboBox->addItems({ tr("Centimeters"), tr("Inches") });
    unitRow->addWidget(rulerMixedComboBox);
    unitRow->addStretch(1);
    rulerOptionsLayout->addLayout(unitRow);

    QHBoxLayout *dpiRow = new QHBoxLayout();
    dpiRow->setSpacing(7);
    dpiRow->addWidget(new QLabel(tr("Fallback DPI:"), rulerOptions));
    rulerDpiSpinBox = new QSpinBox(rulerOptions);
    rulerDpiSpinBox->setRange(30, 2400);
    rulerDpiSpinBox->setValue(96);
    rulerDpiSpinBox->setToolTip(tr("Used to convert pixels to centimeters / inches when the image has no DPI of its own. "
                                   "The collage always uses this value."));
    dpiRow->addWidget(rulerDpiSpinBox);
    dpiRow->addSpacing(10);
    rulerImageDpiCheckBox = new QCheckBox(tr("Use the image's DPI when available"), rulerOptions);
    dpiRow->addWidget(rulerImageDpiCheckBox);
    dpiRow->addStretch(1);
    rulerOptionsLayout->addLayout(dpiRow);

    QHBoxLayout *snapRow = new QHBoxLayout();
    snapRow->setSpacing(7);
    rulerSnapCheckBox = new QCheckBox(tr("Snap guides to multiples of"), rulerOptions);
    rulerSnapComboBox = new QComboBox(rulerOptions);
    rulerSnapComboBox->setEditable(true);
    rulerSnapComboBox->setValidator(new QIntValidator(1, 10000, rulerSnapComboBox));
    rulerSnapComboBox->addItems({ "1", "2", "5", "10", "20", "25", "50", "100" });
    rulerSnapComboBox->setToolTip(tr("1 = whole pixels, 2 = even numbers, 5, 10, ... (in image / canvas pixels)"));
    snapRow->addWidget(rulerSnapCheckBox);
    snapRow->addWidget(rulerSnapComboBox);
    snapRow->addWidget(new QLabel(tr("px"), rulerOptions));
    snapRow->addStretch(1);
    rulerOptionsLayout->addLayout(snapRow);
    connect(rulerSnapCheckBox, &QCheckBox::toggled, rulerSnapComboBox, &QWidget::setEnabled);

    QHBoxLayout *guideRow = new QHBoxLayout();
    guideRow->setSpacing(7);
    QPushButton *clearGuidesButton = new QPushButton(tr("Clear saved guides"), rulerOptions);
    guideRow->addWidget(clearGuidesButton);
    guideRow->addStretch(1);
    rulerOptionsLayout->addLayout(guideRow);
    rulerLayout->addWidget(rulerOptions);

    QLabel *rulerNote = new QLabel(tr("Toggle with Ctrl+Shift+R (rebind it in Controls). Double-click a ruler for exact guide positions and snapping, right-click it for quick unit changes."), rulerGroup);
    rulerNote->setWordWrap(true);
    rulerNote->setAccessibleName("SettingsNote");
    rulerLayout->addWidget(rulerNote);

    connect(rulersCheckBox, &QCheckBox::toggled, rulerOptions, &QWidget::setEnabled);
    auto syncMixed = [this, mixedLabel]() {
        bool mixed = (rulerUnitComboBox->currentIndex() == RULER_MIXED);
        mixedLabel->setEnabled(mixed);
        rulerMixedComboBox->setEnabled(mixed);
    };
    connect(rulerUnitComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, syncMixed);
    connect(clearGuidesButton, &QPushButton::clicked, this, []() {
        settings->setRulerGuides("image", QStringList());
        settings->setRulerGuides("collage", QStringList());
        settings->sendChangeNotification();
    });
    syncMixed();
    rulerOptions->setEnabled(rulersCheckBox->isChecked());

    if(QBoxLayout *page = qobject_cast<QBoxLayout*>(ui->scrollAreaWidgetContents_3->layout())) {
        int index = page->count() - 1; // before the trailing stretch
        page->insertSpacing(index, 12);
        page->insertWidget(index + 1, rulerGroup);
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::readSettings() {
    ui->loopSlideshowCheckBox->setChecked(settings->loopSlideshow());
    ui->videoPlaybackCheckBox->setChecked(settings->videoPlaybackEnabled());
    ui->videoPlaybackGroupContents->setEnabled(settings->videoPlaybackEnabled());
    slideTransitionComboBox->setCurrentIndex(settings->slideshowTransition());
    slideShowNameCheckBox->setChecked(settings->slideshowShowName());
    slideShowDateCheckBox->setChecked(settings->slideshowShowDate());
    rulersCheckBox->setChecked(settings->rulersEnabled());
    rulerUnitComboBox->setCurrentIndex(settings->rulerUnit());
    rulerMixedComboBox->setCurrentIndex(settings->rulerMixedUnit() == RULER_IN ? 1 : 0);
    rulerDpiSpinBox->setValue(settings->rulerDpi());
    rulerImageDpiCheckBox->setChecked(settings->rulerUseImageDpi());
    rulerSnapCheckBox->setChecked(settings->rulerSnap());
    rulerSnapComboBox->setCurrentText(QString::number(settings->rulerSnapStep()));
    rulerSnapComboBox->setEnabled(rulerSnapCheckBox->isChecked());
    {
        QString family = settings->interfaceFont();
        if(family.isEmpty() || !Settings::fontInstalled(family))
            family = QApplication::font().family();
        fontComboBox->setCurrentFont(QFont(family));
        loadedFontFamily = fontComboBox->currentFont().family();
    }
    ui->playSoundsCheckBox->setChecked(settings->playVideoSounds());
    ui->allowMp4CheckBox->setChecked(settings->allowMp4());
    ui->enablePanelCheckBox->setChecked(settings->panelEnabled());
    ui->thumbnailPanelGroupContents->setEnabled(settings->panelEnabled());
    ui->panelFullscreenOnlyCheckBox->setChecked(settings->panelFullscreenOnly());
    ui->squareThumbnailsCheckBox->setChecked(settings->squareThumbnails());
    ui->transparencyGridCheckBox->setChecked(settings->transparencyGrid());
    ui->enableSmoothScrollCheckBox->setChecked(settings->enableSmoothScroll());
    ui->usePreloaderCheckBox->setChecked(settings->usePreloader());
    ui->useThumbnailCacheCheckBox->setChecked(settings->useThumbnailCache());
    ui->smoothUpscalingCheckBox->setChecked(settings->smoothUpscaling());
    ui->expandImageCheckBox->setChecked(settings->expandImage());
    ui->expandImagesGroupContents->setEnabled(settings->expandImage());
    ui->smoothAnimatedImagesCheckBox->setChecked(settings->smoothAnimatedImages());
    ui->bgOpacitySlider->setValue(static_cast<int>(settings->backgroundOpacity() * 100));
    ui->blurBackgroundCheckBox->setChecked(settings->blurBackground());
    // background pattern
    QColor patternColor = settings->patternColor();
    bool patternAutoColor = !patternColor.isValid();
    if(patternAutoColor)
        patternColor = settings->colorScheme().text;
    ui->patternColorButton->setColor(patternColor);
    ui->patternAutoColorCheckBox->setChecked(patternAutoColor);
    ui->patternSizeSlider->setValue(settings->patternSize());
    ui->patternOpacitySlider->setValue(settings->patternOpacity());
    ui->patternSizeValueLabel->setText(QString::number(settings->patternSize()) + " px");
    ui->patternOpacityValueLabel->setText(QString::number(settings->patternOpacity()) + "%");
    // set last so the enable/disable handler sees the final state
    ui->patternComboBox->setCurrentIndex(-1);
    ui->patternComboBox->setCurrentIndex(settings->backgroundPattern());
    ui->sortingComboBox->setCurrentIndex(settings->sortingMode());
    ui->confirmDeleteCheckBox->setChecked(settings->confirmDelete());
    ui->confirmTrashCheckBox->setChecked(settings->confirmTrash());
    ui->unlockMinZoomCheckBox->setChecked(settings->unlockMinZoom());
    ui->sortFoldersCheckBox->setChecked(settings->sortFolders());
    ui->trackpadDetectionCheckBox->setChecked(settings->trackpadDetection());
    ui->clickableEdgesCheckBox->setChecked(settings->clickableEdges());
    ui->clickableEdgesVisibleCheckBox->setChecked(settings->clickableEdgesVisible());
    ui->clickableEdgesVisibleCheckBox->setEnabled(settings->clickableEdges());
    ui->showHiddenFilesCheckBox->setChecked(settings->showHiddenFiles());

    if(settings->zoomIndicatorMode() == INDICATOR_ENABLED)
        ui->zoomIndicatorOn->setChecked(true);
    else if(settings->zoomIndicatorMode() == INDICATOR_AUTO)
        ui->zoomIndicatorAuto->setChecked(true);
    else
        ui->zoomIndicatorOff->setChecked(true);
    ui->showInfoBarFullscreen->setChecked(settings->infoBarFullscreen());
    ui->showInfoBarWindowed->setChecked(settings->infoBarWindowed());
    ui->showTopBar->setChecked(settings->topBarEnabled());
    ui->showTopBarPerformance->setChecked(settings->topBarPerformance());
    ui->showTopBarPerformance->setEnabled(ui->showTopBar->isChecked());
    ui->showExtendedInfoTitle->setChecked(settings->windowTitleExtendedInfo());
    ui->cursorAutohideCheckBox->setChecked(settings->cursorAutohide());
    ui->keepFitModeCheckBox->setChecked(settings->keepFitMode());
    if(settings->focusPointIn1to1Mode() == FOCUS_TOP)
        ui->focus1to1Top->setChecked(true);
    else if(settings->focusPointIn1to1Mode() == FOCUS_CENTER)
        ui->focus1to1Center->setChecked(true);
    else
        ui->focus1to1Cursor->setChecked(true);
    ui->slideshowIntervalSpinBox->setValue(settings->slideshowInterval());
    ui->imageScrollingComboBox->setCurrentIndex(settings->imageScrolling());
    ui->saveOverlayCheckBox->setChecked(settings->showSaveOverlay());
    ui->unloadThumbsCheckBox->setChecked(settings->unloadThumbs());
    if(settings->thumbPanelStyle() == TH_PANEL_SIMPLE)
        ui->thumbStyleSimple->setChecked(true);
    else
        ui->thumbStyleExtended->setChecked(true);
    ui->animatedJxlCheckBox->setChecked(settings->jxlAnimation());
    ui->autoResizeWindowCheckBox->setChecked(settings->autoResizeWindow());
    ui->panelCenterSelectionCheckBox->setChecked(settings->panelCenterSelection());
    ui->useFixedZoomLevelsCheckBox->setChecked(settings->useFixedZoomLevels());
    ui->zoomLevels->setText(settings->zoomLevels());

    if(settings->defaultViewMode() == MODE_FOLDERVIEW)
        ui->startInFolderViewCheckBox->setChecked(true);
    else
        ui->startInFolderViewCheckBox->setChecked(false);

    if(settings->folderEndAction() == FOLDER_END_NO_ACTION)
        ui->folderEndNoAction->setChecked(true);
    else if(settings->folderEndAction() == FOLDER_END_LOOP)
        ui->folderEndLoop->setChecked(true);
    else
        ui->folderEndSwitchFolder->setChecked(true);

    ui->mpvLineEdit->setText(settings->mpvBinary());

    ui->zoomStepSlider->setValue(static_cast<int>(settings->zoomStep() * 100.f));
    onZoomStepSliderChanged(ui->zoomStepSlider->value());

    ui->mouseScrollingSpeedSlider->setValue(static_cast<int>((settings->mouseScrollingSpeed() - 0.5f) / 0.25f));
    onMouseScrollingSpeedSliderChanged(ui->mouseScrollingSpeedSlider->value());

    ui->autoResizeLimitSlider->setValue(static_cast<int>(settings->autoResizeLimit() / 5.f));
    onAutoResizeLimitSliderChanged(ui->autoResizeLimitSlider->value());

    ui->JPEGQualitySlider->setValue(settings->JPEGSaveQuality());
    onJPEGQualitySliderChanged(ui->JPEGQualitySlider->value());

    ui->expandLimitSlider->setValue(settings->expandLimit());
    onExpandLimitSliderChanged(ui->expandLimitSlider->value());

    // thumbnailer threads
    ui->thumbnailerThreadsSlider->setValue(settings->thumbnailerThreadCount());
    onThumbnailerThreadsSliderChanged(ui->thumbnailerThreadsSlider->value());

    ui->memoryLimitSpinBox->setValue(settings->memoryAllocationLimit());

    // language
    QString langName = langs.value(settings->language());
    if(langName.isEmpty() || ui->langComboBox->findText(langName) == -1)
        ui->langComboBox->setCurrentText("en_US");
    else
        ui->langComboBox->setCurrentText(langName);

    // ##### fit mode #####
    if(settings->imageFitMode() == FIT_WINDOW)
        ui->fitModeWindow->setChecked(true);
    else if(settings->imageFitMode() == FIT_WIDTH)
        ui->fitModeWidth->setChecked(true);
    else if(settings->imageFitMode() == FIT_WINDOW_STRETCH)
        ui->fitModeWindowStretch->setChecked(true);
    else
        ui->fitMode1to1->setChecked(true);

    // ##### UI #####
    ui->scalingQualityComboBox->setCurrentIndex(settings->scalingFilter());
    ui->fullscreenCheckBox->setChecked(settings->fullscreenMode());
    ui->pinPanelCheckBox->setChecked(settings->panelPinned());
    ui->panelPositionComboBox->setCurrentIndex(settings->panelPosition());

    // reduce by 10x to have nice granular control in qslider
    ui->panelSizeSlider->setValue(settings->panelPreviewsSize() / 10);

    ui->useSystemColorsCheckBox->setChecked(settings->useSystemColorScheme());
    ui->modifySystemSchemeLabel->setVisible(settings->useSystemColorScheme());
    ui->themeSelectorComboBox->setEnabled(!settings->useSystemColorScheme());
    ui->colorConfigSubgroup->setEnabled(!settings->useSystemColorScheme());
    
    readColorScheme();
    readShortcuts();
    readScripts();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettings() {
    // wait for all background stuff to finish
    if(QThreadPool::globalInstance()->activeThreadCount()) {
        QThreadPool::globalInstance()->waitForDone();
    }

    settings->setLoopSlideshow(ui->loopSlideshowCheckBox->isChecked());
    settings->setFullscreenMode(ui->fullscreenCheckBox->isChecked());
    if(ui->fitModeWindow->isChecked())
        settings->setImageFitMode(FIT_WINDOW);
    else if(ui->fitModeWidth->isChecked())
        settings->setImageFitMode(FIT_WIDTH);
    else if(ui->fitModeWindowStretch->isChecked())
        settings->setImageFitMode(FIT_WINDOW_STRETCH);
    else
        settings->setImageFitMode(FIT_ORIGINAL);

    settings->setLanguage(langs.key(ui->langComboBox->currentText()));

    settings->setVideoPlayback(ui->videoPlaybackCheckBox->isChecked());
    settings->setPlayVideoSounds(ui->playSoundsCheckBox->isChecked());
    settings->setAllowMp4(ui->allowMp4CheckBox->isChecked());
    settings->setPanelEnabled(ui->enablePanelCheckBox->isChecked());
    settings->setPanelFullscreenOnly(ui->panelFullscreenOnlyCheckBox->isChecked());
    settings->setSquareThumbnails(ui->squareThumbnailsCheckBox->isChecked());
    settings->setTransparencyGrid(ui->transparencyGridCheckBox->isChecked());
    settings->setShowHiddenFiles(ui->showHiddenFilesCheckBox->isChecked());
    settings->setEnableSmoothScroll(ui->enableSmoothScrollCheckBox->isChecked());
    settings->setUsePreloader(ui->usePreloaderCheckBox->isChecked());
    settings->setUseThumbnailCache(ui->useThumbnailCacheCheckBox->isChecked());
    settings->setSmoothUpscaling(ui->smoothUpscalingCheckBox->isChecked());
    settings->setExpandImage(ui->expandImageCheckBox->isChecked());
    settings->setSmoothAnimatedImages(ui->smoothAnimatedImagesCheckBox->isChecked());

    settings->setBackgroundOpacity(static_cast<qreal>(ui->bgOpacitySlider->value()) / 100.f);
    settings->setBlurBackground(ui->blurBackgroundCheckBox->isChecked());
    settings->setBackgroundPattern(static_cast<BackgroundPattern>(qMax(0, ui->patternComboBox->currentIndex())));
    settings->setPatternSize(ui->patternSizeSlider->value());
    settings->setPatternOpacity(ui->patternOpacitySlider->value());
    settings->setPatternColor(ui->patternAutoColorCheckBox->isChecked() ? QColor() : ui->patternColorButton->color());
    settings->setSortingMode(static_cast<SortingMode>(ui->sortingComboBox->currentIndex()));
    settings->setConfirmDelete(ui->confirmDeleteCheckBox->isChecked());
    settings->setConfirmTrash(ui->confirmTrashCheckBox->isChecked());
    settings->setUnlockMinZoom(ui->unlockMinZoomCheckBox->isChecked());
    settings->setSortFolders(ui->sortFoldersCheckBox->isChecked());
    settings->setTrackpadDetection(ui->trackpadDetectionCheckBox->isChecked());
    settings->setClickableEdges(ui->clickableEdgesCheckBox->isChecked());
    settings->setClickableEdgesVisible(ui->clickableEdgesVisibleCheckBox->isChecked());

    if(ui->zoomIndicatorOn->isChecked())
        settings->setZoomIndicatorMode(INDICATOR_ENABLED);
    else if(ui->zoomIndicatorAuto->isChecked())
        settings->setZoomIndicatorMode(INDICATOR_AUTO);
    else
        settings->setZoomIndicatorMode(INDICATOR_DISABLED);
    settings->setInfoBarFullscreen(ui->showInfoBarFullscreen->isChecked());
    settings->setInfoBarWindowed(ui->showInfoBarWindowed->isChecked());
    settings->setTopBarEnabled(ui->showTopBar->isChecked());
    settings->setTopBarPerformance(ui->showTopBarPerformance->isChecked());
    settings->setWindowTitleExtendedInfo(ui->showExtendedInfoTitle->isChecked());
    settings->setCursorAutohide(ui->cursorAutohideCheckBox->isChecked());
    settings->setKeepFitMode(ui->keepFitModeCheckBox->isChecked());
    if(ui->focus1to1Top->isChecked())
        settings->setFocusPointIn1to1Mode(FOCUS_TOP);
    else if(ui->focus1to1Center->isChecked())
        settings->setFocusPointIn1to1Mode(FOCUS_CENTER);
    else
        settings->setFocusPointIn1to1Mode(FOCUS_CURSOR);

    settings->setSlideshowInterval(ui->slideshowIntervalSpinBox->value());
    settings->setSlideshowTransition(slideTransitionComboBox->currentIndex());
    settings->setSlideshowShowName(slideShowNameCheckBox->isChecked());
    settings->setSlideshowShowDate(slideShowDateCheckBox->isChecked());
    settings->setRulersEnabled(rulersCheckBox->isChecked());
    settings->setRulerUnit(rulerUnitComboBox->currentIndex());
    settings->setRulerMixedUnit(rulerMixedComboBox->currentIndex() == 1 ? RULER_IN : RULER_CM);
    settings->setRulerDpi(rulerDpiSpinBox->value());
    settings->setRulerUseImageDpi(rulerImageDpiCheckBox->isChecked());
    settings->setRulerSnap(rulerSnapCheckBox->isChecked());
    {
        bool ok = false;
        int step = rulerSnapComboBox->currentText().toInt(&ok);
        if(ok)
            settings->setRulerSnapStep(step);
    }

    // interface font: applied right away, the stylesheet is rebuilt from the new metrics
    {
        QString family = fontComboBox->currentFont().family();
        if(family.compare(loadedFontFamily, Qt::CaseInsensitive) != 0) {
            loadedFontFamily = family;
            settings->setInterfaceFont(family);
            Settings::applyInterfaceFont(family);
            settings->loadStylesheet();
        }
    }

    if(ui->startInFolderViewCheckBox->isChecked())
        settings->setDefaultViewMode(MODE_FOLDERVIEW);
    else
        settings->setDefaultViewMode(MODE_DOCUMENT);

    if(ui->folderEndNoAction->isChecked())
        settings->setFolderEndAction(FOLDER_END_NO_ACTION);
    else if(ui->folderEndLoop->isChecked())
        settings->setFolderEndAction(FOLDER_END_LOOP);
    else
        settings->setFolderEndAction(FOLDER_END_GOTO_ADJACENT);

    settings->setMpvBinary(ui->mpvLineEdit->text());
    settings->setScalingFilter(static_cast<ScalingFilter>(ui->scalingQualityComboBox->currentIndex()));
    settings->setImageScrolling(static_cast<ImageScrolling>(ui->imageScrollingComboBox->currentIndex()));
    settings->setShowSaveOverlay(ui->saveOverlayCheckBox->isChecked());
    settings->setUnloadThumbs(ui->unloadThumbsCheckBox->isChecked());
    if(ui->thumbStyleSimple->isChecked())
        settings->setThumbPanelStyle(TH_PANEL_SIMPLE);
    else
        settings->setThumbPanelStyle(TH_PANEL_EXTENDED);
    settings->setJxlAnimation(ui->animatedJxlCheckBox->isChecked());
    settings->setAutoResizeWindow(ui->autoResizeWindowCheckBox->isChecked());
    settings->setPanelCenterSelection(ui->panelCenterSelectionCheckBox->isChecked());
    settings->setUseFixedZoomLevels(ui->useFixedZoomLevelsCheckBox->isChecked());
    settings->setZoomLevels(ui->zoomLevels->text());

    settings->setPanelPinned(ui->pinPanelCheckBox->isChecked());
    int panelPos = ui->panelPositionComboBox->currentIndex();
    settings->setPanelPosition(static_cast<PanelPosition>(panelPos));

    settings->setPanelPreviewsSize(ui->panelSizeSlider->value() * 10);

    settings->setJPEGSaveQuality(ui->JPEGQualitySlider->value());
    settings->setZoomStep(static_cast<qreal>(ui->zoomStepSlider->value() / 100.f));
    settings->setMouseScrollingSpeed(static_cast<qreal>(0.5f + (ui->mouseScrollingSpeedSlider->value() * 0.25f)));
    settings->setAutoResizeLimit(ui->autoResizeLimitSlider->value() * 5);
    settings->setExpandLimit(ui->expandLimitSlider->value());
    settings->setThumbnailerThreadCount(ui->thumbnailerThreadsSlider->value());
    settings->setMemoryAllocationLimit(ui->memoryLimitSpinBox->value());

    settings->setUseSystemColorScheme(ui->useSystemColorsCheckBox->isChecked());

    saveColorScheme();
    saveShortcuts();

    scriptManager->saveScripts();
    actionManager->saveShortcuts();
    emit settingsChanged();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettingsAndClose() {
    saveSettings();
    this->close();
}
//------------------------------------------------------------------------------
void SettingsDialog::readColorScheme() {
    auto colors = settings->colorScheme();
    setColorScheme(colors);
}

void SettingsDialog::setColorScheme(ColorScheme colors) {
    switch (colors.tid) {
        case COLORS_LIGHT: ui->themeSelectorComboBox->setCurrentIndex(3);   break;
        case COLORS_BLACK: ui->themeSelectorComboBox->setCurrentIndex(0);   break;
        case COLORS_DARK: ui->themeSelectorComboBox->setCurrentIndex(1);    break;
        case COLORS_DARKBLUE: ui->themeSelectorComboBox->setCurrentIndex(2);break;
        default: ui->themeSelectorComboBox->setCurrentIndex(-1);            break;
    }
    ui->colorSelectorAccent->setColor(colors.accent);
    ui->colorSelectorBackground->setColor(colors.background);
    ui->colorSelectorFullscreen->setColor(colors.background_fullscreen);
    ui->colorSelectorFolderview->setColor(colors.folderview);
    ui->colorSelectorFolderviewPanel->setColor(colors.folderview_topbar);
    ui->colorSelectorText->setColor(colors.text);
    ui->colorSelectorIcons->setColor(colors.icons);
    ui->colorSelectorWidget->setColor(colors.widget);
    ui->colorSelectorWidgetBorder->setColor(colors.widget_border);
    ui->colorSelectorOverlay->setColor(colors.overlay);
    ui->colorSelectorOverlayText->setColor(colors.overlay_text);
    ui->colorSelectorScrollbar->setColor(colors.scrollbar);
}

//------------------------------------------------------------------------------
void SettingsDialog::saveColorScheme() {
    BaseColorScheme base;
    base.accent = ui->colorSelectorAccent->color();
    base.background = ui->colorSelectorBackground->color();
    base.background_fullscreen = ui->colorSelectorFullscreen->color();
    base.folderview = ui->colorSelectorFolderview->color();
    base.folderview_topbar = ui->colorSelectorFolderviewPanel->color();
    base.text = ui->colorSelectorText->color();
    base.icons = ui->colorSelectorIcons->color();
    base.widget = ui->colorSelectorWidget->color();
    base.widget_border = ui->colorSelectorWidgetBorder->color();
    base.overlay = ui->colorSelectorOverlay->color();
    base.overlay_text = ui->colorSelectorOverlayText->color();
    base.scrollbar = ui->colorSelectorScrollbar->color();
    base.tid = settings->colorScheme().tid;
    settings->setColorScheme(ColorScheme(base));
}
//------------------------------------------------------------------------------
void SettingsDialog::readShortcuts() {
    ui->shortcutsTableWidget->clearContents();
    ui->shortcutsTableWidget->setRowCount(0);
    const QMap<QString, QString> shortcuts = actionManager->allShortcuts();
    QMapIterator<QString, QString> i(shortcuts);
    while(i.hasNext()) {
        i.next();
        addShortcutToTable(i.value(), i.key());
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::readScripts() {
    ui->scriptsListWidget->clear();
    const QMap<QString, Script> scripts = scriptManager->allScripts();
    QMapIterator<QString, Script> i(scripts);
    while(i.hasNext()) {
        i.next();
        addScriptToList(i.key());
    }
}
//------------------------------------------------------------------------------
// does not check if the shortcut already there
void SettingsDialog::addScriptToList(const QString &name) {
    if(name.isEmpty())
        return;

    QListWidget *list = ui->scriptsListWidget;
    QListWidgetItem *nameItem = new QListWidgetItem(name);
    nameItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    list->insertItem(ui->scriptsListWidget->count(), nameItem);
    list->sortItems(Qt::AscendingOrder);
}
//------------------------------------------------------------------------------
void SettingsDialog::addScript() {
    ScriptEditorDialog w;
    if(w.exec()) {
        if(w.scriptName().isEmpty())
            return;
        scriptManager->addScript(w.scriptName(), w.script());
        readScripts();
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript() {
    int row = ui->scriptsListWidget->currentRow();
    if(row >= 0) {
        QString name = ui->scriptsListWidget->currentItem()->text();
        editScript(name);
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript(QListWidgetItem* item) {
    if(item) {
        editScript(item->text());
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript(QString name) {
    ScriptEditorDialog w(name, scriptManager->getScript(name));
    if(w.exec()) {
        if(w.scriptName().isEmpty())
            return;
        scriptManager->addScript(w.scriptName(), w.script());
        readScripts();
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::removeScript() {
    int row = ui->scriptsListWidget->currentRow();
    if(row >= 0) {
        QString scriptName = ui->scriptsListWidget->currentItem()->text();
        delete ui->scriptsListWidget->takeItem(row);
        saveShortcuts();
        actionManager->removeAllShortcuts("s:"+scriptName);
        readShortcuts();
        scriptManager->removeScript(scriptName);
    }
}
//------------------------------------------------------------------------------
// does not check if the shortcut already there
void SettingsDialog::addShortcutToTable(const QString &action, const QString &shortcut) {
    if(action.isEmpty() || shortcut.isEmpty())
        return;

    ui->shortcutsTableWidget->setRowCount(ui->shortcutsTableWidget->rowCount() + 1);
    QTableWidgetItem *actionItem = new QTableWidgetItem(action);
    actionItem->setTextAlignment(Qt::AlignCenter);
    ui->shortcutsTableWidget->setItem(ui->shortcutsTableWidget->rowCount() - 1, 0, actionItem);
    QTableWidgetItem *shortcutItem = new QTableWidgetItem(shortcut);
    shortcutItem->setTextAlignment(Qt::AlignCenter);
    ui->shortcutsTableWidget->setItem(ui->shortcutsTableWidget->rowCount() - 1, 1, shortcutItem);
    // EFFICIENCY
    ui->shortcutsTableWidget->sortByColumn(0, Qt::AscendingOrder);
}
//------------------------------------------------------------------------------
void SettingsDialog::addShortcut() {
    ShortcutCreatorDialog w;
    if(!w.exec())
        return;
    for(int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
        if(ui->shortcutsTableWidget->item(i, 1)->text() == w.selectedShortcut())
            removeShortcutAt(i);
    }
    addShortcutToTable(w.selectedAction(), w.selectedShortcut());
    // select
    auto items = ui->shortcutsTableWidget->findItems(w.selectedShortcut(), Qt::MatchExactly);
    if(items.count()) {
        int newRow = ui->shortcutsTableWidget->row(items.at(0));
        ui->shortcutsTableWidget->selectRow(newRow);
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::removeShortcutAt(int row) {
    if(row > 0 && row >= ui->shortcutsTableWidget->rowCount())
        return;
    ui->shortcutsTableWidget->removeRow(row);
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut(int row) {
    if(row >= 0) {
        ShortcutCreatorDialog w;
        w.setWindowTitle(tr("Edit shortcut"));
        w.setAction(ui->shortcutsTableWidget->item(row, 0)->text());
        w.setShortcut(ui->shortcutsTableWidget->item(row, 1)->text());
        if(!w.exec())
            return;
        // remove itself
        removeShortcutAt(row);
        // remove anything we are replacing
        for(int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
            if(ui->shortcutsTableWidget->item(i, 1)->text() == w.selectedShortcut())
                removeShortcutAt(i);
        }
        // re-add
        addShortcutToTable(w.selectedAction(), w.selectedShortcut());
        // re-select
        auto items = ui->shortcutsTableWidget->findItems(w.selectedShortcut(), Qt::MatchExactly);
        if(items.count()) {
            int newRow = ui->shortcutsTableWidget->row(items.at(0));
            ui->shortcutsTableWidget->selectRow(newRow);
        }
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut() {
    editShortcut(ui->shortcutsTableWidget->currentRow());
}
//------------------------------------------------------------------------------
void SettingsDialog::removeShortcut() {
    removeShortcutAt(ui->shortcutsTableWidget->currentRow());
}
//------------------------------------------------------------------------------
void SettingsDialog::saveShortcuts() {
    actionManager->removeAllShortcuts();
    for(int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
        actionManager->addShortcut(ui->shortcutsTableWidget->item(i, 1)->text(),
                                   ui->shortcutsTableWidget->item(i, 0)->text());
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::resetShortcuts() {
    actionManager->resetDefaults();
    readShortcuts();
}
//------------------------------------------------------------------------------
void SettingsDialog::resetZoomLevels() {
    ui->zoomLevels->setText(settings->defaultZoomLevels());
}
//------------------------------------------------------------------------------
void SettingsDialog::selectMpvPath() {
    QFileDialog dialog;
    QString file;
    file = dialog.getOpenFileName(this, tr("Navigate to mpv binary"), "", "mpv*");
    if(!file.isEmpty()) {
        ui->mpvLineEdit->setText(file);
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::onExpandLimitSliderChanged(int value) {
    if(value == 0)
        ui->expandLimitLabel->setText("-");
    else
        ui->expandLimitLabel->setText(QString::number(value) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onJPEGQualitySliderChanged(int value) {
    ui->JPEGQualityLabel->setText(QString::number(value) + "%");
}
//------------------------------------------------------------------------------
void SettingsDialog::onZoomStepSliderChanged(int value) {
    ui->zoomStepLabel->setText(QString::number(value / 100.f, 'f', 2) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onMouseScrollingSpeedSliderChanged(int value) {
    ui->mouseScrollingSpeedLabel->setText(QString::number(0.5f + (value*0.25f), 'f', 2) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onThumbnailerThreadsSliderChanged(int value) {
    ui->thumbnailerThreadsLabel->setText(QString::number(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onBgOpacitySliderChanged(int value) {
    ui->bgOpacityPercentLabel->setText(QString::number(value) + "%");
}
//------------------------------------------------------------------------------
void SettingsDialog::onAutoResizeLimitSliderChanged(int value) {
    ui->autoResizeLimit->setText(QString::number(value * 5.f, 'f', 0) + "%");
}
//------------------------------------------------------------------------------
int SettingsDialog::exec() {
    return QDialog::exec();
}

void SettingsDialog::switchToPage(int number) {
    ui->sideBar2->selectEntry(number);
}
