#include "beatdialog.h"

#include <QBuffer>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

#include "avatars.h"
#include "beatboard.h"
#include "theme.h"

// ------------------------------------------------------------ imagens

namespace beatimage {

std::string fromImage(QImage img) {
    if (img.isNull()) return {};
    const int maxSide = 720;
    if (img.width() > maxSide || img.height() > maxSide)
        img = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // JPEG não tem transparência: compõe sobre branco.
    QImage flat(img.size(), QImage::Format_RGB32);
    flat.fill(Qt::white);
    QPainter p(&flat);
    p.drawImage(0, 0, img);
    p.end();
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    flat.save(&buf, "JPEG", 86);
    return bytes.toBase64().toStdString();
}

std::string fromFile(const QString& path, QString* error) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        if (error) *error = reader.errorString();
        return {};
    }
    return fromImage(img);
}

QImage decode(const std::shared_ptr<const std::string>& data) {
    if (!data || data->empty()) return {};
    // A chave é o ponteiro; a entrada guarda o shared_ptr, então o ponteiro não é reaproveitado enquanto vale.
    static QHash<const std::string*, QPair<std::shared_ptr<const std::string>, QImage>> cache;
    auto it = cache.find(data.get());
    if (it != cache.end()) return it->second;
    if (cache.size() > 96) cache.clear();
    QImage img;
    img.loadFromData(QByteArray::fromBase64(QByteArray::fromStdString(*data)));
    cache.insert(data.get(), {data, img});
    return img;
}

QString fileFilter() { return avatars::fileFilter(); }

}  // namespace beatimage

// ------------------------------------------------------------ prévia

BeatImagePreview::BeatImagePreview(QWidget* parent) : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(240, 170);
    setToolTip(tr("Click to choose an image"));
}

void BeatImagePreview::setImage(const QImage& img, const QColor& band) {
    image_ = img;
    band_ = band;
    update();
}

void BeatImagePreview::paintEvent(QPaintEvent*) {
    const ThemeColors& c = ThemeManager::instance().colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    QPainterPath clip;
    clip.addRoundedRect(r, 12, 12);
    p.setClipPath(clip);
    if (image_.isNull()) {
        p.fillRect(r, c.dark ? c.surface : QColor(Qt::white));
        p.setClipping(false);
        QPen dash(hover_ ? c.accent : c.border, 1.5, Qt::DashLine);
        p.setPen(dash);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(6, 6, -6, -6), 9, 9);
        const QIcon icon = ThemeManager::instance().icon("imagem", hover_ ? c.accent : c.muted);
        icon.paint(&p, QRect(static_cast<int>(r.center().x()) - 16, static_cast<int>(r.center().y()) - 30, 32, 32));
        p.setPen(hover_ ? c.accent : c.muted);
        p.drawText(QRectF(r.left(), r.center().y() + 8, r.width(), 24), Qt::AlignCenter, tr("Add an image"));
    } else {
        const double scale = std::max(r.width() / image_.width(), r.height() / image_.height());
        const QSizeF src(r.width() / scale, r.height() / scale);
        p.drawImage(r, image_,
                    QRectF((image_.width() - src.width()) / 2, (image_.height() - src.height()) / 2, src.width(),
                           src.height()));
        if (hover_) {
            p.fillRect(QRectF(r.left(), r.bottom() - 34, r.width(), 34), QColor(0, 0, 0, 140));
            p.setPen(Qt::white);
            p.drawText(QRectF(r.left(), r.bottom() - 34, r.width(), 34), Qt::AlignCenter, tr("Change image"));
        }
    }
    if (band_.isValid()) {
        p.setClipPath(clip);
        p.fillRect(QRectF(r.left(), r.top(), r.width(), 6), band_);
    }
}

void BeatImagePreview::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint())) emit clicked();
}

// ------------------------------------------------------------ janela

