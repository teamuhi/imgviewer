#include "mainwindow.h"

// TODO: nuke this and rewrite

MW::MW(QWidget *parent)
    : FloatingWidgetContainer(parent),
      currentDisplay(0),
      maximized(false),
      activeSidePanel(SIDEPANEL_NONE),
      copyOverlay(nullptr),
      saveOverlay(nullptr),
      renameOverlay(nullptr),
      infoBarFullscreen(nullptr),
      imageInfoOverlay(nullptr),
      floatingMessage(nullptr),
      cropPanel(nullptr),
      cropOverlay(nullptr)
{
    // A translucent top level window is a layered window on Windows: while resizing, the frame / desktop
    // flashes through between repaints. Only pay for it when the background is actually see-through.
    // (applied at startup; changing the background opacity needs a restart to switch modes)
    if(settings->backgroundOpacity() < 1.0) {
        setAttribute(Qt::WA_TranslucentBackground, true);
    } else {
        setAttribute(Qt::WA_OpaquePaintEvent, true); // paintEvent() fills the whole window
        setAutoFillBackground(false);
    }
    rootLayout.setContentsMargins(0,0,0,0);
    rootLayout.setSpacing(0);
    layout.setContentsMargins(0,0,0,0);
    layout.setSpacing(0);

    setMinimumSize(10,10);

    // do not steal focus when clicked
    // this is just a container. accept key events only
    // via passthrough from child widgets
    setFocusPolicy(Qt::NoFocus);

    this->setLayout(&rootLayout);

    setWindowTitle(QCoreApplication::applicationName() + " " +
                   QCoreApplication::applicationVersion());

    this->setMouseTracking(true);
    this->setAcceptDrops(true);
    this->setAccessibleName("mainwindow");
    windowGeometryChangeTimer.setSingleShot(true);
    windowGeometryChangeTimer.setInterval(30);
    setupUi();

    connect(settings, &Settings::settingsChanged, this, &MW::readSettings);
    connect(&windowGeometryChangeTimer, &QTimer::timeout, this, &MW::onWindowGeometryChanged);
    connect(this, &MW::fullscreenStateChanged, this, &MW::adaptToWindowState);

    readSettings();
    currentDisplay = settings->lastDisplay();
    maximized = settings->maximizedWindow();
    restoreWindowGeometry();
}

/*                                                             |--[ImageViewer]
 *                        |--[DocumentWidget]--[ViewerWidget]--|
 * [MW]--[CentralWidget]--|                                    |--[VideoPlayer]
 *                        |--[FolderView]
 *
 *  (not counting floating widgets)
 *  ViewerWidget exists for input handling reasons (correct overlay hover handling)
 */
void MW::setupUi() {
    // top bar spans the whole window, everything else lives in the layout below it
    topBar = new TopBar(this);
    rootLayout.addWidget(topBar);
    rootLayout.addLayout(&layout, 1);

    viewerWidget.reset(new ViewerWidget(this));
    infoBarWindowed.reset(new InfoBarProxy(this));
    docWidget.reset(new DocumentWidget(viewerWidget, infoBarWindowed));
    folderView.reset(new FolderViewProxy(this));
    connect(folderView.get(), &FolderViewProxy::sortingSelected, this, &MW::sortingSelected);
    connect(folderView.get(), &FolderViewProxy::directorySelected, this, &MW::opened);
    connect(folderView.get(), &FolderViewProxy::copyUrlsRequested, this, &MW::copyUrlsRequested);
    connect(folderView.get(), &FolderViewProxy::moveUrlsRequested, this, &MW::moveUrlsRequested);
    connect(folderView.get(), &FolderViewProxy::showFoldersChanged, this, &MW::showFoldersChanged);

    centralWidget.reset(new CentralWidget(docWidget, folderView, this));
    layout.addWidget(centralWidget.get());
    // the collage view only accepts a few navigation actions, everything else targets the document
    connect(centralWidget.get(), &CentralWidget::viewModeChanged, this, [this](ViewMode mode) {
        if(mode == MODE_COLLAGE)
            actionManager->setRestriction(ActionManager::Restriction::Collage);
        else
            actionManager->setRestriction(slideshowMode ? ActionManager::Restriction::Slideshow
                                                        : ActionManager::Restriction::None);
    });
    controlsOverlay = new ControlsOverlay(docWidget.get());
    infoBarFullscreen = new FullscreenInfoOverlayProxy(viewerWidget.get());
    sidePanel = new SidePanel(this);
    layout.addWidget(sidePanel);
    imageInfoOverlay = new ImageInfoOverlayProxy(viewerWidget.get());
    floatingMessage = new FloatingMessageProxy(viewerWidget.get()); // todo: use additional one for folderview?
    connect(viewerWidget.get(), &ViewerWidget::scalingRequested, this, &MW::scalingRequested);
    connect(viewerWidget.get(), &ViewerWidget::draggedOut, this, qOverload<>(&MW::draggedOut));
    connect(viewerWidget.get(), &ViewerWidget::playbackFinished, this, &MW::playbackFinished);
    connect(viewerWidget.get(), &ViewerWidget::zoomLevelChanged, topBar, &TopBar::setZoom);
    connect(topBar, &TopBar::settingsPageRequested, this, &MW::showSettingsPage);
    connect(viewerWidget.get(), &ViewerWidget::showScriptSettings, this, &MW::showScriptSettings);
    connect(this, &MW::zoomIn,        viewerWidget.get(), &ViewerWidget::zoomIn);
    connect(this, &MW::zoomOut,       viewerWidget.get(), &ViewerWidget::zoomOut);
    connect(this, &MW::zoomInCursor,  viewerWidget.get(), &ViewerWidget::zoomInCursor);
    connect(this, &MW::zoomOutCursor, viewerWidget.get(), &ViewerWidget::zoomOutCursor);
    connect(this, &MW::scrollUp,    viewerWidget.get(), &ViewerWidget::scrollUp);
    connect(this, &MW::scrollDown,  viewerWidget.get(), &ViewerWidget::scrollDown);
    connect(this, &MW::scrollLeft,  viewerWidget.get(), &ViewerWidget::scrollLeft);
    connect(this, &MW::scrollRight, viewerWidget.get(), &ViewerWidget::scrollRight);
    connect(this, &MW::pauseVideo,     viewerWidget.get(), &ViewerWidget::pauseResumePlayback);
    connect(this, &MW::stopPlayback,   viewerWidget.get(), &ViewerWidget::stopPlayback);
    connect(this, &MW::seekVideoForward, viewerWidget.get(), &ViewerWidget::seekForward);
    connect(this, &MW::seekVideoBackward,  viewerWidget.get(), &ViewerWidget::seekBackward);
    connect(this, &MW::frameStep,      viewerWidget.get(), &ViewerWidget::frameStep);
    connect(this, &MW::frameStepBack,  viewerWidget.get(), &ViewerWidget::frameStepBack);
    connect(this, &MW::toggleMute,  viewerWidget.get(), &ViewerWidget::toggleMute);
    connect(this, &MW::volumeUp,  viewerWidget.get(), &ViewerWidget::volumeUp);
    connect(this, &MW::volumeDown,  viewerWidget.get(), &ViewerWidget::volumeDown);
    connect(this, &MW::toggleTransparencyGrid, viewerWidget.get(), &ViewerWidget::toggleTransparencyGrid);
    connect(this, &MW::setLoopPlayback,  viewerWidget.get(), &ViewerWidget::setLoopPlayback);
}

