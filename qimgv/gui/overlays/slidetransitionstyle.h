#pragma once

// Kept free of widget headers so settings.cpp can use it. Append only: the index is stored in the config.
enum SlideTransitionStyle {
    TRANSITION_NONE,
    TRANSITION_FADE,
    TRANSITION_SLIDE,
    TRANSITION_ZOOM,
    TRANSITION_DIP_COLOR,
    TRANSITION_BLUR_FADE,
    TRANSITION_MOTION_BLUR,
    TRANSITION_PIXEL_MASH,
    TRANSITION_PUSH,
    TRANSITION_COVER,
    TRANSITION_WIPE,
    TRANSITION_IRIS,
    TRANSITION_DISSOLVE,
    TRANSITION_COUNT
};

// where the old slide leaves to (Auto follows next / previous)
enum SlideTransitionDirection {
    TRANSITION_DIR_AUTO,
    TRANSITION_DIR_LEFT,
    TRANSITION_DIR_RIGHT,
    TRANSITION_DIR_UP,
    TRANSITION_DIR_DOWN
};
