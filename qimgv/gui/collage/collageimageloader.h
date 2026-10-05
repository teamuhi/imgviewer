#pragma once

#include <QObject>
#include <QImage>
#include <QSize>
#include <QString>
#include <QThreadPool>
#include <QRunnable>
#include <QPointer>

// Reads images already downscaled (QImageReader::setScaledSize), so a big photo
// never sits in memory at full resolution just to be shown in a collage tile.
class CollageImageLoader : public QObject {
    Q_OBJECT
public:
    explicit CollageImageLoader(QObject *parent = nullptr);
    ~CollageImageLoader() override;

    // asynchronous; result arrives through loaded(). "animated" = the file has more than one
    // frame (gif / animated webp / apng); the image is then its first frame.
    void load(quint64 itemId, int generation, const QString &path, int longSide);

    // longSide <= 0: full resolution. originalSize receives the size after EXIF rotation.
    // Thread-safe.
    static QImage readScaled(const QString &path, int longSide, QSize *originalSize = nullptr);

    // true for files with more than one frame. Thread-safe.
    static bool isAnimated(const QString &path);

    // cheap header-only size query (EXIF rotation applied). Invalid size if unreadable.
    static QSize probeSize(const QString &path);

signals:
    void loaded(quint64 itemId, int generation, QImage image, QSize originalSize, bool animated);

private:
    QThreadPool mPool;
};
