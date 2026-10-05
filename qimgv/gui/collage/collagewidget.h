#pragma once

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>
#include <QStringList>
#include <QTimer>
#include "gui/collage/collagescene.h"
#include "gui/collage/collageview.h"
#include "gui/customwidgets/colorselectorbutton.h"

// Collage widget with two faces on one image list:
//  View - tiles fill the window, minimal auto-hiding bar, drag to swap, zoom / pan tiles
//  Edit - fixed canvas with free frames, toolbar, per-image properties, export
class WrapLayout;
class CollageWidget : public QWidget {
    Q_OBJECT
public:
    explicit CollageWidget(QWidget *parent = nullptr);

    CollageMode currentMode() const;
    bool isEditMode() const;
    void setMode(CollageMode mode);

    // first batch gets an automatic layout, later batches are cascaded on top
    void addImages(const QStringList &paths);
    int imageCount() const;

    // multi-select file dialog restricted to readable image formats
    static QStringList pickImages(QWidget *parent, const QString &directory);

signals:
    void openImageRequested(const QString &path); // tile opened (double-click / Enter)
    void infoChanged();                           // mode or image count changed
    void exitRequested();                         // back button: leave the collage for the image viewer

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onAddImages();
    void onNewCollage();
    void onArrange();
    void onPresetChanged(int index);
    void onCanvasSpinChanged();
    void onCanvasColorChanged();
    void onExport();
    void onSceneChanged();
    void updatePanel();
    void updateButtonIcons();

private:
    CollageScene *mScene;
    CollageView *mView;

    // toolbar
    QWidget *mToolbar;
    QComboBox *mLayoutCombo, *mPresetCombo;
    QSpinBox *mGapSpin, *mCanvasWidthSpin, *mCanvasHeightSpin;
    ColorSelectorButton *mBackgroundButton;
    QCheckBox *mTransparentCheck, *mAnimateCheck, *mStaticCheck;
    QPushButton *mArrangeButton;
    QPushButton *mPanelToggle;

    // properties panel
    QWidget *mPanelContainer; // header (hide button) + scroll area
    QScrollArea *mPanelScroll, *mHelpScroll;
    QPushButton *mHelpButton;
    QLabel *mHelpViewLabel, *mHelpEditLabel;
    QWidget *mPanelContent;
    QLabel *mNameLabel, *mInfoLabel, *mZoomValueLabel, *mOpacityValueLabel;
    QComboBox *mFitCombo, *mResolutionCombo;
    QSpinBox *mXSpin, *mYSpin, *mWidthSpin, *mHeightSpin, *mRadiusSpin;
    QCheckBox *mKeepAspectCheck;
    QSlider *mZoomSlider, *mOpacitySlider;
    QPushButton *mCropModeButton;
    QCheckBox *mAnimPlayCheck, *mAnimLoopCheck;
    QSlider *mAnimSpeedSlider;
    QLabel *mAnimSpeedLabel;
    QList<QWidget*> mAnimWidgets; // enabled only while an animated image is selected

    // view mode chrome
    QWidget *mOverlayBar;
    WrapLayout *mOverlayLayout;
    QComboBox *mViewLayoutCombo, *mViewAspectCombo;
    QSpinBox *mViewGapSpin;
    QCheckBox *mViewAnimateCheck;
    QSlider *mViewSizeSlider;
    QLabel *mViewSizeValueLabel, *mEmptyHint;
    QPushButton *mViewPanelButton;
    QTimer *mOverlayTimer;
    QList<QPair<QPushButton*, QString>> mIconButtons; // re-tinted when the theme changes
    QList<QWidget*> mEditOnlyWidgets, mViewOnlyWidgets; // panel rows that only make sense in one mode
    QList<QWidget*> mStackWidgets; // front / back: editor and the Freehand view layout
    bool mViewPanelOpen = false, mEditPanelVisible = true;
    int mLastInfoKey = -1;

    QLabel *mStatusLabel;
    QString mNotice; // transient message appended to the status line
    bool mNoticeWarning = false;
    bool mSyncing = false;
    bool mPanelUserSet = false;

    void buildToolbar();
    void buildPanel();
    void buildOverlay();
    void applyModeUi();
    void updateLayoutRows();
    void layoutOverlays();
    void showOverlay();
    void updateOverlayVisibility();
    void emitInfoIfChanged();
    QWidget *labeled(const QString &text, QWidget *control, QWidget *parent);
    void setButtonIcon(QPushButton *button, const QString &iconName);
    CollageItem *primaryItem() const;
    QList<CollageItem*> targets() const;
    void applyCanvasSize();
    void setAnimate(bool enabled);
    void setStaticCanvas(bool enabled);
    void applyStaticLayout();
    void applyGeometry(int source);
    void setNotice(const QString &text, bool warning = false);
    void updateStatus();
    void setPanelVisible(bool visible);
    void updateHelpVisibility();
    static QString helpHtml(bool edit);
};
