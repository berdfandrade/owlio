#include "pixelavatar.h"

#include <QBuffer>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QVBoxLayout>
#include <array>
#include <cmath>

namespace {
constexpr int N = 16;  // a grade do sprite

const char* kSkin[] = {"#f6d5b8", "#f2c49b", "#e0a878", "#c68652", "#8d5a3b", "#5c3a24"};
const char* kHair[] = {"#16161d", "#4a2f1d", "#8a5a2b", "#e3c16f", "#b5482a", "#a8a8b0", "#5d7fd6", "#d06ba8"};
const char* kShirt[] = {"#3d63b5", "#c94f4f", "#3f9e6e", "#e0a43b", "#7b5bc4", "#2b2d36", "#e8e8ec", "#e27a3f"};
const char* kBack[] = {"#e8c9a0", "#bcd7ea", "#c9e4c5", "#f2c6cf", "#d9cff2", "#2f3340", "#f4f4f6", "#f6dd8a"};
const QColor kInk("#15151c");

template <size_t K> int clampIndex(int i, const char* (&)[K]) { return std::clamp(i, 0, int(K) - 1); }

struct Grid {
    QImage img{N, N, QImage::Format_ARGB32};
    Grid() { img.fill(Qt::transparent); }
    void set(int x, int y, const QColor& c) {
        if (x >= 0 && y >= 0 && x < N && y < N) img.setPixelColor(x, y, c);
    }
    void row(int y, int x0, int x1, const QColor& c) { for (int x = x0; x <= x1; ++x) set(x, y, c); }
    void rect(int x0, int y0, int x1, int y1, const QColor& c) { for (int y = y0; y <= y1; ++y) row(y, x0, x1, c); }
    bool filled(int x, int y) const { return x >= 0 && y >= 0 && x < N && y < N && img.pixelColor(x, y).alpha() > 0; }
};

void drawHair(Grid& g, int style, const QColor& h) {
    auto shortTop = [&] {
        g.row(1, 5, 10, h);
        g.row(2, 4, 11, h);
        g.row(3, 3, 12, h);
        g.row(4, 3, 7, h);
        g.set(11, 4, h);
        g.set(12, 4, h);
        g.set(3, 5, h);
        g.set(12, 5, h);
    };
    switch (style) {
        case 1: shortTop(); break;
        case 2:  // bagunçado
            g.row(0, 5, 9, h);
            g.row(1, 3, 12, h);
            g.rect(2, 2, 13, 3, h);
            g.row(4, 2, 5, h);
            g.set(7, 4, h);
            g.row(4, 9, 13, h);
            g.rect(2, 5, 3, 7, h);
            g.rect(12, 5, 13, 7, h);
            g.set(1, 3, h);
            g.set(14, 2, h);
            break;
        case 3:  // comprido
            shortTop();
            g.rect(2, 4, 3, 13, h);
            g.rect(12, 4, 13, 13, h);
            g.set(4, 4, h);
            break;
        case 4:  // chanel
            shortTop();
            g.row(4, 3, 12, h);
            g.rect(3, 5, 4, 9, h);
            g.rect(11, 5, 12, 9, h);
            break;
        case 5:  // coque
            shortTop();
            g.row(0, 6, 9, h);
            break;
        case 6:  // moicano
            g.rect(7, 0, 8, 4, h);
            break;
        case 7:  // black power
            for (int y = 0; y <= 10; ++y)
                for (int x = 1; x <= 14; ++x) {
                    const double dx = x - 7.5, dy = (y - 4.5) * 1.1;
                    if (std::sqrt(dx * dx + dy * dy) > 6.6) continue;
                    if (y >= 4 && x >= 4 && x <= 11) continue;  // o rosto fica à mostra
                    g.set(x, y, h);
                }
            break;
        default: break;  // careca
    }
}
}  // namespace

