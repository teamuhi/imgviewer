#include "collageimageloader.h"
#include <QImageReader>
#include <QMetaObject>
#include <QThread>

namespace {

class LoadTask : public QRunnable {
public:
    LoadTask(CollageImageLoader *loader, quint64 id, int generation, const QString &path, int longSide)
        : mLoader(loader), mId(id), mGeneration(generation), mPath(path), mLongSide(longSide) {}

    void run() override {
        QSize original;
        QImage image = CollageImageLoader::readScaled(mPath, mLongSide, &original);
        bool animated = !image.isNull() && CollageImageLoader::isAnimated(mPath);
        CollageImageLoader *loader = mLoader;
        quint64 id = mId;
        int generation = mGeneration;
        // delivered in the loader's (gui) thread
        QMetaObject::invokeMethod(loader, [loader, id, generation, image, original, animated]() {
            emit loader->loaded(id, generation, image, original, animated);
        }, Qt::QueuedConnection);
    }

private:
    CollageImageLoader *mLoader;
    quint64 mId;
    int mGeneration;
    QString mPath;
    int mLongSide;
};

} // namespace

CollageImageLoader::CollageImageLoader(QObject *parent) : QObject(parent) {
    // few workers: every worker may hold a full-size decode for a moment
    mPool.setMaxThreadCount(qBound(1, QThread::idealThreadCount() / 2, 3));
}

CollageImageLoader::~CollageImageLoader() {
    mPool.clear();
    mPool.waitForDone();
}

void CollageImageLoader::load(quint64 itemId, int generation, const QString &path, int longSide) {
    mPool.start(new LoadTask(this, itemId, generation, path, longSide));
}

bool CollageImageLoader::isAnimated(const QString &path) {
    QImageReader reader(path);
    // imageCount() is 0 when the plugin can not tell: treat "animation capable" as animated then,
    // QMovie shows a single frame for those files anyway
    return reader.supportsAnimation() && reader.imageCount() != 1;
}

QSize CollageImageLoader::probeSize(const QString &path) {
    QImageReader reader(path);
    QSize size = reader.size();
    if(size.isValid() && reader.transformation().testFlag(QImageIOHandler::TransformationRotate90))
        size.transpose();
    return size;
}

QImage CollageImageLoader::readScaled(const QString &path, int longSide, QSize *originalSize) {
    QImageReader reader(path);
    reader.setAutoTransform(true);

    QSize raw = reader.size();
    QSize original = raw;
    if(raw.isValid() && reader.transformation().testFlag(QImageIOHandler::TransformationRotate90))
        original.transpose();

    int rawLong = qMax(raw.width(), raw.height());
    if(raw.isValid() && longSide > 0 && rawLong > longSide) {
        qreal scale = static_cast<qreal>(longSide) / rawLong;
        reader.setScaledSize(QSize(qMax(1, qRound(raw.width() * scale)),
                                   qMax(1, qRound(raw.height() * scale))));
    }

    QImage image = reader.read();
    if(image.isNull())
        return QImage();
    if(!original.isValid())
        original = image.size();
    // some plugins ignore the scaled size
    if(longSide > 0 && qMax(image.width(), image.height()) > longSide)
        image = image.scaled(longSide, longSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if(originalSize)
        *originalSize = original;
    return image;
}
