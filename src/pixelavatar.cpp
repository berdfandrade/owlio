#include "pixelavatar.h"

#include <QBuffer>
#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
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
constexpr int N = 64;          // a grade do sprite
constexpr double C = N / 2.0;  // eixo de simetria do rosto

const char* const kSkin[] = {"#ffe9d6", "#ffe1c8", "#f8cfa6", "#ebb486", "#d69767",
                             "#b9784b", "#955d38", "#6f4327", "#4b2b19", "#33190f"};
// Dez naturais primeiro, depois as cores de fantasia.
const char* const kHair[] = {"#1d1b24", "#3d2719", "#6b4428", "#a0682f", "#e0b65e", "#f1e0a8", "#b8452a", "#7a2e1c",
                             "#b5b5c0", "#f2f2f5", "#4f74d9", "#d66cb0", "#42b088", "#8a5bd0", "#e2553f", "#2f8fa8"};
const char* const kEye[] = {"#6b4426", "#3a281c", "#3f7bcc", "#3f9a58", "#94823c", "#6f7c8c", "#c88a2a", "#7a5bc0"};
const char* const kLips[] = {"#c0716a", "#dc6a86", "#c4283c", "#7f2d55", "#ec8f6e", "#a14a3c", "#e04f9a", "#3a2a3a"};
const char* const kCloth[] = {"#3d63b5", "#c94f4f", "#3f9e6e", "#e0a43b", "#7b5bc4", "#2c2e38", "#e9e9ee", "#e27a3f",
                              "#2f93ad", "#d76d9b", "#7d8c3a", "#8a5a3a", "#f4c7d4", "#b9e0d2", "#1f4d3a", "#7a1f2b"};
const char* const kBack[] = {"#e3c29a", "#9cc7e8", "#a9dbb7", "#f2b8c6", "#c9b8f0", "#2e3452",
                             "#eceef2", "#f5d36b", "#f0956a", "#5f8f5a", "#f7a8a0", "#3f6f8f"};
const QColor kInk("#1d1a24");
const QColor kWhite("#fbfbfd");

template <size_t K> QColor pick(int i, const char* const (&pal)[K]) { return QColor(pal[std::clamp(i, 0, int(K) - 1)]); }

// ---- cores: brilho, luz, base, duas sombras e contorno, com a matiz puxada para o amarelo na luz
// e para o roxo na sombra.
struct Ramp {
    QColor glow, hi, base, shade, deep, line;
    const QColor& tone(int t) const {
        switch (std::clamp(t, 0, 5)) {
            case 0: return glow;
            case 1: return hi;
            case 2: return base;
            case 3: return shade;
            case 4: return deep;
            default: return line;
        }
    }
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
        return {hsv(230, s * 0.3, std::min(1.0, v * 1.2 + 0.12)), hsv(230, s * 0.5, std::min(1.0, v * 1.1 + 0.05)), c,
                hsv(235, s + 0.08, v * 0.76), hsv(245, s + 0.16, v * 0.54), hsv(250, s + 0.2, v * 0.3)};
    }
    const double h = hf * 360.0;
    return {hsv(hueToward(h, 55, 14), s * 0.55, std::min(1.0, v * 1.2 + 0.12)),
            hsv(hueToward(h, 55, 8), s * 0.82, std::min(1.0, v * 1.12 + 0.05)), c,
            hsv(hueToward(h, 265, 12), s * 1.06 + 0.05, v * 0.8),
            hsv(hueToward(h, 265, 24), s * 1.1 + 0.12, v * 0.58),
            hsv(hueToward(h, 265, 32), s * 1.1 + 0.16, v * 0.34)};
}

QColor mix(const QColor& a, const QColor& b, double t) {
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t), float(a.greenF() + (b.greenF() - a.greenF()) * t),
                            float(a.blueF() + (b.blueF() - a.blueF()) * t));
}

// ---- máscaras de pixels: as formas são curvas preenchidas sem suavização.
struct Mask {
    std::array<quint8, N * N> m{};
    bool has(int x, int y) const { return x >= 0 && y >= 0 && x < N && y < N && m[y * N + x]; }
    void set(int x, int y, bool v = true) {
        if (x >= 0 && y >= 0 && x < N && y < N) m[y * N + x] = v;
    }
    bool empty() const { return std::none_of(m.begin(), m.end(), [](quint8 b) { return b != 0; }); }
    Mask& operator|=(const Mask& o) {
        for (int i = 0; i < N * N; ++i) m[i] |= o.m[i];
        return *this;
    }
    Mask& operator&=(const Mask& o) {
        for (int i = 0; i < N * N; ++i) m[i] &= o.m[i];
        return *this;
    }
    Mask& operator-=(const Mask& o) {
        for (int i = 0; i < N * N; ++i) m[i] = m[i] && !o.m[i];
        return *this;
    }
    Mask mirrored() const {
        Mask r;
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) r.m[y * N + x] = m[y * N + (N - 1 - x)];
        return r;
    }
    bool edge(int x, int y) const {  // fora da grade conta como dentro: a roupa não ganha contorno embaixo
        auto in = [&](int a, int b) { return a < 0 || b < 0 || a >= N || b >= N || has(a, b); };
        return !in(x - 1, y) || !in(x + 1, y) || !in(x, y - 1) || !in(x, y + 1);
    }
    bool litEdge(int x, int y) const {  // borda do lado da luz (em cima e à esquerda)
        auto in = [&](int a, int b) { return a < 0 || b < 0 || a >= N || b >= N || has(a, b); };
        return (!in(x - 1, y) || !in(x, y - 1)) && in(x + 1, y) && in(x, y + 1);
    }
};
Mask operator|(Mask a, const Mask& b) { return a |= b; }
Mask operator&(Mask a, const Mask& b) { return a &= b; }
Mask operator-(Mask a, const Mask& b) { return a -= b; }
Mask sym(const Mask& m) { return m | m.mirrored(); }

using Pts = std::vector<QPointF>;

Mask fill(const QPainterPath& p) {
    QImage img(N, N, QImage::Format_Grayscale8);
    img.fill(0);
    {
        QPainterPath q = p;
        q.setFillRule(Qt::WindingFill);
        QPainter pt(&img);
        pt.fillPath(q, Qt::white);
    }
    Mask m;
    for (int y = 0; y < N; ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = 0; x < N; ++x) m.m[y * N + x] = line[x] > 127;
    }
    return m;
}

// Curva suave (Catmull-Rom) pelos pontos dados.
QPainterPath curve(const Pts& p, bool closed = true) {
    QPainterPath path;
    const int n = int(p.size());
    if (n < 2) return path;
    auto at = [&](int i) { return closed ? p[((i % n) + n) % n] : p[std::clamp(i, 0, n - 1)]; };
    path.moveTo(p[0]);
    const int segs = closed ? n : n - 1;
    for (int i = 0; i < segs; ++i) {
        const QPointF p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
        path.cubicTo(p1 + (p2 - p0) / 6.0, p2 - (p3 - p1) / 6.0, p2);
    }
    if (closed) path.closeSubpath();
    return path;
}

Mask blob(const Pts& p) { return fill(curve(p)); }
Mask poly(const Pts& p) {
    QPainterPath path;
    path.moveTo(p[0]);
    for (size_t i = 1; i < p.size(); ++i) path.lineTo(p[i]);
    path.closeSubpath();
    return fill(path);
}
Mask stroke(const Pts& p, double w) {  // traço grosso ao longo de uma curva aberta
    QPainterPathStroker st;
    st.setWidth(w);
    st.setCapStyle(Qt::RoundCap);
    st.setJoinStyle(Qt::RoundJoin);
    return fill(st.createStroke(curve(p, false)));
}
Mask trace(const Pts& p) {  // linha contínua de 1 pixel
    const QPainterPath path = curve(p, false);
    Mask m;
    const int steps = std::max(8, int(path.length() * 3));
    for (int i = 0; i <= steps; ++i) {
        const QPointF q = path.pointAtPercent(i / double(steps));
        m.set(int(std::floor(q.x())), int(std::floor(q.y())));
    }
    return m;
}
Mask ell(double cx, double cy, double rx, double ry) {
    Mask m;
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            const double u = (x + 0.5 - cx) / rx, v = (y + 0.5 - cy) / ry;
            if (u * u + v * v <= 1.0) m.m[y * N + x] = 1;
        }
    return m;
}
Mask box(double x0, double y0, double x1, double y1) {  // pixels com o centro dentro
    Mask m;
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
            if (x + 0.5 >= x0 && x + 0.5 <= x1 && y + 0.5 >= y0 && y + 0.5 <= y1) m.m[y * N + x] = 1;
    return m;
}
Mask rbox(double x0, double y0, double x1, double y1, double r) {
    QPainterPath p;
    p.addRoundedRect(QRectF(x0, y0, x1 - x0, y1 - y0), r, r);
    return fill(p);
}
Mask above(double y) { return box(0, 0, N, y); }
Mask below(double y) { return box(0, y, N, N); }
// Aro: a forma menos ela mesma encolhida em direção ao centro.
Mask ring(const Pts& p, QPointF c, double k) {
    Pts inner;
    for (const QPointF& q : p) inner.push_back(c + (q - c) * k);
    return blob(p) - blob(inner);
}

enum class Tex { None, Strand, Curl, Coil, Braid, Rib, Knit };
struct Light {
    double cx, cy, rx, ry;
};

struct Canvas {
    QImage img{N, N, QImage::Format_ARGB32};
    Canvas() { img.fill(Qt::transparent); }
    void set(int x, int y, const QColor& c) {
        if (x >= 0 && y >= 0 && x < N && y < N) img.setPixelColor(x, y, c);
    }
    void both(int x, int y, const QColor& c) {
        set(x, y, c);
        set(N - 1 - x, y, c);
    }
    QColor at(int x, int y) const { return img.pixelColor(std::clamp(x, 0, N - 1), std::clamp(y, 0, N - 1)); }
    void put(const Mask& m, const QColor& c) {
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
                if (m.has(x, y)) set(x, y, c);
    }
    void tint(const Mask& m, const QColor& c, double t) {
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
                if (m.has(x, y)) set(x, y, mix(at(x, y), c, t));
    }

