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
#include "settings.h"
#include "utils/imagelib.h"

namespace {

struct CanvasPreset {
    const char *name;
    int width, height;
};

const CanvasPreset PRESETS[] = {
    { QT_TR_NOOP("Full HD  1920 x 1080"),         1920, 1080 },
    { QT_TR_NOOP("4K UHD  3840 x 2160"),          3840, 2160 },
    { QT_TR_NOOP("Square  1080 x 1080"),          1080, 1080 },
    { QT_TR_NOOP("Portrait 4:5  1080 x 1350"),    1080, 1350 },
    { QT_TR_NOOP("Story 9:16  1080 x 1920"),      1080, 1920 },
    { QT_TR_NOOP("A4 portrait  2480 x 3508"),     2480, 3508 },
    { QT_TR_NOOP("A4 landscape  3508 x 2480"),    3508, 2480 },
};
const int PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);

// aspect ratio choices for view tiles (0 = keep the image aspect)
const qreal VIEW_ASPECTS[] = { 0.0, 1.0, 4.0 / 3.0, 3.0 / 2.0, 16.0 / 9.0, 9.0 / 16.0, 2.0 / 3.0, 3.0 / 4.0 };
const int VIEW_ASPECT_COUNT = sizeof(VIEW_ASPECTS) / sizeof(VIEW_ASPECTS[0]);
// the overlay bar shows up when the cursor is this close to the top edge
const int OVERLAY_TRIGGER = 70;

const int PANEL_WIDTH = 320;
// gap between the floating panel and the edges of the view
const int PANEL_MARGIN = 10;
// the floating bar only makes room for the panel when this much width is left next to it
const int BAR_MIN_WIDTH = 360;
const int BACKDROP_INTERVAL = 40;
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

// cheap blur without extra libraries: a smooth round trip through a tiny copy of the image
QPixmap blurred(const QPixmap &source) {
    QImage image = source.toImage();
    const QSize full = image.size();
    if(full.isEmpty())
        return QPixmap();
    const QSize medium(qMax(1, full.width() / 4), qMax(1, full.height() / 4));
    const QSize tiny(qMax(1, full.width() / 12), qMax(1, full.height() / 12));
    auto resize = [](const QImage &img, const QSize &size) {
        return img.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    };
    image = resize(resize(resize(resize(image, medium), tiny), medium), full);
    QPixmap result = QPixmap::fromImage(image);
    result.setDevicePixelRatio(source.devicePixelRatio());
    return result;
}

QString megabytes(qint64 bytes) {
    return QString::number(bytes / (1024.0 * 1024.0), 'f', bytes < 10LL * 1024 * 1024 ? 1 : 0) + " MB";
}

}

CollagePanelFrame::CollagePanelFrame(QWidget *parent) : QWidget(parent) {
    setAccessibleName("CollagePanelContainer");
}

void CollagePanelFrame::setBackdrop(const QPixmap &pixmap) {
    mBackdrop = pixmap;
    update();
}

void CollagePanelFrame::paintEvent(QPaintEvent *) {
    const ColorScheme &colors = settings->colorScheme();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(box, 8, 8);
    painter.setClipPath(path);
    if(!mBackdrop.isNull())
        painter.drawPixmap(rect(), mBackdrop);
    QColor tint = colors.widget;
    tint.setAlpha(mBackdrop.isNull() ? 235 : 190);
    painter.fillPath(path, tint);
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

    buildToolbar();
    buildPanel();
    buildOverlay();
    connect(settings, &Settings::settingsChanged, this, &CollageWidget::updateButtonIcons);
    connect(mView, &CollageView::animationToggleRequested, this, [this]() { setAnimate(!mScene->animationsEnabled()); });
    setAnimate(settings->collageAnimate());
    setStaticCanvas(settings->collageStaticCanvas());

    QHBoxLayout *body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(mView, 1); // the properties panel floats above the view instead of taking room from it

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(mToolbar);
    root->addLayout(body, 1);
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

    mBackdropTimer = new QTimer(this);
    mBackdropTimer->setSingleShot(true);
    mBackdropTimer->setInterval(BACKDROP_INTERVAL);
    connect(mBackdropTimer, &QTimer::timeout, this, &CollageWidget::refreshBackdrop);
    connect(mScene, &QGraphicsScene::changed, this, [this]() { scheduleBackdrop(); });
    connect(mView, &CollageView::viewMoved, this, [this]() { scheduleBackdrop(); });
    connect(settings, &Settings::settingsChanged, this, [this]() {
        mPanelContainer->update();
        scheduleBackdrop();
    });

    mScene->setCanvasSize(QSize(PRESETS[0].width, PRESETS[0].height));
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
    // the editor arranges everything the first time it is opened
    bool firstEdit = (mode == CollageMode::Edit && !mScene->editorInitialized());
    mScene->setMode(mode);
    applyModeUi();
    if(firstEdit && mScene->imageCount() > 0)
        onArrange();
    emitInfoIfChanged();
}

