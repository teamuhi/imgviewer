#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include "gui/customwidgets/colorareawidget.h"
#include "settings.h"

// Replacement for QColorDialog: SV square + hue strip, hex / RGB / HSV inputs,
// before/after preview and one-click swatches of the current theme colors.
class ColorPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ColorPickerDialog(const QColor &initial, QWidget *parent = nullptr, const QString &title = QString());

    QColor color() const;

    // returns an invalid color if the dialog was cancelled
    static QColor getColor(const QColor &initial, QWidget *parent = nullptr, const QString &title = QString());

private:
    QColor mInitial, mColor;
    int mHue = 0, mSat = 0, mVal = 0; // h: 0..359, s/v: 0..255
    bool mUpdating = false;

    ColorAreaWidget *svArea, *hueArea;
    QLabel *previewNew, *previewOld;
    QLineEdit *hexEdit;
    QSpinBox *rSpin, *gSpin, *bSpin, *hSpin, *sSpin, *vSpin;

    QSpinBox *createSpinBox(int max, const QString &suffix = QString());
    QWidget *createSwatches();
    void setSwatch(QLabel *label, const QColor &color);
    static QColor parseHex(const QString &text);

    void setRgbColor(const QColor &color);
    void setHsvColor(int h, int s, int v);
    void refresh();
};
