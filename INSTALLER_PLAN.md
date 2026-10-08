# Plan: one-click Windows installer + GitHub release

## Context
Right now a release is only a portable zip (`imgviewer-win64_<tag>.zip`, ~135 MB unzipped, 94 files) built by
[.github/workflows/release.yml](.github/workflows/release.yml) using [scripts/package-win-release.sh](scripts/package-win-release.sh).
Users have to unzip it, find the exe and set up file associations themselves. The user deleted the v1.0.4 GitHub
release. The remote has no tags now, but **local tag `v1.0.4` still exists**.
Goal: a single `qimgv-setup-<ver>-x64.exe` that installs everything (Qt runtime, image plugins, translations),
creates shortcuts, can register image file associations, upgrades in place and uninstalls cleanly. Publish it
on GitHub Releases next to the portable zip.

Decisions (from the user): display name **qimgv**, install dir `Program Files\qimgv` (the same folder `deploy.ps1` uses, so the
installer replaces that copy in place). Release assets: **installer + portable zip**. File associations: **optional checkbox, checked by default**.

## Tool choice: Inno Setup 6
- Gives a modern wizard, upgrades by `AppId`, an uninstaller, Restart Manager (closes a running qimgv), LZMA2 compression
  (~135 MB becomes roughly 35–45 MB), and admin or per-user install through `PrivilegesRequiredOverridesAllowed=dialog`.
- It is available on GitHub `windows-latest` runners. If `ISCC.exe` is missing, install it with `choco install innosetup`.
- It is not installed locally. Local testing is optional: `winget install JRSoftware.InnoSetup`.

## Changes

### 1. New `scripts/qimgv-setup.iss` (Inno script)
- `#define AppVersion` / `SourceDir` / `OutputDir`, overridable with `/D` from CI. Defaults are `1.0.4`, `..\build\pkg` and `..\dist`.
- `[Setup]`:
  - Identity: fixed `AppId={{<new GUID>}`, `AppName=qimgv`, `AppPublisher=teamuhi`, `AppPublisherURL`/`AppSupportURL`/`AppUpdatesURL` = https://github.com/teamuhi/imgviewer.
  - Paths: `DefaultDirName={autopf}\qimgv`, `DefaultGroupName=qimgv`, `DisableProgramGroupPage=yes`.
  - 64-bit only: `ArchitecturesAllowed=x64compatible`, `ArchitecturesInstallIn64BitMode=x64compatible`, `MinVersion=10.0`.
  - Privileges: `PrivilegesRequired=admin`, `PrivilegesRequiredOverridesAllowed=dialog`.
  - Running app and associations: `CloseApplications=yes`, `RestartApplications=no`, `ChangesAssociations=yes`.
  - Look: `WizardStyle=modern`, `SetupIconFile=..\qimgv\res\icons\common\logo\app\qimgv.ico`, `UninstallDisplayIcon={app}\qimgv.exe`, `LicenseFile=..\LICENSE`.
  - Output: `Compression=lzma2/ultra64`, `SolidCompression=yes`, `OutputBaseFilename=qimgv-setup-{#AppVersion}-x64`.
  - Version info: `VersionInfoVersion` = numeric part of the version.
- `[Tasks]`: `desktopicon` (unchecked) and `associate` ("Register qimgv for image files", checked).
- `[InstallDelete]`: delete `{app}\*.dll` and the plugin subfolders (`platforms`, `imageformats`, `styles`, `iconengines`, …) before copying.
  This stops stale DLLs from an older build or the `deploy.ps1` copy from mixing with the new Qt runtime.
- `[Files]`: `{#SourceDir}\*` → `{app}`, `recursesubdirs createallsubdirs ignoreversion`.
- `[Icons]`: Start menu `qimgv` entry, desktop icon (task), and an uninstall entry in the group.
- `[Registry]`, only with `Tasks: associate`. Use root `HKA` so both admin and per-user installs work. Every entry gets `uninsdeletekey` or `uninsdeletevalue`.
  - ProgID `Software\Classes\qimgv.image`: `DefaultIcon` = `{app}\qimgv.exe,0`, and `shell\open\command` = `"{app}\qimgv.exe" "%1"`.
  - For each extension: `Software\Classes\.<ext>\OpenWithProgids` value `qimgv.image` (`ValueType: none`).
  - `Software\Classes\Applications\qimgv.exe\SupportedTypes\.<ext>`.
  - Default Apps support: `Software\qimgv\Capabilities` (ApplicationName, ApplicationDescription, `FileAssociations\.<ext>=qimgv.image`) plus
    `Software\RegisteredApplications` value `qimgv`. qimgv then shows up in Windows Settings > Default apps. Windows 10/11 do not let an installer
    set the default app silently, so the user still has to pick it.
  - Extensions: jpg jpeg jpe jfif png gif webp bmp tif tiff tga ico svg wbmp icns. These are the formats the packaged Qt plugins support.
