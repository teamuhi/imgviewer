#include "topbar.h"
#include "settings.h"
#include "utils/imagelib.h"

namespace {
const int BUTTON_SIZE = 32;
// below this width the resolution / file size text is dropped
const int HIDE_INFO_WIDTH = 480;
// below this width the position counter and the zoom are dropped as well
const int HIDE_INDEX_WIDTH = 300;
// the CPU / RAM readout goes first
const int HIDE_PERF_WIDTH = 640;
// the settings row collapses into a dropdown before the file name gets squeezed below this
const int MIN_NAME_WIDTH = 160;
const int SEPARATOR_HEIGHT = 18;
const int MENU_ICON_SIZE = 16;
}

TopBar::TopBar(QWidget *parent) : QWidget(parent) {
    setAccessibleName("TopBar");
    setAttribute(Qt::WA_StyledBackground, true);

    int height = qMax(BUTTON_SIZE + 10, QFontMetrics(font()).height() + 20);
    setFixedHeight(height);

    indexLabel.setAccessibleName("TopBarLabel");
    nameLabel.setAccessibleName("TopBarLabel");
    infoLabel.setAccessibleName("TopBarLabel");
    nameLabel.setText(tr("No file opened."));
    // the name label takes whatever space is left and gets elided in updateLayoutState()
    nameLabel.setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    nameLabel.setMinimumWidth(0);
    infoLabel.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    infoLabel.hide();
    indexLabel.hide();
    zoomLabel.setAccessibleName("TopBarLabel");
    zoomLabel.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    zoomLabel.setToolTip(tr("Magnification"));
    // fixed width (widest realistic value) so the bar does not jitter while zooming
    zoomLabel.setFixedWidth(zoomLabel.fontMetrics().horizontalAdvance("50000%") + 4);
    zoomLabel.hide();
    perfLabel.setAccessibleName("TopBarLabel");
    perfLabel.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    perfLabel.setToolTip(tr("qimgv process: CPU (all cores) / memory"));
    perfLabel.setFixedWidth(perfLabel.fontMetrics().horizontalAdvance("CPU 100%   RAM 99999 MB") + 4);
    perfLabel.hide();

    backButton       = new ActionButton("documentView", ":/res/icons/common/buttons/panel/back20.png",       BUTTON_SIZE, this);
    openButton       = new ActionButton("open",         ":/res/icons/common/buttons/panel/open20.png",       BUTTON_SIZE, this);
    collageButton    = new ActionButton("openCollage",  ":/res/icons/common/buttons/panel/collage20.png",    BUTTON_SIZE, this);
    folderViewButton = new ActionButton("folderView",   ":/res/icons/common/buttons/panel/folderview20.png", BUTTON_SIZE, this);
    settingsButton   = new ActionButton("openSettings", ":/res/icons/common/buttons/panel/settings20.png",   BUTTON_SIZE, this);
    backButton->setToolTip(tr("Back to the image viewer"));
    openButton->setToolTip(tr("Open"));
    collageButton->setToolTip(tr("Collage"));
    folderViewButton->setToolTip(tr("Folder view"));
    settingsButton->setToolTip(tr("Settings"));
    for(ActionButton *button : {backButton, openButton, collageButton, folderViewButton, settingsButton}) {
        button->setAccessibleName("TopBarButton");
        button->setTriggerMode(TriggerMode::PressTrigger);
        button->setShortcutInToolTip(true);
    }

    buildSettingsMenu();

    layout.setContentsMargins(12, 0, 8, 0);
    layout.setSpacing(10);
    backButton->hide();
    layout.addWidget(backButton);
    layout.addWidget(&menuRow);
    layout.addWidget(&menuCollapsed);
    layout.addWidget(&separator, 0, Qt::AlignVCenter);
    layout.addWidget(&indexLabel);
    layout.addWidget(&nameLabel, 1);
    layout.addWidget(&perfLabel);
    layout.addWidget(&infoLabel);
    layout.addWidget(&zoomLabel);
    layout.addSpacing(6);
    layout.addWidget(openButton);
    layout.addWidget(collageButton);
    layout.addWidget(folderViewButton);
    layout.addWidget(settingsButton);
    setLayout(&layout);

    perfTimer.setInterval(1000);
    connect(&perfTimer, &QTimer::timeout, this, &TopBar::updatePerformance);
    connect(settings, &Settings::settingsChanged, this, &TopBar::applyPerformanceSetting);
    applyPerformanceSetting();
}

