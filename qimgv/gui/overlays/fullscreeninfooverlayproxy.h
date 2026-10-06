#pragma once

#include "gui/overlays/fullscreeninfooverlay.h"
#include "gui/overlays/fullscreenstatsoverlay.h"

struct InfoOverlayStateBuffer {
    QString position;
    QString fileName;
    QString info;
    bool showImmediately = false;
};

class FullscreenInfoOverlayProxy {
public:
    explicit FullscreenInfoOverlayProxy(FloatingWidgetContainer *parent = nullptr);
    ~FullscreenInfoOverlayProxy();
    void init();
    void show();
    void showWhenReady();
    void hide();
    void setInfo(QString position, QString fileName, QString info);

private:
    FloatingWidgetContainer *container;
    FullscreenInfoOverlay *infoOverlay;
    FullscreenStatsOverlay *statsOverlay; // top right, shares the visibility of the info overlay
    InfoOverlayStateBuffer stateBuf;
};
