#include "collagewidget.h"
#include "gui/collage/wraplayout.h"
#include <QApplication>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QSignalBlocker>
#include <QDir>
#include <QPair>
#include <QStyle>
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QGraphicsScene>
#include <QIcon>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QToolButton>
#include <QMenu>
#include <QWidgetAction>
#include <QScrollArea>
#include "settings.h"
#include "utils/imagelib.h"
#include "gui/collage/collagepresets.h"

namespace {

// aspect ratio choices for view tiles (0 = keep the image aspect)
const qreal VIEW_ASPECTS[] = { 0.0, 1.0, 4.0 / 3.0, 3.0 / 2.0, 16.0 / 9.0, 9.0 / 16.0, 2.0 / 3.0, 3.0 / 4.0 };
const int VIEW_ASPECT_COUNT = sizeof(VIEW_ASPECTS) / sizeof(VIEW_ASPECTS[0]);
// view "Canvas" combo: 0 = the whole window, 1..N = COLLAGE_PRESETS, N + 1 = custom size
const int CANVAS_CUSTOM = COLLAGE_PRESET_COUNT + 1;
// export buffers above this are refused by the scene (ARGB32)
const qint64 MAX_CANVAS_PIXELS = 150LL * 1000 * 1000;
// bar slide animation
const int SLIDE_IN_MS = 180;
const int SLIDE_OUT_MS = 150;
// the overlay bar shows up when the cursor is this close to the top edge
const int OVERLAY_TRIGGER = 70;

const int PANEL_WIDTH = 320;
const int MIN_PANEL_WIDTH = 260;
const int MAX_PANEL_WIDTH = 480;
const int LAYOUT_PANEL_WIDTH = 290;
const int GRIP_WIDTH = 6;
// panel card alpha: nearly solid so the text stays readable over any tile, no blur
const int PANEL_OPACITY = 242;
// gap between the floating panel and the edges of the view
const int PANEL_MARGIN = 10;
// the floating bar only makes room for the panel when this much width is left next to it
const int BAR_MIN_WIDTH = 360;
// below this width the properties panel starts hidden
const int NARROW_WIDTH = 900;
const qint64 MEMORY_WARNING = 1024LL * 1024 * 1024;

const int BUTTON_ICON_SIZE = 16;

// Icon tinted like the rest of the UI: base layer in the icon color, optional
// "<name>_accent" layer in the accent color. Always the @2x art, Qt scales it down on 1x screens.
QIcon themedIcon(const QString &iconName) {
    const QString dir = ":/res/icons/common/";
    auto layer = [&dir](const QString &name, const QColor &color) {
        QPixmap pixmap(dir + name + "@2x.png");
        if(pixmap.isNull())
            return pixmap;
        pixmap.setDevicePixelRatio(2.0);
        ImageLib::recolor(pixmap, color);
        return pixmap;
    };
    QPixmap result = layer(iconName, settings->colorScheme().icons);
    if(result.isNull())
        return QIcon();
    QPixmap accent = layer(iconName + "_accent", settings->colorScheme().accent);
    if(!accent.isNull() && accent.size() == result.size()) {
        QPainter painter(&result);
        painter.drawPixmap(0, 0, accent);
    }
    return QIcon(result);
}

int presetIndexFor(int width, int height) {
    for(int i = 0; i < COLLAGE_PRESET_COUNT; i++) {
        if(COLLAGE_PRESETS[i].width == width && COLLAGE_PRESETS[i].height == height)
            return i;
    }
    return -1;
}

const char *BORDER_STYLE_NAMES[] = {
    QT_TRANSLATE_NOOP("CollageWidget", "Solid"),
    QT_TRANSLATE_NOOP("CollageWidget", "Dashed"),
    QT_TRANSLATE_NOOP("CollageWidget", "Dotted"),
    QT_TRANSLATE_NOOP("CollageWidget", "Dash-dot"),
    QT_TRANSLATE_NOOP("CollageWidget", "Double")
};

QString megabytes(qint64 bytes) {
    return QString::number(bytes / (1024.0 * 1024.0), 'f', bytes < 10LL * 1024 * 1024 ? 1 : 0) + " MB";
}

}

CollagePanelFrame::CollagePanelFrame(QWidget *parent) : QWidget(parent) {
    setAccessibleName("CollagePanelContainer");
}

void CollagePanelFrame::setResizeEdge(Qt::Edges edge) {
    mEdge = edge;
    if(!mEdge) {
        delete mGrip;
        mGrip = nullptr;
        return;
    }
    if(!mGrip) {
        mGrip = new QWidget(this);
        mGrip->setCursor(Qt::SizeHorCursor);
        mGrip->setToolTip(tr("Drag to resize the panel"));
        mGrip->installEventFilter(this);
    }
    placeGrip();
}

void CollagePanelFrame::placeGrip() {
    if(!mGrip)
        return;
    int x = (mEdge & Qt::LeftEdge) ? 0 : width() - GRIP_WIDTH;
    mGrip->setGeometry(x, 8, GRIP_WIDTH, qMax(0, height() - 16));
    mGrip->raise();
}

void CollagePanelFrame::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    placeGrip();
}

bool CollagePanelFrame::eventFilter(QObject *watched, QEvent *event) {
    if(watched != mGrip)
        return QWidget::eventFilter(watched, event);
    switch(event->type()) {
    case QEvent::MouseButtonPress: {
        auto *e = static_cast<QMouseEvent*>(event);
        if(e->button() != Qt::LeftButton)
            return false;
        mResizing = true;
        mStartGlobalX = QCursor::pos().x();
        mStartWidth = width();
        return true;
    }
    case QEvent::MouseMove:
        if(mResizing) {
            int dx = QCursor::pos().x() - mStartGlobalX;
            emit widthDragged((mEdge & Qt::LeftEdge) ? mStartWidth - dx : mStartWidth + dx);
            return true;
        }
        return false;
    case QEvent::MouseButtonRelease:
        if(mResizing) {
            mResizing = false;
            emit resizeFinished(width());
            return true;
        }
        return false;
    default:
        return false;
    }
}

