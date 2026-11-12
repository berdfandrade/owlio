#include "pixelavatar.h"

#include <QBuffer>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QToolButton>
#include <QVBoxLayout>
#include <array>
#include <cmath>

#include "theme.h"

namespace {
constexpr int N = 16;  // a grade do sprite

const char* kSkin[] = {"#f6d5b8", "#f2c49b", "#e0a878", "#c68652", "#8d5a3b", "#5c3a24"};
const char* kHair[] = {"#16161d", "#4a2f1d", "#8a5a2b", "#e3c16f", "#b5482a", "#a8a8b0", "#5d7fd6", "#d06ba8"};
const char* kShirt[] = {"#3d63b5", "#c94f4f", "#3f9e6e", "#e0a43b", "#7b5bc4", "#2b2d36", "#e8e8ec", "#e27a3f"};
const char* kBack[] = {"#e8c9a0", "#bcd7ea", "#c9e4c5", "#f2c6cf", "#d9cff2", "#2f3340", "#f4f4f6", "#f6dd8a"};
const QColor kInk("#15151c");

template <size_t K> int pick(int i, const char* (&)[K]) { return std::clamp(i, 0, int(K) - 1); }

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
        case 8:  // repartido de lado
            shortTop();
            g.row(4, 3, 10, h);
            g.set(3, 6, h);
            g.set(10, 3, QColor(h).lighter(130));
            break;
        default: break;  // careca
    }
}
}  // namespace

QImage PixelAvatar::render(int size) const {
    Grid g;
    const QColor skinC(kSkin[pick(skin, kSkin)]);
    const QColor hairC(kHair[pick(hairColor, kHair)]);
    const QColor shirtC(kShirt[pick(shirt, kShirt)]);
    const QColor shade = skinC.darker(118);

    // Corpo e roupa.
    const int half = body == 1 ? 6 : body == 0 ? 4 : 5;
    g.rect(8 - half - 1, 13, 7 + half + 1, 15, shirtC);
    g.row(12, 8 - half, 7 + half, shirtC);
    g.set(8 - half - 1, 13, shirtC.darker(125));
    g.set(7 + half + 1, 13, shirtC.darker(125));
    g.rect(6, 11, 9, 11, shade);  // pescoço
    g.row(12, 7, 8, shade);
    switch (clothes) {
        case 1:  // gola
            g.set(6, 12, Qt::white);
            g.set(9, 12, Qt::white);
            break;
        case 2:  // terno: camisa branca e gravata
            g.rect(6, 12, 9, 15, Qt::white);
            g.rect(7, 13, 8, 15, kInk);
            g.row(12, 7, 8, kInk);
            g.set(5, 13, shirtC.darker(140));
            g.set(10, 13, shirtC.darker(140));
            break;
        case 3:  // moletom: capuz atrás do pescoço e cordões
            g.set(5, 11, shirtC);
            g.set(10, 11, shirtC);
            g.set(6, 14, Qt::white);
            g.set(9, 14, Qt::white);
            break;
        default:
            if (body == 0) g.row(13, 7, 8, shade);  // decote
    }

    // Rosto e orelhas.
    g.rect(4, 3, 11, 9, skinC);
    g.row(10, 5, 10, skinC);
    g.rect(3, 6, 3, 7, skinC);
    g.rect(12, 6, 12, 7, skinC);
    switch (eyes) {
        case 1:  // feliz
            g.set(5, 7, kInk); g.set(6, 6, kInk); g.set(7, 7, kInk);
            g.set(8, 7, kInk); g.set(9, 6, kInk); g.set(10, 7, kInk);
            break;
        case 2:  // arregalado, com brilho
            g.rect(5, 6, 6, 7, kInk);
            g.rect(9, 6, 10, 7, kInk);
            g.set(5, 6, Qt::white);
            g.set(9, 6, Qt::white);
            break;
        case 3:  // cílios
            g.rect(6, 6, 6, 7, kInk);
            g.rect(9, 6, 9, 7, kInk);
            g.set(5, 5, kInk);
            g.set(10, 5, kInk);
            break;
        default:
            g.rect(6, 6, 6, 7, kInk);
            g.rect(9, 6, 9, 7, kInk);
    }
    const QColor lip = shade.darker(120);
    switch (mouth) {
        case 1: g.row(9, 7, 8, lip); break;  // neutra
        case 2:                              // aberta
            g.row(9, 7, 8, kInk);
            g.row(10, 7, 8, QColor("#c0504d"));
            break;
        case 3:  // de lado
            g.row(9, 8, 9, lip);
            g.set(10, 8, lip);
            break;
        default:  // sorriso
            g.row(9, 7, 8, lip);
            g.set(6, 8, lip);
            g.set(9, 8, lip);
    }
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
        for (int x : {5, 7, 8, 10}) g.rect(x, 6, x, 7, frame);
        g.set(4, 6, frame);
        g.set(11, 6, frame);
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
    canvas.fill(QColor(kBack[pick(background, kBack)]));
    QPainter(&canvas).drawImage(0, 0, out.img);
    return canvas.scaled(size, size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

PixelAvatar PixelAvatar::random(unsigned seed) {
    QRandomGenerator r(seed);
    PixelAvatar a;
    a.body = r.bounded(3);
    a.skin = r.bounded(6);
    a.eyes = r.bounded(4) == 0 ? 1 + r.bounded(3) : 0;
    a.mouth = r.bounded(4);
    a.hairStyle = a.body == 0 ? std::array{3, 4, 5, 2, 7}[r.bounded(5)] : std::array{1, 2, 6, 7, 0, 8}[r.bounded(6)];
    a.hairColor = r.bounded(10) < 8 ? r.bounded(6) : r.bounded(8);
    a.facialHair = a.body == 1 && r.bounded(3) == 0 ? 1 + r.bounded(2) : 0;
    a.glasses = r.bounded(5) == 0 ? 1 + r.bounded(2) : 0;
    a.clothes = r.bounded(4);
    a.shirt = r.bounded(8);
    a.background = r.bounded(8);
    return a;
}

// ------------------------------------------------------------ janela

namespace {
// Uma aba do criador: qual campo as opções mudam e qual paleta as cores mudam (pode faltar um dos dois).
struct Category {
    const char* label;
    const char* icon;
    int PixelAvatar::*option;
    QStringList names;
    int PixelAvatar::*color;
    const char* const* palette;
    int paletteSize;
};

QList<Category> categories() {
    using D = PixelAvatarDialog;
    return {
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Face"), "rosto-avatar", &PixelAvatar::body,
         {D::tr("Feminine"), D::tr("Masculine"), D::tr("Neutral")}, &PixelAvatar::skin, kSkin, 6},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Eyes"), "foco", &PixelAvatar::eyes,
         {D::tr("Normal"), D::tr("Happy"), D::tr("Wide"), D::tr("Lashes")}, nullptr, nullptr, 0},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Mouth"), "boca", &PixelAvatar::mouth,
         {D::tr("Smile"), D::tr("Neutral"), D::tr("Open"), D::tr("Smirk")}, nullptr, nullptr, 0},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hair"), "cabelo", &PixelAvatar::hairStyle,
         {D::tr("Bald"), D::tr("Short"), D::tr("Messy"), D::tr("Long"), D::tr("Bob"), D::tr("Bun"), D::tr("Mohawk"),
          D::tr("Afro"), D::tr("Side part")},
         &PixelAvatar::hairColor, kHair, 8},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Facial hair"), "barba", &PixelAvatar::facialHair,
         {D::tr("None"), D::tr("Mustache"), D::tr("Beard")}, &PixelAvatar::hairColor, kHair, 8},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Glasses"), "oculos", &PixelAvatar::glasses,
         {D::tr("None"), D::tr("Round glasses"), D::tr("Sunglasses")}, nullptr, nullptr, 0},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Clothes"), "roupa", &PixelAvatar::clothes,
         {D::tr("T-shirt"), D::tr("Collar"), D::tr("Suit and tie"), D::tr("Hoodie")}, &PixelAvatar::shirt, kShirt, 8},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Background"), "fundo", &PixelAvatar::background,
         {D::tr("Sand"), D::tr("Sky"), D::tr("Mint"), D::tr("Pink"), D::tr("Lavender"), D::tr("Night"), D::tr("White"),
          D::tr("Sun")},
         nullptr, nullptr, 0},
    };
}

