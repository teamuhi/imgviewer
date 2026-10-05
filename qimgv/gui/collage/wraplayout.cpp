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

    for(QLayoutItem *item : mItems) {
        QSize hint = item->sizeHint();
        int nextX = x + hint.width() + mHSpace;
        if(nextX - mHSpace > area.right() && lineHeight > 0) {
            x = area.x();
            y = y + lineHeight + mVSpace;
            nextX = x + hint.width() + mHSpace;
            lineHeight = 0;
        }
        if(!testOnly)
            item->setGeometry(QRect(QPoint(x, y), hint));
        x = nextX;
        lineHeight = qMax(lineHeight, hint.height());
    }
    return y + lineHeight - rect.y() + bottom;
}