void MW::setupFullUi() {
    setupCropPanel();
    docWidget->allowPanelInit();
    docWidget->setupMainPanel();
    infoBarWindowed->init();
    infoBarFullscreen->init();
}

void MW::setupCropPanel() {
    if(cropPanel)
        return;
    cropOverlay = new CropOverlay(viewerWidget.get());
    cropPanel = new CropPanel(cropOverlay, this);
    connect(cropPanel, &CropPanel::cancel, this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::crop,   this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::crop,   this, &MW::cropRequested);
    connect(cropPanel, &CropPanel::cropAndSave, this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::cropAndSave, this, &MW::cropAndSaveRequested);
}

void MW::setupCopyOverlay() {
    copyOverlay = new CopyOverlay(viewerWidget.get());
    connect(copyOverlay, &CopyOverlay::copyRequested, this, &MW::copyRequested);
    connect(copyOverlay, &CopyOverlay::moveRequested, this, &MW::moveRequested);
}

void MW::setupSaveOverlay() {
    saveOverlay = new SaveConfirmOverlay(viewerWidget.get());
    connect(saveOverlay, &SaveConfirmOverlay::saveClicked,    this, &MW::saveRequested);
    connect(saveOverlay, &SaveConfirmOverlay::saveAsClicked,  this, &MW::saveAsClicked);
    connect(saveOverlay, &SaveConfirmOverlay::discardClicked, this, &MW::discardEditsRequested);
}

void MW::setupRenameOverlay() {
    renameOverlay = new RenameOverlay(this);
    renameOverlay->setName(info.fileName);
    connect(renameOverlay, &RenameOverlay::renameRequested, this, &MW::renameRequested);
}

void MW::toggleFolderView() {
    if(!confirmExitCollage())
        return;
    hideCropPanel();
    if(copyOverlay)
        copyOverlay->hide();
    if(renameOverlay)
        renameOverlay->hide();
    docWidget->hideFloatingPanel();
    imageInfoOverlay->hide();
    centralWidget->toggleViewMode();
    onInfoUpdated();
}

bool MW::enableFolderView() {
    if(!confirmExitCollage())
        return false;
    hideCropPanel();
    if(copyOverlay)
        copyOverlay->hide();
    if(renameOverlay)
        renameOverlay->hide();
    docWidget->hideFloatingPanel();
    imageInfoOverlay->hide();
    centralWidget->showFolderView();
    onInfoUpdated();
    return true;
}

bool MW::enableDocumentView() {
    if(!confirmExitCollage())
        return false;
    centralWidget->showDocumentView();
    onInfoUpdated();
    return true;
}

// every way out of the collage view (Exit button, top bar back, Esc, Backspace / Enter, folder button)
// comes through here. The collage itself is kept: Ctrl+G resumes it.
bool MW::confirmExitCollage() {
    if(centralWidget->currentViewMode() != MODE_COLLAGE || !hasCollage() || !settings->collageConfirmExit())
        return true;
    CollageWidget *collage = centralWidget->collageWidget();
    if(collage && collage->isEditMode())
        return true; // the editor goes back to the collage view first, that is not an exit
    QMessageBox box(this);
    box.setWindowTitle(tr("Exit collage"));
    box.setText(tr("Leave the collage view and return to the image viewer?"));
    box.setInformativeText(tr("Your collage stays in memory - press Ctrl+G to resume it."));
    box.setIcon(QMessageBox::Question);
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::Yes);
    QCheckBox *dontAsk = new QCheckBox(tr("Don't ask again"), &box);
    box.setCheckBox(dontAsk);
    if(box.exec() != QMessageBox::Yes)
        return false;
    if(dontAsk->isChecked())
        settings->setCollageConfirmExit(false);
    return true;
}

ViewMode MW::currentViewMode() {
    return centralWidget->currentViewMode();
}

void MW::showCollage(const QStringList &paths) {
    hideCropPanel();
    if(copyOverlay)
        copyOverlay->hide();
    if(renameOverlay)
        renameOverlay->hide();
    docWidget->hideFloatingPanel();
    imageInfoOverlay->hide();
    CollageWidget *collage = centralWidget->ensureCollage();
    if(!collageConnected) {
        collageConnected = true;
        connect(collage, &CollageWidget::openImageRequested, this, &MW::collageImageOpened);
        connect(collage, &CollageWidget::infoChanged, this, &MW::onInfoUpdated);
        connect(collage, &CollageWidget::exitRequested, this, &MW::enableDocumentView);
    }
    centralWidget->showCollageView();
    if(!paths.isEmpty())
        collage->addImages(paths);
    onInfoUpdated();
}

