#pragma once

#include <QWidget>
#include <QLabel>
#include <QFrame>
#include <QToolButton>
#include <QMenu>
#include <QTimer>
#include <QHBoxLayout>
#include <QList>
#include <QPainter>
#include <QStyleOption>
#include <QWheelEvent>
#include <QFontMetrics>
#include "gui/customwidgets/actionbutton.h"
#include "utils/processstats.h"

// Slim always-visible toolbar: settings menu + file info on the left, quick actions on the right.
// Buttons trigger regular actions, so they follow the shortcut / action wiring.
class TopBar : public QWidget {
    Q_OBJECT
public:
    explicit TopBar(QWidget *parent = nullptr);

signals:
    // a settings category was picked (0 = General ... 6 = About, same order as the settings dialog)
    void settingsPageRequested(int page);

public slots:
    void setInfo(QString position, QString fileName, QString info);
    // shown while a view other than the image viewer is active
    void setBackVisible(bool visible);
    // back button returns to the collage (image opened from it) instead of the image viewer
    void setBackToCollage(bool toCollage);
    void setSlideshowAllowed(bool allowed);
    // magnification of the shown image (1.0 = 100%); <= 0 hides the counter
    void setZoom(qreal scale);
    // the counter only makes sense in the image viewer
    void setZoomAllowed(bool allowed);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void applyPerformanceSetting();
    void updatePerformance();
    void updateMenuIcons();

private:
    QHBoxLayout layout;
    QLabel indexLabel, nameLabel, infoLabel, zoomLabel, perfLabel;
    ActionButton *backButton, *openButton, *collageButton, *slideshowButton, *folderViewButton, *settingsButton;
    // settings categories: full row of text buttons, or one dropdown when the bar gets narrow
    QWidget menuRow;
    QHBoxLayout menuRowLayout;
    QToolButton menuCollapsed;
    QMenu *collapsedMenu;
    QList<QToolButton*> menuButtons;
    QList<QAction*> menuActions;
    QColor menuIconColor;
    QFrame separator;
    QString fullName;
    qreal zoomScale = 0.0;
    bool zoomAllowed = true;

    // CPU / RAM of this process (setting "topBarPerformance")
    QTimer perfTimer;
    ProcessStats stats;
    bool perfEnabled = false;
    bool perfHasSample = false;

    void buildSettingsMenu();
    void updatePerformanceTimer();
    void updateLayoutState();
};
