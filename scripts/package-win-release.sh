#!/bin/bash
# Builds a portable Windows package from an already built tree (MSYS2 UCRT64, Qt6).
# Usage (UCRT64 shell, repo root): scripts/package-win-release.sh <build_dir> <out_dir>
# Result: <out_dir> contains qimgv.exe + Qt/runtime DLLs, ready to zip.
set -euo pipefail

BUILD_DIR=$(realpath "${1:?build dir}")
OUT_DIR=$(realpath -m "${2:?output dir}")
SRC_DIR=$(dirname "$(dirname "$(readlink -f "$0")")")
MINGW_BIN=${MINGW_PREFIX:-/ucrt64}/bin

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
cp "$BUILD_DIR/qimgv/qimgv.exe" "$OUT_DIR"
cp -r "$BUILD_DIR/qimgv/translations" "$OUT_DIR"
mkdir -p "$OUT_DIR/cache" "$OUT_DIR/conf" "$OUT_DIR/thumbnails"
cp -r "$SRC_DIR/qimgv/distrib/mimedata/data" "$OUT_DIR"

# Qt DLLs, platform plugin, styles, image format plugins
windeployqt6 --release --no-translations --no-system-d3d-compiler --no-opengl-sw "$OUT_DIR/qimgv.exe"

# Extra image formats (webp, tiff, tga, ...) shipped with qt6-imageformats
PLUG_DIR=$(dirname "$(find "${MINGW_PREFIX:-/ucrt64}/share/qt6/plugins/imageformats" -name 'qwebp.dll' | head -n1)")
mkdir -p "$OUT_DIR/imageformats"
for p in qwebp qtiff qtga qwbmp qicns; do
    [ -f "$PLUG_DIR/$p.dll" ] && cp "$PLUG_DIR/$p.dll" "$OUT_DIR/imageformats/"
done

# Remaining MSYS2 runtime DLLs: resolve with ldd until nothing new shows up
for _ in 1 2 3 4; do
    added=0
    while IFS= read -r dll; do
        [ -f "$OUT_DIR/$(basename "$dll")" ] && continue
        cp "$dll" "$OUT_DIR/" && added=1
    done < <(find "$OUT_DIR" -name '*.exe' -o -name '*.dll' | xargs ldd 2>/dev/null \
             | awk '/=>/ {print $3}' | grep -i "$MINGW_BIN" | sort -u)
    [ "$added" -eq 0 ] && break
done

echo "Packaged $(find "$OUT_DIR" -type f | wc -l) files into $OUT_DIR"
