#pragma once

#include <QtGlobal>

// Canvas sizes offered by the collage view "Canvas" dropdown and the editor preset combo.
// Names are translated in the CollageWidget context: tr(COLLAGE_PRESETS[i].name).
struct CollagePreset {
    const char *name;
    int width, height;
};

inline const CollagePreset COLLAGE_PRESETS[] = {
    { QT_TRANSLATE_NOOP("CollageWidget", "Full HD  1920 x 1080"),         1920, 1080 },
    { QT_TRANSLATE_NOOP("CollageWidget", "4K UHD  3840 x 2160"),          3840, 2160 },
    { QT_TRANSLATE_NOOP("CollageWidget", "QHD  2560 x 1440"),             2560, 1440 },
    { QT_TRANSLATE_NOOP("CollageWidget", "Square  1080 x 1080"),          1080, 1080 },
    { QT_TRANSLATE_NOOP("CollageWidget", "Portrait 4:5  1080 x 1350"),    1080, 1350 },
    { QT_TRANSLATE_NOOP("CollageWidget", "Portrait 3:4  1080 x 1440"),    1080, 1440 },
    { QT_TRANSLATE_NOOP("CollageWidget", "Story 9:16  1080 x 1920"),      1080, 1920 },
    { QT_TRANSLATE_NOOP("CollageWidget", "Mobile 20:9  1080 x 2400"),     1080, 2400 },
    { QT_TRANSLATE_NOOP("CollageWidget", "iPhone  1179 x 2556"),          1179, 2556 },
    { QT_TRANSLATE_NOOP("CollageWidget", "A4 portrait  2480 x 3508"),     2480, 3508 },
    { QT_TRANSLATE_NOOP("CollageWidget", "A4 landscape  3508 x 2480"),    3508, 2480 },
};
inline const int COLLAGE_PRESET_COUNT = sizeof(COLLAGE_PRESETS) / sizeof(COLLAGE_PRESETS[0]);
