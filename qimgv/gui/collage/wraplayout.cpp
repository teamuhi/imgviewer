#include "wraplayout.h"
#include <QWidget>

WrapLayout::WrapLayout(QWidget *parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent), mHSpace(hSpacing), mVSpace(vSpacing)
{
    setContentsMargins(margin, margin, margin, margin);
}

WrapLayout::~WrapLayout() {
    QLayoutItem *item;
    while((item = takeAt(0)))
        delete item;
}

void WrapLayout::addItem(QLayoutItem *item) {
    mItems.append(item);
}

int WrapLayout::count() const {
    return mItems.size();
}

QLayoutItem *WrapLayout::itemAt(int index) const {
    return mItems.value(index, nullptr);
}

QLayoutItem *WrapLayout::takeAt(int index) {
    if(index >= 0 && index < mItems.size())
        return mItems.takeAt(index);
    return nullptr;
}

Qt::Orientations WrapLayout::expandingDirections() const {
    return Qt::Orientations();
}

bool WrapLayout::hasHeightForWidth() const {
    return true;
}

int WrapLayout::heightForWidth(int width) const {
    return doLayout(QRect(0, 0, width, 0), true);
}

void WrapLayout::setGeometry(const QRect &rect) {
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize WrapLayout::sizeHint() const {
    return minimumSize();
}

QSize WrapLayout::minimumSize() const {
    QSize size;
    for(const QLayoutItem *item : mItems)
        size = size.expandedTo(item->minimumSize());
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    return size + QSize(left + right, top + bottom);
}

int WrapLayout::doLayout(const QRect &rect, bool testOnly) const {
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QRect area = rect.adjusted(left, top, -right, -bottom);
    int x = area.x();
    int y = area.y();
    int lineHeight = 0;
    int lineStart = 0; // index of the first item on the current line

    // places the items of the finished line [lineStart, end) centred on the line's vertical middle
    auto placeLine = [&](int end) {
        if(testOnly)
            return;
        for(int i = lineStart; i < end; i++) {
            QLayoutItem *item = mItems.at(i);
            QSize hint = item->sizeHint();
            item->setGeometry(QRect(QPoint(item->geometry().x(), y + (lineHeight - hint.height()) / 2), hint));
        }
    };

    for(int i = 0; i < mItems.size(); i++) {
        QLayoutItem *item = mItems.at(i);
        if(item->isEmpty())
            continue; // hidden widgets take no room
        QSize hint = item->sizeHint();
        int nextX = x + hint.width() + mHSpace;
        if(nextX - mHSpace > area.right() && lineHeight > 0) {
            placeLine(i);
            lineStart = i;
            x = area.x();
            y = y + lineHeight + mVSpace;
            nextX = x + hint.width() + mHSpace;
            lineHeight = 0;
        }
        if(!testOnly)
            item->setGeometry(QRect(QPoint(x, y), hint)); // x is final; y is fixed up in placeLine
        x = nextX;
        lineHeight = qMax(lineHeight, hint.height());
    }
    placeLine(mItems.size());
    return y + lineHeight - rect.y() + bottom;
}
