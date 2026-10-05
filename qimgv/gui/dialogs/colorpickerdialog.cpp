#include "colorpickerdialog.h"

ColorPickerDialog::ColorPickerDialog(const QColor &initial, QWidget *parent, const QString &title)
    : QDialog(parent),
      mInitial(initial.isValid() ? initial : QColor(Qt::white)),
      mColor(mInitial)
{
    setWindowTitle(title.isEmpty() ? tr("Select color") : title);
    setModal(true);

    svArea  = new ColorAreaWidget(ColorAreaWidget::KIND_SV, this);
    hueArea = new ColorAreaWidget(ColorAreaWidget::KIND_HUE, this);

    previewNew = new QLabel(this);
    previewOld = new QLabel(this);
    for(QLabel *label : {previewNew, previewOld}) {
        label->setMinimumHeight(24);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    hexEdit = new QLineEdit(this);
    hexEdit->setMaxLength(7);
    hexEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^#?([0-9a-fA-F]{3}|[0-9a-fA-F]{6})$"), hexEdit));

    rSpin = createSpinBox(255);
    gSpin = createSpinBox(255);
    bSpin = createSpinBox(255);
    hSpin = createSpinBox(359, QString(QChar(0x00B0)));
    sSpin = createSpinBox(100, "%");
    vSpin = createSpinBox(100, "%");

    // inputs column
    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);
    grid->addWidget(new QLabel(tr("New"), this),     0, 0);
    grid->addWidget(previewNew,                      0, 1);
    grid->addWidget(new QLabel(tr("Current"), this), 1, 0);
    grid->addWidget(previewOld,                      1, 1);
    grid->addWidget(new QLabel(tr("Hex"), this),     2, 0);
    grid->addWidget(hexEdit,                         2, 1);
    grid->addWidget(new QLabel("R", this), 3, 0);
    grid->addWidget(rSpin,                 3, 1);
    grid->addWidget(new QLabel("G", this), 4, 0);
    grid->addWidget(gSpin,                 4, 1);
    grid->addWidget(new QLabel("B", this), 5, 0);
    grid->addWidget(bSpin,                 5, 1);
    grid->addWidget(new QLabel("H", this), 6, 0);
    grid->addWidget(hSpin,                 6, 1);
    grid->addWidget(new QLabel("S", this), 7, 0);
    grid->addWidget(sSpin,                 7, 1);
    grid->addWidget(new QLabel("V", this), 8, 0);
    grid->addWidget(vSpin,                 8, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(9, 1);

    QHBoxLayout *top = new QHBoxLayout();
    top->setSpacing(10);
    top->addWidget(svArea, 1);
    top->addWidget(hueArea);
    top->addLayout(grid);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);
    root->addLayout(top, 1);
    root->addWidget(createSwatches());
    root->addWidget(buttons);

    // small enough for low-resolution screens
    setMinimumSize(420, 340);
    resize(500, 380);

    // interactions
    connect(svArea,  &ColorAreaWidget::saturationValueChanged, [this](int s, int v) { setHsvColor(mHue, s, v); });
    connect(hueArea, &ColorAreaWidget::hueChanged,             [this](int h)        { setHsvColor(h, mSat, mVal); });

    connect(hexEdit, &QLineEdit::textEdited, [this](const QString &text) {
        QColor c = parseHex(text);
        if(c.isValid())
            setRgbColor(c);
    });
    // normalize "#abc" / missing '#' once editing is done
    connect(hexEdit, &QLineEdit::editingFinished, [this]() {
        if(!mUpdating)
            hexEdit->setText(mColor.name());
    });

    auto onRgb = [this]() {
        if(!mUpdating)
            setRgbColor(QColor(rSpin->value(), gSpin->value(), bSpin->value()));
    };
    connect(rSpin, qOverload<int>(&QSpinBox::valueChanged), onRgb);
    connect(gSpin, qOverload<int>(&QSpinBox::valueChanged), onRgb);
    connect(bSpin, qOverload<int>(&QSpinBox::valueChanged), onRgb);

    auto onHsv = [this]() {
        if(!mUpdating)
            setHsvColor(hSpin->value(), qRound(sSpin->value() * 2.55), qRound(vSpin->value() * 2.55));
    };
    connect(hSpin, qOverload<int>(&QSpinBox::valueChanged), onHsv);
    connect(sSpin, qOverload<int>(&QSpinBox::valueChanged), onHsv);
    connect(vSpin, qOverload<int>(&QSpinBox::valueChanged), onHsv);

    setSwatch(previewOld, mInitial);
    setRgbColor(mInitial);
}

QColor ColorPickerDialog::color() const {
    return mColor;
}