bool MW::hasCollage() const {
    CollageWidget *collage = centralWidget->collageWidget();
    return collage && collage->imageCount() > 0;
}

QStringList MW::pickCollageImages(const QString &directory) {
    return CollageWidget::pickImages(this, directory);
}

bool MW::confirmDiscardCollage() {
    if(!hasCollage() || collageDiscardConfirmed)
        return true;
    bool ok = showConfirmation(tr("Close"), tr("A collage is open. Close the program and discard it?"));
    if(ok)
        collageDiscardConfirmed = true;
    return ok;
}

void MW::fitWindow() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWindow();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitWidth() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWidth();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitOriginal() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitOriginal();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitWindowStretch() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWindowStretch();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

// switch between 1:1 and Fit All
// TODO: move to viewerWidget?
void MW::switchFitMode() {
    if(viewerWidget->fitMode() == FIT_WINDOW)
        viewerWidget->setFitMode(FIT_ORIGINAL);
    else
        viewerWidget->setFitMode(FIT_WINDOW);
}

void MW::closeImage() {
    info.fileName = "";
    info.filePath = "";
    viewerWidget->closeImage();
}

// todo: fix flicker somehow
// ideally it should change img & resize in one go
void MW::preShowResize(QSize sz) {
    auto screens = qApp->screens();
    if(this->windowState() != Qt::WindowNoState || !screens.count() || screens.count() <= currentDisplay)
        return;
    int decorationSize = frameGeometry().height() - height();
    float maxSzMulti = settings->autoResizeLimit() / 100.f;
    QRect availableGeom = screens.at(currentDisplay)->availableGeometry();
    QSize maxSz = availableGeom.size() * maxSzMulti;
    maxSz.setHeight(maxSz.height() - decorationSize);
    if(!sz.isEmpty()) {
        if(sz.width() > maxSz.width() || sz.height() > maxSz.height())
            sz.scale(maxSz, Qt::KeepAspectRatio);
    } else {
        sz = maxSz;
    }
    QRect newGeom(0,0, sz.width(), sz.height());
    newGeom.moveCenter(availableGeom.center());
    newGeom.translate(0, decorationSize / 2);

    if(this->isVisible())
        setGeometry(newGeom);
    else // setGeometry wont work on hidden windows, so we just save for it to be restored later
        settings->setWindowGeometry(newGeom);
    qApp->processEvents(); // not needed anymore with patched qt?
}

void MW::showImage(std::unique_ptr<QPixmap> pixmap) {
    if(settings->autoResizeWindow())
        preShowResize(pixmap->size());
    viewerWidget->showImage(std::move(pixmap));
    updateCropPanelData();
    startSlideTransition();
}

void MW::setImageDpi(qreal dpi) {
    viewerWidget->setImageDpi(dpi);
}

void MW::showAnimation(std::shared_ptr<QMovie> movie) {
    if(settings->autoResizeWindow())
        preShowResize(movie->frameRect().size());
    viewerWidget->showAnimation(movie);
    updateCropPanelData();
    startSlideTransition();
}

void MW::showVideo(QString file) {
    if(settings->autoResizeWindow())
        preShowResize(QSize()); // tmp. find a way to get this though mpv BEFORE playback
    viewerWidget->showVideo(file);
    startSlideTransition();
}

void MW::showContextMenu() {
    viewerWidget->showContextMenu();
}

void MW::onSortingChanged(SortingMode mode) {
    folderView.get()->onSortingChanged(mode);
    if(centralWidget.get()->currentViewMode() == ViewMode::MODE_DOCUMENT) {
        switch(mode) {
            case SortingMode::SORT_NAME:      showMessage("Sorting: By Name");              break;
            case SortingMode::SORT_NAME_DESC: showMessage("Sorting: By Name (desc.)");      break;
            case SortingMode::SORT_TIME:      showMessage("Sorting: By Time");              break;
            case SortingMode::SORT_TIME_DESC: showMessage("Sorting: By Time (desc.)");      break;
            case SortingMode::SORT_SIZE:      showMessage("Sorting: By File Size");         break;
            case SortingMode::SORT_SIZE_DESC: showMessage("Sorting: By File Size (desc.)"); break;
        }
    }
}

void MW::setDirectoryPath(QString path) {
    //closeImage();
    info.directoryPath = path;
    info.directoryName = path.split("/").last();
    folderView->setDirectoryPath(path);
    onInfoUpdated();
}

void MW::toggleLockZoom() {
    viewerWidget->toggleLockZoom();
    if(viewerWidget->lockZoomEnabled())
        showMessage("Zoom lock: ON");
    else
        showMessage("Zoom lock: OFF");
    onInfoUpdated();
}

void MW::toggleLockView() {
    viewerWidget->toggleLockView();
    if(viewerWidget->lockViewEnabled())
        showMessage("View lock: ON");
    else
        showMessage("View lock: OFF");
    onInfoUpdated();
}

void MW::toggleFullscreenInfoBar() {
    if(!this->isFullScreen())
        return;
    showInfoBarFullscreen = !showInfoBarFullscreen;
    if(showInfoBarFullscreen)
        infoBarFullscreen->showWhenReady();
    else
        infoBarFullscreen->hide();
}

void MW::toggleImageInfoOverlay() {
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(imageInfoOverlay->isHidden())
        imageInfoOverlay->show();
    else
        imageInfoOverlay->hide();
}

void MW::toggleRenameOverlay(QString currentName) {
    if(!renameOverlay)
        setupRenameOverlay();
    if(renameOverlay->isHidden()) {
        renameOverlay->setBackdropEnabled((centralWidget->currentViewMode() == MODE_FOLDERVIEW));
        renameOverlay->setName(currentName);
        renameOverlay->show();
    } else {
        renameOverlay->hide();
    }
}