    // Pinta a máscara com luz vindo do alto à esquerda. O contorno é mais claro do lado da luz.
    void paint(const Mask& m, const Ramp& r, Light l, bool outline = true, Tex tex = Tex::None, int bias = 0) {
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                if (!m.has(x, y)) continue;
                if (outline && m.edge(x, y)) {
                    set(x, y, m.litEdge(x, y) ? r.deep : r.line);
                    continue;
                }
                const double u = (x + 0.5 - l.cx) / l.rx, v = (y + 0.5 - l.cy) / l.ry;
                const double lum = -(0.62 * u + 0.55 * v) + 0.05;
                int tone = (lum > 0.62 ? 1 : lum > -0.28 ? 2 : lum > -0.82 ? 3 : 4) + bias;
                switch (tex) {
                    case Tex::Strand: {  // fios e uma faixa de brilho em zigue-zague
                        const int wave = int(std::lround(1.6 * std::sin(y * 0.3)));
                        if (((x + wave) % 5 + 5) % 5 == 0) ++tone;
                        if (std::abs(v + 0.42 - 0.07 * std::sin(x * 1.3)) < 0.09 && std::abs(u) < 0.78) tone = std::min(tone, 2) - 1;
                        break;
                    }
                    case Tex::Curl: {
                        const double w = std::sin(x * 0.9 + std::sin(y * 0.7) * 1.3) + std::sin(y * 0.9 + std::sin(x * 0.6) * 1.3);
                        if (w > 0.9) --tone;
                        else if (w < -0.9) ++tone;
                        break;
                    }
                    case Tex::Coil: {
                        const double w = std::sin(x * 1.7 + std::sin(y * 1.3) * 1.2) + std::sin(y * 1.7 + std::sin(x * 1.1) * 1.2);
                        if (w > 0.8) --tone;
                        else if (w < -0.8) ++tone;
                        break;
                    }
                    case Tex::Braid:
                        if ((y + (x % 4 < 2 ? 0 : 2)) % 4 == 0) ++tone;
                        break;
                    case Tex::Rib:
                        if (x % 3 == 0) ++tone;
                        break;
                    case Tex::Knit:
                        if ((x + y) % 4 == 0) ++tone;
                        else if ((x - y + 64) % 4 == 0) --tone;
                        break;
                    case Tex::None: break;
                }
                set(x, y, r.tone(std::clamp(tone, 0, 4)));
            }
    }
};

// ---- proporções: variam com o tipo de corpo e o formato do rosto
struct Build {
    double cheek, jaw, chin, chinY, neck, shoulder;
};

Build buildFor(int body, int shape) {
    Build b = body == 1 ? Build{15.4, 12.6, 7.0, 46.5, 6.2, 24} : body == 0 ? Build{14.6, 10.2, 4.2, 45.5, 4.6, 19}
                                                                             : Build{15.0, 11.4, 5.5, 46.0, 5.2, 21};
    switch (shape) {
        case 1: b.cheek += 0.8; b.jaw += 1.8; b.chin += 2.2; b.chinY -= 1.0; break;  // redondo
        case 2: b.jaw += 2.4; b.chin += 3.6; break;                                  // quadrado
        case 3: b.cheek += 0.6; b.jaw -= 1.6; b.chin -= 1.6; break;                  // coração
        case 4: b.cheek -= 1.0; b.jaw -= 0.4; b.chinY += 2.2; break;                 // alongado
        default: break;
    }
    return b;
}

Mask faceMask(const Build& b) {
    const Pts right = {{C, 9.5},           {C + 7.5, 10.6},           {C + 12.8, 14.5},
                       {C + b.cheek, 22},  {C + b.cheek, 30},         {C + b.cheek - 1.0, 36},
                       {C + b.jaw, 41.5},  {C + b.chin, b.chinY - 0.8}, {C, b.chinY}};
    Pts all = right;
    for (int i = int(right.size()) - 2; i >= 1; --i) all.push_back({2 * C - right[i].x(), right[i].y()});
    return blob(all);
}

// ---- cabelo: a parte de trás fica atrás do rosto, a da frente por cima.
struct HairShape {
    Mask back, front, stubble;  // stubble: cabelo raspado, só tinge a pele
    Tex tex = Tex::Strand, backTex = Tex::Strand;
    Mask tie;  // elástico ou fita, na cor do chapéu
};

Mask cap(double grow = 1.6, double hairline = 18.0) { return ell(C, 24.5, 15.6 + grow, 16 + grow) & above(hairline); }
Mask temples(double to, double w = 3.6) { return sym(box(C - 17.4, 16, C - 17.4 + w, to)) & ell(C, 24.5, 17.6, 18.5); }
Mask partFringe() {  // franja repartida ao meio
    return sym(blob({{C, 10}, {C - 1, 17.5}, {C - 7, 19}, {C - 13, 24}, {C - 16.5, 18}, {C - 12, 11}}));
}
Mask straightBangs() {
    return blob({{C - 14.5, 15}, {C - 14.5, 22.5}, {C - 7, 23.6}, {C, 23}, {C + 7, 23.6}, {C + 14.5, 22.5}, {C + 14.5, 15}});
}

