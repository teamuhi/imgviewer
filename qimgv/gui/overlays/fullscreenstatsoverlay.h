#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include "gui/customwidgets/overlaywidget.h"
#include "utils/processstats.h"

// CPU / RAM readout in the top right corner of the fullscreen view.
// Only visible while requested (fullscreen + info bar) and while the top bar performance option is on;
// the sampling timer runs only while it is on screen.
class FullscreenStatsOverlay : public OverlayWidget {
    Q_OBJECT

public:
    explicit FullscreenStatsOverlay(FloatingWidgetContainer *parent = nullptr);
    void setRequested(bool requested);

private slots:
    void applyState();
    void updateStats();

private:
    QHBoxLayout layout;
    QLabel label;
    QTimer timer;
    ProcessStats stats;
    bool requested = false;
};
