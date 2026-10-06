#include "fullscreeninfooverlayproxy.h"

FullscreenInfoOverlayProxy::FullscreenInfoOverlayProxy(FloatingWidgetContainer *parent)
    : container(parent),
      infoOverlay(nullptr),
      statsOverlay(nullptr)
{
}

FullscreenInfoOverlayProxy::~FullscreenInfoOverlayProxy() {
    if(infoOverlay)
        infoOverlay->deleteLater();
    if(statsOverlay)
        statsOverlay->deleteLater();
}

void FullscreenInfoOverlayProxy::show() {
    stateBuf.showImmediately = true;
    init();
}

void FullscreenInfoOverlayProxy::showWhenReady() {
    if(!infoOverlay)
        stateBuf.showImmediately = true;
    else {
        infoOverlay->show();
        statsOverlay->setRequested(true);
    }
}

void FullscreenInfoOverlayProxy::hide() {
    stateBuf.showImmediately = false;
    if(infoOverlay) {
        infoOverlay->hide();
        statsOverlay->setRequested(false);
    }
}

void FullscreenInfoOverlayProxy::setInfo(QString _position, QString _fileName, QString _info) {
    if(infoOverlay) {
        infoOverlay->setInfo(_position, _fileName, _info);
    } else {
        stateBuf.position = _position;
        stateBuf.fileName = _fileName;
        stateBuf.info = _info;
    }
}

void FullscreenInfoOverlayProxy::init() {
    if(infoOverlay)
        return;
    infoOverlay = new FullscreenInfoOverlay(container);
    statsOverlay = new FullscreenStatsOverlay(container);
    if(!stateBuf.fileName.isEmpty())
        setInfo(stateBuf.position, stateBuf.fileName, stateBuf.info);
    if(stateBuf.showImmediately) {
        infoOverlay->show();
        statsOverlay->setRequested(true);
    }
}