// shows / hides the chrome that belongs to the current mode
void CollageWidget::applyModeUi() {
    bool view = !isEditMode();
    mView->setViewMode(view);
    mToolbar->setVisible(!view);
    mStatusLabel->setVisible(!view);
    updateLayoutRows();
    bool panel = view ? mViewPanelOpen : (mPanelUserSet ? mEditPanelVisible : width() >= NARROW_WIDTH);
    setPanelVisible(panel);
    updateHelpVisibility();
    updatePanel();
    updateOverlayVisibility();
    layoutOverlays();
    mView->setFocus();
}

// panel rows / bar controls that depend on the mode and, in the view, on the layout
void CollageWidget::updateLayoutRows() {
    bool view = !isEditMode();
    bool free = view && mScene->isFreeView();
    for(QWidget *widget : mEditOnlyWidgets)
        widget->setVisible(!view);
    // aspect / size only steer the automatic layouts
    for(QWidget *widget : mViewOnlyWidgets)
        widget->setVisible(view && !free);
    for(QWidget *widget : mStackWidgets)
        widget->setVisible(!view || free);
    // static canvas: the editor frames are laid out automatically, so manual geometry is locked
    bool locked = !view && mScene->staticCanvas();
    for(QWidget *widget : mEditOnlyWidgets)
        widget->setEnabled(!locked);
    for(QWidget *widget : mStackWidgets)
        widget->setEnabled(!locked);
    mArrangeButton->setEnabled(!locked);
    mViewGapSpin->setEnabled(!free); // tiles are placed by hand in Freehand, so there is no gap to apply
}