void MW::toggleScalingFilter() {
    ScalingFilter configuredFilter = settings->scalingFilter();
    if(viewerWidget->scalingFilter() == configuredFilter) {
        setFilterNearest();
    }
    else {
        setFilter(configuredFilter);
    }
}

void MW::setFilterNearest() {
    showMessage("Filter: nearest", 600);
    viewerWidget->setFilterNearest();
}

void MW::setFilterBilinear() {
    showMessage("Filter: bilinear", 600);
    viewerWidget->setFilterBilinear();
}

void MW::setFilter(ScalingFilter filter) {
    QString filterName;
    switch (filter) {
        case QI_FILTER_NEAREST:
            filterName = "nearest";
            break;
        case ScalingFilter::QI_FILTER_BILINEAR:
            filterName = "bilinear";
            break;
        case QI_FILTER_CV_BILINEAR_SHARPEN:
            filterName = "bilinear + sharpen";
            break;
        case QI_FILTER_CV_CUBIC:
            filterName = "bicubic";
            break;
        case QI_FILTER_CV_CUBIC_SHARPEN:
            filterName = "bicubic + sharpen";
            break;
        default:
            filterName = "configured " + QString::number(static_cast<int>(filter));
            break;
    }
    showMessage("Filter " + filterName, 600);
    viewerWidget->setScalingFilter(filter);
}

bool MW::isCropPanelActive() {
    return (activeSidePanel == SIDEPANEL_CROP);
}

void MW::onScalingFinished(std::unique_ptr<QPixmap> scaled) {
    viewerWidget->onScalingFinished(std::move(scaled));
}

void MW::saveWindowGeometry() {
    if(this->windowState() == Qt::WindowNoState)
        settings->setWindowGeometry(geometry());
    settings->setMaximizedWindow(maximized);
}

// does not apply fullscreen; window size / maximized state only
void MW::restoreWindowGeometry() {
    this->setGeometry(settings->windowGeometry());
    if(settings->maximizedWindow())
        this->setWindowState(Qt::WindowMaximized);
    updateCurrentDisplay();
}

void MW::updateCurrentDisplay() {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    currentDisplay = desktopWidget.screenNumber(this);
#else
    auto screens = qApp->screens();
    currentDisplay = screens.indexOf(this->window()->screen());
#endif
}

void MW::onWindowGeometryChanged() {
    saveWindowGeometry();
    updateCurrentDisplay();
}

void MW::saveCurrentDisplay() {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    settings->setLastDisplay(desktopWidget.screenNumber(this));
#else
    settings->setLastDisplay(qApp->screens().indexOf(this->window()->screen()));
#endif
}

//#############################################################
//######################### EVENTS ############################
//#############################################################

void MW::mouseMoveEvent(QMouseEvent *event) {
    event->ignore();
}

bool MW::event(QEvent *event) {
    // only save maximized state if we are already visible
    // this filter out out events while the window is still being set up
    if(event->type() == QEvent::WindowStateChange && this->isVisible() && !this->isFullScreen())
        maximized = isMaximized();
    if(event->type() == QEvent::Move || event->type() == QEvent::Resize)
        windowGeometryChangeTimer.start();
    return QWidget::event(event);
}

// hook up to actionManager
void MW::keyPressEvent(QKeyEvent *event) {
    event->accept();
    // slideshow mode keys are fixed (like the collage keys), everything else goes through the actions
    if(slideshowMode && event->modifiers() == Qt::NoModifier) {
        switch(event->key()) {
        case Qt::Key_Space:
            emit slideshowPauseRequested();
            return;
        case Qt::Key_Left:
            emit slideshowStepRequested(-1);
            return;
        case Qt::Key_Right:
            emit slideshowStepRequested(1);
            return;
        case Qt::Key_Escape:
            if(isFullScreen())
                showWindowed();
            else
                emit slideshowExitRequested();
            return;
        default:
            break;
        }
    }
    actionManager->processEvent(event);
}

