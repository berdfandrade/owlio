#include "pixelavatar.h"

#include <QBuffer>
#include <QButtonGroup>
#include <QGridLayout>
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
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "theme.h"

namespace {
constexpr int N = 32;  // a grade do sprite

const char* const kSkin[] = {"#ffe1c8", "#f8cfa6", "#ebb486", "#d69767", "#b9784b", "#955d38", "#6f4327", "#4b2b19"};
const char* const kHair[] = {"#1d1b24", "#3d2719", "#6b4428", "#a0682f", "#e0b65e", "#f1e0a8",
                             "#b8452a", "#b5b5c0", "#f2f2f5", "#4f74d9", "#d66cb0", "#42b088"};
const char* const kEye[] = {"#6b4426", "#3a281c", "#3f7bcc", "#3f9a58", "#94823c", "#6f7c8c"};
const char* const kLips[] = {"#c0716a", "#dc6a86", "#c4283c", "#7f2d55", "#ec8f6e"};
const char* const kCloth[] = {"#3d63b5", "#c94f4f", "#3f9e6e", "#e0a43b", "#7b5bc4", "#2c2e38",
                              "#e9e9ee", "#e27a3f", "#2f93ad", "#d76d9b", "#7d8c3a", "#8a5a3a"};
const char* const kBack[] = {"#e3c29a", "#9cc7e8", "#a9dbb7", "#f2b8c6", "#c9b8f0",
                             "#2e3452", "#eceef2", "#f5d36b", "#f0956a", "#5f8f5a"};
const QColor kInk("#1d1a24");
const QColor kWhite("#fbfbfd");

template <size_t K> QColor pick(int i, const char* const (&pal)[K]) { return QColor(pal[std::clamp(i, 0, int(K) - 1)]); }

// ---- cores: luz, base e duas sombras, com a matiz puxada para o amarelo na luz e para o roxo na sombra.
struct Ramp {
    QColor hi, base, shade, deep;
};

double hueToward(double h, double target, double deg) {
    double d = std::fmod(target - h + 540.0, 360.0) - 180.0;
    const double step = std::copysign(std::min(std::abs(d), deg), d);
    return std::fmod(h + step + 360.0, 360.0);
}

QColor hsv(double h, double s, double v) {
    return QColor::fromHsvF(float(h / 360.0), float(std::clamp(s, 0.0, 1.0)), float(std::clamp(v, 0.0, 1.0)));
}

Ramp ramp(const QColor& c) {
    float hf, sf, vf;
    c.getHsvF(&hf, &sf, &vf);
    const double s = sf, v = vf;
    if (sf < 0.12 || hf < 0) {  // cinzas: sombra levemente azulada
        return {hsv(230, s * 0.5, std::min(1.0, v * 1.1 + 0.05)), c, hsv(235, s + 0.08, v * 0.74),
                hsv(245, s + 0.16, v * 0.48)};
    }
    const double h = hf * 360.0;
    return {hsv(hueToward(h, 55, 8), s * 0.82, std::min(1.0, v * 1.12 + 0.05)), c,
            hsv(hueToward(h, 265, 12), s * 1.06 + 0.05, v * 0.78),
            hsv(hueToward(h, 265, 24), s * 1.1 + 0.12, v * 0.52)};
}

QColor mix(const QColor& a, const QColor& b, double t) {
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t), float(a.greenF() + (b.greenF() - a.greenF()) * t),
                            float(a.blueF() + (b.blueF() - a.blueF()) * t));
}

// ---- máscaras de pixels
int mx(int x) { return N - 1 - x; }  // espelho horizontal

struct Mask {
    std::array<bool, N * N> m{};
    bool has(int x, int y) const { return x >= 0 && y >= 0 && x < N && y < N && m[y * N + x]; }
    void set(int x, int y, bool v = true) {
        if (x >= 0 && y >= 0 && x < N && y < N) m[y * N + x] = v;
    }
    void both(int x, int y) { set(x, y); set(mx(x), y); }
    void span(int y, int x0, int x1) { for (int x = x0; x <= x1; ++x) set(x, y); }
    void spanBoth(int y, int x0, int x1) { span(y, x0, x1); span(y, mx(x1), mx(x0)); }
    void sym(int y, int hw) { span(y, 16 - hw, 15 + hw); }  // centrado na grade
    void rect(int x0, int y0, int x1, int y1) { for (int y = y0; y <= y1; ++y) span(y, x0, x1); }
    void ellipse(double cx, double cy, double rx, double ry, int yMin = 0, int yMax = N - 1) {
        for (int y = std::max(0, yMin); y <= std::min(N - 1, yMax); ++y)
            for (int x = 0; x < N; ++x) {
                const double u = (x + 0.5 - cx) / rx, v = (y + 0.5 - cy) / ry;
                if (u * u + v * v <= 1.0) set(x, y);
            }
    }
    Mask& operator-=(const Mask& o) {
        for (int i = 0; i < N * N; ++i) m[i] = m[i] && !o.m[i];
        return *this;
    }
    bool empty() const { return std::none_of(m.begin(), m.end(), [](bool b) { return b; }); }
    bool edge(int x, int y) const {  // fora da grade conta como dentro: a roupa não ganha contorno embaixo
        auto in = [&](int a, int b) { return a < 0 || b < 0 || a >= N || b >= N || has(a, b); };
        return !in(x - 1, y) || !in(x + 1, y) || !in(x, y - 1) || !in(x, y + 1);
    }
};

enum class Texture { None, Curl, Strand, Braid, Rib };

struct Canvas {
    QImage img{N, N, QImage::Format_ARGB32};
    Canvas() { img.fill(Qt::transparent); }
    void set(int x, int y, const QColor& c) {
        if (x >= 0 && y >= 0 && x < N && y < N) img.setPixelColor(x, y, c);
    }
    void both(int x, int y, const QColor& c) {
        set(x, y, c);
        set(mx(x), y, c);
    }
    QColor at(int x, int y) const { return img.pixelColor(std::clamp(x, 0, N - 1), std::clamp(y, 0, N - 1)); }
    void tint(int x, int y, const QColor& c, double t) { set(x, y, mix(at(x, y), c, t)); }

