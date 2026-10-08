#include "rulers.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QActionGroup>
#include <QScrollBar>
#include <QFontMetrics>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QtMath>
#include <cmath>
#include "settings.h"

namespace {
const int BAND_SIZE = 17;        // one scale (px, cm or in); Mixed stacks two
const int MAX_GUIDES = 100;
const int GUIDE_GRAB = 4;        // px around a guide that grab it
const int MAX_TICKS = 3000;
const int LABEL_GAP = 6;
const double MIN_MAJOR_SPACING = 64.0; // viewport px between two labelled ticks
const double MIN_MINOR_SPACING = 4.0;

QColor withAlpha(QColor c, int alpha) {
    c.setAlpha(alpha);
    return c;
}

QString trimmedNumber(double v, int decimals) {
    QString s = QString::number(v, 'f', decimals);
    if(s.contains('.')) {
        while(s.endsWith('0'))
            s.chop(1);
        if(s.endsWith('.'))
            s.chop(1);
    }
    if(s == "-0")
        s = "0";
    return s;
}
}

//------------------------------------------------------------------------------
// RulerBar
//------------------------------------------------------------------------------
RulerBar::RulerBar(RulerController *controller, Kind kind, QWidget *parent)
    : QWidget(parent), mCtl(controller), mKind(kind) {
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::NoFocus);
    if(kind == Top)
        setCursor(Qt::SplitVCursor);
    else if(kind == Left)
        setCursor(Qt::SplitHCursor);
}

void RulerBar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    const ColorScheme &cs = settings->colorScheme();
    p.fillRect(rect(), cs.widget);

    QFont f = font();
    f.setPixelSize(9);
    p.setFont(f);

    if(mKind == Corner) {
        p.setPen(cs.widget_border);
        p.drawLine(0, height() - 1, width(), height() - 1);
        p.drawLine(width() - 1, 0, width() - 1, height());
        static const char *names[] = { "px", "cm", "in" };
        QString text;
        if(mCtl->unit() == RULER_MIXED)
            text = QString::fromLatin1("px/") + QString::fromLatin1(names[mCtl->mixedUnit() == RULER_IN ? 2 : 1]);
        else
            text = QString::fromLatin1(names[qBound(0, mCtl->unit(), 2)]);
        p.setPen(withAlpha(cs.text, 180));
        p.drawText(rect().adjusted(0, 0, -1, -1), Qt::AlignCenter, text);
        return;
    }

    const bool horizontal = (mKind == Top);
    const int len = horizontal ? width() : height();
    const int thick = horizontal ? height() : width();

    QTransform t;
    if(mCtl->transform(t)) {
        const qreal scale = horizontal ? t.m11() : t.m22();
        const qreal origin = horizontal ? t.dx() : t.dy();
        const int bands = (mCtl->unit() == RULER_MIXED) ? 2 : 1;
        const int bandSize = (thick - 1) / bands;
        for(int b = 0; b < bands; ++b) {
            int unit = (bands == 2) ? (b == 0 ? RULER_PX : mCtl->mixedUnit()) : mCtl->unit();
            int b0 = b * bandSize;
            drawBand(p, horizontal, len, scale, origin, unit, b0, b0 + bandSize);
            if(b > 0) {
                p.setPen(withAlpha(cs.widget_border, 160));
                if(horizontal)
                    p.drawLine(0, b0, len, b0);
                else
                    p.drawLine(b0, 0, b0, len);
            }
        }
        // cursor marker
        if(mCtl->hasCursor()) {
            int pos = horizontal ? mCtl->cursorPos().x() : mCtl->cursorPos().y();
            p.setPen(cs.accent);
            if(horizontal)
                p.drawLine(pos, 0, pos, thick - 1);
            else
                p.drawLine(0, pos, thick - 1, pos);
        }
    }
    // inner edge
    p.setPen(cs.widget_border);
    if(horizontal)
        p.drawLine(0, thick - 1, len, thick - 1);
    else
        p.drawLine(thick - 1, 0, thick - 1, len);
}

