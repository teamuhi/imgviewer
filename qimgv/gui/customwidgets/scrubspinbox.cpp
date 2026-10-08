#include "scrubspinbox.h"
#include <QApplication>
#include <QGuiApplication>
#include <QLineEdit>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QStyle>
#include <QtMath>

namespace {
const int DRAG_THRESHOLD = 3;
// width that counts as the "label" part of the field when it has no prefix
const int MIN_ZONE = 14;

QPoint globalPoint(const QMouseEvent *e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->globalPosition().toPoint();
#else
    return e->globalPos();
#endif
}
}

ScrubController::ScrubController(QAbstractSpinBox *box) : QObject(box), mBox(box) {
    // the line edit receives the mouse; the box itself gets the arrow-button area
    mEdit = box->findChild<QLineEdit*>();
    if(mEdit) {
        mEdit->installEventFilter(this);
        mEdit->setMouseTracking(true);
    }
    box->setProperty("scrubbing", false);
}

double ScrubController::value() const {
    if(auto spin = qobject_cast<QSpinBox*>(mBox))
        return spin->value();
    if(auto spin = qobject_cast<QDoubleSpinBox*>(mBox))
        return spin->value();
    return 0.0;
}

void ScrubController::setValue(double value) {
    if(auto spin = qobject_cast<QSpinBox*>(mBox))
        spin->setValue(qRound(value));
    else if(auto spin = qobject_cast<QDoubleSpinBox*>(mBox))
        spin->setValue(value);
}

double ScrubController::step() const {
    if(auto spin = qobject_cast<QSpinBox*>(mBox))
        return qMax(1, spin->singleStep());
    if(auto spin = qobject_cast<QDoubleSpinBox*>(mBox))
        return spin->singleStep() > 0 ? spin->singleStep() : 1.0;
    return 1.0;
}

// the prefix ("W  ") always scrubs; an unfocused field scrubs anywhere
bool ScrubController::inScrubZone(const QPoint &pos) const {
    if(!mBox->hasFocus())
        return true;
    QString prefix;
    if(auto spin = qobject_cast<QSpinBox*>(mBox))
        prefix = spin->prefix();
    else if(auto spin = qobject_cast<QDoubleSpinBox*>(mBox))
        prefix = spin->prefix();
    int zone = prefix.isEmpty() ? MIN_ZONE : mBox->fontMetrics().horizontalAdvance(prefix) + 4;
    return pos.x() <= zone;
}

void ScrubController::updateHoverCursor(const QPoint &pos) {
    if(!mEdit || !mBox->isEnabled() || mBox->isReadOnly())
        return;
    mEdit->setCursor(inScrubZone(pos) ? Qt::SizeHorCursor : Qt::IBeamCursor);
}

void ScrubController::begin() {
    mScrubbing = true;
    mAccumulated = 0.0;
    mBox->setProperty("scrubbing", true);
    mBox->style()->unpolish(mBox);
    mBox->style()->polish(mBox);
    QApplication::setOverrideCursor(Qt::SizeHorCursor);
    qApp->installEventFilter(this); // Esc can arrive at whatever has the focus
}

void ScrubController::finish(bool cancel) {
    if(mScrubbing) {
        qApp->removeEventFilter(this);
        QApplication::restoreOverrideCursor();
        if(cancel)
            setValue(mStartValue);
        mBox->setProperty("scrubbing", false);
        mBox->style()->unpolish(mBox);
        mBox->style()->polish(mBox);
        emit mBox->editingFinished();
    }
    mScrubbing = false;
    mPending = false;
}

bool ScrubController::eventFilter(QObject *watched, QEvent *event) {
    // application-wide filter is only installed while scrubbing: catch Esc
    if(!mEdit || watched != mEdit.data()) {
        if(mScrubbing && event->type() == QEvent::KeyPress
           && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            finish(true);
            return true;
        }
        return false;
    }
    if(!mBox->isEnabled() || mBox->isReadOnly())
        return false;

    switch(event->type()) {
    case QEvent::MouseButtonPress: {
        auto *e = static_cast<QMouseEvent*>(event);
        if(e->button() != Qt::LeftButton || !inScrubZone(e->pos()))
            return false;
        mPending = true;
        mPressPos = mLastPos = globalPoint(e);
        mStartValue = value();
        return true; // decide on release / move whether this was a click or a drag
    }
    case QEvent::MouseMove: {
        auto *e = static_cast<QMouseEvent*>(event);
        if(!mPending) {
            if(e->buttons() == Qt::NoButton)
                updateHoverCursor(e->pos());
            return false;
        }
        QPoint global = globalPoint(e);
        if(!mScrubbing) {
            if(qAbs(global.x() - mPressPos.x()) < DRAG_THRESHOLD)
                return true;
            begin();
        }
        double multiplier = 1.0;
        if(e->modifiers() & Qt::ShiftModifier)
            multiplier = 10.0;
        else if(e->modifiers() & (Qt::AltModifier | Qt::ControlModifier))
            multiplier = 0.1;
        mAccumulated += (global.x() - mLastPos.x()) * step() * multiplier;
        mLastPos = global;
        setValue(mStartValue + mAccumulated);
        // endless drag: wrap the pointer at the screen edge (not possible on Wayland)
        QScreen *screen = QGuiApplication::screenAt(global);
        if(screen && !QGuiApplication::platformName().startsWith("wayland")) {
            QRect geom = screen->geometry();
            if(global.x() <= geom.left() + 1) {
                QCursor::setPos(geom.right() - 2, global.y());
                mLastPos = QPoint(geom.right() - 2, global.y());
            } else if(global.x() >= geom.right() - 1) {
                QCursor::setPos(geom.left() + 2, global.y());
                mLastPos = QPoint(geom.left() + 2, global.y());
            }
        }
        return true;
    }
    case QEvent::MouseButtonRelease: {
        auto *e = static_cast<QMouseEvent*>(event);
        if(e->button() != Qt::LeftButton || !mPending)
            return false;
        bool scrubbed = mScrubbing;
        finish(false);
        if(!scrubbed) {
            // plain click: type a number
            mBox->setFocus(Qt::MouseFocusReason);
            mBox->selectAll();
        }
        return true;
    }
    case QEvent::MouseButtonDblClick:
        // a double click while unfocused would select a word: just focus and select all
        mBox->setFocus(Qt::MouseFocusReason);
        mBox->selectAll();
        return true;
    default:
        return false;
    }
}

//------------------------------------------------------------------------------
ScrubSpinBox::ScrubSpinBox(QWidget *parent) : QSpinBox(parent) {
    setAccessibleName("ScrubSpinBox");
    setButtonSymbols(QAbstractSpinBox::NoButtons);
    setKeyboardTracking(false);
    new ScrubController(this);
}

ScrubDoubleSpinBox::ScrubDoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent) {
    setAccessibleName("ScrubSpinBox");
    setButtonSymbols(QAbstractSpinBox::NoButtons);
    setKeyboardTracking(false);
    new ScrubController(this);
}
