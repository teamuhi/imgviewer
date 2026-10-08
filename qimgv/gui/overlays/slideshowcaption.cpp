#include "slideshowcaption.h"
#include <QFileInfo>
#include <QDateTime>
#include "settings.h"

SlideshowCaption::SlideshowCaption(FloatingWidgetContainer *parent) : OverlayWidget(parent) {
    setAccessibleName("SlideshowCaption");
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    layout.setContentsMargins(12, 7, 12, 7);
    layout.setSpacing(2);
    nameLabel.setAccessibleName("SlideshowCaptionName");
    dateLabel.setAccessibleName("SlideshowCaptionDate");
    layout.addWidget(&nameLabel);
    layout.addWidget(&dateLabel);
    setLayout(&layout);
    setPosition(FloatingWidgetPosition::BOTTOMLEFT);
    setHorizontalMargin(16);
    setVerticalMargin(16);
    if(parent)
        setContainerSize(parent->size());
    hide();
}

void SlideshowCaption::setFile(const QString &filePath) {
    mPath = filePath;
    refresh();
}

void SlideshowCaption::refresh() {
    const bool showName = settings->slideshowShowName();
    const bool showDate = settings->slideshowShowDate();
    QFileInfo info(mPath);
    if(mPath.isEmpty() || (!showName && !showDate)) {
        hide();
        return;
    }
    // keep long names inside the window
    const int maxWidth = qMax(120, containerSize().width() / 2);
    nameLabel.setText(nameLabel.fontMetrics().elidedText(info.fileName(), Qt::ElideMiddle, maxWidth));
    dateLabel.setText(info.exists() ? locale().toString(info.lastModified(), QLocale::LongFormat) : QString());
    // (isVisible() of the labels is false while this widget is hidden: decide with plain flags)
    const bool nameShown = showName;
    const bool dateShown = showDate && !dateLabel.text().isEmpty();
    nameLabel.setVisible(nameShown);
    dateLabel.setVisible(dateShown);
    if(!nameShown && !dateShown) {
        hide();
        return;
    }
    show();
    layout.activate();
    adjustSize();
    recalculateGeometry();
}