// one scale: ticks hang from the inner edge of the band [b0, b1), labels sit right after the major ticks
void RulerBar::drawBand(QPainter &p, bool horizontal, int len, qreal scale, qreal origin,
                        int unit, int b0, int b1) const {
    if(!(scale > 0) || !std::isfinite(scale) || !std::isfinite(origin))
        return;
    const ColorScheme &cs = settings->colorScheme();
    const qreal ppu = mCtl->pixelsPerUnit(unit);
    const qreal sp = scale * ppu; // viewport px per unit
    if(!(sp > 0) || !std::isfinite(sp))
        return;

    qreal raw = qBound(1e-7, MIN_MAJOR_SPACING / sp, 1e12);
    qreal e = std::floor(std::log10(raw));
    qreal base = std::pow(10.0, e);
    qreal m = raw / base;
    int mant = (m <= 1.0) ? 1 : (m <= 2.0) ? 2 : (m <= 5.0) ? 5 : 10;
    qreal step = mant * base;
    int sub = (mant == 2) ? 4 : (mant == 5) ? 5 : 10;
    qreal minorStep = step / sub;
    int decimals = qMax(0, -static_cast<int>(std::floor(std::log10(step) + 1e-9)));

    qreal vMin = (-8.0 - origin) / sp;
    qreal vMax = (len + 8.0 - origin) / sp;
    if(!std::isfinite(vMin) || !std::isfinite(vMax) || (vMax - vMin) / minorStep > MAX_TICKS)
        return;
    qint64 i0 = static_cast<qint64>(std::ceil(vMin / minorStep));
    qint64 i1 = static_cast<qint64>(std::floor(vMax / minorStep));
    const bool drawMinor = (minorStep * sp >= MIN_MINOR_SPACING);
    const int size = b1 - b0;

    QFontMetrics fm(p.font());
    int lastLabelEnd = -100000;
    for(qint64 i = i0; i <= i1; ++i) {
        int r = static_cast<int>(((i % sub) + sub) % sub);
        bool major = (r == 0);
        bool mid = !major && (sub % 2 == 0) && (r == sub / 2);
        if(!major && !drawMinor)
            continue;
        qreal v = i * minorStep;
        int pos = qRound(sp * v + origin);
        int tick = major ? size : mid ? qRound(size * 0.6) : qRound(size * 0.3);
        p.setPen(withAlpha(cs.text, major ? 200 : 120));
        if(horizontal)
            p.drawLine(pos, b1 - 1, pos, b1 - tick);
        else
            p.drawLine(b1 - 1, pos, b1 - tick, pos);
        if(!major)
            continue;

        QString text = trimmedNumber(v, decimals);
        int w = fm.horizontalAdvance(text);
        int start = pos + 3;
        if(start < lastLabelEnd || start > len)
            continue;
        lastLabelEnd = start + w + LABEL_GAP;
        p.setPen(withAlpha(cs.text, 190));
        if(horizontal) {
            p.drawText(start, b0 + (size - fm.height()) / 2 + fm.ascent(), text);
        } else {
            p.save();
            p.translate(b0 + (size - fm.height()) / 2 + fm.descent(), start);
            p.rotate(90);
            p.drawText(0, 0, text);
            p.restore();
        }
    }
}

void RulerBar::mousePressEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton && mKind != Corner) {
        mCtl->beginBarDrag(mKind == Top ? Qt::Horizontal : Qt::Vertical);
        event->accept();
        return;
    }
    // every other click stays on the ruler: it must not reach the viewer (its right-click menu, middle-click reset, side buttons)
    event->accept();
}

void RulerBar::mouseMoveEvent(QMouseEvent *event) {
    if(mCtl->isDragging()) {
        mCtl->updateDrag(mCtl->toViewport(event->pos(), this));
    }
    event->accept();
}

void RulerBar::mouseReleaseEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton && mCtl->isDragging()) {
        mCtl->endDrag(mCtl->toViewport(event->pos(), this));
        event->accept();
        return;
    }
    event->accept();
}

