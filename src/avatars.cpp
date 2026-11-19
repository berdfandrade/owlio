#include "avatars.h"

#include <QBuffer>
#include <QCache>
#include <QImage>
#include <QPainterPath>

#include "imageio.h"
#include "theme.h"

namespace avatars {

namespace {

// Fotos decodificadas, pelo hash do base64, até 16 MB. Uma foto inválida também entra (nula, custo 1)
// para não ser decodificada de novo a cada pintura.
QImage decoded(const std::string& base64) {
    static QCache<size_t, QImage> cache(16 * 1024 * 1024);
    const size_t key = std::hash<std::string>{}(base64);
    if (QImage* hit = cache.object(key)) return *hit;
    QImage img = imageio::fromBase64(base64, 256);
    cache.insert(key, new QImage(img), std::max<qsizetype>(1, img.sizeInBytes()));
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
    QImage img = imageio::readFile(path, 1024, error);
    if (img.isNull()) return {};
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

QString fileFilter() { return imageio::fileFilter(); }

}  // namespace avatars