    // Pinta a máscara com luz vindo do alto à esquerda; a borda fica com a sombra mais funda.
    void paint(const Mask& m, const Ramp& r, double cx, double cy, double rx, double ry, bool rim = true,
               Texture tex = Texture::None) {
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                if (!m.has(x, y)) continue;
                if (rim && m.edge(x, y)) {
                    set(x, y, r.deep);
                    continue;
                }
                const double u = (x + 0.5 - cx) / rx, v = (y + 0.5 - cy) / ry;
                const double l = -(0.62 * u + 0.55 * v) + 0.08;
                int tone = l > 0.45 ? 0 : l > -0.4 ? 1 : 2;
                switch (tex) {
                    case Texture::Curl:
                        if ((x * 3 + y * 5) % 7 == 0) tone = std::min(2, tone + 1);
                        else if ((x * 5 + y * 3) % 11 == 0) tone = 0;
                        break;
                    case Texture::Strand:
                        if (x % 3 == 1 && y > 10) tone = std::min(2, tone + 1);
                        break;
                    case Texture::Braid:
                        if ((y + x % 2) % 3 == 0) tone = 2;
                        break;
                    case Texture::Rib:
                        if (x % 2 == 0) tone = std::min(2, tone + 1);
                        break;
                    case Texture::None: break;
                }
                set(x, y, tone == 0 ? r.hi : tone == 1 ? r.base : r.shade);
            }
    }
};

// ---- cabelo: a parte de trás fica atrás do rosto, a da frente por cima.
struct HairShape {
    Mask back, front, stubble;  // stubble: cabelo raspado, só tinge a pele
    Texture tex = Texture::None;
};

Mask cap(double rx = 9.2, double ry = 7.6, int yMax = 8) {
    Mask c;
    c.ellipse(16, 11, rx, ry, 0, yMax);
    return c;
}

HairShape hairShape(int style) {
    HairShape h;
    Mask& f = h.front;
    Mask& b = h.back;
    auto temples = [&](int to) {
        for (int y = 9; y <= to; ++y) { f.both(8, y); f.both(7, y); }
    };
    switch (style) {
        case 1:  // curto
            f = cap();
            f.span(9, 8, 14);
            f.span(9, 19, 23);
            f.spanBoth(10, 8, 10);
            temples(13);
            break;
        case 2:  // bagunçado
            f = cap(9.6, 8.2);
            for (int x : {10, 13, 17, 21}) f.set(x, 2);
            for (auto [x0, x1] : {std::pair{9, 11}, {13, 14}, {16, 18}, {20, 22}}) f.span(3, x0, x1);
            f.span(9, 7, 24);
            for (int x : {11, 15, 19}) f.set(x, 9, false);
            for (auto [x0, x1] : {std::pair{8, 9}, {12, 13}, {16, 17}, {20, 23}}) f.span(10, x0, x1);
            for (int x : {8, 13, 17, 23}) f.set(x, 11);
            temples(13);
            break;
        case 3:  // comprido
            f = cap();
            b.rect(6, 9, 25, 26);
            b.span(27, 8, 23);
            f.span(9, 8, 14);
            f.span(9, 17, 23);
            f.spanBoth(10, 8, 12);
            f.spanBoth(11, 7, 10);
            for (int y = 12; y <= 24; ++y) f.spanBoth(y, 6, 8);
            f.spanBoth(25, 6, 7);
            h.tex = Texture::Strand;
            break;
        case 4:  // chanel
            f = cap(9.6, 8.0);
            f.span(9, 7, 24);
            f.span(10, 8, 23);
            for (int y = 11; y <= 18; ++y) f.spanBoth(y, 6, 9);
            f.spanBoth(19, 7, 9);
            break;
        case 5:  // coque
            f = cap(9.0, 7.4);
            temples(10);
            f.ellipse(16, 2.6, 3.4, 2.6);
            break;
        case 6:  // rabo de cavalo
            f = cap(9.0, 7.4);
            temples(11);
            b.ellipse(25.2, 14, 2.8, 8);
            b.rect(23, 7, 25, 9);
            h.tex = Texture::Strand;
            break;
        case 7:  // moicano
            f.rect(14, 2, 17, 8);
            f.span(1, 15, 16);
            f.span(9, 15, 16);
            h.stubble = cap();
            h.stubble -= f;
            break;
        case 8:  // black power
            b.ellipse(16, 10.5, 12.6, 10.4, 0, 20);
            f = cap(11, 9, 8);
            f.spanBoth(9, 8, 10);
            f.spanBoth(10, 8, 9);
            h.tex = Texture::Curl;
            break;
        case 9:  // repartido de lado
            f = cap();
            f.span(9, 8, 10);
            f.span(9, 13, 23);
            f.span(10, 15, 23);
            f.span(11, 18, 23);
            f.span(12, 21, 23);
            f.set(23, 13);
            f.set(8, 10);
            f.set(8, 11);
            break;
        case 10:  // cacheado
            b.ellipse(16, 13, 11.5, 10, 0, 21);
            f = cap(10.4, 8.6);
            for (double cx : {9.0, 12.5, 16.0, 19.5, 23.0}) f.ellipse(cx, 9.6, 1.9, 1.7);
            for (int y = 11; y <= 18; ++y) f.spanBoth(y, 6, 8);
            h.tex = Texture::Curl;
            break;
        case 11:  // tranças
            f = cap();
            f.span(9, 8, 13);
            f.span(9, 18, 23);
            f.spanBoth(10, 8, 9);
            for (int y = 10; y <= 29; ++y) b.spanBoth(y, 5, 7);
            h.tex = Texture::Braid;
            break;
        case 12:  // raspado
            h.stubble = cap();
            h.stubble.spanBoth(9, 8, 9);
            break;
        case 13:  // ondulado longo
            f = cap(9.6, 8.0);
            for (int y = 8; y <= 28; ++y) {
                const int wave = int(std::lround(std::sin(y * 0.8)));
                const int inset = y >= 27 ? (y - 26) * 2 : 0;
                b.span(y, 4 + wave + inset, 27 - wave - inset);
            }
            f.span(9, 8, 14);
            f.span(9, 17, 23);
            f.spanBoth(10, 7, 11);
            for (int y = 11; y <= 25; ++y) {
                const int wave = int(std::lround(std::sin(y * 0.8)));
                f.spanBoth(y, 5 + wave, 8);
            }
            h.tex = Texture::Strand;
            break;
        case 14:  // topete
            f.span(2, 13, 19);
            f.span(3, 11, 21);
            f.span(4, 10, 22);
            f.rect(9, 5, 22, 7);
            f.span(8, 9, 21);
            f.span(9, 12, 18);
            h.stubble = cap();
            h.stubble.spanBoth(9, 8, 9);
            h.stubble -= f;
            break;
        default: break;  // careca
    }
    return h;
}

