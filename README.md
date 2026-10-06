## :exclamation: Updates may be slow due to war in Ukraine :sunflower: :sunflower: :sunflower:

qimgv | Current version: 1.0.2
=====
Image viewer. Fast, easy to use. Optional video support.

## Screenshots

Main window & panel        |  Folder view   |  Settings window  
:-------------------------:|:-------------------------:|:-------------------------:|
[![img1](qimgv/distrib/screenshots/qimgv0.9_1_thumb.jpg)](qimgv/distrib/screenshots/qimgv0.9_1.jpg?raw=true)  |  [![img2](qimgv/distrib/screenshots/qimgv0.9_2_thumb.jpg)](qimgv/distrib/screenshots/qimgv0.9_2.jpg?raw=true) | [![img3](qimgv/distrib/screenshots/qimgv_3_thumb.jpg)](qimgv/distrib/screenshots/qimgv_3.jpg?raw=true)

## Key features:

- Simple UI

- Fast

- Easy to use

- Fully configurable, including themes, shortcuts

- High quality scaling

- Basic image editing: Crop, Rotate and Resize

- Ability to quickly copy / move images to different folders

- Experimental video playback via libmpv

- Folder view mode

- Ability to run shell scripts

## Default control scheme:

| Action  | Shortcut |
| ------------- | ------------- |
| Next image  | Right arrow / Ctrl+MouseWheel |
| Previous image  | Left arrow / Ctrl+MouseWheel |
| Goto first image  | Home |
| Goto last image  | End |
| Zoom in  | MouseWheel / Ctrl+Up |
| Zoom out  | MouseWheel / Ctrl+Down |
| Zoom (alt. method) | Hold right mouse button & move up / down |
| Move the image freely (even when it fits the window) | MiddleMouse drag |
| Reset a moved / zoomed image (back to the default fit, centred) | MiddleMouse click (a fit mode key also re-centres it) |
| Fit mode: window | 1 |
| Fit mode: width | 2 |
| Fit mode: 1:1 (no scaling) | 3 |
| Switch fit modes  | Space |
| Toggle fullscreen mode  | DoubleClick / F / F11 |
| Exit fullscreen mode | Esc (never quits the app) |
| Show EXIF panel  | I |
| Crop image  | X |
| Resize image  | R |
| Rotate left  | Ctrl+L |
| Rotate Right  | Ctrl+R |
| Open containing directory | Ctrl+D |
| Slideshow mode | ~ |
| Shuffle mode | Ctrl+~ |
| Quick copy  | C |
| Quick move  | M |
| Move to trash | Delete |
| Delete file  | Shift+Delete |
| Save  | Ctrl+S |
| Save As  | Ctrl+Shift+S |
| Folder view | Enter / Backspace |
| Open | Ctrl+O |
| Collage view | Ctrl+G |
| Print / Export PDF | Ctrl+P |
| Settings  | P |
| Exit application | Ctrl+Q / Alt+X |

... and more.

Note: you can configure every shortcut by going to __Settings > Controls__

### Top bar

- The menu row on the left (General, View, Theme, Controls, Scripts, Advanced, About) opens the settings dialog on that page. On narrow windows it collapses into a *Settings* dropdown.
- The percentage on the right is the current magnification of the image.
- __Settings > View > Performance > Show CPU / RAM usage in the top bar__ (on by default) adds the CPU and memory usage of qimgv itself, updated every second. Hidden on narrow windows. In fullscreen the same readout shows in the top right corner together with the fullscreen info bar.
- The bar can be turned off in __Settings > General > Top bar__ (it is always hidden in fullscreen).

### Collage

Open several images at once with __Ctrl+G__ (or the collage button in the top bar): select 2+ images in folder view first, or pick files in the dialog.
Only a few shortcuts work while the collage is open (open, settings, fullscreen, folder view).

| Action (collage view) | Shortcut |
| ------------- | ------------- |
| Zoom the whole collage | MouseWheel |
| Pan the whole collage (Freehand: endless canvas, pans at any zoom) | Drag empty space / MiddleMouse drag |
| Move the picture inside its tile | Drag the tile (Alt+drag also works) |
| Zoom the picture inside its tile | Shift+MouseWheel over the tile |
| Swap two tiles | Ctrl+drag a tile onto another |
| Open a tile in the normal viewer | DoubleClick / Enter (the top bar Back button, Backspace or Esc returns to the collage) |
| Back (editor -> collage view -> image viewer) | Esc (when no tile is selected) |
| Tile settings (aspect, size, crop, resolution) | Right click a tile |
| Select previous / next tile | Left / Right arrow (Freehand layout: nudge the tile, Shift = 10 px) |
| Place tiles yourself | Layout -> Freehand: drag a tile, drag the handles to resize (Shift keeps proportions), Alt+drag moves the picture, Page Up / Page Down = front / back, Ctrl+0 = back to the centre |
| Play / pause the selected animated tile (none selected: all) | Space |
| Remove tile | Delete |
| Switch to editor / back to view | E |
| Show the toolbar (it slides in, and slides out again after a moment) | Move the cursor to the top edge |
| Frame shape (Window, 16:9, 4:5, mobile 9:16, square) | "Canvas" dropdown in the toolbar (not used by Freehand) |
| Crop in Freehand (drag pans the picture, wheel zooms it, handles off) | "Crop" toggle in the toolbar |