void RulerBar::mouseDoubleClickEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton) {
        // a top ruler makes horizontal guides, the left one vertical guides (the corner: either, horizontal first)
        mCtl->showSettingsDialog(mKind == Left ? Qt::Vertical : Qt::Horizontal);
        event->accept();
        return;
    }
    event->accept();
}

void RulerBar::wheelEvent(QWheelEvent *event) {
    event->accept(); // no image zoom / navigation from over a ruler
}

void RulerBar::contextMenuEvent(QContextMenuEvent *event) {
    mCtl->showMenu(event->globalPos());
    event->accept();
}

//------------------------------------------------------------------------------
// GuideOverlay
//------------------------------------------------------------------------------
GuideOverlay::GuideOverlay(RulerController *controller, QWidget *parent)
    : QWidget(parent), mCtl(controller) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFocusPolicy(Qt::NoFocus);
}

void GuideOverlay::paintEvent(QPaintEvent *) {
    QTransform t;
    if(!mCtl->transform(t))
        return;
    QPainter p(this);
    const ColorScheme &cs = settings->colorScheme();
    const QVector<RulerGuide> &guides = mCtl->guides();
    for(int i = 0; i < guides.size(); ++i) {
        const RulerGuide &g = guides[i];
        const bool horizontal = (g.orientation == Qt::Horizontal);
        qreal pos = horizontal ? t.m22() * g.pos + t.dy() : t.m11() * g.pos + t.dx();
        if(!std::isfinite(pos) || pos < -1 || pos > (horizontal ? height() : width()) + 1)
            continue;
        int c = qRound(pos);
        QPen halo(withAlpha(cs.accent, 70), 3);
        QPen line(cs.accent, 1);
        if(mCtl->isDragging() && mCtl->dragIndex() == i)
            line.setStyle(Qt::DashLine);
        if(horizontal) {
            p.setPen(halo);
            p.drawLine(0, c, width(), c);
            p.setPen(line);
            p.drawLine(0, c, width(), c);
        } else {
            p.setPen(halo);
            p.drawLine(c, 0, c, height());
            p.setPen(line);
            p.drawLine(c, 0, c, height());
        }
    }

    // value of the guide being dragged
    int di = mCtl->dragIndex();
    if(mCtl->isDragging() && di >= 0 && di < guides.size() && mCtl->hasCursor()) {
        const RulerGuide &g = guides[di];
        QString text = mCtl->formatDoc(g.pos);
        QFont f = font();
        f.setPixelSize(11);
        p.setFont(f);
        QFontMetrics fm(f);
        QSize box(fm.horizontalAdvance(text) + 12, fm.height() + 6);
        QPoint at = mCtl->cursorPos() + QPoint(14, 14);
        at.setX(qBound(2, at.x(), qMax(2, width() - box.width() - 2)));
        at.setY(qBound(2, at.y(), qMax(2, height() - box.height() - 2)));
        QRect r(at, box);
        p.setPen(cs.widget_border);
        p.setBrush(withAlpha(cs.overlay, 230));
        p.drawRoundedRect(r, 4, 4);
        p.setPen(cs.overlay_text);
        p.drawText(r, Qt::AlignCenter, text);
    }
}

//------------------------------------------------------------------------------
// RulerController
//------------------------------------------------------------------------------
RulerController::RulerController(const RulerHost &host, const QString &guidesKey, QObject *parent)
    : QObject(parent), mHost(host), mKey(guidesKey) {
    QAbstractScrollArea *view = mHost.view;
    mTop    = new RulerBar(this, RulerBar::Top, view);
    mLeft   = new RulerBar(this, RulerBar::Left, view);
    mCorner = new RulerBar(this, RulerBar::Corner, view);
    mOverlay = new GuideOverlay(this, view);
    mTop->hide();
    mLeft->hide();
    mCorner->hide();
    mOverlay->hide();

    view->viewport()->setMouseTracking(true);

    // pan / zoom move the content under the rulers: repaint with the same frame as the viewport
    auto onScroll = [this]() { refresh(); };
    connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, this, onScroll);
    connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this, onScroll);
    connect(view->horizontalScrollBar(), &QScrollBar::rangeChanged, this, onScroll);
    connect(view->verticalScrollBar(), &QScrollBar::rangeChanged, this, onScroll);

    connect(settings, &Settings::settingsChanged, this, &RulerController::readSettings);
    readSettings();
}