// ---- partes do rosto (o olho esquerdo é desenhado e espelhado)
void drawEyes(Canvas& c, int style, const Ramp& iris, const Ramp& skin) {
    auto both = [&](int x, int y, const QColor& col) { c.both(x, y, col); };
    auto open = [&](int top) {  // branco por fora, pupila no alto e íris embaixo
        both(10, top, kWhite);
        both(11, top, kInk);
        both(12, top, iris.deep);
        both(10, top + 1, mix(kWhite, skin.shade, 0.35));
        both(11, top + 1, iris.base);
        both(12, top + 1, iris.hi);
    };
    switch (style) {
        case 1:  // feliz
            both(10, 15, kInk);
            both(11, 14, kInk);
            both(12, 14, kInk);
            both(13, 15, kInk);
            break;
        case 2:  // arregalado
            for (int x = 10; x <= 12; ++x) { both(x, 12, kInk); both(x, 13, kWhite); }
            open(14);
            break;
        case 3:  // com cílios
            for (int x = 9; x <= 12; ++x) both(x, 13, kInk);
            both(9, 12, kInk);
            open(14);
            break;
        case 4:  // sonolento
            for (int x = 10; x <= 12; ++x) { both(x, 13, skin.shade); both(x, 14, kInk); }
            both(10, 15, kWhite);
            both(11, 15, iris.base);
            both(12, 15, iris.base);
            break;
        case 5:  // fechado
            both(9, 14, kInk);
            for (int x = 10; x <= 12; ++x) both(x, 15, kInk);
            break;
        default:
            for (int x = 10; x <= 12; ++x) both(x, 13, kInk);
            open(14);
    }
}

void drawBrows(Canvas& c, int style, const QColor& col) {
    using P = std::pair<int, int>;
    std::vector<P> px;
    switch (style) {
        case 1: px = {{9, 12}, {10, 11}, {11, 11}, {12, 11}, {13, 11}, {10, 12}, {11, 12}, {12, 12}}; break;
        case 2: px = {{9, 11}, {10, 10}, {11, 10}, {12, 11}}; break;
        case 3: px = {{9, 11}, {10, 10}, {11, 10}, {12, 10}}; break;
        case 4: px = {{9, 11}, {10, 11}, {11, 12}, {12, 12}, {13, 13}}; break;
        case 5: px = {{9, 12}, {10, 12}, {11, 11}, {12, 11}, {13, 10}}; break;
        default: px = {{9, 12}, {10, 11}, {11, 11}, {12, 11}};
    }
    for (auto [x, y] : px) c.both(x, y, col);
}

void drawMouth(Canvas& c, int style, const Ramp& lip, const Ramp& skin) {
    const QColor line = lip.deep;
    auto row = [&](int y, int x0, int x1, const QColor& col) { for (int x = x0; x <= x1; ++x) c.set(x, y, col); };
    switch (style) {
        case 1:  // neutra
            row(20, 14, 17, line);
            row(21, 15, 16, skin.shade);
            break;
        case 2:  // rindo
            row(19, 13, 18, kWhite);
            c.set(13, 19, line);
            c.set(18, 19, line);
            row(20, 14, 17, QColor("#d75a6a"));
            c.set(14, 20, line);
            c.set(17, 20, line);
            row(21, 15, 16, line);
            break;
        case 3:  // de lado
            row(20, 13, 16, line);
            c.set(17, 19, line);
            row(21, 14, 15, skin.shade);
            break;
        case 4:  // triste
            row(20, 14, 17, line);
            c.set(13, 21, line);
            c.set(18, 21, line);
            break;
        case 5:  // dentes
            row(19, 13, 18, line);
            row(20, 14, 17, kWhite);
            c.set(13, 20, line);
            c.set(18, 20, line);
            row(21, 14, 17, line);
            break;
        case 6:  // lábios cheios
            c.set(14, 19, lip.base);
            c.set(17, 19, lip.base);
            row(19, 15, 16, lip.shade);
            row(20, 13, 18, line);
            row(21, 14, 17, lip.base);
            c.set(15, 21, lip.hi);
            break;
        default:  // sorriso
            c.set(13, 19, line);
            c.set(18, 19, line);
            row(20, 14, 17, line);
            row(21, 15, 16, skin.shade);
    }
}

void drawGlasses(Canvas& c, int style) {
    const QColor frame("#2a2836");
    auto ring = [&](int x0, const QColor& col, bool corners) {
        for (int x = x0 + 1; x <= x0 + 3; ++x) { c.set(x, 12, col); c.set(x, 16, col); }
        for (int y = 13; y <= 15; ++y) { c.set(x0, y, col); c.set(x0 + 4, y, col); }
        if (corners)
            for (auto [x, y] : {std::pair{x0, 12}, {x0 + 4, 12}, {x0, 16}, {x0 + 4, 16}}) c.set(x, y, col);
    };
    switch (style) {
        case 1:
        case 2:
            ring(9, frame, style == 2);
            ring(18, frame, style == 2);
            for (int x = 14; x <= 17; ++x) c.set(x, 13, frame);
            c.both(8, 13, frame);
            c.tint(10, 13, kWhite, 0.7);
            c.tint(19, 13, kWhite, 0.7);
            break;
        case 3: {  // escuros
            const QColor lens("#22242f");
            for (int y = 12; y <= 15; ++y)
                for (int x = 9; x <= 13; ++x) c.both(x, y, y == 12 ? frame : lens);
            for (int x = 14; x <= 17; ++x) c.set(x, 13, frame);
            c.both(8, 13, frame);
            c.set(10, 13, QColor("#6f7894"));
            c.set(19, 13, QColor("#6f7894"));
            c.set(11, 14, QColor("#454a60"));
            c.set(20, 14, QColor("#454a60"));
            break;
        }
        case 4: {  // monóculo
            const QColor gold("#d9a93a");
            ring(18, gold, false);
            c.tint(19, 13, kWhite, 0.7);
            for (auto [x, y] : {std::pair{23, 17}, {23, 18}, {24, 19}, {24, 20}, {24, 21}, {25, 22}}) c.set(x, y, gold);
            break;
        }
        case 5: {  // tapa-olho
            for (int y = 12; y <= 16; ++y)
                for (int x = 9; x <= 13; ++x)
                    if (!((y == 12 || y == 16) && (x == 9 || x == 13))) c.set(x, y, kInk);
            c.set(10, 13, QColor("#3a3644"));
            for (auto [x, y] : {std::pair{8, 12}, {7, 11}, {14, 12}, {15, 11}, {16, 11}, {17, 10}, {18, 10}, {19, 9},
                                {20, 9}, {21, 8}, {22, 8}, {23, 7}, {24, 7}})
                c.set(x, y, kInk);
            break;
        }
        default: break;
    }
}

