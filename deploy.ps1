# Rebuild qimgv and install the exe into C:\Program Files\qimgv (the copy Windows opens images with).
# Usage (PowerShell, from the repo root):  .\deploy.ps1
# Close qimgv first - a running exe can not be overwritten.

$ErrorActionPreference = "Stop"
$repo = $PSScriptRoot
$target = "C:\Program Files\qimgv\qimgv.exe"
$built = Join-Path $repo "build\qimgv\qimgv.exe"

if (Get-Process qimgv -ErrorAction SilentlyContinue) {
    Write-Error "qimgv is running - close it first."
}

$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
cmake --build (Join-Path $repo "build")
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed." }

# Extra image format plugins (webp, tiff, tga, ...) and the DLLs they need.
# Requires: pacman -S mingw-w64-ucrt-x86_64-qt6-imageformats
$installDir = Split-Path $target
$bin = "C:\msys64\ucrt64\bin"
$plugDir = "C:\msys64\ucrt64\share\qt6\plugins\imageformats"
$plugins = "qwebp","qtiff","qtga","qwbmp","qicns"
$deps = "libwebp-7","libwebpdemux-2","libwebpmux-3","libsharpyuv-0","libtiff-6","libjbig-0","libdeflate","liblzma-5","libLerc"
$copy = @(@{ From = $built; To = $target })
foreach ($p in $plugins) { $copy += @{ From = "$plugDir\$p.dll"; To = "$installDir\imageformats\$p.dll" } }
foreach ($d in $deps)    { $copy += @{ From = "$bin\$d.dll";     To = "$installDir\$d.dll" } }
$copy = @($copy | Where-Object { Test-Path $_.From })

try {
    foreach ($c in $copy) { Copy-Item $c.From $c.To -Force -ErrorAction Stop }
} catch {
    # Program Files may need admin rights on another setup: retry elevated
    $cmds = ($copy | ForEach-Object { "Copy-Item '$($_.From)' '$($_.To)' -Force" }) -join "; "
    Start-Process powershell -Verb RunAs -Wait -ArgumentList "-Command $cmds"
}
Write-Host "Installed $((Get-Item $target).LastWriteTime): $target"