void CollagePanelFrame::paintEvent(QPaintEvent *) {
    const ColorScheme &colors = settings->colorScheme();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(box, 8, 8);
    painter.setClipPath(path);
    QColor fill = colors.widget;
    fill.setAlpha(PANEL_OPACITY);
    painter.fillPath(path, fill);
    painter.setClipping(false);
    painter.setPen(QPen(colors.widget_border, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

CollageWidget::CollageWidget(QWidget *parent) : QWidget(parent) {
    setAccessibleName("CollageWidget");

    mScene = new CollageScene(this);
    mView = new CollageView(mScene, this);
    mStatusLabel = new QLabel(this);
    mStatusLabel->setAccessibleName("CollageStatus");

    mPanelWidth = settings->collagePanelWidth();
    buildToolbar();
    buildPanel();
    buildOverlay();
    buildLayoutPanel();
    connect(settings, &Settings::settingsChanged, this, &CollageWidget::updateButtonIcons);
    connect(mView, &CollageView::animationToggleRequested, this, [this]() { setAnimate(!mScene->animationsEnabled()); });
    setAnimate(settings->collageAnimate());

    QHBoxLayout *body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(mView, 1); // the properties panel floats above the view instead of taking room from it

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(body, 1); // the editor toolbar floats over the view, see layoutOverlays()
    root->addWidget(mStatusLabel);

    connect(mView, &CollageView::filesDropped, this, &CollageWidget::addImages);
    connect(mScene, &QGraphicsScene::selectionChanged, this, &CollageWidget::updatePanel);
    connect(mScene, &CollageScene::itemEdited, this, &CollageWidget::updatePanel);
    connect(mScene, &CollageScene::collageChanged, this, &CollageWidget::onSceneChanged);
    connect(mScene, &CollageScene::itemActivated, this, [this](CollageItem *item) {
        if(isEditMode())
            return; // a static canvas makes the editor frames behave like tiles: stay in the editor
        emit openImageRequested(item->path());
    });
    connect(mScene, &CollageScene::itemContextRequested, this, [this](CollageItem *) {
        if(mScene->mode() == CollageMode::View)
            setPanelVisible(true);
    });
    connect(mView, &CollageView::modeToggleRequested, this, [this]() {
        setMode(isEditMode() ? CollageMode::View : CollageMode::Edit);
    });
    mView->viewport()->installEventFilter(this);

    {
        RulerHost host;
        host.view = mView;
        CollageView *view = mView;
        host.docToViewport = [view](QTransform &t) { return view->canvasToViewport(t); };
        host.setMargins = [view](int left, int top) { view->setRulerMargins(left, top); };
        host.docSize = [view]() { return view->canvasRectSize(); };
        mRulers = new RulerController(host, "collage", this);
        connect(mRulers, &RulerController::marginChanged, this, &CollageWidget::layoutOverlays);
    }

    connect(settings, &Settings::settingsChanged, this, [this]() {
        mPanelContainer->update();
        mLayoutPanel->update();
        syncBorderPopups(); // a theme change changes the "theme colour" outline
        mScene->update();
        layoutOverlays(); // the font may have changed: bars re-measure their height
    });

    mScene->setCanvasSize(QSize(COLLAGE_PRESETS[0].width, COLLAGE_PRESETS[0].height));
    {
        QSignalBlocker blockLayout(mLayoutCombo);
        mLayoutCombo->setCurrentIndex(settings->collageEditLayout());
    }
    applyEditLayout();
    restoreViewCanvas();
    syncBackgroundControls();
    syncBorderPopups();
    updateStatus();
    applyModeUi(); // the scene starts in view mode
}

CollageMode CollageWidget::currentMode() const {
    return mScene->mode();
}

bool CollageWidget::isEditMode() const {
    return mScene->mode() == CollageMode::Edit;
}

void CollageWidget::setMode(CollageMode mode) {
    if(mode == mScene->mode())
        return;
    // the editor starts from a mosaic the first time it is opened (the automatic layouts arrange themselves)
    bool firstEdit = (mode == CollageMode::Edit && !mScene->editorInitialized());
    mScene->setMode(mode);
    applyModeUi();
    if(firstEdit && mScene->imageCount() > 0 && mLayoutCombo->currentIndex() == CollageLayout::MODE_FREEFORM)
        mScene->applyLayout(CollageLayout::MODE_MOSAIC, mGapSpin->value());
    emitInfoIfChanged();
}

// shows / hides the chrome that belongs to the current mode
void CollageWidget::applyModeUi() {
    bool view = !isEditMode();
    mView->setViewMode(view);
    mStatusLabel->setVisible(!view);
    updateLayoutRows();
    bool panel = view ? mViewPanelOpen : (mPanelUserSet ? mEditPanelVisible : width() >= NARROW_WIDTH);
    setPanelVisible(panel);
    updateHelpVisibility();
    updatePanel();
    layoutOverlays();
    updateOverlayVisibility(true); // the bar of the new mode slides in
    mView->setFocus();
}

// panel rows / bar controls that depend on the mode and, in the view, on the layout
void CollageWidget::updateLayoutRows() {
    bool view = !isEditMode();
    bool free = view && mScene->isFreeView();
    bool freehand = freeLayoutActive();
    for(QWidget *widget : mEditOnlyWidgets)
        widget->setVisible(!view);
    // aspect / size only steer the automatic layouts
    for(QWidget *widget : mViewOnlyWidgets)
        widget->setVisible(view && !free);
    for(QWidget *widget : mStackWidgets)
        widget->setVisible(!view || free);
    // automatic editor layouts place the frames, so manual geometry is locked
    bool locked = !view && mScene->staticCanvas();
    for(QWidget *widget : mEditOnlyWidgets)
        widget->setEnabled(!locked);
    for(QWidget *widget : mStackWidgets)
        widget->setEnabled(!locked);
    mViewGapSpin->setEnabled(!free); // tiles are placed by hand in Freehand, so there is no gap to apply
    mViewShapeCombo->setEnabled(!free); // Freehand always uses the whole window
    mViewCustomBox->setVisible(!free && mViewShapeCombo->currentIndex() == CANVAS_CUSTOM);
    mGapSpin->setEnabled(!(!view && freehand));
    for(QWidget *widget : mViewSizeWidgets)
        widget->setVisible(view);
    bool autoLayout = mScene->activeLayout() != CollageLayout::MODE_FREEFORM;
    for(QWidget *widget : mBorderModeWidgets)
        widget->setVisible(autoLayout);
    // the layout options panel belongs to Grid / Row / Column
    bool cells = layoutPanelAllowed();
    mViewLayoutPanelButton->setVisible(view && cells);
    mEditLayoutPanelButton->setVisible(!view && cells);
    mLayoutPanel->setVisible(cells && mLayoutPanelWanted);
    syncLayoutPanel();
    // dragging already pans the picture in the automatic layouts, so Crop only exists for Freehand
    mViewCropButton->setVisible(free);
    mEditCropButton->setVisible(!view && freehand);
    applyCrop();
    layoutOverlays(); // hidden / shown bar controls change the bar height
}

//------------------------------------------------------------------------------
// view mode chrome: floating bar (auto-hides) + hint for an empty collage
void CollageWidget::buildOverlay() {
    mOverlayBar = new QWidget(this);
    mOverlayBar->setAccessibleName("CollageOverlayBar");
    mOverlayLayout = new WrapLayout(mOverlayBar, 8, 8, 6);

    QPushButton *exitButton = new QPushButton(tr("Exit"), mOverlayBar);
    exitButton->setToolTip(tr("Exit the collage and go back to the image viewer (Esc). It asks first; "
                              "the collage stays in memory, Ctrl+G resumes it."));
    setButtonIcon(exitButton, "buttons/panel/back20");
    connect(exitButton, &QPushButton::clicked, this, &CollageWidget::exitRequested);

    QPushButton *addButton = new QPushButton(tr("+ Add"), mOverlayBar);
    addButton->setToolTip(tr("Add images (you can also drop files here)"));
    connect(addButton, &QPushButton::clicked, this, &CollageWidget::onAddImages);

    mViewLayoutCombo = new QComboBox(mOverlayBar);
    mViewLayoutCombo->addItems({ tr("Mosaic"), tr("Grid"), tr("Row"), tr("Column"), tr("Freehand") });
    mViewLayoutCombo->setToolTip(tr("Mosaic, Grid, Row and Column arrange the tiles for you (drag the border between two tiles to resize them). "
                                    "Freehand lets you place and resize them yourself."));
    connect(mViewLayoutCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if(index < 0)
            return;
        mScene->setViewLayout(static_cast<CollageLayout::Mode>(index));
        mView->fitCanvas(); // Freehand has a bigger scene than the other layouts
        updateLayoutRows();
        updatePanel();
        mView->setFocus();
    });

    mViewLayoutPanelButton = new QPushButton(tr("Layout options"), mOverlayBar);
    mViewLayoutPanelButton->setCheckable(true);
    mViewLayoutPanelButton->setToolTip(tr("Grid / Row / Column: number of columns and rows, cell size, rotation and cell style"));
    connect(mViewLayoutPanelButton, &QPushButton::toggled, this, &CollageWidget::setLayoutPanelVisible);

    // canvas: the whole window, a pixel resolution (shared with the editor canvas) or a custom size
    mViewShapeCombo = new QComboBox(mOverlayBar);
    mViewShapeCombo->addItem(tr("Window (fit)"));
    for(int i = 0; i < COLLAGE_PRESET_COUNT; i++)
        mViewShapeCombo->addItem(tr(COLLAGE_PRESETS[i].name));
    mViewShapeCombo->addItem(tr("Custom..."));
    mViewShapeCombo->setToolTip(tr("Canvas of the collage: fill the window, or a frame with this pixel resolution. "
                                   "The editor uses the same size for the export."));
    mViewShapeCombo->setFocusPolicy(Qt::NoFocus);
    connect(mViewShapeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &CollageWidget::onViewCanvasChanged);

    mViewCustomBox = new QWidget(mOverlayBar);
    QHBoxLayout *customLayout = new QHBoxLayout(mViewCustomBox);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customLayout->setSpacing(6);
    mViewCanvasWSpin = new ScrubSpinBox(mViewCustomBox);
    mViewCanvasHSpin = new ScrubSpinBox(mViewCustomBox);
    for(ScrubSpinBox *spin : { mViewCanvasWSpin, mViewCanvasHSpin }) {
        spin->setRange(16, 16384);
        spin->setSuffix(" px");
        spin->setToolTip(tr("Custom canvas size. Drag sideways to change it (Shift = faster, Alt = finer), or click to type."));
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
            if(!mSyncing)
                applyViewCanvas(mViewCanvasWSpin->value(), mViewCanvasHSpin->value());
        });
    }
    mViewCanvasWSpin->setPrefix("W  ");
    mViewCanvasHSpin->setPrefix("H  ");
    customLayout->addWidget(mViewCanvasWSpin);
    customLayout->addWidget(mViewCanvasHSpin);
    mViewCustomBox->hide();

    mViewGapSpin = new ScrubSpinBox(mOverlayBar);
    mViewGapSpin->setRange(0, 100);
    mViewGapSpin->setValue(mScene->viewGap());
    mViewGapSpin->setSuffix(" px");
    connect(mViewGapSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        mScene->setViewGap(value);
        syncLayoutPanel();
    });

    // background: theme colour + pattern, or a colour of its own (= the canvas colour of the editor)
    mViewBgButton = new ColorSelectorButton(mOverlayBar);
    mViewBgButton->setMinimumSize(40, 22);
    mViewBgButton->setCursor(Qt::PointingHandCursor);
    mViewBgButton->setDescription(tr("Collage background"));
    mViewColorBgCheck = new QCheckBox(tr("Color"), mOverlayBar);
    mViewColorBgCheck->setToolTip(tr("Paint the collage with the colour on the left (the same colour as the editor canvas). "
                                     "Off: the app background and pattern show."));
    mViewColorBgCheck->setFocusPolicy(Qt::NoFocus);
    connect(mViewColorBgCheck, &QCheckBox::toggled, this, &CollageWidget::onViewBackgroundChanged);
    mViewBgOpacitySpin = new ScrubSpinBox(mOverlayBar);
    mViewBgOpacitySpin->setRange(0, 100);
    mViewBgOpacitySpin->setSuffix(" %");
    mViewBgOpacitySpin->setToolTip(tr("Opacity of the background colour. Drag sideways to change it (Shift = faster, Alt = finer), or click to type."));
    connect(mViewBgOpacitySpin, qOverload<int>(&QSpinBox::valueChanged), this, &CollageWidget::onViewBackgroundChanged);
    connect(mViewBgButton, &ColorSelectorButton::colorChanged, this, [this](const QColor &color) {
        // a picked colour is the collage colour: shared with the editor canvas, opaque
        QColor opaque(color.red(), color.green(), color.blue());
        {
            QSignalBlocker blockColor(mBackgroundButton);
            QSignalBlocker blockTransparent(mTransparentCheck);
            mBackgroundButton->setColor(opaque);
            mTransparentCheck->setChecked(false);
            mBackgroundButton->setEnabled(true);
        }
        mScene->setCanvasColor(opaque);
        QSignalBlocker blockColorCheck(mViewColorBgCheck);
        mViewColorBgCheck->setChecked(true);
        onViewBackgroundChanged();
    });
    QWidget *backgroundGroup = labeled(tr("Background"), mViewBgButton, mOverlayBar);
    backgroundGroup->layout()->addWidget(mViewColorBgCheck);
    backgroundGroup->layout()->addWidget(mViewBgOpacitySpin);

    QToolButton *borderButton = makeBorderButton(mOverlayBar);

    mViewAnimateCheck = new QCheckBox(tr("Animate"), mOverlayBar);
    mViewAnimateCheck->setToolTip(tr("Play animated images (GIF, animated WebP / PNG). Space toggles it."));
    mViewAnimateCheck->setFocusPolicy(Qt::NoFocus);
    connect(mViewAnimateCheck, &QCheckBox::toggled, this, &CollageWidget::setAnimate);

    mViewCropButton = new QPushButton(tr("Crop"), mOverlayBar);
    mViewCropButton->setCheckable(true);
    mViewCropButton->setToolTip(tr("Crop: drag moves the picture inside its tile, the wheel zooms it (resize handles are off while on)"));
    mViewCropButton->setFocusPolicy(Qt::NoFocus);
    mViewCropButton->setVisible(false);
    connect(mViewCropButton, &QPushButton::toggled, this, &CollageWidget::setCropMode);

    mViewPanelButton = new QPushButton(tr("Tile settings"), mOverlayBar);
    mViewPanelButton->setCheckable(true);
    mViewPanelButton->setToolTip(tr("Aspect ratio, size, crop, outline and resolution of the selected tile (also: right-click a tile)"));
    connect(mViewPanelButton, &QPushButton::toggled, this, [this](bool checked) { setPanelVisible(checked); });

    QPushButton *editButton = new QPushButton(tr("Edit / Export"), mOverlayBar);
    editButton->setToolTip(tr("Arrange freely on a fixed canvas and save the collage as an image (E)"));
    connect(editButton, &QPushButton::clicked, this, [this]() { setMode(CollageMode::Edit); });

    for(QPushButton *button : {exitButton, addButton, mViewPanelButton, editButton, mViewLayoutPanelButton})
        button->setFocusPolicy(Qt::NoFocus);
    mViewLayoutCombo->setFocusPolicy(Qt::NoFocus);

    mOverlayLayout->addWidget(exitButton);
    mOverlayLayout->addWidget(labeled(tr("Layout"), mViewLayoutCombo, mOverlayBar));
    mOverlayLayout->addWidget(mViewLayoutPanelButton);
    mOverlayLayout->addWidget(labeled(tr("Canvas"), mViewShapeCombo, mOverlayBar));
    mOverlayLayout->addWidget(mViewCustomBox);
    mOverlayLayout->addWidget(labeled(tr("Gap"), mViewGapSpin, mOverlayBar));
    mOverlayLayout->addWidget(backgroundGroup);
    mOverlayLayout->addWidget(borderButton);
    mOverlayLayout->addWidget(mViewAnimateCheck);
    mOverlayLayout->addWidget(mViewCropButton);
    mOverlayLayout->addWidget(mViewPanelButton);
    mOverlayLayout->addWidget(editButton);
    mOverlayLayout->addWidget(addButton);
    mOverlayLayout->setTrailingWidget(addButton); // far right, the rest wraps left of it

    initSlideBar(mViewBar, mOverlayBar, 1200);
    initSlideBar(mEditBar, mToolbar, 0);

    mOverlayTimer = new QTimer(this);
    mOverlayTimer->setSingleShot(true);
    mOverlayTimer->setInterval(2200);
    connect(mOverlayTimer, &QTimer::timeout, this, [this]() {
        SlideBar &bar = activeBar();
        // keep it while it is being used (hover, open popup / dialog, typing in it) or while there is nothing to look at
        if(!bar.shown)
            return;
        if(barInUse(bar) || mScene->imageCount() == 0) {
            mOverlayTimer->start();
            return;
        }
        slideOut(bar);
    });

    mEmptyHint = new QLabel(tr("Add images or drop files here"), this);
    mEmptyHint->setAccessibleName("CollageEmptyHint");
    mEmptyHint->setAlignment(Qt::AlignCenter);
    mEmptyHint->setAttribute(Qt::WA_TransparentForMouseEvents);

    mOverlayBar->hide();
    mEmptyHint->hide();
}

//------------------------------------------------------------------------------
// tile outline popup (view bar and editor toolbar share the settings)
QToolButton *CollageWidget::makeBorderButton(QWidget *parent) {
    QToolButton *button = new QToolButton(parent);
    button->setText(tr("Border"));
    button->setToolTip(tr("Outline of every tile: width, colour and style. A tile can have its own (Tile settings)."));
    button->setPopupMode(QToolButton::InstantPopup);
    button->setFocusPolicy(Qt::NoFocus);
    button->setAccessibleName("CollageBarToolButton");
    QMenu *menu = new QMenu(button);
    menu->setAccessibleName("CollagePopup");
    QWidget *card = new QWidget(menu);
    card->setAccessibleName("CollagePopupCard");
    QGridLayout *grid = new QGridLayout(card);
    grid->setContentsMargins(12, 10, 12, 10);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);

    BorderPopup popup;
    popup.width = new ScrubSpinBox(card);
    popup.width->setRange(0, 64);
    popup.width->setSuffix(" px");
    popup.width->setSpecialValueText(tr("None"));
    popup.width->setToolTip(tr("Drag sideways to change it, or click to type"));
    popup.color = new ColorSelectorButton(card);
    popup.color->setMinimumSize(40, 22);
    popup.color->setCursor(Qt::PointingHandCursor);
    popup.color->setDescription(tr("Tile outline"));
    popup.themeColor = new QCheckBox(tr("Theme colour"), card);
    popup.style = new QComboBox(card);
    for(const char *name : BORDER_STYLE_NAMES)
        popup.style->addItem(tr(name));
    grid->addWidget(new QLabel(tr("Width"), card), 0, 0);
    grid->addWidget(popup.width, 0, 1, 1, 2);
    grid->addWidget(new QLabel(tr("Colour"), card), 1, 0);
    grid->addWidget(popup.color, 1, 1);
    grid->addWidget(popup.themeColor, 1, 2);
    grid->addWidget(new QLabel(tr("Style"), card), 2, 0);
    grid->addWidget(popup.style, 2, 1, 1, 2);

    QWidgetAction *action = new QWidgetAction(menu);
    action->setDefaultWidget(card);
    menu->addAction(action);
    button->setMenu(menu);
    connect(menu, &QMenu::aboutToShow, this, &CollageWidget::syncBorderPopups);

    auto apply = [this, popup]() {
        if(mSyncing)
            return;
        QColor color = popup.themeColor->isChecked() ? QColor() : popup.color->color();
        applyDefaultBorder(popup.width->value(), color, popup.style->currentIndex());
    };
    connect(popup.width, qOverload<int>(&QSpinBox::valueChanged), this, apply);
    connect(popup.style, qOverload<int>(&QComboBox::currentIndexChanged), this, apply);
    connect(popup.themeColor, &QCheckBox::toggled, this, apply);
    connect(popup.color, &ColorSelectorButton::colorChanged, this, [popup, apply]() {
        QSignalBlocker block(popup.themeColor);
        popup.themeColor->setChecked(false);
        apply();
    });
    mBorderPopups.append(popup);
    return button;
}

void CollageWidget::applyDefaultBorder(int width, const QColor &color, int style) {
    settings->setCollageBorderWidth(width);
    settings->setCollageBorderColor(color);
    settings->setCollageBorderStyle(style);
    mScene->setDefaultBorder(width, color, style);
    syncBorderPopups();
    updatePanel();
}