QImage PixelAvatar::render(int size) const {
    Grid g;
    const QColor skinC(kSkin[clampIndex(skin, kSkin)]);
    const QColor hairC(kHair[clampIndex(hairColor, kHair)]);
    const QColor shirtC(kShirt[clampIndex(shirt, kShirt)]);
    const QColor shade = skinC.darker(118);

    // Corpo: ombros largos, estreitos ou no meio.
    const int half = body == 1 ? 6 : body == 0 ? 4 : 5;
    g.rect(8 - half - 1, 13, 7 + half + 1, 15, shirtC);
    g.row(12, 8 - half, 7 + half, shirtC);
    g.set(8 - half - 1, 13, shirtC.darker(125));
    g.set(7 + half + 1, 13, shirtC.darker(125));
    g.rect(6, 11, 9, 11, shade);  // pescoço
    g.row(12, 7, 8, shade);
    if (body == 0) g.row(13, 7, 8, shade);  // decote

    // Rosto e orelhas.
    g.rect(4, 3, 11, 9, skinC);
    g.row(10, 5, 10, skinC);
    g.rect(3, 6, 3, 7, skinC);
    g.rect(12, 6, 12, 7, skinC);
    g.rect(6, 6, 6, 7, kInk);  // olhos
    g.rect(9, 6, 9, 7, kInk);
    g.row(9, 7, 8, shade.darker(115));  // boca
    if (body == 0) {
        g.set(5, 8, QColor("#e98c8c"));
        g.set(10, 8, QColor("#e98c8c"));
    }

    if (facialHair == 1) g.row(8, 6, 9, hairC);
    if (facialHair == 2) {
        g.rect(4, 8, 4, 9, hairC);
        g.rect(11, 8, 11, 9, hairC);
        g.row(8, 6, 9, hairC);
        g.row(9, 5, 6, hairC);
        g.row(9, 9, 10, hairC);
        g.row(10, 5, 10, hairC);
    }
    if (glasses == 1) {
        const QColor frame("#2a2a35");
        g.row(5, 5, 7, frame);
        g.row(5, 8, 10, frame);
        g.set(5, 6, frame); g.set(7, 6, frame); g.set(8, 6, frame); g.set(10, 6, frame);
        g.set(5, 7, frame); g.set(7, 7, frame); g.set(8, 7, frame); g.set(10, 7, frame);
        g.set(4, 6, frame); g.set(11, 6, frame);
    } else if (glasses == 2) {
        g.rect(5, 6, 10, 7, kInk);
        g.row(6, 4, 11, kInk);
        g.row(7, 7, 8, skinC);
        g.set(6, 6, QColor("#4a4a5a"));
        g.set(9, 6, QColor("#4a4a5a"));
    }
    drawHair(g, hairStyle, hairC);

    // Contorno escuro em volta de tudo, como nos sprites.
    Grid out = g;
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
            if (!g.filled(x, y) &&
                (g.filled(x - 1, y) || g.filled(x + 1, y) || g.filled(x, y - 1) || g.filled(x, y + 1)))
                out.set(x, y, kInk);

    QImage canvas(N, N, QImage::Format_ARGB32);
    canvas.fill(QColor(kBack[clampIndex(background, kBack)]));
    QPainter(&canvas).drawImage(0, 0, out.img);
    return canvas.scaled(size, size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

PixelAvatar PixelAvatar::random(unsigned seed) {
    QRandomGenerator r(seed);
    PixelAvatar a;
    a.body = r.bounded(3);
    a.skin = r.bounded(6);
    a.hairStyle = a.body == 0 ? int(std::array{3, 4, 5, 2, 7}[r.bounded(5)]) : int(std::array{1, 2, 6, 7, 0, 1}[r.bounded(6)]);
    a.hairColor = r.bounded(10) < 8 ? r.bounded(6) : r.bounded(8);
    a.facialHair = a.body == 1 && r.bounded(3) == 0 ? 1 + r.bounded(2) : 0;
    a.glasses = r.bounded(5) == 0 ? 1 + r.bounded(2) : 0;
    a.shirt = r.bounded(8);
    a.background = r.bounded(8);
    return a;
}

// ------------------------------------------------------------ janela

PixelAvatarDialog::PixelAvatarDialog(const QString& name, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Create Avatar"));
    preview_ = new QLabel;
    preview_->setFixedSize(192, 192);
    preview_->setObjectName("amostraAvatar");

    auto combo = [](const QStringList& items) {
        auto* c = new QComboBox;
        c->addItems(items);
        c->setMinimumWidth(200);
        return c;
    };
    auto colors = [](const QStringList& names, const char* const* hex) {
        auto* c = new QComboBox;
        for (int i = 0; i < names.size(); ++i) {
            QPixmap pm(16, 16);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QColor(0, 0, 0, 60));
            p.setBrush(QColor(hex[i]));
            p.drawRoundedRect(QRectF(0.5, 0.5, 15, 15), 4, 4);
            p.end();
            c->addItem(QIcon(pm), names[i]);
        }
        c->setMinimumWidth(200);
        return c;
    };
    body_ = combo({tr("Feminine"), tr("Masculine"), tr("Neutral")});
    skin_ = colors({tr("Very light"), tr("Light"), tr("Medium"), tr("Tan"), tr("Deep"), tr("Very deep")}, kSkin);
    hairStyle_ = combo({tr("Bald"), tr("Short"), tr("Messy"), tr("Long"), tr("Bob"), tr("Bun"), tr("Mohawk"),
                        tr("Afro")});
    hairColor_ = colors({tr("Black"), tr("Brown"), tr("Light brown"), tr("Blond"), tr("Red", "hair"), tr("Gray"),
                         tr("Blue"), tr("Pink")}, kHair);
    facialHair_ = combo({tr("None"), tr("Mustache"), tr("Beard")});
    glasses_ = combo({tr("None"), tr("Round glasses"), tr("Sunglasses")});
    shirt_ = colors({tr("Blue"), tr("Red"), tr("Green"), tr("Yellow"), tr("Purple"), tr("Black"), tr("White"),
                     tr("Orange")}, kShirt);
    background_ = colors({tr("Sand"), tr("Sky"), tr("Mint"), tr("Pink"), tr("Lavender"), tr("Night"), tr("White"),
                          tr("Sun")}, kBack);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(8);
    form->addRow(tr("Body"), body_);
    form->addRow(tr("Skin tone"), skin_);
    form->addRow(tr("Hair"), hairStyle_);
    form->addRow(tr("Hair color"), hairColor_);
    form->addRow(tr("Facial hair"), facialHair_);
    form->addRow(tr("Glasses"), glasses_);
    form->addRow(tr("Clothes"), shirt_);
    form->addRow(tr("Background"), background_);
    for (QComboBox* c : {body_, skin_, hairStyle_, hairColor_, facialHair_, glasses_, shirt_, background_})
        connect(c, &QComboBox::currentIndexChanged, this, &PixelAvatarDialog::refresh);

    auto* shuffle = new QPushButton(tr("Surprise Me"));
    shuffle->setCursor(Qt::PointingHandCursor);
    connect(shuffle, &QPushButton::clicked, this,
            [this] { load(PixelAvatar::random(QRandomGenerator::global()->generate())); });

    auto* left = new QVBoxLayout;
    left->addWidget(preview_);
    left->addWidget(shuffle);
    left->addStretch(1);
    auto* top = new QHBoxLayout;
    top->setSpacing(24);
    top->addLayout(left);
    top->addLayout(form, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton* use = bb->button(QDialogButtonBox::Ok);
    use->setText(tr("Use Avatar"));
    use->setObjectName("primarioBotao");
    bb->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    for (QAbstractButton* b : bb->buttons()) b->setIcon(QIcon());
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* heading = new QLabel(tr("Create Avatar"));
    heading->setObjectName("tituloDialogo");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 20);
    root->setSpacing(16);
    root->addWidget(heading);
    root->addLayout(top);
    root->addWidget(bb);

    load(PixelAvatar::random(qHash(name)));  // o mesmo nome começa sempre com o mesmo rosto
    use->setDefault(true);
}

void PixelAvatarDialog::load(const PixelAvatar& a) {
    const QList<std::pair<QComboBox*, int>> values = {{body_, a.body},           {skin_, a.skin},
                                                      {hairStyle_, a.hairStyle}, {hairColor_, a.hairColor},
                                                      {facialHair_, a.facialHair}, {glasses_, a.glasses},
                                                      {shirt_, a.shirt},         {background_, a.background}};
    for (const auto& [c, v] : values) {
        QSignalBlocker b(c);
        c->setCurrentIndex(v);
    }
    refresh();
}

PixelAvatar PixelAvatarDialog::current() const {
    PixelAvatar a;
    a.body = body_->currentIndex();
    a.skin = skin_->currentIndex();
    a.hairStyle = hairStyle_->currentIndex();
    a.hairColor = hairColor_->currentIndex();
    a.facialHair = facialHair_->currentIndex();
    a.glasses = glasses_->currentIndex();
    a.shirt = shirt_->currentIndex();
    a.background = background_->currentIndex();
    return a;
}

void PixelAvatarDialog::refresh() {
    // Prévia redonda, como o avatar aparece nos painéis.
    QPixmap pm(192, 192);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addEllipse(QRectF(0, 0, 192, 192));
    p.setClipPath(clip);
    p.drawImage(0, 0, current().render(192));
    p.end();
    preview_->setPixmap(pm);
}

std::string PixelAvatarDialog::base64() const {
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    current().render(192).save(&buf, "PNG");
    return bytes.toBase64().toStdString();
}