void RulerController::readSettings() {
    mEnabled = settings->rulersEnabled();
    mUnit = settings->rulerUnit();
    mMixedUnit = settings->rulerMixedUnit();
    mFallbackDpi = settings->rulerDpi();
    mUseImageDpi = settings->rulerUseImageDpi();
    mSnap = settings->rulerSnap();
    mSnapStep = settings->rulerSnapStep();
    mThickness = (mUnit == RULER_MIXED) ? 2 * BAND_SIZE + 1 : BAND_SIZE + 1;
    mActive = mEnabled && !mSuppressed;

    // our filter has to run before the owner's (click zones etc.): re-installing moves it to the front
    QWidget *viewport = mHost.view->viewport();
    viewport->removeEventFilter(this);
    viewport->installEventFilter(this);

    if(!mDrag.active)
        loadGuides();

    int margin = mActive ? mThickness : 0;
    if(margin != mAppliedMargin) {
        mAppliedMargin = margin;
        if(mHost.setMargins)
            mHost.setMargins(margin, margin);
        emit marginChanged();
    }
    if(!mActive) {
        mDrag = Drag();
        clearHoverCursor();
        mCursorVp = QPoint(-1, -1);
    }
    layoutChildren();
}

void RulerController::setImageDpi(qreal dpi) {
    dpi = (std::isfinite(dpi) && dpi >= 1.0 && dpi <= 100000.0) ? dpi : 0.0;
    if(qFuzzyCompare(1.0 + mImageDpi, 1.0 + dpi))
        return;
    mImageDpi = dpi;
    refresh();
}

void RulerController::setSuppressed(bool suppressed) {
    if(mSuppressed == suppressed)
        return;
    mSuppressed = suppressed;
    readSettings();
}

void RulerController::clearGuides() {
    mGuides.clear();
    mDrag = Drag();
    saveGuides();
    refresh();
}

qreal RulerController::dpi() const {
    qreal d = (mUseImageDpi && mImageDpi > 0) ? mImageDpi : mFallbackDpi;
    return qBound<qreal>(1.0, d, 100000.0);
}

qreal RulerController::pixelsPerUnit(int unit) const {
    switch(unit) {
        case RULER_CM: return dpi() / 2.54;
        case RULER_IN: return dpi();
        default:       return 1.0;
    }
}

bool RulerController::transform(QTransform &t) const {
    if(!mActive || !mHost.docToViewport || !mHost.docToViewport(t))
        return false;
    return std::isfinite(t.m11()) && std::isfinite(t.m22()) && std::isfinite(t.dx()) && std::isfinite(t.dy())
        && t.m11() > 1e-9 && t.m22() > 1e-9;
}

QPoint RulerController::toViewport(const QPoint &local, const QWidget *from) const {
    return mHost.view->viewport()->mapFromGlobal(from->mapToGlobal(local));
}

QRect RulerController::viewportRect() const {
    return mHost.view->viewport()->rect();
}

QString RulerController::formatDoc(double docPixels) const {
    QString px = trimmedNumber(docPixels, 1) + " px";
    QString cm = trimmedNumber(docPixels * 2.54 / dpi(), 2) + " cm";
    QString in = trimmedNumber(docPixels / dpi(), 3) + " in";
    switch(mUnit) {
        case RULER_CM: return cm;
        case RULER_IN: return in;
        case RULER_MIXED: return px + " " + QChar(0x00B7) + " " + (mMixedUnit == RULER_IN ? in : cm);
        default: return px;
    }
}

void RulerController::refresh() {
    if(!mActive)
        return;
    mTop->update();
    mLeft->update();
    mCorner->update();
    mOverlay->update();
}

