#include "settings.h"
#include "gui/overlays/slidetransitionstyle.h"

Settings *settings = nullptr;

#if !defined(__linux__) && !defined(__FreeBSD__)
namespace {
// QFileInfo::isWritable() ignores Windows ACLs, so really try to create a file.
// "Program Files" is read-only for normal users: settings written there silently vanish.
bool canWriteTo(const QString &dirPath) {
    QDir dir(dirPath);
    if(!dir.mkpath(dirPath))
        return false;
    QFile probe(dir.absoluteFilePath(".qimgv_write_test"));
    if(!probe.open(QIODevice::WriteOnly))
        return false;
    probe.close();
    probe.remove();
    return true;
}

// portable layout (next to the exe) when that folder is writable, otherwise a per-user folder
QString writableDir(const QString &portablePath, QStandardPaths::StandardLocation fallbackLocation, const QString &subDir) {
    if(canWriteTo(portablePath))
        return portablePath;
    QString base = QStandardPaths::writableLocation(fallbackLocation);
    if(base.isEmpty())
        base = QDir::homePath() + "/.qimgv";
    QString path = subDir.isEmpty() ? base : base + "/" + subDir;
    QDir().mkpath(path);
    qDebug() << portablePath << "is not writable, using" << path;
    return path;
}
}
#endif

