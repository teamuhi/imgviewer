#pragma once

#include <QApplication>
#include <QObject>
#include <QWidget>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QMimeData>
#include <QImageWriter>
#include <QWindow>
#include <QFileInfo>
#include <QStringList>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
#include <QDesktopWidget>
#endif

#include "gui/customwidgets/floatingwidgetcontainer.h"
#include "gui/viewers/viewerwidget.h"
#include "gui/overlays/controlsoverlay.h"
#include "gui/overlays/fullscreeninfooverlayproxy.h"
#include "gui/overlays/floatingmessageproxy.h"
#include "gui/overlays/saveconfirmoverlay.h"
#include "gui/panels/mainpanel/thumbnailstrip.h"
#include "gui/panels/sidepanel/sidepanel.h"
#include "gui/panels/croppanel/croppanel.h"
#include "gui/overlays/cropoverlay.h"
#include "gui/overlays/copyoverlay.h"
#include "gui/overlays/changelogwindow.h"
#include "gui/overlays/imageinfooverlayproxy.h"
#include "gui/overlays/renameoverlay.h"
#include "gui/dialogs/resizedialog.h"
#include "gui/centralwidget.h"
#include "gui/dialogs/filereplacedialog.h"
#include "components/actionmanager/actionmanager.h"
#include "settings.h"
#include "gui/dialogs/settingsdialog.h"
#include "gui/viewers/documentwidget.h"
#include "gui/folderview/folderviewproxy.h"
#include "gui/panels/infobar/infobarproxy.h"
#include "gui/panels/topbar/topbar.h"
#include "gui/overlays/slideshowbar.h"
#include "gui/overlays/slideshowcaption.h"
#include "gui/overlays/slidetransition.h"

#ifdef USE_KDE_BLUR
#include <KWindowEffects>
#endif

struct CurrentInfo {
    int index;
    int fileCount;
    QString fileName;
    QString filePath;
    QString directoryName;
    QString directoryPath;
    QSize imageSize;
    qint64 fileSize;
    bool slideshow;
    bool shuffle;
    bool edited;
};

enum ActiveSidePanel {
    SIDEPANEL_CROP,
    SIDEPANEL_NONE
};

class MW : public FloatingWidgetContainer
{
    Q_OBJECT
public:
    explicit MW(QWidget *parent = nullptr);
    bool isCropPanelActive();
    void onScalingFinished(std::unique_ptr<QPixmap>scaled);
    void showImage(std::unique_ptr<QPixmap> pixmap);
    void showAnimation(std::shared_ptr<QMovie> movie);
    // rulers: DPI stored in the shown image (0 = none)
    void setImageDpi(qreal dpi);
    void showVideo(QString file);

    void setCurrentInfo(int fileIndex, int fileCount, QString filePath, QString fileName, QSize imageSize, qint64 fileSize, bool slideshow, bool shuffle, bool edited);
    void setExifInfo(QMap<QString, QString>);
    std::shared_ptr<FolderViewProxy> getFolderView();
    std::shared_ptr<ThumbnailStripProxy> getThumbnailPanel();

    ViewMode currentViewMode();

    // collage editor
    void showCollage(const QStringList &paths);
    bool hasCollage() const;
    QStringList pickCollageImages(const QString &directory);
    // asks before the program closes while a collage is open. true = ok to proceed
    bool confirmDiscardCollage();

    // asks before leaving the collage view (keeps the collage in memory). true = ok to leave
    bool confirmExitCollage();

    // slideshow mode: hides the chrome, shows the slideshow bar / caption, restricts the actions
    void setSlideshowMode(bool enabled);
    bool isSlideshowMode() const;
    void setSlideshowPaused(bool paused);
    // snapshot of the current slide, animated away once the next one is shown (direction: +1 / -1)
    void prepareSlideTransition(int direction);
    void startSlideTransition();

    bool showConfirmation(QString title, QString msg);
    DialogResult fileReplaceDialog(QString source, QString target, FileReplaceMode mode, bool multiple);

private:
    std::shared_ptr<ViewerWidget> viewerWidget;
    // NOTE: rootLayout must stay declared before layout (layout is nested in it, destruction order)
    QVBoxLayout rootLayout;
    QHBoxLayout layout;
    TopBar *topBar;
    bool returnToCollage = false; // the open image came from the collage
    QTimer windowGeometryChangeTimer;
    int currentDisplay;

    bool cropPanelActive, showInfoBarFullscreen, showInfoBarWindowed, maximized;
    std::shared_ptr<DocumentWidget> docWidget;
    std::shared_ptr<FolderViewProxy> folderView;
    std::shared_ptr<CentralWidget> centralWidget;
    ActiveSidePanel activeSidePanel;
    SidePanel *sidePanel;
    CropPanel *cropPanel;
    CropOverlay *cropOverlay;
    SaveConfirmOverlay *saveOverlay;
    ChangelogWindow *changelogWindow;

    CopyOverlay *copyOverlay;

    RenameOverlay *renameOverlay;

    ImageInfoOverlayProxy *imageInfoOverlay;

    ControlsOverlay *controlsOverlay;
    FullscreenInfoOverlayProxy *infoBarFullscreen;
    std::shared_ptr<InfoBarProxy> infoBarWindowed;
    FloatingMessageProxy *floatingMessage;