void CollageWidget::syncBorderPopups() {
    bool wasSyncing = mSyncing;
    mSyncing = true;
    QColor color = settings->collageBorderColor();
    for(const BorderPopup &popup : mBorderPopups) {
        popup.width->setValue(settings->collageBorderWidth());
        popup.themeColor->setChecked(!color.isValid());
        popup.color->setColor(color.isValid() ? color : settings->colorScheme().widget_border);
        popup.style->setCurrentIndex(settings->collageBorderStyle());
    }
    mSyncing = wasSyncing;
}

//------------------------------------------------------------------------------
// view canvas: "window" fills the viewer, a pixel size becomes the canvas of view and editor alike
void CollageWidget::restoreViewCanvas() {
    QString saved = settings->collageViewCanvas();
    QStringList parts = saved.split('x');
    int w = 0, h = 0;
    if(parts.size() == 2) {
        w = parts.at(0).toInt();
        h = parts.at(1).toInt();
    }
    if(w <= 0 || h <= 0) {
        QSignalBlocker block(mViewShapeCombo);
        mViewShapeCombo->setCurrentIndex(0);
        mScene->setViewShape(0.0);
        return;
    }
    int preset = presetIndexFor(w, h);
    {
        QSignalBlocker block(mViewShapeCombo);
        mViewShapeCombo->setCurrentIndex(preset >= 0 ? preset + 1 : CANVAS_CUSTOM);
    }
    applyViewCanvas(w, h);
}

void CollageWidget::onViewCanvasChanged(int index) {
    if(mSyncing || index < 0)
        return;
    if(index == 0) {
        mScene->setViewShape(0.0);
        settings->setCollageViewCanvas("window");
    } else if(index <= COLLAGE_PRESET_COUNT) {
        const CollagePreset &preset = COLLAGE_PRESETS[index - 1];
        applyViewCanvas(preset.width, preset.height);
    } else {
        QSize size = mScene->canvasSize();
        applyViewCanvas(size.width(), size.height());
    }
    updateLayoutRows(); // shows / hides the custom size fields
    mView->fitCanvas();
    mView->setFocus();
}

void CollageWidget::applyViewCanvas(int width, int height) {
    width = qBound(16, width, 16384);
    height = qBound(16, height, 16384);
    if(static_cast<qint64>(width) * height > MAX_CANVAS_PIXELS) {
        height = static_cast<int>(MAX_CANVAS_PIXELS / width);
        setNotice(tr("Canvas limited to 150 megapixels (the largest image that can be exported)."), true);
    }
    mScene->setCanvasSize(QSize(width, height));
    mScene->setViewShape(static_cast<qreal>(width) / height);
    settings->setCollageViewCanvas(QString("%1x%2").arg(width).arg(height));
    bool wasSyncing = mSyncing;
    mSyncing = true;
    {
        // the editor shows the same canvas
        QSignalBlocker blockW(mCanvasWidthSpin);
        QSignalBlocker blockH(mCanvasHeightSpin);
        QSignalBlocker blockPreset(mPresetCombo);
        mCanvasWidthSpin->setValue(width);
        mCanvasHeightSpin->setValue(height);
        int preset = presetIndexFor(width, height);
        mPresetCombo->setCurrentIndex(preset >= 0 ? preset : COLLAGE_PRESET_COUNT);
    }
    mViewCanvasWSpin->setValue(width);
    mViewCanvasHSpin->setValue(height);
    mSyncing = wasSyncing;
    if(mView->isAutoFit())
        mView->fitCanvas();
    updateStatus();
}

// the editor canvas changed: a view that uses a pixel canvas follows it, "Window" stays the window
void CollageWidget::syncViewCanvasCombo() {
    if(mViewShapeCombo->currentIndex() == 0)
        return;
    QSize size = mScene->canvasSize();
    int preset = presetIndexFor(size.width(), size.height());
    bool wasSyncing = mSyncing;
    mSyncing = true;
    {
        QSignalBlocker block(mViewShapeCombo);
        mViewShapeCombo->setCurrentIndex(preset >= 0 ? preset + 1 : CANVAS_CUSTOM);
    }
    mViewCanvasWSpin->setValue(size.width());
    mViewCanvasHSpin->setValue(size.height());
    mSyncing = wasSyncing;
    mScene->setViewShape(static_cast<qreal>(size.width()) / size.height());
    settings->setCollageViewCanvas(QString("%1x%2").arg(size.width()).arg(size.height()));
    updateLayoutRows();
}

void CollageWidget::onViewBackgroundChanged() {
    bool colorOn = mViewColorBgCheck->isChecked();
    mViewBgOpacitySpin->setEnabled(colorOn);
    settings->setCollageViewThemeBackground(!colorOn);
    settings->setCollageViewBgOpacity(mViewBgOpacitySpin->value());
    mScene->setBackgroundOpacity(mViewBgOpacitySpin->value());
    mScene->setThemeBackground(!colorOn);
}

void CollageWidget::syncBackgroundControls() {
    QSignalBlocker blockCheck(mViewColorBgCheck);
    QSignalBlocker blockOpacity(mViewBgOpacitySpin);
    QSignalBlocker blockColor(mViewBgButton);
    mViewColorBgCheck->setChecked(!settings->collageViewThemeBackground());
    mViewBgOpacitySpin->setValue(settings->collageViewBgOpacity());
    mViewBgOpacitySpin->setEnabled(mViewColorBgCheck->isChecked());
    QColor color = mScene->canvasColor();
    mViewBgButton->setColor(QColor(color.red(), color.green(), color.blue()));
    mScene->setThemeBackground(!mViewColorBgCheck->isChecked());
    mScene->setBackgroundOpacity(mViewBgOpacitySpin->value());
}

// panel: a floating card at the right edge of the view
QRect CollageWidget::contentArea() const {
    int t = mRulers ? mRulers->thickness() : 0;
    return mView->geometry().adjusted(t, t, 0, 0);
}

void CollageWidget::layoutPanel() {
    QRect area = contentArea();
    int height = qMax(120, area.height() - 2 * PANEL_MARGIN);
    int width = qMax(160, qMin(mPanelWidth, area.width() - 2 * PANEL_MARGIN));
    mPanelContainer->setGeometry(area.right() - width - PANEL_MARGIN + 1, area.y() + PANEL_MARGIN, width, height);
    // layout options card: left edge
    int layoutWidth = qMax(160, qMin(LAYOUT_PANEL_WIDTH, area.width() - 2 * PANEL_MARGIN));
    mLayoutPanel->setGeometry(area.x() + PANEL_MARGIN, area.y() + PANEL_MARGIN, layoutWidth, height);
}

// bar centered at the top of the view (beside the panel when it is open), wraps on narrow windows
void CollageWidget::layoutOverlays() {
    QRect area = contentArea();
    mEmptyHint->setGeometry(area);
    mEmptyHint->raise();
    layoutPanel();
    mPanelContainer->raise();

    mLayoutPanel->raise();
    // the bar sits between the floating cards when there is room for it
    QRect barArea = area;
    if(mPanelContainer->isVisible()) {
        int room = mPanelContainer->x() - PANEL_MARGIN - barArea.x();
        if(room >= BAR_MIN_WIDTH)
            barArea.setWidth(room);
    }
    if(mLayoutPanel->isVisible()) {
        int left = mLayoutPanel->geometry().right() + 1;
        if(barArea.right() - left >= BAR_MIN_WIDTH)
            barArea.setLeft(left);
    }
    placeBar(mViewBar, barArea, mOverlayLayout);
    placeBar(mEditBar, barArea, mToolbarLayout);
}

//------------------------------------------------------------------------------
// sliding bars
void CollageWidget::initSlideBar(SlideBar &bar, QWidget *widget, int maxWidth) {
    bar.widget = widget;
    bar.maxWidth = maxWidth;
    bar.anim = new QPropertyAnimation(widget, "pos", this);
    widget->hide();
    // a finished slide-out hides the bar; a slide-in that finished (or was interrupted) leaves shown == true
    connect(bar.anim, &QPropertyAnimation::finished, this, [&bar]() {
        if(!bar.shown)
            bar.widget->hide();
    });
}

CollageWidget::SlideBar &CollageWidget::activeBar() {
    return isEditMode() ? mEditBar : mViewBar;
}

// the bar stays while it is hovered, a combo popup / modal dialog is open, or a control inside it has the focus
bool CollageWidget::barInUse(const SlideBar &bar) const {
    QWidget *focus = QApplication::focusWidget();
    return bar.widget->underMouse()
           || QApplication::activePopupWidget()
           || QApplication::activeModalWidget()
           || (focus && bar.widget->isAncestorOf(focus));
}

// computes the resting geometry of a bar; a bar that is on screen (or sliding) follows it
void CollageWidget::placeBar(SlideBar &bar, const QRect &barArea, WrapLayout *layout) {
    int maxWidth = bar.maxWidth > 0 ? bar.maxWidth : barArea.width();
    int width = qMax(120, qMin(barArea.width() - 20, maxWidth));
    int height = layout->heightForWidth(width);
    bar.target = QRect(barArea.x() + (barArea.width() - width) / 2, barArea.y() + 10, width, height);
    bar.widget->resize(bar.target.size());
    QPoint hidden(bar.target.x(), -height - 2);
    bool running = bar.anim->state() == QAbstractAnimation::Running;
    if(bar.shown) {
        if(running)
            bar.anim->setEndValue(bar.target.topLeft());
        else if(bar.widget->isVisible())
            bar.widget->move(bar.target.topLeft());
        else
            bar.widget->move(hidden);
    } else if(running) {
        bar.anim->setEndValue(hidden);
    } else if(!bar.widget->isVisible()) {
        bar.widget->move(hidden);
    }
    if(bar.widget->isVisible())
        bar.widget->raise();
}

void CollageWidget::slideIn(SlideBar &bar) {
    bar.shown = true;
    bar.anim->stop();
    QPoint hidden(bar.target.x(), -bar.target.height() - 2);
    if(!bar.widget->isVisible())
        bar.widget->move(hidden);
    bar.widget->show();
    bar.widget->raise();
    bar.anim->setDuration(SLIDE_IN_MS);
    bar.anim->setEasingCurve(QEasingCurve::OutCubic);
    bar.anim->setStartValue(bar.widget->pos());
    bar.anim->setEndValue(bar.target.topLeft());
    bar.anim->start();
}

void CollageWidget::slideOut(SlideBar &bar) {
    if(!bar.widget->isVisible()) {
        bar.shown = false;
        return;
    }
    bar.anim->stop(); // before clearing 'shown': a stop may emit finished()
    bar.shown = false;
    bar.anim->setDuration(SLIDE_OUT_MS);
    bar.anim->setEasingCurve(QEasingCurve::InCubic);
    bar.anim->setStartValue(bar.widget->pos());
    bar.anim->setEndValue(QPoint(bar.target.x(), -bar.target.height() - 2));
    bar.anim->start();
}

void CollageWidget::hideBarNow(SlideBar &bar) {
    bar.shown = false;
    bar.anim->stop();
    bar.widget->hide();
}

void CollageWidget::showOverlay() {
    SlideBar &bar = activeBar();
    if(bar.shown) {
        mOverlayTimer->start(); // already out: just keep it a while longer
        return;
    }
    layoutOverlays();
    slideIn(bar);
    mOverlayTimer->start();
}

// reveal: slide the bar of the current mode in from the top (mode switch / collage shown) and arm the auto-hide
void CollageWidget::updateOverlayVisibility(bool reveal) {
    bool view = !isEditMode();
    SlideBar &active = view ? mViewBar : mEditBar;
    hideBarNow(view ? mEditBar : mViewBar);
    bool empty = (mScene->imageCount() == 0);
    mEmptyHint->setVisible(view && empty);
    if(reveal) {
        hideBarNow(active);
        layoutOverlays();
        slideIn(active);
        mOverlayTimer->start();
    } else if(empty) {
        layoutOverlays();
        if(!active.shown)
            slideIn(active);
    } else if(active.shown && !mOverlayTimer->isActive()) {
        mOverlayTimer->start();
    }
}

bool CollageWidget::eventFilter(QObject *watched, QEvent *event) {
    if(watched == mView->viewport()) {
        if(event->type() == QEvent::MouseMove) {
            auto *move = static_cast<QMouseEvent*>(event);
            // near the top edge, and not while a drag (pan / move / rubber band) is going on
            if(move->pos().y() < OVERLAY_TRIGGER && move->buttons() == Qt::NoButton)
                showOverlay();
        } else if(event->type() == QEvent::Resize) {
            layoutOverlays();
        }
    }
    return QWidget::eventFilter(watched, event);
}

// window title / top bar only care about mode and count
void CollageWidget::emitInfoIfChanged() {
    int key = (isEditMode() ? 1000000 : 0) + mScene->imageCount();
    if(key == mLastInfoKey)
        return;
    mLastInfoKey = key;
    emit infoChanged();
}

//------------------------------------------------------------------------------
// icon names are relative to res/icons/common/, without "@2x" and extension
void CollageWidget::setButtonIcon(QPushButton *button, const QString &iconName) {
    button->setIcon(themedIcon(iconName));
    button->setIconSize(QSize(BUTTON_ICON_SIZE, BUTTON_ICON_SIZE));
    mIconButtons << qMakePair(button, iconName);
}

void CollageWidget::updateButtonIcons() {
    for(const auto &entry : mIconButtons)
        entry.first->setIcon(themedIcon(entry.second));
}

QWidget *CollageWidget::labeled(const QString &text, QWidget *control, QWidget *parent) {
    QWidget *group = new QWidget(parent);
    QHBoxLayout *layout = new QHBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    if(!text.isEmpty())
        layout->addWidget(new QLabel(text, group));
    layout->addWidget(control);
    return group;
}