void drawHat(Canvas& c, int style, const Ramp& r) {
    Mask m;
    switch (style) {
        case 1: {  // gorro
            m.ellipse(16, 9, 9.6, 7.2, 0, 8);
            m.rect(7, 8, 24, 10);
            c.paint(m, r, 16, 6, 10, 6);
            for (int x = 8; x <= 23; x += 2) c.set(x, 9, r.shade);
            Mask pom;
            pom.ellipse(16, 1.8, 2.4, 2.0);
            c.paint(pom, ramp(kWhite), 15, 1, 3, 3);
            break;
        }
        case 2: {  // boné
            m.ellipse(16, 9.2, 9.2, 6.4, 0, 8);
            c.paint(m, r, 16, 6, 10, 6);
            for (int y = 4; y <= 7; ++y) c.set(16, y, r.shade);
            c.set(16, 3, r.hi);
            Mask brim;
            brim.span(9, 6, 25);
            brim.span(10, 7, 24);
            c.paint(brim, r, 16, 9, 10, 2, false);
            for (int x = 7; x <= 24; ++x) c.set(x, 10, r.deep);
            break;
        }
        case 3: {  // chapéu
            m.span(2, 11, 14);
            m.span(2, 17, 20);
            m.rect(10, 3, 21, 7);
            c.paint(m, r, 15, 4, 7, 4);
            for (int x = 10; x <= 21; ++x) c.set(x, 6, ramp(r.deep).deep);
            Mask brim;
            brim.span(8, 4, 27);
            brim.span(9, 6, 25);
            c.paint(brim, r, 15, 8, 12, 2);
            break;
        }
        case 4: {  // faixa
            m.span(7, 9, 22);
            m.span(8, 8, 23);
            m.span(9, 7, 24);
            c.paint(m, r, 16, 8, 9, 2, false);
            for (int x = 7; x <= 24; ++x) c.set(x, 9, r.shade);
            break;
        }
        case 5: {  // laço
            m.ellipse(21.3, 4.4, 2.3, 2.0);
            m.ellipse(26.4, 4.4, 2.3, 2.0);
            c.paint(m, r, 22, 3, 5, 3);
            c.set(23, 4, r.deep);
            c.set(24, 4, r.deep);
            c.set(23, 5, r.shade);
            c.set(24, 5, r.shade);
            break;
        }
        case 6: {  // coroa (sempre dourada)
            const Ramp gold = ramp(QColor("#e8b830"));
            for (int x0 : {10, 15, 20}) m.rect(x0, 2, x0 + 1, 4);
            m.rect(10, 5, 21, 7);
            m.span(4, 10, 21);
            c.paint(m, gold, 13, 3, 7, 4);
            c.set(13, 6, QColor("#d0364a"));
            c.set(18, 6, QColor("#3f74d8"));
            c.set(15, 6, QColor("#3fae6c"));
            c.set(16, 6, QColor("#3fae6c"));
            break;
        }
        default: break;
    }
}
}  // namespace

