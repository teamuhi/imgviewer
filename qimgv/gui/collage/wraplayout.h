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

private:
    QList<QLayoutItem*> mItems;
    int mHSpace, mVSpace;

    int doLayout(const QRect &rect, bool testOnly) const;
};