void CollageWidget::buildToolbar() {
    mToolbar = new QWidget(this);
    mToolbar->setAccessibleName("CollageToolbar");
    mToolbarLayout = new WrapLayout(mToolbar, 8, 12, 6);
    WrapLayout *layout = mToolbarLayout;
    mToolbar->hide();

    auto makeButton = [this](const QString &text, const QString &tip, const QString &iconName = QString()) {
        QPushButton *button = new QPushButton(text, mToolbar);
        button->setToolTip(tip);
        button->setFocusPolicy(Qt::NoFocus);
        if(!iconName.isEmpty())
            setButtonIcon(button, iconName);
        return button;
    };

    QPushButton *viewButton = makeButton(tr("Back to collage view"), tr("Back to the collage view (E)"), "buttons/panel/back20");
    connect(viewButton, &QPushButton::clicked, this, [this]() { setMode(CollageMode::View); });
    layout->addWidget(viewButton);

    QPushButton *addButton = makeButton(tr("+ Add images..."), tr("Add more images to the collage (you can also drop files here)"));
    QPushButton *newButton = makeButton(tr("New"), tr("Remove all images and start over"));
    connect(addButton, &QPushButton::clicked, this, &CollageWidget::onAddImages);
    connect(newButton, &QPushButton::clicked, this, &CollageWidget::onNewCollage);
    layout->addWidget(newButton);

    // layout
    mLayoutCombo = new QComboBox(mToolbar);
    mLayoutCombo->addItems({ tr("Mosaic"), tr("Grid"), tr("Row"), tr("Column"), tr("Freehand") });
    mLayoutCombo->setToolTip(tr("Mosaic, Grid, Row and Column arrange the images for you (drag pans the picture, Ctrl+drag swaps two frames). "
                                "Freehand lets you move and resize every frame yourself."));
    mGapSpin = new ScrubSpinBox(mToolbar);
    mGapSpin->setRange(0, 400);
    mGapSpin->setValue(12);
    mGapSpin->setSuffix(" px");
    mGapSpin->setToolTip(tr("Spacing between images and around the edges"));
    connect(mLayoutCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { applyEditLayout(); });
    connect(mGapSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this]() { applyEditLayout(); });
    mEditLayoutPanelButton = makeButton(tr("Layout options"), tr("Grid / Row / Column: number of columns and rows, cell size, rotation and cell style"));
    mEditLayoutPanelButton->setCheckable(true);
    mEditLayoutPanelButton->setVisible(false);
    connect(mEditLayoutPanelButton, &QPushButton::toggled, this, &CollageWidget::setLayoutPanelVisible);
    layout->addWidget(labeled(tr("Layout"), mLayoutCombo, mToolbar));
    layout->addWidget(mEditLayoutPanelButton);
    layout->addWidget(labeled(tr("Gap"), mGapSpin, mToolbar));

    mEditCropButton = makeButton(tr("Crop"), tr("Crop: drag moves the picture inside its frame, the wheel zooms it (resize handles are off while on)"));
    mEditCropButton->setCheckable(true);
    mEditCropButton->setVisible(false);
    connect(mEditCropButton, &QPushButton::toggled, this, &CollageWidget::setCropMode);
    layout->addWidget(mEditCropButton);

    // canvas
    mPresetCombo = new QComboBox(mToolbar);
    for(int i = 0; i < COLLAGE_PRESET_COUNT; i++)
        mPresetCombo->addItem(tr(COLLAGE_PRESETS[i].name));
    mPresetCombo->addItem(tr("Custom"));
    mCanvasWidthSpin = new ScrubSpinBox(mToolbar);
    mCanvasHeightSpin = new ScrubSpinBox(mToolbar);
    for(ScrubSpinBox *spin : {mCanvasWidthSpin, mCanvasHeightSpin}) {
        spin->setRange(16, 16384);
        spin->setSuffix(" px");
        spin->setToolTip(tr("Canvas size. Drag sideways to change it (Shift = faster, Alt = finer), or click to type."));
    }
    mCanvasWidthSpin->setValue(COLLAGE_PRESETS[0].width);
    mCanvasHeightSpin->setValue(COLLAGE_PRESETS[0].height);
    connect(mPresetCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &CollageWidget::onPresetChanged);
    connect(mCanvasWidthSpin, qOverload<int>(&QSpinBox::valueChanged), this, &CollageWidget::onCanvasSpinChanged);
    connect(mCanvasHeightSpin, qOverload<int>(&QSpinBox::valueChanged), this, &CollageWidget::onCanvasSpinChanged);
    layout->addWidget(labeled(tr("Canvas"), mPresetCombo, mToolbar));
    layout->addWidget(labeled(tr("W"), mCanvasWidthSpin, mToolbar));
    layout->addWidget(labeled(tr("H"), mCanvasHeightSpin, mToolbar));

    // canvas background
    mBackgroundButton = new ColorSelectorButton(mToolbar);
    mBackgroundButton->setMinimumSize(40, 22);
    mBackgroundButton->setCursor(Qt::PointingHandCursor);
    mBackgroundButton->setDescription(tr("Canvas background"));
    mBackgroundButton->setColor(QColor(Qt::white));
    mTransparentCheck = new QCheckBox(tr("Transparent"), mToolbar);
    mTransparentCheck->setToolTip(tr("Keep the canvas transparent (PNG / WebP only, JPEG is flattened onto white)"));
    connect(mBackgroundButton, &ColorSelectorButton::colorChanged, this, &CollageWidget::onCanvasColorChanged);
    connect(mTransparentCheck, &QCheckBox::toggled, this, &CollageWidget::onCanvasColorChanged);
    layout->addWidget(labeled(tr("Background"), mBackgroundButton, mToolbar));
    layout->addWidget(mTransparentCheck);
    layout->addWidget(makeBorderButton(mToolbar));

    // view / output
    QPushButton *fitButton = makeButton(tr("Fit view"), tr("Fit the whole canvas into the window (Ctrl+0)"));
    connect(fitButton, &QPushButton::clicked, mView, &CollageView::fitCanvas);
    QPushButton *exportButton = makeButton(tr("Export..."), tr("Save the collage as an image"), "buttons/panel/export20");
    connect(exportButton, &QPushButton::clicked, this, &CollageWidget::onExport);
    mPanelToggle = makeButton(tr("Properties"), tr("Show / hide the image properties panel"));
    mPanelToggle->setCheckable(true);
    connect(mPanelToggle, &QPushButton::toggled, this, [this](bool checked) {
        mPanelUserSet = true;
        setPanelVisible(checked);
    });
    mAnimateCheck = new QCheckBox(tr("Animate"), mToolbar);
    mAnimateCheck->setToolTip(tr("Play animated images (GIF, animated WebP / PNG). Export always uses the first frame."));
    mAnimateCheck->setFocusPolicy(Qt::NoFocus);
    connect(mAnimateCheck, &QCheckBox::toggled, this, &CollageWidget::setAnimate);
    layout->addWidget(fitButton);
    layout->addWidget(mAnimateCheck);
    layout->addWidget(mPanelToggle);
    layout->addWidget(exportButton);
    layout->addWidget(addButton);
    layout->setTrailingWidget(addButton); // far right, like in the view bar
}