Settings::Settings(QObject *parent) : QObject(parent) {
#if defined(__linux__) || defined(__FreeBSD__)
    // config files
    QSettings::setDefaultFormat(QSettings::NativeFormat);
    settingsConf = new QSettings();
    stateConf = new QSettings(QCoreApplication::organizationName(), "savedState");
    themeConf = new QSettings(QCoreApplication::organizationName(), "theme");
#else
    const QString portableConf = QApplication::applicationDirPath() + "/conf";
    mConfDir = new QDir(writableDir(portableConf, QStandardPaths::AppConfigLocation, QString()));
    if(mConfDir->absolutePath() != QDir(portableConf).absolutePath()) {
        // first run from a per-user folder: carry over what an older (read-only) install folder has
        for(const QString &name : { qApp->applicationName() + ".ini", QString("savedState.ini"), QString("theme.ini") }) {
            QString target = mConfDir->absoluteFilePath(name);
            QString source = portableConf + "/" + name;
            if(!QFile::exists(target) && QFile::exists(source))
                QFile::copy(source, target);
        }
    }
    settingsConf = new QSettings(mConfDir->absolutePath() + "/" + qApp->applicationName() + ".ini", QSettings::IniFormat);
    stateConf = new QSettings(mConfDir->absolutePath() + "/savedState.ini", QSettings::IniFormat);
    themeConf = new QSettings(mConfDir->absolutePath() + "/theme.ini", QSettings::IniFormat);
#endif
    fillVideoFormats();
}
//------------------------------------------------------------------------------
Settings::~Settings() {
    saveTheme();
    delete mThumbCacheDir;
    delete mTmpDir;
    delete settingsConf;
    delete stateConf;
    delete themeConf;
}
//------------------------------------------------------------------------------
Settings *Settings::getInstance() {
    if(!settings) {
        settings = new Settings();
        settings->setupCache();
        // before the theme: the stylesheet sizes its widgets from the application font metrics
        applyInterfaceFont(settings->interfaceFont());
        settings->loadTheme();
    }
    return settings;
}
//------------------------------------------------------------------------------
void Settings::setupCache() {
#if defined(__linux__) ||  defined(__FreeBSD__)
    QString genericCacheLocation = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if(genericCacheLocation.isEmpty())
        genericCacheLocation = QDir::homePath() + "/.cache";
    genericCacheLocation.append("/" + QApplication::applicationName());
    QString cacheLocation = settings->settingsConf->value("cacheDir", genericCacheLocation).toString();
    mTmpDir = new QDir(cacheLocation);
    mTmpDir->mkpath(mTmpDir->absolutePath());
    QFileInfo dirTest(mTmpDir->absolutePath());
    if(!dirTest.isDir() || !dirTest.isWritable() || !dirTest.exists()) {
        // fallback
        qDebug() << "Error: cache dir is not writable" << mTmpDir->absolutePath();
        qDebug() << "Trying to use" << genericCacheLocation << "instead";
        mTmpDir->setPath(genericCacheLocation);
        mTmpDir->mkpath(mTmpDir->absolutePath());
    }
    mThumbCacheDir = new QDir(mTmpDir->absolutePath() + "/thumbnails");
    mThumbCacheDir->mkpath(mThumbCacheDir->absolutePath());
#else
    mTmpDir = new QDir(writableDir(QApplication::applicationDirPath() + "/cache", QStandardPaths::CacheLocation, QString()));
    mTmpDir->mkpath(mTmpDir->absolutePath());
    mThumbCacheDir = new QDir(writableDir(QApplication::applicationDirPath() + "/thumbnails", QStandardPaths::CacheLocation, "thumbnails"));
    mThumbCacheDir->mkpath(mThumbCacheDir->absolutePath());
#endif
}
//------------------------------------------------------------------------------
void Settings::sync() {
    settings->settingsConf->sync();
    settings->stateConf->sync();
}
//------------------------------------------------------------------------------
QString Settings::thumbnailCacheDir() {
    return mThumbCacheDir->path() + "/";
}
//------------------------------------------------------------------------------
QString Settings::tmpDir() {
    return mTmpDir->path() + "/";
}
//------------------------------------------------------------------------------
// this here is temporarily, will be moved to some sort of theme manager class
void Settings::loadStylesheet() {
    // stylesheet template file
    QFile file(":/res/styles/style-template.qss");
    if(file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());

        // --- color scheme ---------------------------------------------
        auto colors = settings->colorScheme();
        // tint color for system windows
        QPalette p;
        QColor sys_text = p.text().color();
        QColor sys_window = p.window().color();
        // dialogs follow the app theme unless the system scheme was requested explicitly
        if(!settings->useSystemColorScheme()) {
            sys_text = colors.text;
            sys_window = colors.widget;
        }
        QColor sys_window_tinted, sys_window_tinted_lc, sys_window_tinted_lc2, sys_window_tinted_hc, sys_window_tinted_hc2;
        if(sys_window.valueF() <= 0.45f) {
            // dark system theme
            sys_window_tinted_lc2.setHsv(sys_window.hue(), sys_window.saturation(), sys_window.value() + 6);
            sys_window_tinted_lc.setHsv(sys_window.hue(),  sys_window.saturation(), sys_window.value() + 14);
            sys_window_tinted.setHsv(sys_window.hue(),     sys_window.saturation(), sys_window.value() + 20);
            sys_window_tinted_hc.setHsv(sys_window.hue(),  sys_window.saturation(), sys_window.value() + 35);
            sys_window_tinted_hc2.setHsv(sys_window.hue(), sys_window.saturation(), sys_window.value() + 50);
        } else {
            // light system theme
            sys_window_tinted_lc2.setHsv(sys_window.hue(), sys_window.saturation(), sys_window.value() - 6);
            sys_window_tinted_lc.setHsv(sys_window.hue(),  sys_window.saturation(), sys_window.value() - 14);
            sys_window_tinted.setHsv(sys_window.hue(),     sys_window.saturation(), sys_window.value() - 20);
            sys_window_tinted_hc.setHsv(sys_window.hue(),  sys_window.saturation(), sys_window.value() - 35);
            sys_window_tinted_hc2.setHsv(sys_window.hue(), sys_window.saturation(), sys_window.value() - 50);
        }

        // --- widget sizes ---------------------------------------------
        auto fnt = QGuiApplication::font();
        QFontMetrics fm(fnt);
        // todo: use precise values for ~9-11 point sizes
        int font_small = qMax((int)(fnt.pointSize() * 0.9f), 8);
        int font_large = (int)(fnt.pointSize() * 1.8f);
        int text_height = fm.height();
        int text_padding = (int)(text_height * 0.10f);
        int text_padding_small = (int)(text_height * 0.05f);
        int text_padding_large = (int)(text_height * 0.25f);

        // folderview top panel item sizes
        int top_panel_v_margin = 4;
        // ensure at least 4px so its not too thin
        int top_panel_text_padding = qMax(text_padding, 4);
        // scale with font, 38px base size
        int top_panel_height = qMax((text_height + top_panel_text_padding * 2 + top_panel_v_margin * 2), 38);

        // overlay headers
        int overlay_header_margin = 2;
        // 32px base size
        int overlay_header_size = qMax(text_height + text_padding * 2, 30);

        // todo
        int button_height = text_height + text_padding_large * 2;

        // pseudo-dpi to scale some widget widths
        int text_height_base = 22;
        qreal pDpr = qMax( ((qreal)(text_height) / text_height_base), 1.0);
        int context_menu_width = 212 * pDpr;
        int context_menu_button_height = 32 * pDpr;
        int rename_overlay_width = 380 * pDpr;

        //qDebug()<< "dpr=" << qApp->devicePixelRatio() << "pDpr=" << pDpr;

        // --- write variables into stylesheet --------------------------
        styleSheet.replace("%font_small%", QString::number(font_small)+"pt");
        styleSheet.replace("%font_large%", QString::number(font_large)+"pt");
        styleSheet.replace("%button_height%", QString::number(button_height)+"px");
        styleSheet.replace("%top_panel_height%", QString::number(top_panel_height)+"px");
        styleSheet.replace("%overlay_header_size%", QString::number(overlay_header_size)+"px");
        styleSheet.replace("%context_menu_width%", QString::number(context_menu_width)+"px");
        styleSheet.replace("%context_menu_button_height%", QString::number(context_menu_button_height)+"px");
        styleSheet.replace("%rename_overlay_width%", QString::number(rename_overlay_width)+"px");

        styleSheet.replace("%icontheme%",  "light");
        // Qt::Popup can't do transparency under windows, use square window
#ifdef _WIN32
        styleSheet.replace("%contextmenu_border_radius%",  "0px");
#else
        styleSheet.replace("%contextmenu_border_radius%",  "3px");
#endif
        styleSheet.replace("%sys_window%",    sys_window.name());
        styleSheet.replace("%sys_text%",      sys_text.name());
        styleSheet.replace("%sys_window_tinted%",    sys_window_tinted.name());
        styleSheet.replace("%sys_window_tinted_lc%", sys_window_tinted_lc.name());
        styleSheet.replace("%sys_window_tinted_lc2%", sys_window_tinted_lc2.name());
        styleSheet.replace("%sys_window_tinted_hc%", sys_window_tinted_hc.name());
        styleSheet.replace("%sys_window_tinted_hc2%", sys_window_tinted_hc2.name());
        styleSheet.replace("%sys_text_secondary_rgba%", "rgba(" + QString::number(sys_text.red())   + ","
                                                      + QString::number(sys_text.green()) + ","
                                                      + QString::number(sys_text.blue())  + ",50%)");

        styleSheet.replace("%button%",               colors.button.name());
        styleSheet.replace("%button_hover%",         colors.button_hover.name());
        styleSheet.replace("%button_pressed%",       colors.button_pressed.name());
        styleSheet.replace("%panel_button%",         colors.panel_button.name());
        styleSheet.replace("%panel_button_hover%",   colors.panel_button_hover.name());
        styleSheet.replace("%panel_button_pressed%", colors.panel_button_pressed.name());
        styleSheet.replace("%widget%",               colors.widget.name());
        styleSheet.replace("%widget_border%",        colors.widget_border.name());
        styleSheet.replace("%folderview%",           colors.folderview.name());
        styleSheet.replace("%folderview_topbar%",    colors.folderview_topbar.name());
        styleSheet.replace("%folderview_hc%",        colors.folderview_hc.name());
        styleSheet.replace("%folderview_hc2%",       colors.folderview_hc2.name());
        // check mark drawn on top of the accent colored indicator: dark on light accents
        styleSheet.replace("%check_icon%",           colors.accent.lightnessF() > 0.62
                                                         ? ":/res/icons/common/checkbox/check-dark.png"
                                                         : ":/res/icons/common/checkbox/check-white.png");
        styleSheet.replace("%accent%",               colors.accent.name());
        styleSheet.replace("%input_field_focus%",    colors.input_field_focus.name());
        styleSheet.replace("%overlay%",              colors.overlay.name());
        styleSheet.replace("%icons%",                colors.icons.name());
        styleSheet.replace("%text_hc2%",             colors.text_hc2.name());
        styleSheet.replace("%text_hc%",              colors.text_hc.name());
        styleSheet.replace("%text%",                 colors.text.name());
        styleSheet.replace("%overlay_text%",         colors.overlay_text.name());
        styleSheet.replace("%text_lc%",              colors.text_lc.name());
        styleSheet.replace("%text_lc2%",             colors.text_lc2.name());
        styleSheet.replace("%scrollbar%",            colors.scrollbar.name());
        styleSheet.replace("%scrollbar_hover%",      colors.scrollbar_hover.name());
        styleSheet.replace("%folderview_button_hover%",   colors.folderview_button_hover.name());
        styleSheet.replace("%folderview_button_pressed%", colors.folderview_button_pressed.name());
        styleSheet.replace("%text_secondary_rgba%",  "rgba(" + QString::number(colors.text.red())   + ","
                                                             + QString::number(colors.text.green()) + ","
                                                             + QString::number(colors.text.blue())  + ",62%)");
        styleSheet.replace("%accent_hover_rgba%",    "rgba(" + QString::number(colors.accent.red())   + ","
                                                             + QString::number(colors.accent.green()) + ","
                                                             + QString::number(colors.accent.blue())  + ",65%)");
        styleSheet.replace("%overlay_rgba%",         "rgba(" + QString::number(colors.overlay.red())   + ","
                                                             + QString::number(colors.overlay.green()) + ","
                                                             + QString::number(colors.overlay.blue())  + ",90%)");
        styleSheet.replace("%fv_backdrop_rgba%",     "rgba(" + QString::number(colors.folderview_hc2.red())   + ","
                                                             + QString::number(colors.folderview_hc2.green()) + ","
                                                             + QString::number(colors.folderview_hc2.blue())  + ",80%)");
        // do not show separator line if topbar color matches folderview
        if(colors.folderview != colors.folderview_topbar)
            styleSheet.replace("%topbar_border_rgba%", "rgba(0,0,0,14%)");
        else
            styleSheet.replace("%topbar_border_rgba%", colors.folderview.name());

        // --- apply -------------------------------------------------
        qApp->setStyleSheet(styleSheet);
    }
}
//------------------------------------------------------------------------------
void Settings::loadTheme() {
    if(settings->useSystemColorScheme()) {
        setColorScheme(ThemeStore::colorScheme(ColorSchemes::COLORS_SYSTEM));
    } else {
        BaseColorScheme base;
        themeConf->beginGroup("Colors");
        base.background            = QColor(themeConf->value("background",            "#1a1a1a").toString());
        base.background_fullscreen = QColor(themeConf->value("background_fullscreen", "#1a1a1a").toString());
        base.text                  = QColor(themeConf->value("text",                  "#b6b6b6").toString());
        base.icons                 = QColor(themeConf->value("icons",                 "#a4a4a4").toString());
        base.widget                = QColor(themeConf->value("widget",                "#252525").toString());
        base.widget_border         = QColor(themeConf->value("widget_border",         "#2c2c2c").toString());
        base.accent                = QColor(themeConf->value("accent",                "#8c9b81").toString());
        base.folderview            = QColor(themeConf->value("folderview",            "#242424").toString());
        base.folderview_topbar     = QColor(themeConf->value("folderview_topbar",     "#383838").toString());
        base.scrollbar             = QColor(themeConf->value("scrollbar",             "#5a5a5a").toString());
        base.overlay_text          = QColor(themeConf->value("overlay_text",          "#d2d2d2").toString());
        base.overlay               = QColor(themeConf->value("overlay",               "#1a1a1a").toString());
        base.tid                   = themeConf->value("tid", "-1").toInt();
        themeConf->endGroup();
        setColorScheme(ColorScheme(base));
    }
}
void Settings::saveTheme() {
    if(settings->useSystemColorScheme())
        return;
    themeConf->beginGroup("Colors");
    themeConf->setValue("background",            mColorScheme.background.name());
    themeConf->setValue("background_fullscreen", mColorScheme.background_fullscreen.name());
    themeConf->setValue("text",                  mColorScheme.text.name());
    themeConf->setValue("icons",                 mColorScheme.icons.name());
    themeConf->setValue("widget",                mColorScheme.widget.name());
    themeConf->setValue("widget_border",         mColorScheme.widget_border.name());
    themeConf->setValue("accent",                mColorScheme.accent.name());
    themeConf->setValue("folderview",            mColorScheme.folderview.name());
    themeConf->setValue("folderview_topbar",     mColorScheme.folderview_topbar.name());
    themeConf->setValue("scrollbar",             mColorScheme.scrollbar.name());
    themeConf->setValue("overlay_text",          mColorScheme.overlay_text.name());
    themeConf->setValue("overlay",               mColorScheme.overlay.name());
    themeConf->setValue("tid",                   mColorScheme.tid);
    themeConf->endGroup();
}
//------------------------------------------------------------------------------
const ColorScheme& Settings::colorScheme() {
    return mColorScheme;
}
//------------------------------------------------------------------------------
void Settings::setColorScheme(ColorScheme scheme) {
    mColorScheme = scheme;
    loadStylesheet();
}
//------------------------------------------------------------------------------
void Settings::setColorTid(int tid) {
    mColorScheme.tid = tid;
}
//------------------------------------------------------------------------------
void Settings::fillVideoFormats() {
    mVideoFormatsMap.insert("video/webm",       "webm");
    mVideoFormatsMap.insert("video/mp4",        "mp4");
    mVideoFormatsMap.insert("video/mp4",        "m4v");
    mVideoFormatsMap.insert("video/mpeg",       "mpg");
    mVideoFormatsMap.insert("video/mpeg",       "mpeg");
    mVideoFormatsMap.insert("video/x-matroska", "mkv");
    mVideoFormatsMap.insert("video/x-ms-wmv",   "wmv");
    mVideoFormatsMap.insert("video/x-msvideo",  "avi");
    mVideoFormatsMap.insert("video/quicktime",  "mov");
    mVideoFormatsMap.insert("video/x-flv",      "flv");
}
//------------------------------------------------------------------------------
QString Settings::mpvBinary() {
    QString mpvPath = settings->settingsConf->value("mpvBinary", "").toString();
    if(!QFile::exists(mpvPath)) {
    #ifdef _WIN32
        mpvPath = QCoreApplication::applicationDirPath() + "/mpv.exe";
    #elif defined __linux__
        mpvPath = "/usr/bin/mpv";
    #elif defined __FreeBSD__
        mpvPath = "/usr/local/bin/mpv";
    #endif
        if(!QFile::exists(mpvPath))
            mpvPath = "";
    }
    return mpvPath;
}