// General ... About, same order as the sidebar of the settings dialog
void TopBar::buildSettingsMenu() {
    const QStringList pages = {
        tr("General"), tr("View"), tr("Theme"), tr("Controls"), tr("Scripts"), tr("Advanced"), tr("About")
    };

    menuRowLayout.setContentsMargins(0, 0, 0, 0);
    menuRowLayout.setSpacing(2);
    menuRow.setLayout(&menuRowLayout);

    collapsedMenu = new QMenu(this);
    collapsedMenu->setAccessibleName("TopBarMenu");

    for(int i = 0; i < pages.count(); i++) {
        QToolButton *button = new QToolButton(&menuRow);
        button->setText(pages.at(i));
        button->setToolTip(tr("Settings: %1").arg(pages.at(i)));
        button->setAccessibleName("TopBarMenuButton");
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(MENU_ICON_SIZE, MENU_ICON_SIZE));
        button->setFixedHeight(BUTTON_SIZE - 4);
        connect(button, &QToolButton::clicked, this, [this, i]() { emit settingsPageRequested(i); });
        menuRowLayout.addWidget(button);
        menuButtons.append(button);

        QAction *action = collapsedMenu->addAction(pages.at(i));
        connect(action, &QAction::triggered, this, [this, i]() { emit settingsPageRequested(i); });
        menuActions.append(action);
    }
    updateMenuIcons();
    connect(settings, &Settings::settingsChanged, this, &TopBar::updateMenuIcons);

    menuCollapsed.setText(tr("Settings") + QString::fromUtf8(" \xE2\x96\xBE")); // small down arrow
    menuCollapsed.setToolTip(tr("Settings"));
    menuCollapsed.setAccessibleName("TopBarMenuButton");
    menuCollapsed.setAutoRaise(true);
    menuCollapsed.setFocusPolicy(Qt::NoFocus);
    menuCollapsed.setCursor(Qt::PointingHandCursor);
    menuCollapsed.setToolButtonStyle(Qt::ToolButtonTextOnly);
    menuCollapsed.setPopupMode(QToolButton::InstantPopup);
    menuCollapsed.setMenu(collapsedMenu);
    menuCollapsed.setFixedHeight(BUTTON_SIZE - 4);
    menuCollapsed.hide();

    separator.setAccessibleName("TopBarSeparator");
    separator.setFixedSize(1, SEPARATOR_HEIGHT);
}

// same icons as the settings dialog sidebar, tinted with the theme's icon color
void TopBar::updateMenuIcons() {
    static const char *names[] = { "general", "view", "appearance", "shortcuts", "terminal", "advanced", "about" };
    QColor color = settings->colorScheme().icons;
    if(color == menuIconColor && !menuButtons.isEmpty() && !menuButtons.first()->icon().isNull())
        return;
    menuIconColor = color;
    for(int i = 0; i < menuButtons.count() && i < 7; i++) {
        // the @2x layer (64px) is drawn at 32 logical px and scaled down to the icon size
        // (not arg(): "%1" followed by "32" would parse as placeholder %13)
        QString base = QString(":res/icons/common/settings/") + names[i] + "32";
        QPixmap pixmap(base + "@2x.png");
        if(pixmap.isNull())
            pixmap = QPixmap(base + ".png");
        if(pixmap.isNull())
            continue;
        ImageLib::recolor(pixmap, color);
        QIcon icon(pixmap);
        menuButtons[i]->setIcon(icon);
        menuActions[i]->setIcon(icon);
    }
    updateLayoutState(); // the row got wider / narrower
}

void TopBar::setInfo(QString position, QString fileName, QString info) {
    fullName = fileName;
    indexLabel.setText(position);
    infoLabel.setText(info.simplified());
    updateLayoutState();
}

void TopBar::setZoom(qreal scale) {
    zoomScale = scale;
    if(scale > 0.0)
        zoomLabel.setText(QString::number(qRound(scale * 100.0)) + "%");
    updateLayoutState();
}

void TopBar::setZoomAllowed(bool allowed) {
    if(zoomAllowed == allowed)
        return;
    zoomAllowed = allowed;
    updateLayoutState();
}