HairShape hairShape(int style) {
    HairShape h;
    Mask& f = h.front;
    Mask& b = h.back;
    const Mask shaved = cap(0.6, 17.5) | temples(26);
    switch (style) {
        case 1:  // curto
            f = cap() | temples(26) |
                blob({{C - 13, 17}, {C - 8, 20.5}, {C - 2, 19}, {C + 4, 20.5}, {C + 13, 17}, {C + 13, 12}, {C - 13, 12}});
            break;
        case 2:  // bagunçado
            f = cap(2.4) | temples(27) |
                poly({{C - 14, 17}, {C - 11, 22}, {C - 8, 18.5}, {C - 5, 23}, {C - 1, 18.5}, {C + 3, 22.5}, {C + 6, 18.5},
                      {C + 10, 22}, {C + 14, 17}, {C + 14, 13}, {C - 14, 13}});
            for (double x0 : {-11.0, -5.0, 1.0, 7.0}) f |= poly({{C + x0 - 2.5, 8.5}, {C + x0 + 1, 3.2}, {C + x0 + 3.5, 8.5}});
            break;
        case 3:  // comprido
            b = rbox(C - 18.5, 12, C + 18.5, 59, 5);
            f = cap() | partFringe() |
                sym(blob({{C - 17.6, 16}, {C - 12, 17}, {C - 13.5, 26}, {C - 15, 40}, {C - 15.5, 55}, {C - 18.6, 55}, {C - 18.6, 24}}));
            break;
        case 4:  // chanel
            b = rbox(C - 19, 12, C + 19, 44, 6);
            f = cap(2) | straightBangs() |
                sym(blob({{C - 18.6, 16}, {C - 13, 17}, {C - 14.2, 28}, {C - 13.2, 41}, {C - 18, 43.5}, {C - 19.6, 34}}));
            break;
        case 5:  // coque
            b = ell(C, 6.5, 6.5, 5.5);
            f = cap(1.6, 17) | temples(24);
            break;
        case 6:  // rabo de cavalo
            b = blob({{C + 13, 12}, {C + 22, 10}, {C + 26, 20}, {C + 24, 34}, {C + 19, 44}, {C + 17, 36}, {C + 19, 24}, {C + 15, 17}});
            f = cap(1.6, 17) | temples(24);
            h.tie = box(C + 14.5, 11.5, C + 17.5, 15.5);
            break;
        case 7:  // moicano
            h.stubble = shaved;
            f = poly({{C - 5.5, 18}, {C - 6.5, 9}, {C - 5, 3}, {C - 3, 6.5}, {C - 1.5, 0.5}, {C, 5}, {C + 1.5, 0.5}, {C + 3, 6.5},
                      {C + 5, 3}, {C + 6.5, 9}, {C + 5.5, 18}});
            break;
        case 8:  // black power
            b = ell(C, 19, 25, 20);
            f = (ell(C, 19, 25, 20) - ell(C, 32, 15.2, 14.6)) & above(33);
            h.tex = h.backTex = Tex::Coil;
            break;
        case 9:  // de lado
            f = cap(1.8) | temples(26) |
                blob({{C - 14, 16}, {C - 12.5, 22}, {C - 6, 20}, {C + 2, 22.5}, {C + 10, 21}, {C + 15, 25}, {C + 16.5, 17},
                      {C + 10, 11}, {C - 8, 11}});
            break;
        case 10:  // cacheado
            b = ell(C, 22, 20.5, 17);
            f = (ell(C, 20, 20.5, 15.5) - ell(C, 31.5, 14.6, 13)) & above(30);
            for (double dx : {-12.0, -6.0, 0.0, 6.0, 12.0}) f |= ell(C + dx, 18.5, 3.6, 3.2);
            h.tex = h.backTex = Tex::Curl;
            break;
        case 11:  // tranças
            b = sym(rbox(C - 21.5, 18, C - 15.5, 61, 2.8));
            f = cap() | temples(22) | partFringe();
            h.backTex = Tex::Braid;
            break;
        case 12:  // raspado
            h.stubble = shaved;
            break;
        case 13: {  // ondulado longo
            Pts left, rightSide;
            for (double y = 12; y <= 60; y += 3) left.push_back({C - 19.5 - 1.8 * std::sin(y * 0.42), y});
            for (double y = 60; y >= 12; y -= 3) rightSide.push_back({C + 19.5 + 1.8 * std::sin(y * 0.42), y});
            Pts all = left;
            all.insert(all.end(), rightSide.begin(), rightSide.end());
            b = blob(all);
            Pts lock;
            for (double y = 16; y <= 55; y += 3) lock.push_back({C - 19.5 - 1.8 * std::sin(y * 0.42), y});
            for (double y = 55; y >= 16; y -= 3) lock.push_back({C - 14.6 - 1.2 * std::sin(y * 0.42 + 0.6) + (y < 24 ? (24 - y) * 0.4 : 0), y});
            f = cap(1.8) | partFringe() | sym(blob(lock));
            break;
        }
        case 14:  // topete
            h.stubble = shaved;
            f = blob({{C - 12, 17.5}, {C - 13, 11}, {C - 9, 5}, {C - 1, 2.5}, {C + 8, 3.5}, {C + 13, 8}, {C + 12.5, 15},
                      {C + 8, 18.5}, {C, 16.5}, {C - 6, 18.5}});
            break;
        case 15:  // pixie
            f = cap(1.4) | temples(29, 4.2) |
                blob({{C - 14.5, 16}, {C - 13.5, 23.5}, {C - 8, 21.5}, {C - 1, 19.5}, {C + 6, 17.5}, {C + 14, 16}, {C + 12, 11}, {C - 10, 11}});
            break;
        case 16:  // comprido com franja
            b = rbox(C - 18.5, 12, C + 18.5, 59, 5);
            f = cap() | straightBangs() |
                sym(blob({{C - 17.6, 16}, {C - 13, 17}, {C - 14.4, 26}, {C - 15, 40}, {C - 15.5, 55}, {C - 18.6, 55}, {C - 18.6, 24}}));
            break;
        case 17:  // maria-chiquinha
            b = sym(blob({{C - 17, 20}, {C - 24, 18}, {C - 28, 26}, {C - 27, 40}, {C - 22, 50}, {C - 19, 42}, {C - 21, 30}, {C - 17, 26}}));
            f = cap() | temples(23) | partFringe();
            h.tie = sym(box(C - 20.5, 18.5, C - 17, 22));
            break;
        case 18:  // coques duplos
            b = sym(ell(C - 12.5, 7.5, 6, 5.5));
            f = cap() | temples(23) | partFringe();
            break;
        case 19: {  // dreads
            for (double dx : {-21.0, -17.2})
                b |= sym(rbox(C + dx, 16, C + dx + 3.2, dx < -20 ? 52 : 58, 1.6));
            for (double dx : {-13.0, -8.6, -4.2, 0.2, 4.6, 9.0}) b |= rbox(C + dx, 12, C + dx + 3.6, 24, 1.6);
            f = cap(2.0, 18.5) | sym(rbox(C - 18.8, 15, C - 15.4, 47, 1.7)) | sym(rbox(C - 15.0, 14, C - 11.8, 28, 1.6)) |
                rbox(C - 11, 12, C - 7.6, 23.5, 1.6) | rbox(C + 3, 12, C + 6.4, 22, 1.6);
            h.tex = h.backTex = Tex::Braid;
            break;
        }
        case 20:  // undercut
            h.stubble = shaved;
            f = blob({{C - 11, 13}, {C - 9, 6}, {C, 3.5}, {C + 10, 5}, {C + 15, 11}, {C + 16.5, 20}, {C + 13, 26}, {C + 9, 19.5},
                      {C + 2, 17}, {C - 6, 17.5}});
            break;
        case 21:  // penteado para trás
            f = cap(1.2, 16.2) | temples(25) | poly({{C - 3.5, 16}, {C, 19}, {C + 3.5, 16}});
            break;
        case 22:  // médio com pontas viradas
            b = rbox(C - 20, 13, C + 20, 50, 7);
            f = cap(1.8) |
                sym(blob({{C - 18.6, 16}, {C - 12.5, 17}, {C - 14, 28}, {C - 14.5, 40}, {C - 17, 47}, {C - 22.5, 48.5},
                          {C - 20.5, 36}, {C - 19.5, 24}})) |
                blob({{C - 14, 16}, {C - 12.5, 22}, {C - 6, 20}, {C + 2, 22.5}, {C + 10, 21}, {C + 15, 25}, {C + 16.5, 17},
                      {C + 10, 11}, {C - 8, 11}});
            break;
        case 23:  // cacheado longo
            b = ell(C, 32, 25, 27) & below(8);
            f = ((ell(C, 22, 22, 17) - ell(C, 31.5, 14.4, 13)) & above(32)) | sym(ell(C - 19.5, 40, 6, 14));
            h.tex = h.backTex = Tex::Curl;
            break;
        case 24:  // coque samurai
            h.stubble = shaved;
            b = ell(C, 5.5, 4.8, 4.4);
            f = cap(1.2, 15.2);
            break;
        case 25:  // tigela
            f = ell(C, 22.5, 17.8, 16.5) & above(24);
            break;
        case 26:  // espetado
            f = cap(1.6) | temples(25);
            for (int deg = -75; deg <= 75; deg += 25) {
                const double a = deg * M_PI / 180.0, sx = std::sin(a), cy = -std::cos(a);
                const QPointF base(C + 15 * sx, 22 + 14 * cy), tip(C + 23 * sx, 22 + 22 * cy);
                const QPointF side(-cy * 3.4, sx * 3.4);
                f |= poly({base - side, tip, base + side});
            }
            f |= poly({{C - 13, 17}, {C - 9, 22}, {C - 5, 17.5}, {C - 1, 22}, {C + 3, 17.5}, {C + 7, 21.5}, {C + 13, 17}, {C, 12}});
            break;
        case 27:  // raspado de um lado
            h.stubble = shaved & box(C + 4, 0, N, N);
            f = (cap(1.8) & box(0, 0, C + 7, N)) |
                blob({{C - 18.6, 14}, {C - 12, 16.5}, {C - 13, 28}, {C - 14, 42}, {C - 16, 51}, {C - 21.5, 51}, {C - 20.5, 30}}) |
                blob({{C + 7, 10}, {C + 8, 17.5}, {C - 2, 20}, {C - 9, 25}, {C - 14, 18}, {C - 8, 10}});
            break;
        case 28:  // puffs
            b = sym(ell(C - 15.5, 9, 8.5, 8));
            f = cap() | temples(23) | partFringe();
            h.backTex = Tex::Coil;
            break;
        case 29:  // entradas
            f = sym(blob({{C - 17.6, 17}, {C - 15.5, 12.5}, {C - 10.5, 11.5}, {C - 12.4, 18}, {C - 13.4, 27}, {C - 17.4, 27}}));
            break;
        case 30:  // franja sobre o olho
            f = cap(1.8) | temples(29) |
                blob({{C - 15, 14}, {C - 16, 24}, {C - 13.5, 33}, {C - 8, 34}, {C - 2, 28}, {C + 5, 22.5}, {C + 12, 20},
                      {C + 15.5, 23}, {C + 16.5, 15}, {C + 8, 10}, {C - 8, 10}});
            break;
        default: break;  // careca
    }
    return h;
}

// ---- partes do rosto
constexpr double kEyeY = 29.5, kEyeDx = 7.2;

void drawEye(Canvas& c, bool right, int style, const Ramp& iris, const Ramp& skin) {
    const double cx = right ? C + kEyeDx : C - kEyeDx, cy = kEyeY;
    const double out = right ? 1 : -1;  // direção do canto de fora
    const bool closedHappy = style == 1 || (style == 9 && !right);
    if (closedHappy) {
        c.put(stroke({{cx - 3.6, cy + 1.0}, {cx, cy - 1.6}, {cx + 3.6, cy + 1.0}}, 1.5), kInk);
        return;
    }
    if (style == 5) {
        c.put(stroke({{cx - 3.6, cy - 0.4}, {cx, cy + 1.5}, {cx + 3.6, cy - 0.4}}, 1.4), kInk);
        c.set(int(cx + out * 4.2), int(cy + 1.2), kInk);
        return;
    }
    double rx = 3.9, ry = 2.7, ir = 2.4;
    if (style == 2) { ry = 3.4; ir = 2.1; }
    if (style == 6) { rx = 4.2; ry = 2.0; ir = 2.2; }
    if (style == 7) { rx = 4.3; ry = 3.6; ir = 3.1; }
    const Mask sclera = ell(cx, cy, rx, ry);
    const double icx = cx - out * 0.3, icy = cy + 0.2;
    const Mask irisM = ell(icx, icy, ir, ir) & sclera;
    const Mask pupil = ell(icx, icy, ir * 0.48, ir * 0.48) & sclera;
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            if (!sclera.has(x, y)) continue;
            QColor col = y + 0.5 < cy - ry * 0.35 ? mix(kWhite, skin.deep, 0.35) : kWhite;
            if (irisM.has(x, y)) col = y + 0.5 < icy - 0.5 ? iris.deep : y + 0.5 > icy + ir * 0.45 ? iris.hi : iris.base;
            if (pupil.has(x, y)) col = kInk;
            if (sclera.edge(x, y)) {  // pálpebra de cima escura, de baixo suave
                if (y + 0.5 < cy) col = kInk;
                else if (!irisM.has(x, y)) col = mix(skin.shade, skin.deep, 0.5);
            }
            c.set(x, y, col);
        }
    c.set(int(icx - 1.4), int(icy - 1.3), kWhite);  // brilho
    if (style == 7) c.set(int(icx + 1.2), int(icy + 1.0), iris.glow);
    if (style == 3 || style == 7) {  // cílios
        c.put(stroke({{cx - 3.4, cy - ry + 0.9}, {cx, cy - ry - 0.3}, {cx + 3.4, cy - ry + 0.9}}, 1.3), kInk);
        c.set(int(cx + out * (rx + 0.6)), int(cy - ry + 0.2), kInk);
        c.set(int(cx + out * (rx + 1.6)), int(cy - ry - 0.8), kInk);
    }
    if (style == 4) {  // sonolento: a pálpebra cobre a metade de cima
        const Mask lid = sclera & above(cy - 0.2);
        c.put(lid, skin.shade);
        c.put(sclera & box(0, cy - 0.7, N, cy + 0.3), kInk);
    }
    if (style == 8) c.put(trace({{cx - 3.2, cy + ry + 1.2}, {cx, cy + ry + 2.3}, {cx + 3.2, cy + ry + 1.2}}), skin.shade);
}

