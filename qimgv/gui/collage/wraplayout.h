#pragma once

#include <QLayout>
#include <QList>
#include <QRect>
#include <QSize>

// Left-to-right layout that wraps onto new lines when the width runs out
// (adapted from Qt's flow layout example). Keeps toolbars usable on narrow windows.
class WrapLayout : public QLayout {
public:
    explicit WrapLayout(QWidget *parent = nullptr, int margin = 0, int hSpacing = 6, int vSpacing = 6);
    ~WrapLayout() override;

    void addItem(QLayoutItem *item) override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    // pinned to the right end of the first line, vertically centred; the other items wrap in the
    // width left of it. The widget must already be in the layout (addWidget).
    void setTrailingWidget(QWidget *widget);

private:
    QList<QLayoutItem*> mItems;
    QLayoutItem *mTrailing = nullptr;
    int mHSpace, mVSpace;

    int doLayout(const QRect &rect, bool testOnly) const;
};