    PanelPosition panelPosition;
    CurrentInfo info;
    bool collageDiscardConfirmed = false;
    bool collageConnected = false;

    bool slideshowMode = false;
    SlideshowBar *slideshowBar = nullptr;
    SlideshowCaption *slideshowCaption = nullptr;
    SlideTransition *slideTransition = nullptr;
    QTimer slideshowIdleTimer; // hides the bar and the cursor when the mouse rests
    bool slideshowCursorHidden = false;
    void ensureSlideshowWidgets();
    void placeSlideshowCaption(); // above the bar when the two would overlap
    void slideshowActivity();
    void slideshowIdle();
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    QDesktopWidget desktopWidget;
#endif

    void saveWindowGeometry();
    void restoreWindowGeometry();
    void saveCurrentDisplay();
    void setupUi();

    void applyWindowedBackground();
    void applyFullscreenBackground();
    void mouseDoubleClickEvent(QMouseEvent *event);

    void setupCropPanel();
    void setupCopyOverlay();
    void setupSaveOverlay();
    void setupRenameOverlay();
    void preShowResize(QSize sz);
    void setInteractionEnabled(bool mode);

private slots:
    void updateCurrentDisplay();
    void readSettings();
    void adaptToWindowState();
    void onWindowGeometryChanged();
    void onInfoUpdated();
    void showScriptSettings();
    // opens the settings dialog on a page (0 = General ... 6 = About)
    void showSettingsPage(int page);

protected:
    void mouseMoveEvent(QMouseEvent *event);
    bool event(QEvent *event);
    void paintEvent(QPaintEvent *event);
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *e);
    void dropEvent(QDropEvent *event);
    void resizeEvent(QResizeEvent *event);

    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void wheelEvent(QWheelEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void leaveEvent(QEvent *event);

   // bool focusNextPrevChild(bool);
signals:
    void opened(QString);
    void fullscreenStateChanged(bool);
    void copyRequested(QString);
    void moveRequested(QString);
    void copyUrlsRequested(QList<QString>, QString);
    void moveUrlsRequested(QList<QString>, QString);
    void showFoldersChanged(bool);
    void resizeRequested(QSize);
    void renameRequested(QString);
    void cropRequested(QRect);
    void cropAndSaveRequested(QRect);
    void discardEditsRequested();
    void saveAsClicked();
    void saveRequested();
    void saveAsRequested(QString);
    void sortingSelected(SortingMode);
    void collageImageOpened(const QString &path); // a tile was opened from the collage

    // slideshow bar / slideshow keys
    void slideshowPauseRequested();
    void slideshowStepRequested(int direction);
    void slideshowExitRequested();
    void slideshowOptionsChanged();

    // viewerWidget
    void scalingRequested(QSize, ScalingFilter);
    void zoomIn();
    void zoomOut();
    void zoomInCursor();
    void zoomOutCursor();
    void scrollUp();
    void scrollDown();
    void scrollLeft();
    void scrollRight();
    void pauseVideo();
    void stopPlayback();
    void seekVideoForward();
    void seekVideoBackward();
    void frameStep();
    void frameStepBack();
    void toggleMute();
    void volumeUp();
    void volumeDown();
    void toggleTransparencyGrid();
    void droppedIn(const QMimeData*, QObject*);
    void draggedOut();
    void setLoopPlayback(bool);
    void playbackFinished();

public slots:
    void setupFullUi();
    void showDefault();
    void showCropPanel();
    void hideCropPanel();
    void toggleFolderView();
    bool enableFolderView();
    bool enableDocumentView();
    void showOpenDialog(QString path);
    void showSaveDialog(QString filePath);
    QString getSaveFileName(QString fileName);
    void showResizeDialog(QSize initialSize);
    void showSettings();
    void triggerFullScreen();
    void showMessageDirectory(QString dirName);
    void showMessageDirectoryEnd();
    void showMessageDirectoryStart();
    void showMessageFitWindow();
    void showMessageFitWidth();
    void showMessageFitOriginal();
    void showFullScreen();
    void showWindowed();
    void triggerCopyOverlay();
    void showMessage(QString text);
    void showMessage(QString text, int duration);
    void showMessageSuccess(QString text);
    void showWarning(QString text);
    void showError(QString text);
    void triggerMoveOverlay();
    void setReturnToCollage(bool enabled);
    void collageBack();
    void close();
    void triggerCropPanel();
    void updateCropPanelData();
    void showSaveOverlay();
    void hideSaveOverlay();
    void showChangelogWindow();
    void showChangelogWindow(QString text);
    void fitWindow();
    void fitWidth();
    void fitOriginal();
    void fitWindowStretch();
    void switchFitMode();
    void closeImage();
    void showContextMenu();
    void onSortingChanged(SortingMode);
    void toggleImageInfoOverlay();
    void toggleRenameOverlay(QString currentName);
    void setFilterNearest();
    void setFilterBilinear();
    void setFilter(ScalingFilter filter);
    void toggleScalingFilter();
    void setDirectoryPath(QString path);
    void toggleLockZoom();
    void toggleLockView();
    void toggleFullscreenInfoBar();
};