QImage PixelAvatar::render(int size) const {
    Canvas c;
    const Ramp skinR = ramp(pick(skin, kSkin));
    const Ramp hairR = ramp(pick(hairColor, kHair));
    const QColor clothC = pick(shirt, kCloth);
    const Ramp clothR = ramp(clothC);
    const Ramp lipR = lips == 0 ? ramp(mix(skinR.base, QColor("#b8545a"), 0.45)) : ramp(pick(lips, kLips));

    // ---- Fundo em degradê com pontilhado, mais claro atrás da cabeça.
    const Ramp backR = ramp(pick(background, kBack));
    static const int bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            const double dx = (x + 0.5 - 16) / 15, dy = (y + 0.5 - 13) / 15;
            double t = y / double(N - 1) * 2.0 - (dx * dx + dy * dy < 1 ? 0.45 : 0.0);
            t = std::clamp(t, 0.0, 2.0);
            int level = int(t);
            if (t - level > (bayer[y % 4][x % 4] + 0.5) / 16.0) ++level;
            c.set(x, y, level <= 0 ? backR.hi : level == 1 ? backR.base : backR.shade);
        }

    const HairShape hair = hairShape(hairStyle);
    if (!hair.back.empty()) c.paint(hair.back, ramp(hairR.shade), 16, 10, 12, 14, true, hair.tex);

    // ---- Corpo: capuz atrás, pescoço, tronco e roupa.
    static const int torsoFem[] = {6, 8, 10, 11, 11, 11, 11};
    static const int torsoMasc[] = {7, 10, 12, 13, 13, 13, 13};
    static const int torsoNeu[] = {6, 9, 11, 12, 12, 12, 12};
    const int* torso = body == 1 ? torsoMasc : body == 0 ? torsoFem : torsoNeu;
    Mask trunk;
    for (int y = 25; y < N; ++y) trunk.sym(y, torso[y - 25]);

    if (clothes == 3) {  // capuz
        Mask hood;
        for (int y = 22; y <= 25; ++y) hood.sym(y, 6 + (y - 22) / 2);
        c.paint(hood, clothR, 16, 22, 8, 4);
    }
    Mask neck;
    const int neckHw = body == 1 ? 4 : 3;
    for (int y = 20; y <= 25; ++y) neck.sym(y, neckHw);
    c.paint(neck, skinR, 13, 23, 8, 4);
    for (int y = 21; y <= 22; ++y)
        for (int x = 16 - neckHw + 1; x <= 15 + neckHw - 1; ++x) c.set(x, y, skinR.shade);

    if (clothes == 5) {  // alcinha: ombros à mostra
        c.paint(trunk, skinR, 12, 27, 13, 6);
        Mask top;
        for (int y = 28; y < N; ++y) top.sym(y, torso[y - 25] - 1);
        top.sym(27, 7);
        c.paint(top, clothR, 12, 29, 13, 5);
        for (int y = 25; y <= 27; ++y) { c.set(11, y, clothR.shade); c.set(20, y, clothR.shade); }
    } else {
        c.paint(trunk, clothR, 11, 27, 13, 6);
    }
    switch (clothes) {
        case 0:  // camiseta
            if (body == 0) {
                for (int y = 25; y <= 26; ++y)
                    for (int x = 13; x <= 18; ++x) c.set(x, y, skinR.base);
                for (int x = 14; x <= 17; ++x) c.set(x, 27, skinR.base);
                c.both(12, 25, clothR.deep);
                c.both(12, 26, clothR.deep);
                c.both(13, 27, clothR.deep);
                for (int x = 14; x <= 17; ++x) c.set(x, 28, clothR.deep);
            } else {
                for (int x = 13; x <= 18; ++x) { c.set(x, 25, skinR.shade); c.set(x, 26, clothR.deep); }
                c.both(12, 25, clothR.deep);
            }
            break;
        case 1: {  // camisa com gola
            const Ramp collar = ramp(QColor("#f1f1f4"));
            for (auto [x, y] : {std::pair{12, 25}, {13, 25}, {14, 25}, {13, 26}, {14, 26}, {14, 27}}) c.both(x, y, collar.base);
            c.both(12, 26, collar.shade);
            c.both(13, 27, collar.shade);
            for (int y = 26; y < N; ++y) c.set(16, y, clothR.shade);
            for (int y : {28, 30}) c.set(15, y, clothR.hi);
            break;
        }
        case 2: {  // terno e gravata
            const Ramp shirtW = ramp(QColor("#f1f1f4"));
            for (int y = 25; y < N; ++y)
                for (int x = 0; x < N; ++x) {
                    const double half = 3.6 - (y - 25) * 0.55;
                    const double d = std::abs(x + 0.5 - 16);
                    if (d <= half) c.set(x, y, x < 16 ? shirtW.base : shirtW.shade);
                    else if (d <= half + 1 && half > 0) c.set(x, y, clothR.deep);
                }
            const Ramp tie = ramp(QColor(clothC.red() > 160 && clothC.green() < 110 ? "#2c3a6b" : "#b8323a"));
            for (int y = 25; y < N; ++y) { c.set(15, y, tie.base); c.set(16, y, tie.shade); }
            c.set(15, 25, tie.deep);
            c.set(16, 25, tie.deep);
            for (int y = 29; y < N; ++y) { c.set(14, y, tie.base); c.set(17, y, tie.shade); }
            break;
        }
        case 3:  // moletom
            for (int x = 13; x <= 18; ++x) c.set(x, 25, clothR.deep);
            for (int y = 26; y <= 28; ++y) { c.set(14, y, kWhite); c.set(17, y, kWhite); }
            for (int x = 11; x <= 20; ++x) c.set(x, 30, clothR.deep);
            break;
        case 4: {  // gola alta
            Mask turtle;
            for (int y = 21; y <= 25; ++y) turtle.sym(y, neckHw + 1);
            c.paint(turtle, clothR, 13, 22, 6, 4, true, Texture::Rib);
            break;
        }
        case 6: {  // jaqueta aberta sobre camiseta
            const Ramp tee = ramp(QColor("#ecebf0"));
            for (int y = 26; y < N; ++y)
                for (int x = 14; x <= 17; ++x) c.set(x, y, x < 16 ? tee.base : tee.shade);
            for (int y = 25; y < N; ++y) { c.set(13, y, clothR.deep); c.set(18, y, clothR.deep); }
            for (auto [x, y] : {std::pair{11, 25}, {12, 25}, {12, 26}, {12, 27}}) c.both(x, y, clothR.hi);
            for (int x = 14; x <= 17; ++x) c.set(x, 25, skinR.shade);
            break;
        }
        default: break;
    }

    // ---- Orelhas e rosto.
    static const int faceFem[] = {5, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 6, 5, 3};
    static const int faceMasc[] = {5, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 6, 4};
    static const int faceNeu[] = {5, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 6, 5, 4};
    const int* rows = body == 1 ? faceMasc : body == 0 ? faceFem : faceNeu;
    Mask ears;
    ears.both(7, 13);
    for (int y = 14; y <= 16; ++y) { ears.both(6, y); ears.both(7, y); }
    ears.both(7, 17);
    c.paint(ears, skinR, 10, 12, 12, 8);
    c.both(7, 15, skinR.shade);
    Mask faceM;
    for (int y = 6; y <= 22; ++y) faceM.sym(y, rows[y - 6]);
    c.paint(faceM, skinR, 15, 13, 9, 10);

    // Detalhes do rosto.
    switch (face) {
        case 1:
            for (int x : {10, 11}) {
                c.tint(x, 17, QColor("#f07c86"), 0.55);
                c.tint(mx(x), 17, QColor("#f07c86"), 0.55);
            }
            break;
        case 2:
            for (auto [x, y] : {std::pair{10, 16}, {12, 17}, {11, 18}, {14, 17}}) {
                c.tint(x, y, skinR.deep, 0.55);
                c.tint(mx(x), y, skinR.deep, 0.55);
            }
            break;
        case 3: c.set(20, 19, QColor("#3a2418")); break;
        case 4: {
            const QColor scar = mix(skinR.shade, QColor("#c2585a"), 0.5);
            for (auto [x, y] : {std::pair{19, 15}, {20, 16}, {20, 17}, {21, 18}}) c.set(x, y, scar);
            break;
        }
        case 5:
            for (auto [x, y] : {std::pair{9, 14}, {9, 16}, {11, 16}, {12, 16}, {12, 19}, {12, 20}}) c.both(x, y, skinR.shade);
            break;
        default: break;
    }
    // Nariz.
    c.set(15, 16, skinR.hi);
    c.set(16, 17, skinR.shade);
    c.set(15, 18, skinR.deep);
    c.set(16, 18, skinR.shade);

    drawEyes(c, eyes, ramp(pick(eyeColor, kEye)), skinR);
    drawMouth(c, mouth, lipR, skinR);

    // ---- Barba (por cima da boca, que continua à vista).
    auto isSkin = [&](int x, int y) {
        const QColor p = c.at(x, y);
        return p == skinR.hi || p == skinR.base || p == skinR.shade || p == skinR.deep;
    };
    if (facialHair == 1) {
        for (int y = 17; y <= 23; ++y)
            for (int x = 8; x <= 23; ++x)
                if ((y >= 18 || x <= 10 || x >= 21) && faceM.has(x, y) && isSkin(x, y)) c.tint(x, y, hairR.base, 0.4);
    } else if (facialHair >= 2) {
        Mask beard;
        beard.span(18, 13, 18);
        beard.set(13, 19);
        beard.set(18, 19);
        if (facialHair == 3) {
            beard.rect(14, 21, 17, 22);
            beard.span(23, 15, 16);
        }
        if (facialHair >= 4) {
            for (int y = 15; y <= 22; ++y)
                for (int x = 0; x < N; ++x)
                    if (faceM.has(x, y) && (x <= 10 || x >= 21 || y >= 21)) beard.set(x, y);
            beard.span(23, 12, 19);
        }
        if (facialHair == 5)
            for (int y = 24; y <= 26; ++y) beard.sym(y, 6 - (y - 24) * 2);
        Mask mouthHole;
        mouthHole.rect(14, 19, 17, 20);
        beard -= mouthHole;
        c.paint(beard, hairR, 13, 16, 9, 8, facialHair >= 4, facialHair >= 4 ? Texture::Curl : Texture::None);
    }

    // ---- Raspado, sobrancelhas, cabelo da frente e a sombra dele na testa.
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
            if (hair.stubble.has(x, y) && (faceM.has(x, y) || ears.has(x, y))) c.tint(x, y, hairR.shade, 0.45);
    drawBrows(c, brows, hairR.deep);
    if (!hair.front.empty()) {
        c.paint(hair.front, hairR, 13, 7, 10, 9, true, hair.tex == Texture::Strand ? Texture::None : hair.tex);
        for (int y = 1; y < N; ++y)
            for (int x = 0; x < N; ++x)
                if (faceM.has(x, y) && !hair.front.has(x, y) && hair.front.has(x, y - 1) && isSkin(x, y))
                    c.set(x, y, skinR.shade);
    }

    drawGlasses(c, glasses);
    drawHat(c, hat, ramp(pick(hatColor, kCloth)));

    return c.img.scaled(size, size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

PixelAvatar PixelAvatar::random(unsigned seed) {
    QRandomGenerator r(seed);
    PixelAvatar a;
    a.body = r.bounded(3);
    a.skin = r.bounded(8);
    a.face = r.bounded(4) == 0 ? 1 + r.bounded(5) : 0;
    a.eyes = r.bounded(3) == 0 ? 1 + r.bounded(5) : a.body == 0 ? 3 : 0;
    a.eyeColor = r.bounded(10) < 6 ? r.bounded(2) : r.bounded(6);
    a.brows = r.bounded(3) == 0 ? r.bounded(6) : a.body == 1 ? 1 : 0;
    a.mouth = r.bounded(7);
    a.lips = a.body == 0 && r.bounded(2) ? 1 + r.bounded(4) : 0;
    a.hairStyle = a.body == 0 ? std::array{3, 4, 5, 6, 10, 11, 13, 8}[r.bounded(8)]
                              : std::array{1, 2, 7, 8, 9, 12, 14, 0, 10}[r.bounded(9)];
    a.hairColor = r.bounded(10) < 8 ? r.bounded(8) : r.bounded(12);
    a.facialHair = a.body == 1 && r.bounded(3) == 0 ? 1 + r.bounded(5) : 0;
    a.glasses = r.bounded(5) == 0 ? 1 + r.bounded(3) : 0;
    a.hat = r.bounded(6) == 0 ? 1 + r.bounded(5) : 0;
    a.hatColor = r.bounded(12);
    a.clothes = r.bounded(7);
    if (a.clothes == 5 && a.body == 1) a.clothes = 0;
    a.shirt = r.bounded(12);
    a.background = r.bounded(10);
    return a;
}

// ------------------------------------------------------------ janela

namespace {
// Uma aba do criador: qual campo as opções mudam, qual paleta as cores mudam (pode faltar) e que parte
// da grade aparece nas miniaturas (16×16 a partir de crop, ou o avatar inteiro).
struct Category {
    const char* label;
    const char* icon;
    int PixelAvatar::*option;
    QStringList names;
    int PixelAvatar::*color;
    const char* const* palette;
    int paletteSize;
    const char* colorLabel;
    QPoint crop;  // (-1, -1) = inteiro
};

QList<Category> categories() {
    const QPoint whole(-1, -1);
    return {
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Face"), "rosto-avatar", &PixelAvatar::face,
         {PixelAvatarDialog::tr("Plain"), PixelAvatarDialog::tr("Blush"), PixelAvatarDialog::tr("Freckles"), PixelAvatarDialog::tr("Beauty mark"), PixelAvatarDialog::tr("Scar"), PixelAvatarDialog::tr("Wrinkles")},
         &PixelAvatar::skin, kSkin, 8, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Skin tone"), {8, 7}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Eyes"), "foco", &PixelAvatar::eyes,
         {PixelAvatarDialog::tr("Normal"), PixelAvatarDialog::tr("Happy"), PixelAvatarDialog::tr("Wide"), PixelAvatarDialog::tr("Lashes"), PixelAvatarDialog::tr("Sleepy"), PixelAvatarDialog::tr("Closed")},
         &PixelAvatar::eyeColor, kEye, 6, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Eye color"), {8, 6}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Brows"), "sobrancelha", &PixelAvatar::brows,
         {PixelAvatarDialog::tr("Natural"), PixelAvatarDialog::tr("Thick"), PixelAvatarDialog::tr("Thin"), PixelAvatarDialog::tr("Raised"), PixelAvatarDialog::tr("Angry"), PixelAvatarDialog::tr("Worried")},
         &PixelAvatar::hairColor, kHair, 12, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hair color"), {8, 5}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Mouth"), "boca", &PixelAvatar::mouth,
         {PixelAvatarDialog::tr("Smile"), PixelAvatarDialog::tr("Neutral", "mouth"), PixelAvatarDialog::tr("Laugh"), PixelAvatarDialog::tr("Smirk"), PixelAvatarDialog::tr("Sad"), PixelAvatarDialog::tr("Grin"),
          PixelAvatarDialog::tr("Full lips")},
         &PixelAvatar::lips, kLips, 5, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Lip color"), {8, 12}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hair"), "cabelo", &PixelAvatar::hairStyle,
         {PixelAvatarDialog::tr("Bald"), PixelAvatarDialog::tr("Short"), PixelAvatarDialog::tr("Messy"), PixelAvatarDialog::tr("Long"), PixelAvatarDialog::tr("Bob"), PixelAvatarDialog::tr("Bun"), PixelAvatarDialog::tr("Ponytail"),
          PixelAvatarDialog::tr("Mohawk"), PixelAvatarDialog::tr("Afro"), PixelAvatarDialog::tr("Side part"), PixelAvatarDialog::tr("Curly"), PixelAvatarDialog::tr("Braids"), PixelAvatarDialog::tr("Buzz cut"),
          PixelAvatarDialog::tr("Long waves"), PixelAvatarDialog::tr("Quiff")},
         &PixelAvatar::hairColor, kHair, 12, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hair color"), whole},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Facial hair"), "barba", &PixelAvatar::facialHair,
         {PixelAvatarDialog::tr("None", "facial hair"), PixelAvatarDialog::tr("Stubble"), PixelAvatarDialog::tr("Mustache"), PixelAvatarDialog::tr("Goatee"), PixelAvatarDialog::tr("Beard"), PixelAvatarDialog::tr("Full beard")},
         &PixelAvatar::hairColor, kHair, 12, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hair color"), {8, 12}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Glasses"), "oculos", &PixelAvatar::glasses,
         {PixelAvatarDialog::tr("None", "glasses"), PixelAvatarDialog::tr("Round glasses"), PixelAvatarDialog::tr("Square glasses"), PixelAvatarDialog::tr("Sunglasses"), PixelAvatarDialog::tr("Monocle"),
          PixelAvatarDialog::tr("Eye patch")},
         nullptr, nullptr, 0, nullptr, {8, 6}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hat"), "chapeu", &PixelAvatar::hat,
         {PixelAvatarDialog::tr("None", "hat"), PixelAvatarDialog::tr("Beanie"), PixelAvatarDialog::tr("Cap"), PixelAvatarDialog::tr("Fedora"), PixelAvatarDialog::tr("Headband"), PixelAvatarDialog::tr("Bow"), PixelAvatarDialog::tr("Crown")},
         &PixelAvatar::hatColor, kCloth, 12, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hat color"), whole},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Clothes"), "roupa", &PixelAvatar::clothes,
         {PixelAvatarDialog::tr("T-shirt"), PixelAvatarDialog::tr("Collar shirt"), PixelAvatarDialog::tr("Suit and tie"), PixelAvatarDialog::tr("Hoodie"), PixelAvatarDialog::tr("Turtleneck"),
          PixelAvatarDialog::tr("Strap top"), PixelAvatarDialog::tr("Jacket")},
         &PixelAvatar::shirt, kCloth, 12, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Clothing color"), whole},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Background"), "fundo", &PixelAvatar::background,
         {PixelAvatarDialog::tr("Sand"), PixelAvatarDialog::tr("Sky"), PixelAvatarDialog::tr("Mint"), PixelAvatarDialog::tr("Pink"), PixelAvatarDialog::tr("Lavender"), PixelAvatarDialog::tr("Night"), PixelAvatarDialog::tr("White"),
          PixelAvatarDialog::tr("Sun"), PixelAvatarDialog::tr("Sunset"), PixelAvatarDialog::tr("Forest")},
         nullptr, nullptr, 0, nullptr, whole},
    };
}

