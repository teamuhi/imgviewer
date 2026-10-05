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

try {
    Copy-Item $built $target -Force -ErrorAction Stop
} catch {
    # Program Files may need admin rights on another setup: retry elevated
    Start-Process powershell -Verb RunAs -Wait -ArgumentList "-Command Copy-Item '$built' '$target' -Force"
}
Write-Host "Installed $((Get-Item $target).LastWriteTime): $target"