void Settings::setMpvBinary(QString path) {
    if(QFile::exists(path)) {
        settings->settingsConf->setValue("mpvBinary", path);
    }
}
//------------------------------------------------------------------------------
QList<QByteArray> Settings::supportedFormats() {
    auto formats = QImageReader::supportedImageFormats();
    formats << "jfif";
    if(videoPlayback())
        formats << videoFormats().values();
    formats.removeAll("pdf");
    return formats;
}
//------------------------------------------------------------------------------
// (for open/save dialogs, as a single string)
// example:  "Images (*.jpg, *.png)"
QString Settings::supportedFormatsFilter() {
    QString filters;
    auto formats = supportedFormats();
    filters.append("Supported files (");
    for(int i = 0; i < formats.count(); i++)
        filters.append("*." + QString(formats.at(i)) + " ");
    filters.append(")");
    return filters;
}
//------------------------------------------------------------------------------
QString Settings::supportedFormatsRegex() {
    QString filter;
    QList<QByteArray> formats = supportedFormats();
    filter.append(".*\\.(");
    for(int i = 0; i < formats.count(); i++)
        filter.append(QString(formats.at(i)) + "|");
    filter.chop(1);
    filter.append(")$");
    return filter;
}
//------------------------------------------------------------------------------
// returns list of mime types
QStringList Settings::supportedMimeTypes() {
    QStringList filters;
    QList<QByteArray> mimeTypes = QImageReader::supportedMimeTypes();
    if(videoPlayback())
        mimeTypes << videoFormats().keys();
    for(int i = 0; i < mimeTypes.count(); i++) {
        filters << QString(mimeTypes.at(i));
    }
    return filters;
}
//------------------------------------------------------------------------------
bool Settings::videoPlayback() {
#ifdef USE_MPV
    return videoPlaybackEnabled();
#else
    return false;
#endif
}

bool Settings::videoPlaybackEnabled() {
    return settings->settingsConf->value("videoPlayback", false).toBool();
}

void Settings::setVideoPlayback(bool mode) {
    settings->settingsConf->setValue("videoPlayback", mode);
}

bool Settings::allowMp4() {
    return settings->settingsConf->value("allowMp4", false).toBool();
}

void Settings::setAllowMp4(bool mode) {
    settings->settingsConf->setValue("allowMp4", mode);
}
//------------------------------------------------------------------------------
BackgroundPattern Settings::backgroundPattern() {
    int mode = settings->settingsConf->value("backgroundPattern", BG_PATTERN_GRID).toInt();
    if(mode < BG_PATTERN_NONE || mode > BG_PATTERN_CHECKER)
        mode = BG_PATTERN_GRID;
    return static_cast<BackgroundPattern>(mode);
}

void Settings::setBackgroundPattern(BackgroundPattern mode) {
    settings->settingsConf->setValue("backgroundPattern", mode);
}

int Settings::patternSize() {
    bool ok = true;
    int size = settings->settingsConf->value("patternSize", 24).toInt(&ok);
    return ok ? qBound(8, size, 128) : 24;
}

void Settings::setPatternSize(int size) {
    settings->settingsConf->setValue("patternSize", qBound(8, size, 128));
}

int Settings::patternOpacity() {
    bool ok = true;
    int percent = settings->settingsConf->value("patternOpacity", 18).toInt(&ok);
    return ok ? qBound(1, percent, 100) : 18;
}

void Settings::setPatternOpacity(int percent) {
    settings->settingsConf->setValue("patternOpacity", qBound(1, percent, 100));
}

QColor Settings::patternColor() {
    QString name = settings->settingsConf->value("patternColor", "").toString();
    return name.isEmpty() ? QColor() : QColor(name);
}

void Settings::setPatternColor(QColor color) {
    settings->settingsConf->setValue("patternColor", color.isValid() ? color.name() : QString());
}
//------------------------------------------------------------------------------
bool Settings::useSystemColorScheme() {
    return settings->settingsConf->value("useSystemColorScheme", false).toBool();
}

void Settings::setUseSystemColorScheme(bool mode) {
    settings->settingsConf->setValue("useSystemColorScheme", mode);
}
//------------------------------------------------------------------------------
QVersionNumber Settings::lastVersion() {
    int vmajor = settings->settingsConf->value("lastVerMajor", 0).toInt();
    int vminor = settings->settingsConf->value("lastVerMinor", 0).toInt();
    int vmicro = settings->settingsConf->value("lastVerMicro", 0).toInt();
    return QVersionNumber(vmajor, vminor, vmicro);
}

void Settings::setLastVersion(QVersionNumber &ver) {
    settings->settingsConf->setValue("lastVerMajor", ver.majorVersion());
    settings->settingsConf->setValue("lastVerMinor", ver.minorVersion());
    settings->settingsConf->setValue("lastVerMicro", ver.microVersion());
}
//------------------------------------------------------------------------------
void Settings::setShowChangelogs(bool mode) {
    settings->settingsConf->setValue("showChangelogs", mode);
}

bool Settings::showChangelogs() {
    return settings->settingsConf->value("showChangelogs", true).toBool();
}
//------------------------------------------------------------------------------
qreal Settings::backgroundOpacity() {
    bool ok = false;
    qreal value = settings->settingsConf->value("backgroundOpacity", 1.0).toReal(&ok);
    if(!ok)
        return 0.0;
    if(value > 1.0)
        return 1.0;
    if(value < 0.0)
        return 0.0;
    return value;
}

