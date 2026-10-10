#pragma once

#include <QLabel>
#include <QVBoxLayout>
#include "gui/customwidgets/overlaywidget.h"

// File name and / or date of the current slide, bottom left over the image. Mouse transparent.
class SlideshowCaption : public OverlayWidget {
    Q_OBJECT
public:
    explicit SlideshowCaption(FloatingWidgetContainer *parent = nullptr);
    // shows what the slideshow options ask for; hides itself when there is nothing to show
    void setFile(const QString &filePath);
    void refresh();

private:
    QVBoxLayout layout;
    QLabel nameLabel, dateLabel;
    QString mPath;
    int mAppliedScale = 100;

    void applyScale();
};