QColor ColorPickerDialog::getColor(const QColor &initial, QWidget *parent, const QString &title) {
    ColorPickerDialog dialog(initial, parent, title);
    if(dialog.exec() == QDialog::Accepted)
        return dialog.color();
    return QColor();
}

QSpinBox *ColorPickerDialog::createSpinBox(int max, const QString &suffix) {
    QSpinBox *box = new QSpinBox(this);
    box->setRange(0, max);
    box->setSuffix(suffix);
    box->setKeyboardTracking(false);
    return box;
}

// accepts "#rgb", "#rrggbb" with or without the leading '#'
QColor ColorPickerDialog::parseHex(const QString &text) {
    QString name = text.trimmed();
    if(!name.startsWith('#'))
        name.prepend('#');
    if(name.length() != 4 && name.length() != 7)
        return QColor();
    return QColor(name); // invalid if not a hex color
}

// one-click swatches of the active theme
QWidget *ColorPickerDialog::createSwatches() {
    const ColorScheme &scheme = settings->colorScheme();
    const QList<QPair<QString, QColor>> entries = {
        { tr("Accent"),                scheme.accent },
        { tr("Text"),                  scheme.text },
        { tr("Icons"),                 scheme.icons },
        { tr("Background"),            scheme.background },
        { tr("Fullscreen background"), scheme.background_fullscreen },
        { tr("Widget"),                scheme.widget },
        { tr("Widget border"),         scheme.widget_border },
        { tr("FolderView"),            scheme.folderview },
        { tr("FolderView panel"),      scheme.folderview_topbar },
        { tr("Scrollbars"),            scheme.scrollbar },
        { tr("Overlay"),               scheme.overlay },
        { tr("Overlay text"),          scheme.overlay_text }
    };

    QWidget *holder = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(new QLabel(tr("Theme colors"), holder));
    for(const auto &entry : entries) {
        if(!entry.second.isValid())
            continue;
        QPushButton *button = new QPushButton(holder);
        button->setFixedSize(22, 22);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(entry.first + " " + entry.second.name());
        button->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid rgba(128,128,128,200);"
                                      " border-radius: 3px; padding: 0px; min-width: 0px; min-height: 0px; }"
                                      "QPushButton:hover { border-color: %2; }")
                              .arg(entry.second.name(), scheme.accent.name()));
        QColor swatchColor = entry.second;
        connect(button, &QPushButton::clicked, [this, swatchColor]() { setRgbColor(swatchColor); });
        layout->addWidget(button);
    }
    layout->addStretch(1);
    return holder;
}

void ColorPickerDialog::setSwatch(QLabel *label, const QColor &color) {
    label->setStyleSheet(QString("background-color: %1; border: 1px solid rgba(128,128,128,200); border-radius: 3px;")
                         .arg(color.name()));
}

// color chosen in RGB terms (hex, rgb spins, swatches).
// keeps the previous hue when it is undefined (grey / black)
void ColorPickerDialog::setRgbColor(const QColor &color) {
    if(!color.isValid())
        return;
    mColor = QColor(color.red(), color.green(), color.blue());
    int h, s, v;
    mColor.getHsv(&h, &s, &v);
    if(h >= 0)
        mHue = qBound(0, h, 359);
    mSat = s;
    mVal = v;
    refresh();
}

// color chosen in HSV terms (area widgets, hsv spins)
void ColorPickerDialog::setHsvColor(int h, int s, int v) {
    mHue = qBound(0, h, 359);
    mSat = qBound(0, s, 255);
    mVal = qBound(0, v, 255);
    mColor = QColor::fromHsv(mHue, mSat, mVal).toRgb();
    refresh();
}

// push current state into every input without re-triggering handlers
void ColorPickerDialog::refresh() {
    if(mUpdating)
        return;
    mUpdating = true;

    svArea->setHue(mHue);
    svArea->setSaturationValue(mSat, mVal);
    hueArea->setHue(mHue);

    // do not rewrite the hex field while the user is typing a value that already matches
    if(!(hexEdit->hasFocus() && parseHex(hexEdit->text()) == mColor))
        hexEdit->setText(mColor.name());

    const QList<QPair<QSpinBox *, int>> values = {
        { rSpin, mColor.red() },
        { gSpin, mColor.green() },
        { bSpin, mColor.blue() },
        { hSpin, mHue },
        { sSpin, qRound(mSat / 2.55) },
        { vSpin, qRound(mVal / 2.55) }
    };
    for(const auto &pair : values) {
        QSignalBlocker blocker(pair.first);
        pair.first->setValue(pair.second);
    }
    setSwatch(previewNew, mColor);

    mUpdating = false;
}
