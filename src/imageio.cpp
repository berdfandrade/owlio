#include "imageio.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QImageReader>
#include <QStringList>
#include <QUrl>

namespace imageio {

namespace {
const QList<QByteArray> kFormats{"jpeg", "jpg", "png", "webp"};

QString tr(const char* text) { return QCoreApplication::translate("imageio", text); }
}  // namespace

QImage read(QIODevice* device, int maxSide, QString* error) {
    QImageReader reader(device);
    reader.setAutoTransform(true);  // respeita a orientação EXIF de fotos de celular
    if (!kFormats.contains(reader.format().toLower())) {
        if (error) *error = tr("Only JPEG, PNG and WebP images can be used.");
        return {};
    }
    const QSize size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > kMaxPixels) {
        if (error) *error = tr("The image is too large.");
        return {};
    }
    if (size.width() > maxSide || size.height() > maxSide)
        reader.setScaledSize(size.scaled(maxSide, maxSide, Qt::KeepAspectRatio));
    QImage img = reader.read();
    if (img.isNull() && error) *error = reader.errorString();
    return img;
}

QImage readFile(const QString& path, int maxSide, QString* error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return {};
    }
    return read(&f, maxSide, error);
}

QImage fromBase64(const std::string& base64, int maxSide) {
    if (base64.empty() || static_cast<qsizetype>(base64.size()) > kMaxBase64) return {};
    QByteArray bytes = QByteArray::fromBase64(QByteArray::fromStdString(base64));
    QBuffer buf(&bytes);
    buf.open(QIODevice::ReadOnly);
    return read(&buf, maxSide);
}

QString localPath(const QUrl& url) {
    if (!url.isLocalFile() || !url.host().isEmpty()) return {};
    const QString path = url.toLocalFile();
    if (path.startsWith("//") || path.startsWith("\\\\")) return {};
    return path;
}

bool isImagePath(const QString& path) {
    const int dot = path.lastIndexOf('.');
    return dot >= 0 && kFormats.contains(path.mid(dot + 1).toLower().toLatin1());
}

QString fileFilter() {
    QStringList patterns;
    const QList<QByteArray> supported = QImageReader::supportedImageFormats();
    for (const QByteArray& f : kFormats)
        if (supported.contains(f)) patterns << "*." + QString::fromLatin1(f);
    return tr("Images (%1)").arg(patterns.join(' '));
}

}  // namespace imageio