void Settings::setBackgroundOpacity(qreal value) {
    if(value > 1.0)
        value = 1.0;
    else if(value < 0.0)
        value = 0.0;
    settings->settingsConf->setValue("backgroundOpacity", value);
}
//------------------------------------------------------------------------------
bool Settings::blurBackground() {
#ifndef USE_KDE_BLUR
    return false;
#endif
    return settings->settingsConf->value("blurBackground", true).toBool();
}

void Settings::setBlurBackground(bool mode) {
    settings->settingsConf->setValue("blurBackground", mode);
}
//------------------------------------------------------------------------------
void Settings::setSortingMode(SortingMode mode) {
    if(mode >= 6)
        mode = SortingMode::SORT_NAME;
    settings->settingsConf->setValue("sortingMode", mode);
}

SortingMode Settings::sortingMode() {
    int mode = settings->settingsConf->value("sortingMode", 0).toInt();
    if(mode < 0 || mode >= 6)
        mode = 0;
    return static_cast<SortingMode>(mode);
}
//------------------------------------------------------------------------------
bool Settings::playVideoSounds() {
    return settings->settingsConf->value("playVideoSounds", false).toBool();
}

void Settings::setPlayVideoSounds(bool mode) {
    settings->settingsConf->setValue("playVideoSounds", mode);
}
//------------------------------------------------------------------------------
void Settings::setVolume(int vol) {
    settings->stateConf->setValue("volume", vol);
}

int Settings::volume() {
    return settings->stateConf->value("volume", 100).toInt();
}
//------------------------------------------------------------------------------
FolderViewMode Settings::folderViewMode() {
    int mode = settings->settingsConf->value("folderViewMode", 2).toInt();
    if(mode < 0 || mode >= 3)
        mode = 2;
    return static_cast<FolderViewMode>(mode);
}

void Settings::setFolderViewMode(FolderViewMode mode) {
    settings->settingsConf->setValue("folderViewMode", mode);
}
//------------------------------------------------------------------------------
ThumbPanelStyle Settings::thumbPanelStyle() {
    int mode = settings->settingsConf->value("thumbPanelStyle", 1).toInt();
    if(mode < 0 || mode > 1)
        mode = 1;
    return static_cast<ThumbPanelStyle>(mode);
}

void Settings::setThumbPanelStyle(ThumbPanelStyle mode) {
    settings->settingsConf->setValue("thumbPanelStyle", mode);
}
//------------------------------------------------------------------------------
// mp4 / m4v are blocked unless explicitly allowed (both share the video/mp4 mime key)
const QMultiMap<QByteArray, QByteArray> Settings::videoFormats() const {
    QMultiMap<QByteArray, QByteArray> formats = mVideoFormatsMap;
    if(!settings->allowMp4())
        formats.remove("video/mp4");
    return formats;
}
//------------------------------------------------------------------------------
int Settings::panelPreviewsSize() {
    bool ok = true;
    int size = settings->settingsConf->value("panelPreviewsSize", 140).toInt(&ok);
    if(!ok)
        size = 140;
    size = qBound(100, size, 250);
    return size;
}

void Settings::setPanelPreviewsSize(int size) {
    settings->settingsConf->setValue("panelPreviewsSize", size);
}
//------------------------------------------------------------------------------
bool Settings::usePreloader() {
    return settings->settingsConf->value("usePreloader", true).toBool();
}

void Settings::setUsePreloader(bool mode) {
    settings->settingsConf->setValue("usePreloader", mode);
}
//------------------------------------------------------------------------------
bool Settings::keepFitMode() {
    return settings->settingsConf->value("keepFitMode", false).toBool();
}

void Settings::setKeepFitMode(bool mode) {
    settings->settingsConf->setValue("keepFitMode", mode);
}
//------------------------------------------------------------------------------
bool Settings::fullscreenMode() {
    return settings->settingsConf->value("openInFullscreen", false).toBool();
}

void Settings::setFullscreenMode(bool mode) {
    settings->settingsConf->setValue("openInFullscreen", mode);
}
//------------------------------------------------------------------------------
bool Settings::maximizedWindow() {
    return settings->stateConf->value("maximizedWindow", false).toBool();
}

void Settings::setMaximizedWindow(bool mode) {
    settings->stateConf->setValue("maximizedWindow", mode);
}
//------------------------------------------------------------------------------
bool Settings::panelEnabled() {
    return settings->settingsConf->value("panelEnabled", true).toBool();
}

void Settings::setPanelEnabled(bool mode) {
    settings->settingsConf->setValue("panelEnabled", mode);
}
//------------------------------------------------------------------------------
bool Settings::panelFullscreenOnly() {
    return settings->settingsConf->value("panelFullscreenOnly", true).toBool();
}

void Settings::setPanelFullscreenOnly(bool mode) {
    settings->settingsConf->setValue("panelFullscreenOnly", mode);
}
//------------------------------------------------------------------------------
int Settings::lastDisplay() {
    return settings->stateConf->value("lastDisplay", 0).toInt();
}

void Settings::setLastDisplay(int display) {
    settings->stateConf->setValue("lastDisplay", display);
}
//------------------------------------------------------------------------------
PanelPosition Settings::panelPosition() {
    QString posString = settings->settingsConf->value("panelPosition", "top").toString();
    if(posString == "top") {
        return PanelPosition::PANEL_TOP;
    } else if(posString == "bottom") {
        return PanelPosition::PANEL_BOTTOM;
    } else if(posString == "left") {
        return PanelPosition::PANEL_LEFT;
    } else {
        return PanelPosition::PANEL_RIGHT;
    }
}

void Settings::setPanelPosition(PanelPosition pos) {
    QString posString;
    switch(pos) {
        case PANEL_TOP:
            posString = "top";
            break;
        case PANEL_BOTTOM:
            posString = "bottom";
            break;
        case PANEL_LEFT:
            posString = "left";
            break;
        case PANEL_RIGHT:
            posString = "right";
            break;
    }
    settings->settingsConf->setValue("panelPosition", posString);
}
//------------------------------------------------------------------------------
bool Settings::panelPinned() {
    return settings->settingsConf->value("panelPinned", false).toBool();
}

void Settings::setPanelPinned(bool mode) {
    settings->settingsConf->setValue("panelPinned", mode);
}
//------------------------------------------------------------------------------
/*
 * 0: fit window
 * 1: fit width
 * 2: orginal size
 * 3: fit window (stretch)
 */
ImageFitMode Settings::imageFitMode() {
    int mode = settings->settingsConf->value("defaultFitMode", 0).toInt();
    if(mode < 0 || mode > 3) {
        qDebug() << "Settings: Invalid fit mode ( " + QString::number(mode) + " ). Resetting to default.";
        mode = 0;
    }
    return static_cast<ImageFitMode>(mode);
}

void Settings::setImageFitMode(ImageFitMode mode) {
    int modeInt = static_cast<ImageFitMode>(mode);
    if(modeInt < 0 || modeInt > 3) {
        qDebug() << "Settings: Invalid fit mode ( " + QString::number(modeInt) + " ). Resetting to default.";
        modeInt = 0;
    }
    settings->settingsConf->setValue("defaultFitMode", modeInt);
}
//------------------------------------------------------------------------------
QRect Settings::windowGeometry() {
    QRect savedRect = settings->stateConf->value("windowGeometry").toRect();
    if(savedRect.size().isEmpty())
        savedRect.setRect(100, 100, 900, 600);
    return savedRect;
}

void Settings::setWindowGeometry(QRect geometry) {
    settings->stateConf->setValue("windowGeometry", geometry);
}
//------------------------------------------------------------------------------
bool Settings::loopSlideshow() {
    return settings->settingsConf->value("loopSlideshow", false).toBool();
}