constexpr int kThumb = 96;  // 3 px por pixel no inteiro, 6 px no recorte de 16×16

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

QPushButton* iconButton(const QIcon& icon, const QString& tip) {
    auto* b = new QPushButton(icon, QString());
    b->setObjectName("botaoIconeAvatar");
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setFixedSize(40, 40);
    b->setIconSize(QSize(20, 20));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
}  // namespace

PixelAvatarDialog::PixelAvatarDialog(const QString& name, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Create Avatar"));
    setMinimumSize(1000, 700);
    ThemeManager& tm = ThemeManager::instance();
    initial_ = PixelAvatar::random(qHash(name));  // o mesmo nome começa sempre com o mesmo rosto
    history_ = {initial_};

    // ---- Esquerda: prévia, desfazer/refazer, sortear e recomeçar.
    preview_ = new QLabel;
    preview_->setFixedSize(288, 288);
    preview_->setObjectName("amostraAvatar");
    auto* nameLabel = new QLabel(name);
    nameLabel->setObjectName("nomeAvatar");
    nameLabel->setAlignment(Qt::AlignCenter);

    undo_ = iconButton(tm.icon("desfazer"), tr("Undo"));
    redo_ = iconButton(tm.icon("refazer"), tr("Redo"));
    undo_->setShortcut(QKeySequence::Undo);
    redo_->setShortcut(QKeySequence::Redo);
    connect(undo_, &QPushButton::clicked, this, [this] { if (pos_ > 0) { --pos_; refresh(); } });
    connect(redo_, &QPushButton::clicked, this, [this] { if (pos_ + 1 < history_.size()) { ++pos_; refresh(); } });
    auto* shuffle = new QPushButton(tm.icon("aleatorio"), tr("Random"));
    auto* reset = new QPushButton(tm.icon("redefinir"), tr("Reset"));
    for (QPushButton* b : {shuffle, reset}) {
        b->setFixedHeight(40);
        b->setCursor(Qt::PointingHandCursor);
    }
    connect(shuffle, &QPushButton::clicked, this, [this] {
        PixelAvatar a = PixelAvatar::random(QRandomGenerator::global()->generate());
        a.body = avatar().body;  // o tipo de corpo escolhido fica
        apply(a);
    });
    connect(reset, &QPushButton::clicked, this, [this] { apply(initial_); });
    auto* leftButtons = new QHBoxLayout;
    leftButtons->setSpacing(8);
    leftButtons->addWidget(undo_);
    leftButtons->addWidget(redo_);
    leftButtons->addSpacing(4);
    leftButtons->addWidget(shuffle, 1);
    leftButtons->addWidget(reset, 1);

    auto* left = new QVBoxLayout;
    left->setSpacing(14);
    left->addWidget(preview_, 0, Qt::AlignHCenter);
    left->addWidget(nameLabel);
    left->addLayout(leftButtons);
    left->addStretch(1);

    // ---- Direita: tipo de corpo, categorias, opções e cores.
    auto* heading = new QLabel(tr("Create Avatar"));
    heading->setObjectName("tituloDialogo");

    auto* bodyTitle = new QLabel(tr("Body type"));
    bodyTitle->setObjectName("rotuloAvatar");
    bodies_ = new QButtonGroup(this);
    auto* bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(10);
    const std::pair<QString, const char*> bodyOpts[] = {
        {tr("Feminine"), "feminino"}, {tr("Masculine"), "masculino"}, {tr("Neutral", "body"), "neutro"}};
    for (int i = 0; i < 3; ++i) {
        auto* b = new QToolButton;
        b->setObjectName("corpo");
        b->setCheckable(true);
        b->setText(bodyOpts[i].first);
        b->setIcon(tm.icon(bodyOpts[i].second));
        b->setIconSize(QSize(18, 18));
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setFixedHeight(40);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setCursor(Qt::PointingHandCursor);
        bodies_->addButton(b, i);
        bodyRow->addWidget(b);
    }
    connect(bodies_, &QButtonGroup::idClicked, this, &PixelAvatarDialog::setBody);

    categories_ = new QButtonGroup(this);
    auto* catGrid = new QGridLayout;
    catGrid->setSpacing(8);
    const QList<Category> cats = categories();
    const int perRow = (int(cats.size()) + 1) / 2;
    for (int i = 0; i < cats.size(); ++i) {
        auto* b = new QToolButton;
        b->setObjectName("categoria");
        b->setCheckable(true);
        b->setIcon(tm.icon(cats[i].icon));
        b->setIconSize(QSize(22, 22));
        b->setText(tr(cats[i].label));
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(64);
        b->setMinimumWidth(96);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        categories_->addButton(b, i);
        catGrid->addWidget(b, i / perRow, i % perRow);
    }
    for (int col = 0; col < perRow; ++col) catGrid->setColumnStretch(col, 1);
    connect(categories_, &QButtonGroup::idClicked, this, &PixelAvatarDialog::setCategory);

    optionsTitle_ = new QLabel;
    optionsTitle_->setObjectName("rotuloAvatar");
    options_ = new QListWidget;
    options_->setObjectName("opcoesAvatar");
    options_->setViewMode(QListView::IconMode);
    options_->setIconSize(QSize(kThumb, kThumb));
    options_->setGridSize(QSize(kThumb + 18, kThumb + 18));
    options_->setMovement(QListView::Static);
    options_->setResizeMode(QListView::Adjust);
    options_->setUniformItemSizes(true);
    options_->setCursor(Qt::PointingHandCursor);
    options_->setMinimumHeight(2 * (kThumb + 18) + 20);
    connect(options_, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        PixelAvatar a = avatar();
        a.*(categories()[category_].option) = options_->row(it);
        apply(a);
    });

    colorTitle_ = new QLabel;
    colorTitle_->setObjectName("rotuloAvatar");
    colorRow_ = new QWidget;
    colorLayout_ = new QGridLayout(colorRow_);
    colorLayout_->setContentsMargins(0, 0, 0, 0);
    colorLayout_->setHorizontalSpacing(8);
    colorLayout_->setVerticalSpacing(8);

    auto* done = new QPushButton(tr("Use Avatar"));
    done->setObjectName("primarioBotao");
    auto* cancel = new QPushButton(tr("Cancel"));
    for (QPushButton* b : {done, cancel}) b->setMinimumSize(120, 40);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* bottom = new QHBoxLayout;
    bottom->addStretch(1);
    bottom->addWidget(cancel);
    bottom->addWidget(done);

    auto* right = new QVBoxLayout;
    right->setSpacing(10);
    right->addWidget(heading);
    right->addSpacing(4);
    right->addWidget(bodyTitle);
    right->addLayout(bodyRow);
    right->addSpacing(8);
    right->addLayout(catGrid);
    right->addSpacing(6);
    right->addWidget(optionsTitle_);
    right->addWidget(options_, 1);
    right->addWidget(colorTitle_);
    right->addWidget(colorRow_);
    right->addSpacing(6);
    right->addLayout(bottom);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(32);
    root->addLayout(left);
    root->addLayout(right, 1);

    setCategory(4);  // começa pelo cabelo, como nos criadores de avatar
    done->setDefault(true);
}