void CollageWidget::buildPanel() {
    mPanelScroll = new QScrollArea(this);
    mPanelScroll->setAccessibleName("CollagePanelScroll");
    mPanelScroll->setWidgetResizable(true);
    mPanelScroll->setFrameShape(QFrame::NoFrame);
    mPanelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mPanelScroll->viewport()->setAutoFillBackground(false); // the card behind paints the background

    mPanelContent = new QWidget();
    mPanelContent->setAccessibleName("CollagePanel");
    QVBoxLayout *layout = new QVBoxLayout(mPanelContent);
    layout->setContentsMargins(16, 10, 16, 18);
    layout->setSpacing(14);

    mNameLabel = new QLabel(mPanelContent);
    QFont bold = mNameLabel->font();
    bold.setBold(true);
    mNameLabel->setFont(bold);
    mNameLabel->setWordWrap(true);
    mInfoLabel = new QLabel(mPanelContent);
    mInfoLabel->setAccessibleName("CollageHint");
    mInfoLabel->setWordWrap(true);
    layout->addWidget(mNameLabel);
    layout->addWidget(mInfoLabel);

    // columns: 0 = label, 1 = control, 2 = value readout
    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->setColumnMinimumWidth(0, 72);
    grid->setColumnStretch(1, 1);
    int row = 0;

    auto sectionTitle = [this, grid, &row](const QString &text) {
        QLabel *title = new QLabel(text, mPanelContent);
        title->setAccessibleName("CollageSectionTitle");
        grid->addWidget(title, row++, 0, 1, 3);
        return title;
    };
    auto makeValueLabel = [this]() {
        QLabel *label = new QLabel(mPanelContent);
        label->setMinimumWidth(44);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return label;
    };
    auto makeSpin = [this](int min, int max) {
        ScrubSpinBox *spin = new ScrubSpinBox(mPanelContent);
        spin->setRange(min, max);
        return spin;
    };
    // two controls side by side across the control + value columns
    auto pairRow = [this](QWidget *first, QWidget *second) {
        QWidget *pair = new QWidget(mPanelContent);
        QHBoxLayout *pairLayout = new QHBoxLayout(pair);
        pairLayout->setContentsMargins(0, 0, 0, 0);
        pairLayout->setSpacing(8);
        pairLayout->addWidget(first, 1);
        pairLayout->addWidget(second, 1);
        return pair;
    };

    sectionTitle(tr("Frame"));
    // automatic layouts: what dragging the border between two tiles does
    QLabel *bordersLabel = new QLabel(tr("Borders"), mPanelContent);
    mBorderModeCombo = new QComboBox(mPanelContent);
    mBorderModeCombo->addItems({ tr("Snap to sizes"), tr("Free"), tr("Locked") });
    mBorderModeCombo->setToolTip(tr("Dragging the border between two tiles resizes them. Snap: locks to 1/4, 1/3, 1/2, 2/3, 3/4 "
                                    "and equal sizes (hold Alt to drag freely). Free: any size. Locked: borders can not be dragged."));
    grid->addWidget(bordersLabel, row, 0);
    grid->addWidget(mBorderModeCombo, row++, 1, 1, 2);
    mBorderModeWidgets << bordersLabel << mBorderModeCombo;
    connect(mBorderModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if(mSyncing || index < 0)
            return;
        syncBorderModeCombos(index);
    });

    mFitCombo = new QComboBox(mPanelContent);
    mFitCombo->addItems({ tr("Fill (crop to frame)"), tr("Contain (whole image)"), tr("Stretch") });
    grid->addWidget(new QLabel(tr("Fit"), mPanelContent), row, 0);
    grid->addWidget(mFitCombo, row++, 1, 1, 2);

    // editor only: exact geometry; the spin boxes carry their own X / Y / W / H prefix
    mXSpin = makeSpin(-50000, 50000);
    mYSpin = makeSpin(-50000, 50000);
    mWidthSpin = makeSpin(24, 50000);
    mHeightSpin = makeSpin(24, 50000);
    mXSpin->setPrefix("X  ");
    mYSpin->setPrefix("Y  ");
    mWidthSpin->setPrefix("W  ");
    mHeightSpin->setPrefix("H  ");
    QLabel *positionLabel = new QLabel(tr("Position"), mPanelContent);
    QWidget *positionPair = pairRow(mXSpin, mYSpin);
    grid->addWidget(positionLabel, row, 0);
    grid->addWidget(positionPair, row++, 1, 1, 2);
    QLabel *sizeFrameLabel = new QLabel(tr("Size"), mPanelContent);
    QWidget *sizePair = pairRow(mWidthSpin, mHeightSpin);
    grid->addWidget(sizeFrameLabel, row, 0);
    grid->addWidget(sizePair, row++, 1, 1, 2);
    mKeepAspectCheck = new QCheckBox(tr("Keep frame proportions"), mPanelContent);
    grid->addWidget(mKeepAspectCheck, row++, 1, 1, 2);
    mEditOnlyWidgets << positionLabel << positionPair << sizeFrameLabel << sizePair << mKeepAspectCheck;

    // view mode only: shape and share of the mosaic
    QLabel *aspectLabel = new QLabel(tr("Aspect"), mPanelContent);
    mViewAspectCombo = new QComboBox(mPanelContent);
    mViewAspectCombo->addItems({ tr("Original"), "1:1", "4:3", "3:2", "16:9", "9:16", "2:3", "3:4" });
    grid->addWidget(aspectLabel, row, 0);
    grid->addWidget(mViewAspectCombo, row++, 1, 1, 2);
    QLabel *sizeLabel = new QLabel(tr("Size"), mPanelContent);
    mViewWSpin = makeSpin(24, 20000);
    mViewHSpin = makeSpin(24, 20000);
    mViewWSpin->setPrefix("W  ");
    mViewHSpin->setPrefix("H  ");
    for(ScrubSpinBox *spin : { mViewWSpin, mViewHSpin }) {
        spin->setToolTip(tr("Size of the tile in pixels: drag sideways (Shift = faster, Alt = finer) or click to type. "
                            "The automatic layouts get as close as the neighbours allow; Freehand is exact."));
    }
    QWidget *viewSizePair = pairRow(mViewWSpin, mViewHSpin);
    grid->addWidget(sizeLabel, row, 0);
    grid->addWidget(viewSizePair, row++, 1, 1, 2);
    QPushButton *resetSizeButton = new QPushButton(tr("Reset size"), mPanelContent);
    resetSizeButton->setFocusPolicy(Qt::NoFocus);
    resetSizeButton->setToolTip(tr("Back to the automatic size and shape"));
    grid->addWidget(resetSizeButton, row++, 1, 1, 2);
    mViewSizeWidgets << sizeLabel << viewSizePair;
    mViewOnlyWidgets << aspectLabel << mViewAspectCombo << resetSizeButton;

    sectionTitle(tr("Crop"));
    mZoomSlider = new QSlider(Qt::Horizontal, mPanelContent);
    mZoomSlider->setRange(100, 800);
    mZoomValueLabel = makeValueLabel();
    grid->addWidget(new QLabel(tr("Zoom"), mPanelContent), row, 0);
    grid->addWidget(mZoomSlider, row, 1);
    grid->addWidget(mZoomValueLabel, row++, 2);

    mCropModeButton = new QPushButton(tr("Crop mode"), mPanelContent);
    mCropModeButton->setCheckable(true);
    mCropModeButton->setFocusPolicy(Qt::NoFocus);
    mCropModeButton->setToolTip(tr("Crop (Freehand layouts): while on, dragging inside a frame moves the picture under it and the wheel zooms it. "
                                   "The automatic layouts always work this way. Shift+wheel zooms in any case."));
    QPushButton *resetCropButton = new QPushButton(tr("Reset crop"), mPanelContent);
    resetCropButton->setFocusPolicy(Qt::NoFocus);
    grid->addWidget(pairRow(mCropModeButton, resetCropButton), row++, 0, 1, 3);

    sectionTitle(tr("Look"));
    mOpacitySlider = new QSlider(Qt::Horizontal, mPanelContent);
    mOpacitySlider->setRange(5, 100);
    mOpacityValueLabel = makeValueLabel();
    grid->addWidget(new QLabel(tr("Opacity"), mPanelContent), row, 0);
    grid->addWidget(mOpacitySlider, row, 1);
    grid->addWidget(mOpacityValueLabel, row++, 2);

    mRadiusSpin = makeSpin(0, 1000);
    mRadiusSpin->setSuffix(" px");
    grid->addWidget(new QLabel(tr("Corners"), mPanelContent), row, 0);
    grid->addWidget(mRadiusSpin, row++, 1, 1, 2);

    // this tile's own outline (otherwise the Border default of the bar)
    mTileBorderSpin = makeSpin(0, 64);
    mTileBorderSpin->setSuffix(" px");
    mTileBorderSpin->setSpecialValueText(tr("None"));
    mTileBorderColor = new ColorSelectorButton(mPanelContent);
    mTileBorderColor->setMinimumSize(40, 22);
    mTileBorderColor->setCursor(Qt::PointingHandCursor);
    mTileBorderColor->setDescription(tr("Tile outline"));
    mTileBorderStyle = new QComboBox(mPanelContent);
    for(const char *name : BORDER_STYLE_NAMES)
        mTileBorderStyle->addItem(tr(name));
    QPushButton *borderDefaultButton = new QPushButton(tr("Use default"), mPanelContent);
    borderDefaultButton->setFocusPolicy(Qt::NoFocus);
    borderDefaultButton->setToolTip(tr("Follow the Border setting of the bar again"));
    grid->addWidget(new QLabel(tr("Border"), mPanelContent), row, 0);
    grid->addWidget(pairRow(mTileBorderSpin, mTileBorderColor), row++, 1, 1, 2);
    grid->addWidget(pairRow(mTileBorderStyle, borderDefaultButton), row++, 1, 1, 2);

    sectionTitle(tr("Memory"));
    mResolutionCombo = new QComboBox(mPanelContent);
    mResolutionCombo->addItems({ tr("Auto (canvas size)"), tr("Original"), tr("4K  (3840 px)"),
                                 tr("2K  (2560 px)"), tr("1080p  (1920 px)"), tr("720p  (1280 px)") });
    mResolutionCombo->setToolTip(tr("Longest side of the working copy kept in memory. Lower it to save RAM when using many large images."));
    grid->addWidget(new QLabel(tr("Resolution"), mPanelContent), row, 0);
    grid->addWidget(mResolutionCombo, row++, 1, 1, 2);

    // animated images only
    QLabel *animTitle = sectionTitle(tr("Animation"));
    QLabel *animLabel = new QLabel(tr("Playback"), mPanelContent);
    mAnimPlayCheck = new QCheckBox(tr("Play"), mPanelContent);
    mAnimPlayCheck->setToolTip(tr("Play / pause this image (Space)"));
    mAnimLoopCheck = new QCheckBox(tr("Loop"), mPanelContent);
    mAnimLoopCheck->setToolTip(tr("Off: play once and stop on the last frame"));
    mAnimPlayCheck->setFocusPolicy(Qt::NoFocus);
    mAnimLoopCheck->setFocusPolicy(Qt::NoFocus);
    QWidget *playbackPair = pairRow(mAnimPlayCheck, mAnimLoopCheck);
    grid->addWidget(animLabel, row, 0);
    grid->addWidget(playbackPair, row++, 1, 1, 2);
    QLabel *speedLabel = new QLabel(tr("Speed"), mPanelContent);
    mAnimSpeedSlider = new QSlider(Qt::Horizontal, mPanelContent);
    mAnimSpeedSlider->setRange(25, 400);
    mAnimSpeedSlider->setSingleStep(25);
    mAnimSpeedSlider->setPageStep(25);
    mAnimSpeedSlider->setToolTip(tr("Playback speed, 100% = as authored"));
    mAnimSpeedLabel = makeValueLabel();
    grid->addWidget(speedLabel, row, 0);
    grid->addWidget(mAnimSpeedSlider, row, 1);
    grid->addWidget(mAnimSpeedLabel, row++, 2);
    mAnimWidgets << animTitle << animLabel << playbackPair << speedLabel << mAnimSpeedSlider << mAnimSpeedLabel;
    layout->addLayout(grid);

    // actions
    QPushButton *fillButton = new QPushButton(tr("Fit to border"), mPanelContent);
    fillButton->setToolTip(tr("Stretch the frame to the canvas edges"));
    QPushButton *centerButton = new QPushButton(tr("Center"), mPanelContent);
    QPushButton *frontButton = new QPushButton(tr("Bring to front"), mPanelContent);
    QPushButton *backButton = new QPushButton(tr("Send to back"), mPanelContent);
    QPushButton *removeButton = new QPushButton(tr("Remove"), mPanelContent);
    removeButton->setProperty("danger", true);
    for(QPushButton *button : {fillButton, centerButton, frontButton, backButton, removeButton})
        button->setFocusPolicy(Qt::NoFocus);
    // these keys are handled by CollageView::keyPressEvent
    frontButton->setToolTip(tr("Bring the selected image to the front (Page Up)"));
    backButton->setToolTip(tr("Send the selected image to the back (Page Down)"));
    removeButton->setToolTip(tr("Remove the selected image from the collage (Delete)"));
    mEditOnlyWidgets << fillButton << centerButton;
    mStackWidgets << frontButton << backButton;
    QGridLayout *actions = new QGridLayout();
    actions->setHorizontalSpacing(10);
    actions->setVerticalSpacing(10);
    actions->addWidget(fillButton, 0, 0);
    actions->addWidget(centerButton, 0, 1);
    actions->addWidget(frontButton, 1, 0);
    actions->addWidget(backButton, 1, 1);
    actions->addWidget(removeButton, 2, 0, 1, 2);
    layout->addSpacing(4);
    layout->addLayout(actions);
    layout->addStretch(1);

    mPanelScroll->setWidget(mPanelContent);

    // header with the hide button; lives outside the scroll area so it stays usable
    // while the properties are disabled (nothing selected)
    mPanelContainer = new CollagePanelFrame(this);
    mPanelContainer->setResizeEdge(Qt::LeftEdge);
    connect(mPanelContainer, &CollagePanelFrame::widthDragged, this, [this](int width) {
        mPanelWidth = qBound(MIN_PANEL_WIDTH, width, MAX_PANEL_WIDTH);
        layoutOverlays();
    });
    connect(mPanelContainer, &CollagePanelFrame::resizeFinished, this, [this]() {
        settings->setCollagePanelWidth(mPanelWidth);
    });
    QPushButton *hidePanelButton = new QPushButton(tr("Hide panel"), mPanelContainer);
    setButtonIcon(hidePanelButton, "menuitem/chevron-right16");
    hidePanelButton->setLayoutDirection(Qt::RightToLeft); // chevron after the label
    hidePanelButton->setToolTip(tr("Hide this panel"));
    hidePanelButton->setFocusPolicy(Qt::NoFocus);
    hidePanelButton->setCursor(Qt::PointingHandCursor);
    hidePanelButton->setMinimumHeight(30);
    connect(hidePanelButton, &QPushButton::clicked, this, [this]() {
        mPanelUserSet = true; // do not let the narrow / wide window logic reopen it
        setPanelVisible(false);
        mView->setFocus();
    });
    // shortcuts + features reference; swaps places with the properties (those are disabled without a selection)
    mHelpButton = new QPushButton(tr("Help"), mPanelContainer);
    mHelpButton->setCheckable(true);
    mHelpButton->setToolTip(tr("List all shortcuts and features of the collage"));
    mHelpButton->setFocusPolicy(Qt::NoFocus);
    mHelpButton->setCursor(Qt::PointingHandCursor);
    mHelpButton->setMinimumHeight(30);
    connect(mHelpButton, &QPushButton::toggled, this, [this](bool) { updateHelpVisibility(); });
    mHelpScroll = new QScrollArea(mPanelContainer);
    mHelpScroll->setAccessibleName("CollagePanelScroll");
    mHelpScroll->setWidgetResizable(true);
    mHelpScroll->setFrameShape(QFrame::NoFrame);
    mHelpScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mHelpScroll->viewport()->setAutoFillBackground(false);
    QWidget *helpContent = new QWidget();
    helpContent->setAccessibleName("CollagePanel");
    QVBoxLayout *helpLayout = new QVBoxLayout(helpContent);
    helpLayout->setContentsMargins(16, 10, 16, 18);
    auto makeHelpLabel = [helpContent](const QString &html) {
        QLabel *label = new QLabel(html, helpContent);
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);
        label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        return label;
    };
    mHelpViewLabel = makeHelpLabel(helpHtml(false));
    mHelpEditLabel = makeHelpLabel(helpHtml(true));
    helpLayout->addWidget(mHelpViewLabel);
    helpLayout->addWidget(mHelpEditLabel);
    helpLayout->addStretch(1);
    mHelpScroll->setWidget(helpContent);
    mHelpScroll->hide();

    QFrame *separator = new QFrame(mPanelContainer);
    separator->setAccessibleName("CollagePanelSeparator");
    separator->setFrameShape(QFrame::NoFrame);
    separator->setFixedHeight(1);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(14, 14, 14, 10);
    headerLayout->setSpacing(8);
    headerLayout->addWidget(mHelpButton);
    headerLayout->addWidget(hidePanelButton, 1);
    QVBoxLayout *containerLayout = new QVBoxLayout(mPanelContainer);
    containerLayout->setContentsMargins(1, 0, 1, 1); // keep the scroll content inside the rounded border
    containerLayout->setSpacing(0);
    containerLayout->addLayout(headerLayout);
    containerLayout->addWidget(separator);
    containerLayout->addWidget(mPanelScroll, 1);
    containerLayout->addWidget(mHelpScroll, 1);
    mPanelContainer->hide();

    // --- editing -----------------------------------------------------------
    connect(mFitCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if(mSyncing || index < 0)
            return;
        for(CollageItem *item : targets())
            item->setFit(static_cast<CollageFit>(index));
        updatePanel();
    });
    connect(mXSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { applyGeometry(0); });
    connect(mYSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { applyGeometry(1); });
    connect(mWidthSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { applyGeometry(2); });
    connect(mHeightSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { applyGeometry(3); });

    connect(mViewAspectCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if(mSyncing || index < 0 || index >= VIEW_ASPECT_COUNT)
            return;
        for(CollageItem *item : targets())
            item->setViewAspect(VIEW_ASPECTS[index]);
        mScene->relayoutView();
    });
    connect(mViewWSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { applyViewSize(0); });
    connect(mViewHSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { applyViewSize(1); });
    connect(resetSizeButton, &QPushButton::clicked, this, [this]() {
        for(CollageItem *item : targets()) {
            item->setViewAspect(0.0);
            item->setViewWeight(1.0);
            mScene->clearSplitsFor(item);
        }
        mScene->relayoutView();
    });
    auto applyTileBorder = [this]() {
        if(mSyncing)
            return;
        for(CollageItem *item : targets()) {
            item->setBorderOverride(true);
            item->setBorder(mTileBorderSpin->value(), mTileBorderColor->color(), mTileBorderStyle->currentIndex());
        }
    };
    connect(mTileBorderSpin, qOverload<int>(&QSpinBox::valueChanged), this, applyTileBorder);
    connect(mTileBorderColor, &ColorSelectorButton::colorChanged, this, applyTileBorder);
    connect(mTileBorderStyle, qOverload<int>(&QComboBox::currentIndexChanged), this, applyTileBorder);
    connect(borderDefaultButton, &QPushButton::clicked, this, [this]() {
        for(CollageItem *item : targets()) {
            item->setBorderOverride(false);
            item->setBorder(settings->collageBorderWidth(), settings->collageBorderColor(), settings->collageBorderStyle());
        }
        updatePanel();
    });
    connect(mZoomSlider, &QSlider::valueChanged, this, [this](int value) {
        if(mSyncing)
            return;
        for(CollageItem *item : targets())
            item->setContentZoom(value / 100.0);
        mZoomValueLabel->setText(QString::number(value) + "%");
    });
    connect(mCropModeButton, &QPushButton::toggled, this, &CollageWidget::setCropMode);
    connect(resetCropButton, &QPushButton::clicked, this, [this]() {
        for(CollageItem *item : targets())
            item->resetCrop();
        updatePanel();
    });
    connect(mOpacitySlider, &QSlider::valueChanged, this, [this](int value) {
        if(mSyncing)
            return;
        for(CollageItem *item : targets())
            item->setOpacity(value / 100.0);
        mOpacityValueLabel->setText(QString::number(value) + "%");
    });
    connect(mRadiusSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        if(mSyncing)
            return;
        for(CollageItem *item : targets())
            item->setCornerRadius(value);
    });
    connect(mResolutionCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if(mSyncing || index < 0)
            return;
        for(CollageItem *item : targets()) {
            item->setResolution(static_cast<CollageRes>(index));
            mScene->reloadItem(item);
        }
    });

    connect(mAnimPlayCheck, &QCheckBox::toggled, this, [this](bool on) {
        if(mSyncing)
            return;
        for(CollageItem *item : targets())
            item->setAnimationEnabled(on);
        // a tile can not play while the collage is paused: switch it back on
        if(on && !mScene->animationsEnabled())
            setAnimate(true);
        updatePanel();
    });
    connect(mAnimLoopCheck, &QCheckBox::toggled, this, [this](bool on) {
        if(mSyncing)
            return;
        for(CollageItem *item : targets())
            item->setAnimationLoop(on);
    });
    connect(mAnimSpeedSlider, &QSlider::valueChanged, this, [this](int value) {
        if(mSyncing)
            return;
        for(CollageItem *item : targets())
            item->setAnimationSpeed(value);
        mAnimSpeedLabel->setText(QString::number(value) + "%");
    });

    connect(fillButton, &QPushButton::clicked, this, [this]() {
        for(CollageItem *item : targets())
            mScene->fillCanvas(item);
        updatePanel();
    });
    connect(centerButton, &QPushButton::clicked, this, [this]() {
        QRectF canvas = mScene->canvasRect();
        for(CollageItem *item : targets())
            item->setPos(canvas.center() - QPointF(item->frameSize().width(), item->frameSize().height()) / 2.0);
        updatePanel();
    });
    connect(frontButton, &QPushButton::clicked, this, [this]() {
        for(CollageItem *item : targets())
            mScene->bringToFront(item);
    });
    connect(backButton, &QPushButton::clicked, this, [this]() {
        for(CollageItem *item : targets())
            mScene->sendToBack(item);
    });
    connect(removeButton, &QPushButton::clicked, mScene, &CollageScene::removeSelected);
}