void Settings::setLoopSlideshow(bool mode) {
    settings->settingsConf->setValue("loopSlideshow", mode);
}
//------------------------------------------------------------------------------
void Settings::sendChangeNotification() {
    emit settingsChanged();
}
//------------------------------------------------------------------------------
void Settings::readShortcuts(QMap<QString, QString> &shortcuts) {
    settings->settingsConf->beginGroup("Controls");
    QStringList in, pair;
    in = settings->settingsConf->value("shortcuts").toStringList();
    for(int i = 0; i < in.count(); i++) {
        pair = in[i].split("=");
        if(!pair[0].isEmpty() && !pair[1].isEmpty()) {
            if(pair[1].endsWith("eq"))
                pair[1]=pair[1].chopped(2) + "=";
            shortcuts.insert(pair[1], pair[0]);
        }
    }
    settings->settingsConf->endGroup();
}

void Settings::saveShortcuts(const QMap<QString, QString> &shortcuts) {
    settings->settingsConf->beginGroup("Controls");
    QMapIterator<QString, QString> i(shortcuts);
    QStringList out;
    while(i.hasNext()) {
        i.next();
        if(i.key().endsWith("="))
            out << i.value() + "=" + i.key().chopped(1) + "eq";
        else
            out << i.value() + "=" + i.key();
    }
    settings->settingsConf->setValue("shortcuts", out);
    settings->settingsConf->endGroup();
}
//------------------------------------------------------------------------------
void Settings::readScripts(QMap<QString, Script> &scripts) {
    scripts.clear();
    settings->settingsConf->beginGroup("Scripts");
    int size = settings->settingsConf->beginReadArray("script");
    for(int i=0; i < size; i++) {
        settings->settingsConf->setArrayIndex(i);
        QString name = settings->settingsConf->value("name").toString();
        QVariant value = settings->settingsConf->value("value");
        Script scr = value.value<Script>();
        scripts.insert(name, scr);
    }
    settings->settingsConf->endArray();
    settings->settingsConf->endGroup();
}

void Settings::saveScripts(const QMap<QString, Script> &scripts) {
    settings->settingsConf->beginGroup("Scripts");
    settings->settingsConf->beginWriteArray("script");
    QMapIterator<QString, Script> i(scripts);
    int counter = 0;
    while(i.hasNext()) {
        i.next();
        settings->settingsConf->setArrayIndex(counter);
        settings->settingsConf->setValue("name", i.key());
        settings->settingsConf->setValue("value", QVariant::fromValue(i.value()));
        counter++;
    }
    settings->settingsConf->endArray();
    settings->settingsConf->endGroup();
}
//------------------------------------------------------------------------------
bool Settings::squareThumbnails() {
    return settings->settingsConf->value("squareThumbnails", false).toBool();
}

void Settings::setSquareThumbnails(bool mode) {
    settings->settingsConf->setValue("squareThumbnails", mode);
}
//------------------------------------------------------------------------------
bool Settings::transparencyGrid() {
    return settings->settingsConf->value("drawTransparencyGrid", false).toBool();
}

void Settings::setTransparencyGrid(bool mode) {
    settings->settingsConf->setValue("drawTransparencyGrid", mode);
}
//------------------------------------------------------------------------------
bool Settings::enableSmoothScroll() {
    return settings->settingsConf->value("enableSmoothScroll", true).toBool();
}

void Settings::setEnableSmoothScroll(bool mode) {
    settings->settingsConf->setValue("enableSmoothScroll", mode);
}
//------------------------------------------------------------------------------
bool Settings::useThumbnailCache() {
    return settings->settingsConf->value("thumbnailCache", true).toBool();
}

void Settings::setUseThumbnailCache(bool mode) {
    settings->settingsConf->setValue("thumbnailCache", mode);
}
//------------------------------------------------------------------------------
QStringList Settings::savedPaths() {
    return settings->stateConf->value("savedPaths", QDir::homePath()).toStringList();
}

void Settings::setSavedPaths(QStringList paths) {
    settings->stateConf->setValue("savedPaths", paths);
}
//------------------------------------------------------------------------------
QStringList Settings::bookmarks() {
    return settings->stateConf->value("bookmarks").toStringList();
}

void Settings::setBookmarks(QStringList paths) {
    settings->stateConf->setValue("bookmarks", paths);
    settings->stateConf->sync(); // write now, do not wait for a clean exit
}
//------------------------------------------------------------------------------
bool Settings::placesPanel() {
    return settings->stateConf->value("placesPanel", true).toBool();
}

void Settings::setPlacesPanel(bool mode) {
    settings->stateConf->setValue("placesPanel", mode);
}
//------------------------------------------------------------------------------
bool Settings::placesPanelBookmarksExpanded() {
    return settings->stateConf->value("placesPanelBookmarksExpanded", true).toBool();
}

void Settings::setPlacesPanelBookmarksExpanded(bool mode) {
    settings->stateConf->setValue("placesPanelBookmarksExpanded", mode);
}
//------------------------------------------------------------------------------
bool Settings::placesPanelTreeExpanded() {
    return settings->stateConf->value("placesPanelTreeExpanded", true).toBool();
}

void Settings::setPlacesPanelTreeExpanded(bool mode) {
    settings->stateConf->setValue("placesPanelTreeExpanded", mode);
}
//------------------------------------------------------------------------------
int Settings::placesPanelWidth() {
    return settings->stateConf->value("placesPanelWidth", 260).toInt();
}

void Settings::setPlacesPanelWidth(int width) {
    settings->stateConf->setValue("placesPanelWidth", width);
}
//------------------------------------------------------------------------------
void Settings::setSlideshowInterval(int ms) {
    settings->settingsConf->setValue("slideshowInterval", ms);
}

int Settings::slideshowInterval() {
    int interval = settings->settingsConf->value("slideshowInterval", 3000).toInt();
    if(interval <= 0)
        interval = 3000;
    return qBound(500, interval, 120000);
}

int Settings::slideshowTransition() {
    return qBound(0, settings->settingsConf->value("slideshowTransition", 1).toInt(), int(TRANSITION_COUNT) - 1);
}

void Settings::setSlideshowTransition(int style) {
    settings->settingsConf->setValue("slideshowTransition", style);
}

bool Settings::slideshowShowName() {
    return settings->settingsConf->value("slideshowShowName", false).toBool();
}

void Settings::setSlideshowShowName(bool mode) {
    settings->settingsConf->setValue("slideshowShowName", mode);
}

bool Settings::slideshowShowDate() {
    return settings->settingsConf->value("slideshowShowDate", false).toBool();
}

void Settings::setSlideshowShowDate(bool mode) {
    settings->settingsConf->setValue("slideshowShowDate", mode);
}

int Settings::slideshowCaptionScale() {
    return qBound(75, settings->settingsConf->value("slideshowCaptionScale", 100).toInt(), 300);
}

void Settings::setSlideshowCaptionScale(int percent) {
    settings->settingsConf->setValue("slideshowCaptionScale", qBound(75, percent, 300));
}

int Settings::slideshowTransitionDuration() {
    return qBound(80, settings->settingsConf->value("slideshowTransitionDuration", 400).toInt(), 3000);
}

void Settings::setSlideshowTransitionDuration(int ms) {
    settings->settingsConf->setValue("slideshowTransitionDuration", qBound(80, ms, 3000));
}

QString Settings::slideshowTransitionEase() {
    return settings->settingsConf->value("slideshowTransitionEase", "0.65,0,0.35,1").toString();
}

void Settings::setSlideshowTransitionEase(const QString &curve) {
    settings->settingsConf->setValue("slideshowTransitionEase", curve);
}

int Settings::slideshowTransitionStrength() {
    return qBound(0, settings->settingsConf->value("slideshowTransitionStrength", 50).toInt(), 100);
}

void Settings::setSlideshowTransitionStrength(int percent) {
    settings->settingsConf->setValue("slideshowTransitionStrength", qBound(0, percent, 100));
}

int Settings::slideshowTransitionSoftness() {
    return qBound(0, settings->settingsConf->value("slideshowTransitionSoftness", 30).toInt(), 100);
}

void Settings::setSlideshowTransitionSoftness(int percent) {
    settings->settingsConf->setValue("slideshowTransitionSoftness", qBound(0, percent, 100));
}