void PixelAvatarDialog::setCategory(int index) {
    category_ = std::clamp(index, 0, int(categories().size()) - 1);
    categories_->button(category_)->setChecked(true);
    refresh();
}

void PixelAvatarDialog::setBody(int body) {
    PixelAvatar a = avatar();
    if (a.body == body) return;
    a.body = std::clamp(body, 0, 2);
    apply(a);
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
    optionsTitle_->setText(tr(c.label));
    QSignalBlocker block(options_);
    options_->clear();
    for (int i = 0; i < c.names.size(); ++i) {
        PixelAvatar v = a;
        v.*(c.option) = i;
        QImage img;
        if (c.crop.x() < 0) {
            img = v.render(kThumb);
        } else {
            const int s = kThumb / 16;  // recorte de 16×16 pixels da grade
            img = v.render(32 * s).copy(c.crop.x() * s, c.crop.y() * s, kThumb, kThumb);
        }
        const QPixmap pm = rounded(img, kThumb, 14);
        QIcon icon(pm);
        icon.addPixmap(pm, QIcon::Selected);  // sem o véu azul da seleção: a borda já marca a escolhida
        auto* it = new QListWidgetItem(icon, QString());
        it->setToolTip(c.names[i]);
        it->setData(Qt::AccessibleTextRole, c.names[i]);
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
    const bool colors = c.palette != nullptr;
    colorRow_->setVisible(colors);
    colorTitle_->setVisible(colors);
    if (colors) colorTitle_->setText(tr(c.colorLabel));
    for (int i = 0; i < c.paletteSize; ++i) {
        auto* b = new QToolButton;
        b->setObjectName("corAvatar");
        b->setCheckable(true);
        b->setChecked(a.*(c.color) == i);
        b->setFixedSize(32, 32);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString("QToolButton#corAvatar { background: %1; }").arg(c.palette[i]));
        connect(b, &QToolButton::clicked, this, [this, i] {
            PixelAvatar v = avatar();
            v.*(categories()[category_].color) = i;
            apply(v);
        });
        colorLayout_->addWidget(b, 0, i);
    }
    for (int col = 0; col <= 12; ++col) colorLayout_->setColumnStretch(col, col == c.paletteSize ? 1 : 0);
}

void PixelAvatarDialog::refresh() {
    const PixelAvatar a = avatar();
    preview_->setPixmap(rounded(a.render(288), 288, 36));
    if (QAbstractButton* b = bodies_->button(a.body)) b->setChecked(true);
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