| Action (collage editor) | Shortcut |
| ------------- | ------------- |
| Move / resize a frame | Drag / drag the handles (Shift keeps proportions, Ctrl disables snapping) |
| Show the toolbar (it slides in, and slides out again after a moment) | Move the cursor to the top edge |
| Pick how frames are arranged (Mosaic is the default) | "Layout" dropdown: Mosaic / Grid / Row / Column arrange automatically (drag pans the picture, Ctrl+drag swaps frames), Freehand = place frames yourself |
| Pan the picture inside a frame (Freehand) | "Crop" toggle in the toolbar (drag pans, wheel zooms, handles off) or Alt+drag |
| Canvas size, incl. portrait / mobile presets (3:4, 9:16, 20:9, iPhone) | "Canvas" dropdown or W / H in the toolbar |
| Zoom the picture inside a frame | Shift+MouseWheel |
| Nudge selected frames | Arrow keys (Shift = 10 px) |
| Select all / bring to front / send to back | Ctrl+A / PageUp / PageDown |
| Fit canvas / actual size | Ctrl+0 / Ctrl+1 |
| Play / pause animated frames | Space |

# User interface

The idea is to have a uncluttered, simple and easy to use UI. You can see UI elements only when you need them.

There is a pull-down panel with thumbnails, as well as folder view. You can also bring up a context menu via right click.

## Using quick copy / quick move panels

Bring up the panel with C or M shortcut. You will see 9 destination directories, click on the folder icon to change them.

With panel visible, use 1 - 9 keys to copy/move current image to corresponding directory.

When you are done press C or M again to hide the panel.

## Running scripts

You can run custom scripts on a current image.

Open __Settings > Scripts__. Press Add. Here you can choose between a shell command and a shell script. 

Example of a command: 

`convert %file% %file%_.pdf`

Example of a shell script file (`$1` will be image path): 
```
#!/bin/bash
gimp "$1"
```
_Note: The script file must be an executable. Also, "shebang" (`#!/bin/bash`) needs to be present._

When you've created your script go to __Settings > Controls > Add__, then select it and assign a shortcut like for any regular action.

## HiDPI (Linux / MacOS only)

If qimgv appears too small / too big on your display, you can override the scale factor. Example:
```
QT_SCALE_FACTOR="1.5" qimgv /path/to/image.png
```
You can put it in `qimgv.desktop` file to make it permanent. Using values less than `1.0` is not supported.

qimgv should also obey the global scale factor set in KDE's systemsettings.

## High quality scaling

qimgv supports nicer scaling filters when compiled with `opencv` support (ON by default, but might vary depending on your linux distribution). Filter options are available in __Settings > Scaling__. `Bicubic` or `bilinear+sharpen` is recommended.

# Additional image formats

Built-in Qt plugins cover JPEG, PNG, GIF, BMP, ICO, SVG, WebP, TIFF, TGA, WBMP and ICNS (WebP / TIFF / TGA / WBMP / ICNS come from `qt6-imageformats`, which `deploy.ps1` copies next to the exe).

The format is detected from the file **content**, not the extension, so a WebP renamed to `.png` still opens.

qimgv can open some extra formats via third-party image plugins. All of them are included with windows package.

| Format  | Plugin |
| ------- | ------------- |
| JPEG-XL | [github.com/novomesk/qt-jpegxl-image-plugin](https://github.com/novomesk/qt-jpegxl-image-plugin) |
| AVIF | [github.com/novomesk/qt-avif-image-plugin](https://github.com/novomesk/qt-avif-image-plugin) |
| APNG | [github.com/Skycoder42/QtApng](https://github.com/Skycoder42/QtApng) |
| HEIF / HEIC | [github.com/jakar/qt-heif-image-plugin](https://github.com/jakar/qt-heif-image-plugin) |
| RAW | [https://gitlab.com/mardy/qtraw](https://gitlab.com/mardy/qtraw) |

# Installation

## Windows
  
  Windows builds are portable (everything is contained within the install folder). The installer also sets up file associations.
  
  _NOTE: `-video` variants include mpv for video support_
  
  Grab the latest version from the [releases page](https://github.com/easymodo/qimgv/releases)

  Alternatively you can install it with Chocolatey:
  
  ```
  choco install qimgv
  ```
  
  Or WinGet:
  
  ```
  winget install --id easymodo.qimgv
  ```

## GNU+Linux

### Arch Linux / Manjaro / etc.

AUR package: 

```
qimgv-git
```
  
### Ubuntu / Linux Mint / Pop!\_OS / etc.

```
sudo apt install qimgv
```

### Fedora

```
sudo dnf install qimgv
```

### OpenSUSE

```
zypper install qimgv
```

### Gentoo

```
emerge qimgv
```

### Void linux

```
xbps-install -S qimgv
```

### Alpine Linux

```
apk add qimgv
```

## BSD

### FreeBSD

```
pkg install qimgv
```

This list may be incomplete. 

## Compiling from source

See [Compiling qimgv from source](https://github.com/easymodo/qimgv/wiki/Compiling-qimgv-from-source) on the wiki

# Donate

If you wish to give me a few bucks, please consider donating to the Ukrainian Army instead:

[https://savelife.in.ua/en/donate-en/#donate-army-card-once](https://savelife.in.ua/en/donate-en/#donate-army-card-once)

[https://u24.gov.ua/](https://u24.gov.ua/)

This directly increases the chances of me being able to work on this in future