void drawBrows(Canvas& c, int style, const QColor& col) {
    // altura do lado de fora, do meio e do lado de dentro; espessura
    static const double shapes[][4] = {{24.6, 23.2, 23.8, 1.5}, {24.6, 23.0, 23.8, 2.5}, {24.4, 23.2, 23.8, 1.0},
                                       {23.6, 21.4, 22.8, 1.5}, {22.4, 23.4, 25.4, 1.9}, {25.4, 23.6, 22.2, 1.5},
                                       {23.6, 23.6, 23.6, 1.7}, {25.2, 21.8, 24.4, 1.3}, {0, 23.3, 23.6, 2.6}};
    const double* s = shapes[std::clamp(style, 0, 8)];
    const double cx = C - kEyeDx;
    const Mask brow = style == 8 ? stroke({{cx - 0.2, s[1]}, {cx + 3.2, s[2]}}, s[3])
                                 : stroke({{cx - 4.4, s[0]}, {cx - 0.8, s[1]}, {cx + 4.0, s[2]}}, s[3]);
    c.put(sym(brow), col);
}

void drawNose(Canvas& c, int style, const Ramp& skin) {
    const int x = int(C);
    switch (style) {
        case 1:  // arredondado
            c.put(ell(C, 35, 2.7, 2.3), skin.hi);
            c.put(trace({{C - 2.6, 35.6}, {C, 37.6}, {C + 2.6, 35.6}}), skin.shade);
            c.set(x - 2, 36, skin.deep);
            c.set(x + 1, 36, skin.deep);
            c.set(x - 1, 34, skin.glow);
            break;
        case 2:  // comprido
            c.put(trace({{C + 1.6, 26.5}, {C + 2.2, 32}, {C + 2.6, 36.5}}), skin.shade);
            c.put(trace({{C - 2.4, 37.6}, {C, 38.6}, {C + 2.6, 37.4}}), skin.deep);
            c.set(x - 1, 35, skin.hi);
            c.set(x - 1, 36, skin.hi);
            break;
        case 3:  // largo
            c.put(trace({{C - 4.4, 35}, {C - 3.6, 37}, {C - 1.6, 36.8}}), skin.shade);
            c.put(trace({{C + 4.4, 35}, {C + 3.6, 37}, {C + 1.6, 36.8}}), skin.shade);
            c.put(trace({{C - 1.4, 37.6}, {C + 1.4, 37.6}}), skin.shade);
            c.set(x - 3, 36, skin.deep);
            c.set(x + 2, 36, skin.deep);
            c.set(x - 1, 34, skin.hi);
            c.set(x, 34, skin.hi);
            break;
        case 4:  // pontudo
            c.put(trace({{C + 0.6, 28.5}, {C - 1.2, 33}, {C - 2.8, 36.4}}), skin.shade);
            c.put(trace({{C - 2.8, 36.6}, {C - 0.5, 37.4}, {C + 1.8, 36.6}}), skin.deep);
            c.set(x, 35, skin.hi);
            break;
        default:  // suave
            c.put(trace({{C + 1.8, 31}, {C + 2.2, 35.4}}), skin.shade);
            c.put(trace({{C - 1.6, 37.2}, {C + 1.6, 37.2}}), skin.shade);
            c.set(x - 3, 36, skin.deep);
            c.set(x + 2, 36, skin.deep);
            c.set(x - 1, 34, skin.hi);
            c.set(x - 1, 35, skin.hi);
            break;
    }
}

void drawMouth(Canvas& c, int style, const Ramp& lip, const Ramp& skin, bool painted) {
    const QColor dark("#5a1f2a"), tongue("#e0707a");
    const double y = 40.6;
    auto lowerLip = [&] {
        if (painted) c.tint(ell(C, y + 1.9, 3.6, 1.3), lip.base, 0.75);
        else c.put(trace({{C - 2, y + 3}, {C + 2, y + 3}}), skin.shade);
    };
    const QColor lineCol = painted ? lip.deep : mix(lip.deep, kInk, 0.35);
    switch (style) {
        case 1:  // neutra
            lowerLip();
            c.put(stroke({{C - 4, y}, {C + 4, y}}, 1.3), lineCol);
            break;
        case 2: {  // rindo
            const Mask open = ell(C, y - 0.6, 5.6, 4.6) & below(y - 1);
            c.put(open, dark);
            c.put(open & above(y + 0.6), kWhite);
            c.put(ell(C, y + 3.2, 3, 1.6) & open, tongue);
            for (int yy = 0; yy < N; ++yy)
                for (int xx = 0; xx < N; ++xx)
                    if (open.has(xx, yy) && open.edge(xx, yy)) c.set(xx, yy, lineCol);
            break;
        }
        case 3:  // de lado
            lowerLip();
            c.put(stroke({{C - 4, y + 0.6}, {C + 1, y + 0.6}, {C + 5, y - 1.6}}, 1.3), lineCol);
            break;
        case 4:  // triste
            c.put(stroke({{C - 4.6, y + 1.6}, {C, y - 0.6}, {C + 4.6, y + 1.6}}, 1.3), lineCol);
            break;
        case 5: {  // dentes
            const Mask teeth = rbox(C - 5.5, y - 1.8, C + 5.5, y + 2.2, 1.8);
            c.put(teeth, kWhite);
            for (int yy = 0; yy < N; ++yy)
                for (int xx = 0; xx < N; ++xx)
                    if (teeth.has(xx, yy)) {
                        if (teeth.edge(xx, yy)) c.set(xx, yy, lineCol);
                        else if ((xx - int(C) + 64) % 3 == 1) c.set(xx, yy, QColor("#d9dbe6"));
                    }
            break;
        }
        case 6:    // lábios cheios
        case 9: {  // biquinho
            const double k = style == 9 ? 0.6 : 1.0;
            const Mask upper = blob({{C - 5 * k, y}, {C - 2 * k, y - 1.9}, {C, y - 1.1}, {C + 2 * k, y - 1.9}, {C + 5 * k, y}, {C, y + 0.5}});
            const Mask lower = blob({{C - 5 * k, y}, {C, y + 0.5}, {C + 5 * k, y}, {C + 2.6 * k, y + 2.9}, {C - 2.6 * k, y + 2.9}});
            c.put(lower, lip.base);
            c.put(upper, lip.shade);
            c.put(trace({{C - 4.6 * k, y + 0.1}, {C, y + 0.5}, {C + 4.6 * k, y + 0.1}}), lip.line);
            c.set(int(C - 1.5), int(y + 1.6), lip.glow);
            c.set(int(C - 0.5), int(y + 1.6), lip.hi);
            break;
        }
        case 7: {  // surpresa
            const Mask o = ell(C, y + 0.4, 2.4, 2.8);
            c.put(o, dark);
            for (int yy = 0; yy < N; ++yy)
                for (int xx = 0; xx < N; ++xx)
                    if (o.has(xx, yy) && o.edge(xx, yy)) c.set(xx, yy, lineCol);
            break;
        }
        case 8: {  // língua de fora
            c.put(stroke({{C - 5, y - 1}, {C, y + 1.2}, {C + 5, y - 1}}, 1.3), lineCol);
            const Mask t = ell(C + 1, y + 2.6, 2.2, 2.2) & below(y + 1.2);
            c.put(t, tongue);
            c.put(trace({{C + 1, y + 1.6}, {C + 1, y + 3.4}}), QColor("#b84a58"));
            break;
        }
        default:  // sorriso
            lowerLip();
            c.put(stroke({{C - 5, y - 1}, {C, y + 1.2}, {C + 5, y - 1}}, 1.3), lineCol);
            c.set(int(C - 6), int(y - 1.6), skin.shade);
            c.set(int(C + 5), int(y - 1.6), skin.shade);
            break;
    }
}

void drawGlasses(Canvas& c, int style, const Build& b) {
    if (style == 0) return;
    const double lx = C - kEyeDx, y = kEyeY;
    const QColor frame = kInk, gold("#d9a520");
    auto arms = [&](double fromDx, const QColor& col) {  // hastes até as orelhas
        c.put(sym(box(C - b.cheek - 0.6, y - 1.6, C - fromDx, y - 0.6)), col);
    };
    auto bridge = [&](double half, const QColor& col) { c.put(box(C - half, y - 1.6, C + half, y - 0.4), col); };
    switch (style) {
        case 1:  // redondos
        case 8: {  // redondos pequenos
            const double r = style == 1 ? 5.4 : 3.9;
            c.tint(sym(ell(lx, y, r - 1, r - 1)), kWhite, 0.18);
            c.put(sym(ell(lx, y, r, r) - ell(lx, y, r - 1.2, r - 1.2)), style == 1 ? frame : gold);
            bridge(kEyeDx - r + 0.4, style == 1 ? frame : gold);
            arms(kEyeDx + r - 0.4, style == 1 ? frame : gold);
            break;
        }
        case 2:  // quadrados
            c.tint(sym(rbox(lx - 4.6, y - 3.6, lx + 4.6, y + 3.8, 1)), kWhite, 0.18);
            c.put(sym(rbox(lx - 5.6, y - 4.6, lx + 5.6, y + 4.8, 1.6) - rbox(lx - 4.4, y - 3.4, lx + 4.4, y + 3.6, 1)), frame);
            bridge(1.8, frame);
            arms(kEyeDx + 5.2, frame);
            break;
        case 3: {  // escuros
            const Mask lens = sym(rbox(lx - 5.8, y - 4, lx + 5.8, y + 4.4, 2.2));
            c.put(lens, QColor("#23232e"));
            c.put(sym(trace({{lx - 3.5, y - 0.5}, {lx - 0.5, y - 3}})) & lens, QColor("#5b5d75"));
            c.put(sym(trace({{lx - 2, y + 1.5}, {lx + 1.5, y - 1.8}})) & lens, QColor("#44465c"));
            bridge(1.8, frame);
            arms(kEyeDx + 5.4, frame);
            break;
        }
        case 4: {  // monóculo
            const double rxm = C + kEyeDx;
            c.tint(ell(rxm, y, 4.6, 4.6), kWhite, 0.2);
            c.put(ell(rxm, y, 5.6, 5.6) - ell(rxm, y, 4.4, 4.4), gold);
            c.put(trace({{rxm + 4.5, y + 3.5}, {rxm + 6.5, y + 12}, {rxm + 5, y + 20}}), gold);
            break;
        }
        case 5:  // tapa-olho
            c.put(stroke({{C - b.cheek, y - 9}, {lx, y - 1}, {C + b.cheek, y - 3}}, 1.3), frame);
            c.put(blob({{lx - 5.4, y - 3.4}, {lx + 5.4, y - 3.4}, {lx + 4.2, y + 3.6}, {lx, y + 5}, {lx - 4.2, y + 3.6}}), frame);
            break;
        case 6: {  // gatinho
            const Pts left = {{lx + 5, y - 2.6}, {lx - 3, y - 3.2}, {lx - 7.4, y - 5.6}, {lx - 5.8, y + 2}, {lx - 1, y + 4}, {lx + 4, y + 3.2}};
            c.tint(sym(blob(left)), kWhite, 0.15);
            c.put(sym(ring(left, {lx - 0.6, y + 0.2}, 0.72)), QColor("#8a1f3a"));
            bridge(2.4, QColor("#8a1f3a"));
            arms(kEyeDx + 6.2, QColor("#8a1f3a"));
            break;
        }
        case 7: {  // aviador
            const Pts left = {{lx - 5.2, y - 3.2}, {lx + 5.2, y - 3.2}, {lx + 5.8, y + 0.6}, {lx + 2.2, y + 5}, {lx - 2.2, y + 5}, {lx - 5.8, y + 0.6}};
            const Mask lens = sym(blob(left));
            c.tint(lens, QColor("#2a3a36"), 0.62);
            c.put(sym(ring(left, {lx, y + 0.6}, 0.82)), gold);
            c.put(box(C - 2.4, y - 3.4, C + 2.4, y - 2.4), gold);
            bridge(2.4, gold);
            arms(kEyeDx + 5.2, gold);
            break;
        }
        default: {  // meio aro
            c.tint(sym(rbox(lx - 4.8, y - 3.4, lx + 4.8, y + 3.8, 1.6)), kWhite, 0.15);
            c.put(sym(rbox(lx - 5.6, y - 4.6, lx + 5.6, y - 2.2, 1)), frame);
            c.put(sym(trace({{lx - 5, y - 2}, {lx - 4.4, y + 3}, {lx, y + 4.2}, {lx + 4.4, y + 3}, {lx + 5, y - 2}})), QColor("#8d8fa6"));
            bridge(1.8, frame);
            arms(kEyeDx + 5.2, frame);
            break;
        }
    }
}