void MW::wheelEvent(QWheelEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mousePressEvent(QMouseEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mouseReleaseEvent(QMouseEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mouseDoubleClickEvent(QMouseEvent *event) {
    event->accept();
    QMouseEvent *fakePressEvent = new QMouseEvent(
        QEvent::MouseButtonPress,
        event->pos(),
        event->button(),
        event->buttons(),
        event->modifiers()
    );
    actionManager->processEvent(fakePressEvent);
    actionManager->processEvent(event);
}

void MW::close() {
    saveWindowGeometry();
    saveCurrentDisplay();
    // try to close window sooner
    // since qt6.3 QWidget::close() no longer works on hidden windows (bug?)
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    this->hide();
#endif
    if(copyOverlay)
        copyOverlay->saveSettings();
    QWidget::close();
}

void MW::closeEvent(QCloseEvent *event) {
    // catch the close event when user presses X on the window itself
    if(!confirmDiscardCollage()) {
        event->ignore();
        return;
    }
    // closing the window is deliberate: bypass the collage / slideshow action lock
    actionManager->setRestriction(ActionManager::Restriction::None);
    event->accept();
    actionManager->invokeAction("exit");
}

void MW::dragEnterEvent(QDragEnterEvent *e) {
    if(e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void MW::dropEvent(QDropEvent *event) {
    emit droppedIn(event->mimeData(), event->source());
}

void MW::resizeEvent(QResizeEvent *event) {
    if(slideshowMode)
        placeSlideshowCaption();
    if(activeSidePanel == SIDEPANEL_CROP) {
        cropOverlay->setImageScale(viewerWidget->currentScale());
        cropOverlay->setImageDrawRect(viewerWidget->imageRect());
    }
    FloatingWidgetContainer::resizeEvent(event);
}

void MW::showDefault() {
    if(!this->isVisible()) {
        if(settings->fullscreenMode())
            showFullScreen();
        else
            showWindowed();
    }
}

void MW::showSaveDialog(QString filePath) {
    QString newFilePath = getSaveFileName(filePath);
    if(!newFilePath.isEmpty())
        emit saveAsRequested(newFilePath);
}

QString MW::getSaveFileName(QString filePath) {
    docWidget->hideFloatingPanel();
    QStringList filters;
    // generate filter for writable images
    // todo: some may need to be blacklisted
    auto writerFormats = QImageWriter::supportedImageFormats();
    if(writerFormats.contains("jpg"))  filters.append("JPEG (*.jpg *.jpeg *jpe *jfif)");
    if(writerFormats.contains("png"))  filters.append("PNG (*.png)");
    if(writerFormats.contains("webp")) filters.append("WebP (*.webp)");
    // may not work..
    if(writerFormats.contains("jp2"))  filters.append("JPEG 2000 (*.jp2 *.j2k *.jpf *.jpx *.jpm *.jpgx)");
    if(writerFormats.contains("jxl"))  filters.append("JPEG-XL (*.jxl)");
    if(writerFormats.contains("avif")) filters.append("AVIF (*.avif *.avifs)");
    if(writerFormats.contains("tif"))  filters.append("TIFF (*.tif *.tiff)");
    if(writerFormats.contains("bmp"))  filters.append("BMP (*.bmp)");
#ifdef _WIN32
    if(writerFormats.contains("ico"))  filters.append("Icon Files (*.ico)");
#endif
    if(writerFormats.contains("ppm"))  filters.append("PPM (*.ppm)");
    if(writerFormats.contains("xbm"))  filters.append("XBM (*.xbm)");
    if(writerFormats.contains("xpm"))  filters.append("XPM (*.xpm)");
    if(writerFormats.contains("dds"))  filters.append("DDS (*.dds)");
    if(writerFormats.contains("wbmp")) filters.append("WBMP (*.wbmp)");
    // add everything else from imagewriter
    for(auto fmt : writerFormats) {
        if(filters.filter(fmt).isEmpty())
            filters.append(fmt.toUpper() + " (*." + fmt + ")");
    }
    QString filterString = filters.join(";; ");

    // find matching filter for the current image
    QString selectedFilter = "JPEG (*.jpg *.jpeg *jpe *jfif)";
    QFileInfo fi(filePath);
    for(auto filter : filters) {
        if(filter.contains(fi.suffix().toLower())) {
            selectedFilter = filter;
            break;
        }
    }
    QString newFilePath = QFileDialog::getSaveFileName(this, tr("Save File as..."), filePath, filterString, &selectedFilter);
    return newFilePath;
}

void MW::showOpenDialog(QString path) {
    docWidget->hideFloatingPanel();

    QFileDialog dialog(this);
    QStringList imageFilter;
    imageFilter.append(settings->supportedFormatsFilter());
    imageFilter.append("All Files (*)");
    dialog.setDirectory(path);
    dialog.setNameFilters(imageFilter);
    dialog.setWindowTitle("Open image");
    dialog.setWindowModality(Qt::ApplicationModal);
    connect(&dialog, &QFileDialog::fileSelected, this, &MW::opened);
    dialog.exec();
}

void MW::showResizeDialog(QSize initialSize) {
    ResizeDialog dialog(initialSize, this);
    connect(&dialog, &ResizeDialog::sizeSelected, this, &MW::resizeRequested);
    dialog.exec();
}

DialogResult MW::fileReplaceDialog(QString src, QString dst, FileReplaceMode mode, bool multiple) {
    FileReplaceDialog dialog(this);
    dialog.setModal(true);
    dialog.setSource(src);
    dialog.setDestination(dst);
    dialog.setMode(mode);
    dialog.setMulti(multiple);

    dialog.exec();

    return dialog.getResult();
}

void MW::showSettings() {
    showSettingsPage(0);
}

void MW::showScriptSettings() {
    showSettingsPage(4);
}

void MW::showSettingsPage(int page) {
    docWidget->hideFloatingPanel();
    SettingsDialog settingsDialog(this);
    if(page > 0)
        settingsDialog.switchToPage(qMin(page, 6));
    settingsDialog.exec();
}

void MW::triggerFullScreen() {
    if(!isFullScreen()) {
        showFullScreen();
    } else {
        showWindowed();
    }
}

void MW::showFullScreen() {
    //do not save immediately on application start
    if(!isHidden())
        saveWindowGeometry();
    auto screens = qApp->screens();
    // todo: why check the screen again?
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    int _currentDisplay = desktopWidget.screenNumber(this);
#else
    int _currentDisplay = screens.indexOf(this->window()->screen());
#endif
    //move to target screen
    if(screens.count() > currentDisplay && currentDisplay != _currentDisplay) {
        this->move(screens.at(currentDisplay)->geometry().x(),
                   screens.at(currentDisplay)->geometry().y());
    }
    QWidget::showFullScreen();
    // try to repaint sooner
    qApp->processEvents();
    emit fullscreenStateChanged(true);
}

void MW::showWindowed() {
    if(isFullScreen())
        QWidget::showNormal();
    restoreWindowGeometry();
    // show maximized directly; show() on a hidden window flashes the normal-sized one first
    if(settings->maximizedWindow())
        QWidget::showMaximized();
    else
        QWidget::show();
    // try to repaint sooner
    qApp->processEvents();
    emit fullscreenStateChanged(false);
}

void MW::updateCropPanelData() {
    if(cropPanel && activeSidePanel == SIDEPANEL_CROP) {
        cropPanel->setImageRealSize(viewerWidget->sourceSize());
        cropOverlay->setImageDrawRect(viewerWidget->imageRect());
        cropOverlay->setImageScale(viewerWidget->currentScale());
        cropOverlay->setImageRealSize(viewerWidget->sourceSize());
    }
}

void MW::showSaveOverlay() {
    if(!settings->showSaveOverlay())
        return;
    if(!saveOverlay)
        setupSaveOverlay();
    saveOverlay->show();
}

void MW::hideSaveOverlay() {
    if(!saveOverlay)
        return;
    saveOverlay->hide();
}

void MW::showChangelogWindow() {
    changelogWindow->show();
}

void MW::showChangelogWindow(QString text) {
    changelogWindow->setText(text);
    changelogWindow->show();
}

void MW::triggerCropPanel() {
    if(activeSidePanel != SIDEPANEL_CROP) {
        showCropPanel();
    } else {
        hideCropPanel();
    }
}

void MW::showCropPanel() {
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;

    if(activeSidePanel != SIDEPANEL_CROP) {
        docWidget->hideFloatingPanel();
        sidePanel->setWidget(cropPanel);
        sidePanel->show();
        cropOverlay->show();
        activeSidePanel = SIDEPANEL_CROP;
        // reset & lock zoom so CropOverlay won't go crazy
        viewerWidget->fitWindow();
        setInteractionEnabled(false);
        // feed the panel current image info
        updateCropPanelData();
    }
}

void MW::setInteractionEnabled(bool mode) {
    docWidget->setInteractionEnabled(mode);
    viewerWidget->setInteractionEnabled(mode);
}

void MW::hideCropPanel() {
    sidePanel->hide();
    if(activeSidePanel == SIDEPANEL_CROP) {
        cropOverlay->hide();
        setInteractionEnabled(true);
    }
    activeSidePanel = SIDEPANEL_NONE;
}

void MW::triggerCopyOverlay() {
    if(!viewerWidget->isDisplaying())
        return;
    if(!copyOverlay)
        setupCopyOverlay();

    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(copyOverlay->operationMode() == OVERLAY_COPY) {
        copyOverlay->isHidden() ? copyOverlay->show() : copyOverlay->hide();
    } else {
        copyOverlay->setDialogMode(OVERLAY_COPY);
        copyOverlay->show();
    }
}

void MW::triggerMoveOverlay() {
    if(!viewerWidget->isDisplaying())
        return;
    if(!copyOverlay)
        setupCopyOverlay();

    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(copyOverlay->operationMode() == OVERLAY_MOVE) {
        copyOverlay->isHidden() ? copyOverlay->show() : copyOverlay->hide();
    } else {
        copyOverlay->setDialogMode(OVERLAY_MOVE);
        copyOverlay->show();
    }
}

void MW::setReturnToCollage(bool enabled) {
    if(returnToCollage == enabled)
        return;
    returnToCollage = enabled;
    onInfoUpdated();
}

//------------------------------------------------------------------------------
// slideshow mode
void MW::ensureSlideshowWidgets() {
    if(slideshowBar)
        return;
    slideshowBar = new SlideshowBar(viewerWidget.get());
    slideshowCaption = new SlideshowCaption(viewerWidget.get());
    slideTransition = new SlideTransition(viewerWidget.get());
    connect(slideshowBar, &SlideshowBar::prevRequested,  this, [this]() { emit slideshowStepRequested(-1); });
    connect(slideshowBar, &SlideshowBar::nextRequested,  this, [this]() { emit slideshowStepRequested(1); });
    connect(slideshowBar, &SlideshowBar::pauseRequested, this, &MW::slideshowPauseRequested);
    connect(slideshowBar, &SlideshowBar::exitRequested,  this, &MW::slideshowExitRequested);
    connect(slideshowBar, &SlideshowBar::optionsChanged, this, [this]() {
        slideshowCaption->refresh();
        placeSlideshowCaption();
        emit slideshowOptionsChanged();
    });
    slideshowIdleTimer.setSingleShot(true);
    slideshowIdleTimer.setInterval(2000);
    connect(&slideshowIdleTimer, &QTimer::timeout, this, &MW::slideshowIdle);
}

void MW::placeSlideshowCaption() {
    if(!slideshowCaption || !slideshowBar)
        return;
    const int margin = 16;
    int barLeft = (viewerWidget->width() - slideshowBar->sizeHint().width()) / 2;
    bool overlap = margin + slideshowCaption->sizeHint().width() + 8 > barLeft;
    slideshowCaption->setVerticalMargin(overlap ? slideshowBar->verticalMargin() + slideshowBar->sizeHint().height() + 10 : margin);
}

bool MW::isSlideshowMode() const {
    return slideshowMode;
}

void MW::setSlideshowMode(bool enabled) {
    if(slideshowMode == enabled)
        return;
    slideshowMode = enabled;
    ensureSlideshowWidgets();
    if(enabled) {
        hideCropPanel();
        if(copyOverlay)
            copyOverlay->hide();
        if(renameOverlay)
            renameOverlay->hide();
        imageInfoOverlay->hide();
        docWidget->setPanelSuppressed(true);
        viewerWidget->setRulersSuppressed(true);
        actionManager->setRestriction(ActionManager::Restriction::Slideshow);
        setSlideshowPaused(false);
        qApp->installEventFilter(this); // mouse movement anywhere reveals the bar
        slideshowCaption->setFile(info.filePath);
        placeSlideshowCaption();
        slideshowActivity();
    } else {
        qApp->removeEventFilter(this);
        slideshowIdleTimer.stop();
        if(slideshowCursorHidden) {
            QApplication::restoreOverrideCursor();
            slideshowCursorHidden = false;
        }
        slideTransition->finish();
        slideshowBar->hide();
        slideshowCaption->hide();
        docWidget->setPanelSuppressed(false);
        viewerWidget->setRulersSuppressed(false);
        actionManager->setRestriction(currentViewMode() == MODE_COLLAGE ? ActionManager::Restriction::Collage
                                                                        : ActionManager::Restriction::None);
    }
    adaptToWindowState();
    onInfoUpdated();
}

void MW::setSlideshowPaused(bool paused) {
    if(slideshowBar)
        slideshowBar->setPaused(paused);
}

// mouse moved: bar + cursor back, hide them again after a moment of rest
void MW::slideshowActivity() {
    if(slideshowCursorHidden) {
        QApplication::restoreOverrideCursor();
        slideshowCursorHidden = false;
    }
    slideshowBar->show();
    slideshowIdleTimer.start();
}

void MW::slideshowIdle() {
    if(!slideshowMode)
        return;
    // keep the bar while it is being used (hover, open combo popup, typing the timer)
    QWidget *focus = QApplication::focusWidget();
    if(slideshowBar->underMouse() || QApplication::activePopupWidget() || QApplication::activeModalWidget()
       || (focus && slideshowBar->isAncestorOf(focus))) {
        slideshowIdleTimer.start();
        return;
    }
    slideshowBar->hide();
    if(!slideshowCursorHidden && isActiveWindow()) {
        QApplication::setOverrideCursor(Qt::BlankCursor);
        slideshowCursorHidden = true;
    }
}

bool MW::eventFilter(QObject *watched, QEvent *event) {
    if(slideshowMode && event->type() == QEvent::MouseMove)
        slideshowActivity();
    return FloatingWidgetContainer::eventFilter(watched, event);
}

void MW::prepareSlideTransition(int direction) {
    if(!slideshowMode || currentViewMode() != MODE_DOCUMENT || !viewerWidget->isDisplaying())
        return;
    int style = settings->slideshowTransition();
    if(style == TRANSITION_NONE)
        return;
    // the bar / caption are not part of the picture that leaves
    bool barVisible = slideshowBar->isVisible(), captionVisible = slideshowCaption->isVisible();
    slideshowBar->setVisible(false);
    slideshowCaption->setVisible(false);
    QPixmap snapshot = viewerWidget->grab();
    slideshowBar->setVisible(barVisible);
    slideshowCaption->setVisible(captionVisible);
    slideTransition->arm(snapshot, style, direction);
    slideshowBar->raise();
    slideshowCaption->raise();
}

void MW::startSlideTransition() {
    if(!slideTransition || !slideTransition->isArmed())
        return;
    // never longer than half a slide
    slideTransition->start(qMin(400, settings->slideshowInterval() / 2));
}

// Esc inside the collage: editor -> collage view -> image viewer
void MW::collageBack() {
    CollageWidget *collage = centralWidget->collageWidget();
    if(!collage)
        return;
    if(collage->isEditMode())
        collage->setMode(CollageMode::View);
    else
        enableDocumentView();
}

// todo: this is crap, use shared state object
void MW::setCurrentInfo(int _index, int _fileCount, QString _filePath, QString _fileName, QSize _imageSize, qint64 _fileSize, bool slideshow, bool shuffle, bool edited) {
    info.index = _index;
    info.fileCount = _fileCount;
    info.fileName = _fileName;
    info.filePath = _filePath;
    info.imageSize = _imageSize;
    info.fileSize = _fileSize;
    info.slideshow = slideshow;
    info.shuffle = shuffle;
    info.edited = edited;
    onInfoUpdated();
}

// todo: nuke and rewrite
void MW::onInfoUpdated() {
    QString posString;
    if(info.fileCount)
        posString = "[ " + QString::number(info.index + 1) + "/" + QString::number(info.fileCount) + " ]";
    QString resString;
    if(info.imageSize.width())
        resString = QString::number(info.imageSize.width()) + " x " + QString::number(info.imageSize.height());
    QString sizeString;
    if(info.fileSize)
        sizeString = this->locale().formattedDataSize(info.fileSize, 1);

    if(renameOverlay)
        renameOverlay->setName(info.fileName);

    // no way back from the other views without this
    ViewMode viewMode = centralWidget->currentViewMode();
    bool backToCollage = returnToCollage && viewMode == MODE_DOCUMENT;
    topBar->setBackToCollage(backToCollage);
    topBar->setBackVisible(backToCollage || viewMode != MODE_DOCUMENT);
    topBar->setZoomAllowed(viewMode == MODE_DOCUMENT);
    topBar->setSlideshowAllowed(viewMode != MODE_COLLAGE);

    QString windowTitle;
    if(centralWidget->currentViewMode() == MODE_COLLAGE) {
        CollageWidget *collage = centralWidget->collageWidget();
        QString name = (collage && collage->isEditMode()) ? tr("Collage editor") : tr("Collage");
        QString count = tr("%n image(s)", "", collage ? collage->imageCount() : 0);
        windowTitle = name + " - " + count;
        infoBarFullscreen->setInfo("", name, count);
        infoBarWindowed->setInfo("", name, count);
        topBar->setInfo("", name, count);
    } else if(centralWidget->currentViewMode() == MODE_FOLDERVIEW) {
        windowTitle = tr("Folder view");
        infoBarFullscreen->setInfo("", tr("No file opened."), "");
        infoBarWindowed->setInfo("", tr("No file opened."), "");
        topBar->setInfo("", tr("Folder view"), "");
    } else if(info.fileName.isEmpty()) {
        windowTitle = qApp->applicationName();
        infoBarFullscreen->setInfo("", tr("No file opened."), "");
        infoBarWindowed->setInfo("", tr("No file opened."), "");
        topBar->setInfo("", tr("No file opened."), "");
    } else {
        windowTitle = info.fileName;
        if(settings->windowTitleExtendedInfo()) {
            windowTitle.prepend(posString + "  ");
            if(!resString.isEmpty())
                windowTitle.append("  -  " + resString);
            if(!sizeString.isEmpty())
                windowTitle.append("  -  " + sizeString);
        }

        // toggleable states
        QString states;
        if(info.slideshow)
            states.append(" [slideshow]");
        if(info.shuffle)
            states.append(" [shuffle]");
        if(viewerWidget->lockZoomEnabled())
            states.append(" [zoom lock]");
        if(viewerWidget->lockViewEnabled())
            states.append(" [view lock]");

        if(!settings->infoBarWindowed() && !states.isEmpty())
            windowTitle.append(" -" + states);
        if(info.edited)
            windowTitle.prepend("* ");

        infoBarFullscreen->setInfo(posString, info.fileName + (info.edited ? "  *" : ""), resString + "  " + sizeString);
        // the top bar already shows index / name / resolution / size / states: the bottom bar shows the rest
        QFileInfo fileInfo(info.filePath);
        QStringList details;
        if(!fileInfo.suffix().isEmpty())
            details << fileInfo.suffix().toUpper();
        if(fileInfo.exists())
            details << tr("modified %1").arg(locale().toString(fileInfo.lastModified(), QLocale::ShortFormat));
        QStringList shape;
        if(info.imageSize.width() > 0 && info.imageSize.height() > 0) {
            const int w = info.imageSize.width(), h = info.imageSize.height();
            shape << tr("%1 MP").arg(QString::number(double(w) * h / 1000000.0, 'f', w * h < 100000 ? 2 : 1));
            int g = w, rest = h;
            while(rest) { const int t = g % rest; g = rest; rest = t; } // gcd
            if(w / g <= 32 && h / g <= 32) // 16:9, 4:3, 1:1; odd sizes get a decimal ratio instead
                shape << QString("%1:%2").arg(w / g).arg(h / g);
            else
                shape << QString("%1:1").arg(double(w) / h, 0, 'f', 2);
        }
        infoBarWindowed->setInfo(info.directoryName, details.join("  ·  "), shape.join("  ·  "));
        topBar->setInfo(posString, info.fileName + (info.edited ? "  *" : ""), resString + "  " + sizeString + " " + states);
    }
    setWindowTitle(windowTitle);
    if(slideshowMode && slideshowCaption) {
        slideshowCaption->setFile(info.filePath);
        placeSlideshowCaption();
    }
}

// TODO!!! buffer this in mw
void MW::setExifInfo(QMap<QString, QString> info) {
    if(imageInfoOverlay)
        imageInfoOverlay->setExifInfo(info);
}

std::shared_ptr<FolderViewProxy> MW::getFolderView() {
    return folderView;
}

std::shared_ptr<ThumbnailStripProxy> MW::getThumbnailPanel() {
    return docWidget->thumbPanel();
}

// todo: this is crap
void MW::showMessageDirectory(QString dirName) {
    floatingMessage->showMessage(dirName, FloatingMessageIcon::ICON_DIRECTORY, 1700);
}

void MW::showMessageDirectoryEnd() {
    // TODO replace with something nicer (integrate with click overlay?)
    //floatingMessage->showMessage("", FloatingWidgetPosition::RIGHT, FloatingMessageIcon::ICON_RIGHT_EDGE, 400);
}

void MW::showMessageDirectoryStart() {
    // TODO replace with something nicer (integrate with click overlay?)
    //floatingMessage->showMessage("", FloatingWidgetPosition::LEFT, FloatingMessageIcon::ICON_LEFT_EDGE, 400);
}

void MW::showMessageFitWindow() {
    floatingMessage->showMessage(tr("Fit Window"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessageFitWidth() {
    floatingMessage->showMessage(tr("Fit Width"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessageFitOriginal() {
    floatingMessage->showMessage(tr("Fit 1:1"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessage(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::NO_ICON, 1500);
}

void MW::showMessage(QString text, int duration) {
    floatingMessage->showMessage(text, FloatingMessageIcon::NO_ICON, duration);
}

void MW::showMessageSuccess(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::ICON_SUCCESS, 1500);
}

void MW::showWarning(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::ICON_WARNING, 1500);
}

void MW::showError(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::ICON_ERROR, 2800);
}

bool MW::showConfirmation(QString title, QString msg) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(title);
    msgBox.setText(msg);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.addButton(QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.setModal(true);
    if(msgBox.exec() == QMessageBox::Yes)
        return true;
    else
        return false;
}

void MW::readSettings() {
    showInfoBarFullscreen = settings->infoBarFullscreen();
    showInfoBarWindowed = settings->infoBarWindowed();
    adaptToWindowState();
}

// todo: remove/rename?
void MW::applyWindowedBackground() {
#ifdef USE_KDE_BLUR
    QWindow* window = this->windowHandle();
    if(window) {
        if(settings->backgroundOpacity() == 1.0)
            KWindowEffects::enableBlurBehind(window, false);
        else
            KWindowEffects::enableBlurBehind(window, settings->blurBackground());
    }
#endif
}

void MW::applyFullscreenBackground() {
#ifdef USE_KDE_BLUR
    QWindow* window = this->windowHandle();
    if(window)
        KWindowEffects::enableBlurBehind(window, false);
#endif
}

// changes ui elements according to fullscreen state
void MW::adaptToWindowState() {
    docWidget->hideFloatingPanel();
    if(isFullScreen()) { //-------------------------------------- fullscreen ---
        applyFullscreenBackground();
        infoBarWindowed->hide();
        topBar->hide();

        if(showInfoBarFullscreen)
            infoBarFullscreen->showWhenReady();
        else
            infoBarFullscreen->hide();    

        auto pos = settings->panelPosition();
        if(!settings->panelEnabled() || pos == PANEL_BOTTOM || pos == PANEL_LEFT)
            controlsOverlay->show();
        else
            controlsOverlay->hide();
    } else { //------------------------------------------------------ window ---
        applyWindowedBackground();
        infoBarFullscreen->hide();
        topBar->setVisible(settings->topBarEnabled());

        if(showInfoBarWindowed)
            infoBarWindowed->show();
        else
            infoBarWindowed->hide();

        controlsOverlay->hide();
    }
    if(slideshowMode) { // nothing but the picture (+ slideshow bar / caption)
        topBar->hide();
        infoBarWindowed->hide();
        infoBarFullscreen->hide();
        controlsOverlay->hide();
    }
    folderView->onFullscreenModeChanged(isFullScreen());
    docWidget->onFullscreenModeChanged(isFullScreen());
    viewerWidget->onFullscreenModeChanged(isFullScreen());
}

void MW::paintEvent(QPaintEvent *event) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    FloatingWidgetContainer::paintEvent(event);
}

void MW::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    docWidget->hideFloatingPanel(true);
}

// block native tab-switching so we can use it in shortcuts
//bool MW::focusNextPrevChild(bool) {
//    return false;
//}