int Settings::slideshowTransitionBlockSize() {
    return qBound(16, settings->settingsConf->value("slideshowTransitionBlockSize", 64).toInt(), 256);
}

void Settings::setSlideshowTransitionBlockSize(int px) {
    settings->settingsConf->setValue("slideshowTransitionBlockSize", qBound(16, px, 256));
}

int Settings::slideshowTransitionDirection() {
    return qBound(0, settings->settingsConf->value("slideshowTransitionDirection", 0).toInt(), int(TRANSITION_DIR_DOWN));
}

void Settings::setSlideshowTransitionDirection(int direction) {
    settings->settingsConf->setValue("slideshowTransitionDirection", direction);
}

QColor Settings::slideshowTransitionDipColor() {
    QColor color(settings->settingsConf->value("slideshowTransitionDipColor", "#000000").toString());
    return color.isValid() ? color : QColor(Qt::black);
}

void Settings::setSlideshowTransitionDipColor(const QColor &color) {
    settings->settingsConf->setValue("slideshowTransitionDipColor", color.isValid() ? color.name() : QString("#000000"));
}

bool Settings::slideshowTransitionRandom() {
    return settings->settingsConf->value("slideshowTransitionRandom", false).toBool();
}

void Settings::setSlideshowTransitionRandom(bool mode) {
    settings->settingsConf->setValue("slideshowTransitionRandom", mode);
}

// the style, caption toggles and the timer are left alone
void Settings::resetSlideshowTransitionOptions() {
    for(const char *key : { "slideshowTransitionDuration", "slideshowTransitionEase", "slideshowTransitionStrength",
                            "slideshowTransitionSoftness", "slideshowTransitionBlockSize", "slideshowTransitionDirection",
                            "slideshowTransitionDipColor", "slideshowTransitionRandom", "slideshowCaptionScale" })
        settings->settingsConf->remove(key);
}
//------------------------------------------------------------------------------
QStringList Settings::defaultFontFamilies() {
    return { "Consolas", "DejaVu Sans Mono", "Menlo", "Monospace" };
}

bool Settings::fontInstalled(const QString &family) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QStringList families = QFontDatabase::families();
#else
    const QStringList families = QFontDatabase().families();