//------------------------------------------------------------------------------
// view mode chrome: floating bar (auto-hides) + hint for an empty collage
void CollageWidget::buildOverlay() {
    mOverlayBar = new QWidget(this);
    mOverlayBar->setAccessibleName("CollageOverlayBar");
    mOverlayLayout = new WrapLayout(mOverlayBar, 8, 8, 6);

    QPushButton *backButton = new QPushButton(tr("Back"), mOverlayBar);
    backButton->setToolTip(tr("Back to the image viewer"));
    setButtonIcon(backButton, "buttons/panel/back20");
    connect(backButton, &QPushButton::clicked, this, &CollageWidget::exitRequested);

    QPushButton *addButton = new QPushButton(tr("+ Add"), mOverlayBar);
    addButton->setToolTip(tr("Add images (you can also drop files here)"));
    connect(addButton, &QPushButton::clicked, this, &CollageWidget::onAddImages);

    mViewLayoutCombo = new QComboBox(mOverlayBar);
    mViewLayoutCombo->addItems({ tr("Mosaic"), tr("Grid"), tr("Row"), tr("Column"), tr("Freehand") });
    mViewLayoutCombo->setToolTip(tr("Mosaic, Grid, Row and Column arrange the tiles for you. Freehand lets you place and resize them yourself."));
    connect(mViewLayoutCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if(index < 0)
            return;
        mScene->setViewLayout(static_cast<CollageLayout::Mode>(index));
        mView->fitCanvas(); // Freehand has a bigger scene than the other layouts
        updateLayoutRows();
        updatePanel();
        mView->setFocus();
    });

    mViewGapSpin = new QSpinBox(mOverlayBar);
    mViewGapSpin->setRange(0, 100);
    mViewGapSpin->setValue(mScene->viewGap());
    mViewGapSpin->setSuffix(" px");
    mViewGapSpin->setKeyboardTracking(false);
    connect(mViewGapSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        mScene->setViewGap(value);
    });

    mViewAnimateCheck = new QCheckBox(tr("Animate"), mOverlayBar);
    mViewAnimateCheck->setToolTip(tr("Play animated images (GIF, animated WebP / PNG). Space toggles it."));
    mViewAnimateCheck->setFocusPolicy(Qt::NoFocus);
    connect(mViewAnimateCheck, &QCheckBox::toggled, this, &CollageWidget::setAnimate);

    mViewPanelButton = new QPushButton(tr("Tile settings"), mOverlayBar);
    mViewPanelButton->setCheckable(true);
    mViewPanelButton->setToolTip(tr("Aspect ratio, size, crop and resolution of the selected tile (also: right-click a tile)"));
    connect(mViewPanelButton, &QPushButton::toggled, this, [this](bool checked) { setPanelVisible(checked); });

    QPushButton *editButton = new QPushButton(tr("Edit / Export"), mOverlayBar);
    editButton->setToolTip(tr("Arrange freely on a fixed canvas and save the collage as an image (E)"));
    connect(editButton, &QPushButton::clicked, this, [this]() { setMode(CollageMode::Edit); });

    for(QPushButton *button : {backButton, addButton, mViewPanelButton, editButton})
        button->setFocusPolicy(Qt::NoFocus);
    mViewLayoutCombo->setFocusPolicy(Qt::NoFocus);

    mOverlayLayout->addWidget(backButton);
    mOverlayLayout->addWidget(addButton);
    mOverlayLayout->addWidget(labeled(tr("Layout"), mViewLayoutCombo, mOverlayBar));
    mOverlayLayout->addWidget(labeled(tr("Gap"), mViewGapSpin, mOverlayBar));
    mOverlayLayout->addWidget(mViewAnimateCheck);
    mOverlayLayout->addWidget(mViewPanelButton);
    mOverlayLayout->addWidget(editButton);

    mOverlayTimer = new QTimer(this);
    mOverlayTimer->setSingleShot(true);
    mOverlayTimer->setInterval(2200);
    connect(mOverlayTimer, &QTimer::timeout, this, [this]() {
        // keep it while it is being used (hover, open combo popup) or while there is nothing to look at
        if(mOverlayBar->underMouse() || QApplication::activePopupWidget() || mScene->imageCount() == 0) {
            mOverlayTimer->start();
            return;
        }
        mOverlayBar->hide();
    });

    mEmptyHint = new QLabel(tr("Add images or drop files here"), this);
    mEmptyHint->setAccessibleName("CollageEmptyHint");
    mEmptyHint->setAlignment(Qt::AlignCenter);
    mEmptyHint->setAttribute(Qt::WA_TransparentForMouseEvents);

    mOverlayBar->hide();
    mEmptyHint->hide();
}

// panel: a floating card at the right edge of the view
void CollageWidget::layoutPanel() {
    QRect area = mView->geometry();
    int width = qMax(160, qMin(PANEL_WIDTH, area.width() - 2 * PANEL_MARGIN));
    int height = qMax(120, area.height() - 2 * PANEL_MARGIN);
    mPanelContainer->setGeometry(area.right() - width - PANEL_MARGIN + 1, area.y() + PANEL_MARGIN, width, height);
}

// bar centered at the top of the view (beside the panel when it is open), wraps on narrow windows
void CollageWidget::layoutOverlays() {
    QRect area = mView->geometry();
    mEmptyHint->setGeometry(area);
    mEmptyHint->raise();
    layoutPanel();
    mPanelContainer->raise();

    QRect barArea = area;
    if(mPanelContainer->isVisible()) {
        int room = mPanelContainer->x() - PANEL_MARGIN - area.x();
        if(room >= BAR_MIN_WIDTH)
            barArea.setWidth(room);
    }
    int width = qMax(120, qMin(barArea.width() - 20, 780));
    int height = mOverlayLayout->heightForWidth(width);
    mOverlayBar->setGeometry(barArea.x() + (barArea.width() - width) / 2, barArea.y() + 10, width, height);
    mOverlayBar->raise();
}