- `[Run]`: "Launch qimgv" postinstall checkbox (`nowait postinstall skipifsilent`).
- User settings stay in `%AppData%`/`%LocalAppData%`, so uninstall does not touch them. This is the standard behaviour.
  [settings.cpp](qimgv/settings.cpp) already falls back there when the install folder is not writable (Program Files).
  A per-user install folder is writable, so settings stay portable next to the exe there.

### 2. Update [.github/workflows/release.yml](.github/workflows/release.yml)
- Keep the MSYS2 build step as it is.
- **Version step (msys2):** read `VERSION` from the root [CMakeLists.txt](CMakeLists.txt) `project(... VERSION x.y.z)`.
  On a tag push, strip the leading `v` and **fail if the tag ≠ CMake version** so the exe and the installer versions always match.
  Write `version=` to `$GITHUB_OUTPUT`.
- **Package step:** unchanged (`package-win-release.sh build "$NAME"` + zip). Also output the absolute package dir.
- **New installer step, `shell: pwsh`, not msys2:** MSYS rewrites arguments that start with `/`, which would break the
  `/DAppVersion=` style ISCC arguments. The step:
  - finds `ISCC.exe` (`${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe`) and runs `choco install innosetup -y --no-progress` if it is missing;
  - runs `ISCC /DAppVersion=<ver> /DSourceDir=<pkg dir> /DOutputDir=<workspace>\dist scripts\qimgv-setup.iss`;
  - outputs the setup path.
- `softprops/action-gh-release@v2`: `files:` lists both the setup exe and the zip. Keep `generate_release_notes: true`.
  Add a short `body` that says to download `qimgv-setup-…exe` (or the zip for portable use) and that SmartScreen may warn because the installer is unsigned.
- Manual dispatch: `upload-artifact` uploads both files.

### 3. [README.md](README.md) Installation > Windows
Replace the upstream text (easymodo releases link, choco and winget lines, which install upstream qimgv) with:
- download `qimgv-setup-<ver>-x64.exe` from https://github.com/teamuhi/imgviewer/releases and run it;
- the portable zip option;
- the SmartScreen "More info → Run anyway" note;
- how to set qimgv as the default viewer (Settings > Default apps).

Leave the Linux sections as they are. No keybind changes.

### 4. `CLAUDELOGS.md`
Add a handoff entry. Mention the local-tag cleanup and how to cut future releases: bump `VERSION` in CMakeLists, then tag `vX.Y.Z`.

## Release steps (after the files are committed)
1. Commit. Per CLAUDE.md, the commit message must not contain any AI agent name, so leave out the Co-Authored-By line.
2. `git tag -d v1.0.4`. This removes the stale local tag, which points at the old commit. The remote tag is already gone.
3. `git push origin master`, then `git tag v1.0.4 && git push origin v1.0.4`. This triggers the release workflow.
4. There is no `gh` CLI, so check the run in the Actions tab. If it fails, nothing is published. Fix the problem, delete and re-push the tag.

## Verification
- **Local, optional (UCRT64 shell):**
  1. Run `scripts/package-win-release.sh build build/pkg`.
  2. In PowerShell, run `& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" /DSourceDir=..\build\pkg scripts\qimgv-setup.iss`.
  3. Run the generated setup and check that:
     - it installs to `C:\Program Files\qimgv` and replaces the `deploy.ps1` copy;
     - it closes a running qimgv;
     - the Start menu entry works;
     - right-click a .webp → Open with lists qimgv;
     - Settings > Default apps lists qimgv;
     - the "only for me" install mode works;
     - running the setup again upgrades in place;
     - uninstall removes the files and registry keys but keeps the settings in `%AppData%`.
  4. Start the installed exe with a PATH that has no MSYS2 in it, to confirm no DLLs are missing.
- **CI:** the workflow run is green and the release has both assets. Download the setup on a clean PC or VM, install it, and open a jpg, png, webp and tiff.
