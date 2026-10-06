#include "fullscreenstatsoverlay.h"
#include "settings.h"

FullscreenStatsOverlay::FullscreenStatsOverlay(FloatingWidgetContainer *parent) : OverlayWidget(parent) {
    setEnabled(false); // let the mouse through
    layout.setContentsMargins(10, 5, 10, 5);
    layout.addWidget(&label);
    setLayout(&layout);
    // fixed width (widest realistic value) so the box does not jitter
    label.setFixedWidth(label.fontMetrics().horizontalAdvance("CPU 100%   RAM 99999 MB") + 4);
    label.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setPosition(FloatingWidgetPosition::TOPRIGHT);
    setHorizontalMargin(0);
    setVerticalMargin(0);

    timer.setInterval(1000);
    connect(&timer, &QTimer::timeout, this, &FullscreenStatsOverlay::updateStats);
    connect(settings, &Settings::settingsChanged, this, &FullscreenStatsOverlay::applyState);
    if(parent)
        setContainerSize(parent->size());
    hide();
}

void FullscreenStatsOverlay::setRequested(bool value) {
    requested = value;
    applyState();
}

void FullscreenStatsOverlay::applyState() {
    if(requested && settings->topBarPerformance()) {
        if(!timer.isActive()) {
            updateStats(); // primes the counters, shows the memory right away
            timer.start();
        }
        if(!isVisible() && !label.text().isEmpty())
            show();
    } else {
        timer.stop();
        stats = ProcessStats(); // do not average the cpu usage over the time we were hidden
        label.clear();
        hide();
    }
}

void FullscreenStatsOverlay::updateStats() {
    if(!stats.sample()) { // unsupported platform
        timer.stop();
        hide();
        return;
    }
    QString cpu = stats.hasCpu() ? QString::number(qRound(stats.cpuPercent())) + "%" : QString::fromUtf8("\xE2\x80\x93");
    label.setText(tr("CPU %1   RAM %2").arg(cpu, ProcessStats::memoryText(stats.memoryBytes())));
    adjustSize();
    recalculateGeometry();
    if(requested && !isVisible())
        show();
}