void CollageWidget::showOverlay() {
    if(isEditMode())
        return;
    layoutOverlays();
    mOverlayBar->show();
    mOverlayBar->raise();
    mOverlayTimer->start();
}

void CollageWidget::updateOverlayVisibility() {
    bool view = !isEditMode();
    if(!view) {
        mOverlayBar->hide();
        mEmptyHint->hide();
        mOverlayTimer->stop();
        return;
    }
    bool empty = (mScene->imageCount() == 0);
    mEmptyHint->setVisible(empty);
    if(empty) {
        layoutOverlays();
        mOverlayBar->show();
        mOverlayBar->raise();
    } else if(mOverlayBar->isVisible() && !mOverlayTimer->isActive()) {
        mOverlayTimer->start();
    }
}

bool CollageWidget::eventFilter(QObject *watched, QEvent *event) {
    if(watched == mView->viewport()) {
        if(event->type() == QEvent::MouseMove) {
            if(!isEditMode() && static_cast<QMouseEvent*>(event)->pos().y() < OVERLAY_TRIGGER)
                showOverlay();
        } else if(event->type() == QEvent::Resize) {
            layoutOverlays();
            scheduleBackdrop();
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
    WrapLayout *layout = new WrapLayout(mToolbar, 8, 12, 6);

    auto makeButton = [this](const QString &text, const QString &tip, const QString &iconName = QString()) {
        QPushButton *button = new QPushButton(text, mToolbar);
        button->setToolTip(tip);
        button->setFocusPolicy(Qt::NoFocus);
        if(!iconName.isEmpty())
            setButtonIcon(button, iconName);
        return button;
    };

    QPushButton *addButton = makeButton(tr("Add images..."), tr("Add more images to the collage (you can also drop files here)"));
    QPushButton *newButton = makeButton(tr("New"), tr("Remove all images and start over"));
    connect(addButton, &QPushButton::clicked, this, &CollageWidget::onAddImages);
    connect(newButton, &QPushButton::clicked, this, &CollageWidget::onNewCollage);
    layout->addWidget(addButton);
    layout->addWidget(newButton);

    // layout
    mLayoutCombo = new QComboBox(mToolbar);
    mLayoutCombo->addItems({ tr("Mosaic"), tr("Grid"), tr("Row"), tr("Column") });
    mGapSpin = new QSpinBox(mToolbar);
    mGapSpin->setRange(0, 400);
    mGapSpin->setValue(12);
    mGapSpin->setSuffix(" px");
    mGapSpin->setKeyboardTracking(false);
    mGapSpin->setToolTip(tr("Spacing between images and around the edges"));
    mArrangeButton = makeButton(tr("Arrange"), tr("Re-arrange all images with the selected layout (resets crops)"));
    connect(mArrangeButton, &QPushButton::clicked, this, &CollageWidget::onArrange);
    mStaticCheck = new QCheckBox(tr("Static canvas"), mToolbar);
    mStaticCheck->setToolTip(tr("Lock the layout: images stay in the automatic Layout / Gap arrangement, no free moving or resizing. "
                                "Drag still moves the picture inside its frame, Ctrl+drag swaps two frames."));
    mStaticCheck->setFocusPolicy(Qt::NoFocus);
    connect(mStaticCheck, &QCheckBox::toggled, this, &CollageWidget::setStaticCanvas);
    connect(mLayoutCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { applyStaticLayout(); });
    connect(mGapSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this]() { applyStaticLayout(); });
    layout->addWidget(labeled(tr("Layout"), mLayoutCombo, mToolbar));
    layout->addWidget(labeled(tr("Gap"), mGapSpin, mToolbar));
    layout->addWidget(mArrangeButton);
    layout->addWidget(mStaticCheck);

    // canvas
    mPresetCombo = new QComboBox(mToolbar);
    for(int i = 0; i < PRESET_COUNT; i++)
        mPresetCombo->addItem(tr(PRESETS[i].name));
    mPresetCombo->addItem(tr("Custom"));
    mCanvasWidthSpin = new QSpinBox(mToolbar);
    mCanvasHeightSpin = new QSpinBox(mToolbar);
    for(QSpinBox *spin : {mCanvasWidthSpin, mCanvasHeightSpin}) {
        spin->setRange(16, 16384);
        spin->setKeyboardTracking(false);
        spin->setSuffix(" px");
    }
    mCanvasWidthSpin->setValue(PRESETS[0].width);
    mCanvasHeightSpin->setValue(PRESETS[0].height);
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
    QPushButton *viewButton = makeButton(tr("View"), tr("Back to the collage view (E)"), "buttons/panel/back20");
    connect(viewButton, &QPushButton::clicked, this, [this]() { setMode(CollageMode::View); });
    mAnimateCheck = new QCheckBox(tr("Animate"), mToolbar);
    mAnimateCheck->setToolTip(tr("Play animated images (GIF, animated WebP / PNG). Export always uses the first frame."));
    mAnimateCheck->setFocusPolicy(Qt::NoFocus);
    connect(mAnimateCheck, &QCheckBox::toggled, this, &CollageWidget::setAnimate);
    layout->addWidget(viewButton);
    layout->addWidget(fitButton);
    layout->addWidget(mAnimateCheck);
    layout->addWidget(mPanelToggle);
    layout->addWidget(exportButton);
}

void CollageWidget::buildPanel() {
    mPanelScroll = new QScrollArea(this);
    mPanelScroll->setAccessibleName("CollagePanelScroll");
    mPanelScroll->setWidgetResizable(true);
    mPanelScroll->setFrameShape(QFrame::NoFrame);
    mPanelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mPanelScroll->viewport()->setAutoFillBackground(false); // the blurred backdrop shows through

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
        QSpinBox *spin = new QSpinBox(mPanelContent);
        spin->setRange(min, max);
        spin->setKeyboardTracking(false);
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
    mViewSizeSlider = new QSlider(Qt::Horizontal, mPanelContent);
    mViewSizeSlider->setRange(50, 300);
    mViewSizeValueLabel = makeValueLabel();
    grid->addWidget(sizeLabel, row, 0);
    grid->addWidget(mViewSizeSlider, row, 1);
    grid->addWidget(mViewSizeValueLabel, row++, 2);
    mViewOnlyWidgets << aspectLabel << mViewAspectCombo << sizeLabel << mViewSizeSlider << mViewSizeValueLabel;

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
    mCropModeButton->setToolTip(tr("While on, dragging inside a frame moves the picture under it (always on in the collage view). Shift+wheel zooms."));
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
    connect(mViewSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        if(mSyncing)
            return;
        for(CollageItem *item : targets())
            item->setViewWeight(value / 100.0);
        mViewSizeValueLabel->setText(QString::number(value) + "%");
        mScene->relayoutView();
    });
    connect(mZoomSlider, &QSlider::valueChanged, this, [this](int value) {
        if(mSyncing)
            return;
        for(CollageItem *item : targets())
            item->setContentZoom(value / 100.0);
        mZoomValueLabel->setText(QString::number(value) + "%");
    });
    connect(mCropModeButton, &QPushButton::toggled, this, [this](bool on) {
        for(CollageItem *item : mScene->collageItems())
            item->setCropMode(on);
    });
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

// mirrors the first selected item into the panel
void CollageWidget::updatePanel() {
    mSyncing = true;
    const QList<CollageItem*> selected = mScene->selectedCollageItems();
    CollageItem *item = selected.isEmpty() ? nullptr : selected.first();
    mPanelContent->setEnabled(item != nullptr);

    if(!item) {
        mNameLabel->setText(tr("No image selected"));
        if(isEditMode() && mScene->staticCanvas())
            mInfoLabel->setText(tr("Static canvas: images follow the Layout / Gap above. Drag moves the picture inside its frame, "
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
    mCropModeButton->setEnabled(item->fit() == CollageFit::Fill);
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
    int size = qRound(item->viewWeight() * 100.0);
    mViewSizeSlider->setValue(size);
    mViewSizeValueLabel->setText(QString::number(size) + "%");

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
            item->setCropMode(mCropModeButton->isChecked());
            added++;
        } else {
            failed++;
        }
    }
    if(wasEmpty && added > 0)
        mScene->applyLayout(static_cast<CollageLayout::Mode>(mLayoutCombo->currentIndex()), mGapSpin->value());
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

void CollageWidget::onArrange() {
    mScene->applyLayout(static_cast<CollageLayout::Mode>(mLayoutCombo->currentIndex()), mGapSpin->value());
}

//------------------------------------------------------------------------------
void CollageWidget::onPresetChanged(int index) {
    if(index < 0 || index >= PRESET_COUNT)
        return; // "Custom": keep the current values
    {
        QSignalBlocker blockW(mCanvasWidthSpin);
        QSignalBlocker blockH(mCanvasHeightSpin);
        mCanvasWidthSpin->setValue(PRESETS[index].width);
        mCanvasHeightSpin->setValue(PRESETS[index].height);
    }
    applyCanvasSize();
}

void CollageWidget::onCanvasSpinChanged() {
    // switch the preset box to a matching preset, or to "Custom"
    int match = PRESET_COUNT;
    for(int i = 0; i < PRESET_COUNT; i++) {
        if(PRESETS[i].width == mCanvasWidthSpin->value() && PRESETS[i].height == mCanvasHeightSpin->value())
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

void CollageWidget::applyStaticLayout() {
    mScene->setStaticLayout(static_cast<CollageLayout::Mode>(mLayoutCombo->currentIndex()), mGapSpin->value());
}

// static canvas: editor frames follow the Layout / Gap toolbar values instead of free placement
void CollageWidget::setStaticCanvas(bool enabled) {
    {
        QSignalBlocker block(mStaticCheck);
        mStaticCheck->setChecked(enabled);
    }
    settings->setCollageStaticCanvas(enabled);
    applyStaticLayout();
    mScene->setStaticCanvas(enabled);
    updateLayoutRows();
    updatePanel();
    mView->setFocus();
}

void CollageWidget::onCanvasColorChanged() {
    QColor color = mBackgroundButton->color();
    if(mTransparentCheck->isChecked())
        color.setAlpha(0);
    mBackgroundButton->setEnabled(!mTransparentCheck->isChecked());
    mScene->setCanvasColor(color);
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
            { tr("Drag empty space / middle-drag"), tr("pan the whole collage") },
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
            { tr("Esc"), tr("clear the selection") },
            { tr("Space"), tr("play / pause the selected animation (none selected: all)") },
            { tr("+ / -"), tr("zoom the whole collage") },
            { tr("E"), tr("switch to the editor") } });
        html += section(tr("Features"), {
            { tr("Layout"), tr("Mosaic, Grid, Row, Column or Freehand") },
            { tr("Gap"), tr("spacing between tiles (not used by Freehand)") },
            { tr("Aspect"), tr("shape of the selected tile (Original, 1:1, 4:3, 16:9, ...)") },
            { tr("Size"), tr("how much room the tile takes in the mosaic") },
            { tr("Fit"), tr("Fill (crop), Contain or Stretch") },
            { tr("Zoom / Reset crop"), tr("frame the picture inside the tile") },
            { tr("Opacity / Corners"), tr("transparency and rounded corners") },
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
            { tr("Middle-drag"), tr("pan the canvas") },
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
            { tr("Layout + Arrange"), tr("re-arrange everything (resets crops)") },
            { tr("Static canvas"), tr("lock the arrangement to the Layout / Gap (no free placement)") },
            { tr("Canvas"), tr("size presets or custom width / height") },
            { tr("Background / Transparent"), tr("canvas colour, PNG / WebP keep transparency") },
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
    if(visible)
        refreshBackdrop();
}

void CollageWidget::scheduleBackdrop() {
    if(mPanelContainer->isVisible() && !mBackdropTimer->isActive())
        mBackdropTimer->start();
}

// blurred copy of the part of the collage under the panel
void CollageWidget::refreshBackdrop() {
    if(!mPanelContainer->isVisible())
        return;
    QWidget *viewport = mView->viewport();
    QRect area(viewport->mapFrom(this, mPanelContainer->pos()), mPanelContainer->size());
    area &= viewport->rect();
    if(area.isEmpty())
        return;
    // the panel is a sibling of the view, so it is not part of the grab
    mPanelContainer->setBackdrop(blurred(viewport->grab(area)));
}

// animations only run while the collage is on screen
void CollageWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    mScene->setAnimationsActive(true);
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