//------------------------------------------------------------------------------
CollageItem *CollageWidget::primaryItem() const {
    const QList<CollageItem*> selected = mScene->selectedCollageItems();
    return selected.isEmpty() ? nullptr : selected.first();
}

QList<CollageItem*> CollageWidget::targets() const {
    return mScene->selectedCollageItems();
}

// source: 0 = x, 1 = y, 2 = width, 3 = height
void CollageWidget::applyGeometry(int source) {
    CollageItem *item = primaryItem();
    if(mSyncing || !item)
        return;
    QSizeF size = item->frameSize();
    if(source >= 2) {
        qreal ratio = size.height() > 0 ? size.width() / size.height() : 1.0;
        qreal w = mWidthSpin->value();
        qreal h = mHeightSpin->value();
        if(mKeepAspectCheck->isChecked()) {
            if(source == 2)
                h = qMax<qreal>(24.0, w / ratio);
            else
                w = qMax<qreal>(24.0, h * ratio);
        }
        item->setFrameSize(QSizeF(w, h));
    } else {
        item->setPos(mXSpin->value(), mYSpin->value());
    }
    updatePanel();
}

// W / H of the selected tile. Freehand: exact, around the tile centre. Automatic layouts: the tile's
// aspect and share are adjusted, the result is as close as the neighbours allow (written back by updatePanel)
void CollageWidget::applyViewSize(int source) {
    CollageItem *item = primaryItem();
    if(mSyncing || !item || isEditMode())
        return;
    qreal w = mViewWSpin->value(), h = mViewHSpin->value();
    if(mScene->isFreeView()) {
        QSizeF old = item->frameSize();
        QPointF center = item->pos() + QPointF(old.width() / 2.0, old.height() / 2.0);
        item->setFrameSize(QSizeF(w, h));
        item->setPos(center - QPointF(w / 2.0, h / 2.0));
        emit mScene->itemEdited();
        return;
    }
    QSizeF current = item->frameSize();
    item->setViewAspect(w / h);
    if(source == 0 && current.width() > 0)
        item->setViewWeight(item->viewWeight() * w / current.width());
    else if(source == 1 && current.height() > 0)
        item->setViewWeight(item->viewWeight() * h / current.height());
    mScene->clearSplitsFor(item);
    mScene->relayoutView();
}

// one border mode for the panel combo, the layout panel combo and the scene
void CollageWidget::syncBorderModeCombos(int mode) {
    settings->setCollageBorderMode(mode);
    for(QComboBox *combo : { mBorderModeCombo, mLayBorderMode }) {
        if(!combo)
            continue;
        QSignalBlocker block(combo);
        combo->setCurrentIndex(mode);
    }
    mScene->refreshSeparators();
}

// mirrors the first selected item into the panel
void CollageWidget::updatePanel() {
    mSyncing = true;
    const QList<CollageItem*> selected = mScene->selectedCollageItems();
    CollageItem *item = selected.isEmpty() ? nullptr : selected.first();
    mPanelContent->setEnabled(item != nullptr);

    if(!item) {
        mNameLabel->setText(tr("No image selected"));
        if(isEditMode() && mScene->staticCanvas())
            mInfoLabel->setText(tr("Auto layout: images follow the Layout / Gap above. Drag moves the picture inside its frame, "
                                   "Ctrl+drag swaps two frames. Wheel zooms the view, middle-drag pans."));
        else if(isEditMode())
            mInfoLabel->setText(tr("Click an image on the canvas to edit it. Drag to move, use the handles to resize "
                                   "(Shift keeps proportions). Wheel zooms the view, middle-drag pans."));
        else if(mScene->isFreeView())
            mInfoLabel->setText(tr("Drag a tile to move it, use the handles to resize it (Shift keeps proportions). "
                                   "Alt+drag moves the picture inside the tile, Shift+wheel zooms it, "
                                   "double-click opens it. Wheel zooms the whole collage, middle-drag pans it."));
        else
            mInfoLabel->setText(tr("Click a tile to adjust it. Drag moves the picture inside its tile, Shift+wheel zooms "
                                   "it, Ctrl+drag swaps the tile with another, double-click opens it. "
                                   "Wheel zooms the whole collage, middle-drag pans it."));
        mSyncing = false;
        return;
    }

    mNameLabel->setText(selected.size() > 1 ? tr("%1 images selected").arg(selected.size()) : item->displayName());
    QString info = tr("Original: %1 x %2 px").arg(item->originalSize().width()).arg(item->originalSize().height());
    if(item->isAnimated())
        info += "\n" + tr("Animated image");
    if(item->isLoaded()) {
        info += "\n" + tr("In memory: %1 x %2 px (%3)")
                .arg(item->workingSize().width()).arg(item->workingSize().height()).arg(megabytes(item->memoryBytes()));
    } else {
        info += "\n" + (item->hasFailed() ? tr("Could not be loaded") : tr("Loading..."));
    }
    mInfoLabel->setText(info);

    mFitCombo->setCurrentIndex(static_cast<int>(item->fit()));
    mXSpin->setValue(qRound(item->pos().x()));
    mYSpin->setValue(qRound(item->pos().y()));
    mWidthSpin->setValue(qRound(item->frameSize().width()));
    mHeightSpin->setValue(qRound(item->frameSize().height()));
    int zoom = qRound(item->contentZoom() * 100.0);
    mZoomSlider->setValue(zoom);
    mZoomValueLabel->setText(QString::number(zoom) + "%");
    // zoom / pan only exist in Fill mode
    mZoomSlider->setEnabled(item->fit() == CollageFit::Fill);
    mCropModeButton->setEnabled(item->fit() == CollageFit::Fill && freeLayoutActive());
    int opacity = qRound(item->opacity() * 100.0);
    mOpacitySlider->setValue(opacity);
    mOpacityValueLabel->setText(QString::number(opacity) + "%");
    mRadiusSpin->setValue(qRound(item->cornerRadius()));
    mResolutionCombo->setCurrentIndex(static_cast<int>(item->resolution()));
    int aspectIndex = 0;
    for(int i = 0; i < VIEW_ASPECT_COUNT; i++) {
        if(qAbs(VIEW_ASPECTS[i] - item->viewAspect()) < 0.001)
            aspectIndex = i;
    }
    mViewAspectCombo->setCurrentIndex(aspectIndex);
    mViewWSpin->setValue(qRound(item->frameSize().width()));
    mViewHSpin->setValue(qRound(item->frameSize().height()));
    mTileBorderSpin->setValue(item->borderWidth());
    mTileBorderColor->setColor(item->effectiveBorderColor());
    mTileBorderStyle->setCurrentIndex(item->borderStyle());
    mBorderModeCombo->setCurrentIndex(settings->collageBorderMode());

    CollageItem *animated = nullptr;
    for(CollageItem *candidate : selected) {
        if(candidate->isAnimated()) {
            animated = candidate;
            break;
        }
    }
    for(QWidget *widget : mAnimWidgets)
        widget->setEnabled(animated != nullptr);
    if(animated) {
        mAnimPlayCheck->setChecked(animated->animationEnabled());
        mAnimLoopCheck->setChecked(animated->animationLoop());
        mAnimSpeedSlider->setValue(animated->animationSpeed());
        mAnimSpeedLabel->setText(QString::number(animated->animationSpeed()) + "%");
    } else {
        mAnimPlayCheck->setChecked(false);
        mAnimSpeedLabel->setText(QString());
    }
    mSyncing = false;
}

//------------------------------------------------------------------------------
void CollageWidget::addImages(const QStringList &paths) {
    if(paths.isEmpty())
        return;
    bool wasEmpty = (mScene->imageCount() == 0);
    int failed = 0;
    int added = 0;
    for(const QString &path : paths) {
        CollageItem *item = mScene->addImage(path, !wasEmpty);
        if(item) {
            item->setCropMode(mCropOn && freeLayoutActive());
            added++;
        } else {
            failed++;
        }
    }
    // the automatic layouts arrange new images by themselves; Freehand starts from a mosaic
    if(wasEmpty && added > 0 && mLayoutCombo->currentIndex() == CollageLayout::MODE_FREEFORM)
        mScene->applyLayout(CollageLayout::MODE_MOSAIC, mGapSpin->value());
    if(failed > 0)
        setNotice(tr("%1 file(s) could not be read as images.").arg(failed), true);
    else
        updateStatus();
}

int CollageWidget::imageCount() const {
    return mScene->imageCount();
}

QStringList CollageWidget::pickImages(QWidget *parent, const QString &directory) {
    QStringList patterns;
    for(const QByteArray &format : QImageReader::supportedImageFormats()) {
        if(format != "pdf")
            patterns << "*." + QString::fromLatin1(format);
    }
    QString filter = tr("Images (%1)").arg(patterns.join(" ")) + ";;" + tr("All files (*)");
    return QFileDialog::getOpenFileNames(parent, tr("Select images for the collage"), directory, filter);
}

void CollageWidget::onAddImages() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    CollageItem *item = primaryItem();
    if(item)
        dir = QFileInfo(item->path()).absolutePath();
    addImages(pickImages(this, dir));
}