// Onde o chapéu corta o cabelo (0 = não corta, -1 = esconde tudo).
double hatClip(int hat) {
    switch (hat) {
        case 1: return 17;
        case 2: return 15.5;
        case 3: case 7: case 8: return 15;
        case 9: return 14;
        case 10: return 15.5;
        case 12: return -1;
        default: return 0;
    }
}

void drawHat(Canvas& c, int style, const Ramp& r, const Build& b) {
    const Ramp dark = ramp(r.deep);
    const Light top{C - 5, 9, 20, 13};
    switch (style) {
        case 1: {  // gorro
            c.paint(ell(C, 17, 17.6, 14) & above(17), r, top, true, Tex::Rib);
            c.paint(rbox(C - 18.4, 14.5, C + 18.4, 20.6, 2.2), r, {C - 6, 16, 22, 5}, true, Tex::None, -1);
            c.paint(ell(C, 3.8, 3.6, 3.4), ramp(QColor("#f1f1f4")), {C, 3.8, 4, 4});
            break;
        }
        case 2: {  // boné
            c.paint(ell(C, 16.5, 17.2, 13) & above(17), r, top);
            c.paint(blob({{C - 17, 15.4}, {C - 8, 13.8}, {C + 8, 13.8}, {C + 20, 15.8}, {C + 25, 19.6}, {C + 10, 18.8}, {C - 17, 18.8}}),
                    dark, {C, 16, 26, 4});
            c.paint(ell(C, 3.8, 1.8, 1.2), r, {C, 4, 2, 2});
            c.put(trace({{C, 5}, {C, 13}}), r.shade);
            break;
        }
        case 3: {  // chapéu
            c.paint(blob({{C - 12.5, 15}, {C - 11.5, 5}, {C - 4, 3.2}, {C, 5}, {C + 4, 3.2}, {C + 11.5, 5}, {C + 12.5, 15}}), r, top);
            c.put(box(C - 12, 11.2, C + 12, 14.6), dark.deep);
            c.paint(ell(C, 16.2, 23.5, 3.6), r, {C - 6, 15, 24, 4});
            break;
        }
        case 4:  // faixa
            c.paint(ell(C, 24.5, 17.8, 18) & box(0, 12.4, N, 16.2), r, {C - 5, 13, 20, 5});
            break;
        case 5: {  // laço
            const double bx = C + 7, by = 9;
            c.paint(poly({{bx, by}, {bx - 6.5, by - 4.6}, {bx - 6.5, by + 4.6}}), r, {bx - 4, by - 1, 5, 5});
            c.paint(poly({{bx, by}, {bx + 6.5, by - 4.6}, {bx + 6.5, by + 4.6}}), r, {bx + 3, by - 1, 5, 5});
            c.paint(ell(bx, by, 2, 2.4), r, {bx, by, 2, 2}, true, Tex::None, 1);
            break;
        }
        case 6: {  // coroa
            const Ramp gold = ramp(QColor("#f2c230"));
            c.paint(poly({{C - 11.5, 14.5}, {C - 11.5, 5}, {C - 7, 9.5}, {C - 3.4, 3.2}, {C, 9}, {C + 3.4, 3.2}, {C + 7, 9.5},
                          {C + 11.5, 5}, {C + 11.5, 14.5}}),
                    gold, {C - 4, 8, 13, 8});
            for (double dx : {-6.5, 0.0, 6.5}) c.put(ell(C + dx, 11.4, 1.4, 1.4), r.base);
            for (double dx : {-6.5, 0.0, 6.5}) c.set(int(C + dx - 0.6), 10, r.glow);
            break;
        }
        case 7:  // boina
            c.paint(blob({{C - 17, 15}, {C - 19.5, 9}, {C - 10, 3.5}, {C + 4, 2.8}, {C + 17.5, 7}, {C + 19.5, 13}, {C + 13, 16.5}, {C - 10, 16.8}}),
                    r, top);
            c.put(box(C + 2, 1.4, C + 3.4, 3.4), r.line);
            c.put(trace({{C - 14, 16}, {C, 16.4}, {C + 13, 15.6}}), r.line);
            break;
        case 8:  // caubói
            c.paint(blob({{C - 27, 11}, {C - 18, 15.2}, {C, 16.6}, {C + 18, 15.2}, {C + 27, 11}, {C + 21, 18.6}, {C, 20.2}, {C - 21, 18.6}}),
                    r, {C - 6, 15, 28, 5});
            c.paint(blob({{C - 10.5, 15.5}, {C - 11.5, 6}, {C - 5, 2.8}, {C, 5.6}, {C + 5, 2.8}, {C + 11.5, 6}, {C + 10.5, 15.5}}), r, top);
            c.put(box(C - 10.6, 12.4, C + 10.6, 15), dark.deep);
            break;
        case 9:  // cartola
            c.paint(rbox(C - 10.6, 0, C + 10.6, 15, 1.6), r, {C - 4, 6, 12, 10});
            c.put(box(C - 10, 10.4, C + 10, 13.4), dark.deep);
            c.paint(rbox(C - 17.5, 13.4, C + 17.5, 17, 1.7), r, {C - 6, 15, 18, 3});
            break;
        case 10: {  // bandana
            const Mask band = ell(C, 24.5, 17.6, 17.8) & above(17.4);
            c.paint(band, r, top);
            for (int y = 8; y < 17; y += 3)
                for (int x = (y / 3) % 2 * 2 + 16; x < 48; x += 4)
                    if (band.has(x, y) && !band.edge(x, y)) c.set(x, y, kWhite);
            c.paint(blob({{C + 16, 15}, {C + 21.5, 13.5}, {C + 24, 17}, {C + 22, 23}, {C + 19, 20}, {C + 16.5, 18.5}}), r, {C + 19, 16, 5, 5});
            break;
        }
        case 11: {  // fones
            const Ramp grey = ramp(QColor("#3a3c4c"));
            c.paint((ell(C, 23, 19.6, 19.2) - ell(C, 23.5, 17.2, 16.8)) & above(24), grey, top);
            c.paint(sym(rbox(C - 21.8, 23.5, C - 15.6, 35.5, 2.6)), r, {C, 28, 23, 8});
            break;
        }
        case 12: {  // hijab
            const Mask cloth = (ell(C, 26, 20.5, 21.5) |
                                blob({{C - 19, 30}, {C - 22, 50}, {C - 13, 59}, {C, 61}, {C + 13, 59}, {C + 22, 50}, {C + 19, 30}})) -
                               (ell(C, 31.2, b.cheek - 2.2, 14.4) & faceMask(b));
            c.paint(cloth, r, {C - 5, 26, 24, 34});
            c.put(trace({{C - 9, 47}, {C - 2, 54}, {C + 6, 58}}), r.shade);
            c.put(trace({{C + 10, 46}, {C + 5, 53}}), r.shade);
            break;
        }
        default: break;
    }
}
}  // namespace