void RulerController::layoutChildren() {
    if(!mActive) {
        mTop->hide();
        mLeft->hide();
        mCorner->hide();
        mOverlay->hide();
        return;
    }
    const QRect vg = mHost.view->viewport()->geometry();
    const int t = mThickness;
    mTop->setGeometry(vg.left(), vg.top() - t, vg.width(), t);
    mLeft->setGeometry(vg.left() - t, vg.top(), t, vg.height());
    mCorner->setGeometry(vg.left() - t, vg.top() - t, t, t);
    mOverlay->setGeometry(vg);
    mTop->show();
    mLeft->show();
    mCorner->show();
    mOverlay->show();
    mOverlay->raise();
    refresh();
}

//------------------------------------------------------------------------------
// guides
//------------------------------------------------------------------------------
void RulerController::loadGuides() {
    mGuides.clear();
    const QStringList list = settings->rulerGuides(mKey);
    for(const QString &entry : list) {
        if(mGuides.size() >= MAX_GUIDES)
            break;
        if(entry.size() < 3 || entry[1] != ':')
            continue;
        bool ok = false;
        double pos = entry.mid(2).toDouble(&ok);
        if(!ok || !std::isfinite(pos) || std::abs(pos) > 1e7)
            continue;
        if(entry[0] == 'h')
            mGuides.append({ Qt::Horizontal, pos });
        else if(entry[0] == 'v')
            mGuides.append({ Qt::Vertical, pos });
    }
}

void RulerController::saveGuides() {
    QStringList list;
    for(const RulerGuide &g : mGuides)
        list << QString("%1:%2").arg(g.orientation == Qt::Horizontal ? 'h' : 'v').arg(g.pos, 0, 'f', 2);
    settings->setRulerGuides(mKey, list);
}