void CollageWidget::onNewCollage() {
    if(mScene->imageCount() > 0) {
        auto answer = QMessageBox::question(this, tr("New collage"), tr("Remove all images from the collage?"),
                                            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if(answer != QMessageBox::Yes)
            return;
    }
    mScene->clearCollage();
    setNotice(QString());
}

//------------------------------------------------------------------------------
void CollageWidget::onPresetChanged(int index) {
    if(index < 0 || index >= COLLAGE_PRESET_COUNT)
        return; // "Custom": keep the current values
    {
        QSignalBlocker blockW(mCanvasWidthSpin);
        QSignalBlocker blockH(mCanvasHeightSpin);
        mCanvasWidthSpin->setValue(COLLAGE_PRESETS[index].width);
        mCanvasHeightSpin->setValue(COLLAGE_PRESETS[index].height);
    }
    applyCanvasSize();
}

void CollageWidget::onCanvasSpinChanged() {
    // switch the preset box to a matching preset, or to "Custom"
    int match = COLLAGE_PRESET_COUNT;
    for(int i = 0; i < COLLAGE_PRESET_COUNT; i++) {
        if(COLLAGE_PRESETS[i].width == mCanvasWidthSpin->value() && COLLAGE_PRESETS[i].height == mCanvasHeightSpin->value())
            match = i;
    }
    {
        QSignalBlocker block(mPresetCombo);
        mPresetCombo->setCurrentIndex(match);
    }
    applyCanvasSize();
}

void CollageWidget::applyCanvasSize() {
    mScene->setCanvasSize(QSize(mCanvasWidthSpin->value(), mCanvasHeightSpin->value()));
    syncViewCanvasCombo();
    if(mView->isAutoFit())
        mView->fitCanvas();
    updateStatus();
}

// global play switch: scene, both checkboxes (view bar / editor toolbar) and the saved setting
void CollageWidget::setAnimate(bool enabled) {
    mScene->setAnimationsEnabled(enabled);
    for(QCheckBox *check : {mAnimateCheck, mViewAnimateCheck}) {
        QSignalBlocker block(check);
        check->setChecked(enabled);
    }
    settings->setCollageAnimate(enabled);
    updatePanel();
}

// editor layout combo: Mosaic / Grid / Row / Column arrange the frames automatically, Freehand frees them
void CollageWidget::applyEditLayout() {
    int index = mLayoutCombo->currentIndex();
    if(index < 0)
        return;
    bool freehand = (index == CollageLayout::MODE_FREEFORM);
    // layout first, then the flag: switching from Freehand relayouts exactly once
    if(!freehand)
        mScene->setStaticLayout(static_cast<CollageLayout::Mode>(index), mGapSpin->value());
    mScene->setStaticCanvas(!freehand);
    settings->setCollageEditLayout(index);
    updateLayoutRows();
    updatePanel();
}

// one crop state for the three Crop buttons (view bar, editor toolbar, panel)
void CollageWidget::setCropMode(bool on) {
    mCropOn = on;
    for(QPushButton *button : {mViewCropButton, mEditCropButton, mCropModeButton}) {
        QSignalBlocker block(button);
        button->setChecked(on);
    }
    applyCrop();
    updatePanel();
}

// Crop only has an effect while the frames are placed by hand
bool CollageWidget::freeLayoutActive() const {
    return isEditMode() ? !mScene->staticCanvas() : mScene->isFreeView();
}

void CollageWidget::applyCrop() {
    bool effective = mCropOn && freeLayoutActive();
    mView->setCropMode(effective);
    for(CollageItem *item : mScene->collageItems())
        item->setCropMode(effective);
}

void CollageWidget::onCanvasColorChanged() {
    QColor color = mBackgroundButton->color();
    if(mTransparentCheck->isChecked())
        color.setAlpha(0);
    mBackgroundButton->setEnabled(!mTransparentCheck->isChecked());
    mScene->setCanvasColor(color);
    syncBackgroundControls();
}

//------------------------------------------------------------------------------
void CollageWidget::onExport() {
    if(mScene->imageCount() == 0) {
        QMessageBox::information(this, tr("Export"), tr("Add some images first."));
        return;
    }

    QStringList filters;
    QList<QByteArray> writable = QImageWriter::supportedImageFormats();
    if(writable.contains("png"))
        filters << tr("PNG image (*.png)");
    if(writable.contains("jpg") || writable.contains("jpeg"))
        filters << tr("JPEG image (*.jpg *.jpeg)");
    if(writable.contains("webp"))
        filters << tr("WebP image (*.webp)");
    if(writable.contains("bmp"))
        filters << tr("BMP image (*.bmp)");
    if(filters.isEmpty()) {
        QMessageBox::warning(this, tr("Export"), tr("No supported output image format found."));
        return;
    }

    QString selectedFilter;
    QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString path = QFileDialog::getSaveFileName(this, tr("Export collage"), dir + "/collage",
                                                filters.join(";;"), &selectedFilter);
    if(path.isEmpty())
        return;
    if(QFileInfo(path).suffix().isEmpty()) {
        if(selectedFilter.contains("jpg"))
            path += ".jpg";
        else if(selectedFilter.contains("webp"))
            path += ".webp";
        else if(selectedFilter.contains("bmp"))
            path += ".bmp";
        else
            path += ".png";
    }

    QProgressDialog progress(tr("Exporting collage..."), tr("Cancel"), 0, mScene->imageCount(), this);
    progress.setWindowTitle(tr("Export"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    QString error;
    bool ok = mScene->exportImage(path, &error, [&progress](int done, int total) {
        progress.setMaximum(qMax(1, total));
        progress.setValue(done);
        QCoreApplication::processEvents();
        return !progress.wasCanceled();
    });
    // read before hiding: closing the dialog raises canceled()
    bool cancelled = progress.wasCanceled();
    progress.reset();

    if(ok)
        setNotice(tr("Saved to %1").arg(QDir::toNativeSeparators(path)));
    else if(cancelled)
        setNotice(tr("Export cancelled."), true);
    else
        QMessageBox::warning(this, tr("Export failed"), error);
}

//------------------------------------------------------------------------------
void CollageWidget::onSceneChanged() {
    updateStatus();
    updatePanel();
    updateOverlayVisibility();
    emitInfoIfChanged();
}

void CollageWidget::setNotice(const QString &text, bool warning) {
    mNotice = text;
    mNoticeWarning = warning;
    updateStatus();
}

void CollageWidget::updateStatus() {
    qint64 bytes = mScene->memoryBytes();
    QSize canvas = mScene->canvasSize();
    qint64 exportBytes = static_cast<qint64>(canvas.width()) * canvas.height() * 4;
    QString text = tr("%n image(s)", "", mScene->imageCount())
                   + "  |  " + tr("Memory: %1").arg(megabytes(bytes))
                   + "  |  " + tr("Canvas: %1 x %2 (export buffer %3)").arg(canvas.width()).arg(canvas.height()).arg(megabytes(exportBytes));
    if(!mNotice.isEmpty())
        text += "  |  " + mNotice;
    bool warning = (bytes > MEMORY_WARNING) || (!mNotice.isEmpty() && mNoticeWarning);
    mStatusLabel->setText(text);
    mStatusLabel->setProperty("warning", warning);
    mStatusLabel->style()->unpolish(mStatusLabel);
    mStatusLabel->style()->polish(mStatusLabel);
}

// shortcut / feature reference shown by the panel's Help button; keep in sync with CollageView / CollageItem
QString CollageWidget::helpHtml(bool edit) {
    using Rows = QList<QPair<QString, QString>>;
    auto section = [](const QString &title, const Rows &rows) {
        QString html = "<p style='margin-top:20px; margin-bottom:6px'><b>" + title.toHtmlEscaped() + "</b></p>";
        for(const auto &row : rows)
            html += "<p style='margin-top:5px; margin-bottom:5px; line-height:130%'><b>" + row.first.toHtmlEscaped()
                    + "</b> &ndash; " + row.second.toHtmlEscaped() + "</p>";
        return html;
    };
    QString html;
    if(!edit) {
        html += "<p><b>" + tr("Collage view").toHtmlEscaped() + "</b></p>";
        html += section(tr("Mouse"), {
            { tr("Wheel"), tr("zoom the whole collage") },
            { tr("Hold the middle button + move"), tr("move the whole collage freely, also when starting on a tile") },
            { tr("Middle click"), tr("fit the collage back into the window") },
            { tr("Drag empty space"), tr("pan the whole collage") },
            { tr("Drag the border between two tiles"), tr("resize them (Snap locks to 1/4, 1/3, 1/2, 2/3, 3/4; Alt = free); double-click = reset") },
            { tr("Click a tile"), tr("select it and show its settings") },
            { tr("Drag a tile"), tr("move the picture inside its tile") },
            { tr("Shift + wheel"), tr("zoom the picture inside the tile") },
            { tr("Ctrl + drag a tile"), tr("swap it with another tile") },
            { tr("Double-click a tile"), tr("open it in the normal viewer") },
            { tr("Right-click a tile"), tr("open its settings") },
            { tr("Drop files"), tr("add them to the collage") } });
        html += section(tr("Keyboard"), {
            { tr("Left / Right"), tr("select previous / next tile") },
            { tr("Enter"), tr("open the selected tile") },
            { tr("Delete / Backspace"), tr("remove the selected tile") },
            { tr("Esc"), tr("clear the selection; nothing selected: exit the collage (asks first)") },
            { tr("Space"), tr("play / pause the selected animation (none selected: all)") },
            { tr("+ / -"), tr("zoom the whole collage") },
            { tr("Ctrl + 0"), tr("fit the collage into the window") },
            { tr("E"), tr("switch to the editor") } });
        html += section(tr("Features"), {
            { tr("Exit"), tr("back to the image viewer (asks first); the collage stays in memory, Ctrl+G resumes it") },
            { tr("Layout"), tr("Mosaic, Grid, Row, Column or Freehand") },
            { tr("Layout options"), tr("Grid / Row / Column: columns, rows, cell size, rotation and cell style (tiles, circle, hexagon, polygon / star...)") },
            { tr("Canvas"), tr("Window, a pixel resolution (shared with the editor canvas) or Custom W x H (not used by Freehand)") },
            { tr("Background"), tr("App background and pattern, or a colour of your own (also the editor canvas colour) with an opacity") },
            { tr("Border"), tr("outline of every tile: width, colour, style (solid, dashed, dotted, dash-dot, double)") },
            { tr("Crop (Freehand)"), tr("drag moves the picture inside the tile, wheel zooms it, handles are off") },
            { tr("Gap"), tr("spacing between tiles (not used by Freehand)") },
            { tr("Aspect"), tr("shape of the selected tile (Original, 1:1, 4:3, 16:9, ...)") },
            { tr("Size W / H"), tr("drag the W / H label sideways (Shift = faster, Alt = finer) or type; exact in Freehand") },
            { tr("Borders"), tr("Snap to sizes, Free or Locked dragging of the borders between tiles") },
            { tr("Fit"), tr("Fill (crop), Contain or Stretch") },
            { tr("Zoom / Reset crop"), tr("frame the picture inside the tile") },
            { tr("Opacity / Corners"), tr("transparency and rounded corners") },
            { tr("Border (tile)"), tr("this tile's own outline; Use default follows the bar again") },
            { tr("Resolution"), tr("working copy size, lower it to save memory") },
            { tr("Animate"), tr("play GIF / animated WebP / PNG tiles; per tile: Play, Loop, Speed") },
            { tr("Edit / Export"), tr("arrange freely and save as an image") } });
        html += section(tr("Freehand layout"), {
            { tr("Drag a tile"), tr("move it (it comes to the front)") },
            { tr("Drag the handles"), tr("resize (Shift keeps proportions)") },
            { tr("Alt + drag"), tr("move the picture inside the tile") },
            { tr("Arrow keys"), tr("nudge the selected tile (Shift = 10 px)") },
            { tr("Page Up / Page Down"), tr("bring to front / send to back") },
            { tr("Drag empty space / middle-drag"), tr("pan the endless canvas at any zoom (tiles can be parked off-screen)") },
            { tr("Ctrl + 0"), tr("bring the canvas back to the centre") },
            { tr("Switching layout"), tr("your Freehand arrangement is kept for when you come back") } });
    } else {
        html += "<p><b>" + tr("Collage editor").toHtmlEscaped() + "</b></p>";
        html += section(tr("Mouse"), {
            { tr("Wheel"), tr("zoom the canvas") },
            { tr("Hold the middle button + move"), tr("move the canvas freely (middle click fits it back)") },
            { tr("Drag the border between two frames"), tr("automatic layouts: resize them (Alt = no snapping, double-click = reset)") },
            { tr("Click / drag a box"), tr("select one or several frames") },
            { tr("Drag a frame"), tr("move it (Ctrl disables snapping)") },
            { tr("Drag the handles"), tr("resize (Shift keeps proportions)") },
            { tr("Alt + drag / Crop mode"), tr("move the picture inside the frame") },
            { tr("Shift + wheel"), tr("zoom the picture inside the frame") },
            { tr("Drop files"), tr("add them to the canvas") } });
        html += section(tr("Keyboard"), {
            { tr("Arrow keys"), tr("nudge selected frames (Shift = 10 px)") },
            { tr("Ctrl + A"), tr("select all frames") },
            { tr("Page Up / Page Down"), tr("bring to front / send to back") },
            { tr("Delete / Backspace"), tr("remove the selected frames") },
            { tr("Esc"), tr("clear the selection") },
            { tr("Space"), tr("play / pause the selected animation (none selected: all)") },
            { tr("Ctrl + 0 / Ctrl + 1"), tr("fit the canvas / actual size") },
            { tr("+ / -"), tr("zoom the canvas") },
            { tr("E"), tr("back to the collage view") } });
        html += section(tr("Features"), {
            { tr("Layout"), tr("Mosaic, Grid, Row and Column arrange the frames automatically, Freehand = place them yourself") },
            { tr("Crop (Freehand)"), tr("drag moves the picture inside its frame, wheel zooms it, handles are off") },
            { tr("Canvas"), tr("size presets (incl. portrait / mobile) or custom width / height, shared with the view") },
            { tr("Background / Transparent"), tr("canvas colour, PNG / WebP keep transparency") },
            { tr("Border"), tr("outline of every frame (exported too)") },
            { tr("Layout options"), tr("Grid / Row / Column: counts, cell size, rotation, cell style (exported too)") },
            { tr("X, Y, W, H"), tr("exact position and size of a frame") },
            { tr("Fit to border / Center"), tr("snap a frame to the canvas") },
            { tr("Fit, Zoom, Opacity, Corners"), tr("per-frame look") },
            { tr("Animate"), tr("play animated images (export uses the first frame)") },
            { tr("Export..."), tr("save the collage as an image") } });
    }
    html += "<p style='margin-top:20px; line-height:130%'>" + tr("Fullscreen, open and settings shortcuts also work here.").toHtmlEscaped() + "</p>";
    return html;
}

void CollageWidget::updateHelpVisibility() {
    bool help = mHelpButton->isChecked();
    mPanelScroll->setVisible(!help);
    mHelpScroll->setVisible(help);
    mHelpViewLabel->setVisible(!isEditMode());
    mHelpEditLabel->setVisible(isEditMode());
}

void CollageWidget::setPanelVisible(bool visible) {
    // each mode remembers its own panel state
    if(isEditMode())
        mEditPanelVisible = visible;
    else
        mViewPanelOpen = visible;
    // narrow window: the two cards would cover everything, keep one
    if(visible && width() < NARROW_WIDTH && mLayoutPanel->isVisible())
        setLayoutPanelVisible(false);
    layoutOverlays();
    mPanelContainer->setVisible(visible);
    {
        QSignalBlocker blockEdit(mPanelToggle);
        QSignalBlocker blockView(mViewPanelButton);
        mPanelToggle->setChecked(visible);
        mViewPanelButton->setChecked(visible);
    }
    // the panel floats, so the view keeps its size, zoom and tiles; only the bar may move aside
    layoutOverlays();
}

// animations only run while the collage is on screen
void CollageWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    mScene->setAnimationsActive(true);
    updateOverlayVisibility(true);
}

void CollageWidget::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    mScene->setAnimationsActive(false);
}

void CollageWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    layoutOverlays();
    // panel starts hidden on narrow windows unless the user decided otherwise
    if(isEditMode() && !mPanelUserSet) {
        bool wanted = width() >= NARROW_WIDTH;
        if(wanted != mPanelContainer->isVisible())
            setPanelVisible(wanted);
    }
}

