#include "iconwidget.h"
#include <QFile>

IconWidget::IconWidget(QWidget *parent)
    : QWidget(parent),
      hiResPixmap(false),
      pixmap(nullptr)
{
    dpr = this->devicePixelRatioF();
    color = settings->colorScheme().icons;
    accentColor = settings->colorScheme().accent;
    connect(settings, &Settings::settingsChanged, this, &IconWidget::onSettingsChanged);
}

IconWidget::~IconWidget() {
    if(pixmap)
        delete pixmap;
}

void IconWidget::onSettingsChanged() {
    if(colorMode == ICON_COLOR_THEME &&
       (color != settings->colorScheme().icons || accentColor != settings->colorScheme().accent))
    {
        color = settings->colorScheme().icons;
        accentColor = settings->colorScheme().accent;
        applyColor();
    }
}

void IconWidget::setIconPath(QString path) {
    if(iconPath == path)
        return;
    iconPath = path;
    loadIcon();
}

// Loads one icon layer, picking the @2x twin on hidpi screens.
QPixmap IconWidget::loadPixmap(QString path) {
    int dot = path.lastIndexOf('.');
    if(dpr >= (1.0 + 0.001) && dot > 0) {
        hiResPixmap = true;
        QPixmap result(path.left(dot) + "@2x" + path.mid(dot));
        pixmapDrawScale = (dpr >= (2.0 - 0.001)) ? dpr : 2.0;
        result.setDevicePixelRatio(pixmapDrawScale);
        return result;
    }
    hiResPixmap = false;
    pixmapDrawScale = dpr;
    return QPixmap(path);
}

void IconWidget::loadIcon() {
    if(pixmap) {
        delete pixmap;
        pixmap = nullptr;
    }
    baseSource = loadPixmap(iconPath);
    // optional accent layer: "name.png" -> "name_accent.png"
    accentSource = QPixmap();
    hasAccent = false;
    int dot = iconPath.lastIndexOf('.');
    if(dot > 0 && !baseSource.isNull()) {
        QString accentPath = iconPath.left(dot) + "_accent" + iconPath.mid(dot);
        if(QFile::exists(accentPath)) {
            QPixmap layer = loadPixmap(accentPath);
            if(!layer.isNull() && layer.size() == baseSource.size()) {
                accentSource = layer;
                hasAccent = true;
            }
        }
    }
    if(!baseSource.isNull()) {
        pixmap = new QPixmap(baseSource);
        applyColor();
    }
    update();
}

QSize IconWidget::minimumSizeHint() const {
    if(pixmap && !pixmap->isNull())
        return pixmap->size() / dpr;
    else
        return QWidget::minimumSizeHint();
}

void IconWidget::setIconOffset(int x, int y) {
    iconOffset.setX(x);
    iconOffset.setY(y);
    update();
}

void IconWidget::setColorMode(IconColorMode _mode) {
    if(colorMode != _mode && _mode == ICON_COLOR_SOURCE) {
        colorMode = _mode;
        // reload uncolored
        loadIcon();
    } else {
        colorMode = _mode;
        applyColor();
    }
}

void IconWidget::setColor(QColor _color) {
    colorMode = ICON_COLOR_CUSTOM;
    color = _color;
    applyColor();
}

void IconWidget::applyColor() {
    if(!pixmap || baseSource.isNull())
        return;
    // rebuild from the pristine layers so repeated theme changes never stack
    *pixmap = baseSource;
    if(colorMode == ICON_COLOR_SOURCE) {
        update();
        return;
    }
    ImageLib::recolor(*pixmap, color);
    if(hasAccent) {
        QPixmap layer = accentSource;
        // forced-color callers (e.g. white click zone arrows) get a flat icon
        ImageLib::recolor(layer, colorMode == ICON_COLOR_THEME ? accentColor : color);
        QPainter p(pixmap);
        p.drawPixmap(0, 0, layer);
    }
    update();
}

void IconWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter p(this);
    if(!this->isEnabled())
        p.setOpacity(0.5f);
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    if(pixmap) {
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QPointF pos;
        if(hiResPixmap) {
            pos = QPointF(width()  / 2 - pixmap->width()  / (2 * pixmapDrawScale),
                          height() / 2 - pixmap->height() / (2 * pixmapDrawScale));
        } else {
            pos = QPointF(width()  / 2 - pixmap->width()  / 2,
                          height() / 2 - pixmap->height() / 2);
        }
        p.drawPixmap(pos + iconOffset, *pixmap);
    }
}