int RulerController::guideAt(const QPoint &vp) const {
    QTransform t;
    if(!transform(t))
        return -1;
    int best = -1;
    double bestDist = GUIDE_GRAB + 1;
    for(int i = 0; i < mGuides.size(); ++i) {
        const RulerGuide &g = mGuides[i];
        const bool horizontal = (g.orientation == Qt::Horizontal);
        double pos = horizontal ? t.m22() * g.pos + t.dy() : t.m11() * g.pos + t.dx();
        double dist = std::abs(pos - (horizontal ? vp.y() : vp.x()));
        if(dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

void RulerController::setHoverCursor(Qt::Orientation orientation) {
    QWidget *viewport = mHost.view->viewport();
    if(!mHoverCursorSet) {
        mHadCursor = viewport->testAttribute(Qt::WA_SetCursor);
        mSavedCursor = viewport->cursor();
        mHoverCursorSet = true;
    }
    viewport->setCursor(orientation == Qt::Horizontal ? Qt::SplitVCursor : Qt::SplitHCursor);
}

void RulerController::clearHoverCursor() {
    if(!mHoverCursorSet)
        return;
    mHoverCursorSet = false;
    QWidget *viewport = mHost.view->viewport();
    if(mHadCursor)
        viewport->setCursor(mSavedCursor);
    else
        viewport->unsetCursor();
}

void RulerController::startDrag(int index, Qt::Orientation orientation) {
    mDrag.active = true;
    mDrag.index = index;
    mDrag.orientation = orientation;
    setHoverCursor(orientation);
}

void RulerController::beginBarDrag(Qt::Orientation orientation) {
    if(!mActive || mGuides.size() >= MAX_GUIDES)
        return;
    startDrag(-1, orientation);
}

void RulerController::updateDrag(const QPoint &vp) {
    if(!mDrag.active)
        return;
    mCursorVp = vp;
    QTransform t;
    if(transform(t)) {
        const bool horizontal = (mDrag.orientation == Qt::Horizontal);
        const double s = horizontal ? t.m22() : t.m11();
        const double o = horizontal ? t.dy() : t.dx();
        double doc = ((horizontal ? vp.y() : vp.x()) - o) / s;
        if(mSnap && mSnapStep > 0)
            doc = std::round(doc / mSnapStep) * mSnapStep; // multiples of the chosen step (even numbers, 5, 10, ...)
        else // whole pixels when zoomed in enough to tell them apart
            doc = (s >= 1.0) ? std::round(doc) : std::round(doc * 10.0) / 10.0;
        if(std::isfinite(doc)) {
            if(mDrag.index < 0) {
                if(viewportRect().contains(vp) && mGuides.size() < MAX_GUIDES) {
                    mGuides.append({ mDrag.orientation, doc });
                    mDrag.index = mGuides.size() - 1;
                }
            } else if(mDrag.index < mGuides.size()) {
                mGuides[mDrag.index].pos = doc;
            }
        }
    }
    refresh();
}

void RulerController::endDrag(const QPoint &vp) {
    if(!mDrag.active)
        return;
    // dropped back on a ruler (or anywhere outside the canvas): the guide is removed
    if(mDrag.index >= 0 && mDrag.index < mGuides.size() && !viewportRect().contains(vp))
        mGuides.remove(mDrag.index);
    mDrag = Drag();
    saveGuides();
    clearHoverCursor();
    refresh();
}

//------------------------------------------------------------------------------
// context menu of the rulers
//------------------------------------------------------------------------------
void RulerController::showMenu(const QPoint &globalPos) {
    QMenu menu(mHost.view);
    QActionGroup *group = new QActionGroup(&menu);
    struct Entry { QString text; int unit; };
    const Entry entries[] = {
        { tr("Pixels"), RULER_PX }, { tr("Centimeters"), RULER_CM },
        { tr("Inches"), RULER_IN }, { tr("Mixed (px + cm / in)"), RULER_MIXED }
    };
    for(const Entry &e : entries) {
        QAction *a = menu.addAction(e.text);
        a->setCheckable(true);
        a->setChecked(mUnit == e.unit);
        group->addAction(a);
        int unit = e.unit;
        connect(a, &QAction::triggered, this, [unit]() {
            settings->setRulerUnit(unit);
            settings->sendChangeNotification();
        });
    }
    menu.addSeparator();
    QAction *dialog = menu.addAction(tr("Ruler settings..."));
    connect(dialog, &QAction::triggered, this, [this]() { showSettingsDialog(Qt::Horizontal); });
    QAction *clear = menu.addAction(tr("Clear guides"));
    clear->setEnabled(!mGuides.isEmpty());
    connect(clear, &QAction::triggered, this, &RulerController::clearGuides);
    QAction *hide = menu.addAction(tr("Hide rulers"));
    connect(hide, &QAction::triggered, this, []() {
        settings->setRulersEnabled(false);
        settings->sendChangeNotification();
    });
    menu.exec(globalPos);
}

//------------------------------------------------------------------------------
// "Ruler settings" window (double click on a ruler)
//------------------------------------------------------------------------------
bool RulerController::addGuide(Qt::Orientation orientation, double docPixels) {
    if(!std::isfinite(docPixels) || std::abs(docPixels) > 1e7 || mGuides.size() >= MAX_GUIDES)
        return false;
    mGuides.append({ orientation, docPixels });
    saveGuides();
    refresh();
    return true;
}

void RulerController::showSettingsDialog(Qt::Orientation orientation) {
    if(!mActive)
        return;
    if(mDialog) {
        if(auto *direction = mDialog->findChild<QComboBox*>("rulerDirection"))
            direction->setCurrentIndex(orientation == Qt::Horizontal ? 0 : 1);
        mDialog->show();
        mDialog->raise();
        mDialog->activateWindow();
        return;
    }

    auto *dlg = new QDialog(mHost.view->window(), Qt::Tool | Qt::WindowCloseButtonHint);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Ruler settings"));
    mDialog = dlg;
    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(8);

    auto title = [dlg](const QString &text) {
        auto *label = new QLabel(text, dlg);
        QFont f = label->font();
        f.setBold(true);
        label->setFont(f);
        return label;
    };

    // --- snapping (top: it changes how every drag behaves) ---
    root->addWidget(title(tr("Snap")));
    auto *snapRow = new QHBoxLayout();
    snapRow->setSpacing(6);
    auto *snapCheck = new QCheckBox(tr("Snap guides to multiples of"), dlg);
    snapCheck->setChecked(mSnap);
    auto *snapStep = new QComboBox(dlg);
    snapStep->setEditable(true);
    snapStep->setValidator(new QIntValidator(1, 10000, snapStep));
    snapStep->addItems({ "1", "2", "5", "10", "20", "25", "50", "100" });
    snapStep->setCurrentText(QString::number(mSnapStep));
    snapStep->setMinimumWidth(70);
    snapStep->setToolTip(tr("1 = whole pixels, 2 = even numbers, 5, 10, ... (in document pixels)"));
    snapStep->setEnabled(mSnap);
    snapRow->addWidget(snapCheck);
    snapRow->addWidget(snapStep);
    snapRow->addWidget(new QLabel(tr("px"), dlg));
    snapRow->addStretch(1);
    root->addLayout(snapRow);

    auto applySnap = [snapCheck, snapStep]() {
        bool ok = false;
        int step = snapStep->currentText().toInt(&ok);
        settings->setRulerSnap(snapCheck->isChecked());
        if(ok && step >= 1)
            settings->setRulerSnapStep(step);
        settings->sendChangeNotification();
    };
    connect(snapCheck, &QCheckBox::toggled, dlg, [snapStep, applySnap](bool on) {
        snapStep->setEnabled(on);
        applySnap();
    });
    connect(snapStep, &QComboBox::currentTextChanged, dlg, [applySnap](const QString &) { applySnap(); });

    auto *line = new QFrame(dlg);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setEnabled(false); // dimmed by the style
    root->addSpacing(2);
    root->addWidget(line);
    root->addSpacing(2);

    // --- add a guide at an exact position ---
    root->addWidget(title(tr("Add guide")));
    auto *addRow = new QHBoxLayout();
    addRow->setSpacing(6);
    auto *direction = new QComboBox(dlg);
    direction->setObjectName("rulerDirection");
    direction->addItems({ tr("Horizontal"), tr("Vertical") });
    direction->setCurrentIndex(orientation == Qt::Horizontal ? 0 : 1);
    auto *value = new QDoubleSpinBox(dlg);
    value->setDecimals(2);
    value->setRange(-1000000.0, 1000000.0);
    value->setValue(0.0);
    value->setMinimumWidth(90);
    value->setKeyboardTracking(false);
    auto *unit = new QComboBox(dlg);
    unit->addItems({ tr("px"), tr("%"), tr("cm"), tr("in") });
    addRow->addWidget(direction);
    addRow->addWidget(value, 1);
    addRow->addWidget(unit);
    root->addLayout(addRow);

    auto *hint = new QLabel(dlg);
    hint->setWordWrap(true);
    hint->setAccessibleName("SettingsNote");
    root->addWidget(hint);
    auto updateHint = [direction, unit, hint]() {
        QString text;
        if(unit->currentIndex() == 1)
            text = QObject::tr("Percent of the %1 (0% = top / left edge, 100% = bottom / right edge).")
                       .arg(direction->currentIndex() == 0 ? QObject::tr("height") : QObject::tr("width"));
        else
            text = QObject::tr("Distance from the top / left edge (the 0 of the rulers).");
        hint->setText(text);
    };
    connect(direction, qOverload<int>(&QComboBox::currentIndexChanged), dlg, updateHint);
    connect(unit, qOverload<int>(&QComboBox::currentIndexChanged), dlg, updateHint);
    updateHint();

    auto *status = new QLabel(dlg);
    status->setWordWrap(true);
    status->setAccessibleName("SettingsNote");
    root->addWidget(status);
    root->addStretch(1);

    // --- actions: big buttons along the bottom ---
    auto *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);
    auto *addButton = new QPushButton(tr("Add guide"), dlg);
    addButton->setDefault(true);
    auto *clearButton = new QPushButton(tr("Clear guides"), dlg);
    for(QPushButton *b : { addButton, clearButton }) {
        b->setMinimumHeight(38);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        buttonRow->addWidget(b, 1);
    }
    root->addSpacing(4);
    root->addLayout(buttonRow);

    connect(addButton, &QPushButton::clicked, dlg, [this, direction, value, unit, status]() {
        const Qt::Orientation o = direction->currentIndex() == 0 ? Qt::Horizontal : Qt::Vertical;
        double v = value->value();
        double doc = v;
        switch(unit->currentIndex()) {
            case 1: {
                QSizeF size = mHost.docSize ? mHost.docSize() : QSizeF();
                double extent = (o == Qt::Horizontal) ? size.height() : size.width();
                if(extent <= 0) {
                    status->setText(tr("Nothing to measure yet."));
                    return;
                }
                doc = v / 100.0 * extent;
                break;
            }
            case 2: doc = v * pixelsPerUnit(RULER_CM); break;
            case 3: doc = v * pixelsPerUnit(RULER_IN); break;
            default: break;
        }
        if(addGuide(o, doc))
            status->setText(tr("Added at %1.").arg(formatDoc(doc)));
        else
            status->setText(tr("Could not add the guide (limit: %1).").arg(MAX_GUIDES));
    });
    connect(clearButton, &QPushButton::clicked, dlg, [this, status]() {
        clearGuides();
        status->setText(tr("Guides cleared."));
    });

    dlg->setMinimumWidth(360);
    dlg->adjustSize();
    // beside the view's top-left corner, inside the window
    QWidget *window = mHost.view->window();
    QPoint at = mHost.view->mapToGlobal(QPoint(mThickness + 12, mThickness + 12));
    QRect bounds = window->frameGeometry();
    at.setX(qBound(bounds.left(), at.x(), qMax(bounds.left(), bounds.right() - dlg->width())));
    at.setY(qBound(bounds.top(), at.y(), qMax(bounds.top(), bounds.bottom() - dlg->height())));
    dlg->move(at);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

//------------------------------------------------------------------------------
// viewport events: guide hover / drag, cursor marker, repaint on change
//------------------------------------------------------------------------------
void RulerController::setCursorMarker(const QPoint &pos) {
    if(mCursorVp == pos)
        return;
    mCursorVp = pos;
    mTop->update();
    mLeft->update();
}

bool RulerController::eventFilter(QObject *watched, QEvent *event) {
    if(watched != mHost.view->viewport())
        return false;
    switch(event->type()) {
        case QEvent::Resize:
        case QEvent::Move:
            if(mActive)
                layoutChildren();
            return false;
        case QEvent::Paint: {
            // pan / zoom normally arrive through the scroll bars; this catches the rest (fit mode, new image)
            if(!mActive)
                return false;
            QTransform t;
            bool valid = transform(t);
            if(valid != mLastValid || (valid && t != mLastTransform)) {
                mLastValid = valid;
                mLastTransform = t;
                refresh();
            }
            return false;
        }
        case QEvent::Leave:
            if(mActive && !mDrag.active)
                setCursorMarker(QPoint(-1, -1));
            return false;
        case QEvent::MouseMove: {
            if(!mActive)
                return false;
            auto *e = static_cast<QMouseEvent*>(event);
            if(mDrag.active) {
                updateDrag(e->pos());
                setCursorMarker(e->pos());
                return true;
            }
            setCursorMarker(e->pos());
            if(e->buttons() == Qt::NoButton) {
                int idx = guideAt(e->pos());
                if(idx >= 0)
                    setHoverCursor(mGuides[idx].orientation);
                else
                    clearHoverCursor();
            }
            return false;
        }
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick: {
            if(!mActive)
                return false;
            auto *e = static_cast<QMouseEvent*>(event);
            if(e->button() != Qt::LeftButton)
                return false;
            int idx = guideAt(e->pos());
            if(idx < 0)
                return false;
            startDrag(idx, mGuides[idx].orientation);
            setCursorMarker(e->pos());
            refresh();
            return true; // the viewer must not start a pan / tile drag
        }
        case QEvent::MouseButtonRelease: {
            if(!mDrag.active)
                return false;
            auto *e = static_cast<QMouseEvent*>(event);
            if(e->button() != Qt::LeftButton)
                return false;
            endDrag(e->pos());
            return true;
        }
        default:
            return false;
    }
}