//------------------------------------------------------------------------------
// Layout options panel (Grid / Row / Column): floats at the left edge of the view
void CollageWidget::buildLayoutPanel() {
    mLayoutPanel = new CollagePanelFrame(this);

    mLayoutPanelTitle = new QLabel(tr("Layout options"), mLayoutPanel);
    QFont bold = mLayoutPanelTitle->font();
    bold.setBold(true);
    mLayoutPanelTitle->setFont(bold);
    QPushButton *hideButton = new QPushButton(tr("Hide"), mLayoutPanel);
    hideButton->setToolTip(tr("Hide this panel"));
    hideButton->setFocusPolicy(Qt::NoFocus);
    hideButton->setCursor(Qt::PointingHandCursor);
    hideButton->setMinimumHeight(30);
    connect(hideButton, &QPushButton::clicked, this, [this]() {
        setLayoutPanelVisible(false);
        mView->setFocus();
    });
    QHBoxLayout *header = new QHBoxLayout();
    header->setContentsMargins(14, 14, 14, 10);
    header->setSpacing(8);
    header->addWidget(mLayoutPanelTitle, 1);
    header->addWidget(hideButton);

    QFrame *separator = new QFrame(mLayoutPanel);
    separator->setAccessibleName("CollagePanelSeparator");
    separator->setFrameShape(QFrame::NoFrame);
    separator->setFixedHeight(1);

    QScrollArea *scroll = new QScrollArea(mLayoutPanel);
    scroll->setAccessibleName("CollagePanelScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    QWidget *content = new QWidget();
    content->setAccessibleName("CollagePanel");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(16, 10, 16, 18);
    contentLayout->setSpacing(14);
    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->setColumnMinimumWidth(0, 84);
    grid->setColumnStretch(1, 1);
    int row = 0;

    auto makeSpin = [content](int min, int max, const QString &suffix, const QString &special) {
        ScrubSpinBox *spin = new ScrubSpinBox(content);
        spin->setRange(min, max);
        if(!suffix.isEmpty())
            spin->setSuffix(suffix);
        if(!special.isEmpty())
            spin->setSpecialValueText(special);
        return spin;
    };
    auto addRow = [content, grid, &row](const QString &text, QWidget *control, QList<QWidget*> *group) {
        QLabel *label = new QLabel(text, content);
        grid->addWidget(label, row, 0);
        grid->addWidget(control, row++, 1);
        if(group)
            *group << label << control;
        return label;
    };
    auto section = [content, grid, &row](const QString &text) {
        QLabel *title = new QLabel(text, content);
        title->setAccessibleName("CollageSectionTitle");
        grid->addWidget(title, row++, 0, 1, 2);
    };

    // --- cells -------------------------------------------------------------
    section(tr("Cells"));
    mLayColumns = makeSpin(0, 12, QString(), tr("Auto"));
    mLayRows = makeSpin(0, 12, QString(), tr("Auto"));
    mLayCellW = makeSpin(0, 8000, " px", tr("Auto"));
    mLayCellH = makeSpin(0, 8000, " px", tr("Auto"));
    mLayCellW->setToolTip(tr("Auto: the cells share the canvas. A fixed size centres the grid (pan to see the rest)."));
    mLayCellH->setToolTip(mLayCellW->toolTip());
    mLayHoneycomb = new QCheckBox(tr("Honeycomb (shift every other row)"), content);
    addRow(tr("Columns"), mLayColumns, &mLayGridRows);
    addRow(tr("Rows"), mLayRows, &mLayGridRows);
    addRow(tr("Cell width"), mLayCellW, &mLayGridRows);
    addRow(tr("Cell height"), mLayCellH, &mLayGridRows);
    grid->addWidget(mLayHoneycomb, row++, 0, 1, 2);
    mLayGridRows << mLayHoneycomb;

    mLayLines = makeSpin(1, 12, QString(), QString());
    mLayLineSize = makeSpin(0, 8000, " px", tr("Auto"));
    mLayLinesLabel = addRow(tr("Rows"), mLayLines, &mLayLineRows);
    mLayLineSizeLabel = addRow(tr("Row height"), mLayLineSize, &mLayLineRows);

    mLayRotation = makeSpin(-45, 45, QString::fromUtf8("\xC2\xB0"), QString());
    mLayRotation->setToolTip(tr("Turns every tile around its centre"));
    mLayGap = makeSpin(0, 400, " px", QString());
    addRow(tr("Rotation"), mLayRotation, nullptr);
    addRow(tr("Gap"), mLayGap, nullptr);

    // --- style -------------------------------------------------------------
    section(tr("Style"));
    mLayShape = new QComboBox(content);
    mLayShape->addItems({ tr("Default"), tr("Tiles (rounded)"), tr("Circle"), tr("Hexagon"), tr("Diamond"),
                          tr("Triangle"), tr("Parallelogram"), tr("Custom polygon") });
    addRow(tr("Shape"), mLayShape, nullptr);
    mLaySides = makeSpin(3, 12, QString(), QString());
    addRow(tr("Corners"), mLaySides, &mLayPolygonRows);
    mLayStar = new QCheckBox(tr("Star"), content);
    grid->addWidget(mLayStar, row++, 1);
    mLayPolygonRows << mLayStar;
    mLayStarDepth = makeSpin(10, 90, "%", QString());
    mLayStarDepth->setToolTip(tr("Inner points of the star, in percent of the outer ones"));
    addRow(tr("Star depth"), mLayStarDepth, &mLayPolygonRows);

    // --- borders -----------------------------------------------------------
    section(tr("Borders"));
    mLayBorderMode = new QComboBox(content);
    mLayBorderMode->addItems({ tr("Snap to sizes"), tr("Free"), tr("Locked") });
    mLayBorderMode->setToolTip(tr("Dragging the border between two tiles resizes them. Snap: locks to 1/4, 1/3, 1/2, 2/3, 3/4 "
                                  "and equal sizes (hold Alt to drag freely). Free: any size. Locked: borders can not be dragged."));
    addRow(tr("Dragging"), mLayBorderMode, nullptr);
    QPushButton *resetButton = new QPushButton(tr("Reset layout"), content);
    resetButton->setFocusPolicy(Qt::NoFocus);
    resetButton->setToolTip(tr("Automatic counts and sizes, no rotation, plain cells, borders back in place"));
    grid->addWidget(resetButton, row++, 0, 1, 2);

    contentLayout->addLayout(grid);
    contentLayout->addStretch(1);
    scroll->setWidget(content);

    QVBoxLayout *outer = new QVBoxLayout(mLayoutPanel);
    outer->setContentsMargins(1, 0, 1, 1);
    outer->setSpacing(0);
    outer->addLayout(header);
    outer->addWidget(separator);
    outer->addWidget(scroll, 1);
    mLayoutPanel->hide();

    for(ScrubSpinBox *spin : { mLayColumns, mLayRows, mLayCellW, mLayCellH, mLayLines, mLayLineSize,
                               mLayRotation, mLaySides, mLayStarDepth })
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, &CollageWidget::applyLayoutPanel);
    for(QCheckBox *check : { mLayHoneycomb, mLayStar })
        connect(check, &QCheckBox::toggled, this, &CollageWidget::applyLayoutPanel);
    connect(mLayShape, qOverload<int>(&QComboBox::currentIndexChanged), this, &CollageWidget::applyLayoutPanel);
    connect(mLayGap, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        if(mSyncing)
            return;
        // same gap as the bar of the current mode
        if(isEditMode())
            mGapSpin->setValue(value);
        else
            mViewGapSpin->setValue(value);
    });
    connect(mLayBorderMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if(mSyncing || index < 0)
            return;
        syncBorderModeCombos(index);
    });
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        mScene->setLayoutOptions(CollageLayout::Options());
        syncLayoutPanel();
    });
}

bool CollageWidget::layoutPanelAllowed() const {
    return CollageLayout::usesCellOptions(mScene->activeLayout());
}

// controls follow the scene options and the current layout
void CollageWidget::syncLayoutPanel() {
    if(!mLayoutPanel)
        return;
    CollageLayout::Mode mode = mScene->activeLayout();
    bool gridMode = (mode == CollageLayout::MODE_GRID);
    bool rowMode = (mode == CollageLayout::MODE_ROW);
    const CollageLayout::Options &o = mScene->layoutOptions();
    bool wasSyncing = mSyncing;
    mSyncing = true;
    mLayoutPanelTitle->setText(gridMode ? tr("Grid options") : rowMode ? tr("Row options") : tr("Column options"));
    for(QWidget *widget : mLayGridRows)
        widget->setVisible(gridMode);
    for(QWidget *widget : mLayLineRows)
        widget->setVisible(!gridMode);
    mLayLinesLabel->setText(rowMode ? tr("Rows") : tr("Columns"));
    mLayLineSizeLabel->setText(rowMode ? tr("Row height") : tr("Column width"));
    mLayColumns->setValue(o.columns);
    mLayRows->setValue(o.rows);
    mLayCellW->setValue(qRound(o.cellWidth));
    mLayCellH->setValue(qRound(o.cellHeight));
    mLayHoneycomb->setChecked(o.honeycomb);
    mLayLines->setValue(qMax(1, rowMode ? o.rows : o.columns));
    mLayLineSize->setValue(qRound(rowMode ? o.cellHeight : o.cellWidth));
    mLayRotation->setValue(qRound(o.rotation));
    mLayShape->setCurrentIndex(o.shape);
    mLaySides->setValue(o.polygonSides);
    mLayStar->setChecked(o.star);
    mLayStarDepth->setValue(qRound(o.starDepth * 100.0));
    for(QWidget *widget : mLayPolygonRows)
        widget->setVisible(o.shape == CollageLayout::SHAPE_POLYGON);
    mLayGap->setValue(isEditMode() ? mGapSpin->value() : mViewGapSpin->value());
    mLayBorderMode->setCurrentIndex(settings->collageBorderMode());
    mSyncing = wasSyncing;
}

void CollageWidget::applyLayoutPanel() {
    if(mSyncing)
        return;
    CollageLayout::Mode mode = mScene->activeLayout();
    CollageLayout::Options o = mScene->layoutOptions(); // keeps the dragged borders unless the structure changes
    const CollageLayout::Options before = o;
    if(mode == CollageLayout::MODE_GRID) {
        o.columns = mLayColumns->value();
        o.rows = mLayRows->value();
        o.cellWidth = mLayCellW->value();
        o.cellHeight = mLayCellH->value();
        o.honeycomb = mLayHoneycomb->isChecked();
    } else if(mode == CollageLayout::MODE_ROW) {
        o.rows = mLayLines->value();
        o.cellHeight = mLayLineSize->value();
    } else if(mode == CollageLayout::MODE_COLUMN) {
        o.columns = mLayLines->value();
        o.cellWidth = mLayLineSize->value();
    }
    o.rotation = mLayRotation->value();
    o.shape = qMax(0, mLayShape->currentIndex());
    o.polygonSides = mLaySides->value();
    o.star = mLayStar->isChecked();
    o.starDepth = mLayStarDepth->value() / 100.0;
    // different counts / sizes: the dragged borders belonged to another structure
    if(o.columns != before.columns || o.rows != before.rows || !qFuzzyCompare(1.0 + o.cellWidth, 1.0 + before.cellWidth)
       || !qFuzzyCompare(1.0 + o.cellHeight, 1.0 + before.cellHeight) || o.honeycomb != before.honeycomb)
        o.splits.clear();
    mScene->setLayoutOptions(o);
    for(QWidget *widget : mLayPolygonRows)
        widget->setVisible(o.shape == CollageLayout::SHAPE_POLYGON);
}

void CollageWidget::setLayoutPanelVisible(bool visible) {
    mLayoutPanelWanted = visible;
    bool show = visible && layoutPanelAllowed();
    // narrow window: one floating card at a time
    if(show && width() < NARROW_WIDTH && mPanelContainer->isVisible())
        setPanelVisible(false);
    for(QPushButton *button : { mViewLayoutPanelButton, mEditLayoutPanelButton }) {
        QSignalBlocker block(button);
        button->setChecked(visible);
    }
    syncLayoutPanel();
    mLayoutPanel->setVisible(show);
    layoutOverlays();
}
