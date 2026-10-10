#include "slideshowcaption.h"
#include <QFileInfo>
#include <QDateTime>
#include <QApplication>
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

// text size + box padding follow the "Caption size" option. A widget stylesheet is used because the
// app stylesheet would override a plain setFont()
void SlideshowCaption::applyScale() {
    // on short windows the caption never grows past about a third of the height (2 lines ~ 60 px at 100 %)
    const int heightCap = qMax(75, containerSize().height() * 100 / (3 * 60));
    const int percent = qMin(settings->slideshowCaptionScale(), heightCap);
    if(percent == mAppliedScale)
        return;
    mAppliedScale = percent;
    const qreal factor = percent / 100.0;
    qreal basePt = QApplication::font().pointSizeF();
    if(basePt <= 0)
        basePt = 10.0; // pixel sized application font
    setStyleSheet(percent == 100 ? QString() : QString("SlideshowCaption QLabel { font-size: %1pt; }").arg(basePt * factor, 0, 'f', 1));
    nameLabel.ensurePolished();
    dateLabel.ensurePolished();
    layout.setContentsMargins(qRound(12 * factor), qRound(7 * factor), qRound(12 * factor), qRound(7 * factor));
    layout.setSpacing(qRound(2 * factor));
}

void SlideshowCaption::refresh() {
    applyScale(); // before the elide width is measured
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
