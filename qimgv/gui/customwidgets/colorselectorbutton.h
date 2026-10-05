#ifndef COLORSELECTORBUTTON_H
#define COLORSELECTORBUTTON_H

#include <QPainter>
#include "gui/dialogs/colorpickerdialog.h"
#include "gui/customwidgets/clickablelabel.h"

class ColorSelectorButton : public ClickableLabel {
    Q_OBJECT
public:
    explicit ColorSelectorButton(QWidget *parent = nullptr);

    void setColor(const QColor &newColor);
    QColor color();
    void setDescription(QString text);

signals:
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *e);

private slots:
    void showColorSelector();

private:
    QColor mColor;
    QString mDescription;
};

#endif // COLORSELECTORBUTTON_H