QPixmap rounded(const QImage& img, int size, double radius) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, size, size), radius, radius);
    p.setClipPath(clip);
    p.drawImage(QRect(0, 0, size, size), img);
    return pm;
}
}  // namespace

PixelAvatarDialog::PixelAvatarDialog(const QString& name, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Create Avatar"));
    setMinimumSize(860, 560);
    ThemeManager& tm = ThemeManager::instance();
    initial_ = PixelAvatar::random(qHash(name));  // o mesmo nome começa sempre com o mesmo rosto
    history_ = {initial_};

    // ---- Esquerda: prévia, sortear e recomeçar.
    preview_ = new QLabel;
    preview_->setFixedSize(288, 288);
    preview_->setObjectName("amostraAvatar");
    auto* shuffle = new QPushButton(tm.icon("aleatorio"), tr("Random"));
    auto* reset = new QPushButton(tm.icon("redefinir"), tr("Reset"));
    connect(shuffle, &QPushButton::clicked, this,
            [this] { apply(PixelAvatar::random(QRandomGenerator::global()->generate())); });
    connect(reset, &QPushButton::clicked, this, [this] { apply(initial_); });
    auto* leftButtons = new QHBoxLayout;
    leftButtons->addWidget(shuffle);
    leftButtons->addWidget(reset);
    auto* left = new QVBoxLayout;
    left->setSpacing(14);
    left->addWidget(preview_);
    left->addLayout(leftButtons);
    left->addStretch(1);

    // ---- Direita: desfazer/refazer, categorias, opções e cores.
    undo_ = new QPushButton(tm.icon("desfazer"), tr("Undo"));
    redo_ = new QPushButton(tm.icon("refazer"), tr("Redo"));
    connect(undo_, &QPushButton::clicked, this, [this] { if (pos_ > 0) { --pos_; refresh(); } });
    connect(redo_, &QPushButton::clicked, this, [this] { if (pos_ + 1 < history_.size()) { ++pos_; refresh(); } });
    auto* heading = new QLabel(tr("Create Avatar"));
    heading->setObjectName("tituloDialogo");
    auto* topRow = new QHBoxLayout;
    topRow->addWidget(heading);
    topRow->addStretch(1);
    topRow->addWidget(undo_);
    topRow->addWidget(redo_);

    categories_ = new QButtonGroup(this);
    auto* catRow = new QHBoxLayout;
    catRow->setSpacing(6);
    const QList<Category> cats = categories();
    for (int i = 0; i < cats.size(); ++i) {
        auto* b = new QToolButton;
        b->setObjectName("categoria");
        b->setCheckable(true);
        b->setIcon(tm.icon(cats[i].icon));
        b->setIconSize(QSize(22, 22));
        b->setText(tr(cats[i].label));
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setCursor(Qt::PointingHandCursor);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        categories_->addButton(b, i);
        catRow->addWidget(b);
    }
    connect(categories_, &QButtonGroup::idClicked, this, &PixelAvatarDialog::setCategory);

    options_ = new QListWidget;
    options_->setObjectName("opcoesAvatar");
    options_->setViewMode(QListView::IconMode);
    options_->setIconSize(QSize(80, 80));
    options_->setGridSize(QSize(100, 100));
    options_->setMovement(QListView::Static);
    options_->setResizeMode(QListView::Adjust);
    options_->setUniformItemSizes(true);
    options_->setSpacing(4);
    options_->setCursor(Qt::PointingHandCursor);
    connect(options_, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        PixelAvatar a = avatar();
        a.*(categories()[category_].option) = options_->row(it);
        apply(a);
    });

    colorRow_ = new QWidget;
    colorLayout_ = new QHBoxLayout(colorRow_);
    colorLayout_->setContentsMargins(0, 0, 0, 0);
    colorLayout_->setSpacing(8);

    auto* done = new QPushButton(tr("Use Avatar"));
    done->setObjectName("primarioBotao");
    auto* cancel = new QPushButton(tr("Cancel"));
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* bottom = new QHBoxLayout;
    bottom->addWidget(colorRow_);
    bottom->addStretch(1);
    bottom->addWidget(cancel);
    bottom->addWidget(done);

    auto* right = new QVBoxLayout;
    right->setSpacing(14);
    right->addLayout(topRow);
    right->addLayout(catRow);
    right->addWidget(options_, 1);
    right->addLayout(bottom);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(28);
    root->addLayout(left);
    root->addLayout(right, 1);

    setCategory(3);  // começa pelo cabelo, como nos criadores de avatar
    done->setDefault(true);
}