void TopBar::setBackVisible(bool visible) {
    backButton->setVisible(visible);
    updateLayoutState();
}

void TopBar::setBackToCollage(bool toCollage) {
    backButton->setAction(toCollage ? "openCollage" : "documentView");
    backButton->setToolTip(toCollage ? tr("Back to the collage") : tr("Back to the image viewer"));
}

//------------------------------------------------------------------------------
// CPU / RAM readout. The timer only runs while the bar is on screen and the option is on.
void TopBar::applyPerformanceSetting() {
    perfEnabled = settings->topBarPerformance();
    if(!perfEnabled)
        perfHasSample = false;
    updatePerformanceTimer();
    updateLayoutState();
}

void TopBar::updatePerformanceTimer() {
    bool run = perfEnabled && isVisible();
    if(run && !perfTimer.isActive()) {
        updatePerformance(); // primes the counters, shows the memory right away
        if(perfEnabled)      // unsupported platforms switch the option off
            perfTimer.start();
    } else if(!run && perfTimer.isActive()) {
        perfTimer.stop();
        // the next start must not average the cpu usage over the time we were hidden
        stats = ProcessStats();
        perfHasSample = false;
    }
}

void TopBar::updatePerformance() {
    if(!stats.sample()) {
        perfEnabled = false;
        perfHasSample = false;
        perfTimer.stop();
        updateLayoutState();
        return;
    }
    QString cpu = stats.hasCpu() ? QString::number(qRound(stats.cpuPercent())) + "%" : QString::fromUtf8("\xE2\x80\x93");
    perfLabel.setText(tr("CPU %1   RAM %2").arg(cpu, ProcessStats::memoryText(stats.memoryBytes())));
    if(!perfHasSample) {
        perfHasSample = true;
        updateLayoutState(); // the label becomes visible; its width is fixed, so later ticks need no relayout
    }
}

//------------------------------------------------------------------------------
// Narrow windows: drop the secondary info (CPU / RAM first), collapse the settings row
// into a dropdown, elide the file name. Buttons always stay fully visible.
void TopBar::updateLayoutState() {
    int spacing = layout.spacing();
    indexLabel.setVisible(width() >= HIDE_INDEX_WIDTH && !indexLabel.text().isEmpty());
    infoLabel.setVisible(width() >= HIDE_INFO_WIDTH && !infoLabel.text().isEmpty());
    zoomLabel.setVisible(width() >= HIDE_INDEX_WIDTH && zoomAllowed && zoomScale > 0.0);
    perfLabel.setVisible(perfEnabled && perfHasSample && width() >= HIDE_PERF_WIDTH);

    int buttonsWidth = 4 * BUTTON_SIZE + spacing * 4 + 6;
    int used = layout.contentsMargins().left() + layout.contentsMargins().right() + buttonsWidth;
    if(!backButton->isHidden())
        used += BUTTON_SIZE + spacing;
    if(indexLabel.isVisible())
        used += indexLabel.sizeHint().width() + spacing;
    if(infoLabel.isVisible())
        used += infoLabel.sizeHint().width() + spacing;
    if(zoomLabel.isVisible())
        used += zoomLabel.width() + spacing;
    if(perfLabel.isVisible())
        used += perfLabel.width() + spacing;

    // settings: the full row if the file name still keeps a decent width, otherwise the dropdown
    int separatorWidth = separator.width() + spacing;
    int fullWidth = menuRow.sizeHint().width() + spacing + separatorWidth;
    bool fullRow = (width() - used - fullWidth - spacing) >= MIN_NAME_WIDTH;
    menuRow.setVisible(fullRow);
    menuCollapsed.setVisible(!fullRow);
    used += fullRow ? fullWidth : (menuCollapsed.sizeHint().width() + spacing + separatorWidth);

    int available = qMax(0, width() - used - spacing);
    QString text = fullName.isEmpty() ? tr("No file opened.") : fullName;
    nameLabel.setText(nameLabel.fontMetrics().elidedText(text, Qt::ElideMiddle, available));
    nameLabel.setToolTip(text);
}

void TopBar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateLayoutState();
}

void TopBar::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    updatePerformanceTimer();
}

void TopBar::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    updatePerformanceTimer();
}

// do not let the wheel fall through to the image underneath
void TopBar::wheelEvent(QWheelEvent *event) {
    event->accept();
}

void TopBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
