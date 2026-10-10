A personal fork of [qimgv](https://github.com/easymodo/qimgv) by easymodo, made for my own use. I do not claim or own anything in the original project.

qimgv version: 1.0.6
=====
A fast, lightweight image viewer with a clean, uncluttered interface: panels and bars only show up when you need them. Video playback is optional.

<p align="center"><img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\192.png"></p>

>una

## Screenshots <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

<table>
  <thead>
    <tr>
      <th align="center">Main window &amp; panel</th>
      <th align="center">Collage view</th>
      <th align="center">Settings window</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td align="center"><img src="qimgv/distrib/screenshots/qimgv.PNG" alt="img1"></td>
      <td align="center"><img src="qimgv/distrib/screenshots/qimgv3.PNG" alt="img2"></td>
      <td align="center"><img src="qimgv/distrib/screenshots/qimgv2.PNG" alt="img3"></td>
    </tr>
  </tbody>
</table>

## New key features <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

- **Collage view and export** - arrange several images on one canvas (Mosaic, Grid, Row, Column or Freehand layouts), adjust every tile, and export the result as an image. Open it with __Ctrl+G__. See [Collage](#collage).

- **Improved slideshow** - a floating control bar, 13 transition styles (fade, slide, blur, pixel mash, wipe, iris, dissolve...), an adjustable easing curve, duration and strength, and optional file name / date captions. See [Slideshow](#slideshow).

- **More image formats** - WebP, TIFF, TGA, WBMP and ICNS out of the box, plus JPEG-XL, AVIF, APNG, HEIF and RAW through plugins. See [Additional image formats](#additional-image-formats).

- **Batch image format conversion** - convert many images to another format in one go.

- **Rulers and guides, top bar** - pixel / % / cm / in rulers with draggable guides, and a slim top bar with the settings menu, zoom level and an optional CPU / RAM readout. See [Top bar](#top-bar).

## Default control scheme: <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

<table>
  <thead>
    <tr>
      <th align="left">Action</th>
      <th align="left">Shortcut</th>
    </tr>
  </thead>
  <tbody>
    <tr><td>Next image</td><td>Right arrow / Ctrl+MouseWheel</td></tr>
    <tr><td>Previous image</td><td>Left arrow / Ctrl+MouseWheel</td></tr>
    <tr><td>Goto first image</td><td>Home</td></tr>
    <tr><td>Goto last image</td><td>End</td></tr>
    <tr><td>Zoom in</td><td>MouseWheel / Ctrl+Up</td></tr>
    <tr><td>Zoom out</td><td>MouseWheel / Ctrl+Down</td></tr>
    <tr><td>Zoom (alt. method)</td><td>Hold right mouse button &amp; move up / down</td></tr>
    <tr><td>Move the image freely (even when it fits the window)</td><td>MiddleMouse drag</td></tr>
    <tr><td>Reset a moved / zoomed image (back to the default fit, centred)</td><td>MiddleMouse click (a fit mode key also re-centres it)</td></tr>
    <tr><td>Fit mode: window</td><td>1</td></tr>
    <tr><td>Fit mode: width</td><td>2</td></tr>
    <tr><td>Fit mode: 1:1 (no scaling)</td><td>3</td></tr>
    <tr><td>Fit mode: window (stretch)</td><td>4</td></tr>
    <tr><td>Switch fit modes</td><td>Space</td></tr>
    <tr><td>Toggle fullscreen mode</td><td>DoubleClick / F / F11</td></tr>
    <tr><td>Exit fullscreen mode</td><td>Esc (never quits the app)</td></tr>
    <tr><td>Show EXIF panel</td><td>I</td></tr>
    <tr><td>Crop image</td><td>X</td></tr>
    <tr><td>Resize image</td><td>R</td></tr>
    <tr><td>Rotate left</td><td>Ctrl+L</td></tr>
    <tr><td>Rotate Right</td><td>Ctrl+R</td></tr>
    <tr><td>Flip horizontal / vertical</td><td>H / V</td></tr>
    <tr><td>Open containing directory</td><td>Ctrl+D</td></tr>
    <tr><td>Slideshow mode</td><td>Slideshow button in the top bar / context menu (no default key, bind <code>toggleSlideshow</code> in Settings &gt; Controls)</td></tr>
    <tr><td>Toggle rulers / guides</td><td>Ctrl+Shift+R (Settings &gt; View &gt; Rulers &amp; guides; drag from a ruler to add a guide, drag a guide back onto a ruler to remove it, right-click a ruler for units)</td></tr>
    <tr><td>Ruler settings (add a guide at an exact px / % / cm / in, snap to multiples of 2 / 5 / 10 ...)</td><td>Double-click a ruler</td></tr>
    <tr><td>Shuffle mode</td><td>Ctrl+<code>`</code></td></tr>
    <tr><td>Quick copy</td><td>C</td></tr>
    <tr><td>Quick move</td><td>M</td></tr>
    <tr><td>Move to trash</td><td>Delete</td></tr>
    <tr><td>Delete file</td><td>Shift+Delete</td></tr>
    <tr><td>Save</td><td>Ctrl+S</td></tr>
    <tr><td>Save As</td><td>Ctrl+Shift+S</td></tr>
    <tr><td>Discard edits</td><td>Ctrl+Z</td></tr>
    <tr><td>Rename</td><td>F2</td></tr>
    <tr><td>Reload image</td><td>F5</td></tr>
    <tr><td>Copy file / copy path / paste file</td><td>Ctrl+C / Ctrl+Shift+C / Ctrl+V</td></tr>
    <tr><td>Set as wallpaper</td><td>Ctrl+W</td></tr>
    <tr><td>Next / previous folder</td><td>Shift+Right / Shift+Left</td></tr>
    <tr><td>Toggle fullscreen info bar</td><td>Shift+F</td></tr>
    <tr><td>Zoom (keys) / scroll</td><td>+ - = (also with Ctrl) / Up Down</td></tr>
    <tr><td>Video: seek / frame step</td><td>Ctrl+Left / Ctrl+Right, <code>,</code> <code>.</code></td></tr>
    <tr><td>Mouse side buttons</td><td>previous / next image</td></tr>
    <tr><td>Context menu</td><td>Right click / Menu key</td></tr>
    <tr><td>Folder view</td><td>Enter / Backspace</td></tr>
    <tr><td>Open</td><td>Ctrl+O</td></tr>
    <tr><td>Collage view</td><td>Ctrl+G</td></tr>
    <tr><td>Print / Export PDF</td><td>Ctrl+P</td></tr>
    <tr><td>Settings</td><td>P</td></tr>
    <tr><td>Exit application</td><td>Ctrl+Q / Alt+X</td></tr>
  </tbody>
</table>

... and more.

Note: you can configure every shortcut by going to __Settings > Controls__

### Top bar <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

- The menu row on the left (General, View, Theme, Controls, Scripts, Advanced, About) opens the settings dialog on that page. On narrow windows it collapses into a *Settings* dropdown.

- The percentage on the right is the current magnification of the image.

- __Settings > View > Performance > Show CPU / RAM usage in the top bar__ (on by default) adds the CPU and memory usage of qimgv itself, updated every second. Hidden on narrow windows. In fullscreen the same readout shows in the top right corner together with the fullscreen info bar.

- The bar can be turned off in __Settings > General > Top bar__ (it is always hidden in fullscreen).

### Slideshow <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

Start it with the Slideshow button in the top bar (or the context menu). The window is stripped down to the picture: the top bar, panels and info bars are hidden. Move the mouse to bring up the slideshow bar; after a moment without movement the bar and the cursor hide again. The bar has previous / pause / next, the timer, the transition style, Loop, the settings gear and Exit. The timer, style and loop can also be set in __Settings > General > Slideshow__. Editing actions (delete, crop, rename...) are blocked while it runs.

#### Slideshow settings <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

The gear on the bar opens a popup above it. Changes apply immediately. It closes with Esc or a click outside it, and the bar stays visible while it is open. Controls that the chosen transition style does not use are greyed out.

<table>
  <thead>
    <tr>
      <th align="left">Option</th>
      <th align="left">What it does</th>
    </tr>
  </thead>
  <tbody>
    <tr><td>Captions</td><td>Shows the file name and / or the date the file was modified, bottom left. <strong>Text size</strong> scales them from 75 to 300 %</td></tr>
    <tr><td>Duration</td><td>How long one transition takes, 0.08 - 3 s (capped at 80 % of the time per slide)</td></tr>
    <tr><td>Easing</td><td>How the transition speeds up and slows down. Pick a preset, or drag the two handles of the curve (double-click a handle to reset it). The curve may overshoot for a springy feel</td></tr>
    <tr><td>Strength</td><td>The main amount of the effect: blur radius, pixel size, zoom, travel distance, motion trail length...</td></tr>
    <tr><td>Softness</td><td>Width of the soft edge of Wipe and Iris</td></tr>
    <tr><td>Block size</td><td>Tile size of Dissolve</td></tr>
    <tr><td>Direction</td><td>Which way the old slide leaves: Auto (follows next / previous), Left, Right, Up or Down</td></tr>
    <tr><td>Dip color</td><td>The color <strong>Dip to color</strong> fades through: any color from the picker, or Black / White / Accent</td></tr>
    <tr><td>Random style</td><td>Uses a different transition style on every slide (never the same one twice in a row)</td></tr>
  </tbody>
</table>

#### Transition styles <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

<table>
  <thead>
    <tr>
      <th align="left">Style</th>
      <th align="left">What it looks like</th>
    </tr>
  </thead>
  <tbody>
    <tr><td>None</td><td>Instant switch</td></tr>
    <tr><td>Fade</td><td>The old slide fades into the next one</td></tr>
    <tr><td>Slide</td><td>The old slide slides away and uncovers the next one</td></tr>
    <tr><td>Zoom</td><td>The old slide zooms in while it fades out</td></tr>
    <tr><td>Dip to color</td><td>Fades to a color, then from that color into the next slide</td></tr>
    <tr><td>Blur fade</td><td>The old slide blurs out while the next one sharpens in</td></tr>
    <tr><td>Motion blur</td><td>Both slides sweep across the screen with a motion trail</td></tr>
    <tr><td>Pixel mash</td><td>Both slides break into big pixels and swap at the coarsest point</td></tr>
    <tr><td>Push</td><td>The next slide pushes the old one out</td></tr>
    <tr><td>Cover</td><td>The next slide slides in over the old one, which dims</td></tr>
    <tr><td>Wipe</td><td>A soft edge sweeps across and replaces the old slide</td></tr>
    <tr><td>Iris</td><td>The next slide opens up from the centre in a growing circle</td></tr>
    <tr><td>Dissolve</td><td>Random tiles of the old slide fade away</td></tr>
  </tbody>
</table>

#### Slideshow keys <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

<table>
  <thead>
    <tr>
      <th align="left">Action (slideshow)</th>
      <th align="left">Shortcut</th>
    </tr>
  </thead>
  <tbody>
    <tr><td>Pause / resume</td><td>Space</td></tr>
    <tr><td>Previous / next slide</td><td>Left / Right</td></tr>
    <tr><td>Close the settings popup</td><td>Esc (first press, while it is open)</td></tr>
    <tr><td>Leave the slideshow</td><td>Esc (in fullscreen: leaves fullscreen first)</td></tr>
    <tr><td>Fullscreen</td><td>F / F11</td></tr>
  </tbody>
</table>

### Interface font <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

__Settings > General > Interface font__ (default Consolas, falls back to another monospace font when Consolas is not installed). Applied right away.

### Collage <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

Combine several images on one canvas and export the result. Open it with __Ctrl+G__ (or the collage button in the top bar): select 2+ images in folder view first, or pick files in the dialog. Choose how the tiles are arranged with the __Layout__ dropdown, then adjust each tile and export.
Only a few shortcuts work while the collage is open (open, settings, fullscreen, folder view).

<table>
  <thead>
    <tr>
      <th align="left">Action (collage view)</th>
      <th align="left">Shortcut</th>
    </tr>
  </thead>
  <tbody>
    <tr><td>Zoom the whole collage</td><td>MouseWheel</td></tr>
    <tr><td>Move the whole collage freely (any layout, also when starting on a tile)</td><td>Hold MiddleMouse + move</td></tr>
    <tr><td>Fit the collage back into the window</td><td>MiddleMouse click / Ctrl+0</td></tr>
    <tr><td>Pan the whole collage</td><td>Drag empty space</td></tr>
    <tr><td>Resize two neighbouring tiles (Mosaic / Grid / Row / Column)</td><td>Drag the border between them (Snap mode locks to 1/4, 1/3, 1/2, 2/3, 3/4; Alt = free; double-click = reset)</td></tr>
    <tr><td>Change a number field (W / H, gap, canvas, border...)</td><td>Drag it sideways (Shift = x10, Alt = finer), or click and type</td></tr>
    <tr><td>Move the picture inside its tile</td><td>Drag the tile (Alt+drag also works)</td></tr>
    <tr><td>Zoom the picture inside its tile</td><td>Shift+MouseWheel over the tile</td></tr>
    <tr><td>Swap two tiles</td><td>Ctrl+drag a tile onto another</td></tr>
    <tr><td>Open a tile in the normal viewer</td><td>DoubleClick / Enter (the top bar Back button, Backspace or Esc returns to the collage)</td></tr>
    <tr><td>Back (editor -&gt; collage view -&gt; image viewer)</td><td>Esc (when no tile is selected); leaving the collage view asks first, the collage stays in memory (Ctrl+G resumes it)</td></tr>
    <tr><td>Tile settings (aspect, size, crop, resolution)</td><td>Right click a tile</td></tr>
    <tr><td>Select previous / next tile</td><td>Left / Right arrow (Freehand layout: nudge the tile, Shift = 10 px)</td></tr>
    <tr><td>Place tiles yourself</td><td>Layout -&gt; Freehand: drag a tile, drag the handles to resize (Shift keeps proportions), Alt+drag moves the picture, Page Up / Page Down = front / back, Ctrl+0 = back to the centre</td></tr>
    <tr><td>Play / pause the selected animated tile (none selected: all)</td><td>Space</td></tr>
    <tr><td>Remove tile</td><td>Delete</td></tr>
    <tr><td>Switch to editor / back to view</td><td>E</td></tr>
    <tr><td>Show the toolbar (it slides in, and slides out again after a moment)</td><td>Move the cursor to the top edge</td></tr>
    <tr><td>Canvas (Window, pixel presets like 1920 x 1080 / 1080 x 1920, Custom W x H, shared with the editor)</td><td>"Canvas" dropdown in the bar (not used by Freehand)</td></tr>
    <tr><td>Background colour with opacity (or the theme background) / tile outline (width, colour, style)</td><td>"Background" + "Color" + opacity % / "Border" in the bar</td></tr>
    <tr><td>Grid / Row / Column options (columns, rows, cell size, rotation, style: tiles, circle, hexagon, polygon / star)</td><td>"Layout options" in the bar</td></tr>
    <tr><td>Exit the collage</td><td>"Exit" (first button of the bar)</td></tr>
    <tr><td>Crop in Freehand (drag pans the picture, wheel zooms it, handles off)</td><td>"Crop" toggle in the toolbar</td></tr>
  </tbody>
</table>

<table>
  <thead>
    <tr>
      <th align="left">Action (collage editor)</th>
      <th align="left">Shortcut</th>
    </tr>
  </thead>
  <tbody>
    <tr><td>Move / resize a frame</td><td>Drag / drag the handles (Shift keeps proportions, Ctrl disables snapping)</td></tr>
    <tr><td>Show the toolbar (it slides in, and slides out again after a moment)</td><td>Move the cursor to the top edge</td></tr>
    <tr><td>Pick how frames are arranged (Mosaic is the default)</td><td>"Layout" dropdown: Mosaic / Grid / Row / Column arrange automatically (drag pans the picture, Ctrl+drag swaps frames, drag a border to resize), Freehand = place frames yourself</td></tr>
    <tr><td>Move the canvas freely / fit it back</td><td>Hold MiddleMouse + move / MiddleMouse click</td></tr>
    <tr><td>Outline of every frame, Grid / Row / Column options</td><td>"Border" / "Layout options" in the toolbar (exported too)</td></tr>
    <tr><td>Pan the picture inside a frame (Freehand)</td><td>"Crop" toggle in the toolbar (drag pans, wheel zooms, handles off) or Alt+drag</td></tr>
    <tr><td>Canvas size, incl. portrait / mobile presets (3:4, 9:16, 20:9, iPhone)</td><td>"Canvas" dropdown or W / H in the toolbar</td></tr>
    <tr><td>Zoom the picture inside a frame</td><td>Shift+MouseWheel</td></tr>
    <tr><td>Nudge selected frames</td><td>Arrow keys (Shift = 10 px)</td></tr>
    <tr><td>Select all / bring to front / send to back</td><td>Ctrl+A / PageUp / PageDown</td></tr>
    <tr><td>Fit canvas / actual size</td><td>Ctrl+0 / Ctrl+1</td></tr>
    <tr><td>Play / pause animated frames</td><td>Space</td></tr>
  </tbody>
</table>

# User interface <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

The idea is to have an uncluttered, simple and easy to use UI. UI elements only show up when you need them.

There is a pull-down panel with thumbnails, as well as a folder view. A context menu is available on right click.

## Using quick copy / quick move panels <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

Bring up the panel with C or M shortcut. You will see 9 destination directories, click on the folder icon to change them.

With panel visible, use 1 - 9 keys to copy/move current image to corresponding directory.

When you are done press C or M again to hide the panel.

## Running scripts <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

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

## HiDPI (Linux / MacOS only) <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

If qimgv appears too small / too big on your display, you can override the scale factor. Example:
```
QT_SCALE_FACTOR="1.5" qimgv /path/to/image.png
```
You can put it in `qimgv.desktop` file to make it permanent. Using values less than `1.0` is not supported.

qimgv should also obey the global scale factor set in KDE's systemsettings.

## High quality scaling <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

qimgv supports nicer scaling filters when compiled with `opencv` support (ON by default, but might vary depending on your linux distribution). Filter options are available in __Settings > Scaling__. `Bicubic` or `bilinear+sharpen` is recommended.

# Additional image formats <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

Built-in Qt plugins cover JPEG, PNG, GIF, BMP, ICO, SVG, WebP, TIFF, TGA, WBMP and ICNS (WebP / TIFF / TGA / WBMP / ICNS come from `qt6-imageformats`, which `deploy.ps1` copies next to the exe).

The format is detected from the file **content**, not the extension, so a WebP renamed to `.png` still opens.

qimgv can open some extra formats via third-party image plugins. All of them are included with windows package.

<table>
  <thead>
    <tr>
      <th align="left">Format</th>
      <th align="left">Plugin</th>
    </tr>
  </thead>
  <tbody>
    <tr><td>JPEG-XL</td><td><a href="https://github.com/novomesk/qt-jpegxl-image-plugin">github.com/novomesk/qt-jpegxl-image-plugin</a></td></tr>
    <tr><td>AVIF</td><td><a href="https://github.com/novomesk/qt-avif-image-plugin">github.com/novomesk/qt-avif-image-plugin</a></td></tr>
    <tr><td>APNG</td><td><a href="https://github.com/Skycoder42/QtApng">github.com/Skycoder42/QtApng</a></td></tr>
    <tr><td>HEIF / HEIC</td><td><a href="https://github.com/jakar/qt-heif-image-plugin">github.com/jakar/qt-heif-image-plugin</a></td></tr>
    <tr><td>RAW</td><td><a href="https://gitlab.com/mardy/qtraw">https://gitlab.com/mardy/qtraw</a></td></tr>
  </tbody>
</table>

# Installation <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

## Windows <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

  Download `qimgv-setup-<version>-x64.exe` from the [releases page](https://github.com/teamuhi/imgviewer/releases) and run it. The installer:
  - installs to `Program Files\qimgv` (or per-user, via the "only for me" choice), upgrades in place and uninstalls cleanly (your settings in `%AppData%` are kept);
  - adds Start menu / optional desktop shortcuts;
  - optionally registers qimgv for image files (jpg, png, gif, webp, bmp, tiff, tga, ico, svg, ...).

  Prefer no install? Download `imgviewer-win64_<version>.zip` instead: it is portable (everything is contained within the folder).

  _The installer is unsigned, so Windows SmartScreen may warn: click __More info > Run anyway__._

  _Making qimgv the default viewer:_ Windows does not let an installer do that silently. Open __Settings > Apps > Default apps__, search for qimgv and pick it for the formats you want (or right-click an image > Open with > Choose another app > Always).

## GNU+Linux <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

### Arch Linux / Manjaro / etc. <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

AUR package: 

```
qimgv-git
```
  
### Ubuntu / Linux Mint / Pop!\_OS / etc. <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

```
sudo apt install qimgv
```

### Fedora <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

```
sudo dnf install qimgv
```

### OpenSUSE <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

```
zypper install qimgv
```

### Gentoo <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

```
emerge qimgv
```

### Void linux <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

```
xbps-install -S qimgv
```

### Alpine Linux <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

```
apk add qimgv
```

## BSD <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

### FreeBSD <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

```
pkg install qimgv
```

This list may be incomplete. 

## Compiling from source <img src="C:\Users\timoT\Documents\GitHub\imgviewer\qimgv\res\icons\common\logo\app\20.png">

See [Compiling qimgv from source](https://github.com/easymodo/qimgv/wiki/Compiling-qimgv-from-source) on the wiki