QImage PixelAvatar::render(int size) const {
    Canvas c;
    const Build b = buildFor(body, shape);
    const Ramp skinR = ramp(pick(skin, kSkin));
    const Ramp hairR = ramp(pick(hairColor, kHair));
    const QColor clothC = pick(shirt, kCloth);
    const Ramp clothR = ramp(clothC);
    const Ramp accentR = ramp(pick(hatColor, kCloth));
    const Ramp whiteR = ramp(QColor("#f1f1f4"));
    const Ramp goldR = ramp(QColor("#e8b530"));
    const bool painted = lips != 0;
    const Ramp lipR = painted ? ramp(pick(lips, kLips)) : ramp(mix(skinR.base, QColor("#b8545a"), 0.45));
    const Light skinL{C - 2, 28, 30, 34};  // luz suave: só a borda de baixo à direita escurece

    // ---- Fundo em degradê com pontilhado, mais claro atrás da cabeça.
    const Ramp backR = ramp(pick(background, kBack));
    static const int bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            const double dx = (x + 0.5 - C) / 30, dy = (y + 0.5 - 26) / 30;
            double t = y / double(N - 1) * 2.0 - (dx * dx + dy * dy < 1 ? 0.5 : 0.0);
            t = std::clamp(t, 0.0, 2.0);
            int level = int(t);
            if (t - level > (bayer[y % 4][x % 4] + 0.5) / 16.0) ++level;
            c.set(x, y, level <= 0 ? backR.hi : level == 1 ? backR.base : backR.shade);
        }

    HairShape hair = hairShape(hairStyle);
    if (const double clip = hatClip(hat); clip != 0) {
        const Mask keep = clip < 0 ? Mask{} : below(clip);
        hair.front &= keep;
        hair.back &= keep;
        hair.stubble &= keep;
        hair.tie &= keep;
    }
    if (!hair.back.empty()) c.paint(hair.back, ramp(hairR.shade), {C - 4, 30, 26, 32}, true, hair.backTex);

    // ---- Corpo: capuz atrás, pescoço, tronco e roupa.
    const double nw = b.neck, sw = b.shoulder;
    const Mask trunk = blob({{C - nw - 1, 49.5}, {C - sw * 0.62, 51.5}, {C - sw, 55.5}, {C - sw - 2, 62}, {C - sw - 2, 70},
                             {C + sw + 2, 70}, {C + sw + 2, 62}, {C + sw, 55.5}, {C + sw * 0.62, 51.5}, {C + nw + 1, 49.5}});
    const Light bodyL{C - 6, 56, sw + 4, 12};
    if (clothes == 3) c.paint(ell(C, 47.5, nw + 9.5, 7.5), clothR, {C - 3, 46, 14, 8}, true, Tex::None, 1);

    const Mask neck = box(C - nw, 40, C + nw, 53);
    Mask neckline;  // parte do tronco onde a pele aparece
    switch (clothes) {
        case 0: case 8: case 6: case 9: neckline = ell(C, 50.5, nw + 2.4, 3.4); break;
        case 1: neckline = poly({{C - nw - 1.5, 49}, {C + nw + 1.5, 49}, {C, 56.5}}); break;
        case 3: neckline = ell(C, 50.5, nw + 1.6, 2.6); break;
        case 5: neckline = trunk & above(56.5); break;
        case 7: neckline = poly({{C - nw - 2.4, 49}, {C + nw + 2.4, 49}, {C, 59.5}}); break;
        case 11: neckline = ell(C, 50, nw + 1.8, 2.4); break;
        case 13: neckline = ell(C, 50, sw * 0.86, 5.2); break;
        default: break;  // terno, smoking, gola alta e casaco cobrem tudo
    }
    c.paint(trunk & neckline, skinR, {C - 4, 52, sw, 8}, false);
    c.paint(neck, skinR, {C - 3, 46, nw + 2, 8}, false);
    c.put(neck & above(b.chinY + 3.6), skinR.shade);
    c.put(neck & above(b.chinY + 1.2), skinR.deep);
    for (int y = 40; y < 53; ++y) {  // contorno do pescoço
        c.set(int(C - nw), y, skinR.deep);
        c.set(int(C + nw - 0.01), y, skinR.line);
    }
    if (clothes == 5) c.put(trace({{C - nw - 3, 52.6}, {C, 53.6}, {C + nw + 3, 52.6}}), skinR.shade);  // clavícula

    const Mask garment = trunk - neckline;
    const Tex clothTex = clothes == 11 ? Tex::Knit : Tex::None;
    if (clothes == 9) c.paint(garment, whiteR, bodyL);  // camiseta por baixo do macacão
    else c.paint(garment, clothR, bodyL, true, clothTex);
    switch (clothes) {
        case 1: {  // camisa com gola
            const Mask flap = poly({{C - nw - 2.6, 48.4}, {C - 0.6, 55}, {C - nw - 0.4, 58.4}, {C - nw - 4.6, 52.4}});
            c.paint(sym(flap), ramp(clothR.hi), {C, 52, 12, 6});
            c.put(trace({{C, 57}, {C, 64}}), clothR.shade);
            for (int y : {59, 62}) c.set(int(C) - 1, y, clothR.glow);
            break;
        }
        case 2:     // terno e gravata
        case 10: {  // smoking
            const Mask shirtM = poly({{C - nw - 1.5, 49}, {C + nw + 1.5, 49}, {C + 3.4, 66}, {C - 3.4, 66}}) & trunk;
            c.paint(shirtM, whiteR, {C - 3, 54, 8, 10}, false);
            c.paint(sym(poly({{C - nw - 2.4, 48.6}, {C - 2.6, 64}, {C - 8.5, 64}, {C - nw - 6.5, 53}})), ramp(clothR.shade), bodyL);
            const Ramp tie = ramp(pick(hatColor, kCloth));
            if (clothes == 2) {
                c.paint(poly({{C - 2, 49.6}, {C + 2, 49.6}, {C + 1.2, 52.4}, {C - 1.2, 52.4}}), tie, {C, 51, 3, 3});
                c.paint(poly({{C - 1.4, 52.2}, {C + 1.4, 52.2}, {C + 2.4, 62}, {C, 64.6}, {C - 2.4, 62}}), tie, {C - 1, 57, 4, 8});
            } else {
                c.paint(sym(poly({{C, 51}, {C - 5, 48.6}, {C - 5, 53.6}})), ramp(QColor("#23232e")), {C, 51, 6, 3});
                c.put(box(C - 1, 50, C + 1, 52.2), kInk);
                for (int y : {56, 60}) c.set(int(C) - 1, y, kInk);
            }
            break;
        }
        case 3:  // moletom
            c.put(trace({{C - nw - 1.6, 50.6}, {C, 53.4}, {C + nw + 1.6, 50.6}}), clothR.line);
            for (double dx : {-2.6, 2.6}) c.put(box(C + dx - 0.5, 53.4, C + dx + 0.5, 60), kWhite);
            c.put(trace({{C - 9, 61}, {C + 9, 61}}), clothR.deep);
            break;
        case 4: {  // gola alta
            c.paint(rbox(C - nw - 1.6, 43.4, C + nw + 1.6, 52, 1.6), clothR, {C - 3, 46, nw + 3, 6}, true, Tex::Rib);
            break;
        }
        case 5:  // alcinha
            c.put(sym(box(C - nw - 6.4, 49.6, C - nw - 4.4, 57)) & trunk, clothR.base);
            c.put(sym(box(C - nw - 4.4, 49.6, C - nw - 3.6, 57)) & trunk, clothR.line);
            break;
        case 6: {  // jaqueta aberta sobre camiseta
            const Mask tee = (poly({{C - 6.5, 48}, {C + 6.5, 48}, {C + 5.5, 66}, {C - 5.5, 66}}) & trunk) - neckline;
            c.paint(tee, whiteR, {C - 3, 56, 8, 10}, false);
            c.put(sym(trace({{C - 6.5, 50}, {C - 5.5, 64}})), clothR.line);
            c.put(sym(poly({{C - nw - 3, 49.4}, {C - 6.8, 50.4}, {C - 7.6, 57}, {C - nw - 6, 53}})), clothR.hi);
            break;
        }
        case 8:  // listrada
            for (int y = 53; y < N; y += 4)
                for (int x = 0; x < N; ++x)
                    for (int k = 0; k < 2; ++k)
                        if (garment.has(x, y + k) && !garment.edge(x, y + k)) c.set(x, y + k, mix(c.at(x, y + k), kWhite, 0.82));
            break;
        case 9: {  // macacão
            c.paint(rbox(C - 8, 56, C + 8, 70, 1.6) & trunk, clothR, bodyL);
            c.paint(sym(poly({{C - 8, 57.6}, {C - 5.6, 57.6}, {C - nw - 3.4, 50}, {C - nw - 6, 50.6}})) & trunk, clothR, bodyL);
            c.both(int(C - 6.6), 58, goldR.base);
            break;
        }
        case 11:  // suéter
            c.paint(ell(C, 50, nw + 3.4, 4) - ell(C, 50, nw + 1.8, 2.4), clothR, {C - 3, 50, nw + 4, 4}, true, Tex::Rib, -1);
            break;
        case 12: {  // casaco e cachecol
            c.paint(ell(C, 49.4, nw + 5.4, 4.6), accentR, {C - 4, 48, nw + 6, 5}, true, Tex::Rib);
            c.paint(rbox(C + 1.6, 51, C + 7.6, 63, 1.4), accentR, {C + 3, 55, 5, 8}, true, Tex::Rib);
            break;
        }
        case 13:  // ombro a ombro
            c.put(trace({{C - sw * 0.84, 51.4}, {C, 55.4}, {C + sw * 0.84, 51.4}}), clothR.hi);
            break;
        default: break;
    }

    // ---- Orelhas e rosto.
    const Mask ears = sym(ell(C - b.cheek - 0.6, 30, 2.8, 4.6));
    c.paint(ears, skinR, {C - 3, 28, 20, 9});
    c.put(sym(trace({{C - b.cheek - 1.2, 28.4}, {C - b.cheek - 0.2, 31.4}})), skinR.shade);
    const Mask faceM = faceMask(b);
    c.paint(faceM, skinR, skinL);

    // Detalhes do rosto.
    const QColor pinkC("#f07c86");
    switch (face) {
        case 1: c.tint(sym(ell(C - 9.4, 36, 3.4, 1.9)), pinkC, 0.4); break;
        case 2:
            for (auto [dx, y] : {std::pair{-11.5, 34.0}, {-9.5, 36.0}, {-7.5, 34.5}, {-10.5, 37.5}, {-6.0, 36.5}, {-8.5, 33.0}, {-2.0, 33.5}}) {
                c.tint(sym(box(C + dx, y, C + dx + 1, y + 1)), skinR.deep, 0.55);
            }
            break;
        case 3:
            c.set(int(C + 8), 39, QColor("#3a2418"));
            c.set(int(C + 9), 39, QColor("#3a2418"));
            c.set(int(C + 8), 40, QColor("#3a2418"));
            break;
        case 4: {
            const QColor scar = mix(skinR.shade, QColor("#c2585a"), 0.55);
            c.put(trace({{C + 8.6, 21.5}, {C + 9.8, 27}}), scar);
            c.put(trace({{C + 10, 32.6}, {C + 11, 38}}), scar);
            break;
        }
        case 5:
            c.put(trace({{C - 7, 16.4}, {C, 15.8}, {C + 7, 16.4}}), skinR.shade);
            c.put(trace({{C - 5, 19}, {C, 18.6}, {C + 5, 19}}), skinR.shade);
            c.put(sym(trace({{C - 5.4, 35.4}, {C - 6.6, 39.6}})), skinR.shade);
            c.put(sym(trace({{C - 12.6, 30}, {C - 13.8, 30.8}})), skinR.shade);
            c.put(sym(trace({{C - 12.6, 32}, {C - 13.8, 32.4}})), skinR.shade);
            break;
        case 6: {
            const Mask aid = poly({{C - 12.4, 35}, {C - 5.4, 37.6}, {C - 6.4, 40.2}, {C - 13.4, 37.6}}) & faceM;
            c.put(aid, QColor("#e9c9a2"));
            c.put(poly({{C - 10.4, 36.2}, {C - 7.8, 37.2}, {C - 8.6, 39.2}, {C - 11.2, 38.2}}) & aid, QColor("#f7e6cf"));
            break;
        }
        case 7:
            c.tint(sym(ell(C - kEyeDx, 26.4, 4.8, 2.0)), lipR.base, 0.5);
            c.tint(sym(ell(C - 9.4, 36, 3.4, 1.9)), pinkC, 0.35);
            break;
        default: break;
    }
    drawNose(c, nose, skinR);
    const Ramp irisR = ramp(pick(eyeColor, kEye));
    drawEye(c, false, eyes, irisR, skinR);
    drawEye(c, true, eyes, irisR, skinR);
    drawMouth(c, mouth, lipR, skinR, painted);

    // ---- Barba (por cima do rosto; a boca continua à vista).
    const Mask mouthHole = ell(C, 40.9, 4.4, mouth == 2 ? 4.2 : 2.4);
    const Mask mustache = blob({{C - 5.6, 39.2}, {C - 2, 37}, {C, 37.9}, {C + 2, 37}, {C + 5.6, 39.2}, {C + 3, 39.5}, {C, 38.8}, {C - 3, 39.5}});
    const Light beardL{C - 3, 40, 16, 10};
    Mask beard;
    Tex beardTex = Tex::None;
    switch (facialHair) {
        case 1: c.tint(((faceM & below(35.5)) - ell(C, 33, b.cheek - 3.4, 8.6)) | mustache, hairR.base, 0.34); break;
        case 2: beard = mustache; break;
        case 3: beard = mustache | (ell(C, 44, 3.6, 3.4) & faceM); break;
        case 4:
        case 5:
            beard = ((faceM & below(34)) - ell(C, 32.5, b.cheek - 2.8, 9.4)) | mustache;
            if (facialHair == 5) beard |= ell(C, b.chinY - 1, b.jaw + 1.6, 8.6) & below(41);
            beardTex = Tex::Curl;
            break;
        case 6: beard = stroke({{C - 8, 36.6}, {C - 6.4, 39.4}, {C - 2, 38}, {C, 38.6}, {C + 2, 38}, {C + 6.4, 39.4}, {C + 8, 36.6}}, 1.8); break;
        case 7: beard = ell(C, 44.2, 1.5, 1.7); break;
        case 8: beard = (faceM - ell(C, 33, b.cheek - 5.4, 14)) & box(0, 25, N, 42); break;
        case 9: beard = ((ell(C, 41.4, 6.4, 6) - ell(C, 40.9, 3.6, 2.2)) & faceM) | mustache; break;
        default: break;
    }
    if (!beard.empty()) {
        if (facialHair != 2 && facialHair != 6) beard -= mouthHole;
        c.paint(beard, hairR, beardL, true, beardTex);
    }

    // ---- Raspado, sobrancelhas, cabelo da frente e a sombra dele na testa.
    c.tint(hair.stubble & (faceM | ears), hairR.shade, 0.45);
    float hh, hs, hv;
    hairR.base.getHsvF(&hh, &hs, &hv);
    drawBrows(c, brows, hv > 0.75 ? hairR.deep : hv > 0.3 ? hairR.line : hairR.shade);
    if (!hair.front.empty()) {
        c.paint(hair.front, hairR, {C - 4, 15, 20, 16}, true, hair.tex);
        for (int y = 2; y < N; ++y)
            for (int x = 0; x < N; ++x)
                if (faceM.has(x, y) && !faceM.edge(x, y) && !hair.front.has(x, y) && (hair.front.has(x, y - 1) || hair.front.has(x, y - 2))) {
                    const QColor p = c.at(x, y);
                    if (p == skinR.hi || p == skinR.base || p == skinR.glow) c.set(x, y, skinR.shade);
                }
    }
    if (!hair.tie.empty()) c.paint(hair.tie, accentR, {C, 16, 30, 10});

    // ---- Acessórios.
    const double earX = C - b.cheek - 1.0;
    switch (accessory) {
        case 1: c.put(sym(box(earX - 0.6, 33.4, earX + 1.4, 35.4)), goldR.hi); break;
        case 2: c.put(sym(ell(earX, 37, 2.9, 3.1) - ell(earX, 37, 1.7, 1.9)), goldR.base); break;
        case 3:
            c.put(sym(box(earX, 34, earX + 1, 38)), goldR.base);
            c.paint(sym(ell(earX + 0.5, 39.6, 1.7, 2.1)), ramp(QColor("#3fb0c9")), {C, 39, 20, 3});
            break;
        case 4:
            c.put(trace({{C - nw - 0.4, 49}, {C, 54.6}, {C + nw + 0.4, 49}}), goldR.base);
            c.paint(ell(C, 55.8, 1.7, 1.7), goldR, {C, 55.8, 2, 2});
            break;
        case 5: c.put(box(C - nw + 1, 47.4, C + nw - 1, 49.4), kInk); break;
        case 6:
            for (auto [x, y] : {std::pair{2, 36}, {3, 37}, {2, 38}}) c.set(int(C) + x, y, goldR.hi);
            break;
        default: break;
    }

    drawGlasses(c, glasses, b);
    drawHat(c, hat, accentR, b);

    return c.img.scaled(size, size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

PixelAvatar PixelAvatar::random(unsigned seed) {
    QRandomGenerator r(seed);
    PixelAvatar a;
    a.body = r.bounded(3);
    a.shape = r.bounded(2) ? r.bounded(5) : 0;
    a.skin = r.bounded(10);
    a.face = r.bounded(4) == 0 ? 1 + r.bounded(7) : 0;
    a.eyes = r.bounded(3) == 0 ? 1 + r.bounded(9) : a.body == 0 ? 3 : 0;
    a.eyeColor = r.bounded(10) < 6 ? r.bounded(2) : r.bounded(8);
    a.brows = r.bounded(3) == 0 ? r.bounded(9) : a.body == 1 ? 1 : 0;
    a.nose = r.bounded(5);
    a.mouth = r.bounded(10) < 7 ? r.bounded(2) : r.bounded(10);
    a.lips = a.body == 0 && r.bounded(2) ? 1 + r.bounded(7) : 0;
    a.hairStyle = a.body == 0 ? std::array{3, 4, 5, 6, 10, 11, 13, 8, 15, 16, 17, 18, 22, 23, 28, 30}[r.bounded(16)]
                              : std::array{1, 2, 7, 8, 9, 12, 14, 0, 10, 19, 20, 21, 24, 25, 26, 27, 29}[r.bounded(17)];
    a.hairColor = r.bounded(10) < 8 ? r.bounded(10) : r.bounded(16);
    a.facialHair = a.body == 1 && r.bounded(3) == 0 ? 1 + r.bounded(9) : 0;
    a.glasses = r.bounded(5) == 0 ? 1 + r.bounded(9) : 0;
    a.hat = r.bounded(6) == 0 ? 1 + r.bounded(12) : 0;
    a.hatColor = r.bounded(16);
    a.clothes = r.bounded(14);
    if ((a.clothes == 5 || a.clothes == 13) && a.body == 1) a.clothes = 0;
    a.accessory = r.bounded(4) == 0 ? 1 + r.bounded(6) : 0;
    a.shirt = r.bounded(16);
    a.background = r.bounded(12);
    return a;
}

// ------------------------------------------------------------ janela

namespace {
// Uma aba do criador: qual campo as opções mudam, qual paleta as cores mudam (pode faltar) e que parte
// da grade aparece nas miniaturas: um quadrado de 32 (ampliado 3×) ou de 48 pixels (2×).
struct Crop {
    int x, y, size;
};
struct Category {
    const char* label;
    const char* icon;
    int PixelAvatar::*option;
    QStringList names;
    int PixelAvatar::*color;
    const char* const* palette;
    int paletteSize;
    const char* colorLabel;
    Crop crop;
};

QList<Category> categories() {
    return {
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Head"), "rosto-avatar", &PixelAvatar::shape,
         {PixelAvatarDialog::tr("Oval"), PixelAvatarDialog::tr("Round"), PixelAvatarDialog::tr("Square"), PixelAvatarDialog::tr("Heart"), PixelAvatarDialog::tr("Long")},
         &PixelAvatar::skin, kSkin, 10, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Skin tone"), {16, 15, 32}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Details"), "detalhe", &PixelAvatar::face,
         {PixelAvatarDialog::tr("Plain"), PixelAvatarDialog::tr("Blush"), PixelAvatarDialog::tr("Freckles"), PixelAvatarDialog::tr("Beauty mark"), PixelAvatarDialog::tr("Scar"), PixelAvatarDialog::tr("Wrinkles"), PixelAvatarDialog::tr("Band-aid"), PixelAvatarDialog::tr("Makeup")},
         &PixelAvatar::skin, kSkin, 10, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Skin tone"), {16, 15, 32}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Eyes"), "foco", &PixelAvatar::eyes,
         {PixelAvatarDialog::tr("Normal"), PixelAvatarDialog::tr("Happy"), PixelAvatarDialog::tr("Wide"), PixelAvatarDialog::tr("Lashes"), PixelAvatarDialog::tr("Sleepy"), PixelAvatarDialog::tr("Closed"), PixelAvatarDialog::tr("Almond"), PixelAvatarDialog::tr("Big"), PixelAvatarDialog::tr("Tired"), PixelAvatarDialog::tr("Wink")},
         &PixelAvatar::eyeColor, kEye, 8, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Eye color"), {16, 13, 32}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Brows"), "sobrancelha", &PixelAvatar::brows,
         {PixelAvatarDialog::tr("Natural"), PixelAvatarDialog::tr("Thick"), PixelAvatarDialog::tr("Thin"), PixelAvatarDialog::tr("Raised"), PixelAvatarDialog::tr("Angry"), PixelAvatarDialog::tr("Worried"), PixelAvatarDialog::tr("Straight"), PixelAvatarDialog::tr("Arched"), PixelAvatarDialog::tr("Short brows")},
         &PixelAvatar::hairColor, kHair, 16, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hair color"), {16, 10, 32}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Nose"), "nariz", &PixelAvatar::nose,
         {PixelAvatarDialog::tr("Soft"), PixelAvatarDialog::tr("Button"), PixelAvatarDialog::tr("Long nose"), PixelAvatarDialog::tr("Wide nose"), PixelAvatarDialog::tr("Pointed")},
         &PixelAvatar::skin, kSkin, 10, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Skin tone"), {16, 19, 32}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Mouth"), "boca", &PixelAvatar::mouth,
         {PixelAvatarDialog::tr("Smile"), PixelAvatarDialog::tr("Neutral", "mouth"), PixelAvatarDialog::tr("Laugh"), PixelAvatarDialog::tr("Smirk"), PixelAvatarDialog::tr("Sad"), PixelAvatarDialog::tr("Grin"), PixelAvatarDialog::tr("Full lips"), PixelAvatarDialog::tr("Surprised"), PixelAvatarDialog::tr("Tongue out"), PixelAvatarDialog::tr("Pout")},
         &PixelAvatar::lips, kLips, 8, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Lip color"), {16, 26, 32}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hair"), "cabelo", &PixelAvatar::hairStyle,
         {PixelAvatarDialog::tr("Bald"), PixelAvatarDialog::tr("Short"), PixelAvatarDialog::tr("Messy"), PixelAvatarDialog::tr("Long"), PixelAvatarDialog::tr("Bob"), PixelAvatarDialog::tr("Bun"), PixelAvatarDialog::tr("Ponytail"), PixelAvatarDialog::tr("Mohawk"), PixelAvatarDialog::tr("Afro"), PixelAvatarDialog::tr("Side part"), PixelAvatarDialog::tr("Curly"), PixelAvatarDialog::tr("Braids"), PixelAvatarDialog::tr("Buzz cut"), PixelAvatarDialog::tr("Long waves"), PixelAvatarDialog::tr("Quiff"), PixelAvatarDialog::tr("Pixie"), PixelAvatarDialog::tr("Long with bangs"), PixelAvatarDialog::tr("Pigtails"), PixelAvatarDialog::tr("Space buns"), PixelAvatarDialog::tr("Dreadlocks"), PixelAvatarDialog::tr("Undercut"), PixelAvatarDialog::tr("Slicked back"), PixelAvatarDialog::tr("Shoulder length"), PixelAvatarDialog::tr("Long curls"), PixelAvatarDialog::tr("Top knot"), PixelAvatarDialog::tr("Bowl cut"), PixelAvatarDialog::tr("Spiky"), PixelAvatarDialog::tr("Side shave"), PixelAvatarDialog::tr("Afro puffs"), PixelAvatarDialog::tr("Receding"), PixelAvatarDialog::tr("Side fringe")},
         &PixelAvatar::hairColor, kHair, 16, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hair color"), {8, 0, 48}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Facial hair"), "barba", &PixelAvatar::facialHair,
         {PixelAvatarDialog::tr("None", "facial hair"), PixelAvatarDialog::tr("Stubble"), PixelAvatarDialog::tr("Mustache"), PixelAvatarDialog::tr("Goatee"), PixelAvatarDialog::tr("Beard"), PixelAvatarDialog::tr("Full beard"), PixelAvatarDialog::tr("Handlebar"), PixelAvatarDialog::tr("Soul patch"), PixelAvatarDialog::tr("Sideburns"), PixelAvatarDialog::tr("Circle beard")},
         &PixelAvatar::hairColor, kHair, 16, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hair color"), {16, 26, 32}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Glasses"), "oculos", &PixelAvatar::glasses,
         {PixelAvatarDialog::tr("None", "glasses"), PixelAvatarDialog::tr("Round glasses"), PixelAvatarDialog::tr("Square glasses"), PixelAvatarDialog::tr("Sunglasses"), PixelAvatarDialog::tr("Monocle"), PixelAvatarDialog::tr("Eye patch"), PixelAvatarDialog::tr("Cat-eye glasses"), PixelAvatarDialog::tr("Aviators"), PixelAvatarDialog::tr("Small round glasses"), PixelAvatarDialog::tr("Half-rim glasses")},
         nullptr, nullptr, 0, nullptr, {16, 13, 32}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hat"), "chapeu", &PixelAvatar::hat,
         {PixelAvatarDialog::tr("None", "hat"), PixelAvatarDialog::tr("Beanie"), PixelAvatarDialog::tr("Cap"), PixelAvatarDialog::tr("Fedora"), PixelAvatarDialog::tr("Headband"), PixelAvatarDialog::tr("Bow"), PixelAvatarDialog::tr("Crown"), PixelAvatarDialog::tr("Beret"), PixelAvatarDialog::tr("Cowboy hat"), PixelAvatarDialog::tr("Top hat"), PixelAvatarDialog::tr("Bandana"), PixelAvatarDialog::tr("Headphones"), PixelAvatarDialog::tr("Hijab")},
         &PixelAvatar::hatColor, kCloth, 16, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Hat color"), {8, 0, 48}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Clothes"), "roupa", &PixelAvatar::clothes,
         {PixelAvatarDialog::tr("T-shirt"), PixelAvatarDialog::tr("Collar shirt"), PixelAvatarDialog::tr("Suit and tie"), PixelAvatarDialog::tr("Hoodie"), PixelAvatarDialog::tr("Turtleneck"), PixelAvatarDialog::tr("Strap top"), PixelAvatarDialog::tr("Jacket"), PixelAvatarDialog::tr("V-neck"), PixelAvatarDialog::tr("Striped tee"), PixelAvatarDialog::tr("Overalls"), PixelAvatarDialog::tr("Tuxedo"), PixelAvatarDialog::tr("Sweater"), PixelAvatarDialog::tr("Coat and scarf"), PixelAvatarDialog::tr("Off-shoulder")},
         &PixelAvatar::shirt, kCloth, 16, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Clothing color"), {8, 16, 48}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Accessories"), "acessorio", &PixelAvatar::accessory,
         {PixelAvatarDialog::tr("None", "accessory"), PixelAvatarDialog::tr("Stud earrings"), PixelAvatarDialog::tr("Hoop earrings"), PixelAvatarDialog::tr("Drop earrings"), PixelAvatarDialog::tr("Necklace"), PixelAvatarDialog::tr("Choker"), PixelAvatarDialog::tr("Nose ring")},
         &PixelAvatar::hatColor, kCloth, 16, QT_TRANSLATE_NOOP("PixelAvatarDialog", "Accent color"), {8, 16, 48}},
        {QT_TRANSLATE_NOOP("PixelAvatarDialog", "Background"), "fundo", &PixelAvatar::background,
         {PixelAvatarDialog::tr("Sand"), PixelAvatarDialog::tr("Sky"), PixelAvatarDialog::tr("Mint"), PixelAvatarDialog::tr("Pink"), PixelAvatarDialog::tr("Lavender"), PixelAvatarDialog::tr("Night"), PixelAvatarDialog::tr("White"), PixelAvatarDialog::tr("Sun"), PixelAvatarDialog::tr("Sunset"), PixelAvatarDialog::tr("Forest"), PixelAvatarDialog::tr("Coral"), PixelAvatarDialog::tr("Ocean")},
         nullptr, nullptr, 0, nullptr, {8, 8, 48}},
    };
}