void PixelAvatarDialog::setCategory(int index) {
    category_ = std::clamp(index, 0, int(categories().size()) - 1);
    categories_->button(category_)->setChecked(true);
    refresh();
}

void PixelAvatarDialog::apply(const PixelAvatar& a) {
    history_.resize(pos_ + 1);
    history_.append(a);
    pos_ = static_cast<int>(history_.size()) - 1;
    refresh();
}

void PixelAvatarDialog::fillOptions() {
    // Cada opção é desenhada com o avatar atual, só trocando a parte daquela categoria.
    const Category c = categories()[category_];
    const PixelAvatar a = avatar();
    QSignalBlocker block(options_);
    options_->clear();
    for (int i = 0; i < c.names.size(); ++i) {
        PixelAvatar v = a;
        v.*(c.option) = i;
        const QPixmap pm = rounded(v.render(160), 160, 18);
        QIcon icon(pm);
        icon.addPixmap(pm, QIcon::Selected);  // sem o véu azul da seleção: a borda já marca a escolhida
        auto* it = new QListWidgetItem(icon, QString());
        it->setToolTip(c.names[i]);
        options_->addItem(it);
    }
    options_->setCurrentRow(a.*(c.option));

    while (QLayoutItem* item = colorLayout_->takeAt(0)) {
        if (QWidget* w = item->widget()) {  // pode ser o botão que acabou de ser clicado
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    colorRow_->setVisible(c.palette != nullptr);
    for (int i = 0; i < c.paletteSize; ++i) {
        auto* b = new QToolButton;
        b->setObjectName("corAvatar");
        b->setCheckable(true);
        b->setChecked(a.*(c.color) == i);
        b->setFixedSize(30, 30);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString("QToolButton#corAvatar { background: %1; }").arg(c.palette[i]));
        connect(b, &QToolButton::clicked, this, [this, i] {
            PixelAvatar v = avatar();
            v.*(categories()[category_].color) = i;
            apply(v);
        });
        colorLayout_->addWidget(b);
    }
}

void PixelAvatarDialog::refresh() {
    preview_->setPixmap(rounded(avatar().render(288), 288, 144));
    undo_->setEnabled(pos_ > 0);
    redo_->setEnabled(pos_ + 1 < history_.size());
    fillOptions();
}

std::string PixelAvatarDialog::base64() const {
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    avatar().render(192).save(&buf, "PNG");
    return bytes.toBase64().toStdString();
}