#endif
    for(const QString &name : families) {
        if(name.compare(family, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

// Sets the application font: the chosen family, else the default family list (Consolas + monospace
// fallbacks). The system point size is kept so the stylesheet metrics stay sane. Empty = system font.
void Settings::applyInterfaceFont(const QString &family) {
    static const QFont systemFont = QApplication::font(); // captured on the first (startup) call
    QFont font = systemFont;
    if(font.pointSizeF() <= 0) // pixel sized system font: the QSS math needs points
        font.setPointSize(9);
    if(!family.isEmpty()) {
        QStringList families;
        if(fontInstalled(family))
            families << family;
        for(const QString &fallback : defaultFontFamilies()) {
            if(!families.contains(fallback, Qt::CaseInsensitive) && fontInstalled(fallback))
                families << fallback;
        }
        if(!families.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
            font.setFamilies(families);
#else
            font.setFamily(families.first());
#endif
            if(!fontInstalled(family))
                font.setStyleHint(QFont::Monospace);
        }
    }
    QApplication::setFont(font);
}

QString Settings::interfaceFont() {
    return settings->settingsConf->value("interfaceFont", "Consolas").toString();
}

void Settings::setInterfaceFont(const QString &family) {
    settings->settingsConf->setValue("interfaceFont", family);
}
//------------------------------------------------------------------------------
bool Settings::rulersEnabled() {
    return settings->settingsConf->value("rulersEnabled", false).toBool();
}

void Settings::setRulersEnabled(bool mode) {
    settings->settingsConf->setValue("rulersEnabled", mode);
}

int Settings::rulerUnit() {
    return qBound(0, settings->settingsConf->value("rulerUnit", RULER_PX).toInt(), static_cast<int>(RULER_MIXED));
}

void Settings::setRulerUnit(int unit) {
    settings->settingsConf->setValue("rulerUnit", qBound(0, unit, static_cast<int>(RULER_MIXED)));
}

int Settings::rulerMixedUnit() {
    return settings->settingsConf->value("rulerMixedUnit", RULER_CM).toInt() == RULER_IN ? RULER_IN : RULER_CM;
}

void Settings::setRulerMixedUnit(int unit) {
    settings->settingsConf->setValue("rulerMixedUnit", unit == RULER_IN ? RULER_IN : RULER_CM);
}

int Settings::rulerDpi() {
    return qBound(30, settings->settingsConf->value("rulerDpi", 96).toInt(), 2400);
}

void Settings::setRulerDpi(int dpi) {
    settings->settingsConf->setValue("rulerDpi", qBound(30, dpi, 2400));
}

bool Settings::rulerUseImageDpi() {
    return settings->settingsConf->value("rulerUseImageDpi", true).toBool();
}

void Settings::setRulerUseImageDpi(bool mode) {
    settings->settingsConf->setValue("rulerUseImageDpi", mode);
}

bool Settings::rulerSnap() {
    return settings->settingsConf->value("rulerSnap", false).toBool();
}

void Settings::setRulerSnap(bool mode) {
    settings->settingsConf->setValue("rulerSnap", mode);
}

int Settings::rulerSnapStep() {
    return qBound(1, settings->settingsConf->value("rulerSnapStep", 10).toInt(), 10000);
}

void Settings::setRulerSnapStep(int step) {
    settings->settingsConf->setValue("rulerSnapStep", qBound(1, step, 10000));
}

QStringList Settings::rulerGuides(const QString &key) {
    return settings->stateConf->value("rulerGuides/" + key).toStringList();
}

void Settings::setRulerGuides(const QString &key, const QStringList &guides) {
    if(guides.isEmpty())
        settings->stateConf->remove("rulerGuides/" + key);
    else
        settings->stateConf->setValue("rulerGuides/" + key, guides);
}
//------------------------------------------------------------------------------
int Settings::thumbnailerThreadCount() {
    int count = settings->settingsConf->value("thumbnailerThreads", 4).toInt();
    if(count < 1)
        count = 4;
    return count;
}

void Settings::setThumbnailerThreadCount(int count) {
    settings->settingsConf->setValue("thumbnailerThreads", count);
}
//------------------------------------------------------------------------------
bool Settings::smoothUpscaling() {
    return settings->settingsConf->value("smoothUpscaling", true).toBool();
}

void Settings::setSmoothUpscaling(bool mode) {
    settings->settingsConf->setValue("smoothUpscaling", mode);
}
//------------------------------------------------------------------------------
int Settings::folderViewIconSize() {
    return settings->settingsConf->value("folderViewIconSize", 120).toInt();
}

void Settings::setFolderViewIconSize(int value) {
    settings->settingsConf->setValue("folderViewIconSize", value);
}
//------------------------------------------------------------------------------
bool Settings::expandImage() {
    return settings->settingsConf->value("expandImage", false).toBool();
}

void Settings::setExpandImage(bool mode) {
    settings->settingsConf->setValue("expandImage", mode);
}
//------------------------------------------------------------------------------
int Settings::expandLimit() {
    return settings->settingsConf->value("expandLimit", 2).toInt();
}

void Settings::setExpandLimit(int value) {
    settings->settingsConf->setValue("expandLimit", value);
}
//------------------------------------------------------------------------------
int Settings::JPEGSaveQuality() {
    int quality = std::clamp(settings->settingsConf->value("JPEGSaveQuality", 95).toInt(), 0, 100);
    return quality;
}

void Settings::setJPEGSaveQuality(int value) {
    settings->settingsConf->setValue("JPEGSaveQuality", value);
}
//------------------------------------------------------------------------------
ScalingFilter Settings::scalingFilter() {
    int defaultFilter = 1;
#ifdef USE_OPENCV
    // default to a nicer QI_FILTER_CV_CUBIC
    defaultFilter = 3;
#endif
    int mode = settings->settingsConf->value("scalingFilter", defaultFilter).toInt();
#ifndef USE_OPENCV
    if(mode > 2)
        mode = 1;
#endif
    if(mode < 0 || mode > 4)
        mode = 1;
    return static_cast<ScalingFilter>(mode);
}

void Settings::setScalingFilter(ScalingFilter mode) {
    settings->settingsConf->setValue("scalingFilter", mode);
}
//------------------------------------------------------------------------------
bool Settings::smoothAnimatedImages() {
    return settings->settingsConf->value("smoothAnimatedImages", true).toBool();
}

void Settings::setSmoothAnimatedImages(bool mode) {
    settings->settingsConf->setValue("smoothAnimatedImages", mode);
}
//------------------------------------------------------------------------------
bool Settings::infoBarFullscreen() {
    return settings->settingsConf->value("infoBarFullscreen", true).toBool();
}

void Settings::setInfoBarFullscreen(bool mode) {
    settings->settingsConf->setValue("infoBarFullscreen", mode);
}
//------------------------------------------------------------------------------
bool Settings::infoBarWindowed() {
    return settings->settingsConf->value("infoBarWindowed", false).toBool();
}

void Settings::setInfoBarWindowed(bool mode) {
    settings->settingsConf->setValue("infoBarWindowed", mode);
}
//------------------------------------------------------------------------------
bool Settings::topBarEnabled() {
    return settings->settingsConf->value("topBarEnabled", true).toBool();
}

void Settings::setTopBarEnabled(bool mode) {
    settings->settingsConf->setValue("topBarEnabled", mode);
}

bool Settings::topBarPerformance() {
    return settings->settingsConf->value("topBarPerformance", true).toBool();
}

void Settings::setTopBarPerformance(bool mode) {
    settings->settingsConf->setValue("topBarPerformance", mode);
}
//------------------------------------------------------------------------------
bool Settings::windowTitleExtendedInfo() {
    return settings->settingsConf->value("windowTitleExtendedInfo", true).toBool();
}

void Settings::setWindowTitleExtendedInfo(bool mode) {
    settings->settingsConf->setValue("windowTitleExtendedInfo", mode);
}

//------------------------------------------------------------------------------
bool Settings::cursorAutohide() {
    return settings->settingsConf->value("cursorAutohiding", true).toBool();
}

void Settings::setCursorAutohide(bool mode) {
    settings->settingsConf->setValue("cursorAutohiding", mode);
}
//------------------------------------------------------------------------------
bool Settings::firstRun() {
    return settings->settingsConf->value("firstRun", true).toBool();
}

void Settings::setFirstRun(bool mode) {
    settings->settingsConf->setValue("firstRun", mode);
}
//------------------------------------------------------------------------------
bool Settings::showSaveOverlay() {
    return settings->settingsConf->value("showSaveOverlay", true).toBool();
}

void Settings::setShowSaveOverlay(bool mode) {
    settings->settingsConf->setValue("showSaveOverlay", mode);
}
//------------------------------------------------------------------------------
bool Settings::confirmDelete() {
    return settings->settingsConf->value("confirmDelete", true).toBool();
}

void Settings::setConfirmDelete(bool mode) {
    settings->settingsConf->setValue("confirmDelete", mode);
}
//------------------------------------------------------------------------------
bool Settings::confirmTrash() {
    return settings->settingsConf->value("confirmTrash", true).toBool();
}

void Settings::setConfirmTrash(bool mode) {
    settings->settingsConf->setValue("confirmTrash", mode);
}
//------------------------------------------------------------------------------
bool Settings::unloadThumbs() {
    return settings->settingsConf->value("unloadThumbs", true).toBool();
}

void Settings::setUnloadThumbs(bool mode) {
    settings->settingsConf->setValue("unloadThumbs", mode);
}
//------------------------------------------------------------------------------
float Settings::zoomStep() {
    bool ok = false;
    float value = settings->settingsConf->value("zoomStep", 0.2f).toFloat(&ok);
    if(!ok)
        return 0.2f;
    value = qBound(0.01f, value, 0.5f);
    return value;
}

void Settings::setZoomStep(float value) {
    value = qBound(0.01f, value, 0.5f);
    settings->settingsConf->setValue("zoomStep", value);
}
//------------------------------------------------------------------------------
float Settings::mouseScrollingSpeed() {
    bool ok = false;
    float value = settings->settingsConf->value("mouseScrollingSpeed", 1.0f).toFloat(&ok);
    if(!ok)
        return 1.0f;
    value = qBound(0.5f, value, 2.0f);
    return value;
}

void Settings::setMouseScrollingSpeed(float value) {
    value = qBound(0.5f, value, 2.0f);
    settings->settingsConf->setValue("mouseScrollingSpeed", value);
}
//------------------------------------------------------------------------------
void Settings::setZoomIndicatorMode(ZoomIndicatorMode mode) {
    settings->settingsConf->setValue("zoomIndicatorMode", mode);
}

ZoomIndicatorMode Settings::zoomIndicatorMode() {
    int mode = settings->settingsConf->value("zoomIndicatorMode", 0).toInt();
    if(mode < 0 || mode > 2)
        mode = 0;
    return static_cast<ZoomIndicatorMode>(mode);
}
//------------------------------------------------------------------------------
void Settings::setFocusPointIn1to1Mode(ImageFocusPoint mode) {
    settings->settingsConf->setValue("focusPointIn1to1Mode", mode);
}

ImageFocusPoint Settings::focusPointIn1to1Mode() {
    int mode = settings->settingsConf->value("focusPointIn1to1Mode", 1).toInt();
    if(mode < 0 || mode > 2)
        mode = 1;
    return static_cast<ImageFocusPoint>(mode);
}

void Settings::setDefaultCropAction(DefaultCropAction mode) {
    settings->settingsConf->setValue("defaultCropAction", mode);
}

DefaultCropAction Settings::defaultCropAction() {
    int mode = settings->settingsConf->value("defaultCropAction", 0).toInt();
    if(mode < 0 || mode > 1)
        mode = 0;
    return static_cast<DefaultCropAction>(mode);
}

ImageScrolling Settings::imageScrolling() {
    int mode = settings->settingsConf->value("imageScrolling", 1).toInt();
    if(mode < 0 || mode > 2)
        mode = 0;
    return static_cast<ImageScrolling>(mode);
}

void Settings::setImageScrolling(ImageScrolling mode) {
    settings->settingsConf->setValue("imageScrolling", mode);
}
//------------------------------------------------------------------------------
ViewMode Settings::defaultViewMode() {
    int mode = settings->settingsConf->value("defaultViewMode", 0).toInt();
    if(mode < 0 || mode > 1)
        mode = 0;
    return static_cast<ViewMode>(mode);
}

void Settings::setDefaultViewMode(ViewMode mode) {
    settings->settingsConf->setValue("defaultViewMode", mode);
}
//------------------------------------------------------------------------------
FolderEndAction Settings::folderEndAction() {
    int mode = settings->settingsConf->value("folderEndAction", 0).toInt();
    if(mode < 0 || mode > 2)
        mode = 0;
    return static_cast<FolderEndAction>(mode);
}

void Settings::setFolderEndAction(FolderEndAction mode) {
    settings->settingsConf->setValue("folderEndAction", mode);
}
//------------------------------------------------------------------------------
bool Settings::printLandscape() {
    return stateConf->value("printLandscape", false).toBool();
}

void Settings::setPrintLandscape(bool mode) {
    stateConf->setValue("printLandscape", mode);
}
//------------------------------------------------------------------------------
bool Settings::printPdfDefault() {
    return stateConf->value("printPdfDefault", false).toBool();
}

void Settings::setPrintPdfDefault(bool mode) {
    stateConf->setValue("printPdfDefault", mode);
}
//------------------------------------------------------------------------------
bool Settings::printColor() {
    return stateConf->value("printColor", false).toBool();
}

void Settings::setPrintColor(bool mode) {
    stateConf->setValue("printColor", mode);
}
//------------------------------------------------------------------------------
bool Settings::printFitToPage() {
    return stateConf->value("printFitToPage", true).toBool();
}

void Settings::setPrintFitToPage(bool mode) {
    stateConf->setValue("printFitToPage", mode);
}
//------------------------------------------------------------------------------
QString Settings::lastPrinter() {
    return stateConf->value("lastPrinter", "").toString();
}

void Settings::setLastPrinter(QString name) {
    stateConf->setValue("lastPrinter", name);
}
//------------------------------------------------------------------------------
bool Settings::jxlAnimation() {
    return settings->settingsConf->value("jxlAnimation", false).toBool();
}

void Settings::setJxlAnimation(bool mode) {
    settings->settingsConf->setValue("jxlAnimation", mode);
}
//------------------------------------------------------------------------------
// editor layout: index of CollageLayout::Mode (0 = mosaic, 4 = freehand)
int Settings::collageEditLayout() {
    return qBound(0, settings->settingsConf->value("collageEditLayout", 0).toInt(), 4);
}

void Settings::setCollageEditLayout(int mode) {
    settings->settingsConf->setValue("collageEditLayout", mode);
}

// collage view canvas: "window" or "<w>x<h>". Older versions stored an index into a list of shapes
QString Settings::collageViewCanvas() {
    QSettings *conf = settings->settingsConf;
    if(conf->contains("collageViewShape")) {
        static const char *legacy[] = { "window", "1920x1080", "1080x1350", "1080x1920", "1080x1080" };
        int index = qBound(0, conf->value("collageViewShape", 0).toInt(), 4);
        conf->remove("collageViewShape");
        if(!conf->contains("collageViewCanvas"))
            conf->setValue("collageViewCanvas", QString(legacy[index]));
    }
    return conf->value("collageViewCanvas", "window").toString();
}

void Settings::setCollageViewCanvas(const QString &canvas) {
    settings->settingsConf->setValue("collageViewCanvas", canvas);
}

bool Settings::collageViewThemeBackground() {
    return settings->settingsConf->value("collageViewThemeBackground", true).toBool();
}

void Settings::setCollageViewThemeBackground(bool mode) {
    settings->settingsConf->setValue("collageViewThemeBackground", mode);
}

int Settings::collageViewBgOpacity() {
    return qBound(0, settings->settingsConf->value("collageViewBgOpacity", 100).toInt(), 100);
}

void Settings::setCollageViewBgOpacity(int percent) {
    settings->settingsConf->setValue("collageViewBgOpacity", qBound(0, percent, 100));
}

int Settings::collageBorderWidth() {
    return qBound(0, settings->settingsConf->value("collageBorderWidth", 0).toInt(), 64);
}

void Settings::setCollageBorderWidth(int px) {
    settings->settingsConf->setValue("collageBorderWidth", px);
}

QColor Settings::collageBorderColor() {
    QString name = settings->settingsConf->value("collageBorderColor", "").toString();
    return name.isEmpty() ? QColor() : QColor(name);
}

void Settings::setCollageBorderColor(QColor color) {
    settings->settingsConf->setValue("collageBorderColor", color.isValid() ? color.name(QColor::HexArgb) : QString());
}

int Settings::collageBorderStyle() {
    return qBound(0, settings->settingsConf->value("collageBorderStyle", 0).toInt(), 4);
}

void Settings::setCollageBorderStyle(int style) {
    settings->settingsConf->setValue("collageBorderStyle", style);
}

int Settings::collageBorderMode() {
    return qBound(0, settings->settingsConf->value("collageBorderMode", 0).toInt(), 2);
}

void Settings::setCollageBorderMode(int mode) {
    settings->settingsConf->setValue("collageBorderMode", mode);
}

bool Settings::collageConfirmExit() {
    return settings->settingsConf->value("collageConfirmExit", true).toBool();
}

void Settings::setCollageConfirmExit(bool mode) {
    settings->settingsConf->setValue("collageConfirmExit", mode);
}

int Settings::collagePanelWidth() {
    return qBound(260, settings->stateConf->value("collagePanelWidth", 320).toInt(), 480);
}

void Settings::setCollagePanelWidth(int width) {
    settings->stateConf->setValue("collagePanelWidth", width);
}

bool Settings::collageAnimate() {
    return settings->settingsConf->value("collageAnimate", true).toBool();
}

void Settings::setCollageAnimate(bool mode) {
    settings->settingsConf->setValue("collageAnimate", mode);
}
//------------------------------------------------------------------------------
bool Settings::autoResizeWindow() {
    return settings->settingsConf->value("autoResizeWindow", false).toBool();
}

void Settings::setAutoResizeWindow(bool mode) {
    settings->settingsConf->setValue("autoResizeWindow", mode);
}
//------------------------------------------------------------------------------
int Settings::autoResizeLimit() {
    int limit = settings->settingsConf->value("autoResizeLimit", 90).toInt();
    if(limit < 30 || limit > 100)
        limit = 90;
    return limit;
}

void Settings::setAutoResizeLimit(int percent) {
    settings->settingsConf->setValue("autoResizeLimit", percent);
}
//------------------------------------------------------------------------------
int Settings::memoryAllocationLimit() {
    int limit = settings->settingsConf->value("memoryAllocationLimit", 1024).toInt();
    if(limit < 512)
        limit = 512;
    else if(limit > 8192)
        limit = 8192;
    return limit;
}

void Settings::setMemoryAllocationLimit(int limitMB) {
    settings->settingsConf->setValue("memoryAllocationLimit", limitMB);
}
//------------------------------------------------------------------------------
bool Settings::panelCenterSelection() {
    return settings->settingsConf->value("panelCenterSelection", false).toBool();
}

void Settings::setPanelCenterSelection(bool mode) {
    settings->settingsConf->setValue("panelCenterSelection", mode);
}
//------------------------------------------------------------------------------
QString Settings::language() {
    return settingsConf->value("language", "en_US").toString();
}

void Settings::setLanguage(QString lang) {
    settingsConf->setValue("language", lang);
}
//------------------------------------------------------------------------------
bool Settings::useFixedZoomLevels() {
    return settings->settingsConf->value("useFixedZoomLevels", false).toBool();
}

void Settings::setUseFixedZoomLevels(bool mode) {
    settings->settingsConf->setValue("useFixedZoomLevels", mode);
}
//------------------------------------------------------------------------------
QString Settings::defaultZoomLevels() {
    return QString("0.05,0.1,0.125,0.166,0.25,0.333,0.5,0.66,1,1.5,2,3,4,5,6,7,8");
}
QString Settings::zoomLevels() {
    return settingsConf->value("fixedZoomLevels", defaultZoomLevels()).toString();
}

void Settings::setZoomLevels(QString levels) {
    settingsConf->setValue("fixedZoomLevels", levels);
}
//------------------------------------------------------------------------------
bool Settings::unlockMinZoom() {
    return settings->settingsConf->value("unlockMinZoom", true).toBool();
}

void Settings::setUnlockMinZoom(bool mode) {
    settings->settingsConf->setValue("unlockMinZoom", mode);
}
//------------------------------------------------------------------------------
bool Settings::sortFolders() {
    return settings->settingsConf->value("sortFolders", true).toBool();
}

void Settings::setSortFolders(bool mode) {
    settings->settingsConf->setValue("sortFolders", mode);
}
//------------------------------------------------------------------------------
bool Settings::trackpadDetection() {
    return settings->settingsConf->value("trackpadDetection", true).toBool();
}

void Settings::setTrackpadDetection(bool mode) {
    settings->settingsConf->setValue("trackpadDetection", mode);
}
//------------------------------------------------------------------------------
bool Settings::clickableEdges() {
    return settings->settingsConf->value("clickableEdges", false).toBool();
}

void Settings::setClickableEdges(bool mode) {
    settings->settingsConf->setValue("clickableEdges", mode);
}
//------------------------------------------------------------------------------
bool Settings::clickableEdgesVisible() {
    return settings->settingsConf->value("clickableEdgesVisible", true).toBool();
}

void Settings::setClickableEdgesVisible(bool mode) {
    settings->settingsConf->setValue("clickableEdgesVisible", mode);
}
//------------------------------------------------------------------------------
bool Settings::showHiddenFiles() {
    return settings->settingsConf->value("showHiddenFiles", false).toBool();
}

void Settings::setShowHiddenFiles(bool mode) {
    settings->settingsConf->setValue("showHiddenFiles", mode);
}