constexpr int kThumb = 96;
constexpr int kPreview = 320;  // 5 px por pixel da grade

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
    setMinimumSize(1080, 740);
    ThemeManager& tm = ThemeManager::instance();
    initial_ = PixelAvatar::random(qHash(name));  // o mesmo nome começa sempre com o mesmo rosto
    history_ = {initial_};

    // ---- Esquerda: prévia, desfazer/refazer, sortear e recomeçar.
    preview_ = new QLabel;
    preview_->setFixedSize(kPreview, kPreview);
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
    const int perRow = 7;
    for (int i = 0; i < cats.size(); ++i) {
        auto* b = new QToolButton;
        b->setObjectName("categoria");
        b->setCheckable(true);
        b->setIcon(tm.icon(cats[i].icon));
        b->setIconSize(QSize(22, 22));
        b->setText(tr(cats[i].label));
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(60);
        b->setMinimumWidth(84);
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
    bottom->addWidget(done);
    bottom->addWidget(cancel);

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

    setCategory(6);  // começa pelo cabelo, como nos criadores de avatar
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
        const int s = kThumb / c.crop.size;  // pixels de tela por pixel da grade
        const QImage img = v.render(64 * s).copy(c.crop.x * s, c.crop.y * s, kThumb, kThumb);
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
    for (int col = 0; col <= 16; ++col) colorLayout_->setColumnStretch(col, col == c.paletteSize ? 1 : 0);
}

void PixelAvatarDialog::refresh() {
    const PixelAvatar a = avatar();
    preview_->setPixmap(rounded(a.render(kPreview), kPreview, 36));
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
