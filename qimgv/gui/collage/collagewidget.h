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
#include <QRect>
#include "gui/collage/collagescene.h"
#include "gui/collage/collageview.h"
#include "gui/customwidgets/colorselectorbutton.h"
#include "gui/customwidgets/scrubspinbox.h"
#include "gui/customwidgets/rulers.h"
#include <QToolButton>

// Collage widget with two faces on one image list:
//  View - tiles fill the window, minimal auto-hiding bar, drag to swap, zoom / pan tiles
//  Edit - fixed canvas with free frames, toolbar, per-image properties, export
class WrapLayout;
class QPropertyAnimation;

// Floating card for the properties / layout panels: a rounded, theme-tinted overlay painted over the collage.
// Optionally resizable by dragging one vertical edge.
class CollagePanelFrame : public QWidget {
    Q_OBJECT
public:
    explicit CollagePanelFrame(QWidget *parent = nullptr);
    // edge: Qt::LeftEdge (panel docked right) or Qt::RightEdge (panel docked left); 0 = fixed width
    void setResizeEdge(Qt::Edges edge);

signals:
    void widthDragged(int width);
    void resizeFinished(int width);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Qt::Edges mEdge;
    QWidget *mGrip = nullptr; // thin strip on the resizable edge, above the content
    bool mResizing = false;
    int mStartGlobalX = 0, mStartWidth = 0;
    void placeGrip();
};

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
    RulerController *mRulers = nullptr; // rulers + guides inside the view (both modes)
    QRect contentArea() const;          // the view without the rulers: floating bars / cards stay inside it

    // toolbar
    QWidget *mToolbar;
    QComboBox *mLayoutCombo, *mPresetCombo;
    ScrubSpinBox *mGapSpin, *mCanvasWidthSpin, *mCanvasHeightSpin;
    ColorSelectorButton *mBackgroundButton;
    QCheckBox *mTransparentCheck, *mAnimateCheck;
    QPushButton *mPanelToggle, *mEditCropButton, *mEditLayoutPanelButton;
    WrapLayout *mToolbarLayout;

    // properties panel
    CollagePanelFrame *mPanelContainer; // floats over the view: header (hide button) + scroll area
    QScrollArea *mPanelScroll, *mHelpScroll;
    QPushButton *mHelpButton;
    QLabel *mHelpViewLabel, *mHelpEditLabel;
    QWidget *mPanelContent;
    QLabel *mNameLabel, *mInfoLabel, *mZoomValueLabel, *mOpacityValueLabel;
    QComboBox *mFitCombo, *mResolutionCombo;
    ScrubSpinBox *mXSpin, *mYSpin, *mWidthSpin, *mHeightSpin, *mRadiusSpin;
    // view: size of the selected tile (best effort in the automatic layouts, exact in Freehand)
    ScrubSpinBox *mViewWSpin, *mViewHSpin;
    QList<QWidget*> mViewSizeWidgets;
    QComboBox *mBorderModeCombo = nullptr;
    QList<QWidget*> mBorderModeWidgets; // only while an automatic layout is on screen
    // per-tile outline (overrides the collage default)
    ScrubSpinBox *mTileBorderSpin;
    ColorSelectorButton *mTileBorderColor;
    QComboBox *mTileBorderStyle;
    int mPanelWidth = 320;
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
    QComboBox *mViewLayoutCombo, *mViewAspectCombo, *mViewShapeCombo;
    ScrubSpinBox *mViewGapSpin;
    QCheckBox *mViewAnimateCheck;
    QLabel *mEmptyHint;
    QPushButton *mViewPanelButton, *mViewCropButton, *mViewLayoutPanelButton;
    // view canvas: "Custom..." width / height
    QWidget *mViewCustomBox = nullptr;
    ScrubSpinBox *mViewCanvasWSpin, *mViewCanvasHSpin;
    // view background: theme (colour + pattern) or the canvas colour
    QCheckBox *mViewThemeBgCheck;
    ColorSelectorButton *mViewBgButton;
    // "Border" popups of the view bar and the editor toolbar (same settings)
    struct BorderPopup {
        ScrubSpinBox *width = nullptr;
        ColorSelectorButton *color = nullptr;
        QCheckBox *themeColor = nullptr;
        QComboBox *style = nullptr;
    };
    QList<BorderPopup> mBorderPopups;

    // layout options panel (Grid / Row / Column), floats at the left edge
    CollagePanelFrame *mLayoutPanel = nullptr;
    QLabel *mLayoutPanelTitle;
    ScrubSpinBox *mLayColumns, *mLayRows, *mLayCellW, *mLayCellH, *mLayLines, *mLayLineSize;
    ScrubSpinBox *mLayRotation, *mLayGap, *mLaySides, *mLayStarDepth;
    QCheckBox *mLayHoneycomb, *mLayStar;
    QComboBox *mLayShape = nullptr, *mLayBorderMode = nullptr;
    QLabel *mLayLinesLabel, *mLayLineSizeLabel;
    QList<QWidget*> mLayGridRows, mLayLineRows, mLayPolygonRows;
    bool mLayoutPanelWanted = false;
    QTimer *mOverlayTimer;
    QList<QPair<QPushButton*, QString>> mIconButtons; // re-tinted when the theme changes
    QList<QWidget*> mEditOnlyWidgets, mViewOnlyWidgets; // panel rows that only make sense in one mode
    QList<QWidget*> mStackWidgets; // front / back: editor and the Freehand view layout
    bool mViewPanelOpen = false, mEditPanelVisible = true;
    bool mCropOn = false; // the Crop toggle (only takes effect in Freehand layouts)
    int mLastInfoKey = -1;

    QLabel *mStatusLabel;
    QString mNotice; // transient message appended to the status line
    bool mNoticeWarning = false;
    bool mSyncing = false;
    bool mPanelUserSet = false;

    // a bar that floats at the top of the view and slides in / out (view overlay bar, editor toolbar)
    struct SlideBar {
        QWidget *widget = nullptr;
        QPropertyAnimation *anim = nullptr;
        QRect target;        // resting geometry, in this widget's coordinates
        bool shown = false;  // wanted state; the animation may still be on its way
        int maxWidth = 0;    // 0 = as wide as the available area
    };
    SlideBar mViewBar, mEditBar;

    void initSlideBar(SlideBar &bar, QWidget *widget, int maxWidth);
    void placeBar(SlideBar &bar, const QRect &barArea, WrapLayout *layout);
    void slideIn(SlideBar &bar);
    void slideOut(SlideBar &bar);
    void hideBarNow(SlideBar &bar);
    SlideBar &activeBar();
    bool barInUse(const SlideBar &bar) const;
    void buildToolbar();
    void buildPanel();
    void buildOverlay();
    void applyModeUi();
    void updateLayoutRows();
    void layoutOverlays();
    void layoutPanel();
    void showOverlay();
    void updateOverlayVisibility(bool reveal = false);
    void emitInfoIfChanged();
    QWidget *labeled(const QString &text, QWidget *control, QWidget *parent);
    void setButtonIcon(QPushButton *button, const QString &iconName);
    CollageItem *primaryItem() const;
    QList<CollageItem*> targets() const;
    void applyCanvasSize();
    void setAnimate(bool enabled);
    void applyEditLayout();
    void setCropMode(bool on);
    void applyCrop();
    bool freeLayoutActive() const;
    void applyGeometry(int source);
    void setNotice(const QString &text, bool warning = false);
    void updateStatus();
    void setPanelVisible(bool visible);
    void updateHelpVisibility();
    // layout options panel
    void buildLayoutPanel();
    void syncLayoutPanel();
    void applyLayoutPanel();
    void setLayoutPanelVisible(bool visible);
    bool layoutPanelAllowed() const;
    // tile outline default (view bar / editor toolbar popups)
    QToolButton *makeBorderButton(QWidget *parent);
    void syncBorderPopups();
    void applyDefaultBorder(int width, const QColor &color, int style);
    // view canvas (pixel size shared with the editor canvas, or the whole window)
    void restoreViewCanvas();
    void onViewCanvasChanged(int index);
    void applyViewCanvas(int width, int height);
    void syncViewCanvasCombo();
    void onViewBackgroundChanged();
    void syncBackgroundControls();
    // W / H of the selected tile in the view
    void applyViewSize(int source);
    void syncBorderModeCombos(int mode);
    static QString helpHtml(bool edit);
};