namespace {
QLabel* sectionLabel(const QString& text) {
    auto* l = new QLabel(text.toUpper());
    l->setObjectName("secao");
    QFont f = l->font();
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
    l->setFont(f);
    return l;
}

QIcon swatchIcon(const QColor& color, bool checked, const ThemeColors& c) {
    QPixmap pm(56, 56);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    if (checked) {
        p.setPen(QPen(c.text, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(1.5, 1.5, 25, 25));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QRectF(5, 5, 18, 18));
    return QIcon(pm);
}
}  // namespace

BeatDialog::BeatDialog(const Beat& beat, QWidget* parent) : QDialog(parent), beat_(beat) {
    setWindowTitle(tr("Beat"));
    setMinimumSize(700, 440);

    const ThemeColors& tc = ThemeManager::instance().colors();

    // ---- Esquerda: imagem
    preview_ = new BeatImagePreview;
    connect(preview_, &BeatImagePreview::clicked, this, &BeatDialog::chooseImage);
    auto* choose = new QPushButton(tr("Choose Image…"));
    choose->setObjectName("link");
    connect(choose, &QPushButton::clicked, this, &BeatDialog::chooseImage);
    removeImage_ = new QPushButton(tr("Remove"));
    removeImage_->setObjectName("perigo");
    connect(removeImage_, &QPushButton::clicked, this, [this] {
        beat_.image.reset();
        refreshPreview();
    });
    auto* imageButtons = new QHBoxLayout;
    imageButtons->addStretch(1);
    imageButtons->addWidget(choose);
    imageButtons->addWidget(removeImage_);
    imageButtons->addStretch(1);
    auto* imageHint = new QLabel(tr("You can also drop images on the board or paste them with Ctrl+V."));
    imageHint->setObjectName("subtituloDialogo");
    imageHint->setWordWrap(true);
    imageHint->setAlignment(Qt::AlignCenter);

    auto* side = new QFrame;
    side->setObjectName("ficha");
    side->setFixedWidth(300);
    auto* sl = new QVBoxLayout(side);
    sl->setContentsMargins(18, 18, 18, 14);
    sl->setSpacing(8);
    preview_->setFixedHeight(200);
    sl->addWidget(preview_);
    sl->addLayout(imageButtons);
    sl->addWidget(imageHint);
    sl->addStretch(1);

    // ---- Direita: texto e cor
    auto* heading = new QLabel(tr("Beat"));
    heading->setObjectName("tituloDialogo");
    auto* sub = new QLabel(tr("An idea, a moment or a turning point. Connect beats on the board with flow lines."));
    sub->setObjectName("subtituloDialogo");
    sub->setWordWrap(true);

    title_ = new QLineEdit(QString::fromStdU32String(beat.title));
    title_->setObjectName("campoTitulo");
    title_->setPlaceholderText(tr("Title, e.g. Helena misses the last show"));
    text_ = new QPlainTextEdit(QString::fromStdU32String(beat.text));
    text_->setPlaceholderText(tr("What happens, why it matters, what changes…"));
    text_->setTabChangesFocus(true);

    colors_ = new QButtonGroup(this);
    colors_->setExclusive(true);
    auto* colorRow = new QHBoxLayout;
    colorRow->setSpacing(2);
    const QStringList palette = BeatBoardView::colors();
    for (int i = 0; i < palette.size(); ++i) {
        auto* b = new QToolButton;
        b->setCheckable(true);
        b->setAutoRaise(true);
        b->setIconSize(QSize(28, 28));
        b->setFixedSize(34, 34);
        b->setProperty("color", palette[i]);
        b->setChecked(palette[i] == QString::fromStdString(beat.color));
        b->setIcon(swatchIcon(QColor(palette[i]), b->isChecked(), tc));
        b->setFocusPolicy(Qt::TabFocus);
        colors_->addButton(b, i);
        colorRow->addWidget(b);
    }
    if (!colors_->checkedButton()) colors_->button(0)->setChecked(true);
    colorRow->addStretch(1);
    connect(colors_, &QButtonGroup::idToggled, this, [this](int, bool) { refreshPreview(); });

    auto* fields = new QVBoxLayout;
    fields->setSpacing(8);
    fields->addWidget(heading);
    fields->addWidget(sub);
    fields->addSpacing(10);
    fields->addWidget(sectionLabel(tr("Title")));
    fields->addWidget(title_);
    fields->addSpacing(6);
    fields->addWidget(sectionLabel(tr("Description")));
    fields->addWidget(text_, 1);
    fields->addSpacing(6);
    fields->addWidget(sectionLabel(tr("Color")));
    fields->addLayout(colorRow);

    auto* columns = new QHBoxLayout;
    columns->setSpacing(26);
    columns->addWidget(side);
    columns->addLayout(fields, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText(tr("Save"));
    bb->button(QDialogButtonBox::Save)->setObjectName("primarioBotao");
    bb->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    for (QAbstractButton* b : bb->buttons()) b->setIcon(QIcon());
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 28, 22);
    root->setSpacing(16);
    root->addLayout(columns, 1);
    root->addWidget(bb);

    refreshPreview();
    title_->setFocus();
    // Só depois de estar na janela: antes disso o QDialog escolheria outro botão padrão.
    bb->button(QDialogButtonBox::Save)->setDefault(true);
}

Beat BeatDialog::beat() const {
    Beat b = beat_;
    b.title = title_->text().trimmed().toStdU32String();
    QString text = text_->toPlainText();
    while (text.endsWith('\n') || text.endsWith(' ')) text.chop(1);
    b.text = text.toStdU32String();
    if (QAbstractButton* c = colors_->checkedButton()) b.color = c->property("color").toString().toStdString();
    return b;
}

void BeatDialog::chooseImage() {
    QSettings settings;
    const QString path = QFileDialog::getOpenFileName(this, tr("Image for the Beat"),
                                                      settings.value("lastImageDir").toString(),
                                                      beatimage::fileFilter());
    if (path.isEmpty()) return;
    settings.setValue("lastImageDir", QFileInfo(path).absolutePath());
    QString err;
    const std::string data = beatimage::fromFile(path, &err);
    if (data.empty()) {
        QMessageBox::warning(this, tr("Image"), tr("Could not open the image:\n%1").arg(err));
        return;
    }
    beat_.image = std::make_shared<const std::string>(data);
    if (beat_.h < 200) beat_.h = 220;
    refreshPreview();
}

void BeatDialog::refreshPreview() {
    const ThemeColors& tc = ThemeManager::instance().colors();
    QColor band(QString::fromStdString(beat_.color));
    if (QAbstractButton* c = colors_->checkedButton()) band = QColor(c->property("color").toString());
    for (QAbstractButton* b : colors_->buttons())
        b->setIcon(swatchIcon(QColor(b->property("color").toString()), b->isChecked(), tc));
    preview_->setImage(beatimage::decode(beat_.image), band);
    removeImage_->setVisible(static_cast<bool>(beat_.image));
}
