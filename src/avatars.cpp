#include "avatars.h"

#include <QBuffer>
#include <QCache>
#include <QCoreApplication>
#include <QImage>
#include <QImageReader>
#include <QPainterPath>
#include <QStringList>

#include "theme.h"

namespace avatars {

namespace {

// Cache de imagens decodificadas: chave = hash do base64.
QImage decoded(const std::string& base64) {
    static QCache<size_t, QImage> cache(64);
    const size_t key = std::hash<std::string>{}(base64);
    if (QImage* hit = cache.object(key)) return *hit;
    QImage img;
    img.loadFromData(QByteArray::fromBase64(QByteArray::fromStdString(base64)));
    if (!img.isNull()) cache.insert(key, new QImage(img));
    return img;
}

QString initials(const QString& name) {
    QString out;
    for (const QString& part : name.split(' ', Qt::SkipEmptyParts)) {
        if (out.size() < 2 && part.at(0).isLetter()) out += part.at(0);
    }
    return out.isEmpty() ? name.left(1) : out;
}

}  // namespace

void draw(QPainter& p, const QRectF& r, const QString& name, const std::string& base64, double ring,
          const QColor& ringColor) {
    const ThemeColors& c = ThemeManager::instance().colors();
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    QImage img = base64.empty() ? QImage() : decoded(base64);
    if (!img.isNull()) {
        QPainterPath clip;
        clip.addEllipse(r);
        p.setClipPath(clip);
        p.drawImage(r, img);
        p.setClipping(false);
    } else {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor::fromHsl(static_cast<int>(qHash(name) % 360), c.dark ? 110 : 140, c.dark ? 120 : 150));
        p.drawEllipse(r);
        QFont f = p.font();
        f.setPixelSize(std::max(7, static_cast<int>(r.height() * 0.46)));
        f.setWeight(QFont::Bold);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, initials(name));
    }
    if (ring > 0) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(ringColor, ring));
        p.drawEllipse(r);
    }
    p.restore();
}

std::string fromImageFile(const QString& path, QString* error) {
    QImageReader reader(path);
    reader.setAutoTransform(true);  // respeita a orientação EXIF de fotos de celular
    QImage img = reader.read();
    if (img.isNull()) {
        if (error) *error = reader.errorString();
        return {};
    }
    const int side = std::min(img.width(), img.height());
    img = img.copy((img.width() - side) / 2, (img.height() - side) / 2, side, side)
              .scaled(192, 192, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
              .convertToFormat(QImage::Format_RGB32);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", 88);
    return bytes.toBase64().toStdString();
}

QString fileFilter() {
    QStringList patterns;
    for (const QByteArray& f : QImageReader::supportedImageFormats()) {
        const QString fmt = QString::fromLatin1(f).toLower();
        if (fmt == "svg" || fmt == "svgz" || fmt == "ico" || fmt == "pbm" || fmt == "pgm" || fmt == "ppm" ||
            fmt == "xbm" || fmt == "xpm" || fmt == "cur")
            continue;
        patterns << "*." + fmt;
    }
    return QCoreApplication::translate("avatars", "Images (%1)").arg(patterns.join(' '));
}

}  // namespace avatars
