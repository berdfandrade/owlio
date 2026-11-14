#include "versionbrowser.h"

#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QVariantAnimation>
#include <algorithm>
#include <cmath>

#include "autosave.h"
#include "fdx.h"
#include "fountain.h"
#include "pagepainter.h"
#include "theme.h"

namespace {
constexpr double kBottomBar = 118;  // faixa de baixo: botões e data escolhida
constexpr double kRightRail = 200;  // coluna da linha do tempo
constexpr double kHeaderH = 34;     // barra de título de cada cartão
constexpr double kShadowMargin = 22;  // quanto a sombra passa da borda do cartão
constexpr double kShrink = 0.84;    // cada versão mais antiga fica 16% menor
const QColor kHeaderColor(236, 239, 246);
const QColor kHeaderText(32, 38, 58);
}  // namespace

QList<VersionBrowser::Version> VersionBrowser::versionsOf(const QString& scriptPath) {
    QList<Version> out;
    if (scriptPath.isEmpty()) return out;
    // Os backups se chamam "<roteiro> aaaa-MM-dd HH-mm-ss[ (n)].<ext>"; a data vem do nome.
    static const QRegularExpression stamp(R"((\d{4}-\d{2}-\d{2} \d{2}-\d{2}-\d{2})(?: \(\d+\))?$)");
    for (const QString& f : autosave::backups(scriptPath)) {
        const QFileInfo fi(f);
        QDateTime when;
        const auto m = stamp.match(fi.completeBaseName());
        if (m.hasMatch()) when = QDateTime::fromString(m.captured(1), "yyyy-MM-dd HH-mm-ss");
        if (!when.isValid()) when = fi.lastModified();
        out.append({f, when});
    }
    return out;
}

VersionBrowser::VersionBrowser(const Editor& current, const QString& scriptName, const QList<Version>& versions,
                               int page, QWidget* parent)
    : QDialog(parent), current_(current), scriptName_(scriptName), page_(std::max(0, page)) {
    setWindowTitle(tr("Versions"));
    setObjectName("navegadorVersoes");
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(520, 420);

    entries_.push_back({QString(), QDateTime::currentDateTime(), nullptr, {}});
    for (const Version& v : versions) entries_.push_back({v.file, v.when, nullptr, {}});

    anim_ = new QVariantAnimation(this);
    anim_->setDuration(420);
    anim_->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        pos_ = v.toDouble();
        update();
    });

    warm_ = new QTimer(this);
    warm_->setSingleShot(true);
    connect(warm_, &QTimer::timeout, this, &VersionBrowser::warmUp);
    connect(anim_, &QVariantAnimation::finished, this, [this] { warm_->start(0); });
    warm_->start(0);

    cancel_ = new QPushButton(tr("Cancel"), this);
    cancel_->setObjectName("versaoBotao");
    restore_ = new QPushButton(tr("Restore"), this);
    restore_->setObjectName("versaoRestaurar");
    for (QPushButton* b : {cancel_, restore_}) {
        b->setFocusPolicy(Qt::NoFocus);  // as setas ficam com a pilha
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(132, 42);
    }
    connect(cancel_, &QPushButton::clicked, this, &QDialog::reject);
    connect(restore_, &QPushButton::clicked, this, &QDialog::accept);
    updateBottom();
}

VersionBrowser::~VersionBrowser() = default;

void VersionBrowser::cover(QWidget* area) {
    area_ = area;
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setMinimumSize(0, 0);
    area->installEventFilter(this);
    area->window()->installEventFilter(this);
    fitToArea();
}

void VersionBrowser::fitToArea() {
    if (!area_) return;
    setGeometry(QRect(area_->mapToGlobal(QPoint(0, 0)), area_->size()));
}

bool VersionBrowser::eventFilter(QObject* obj, QEvent* ev) {
    if (area_ && (ev->type() == QEvent::Move || ev->type() == QEvent::Resize)) fitToArea();
    return QDialog::eventFilter(obj, ev);
}

// ------------------------------------------------------------------ dados

const Editor* VersionBrowser::editorAt(int i) {
    if (i <= 0) return &current_;
    Entry& e = entries_[i];
    if (!e.ed) {
        e.ed = std::make_unique<Editor>();
        e.ed->setScriptLanguage(current_.scriptLanguage());
        e.ed->setLayoutOptions(current_.layoutOptions());
        QFile f(e.file);
        if (f.open(QIODevice::ReadOnly)) {
            const std::string data = f.readAll().toStdString();
            e.ed->setDocument(e.file.endsWith(".fdx", Qt::CaseInsensitive) ? parseFdx(data) : parseFountain(data));
        }
    }
    return e.ed.get();
}

Document VersionBrowser::selectedDocument() { return editorAt(target_)->doc(); }

QImage VersionBrowser::pageImage(int i) {
    const QRectF fr = frontRect();
    const double w = std::round(fr.width());
    if (w != cachedWidth_) {
        for (Entry& e : entries_) e.pages.clear();
        cachedWidth_ = w;
    }
    Entry& e = entries_[i];
    const Editor* ed = editorAt(i);
    const int pg = std::clamp(page_, 0, ed->layout().pageCount - 1);
    auto it = e.pages.find(pg);
    if (it != e.pages.end()) return *it;

    const double dpr = devicePixelRatioF();
    const PaperGeometry paper = current_.paper();
    const double ppi = w / paper.width;
    QImage img(QSize(int(w * dpr), int(paper.height * ppi * dpr)), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    PagePainter painter(*ed);
    painter.paintPage(p, pg, QPointF(0, 0), ppi, PagePainter::Extras{});
    p.end();
    e.pages.insert(pg, img);
    return img;
}

void VersionBrowser::warmUp() {
    // Carregar e desenhar uma página leva alguns milissegundos: feito entre um quadro e outro,
    // uma página por vez, e nunca durante a animação.
    if (anim_->state() == QAbstractAnimation::Running) {
        warm_->start(40);
        return;
    }
    const double w = std::round(frontRect().width());
    for (int i = std::max(0, target_ - 1); i <= std::min(count() - 1, target_ + 5); ++i) {
        const Entry& e = entries_[i];
        bool ready = w == cachedWidth_ && (i == 0 || e.ed);
        if (ready) {
            const int pg = std::clamp(page_, 0, editorAt(i)->layout().pageCount - 1);
            ready = e.pages.contains(pg);
        }
        if (!ready) {
            pageImage(i);
            warm_->start(0);
            return;
        }
    }
}

QString VersionBrowser::dayLabel(const QDate& d) const {
    const QDate today = QDate::currentDate();
    if (d == today) return tr("Today");
    if (d == today.addDays(-1)) return tr("Yesterday");
    return QLocale().toString(d, d.year() == today.year() ? "d MMM" : "d MMM yyyy");
}

QString VersionBrowser::label(int index) const {
    if (index <= 0) return tr("Today (Now)");
    const QDateTime& w = entries_[index].when;
    return dayLabel(w.date()) + ", " + QLocale().toString(w.time(), QLocale::ShortFormat);
}

// ------------------------------------------------------------------ navegação

void VersionBrowser::select(int index, bool animate) {
    index = std::clamp(index, 0, count() - 1);
    if (index == target_ && (!animate || anim_->state() != QAbstractAnimation::Running)) {
        update();
        return;
    }
    target_ = index;
    anim_->stop();
    if (animate) {
        anim_->setStartValue(pos_);
        anim_->setEndValue(double(index));
        anim_->start();
    } else {
        pos_ = index;
    }
    updateBottom();
    update();
    warm_->start(0);
}

void VersionBrowser::setPage(int page) {
    const int last = editorAt(target_)->layout().pageCount - 1;
    page = std::clamp(page, 0, std::max(0, last));
    if (page == page_) return;
    page_ = page;
    updateBottom();
    update();
    warm_->start(0);
}

void VersionBrowser::updateBottom() {
    restore_->setEnabled(target_ > 0);
    restore_->setToolTip(target_ > 0 ? tr("Bring back the script as it was on %1. Ctrl+Z undoes it.").arg(label(target_))
                                     : QString());
}

// ------------------------------------------------------------------ geometria

QRectF VersionBrowser::frontRect() const {
    // Centralizado na janela (a linha do tempo fica por cima, na borda direita).
    const double areaW = width() - 2 * kRightRail;
    const double areaH = height() - kBottomBar - 30;
    const double ratio = current_.paper().height / current_.paper().width;
    double pageH = std::min(areaH * 0.82 - kHeaderH, areaW * 0.9 * ratio);
    pageH = std::max(pageH, 200.0);
    const double w = pageH / ratio;
    const double h = pageH + kHeaderH;
    const double cx = width() / 2.0;
    const double bottom = height() - kBottomBar - 6;
    return QRectF(cx - w / 2, bottom - h, w, h);
}

QPointF VersionBrowser::vanishingPoint() const { return QPointF(frontRect().center().x(), 22); }

QRectF VersionBrowser::timelineRect() const {
    return QRectF(width() - kRightRail + 10, 70, kRightRail - 40, height() - kBottomBar - 110);
}

double VersionBrowser::tickY(int i) const {
    const QRectF r = timelineRect();
    if (count() <= 1) return r.bottom();
    const double step = std::min(28.0, r.height() / (count() - 1));
    return r.bottom() - i * step;  // agora embaixo, versões antigas para cima
}

int VersionBrowser::nearestTick(double y) const {
    const QRectF r = timelineRect();
    if (count() <= 1) return 0;
    const double step = std::min(28.0, r.height() / (count() - 1));
    return std::clamp(int(std::lround((r.bottom() - y) / step)), 0, count() - 1);
}

bool VersionBrowser::onRail(QPointF pt) const {
    const QRectF r = timelineRect();
    return pt.x() >= r.left() - 30 && pt.y() >= tickY(count() - 1) - 16 && pt.y() <= r.bottom() + 16;
}

QRectF VersionBrowser::arrowRect(bool older) const {
    const QRectF fr = frontRect();
    const double x = std::min(fr.right() + 48, width() - kRightRail - 50.0);
    const double cy = fr.center().y();
    return QRectF(x, older ? cy - 58 : cy + 14, 44, 44);
}

// ------------------------------------------------------------------ desenho

void VersionBrowser::paintBackdrop(QPainter& out) {
    // O céu só muda com o tamanho da janela: desenhado uma vez e reaproveitado a cada quadro.
    const double dpr = devicePixelRatioF();
    if (backdrop_.size() != size() * dpr) {
        backdrop_ = QPixmap(size() * dpr);
        backdrop_.setDevicePixelRatio(dpr);
        QPainter p(&backdrop_);
        p.setRenderHint(QPainter::Antialiasing);
        drawSky(p);
    }
    out.drawPixmap(0, 0, backdrop_);
}

void VersionBrowser::drawSky(QPainter& p) {
    // Céu de madrugada (a hora da coruja), com um brilho atrás da pilha de versões.
    QLinearGradient sky(0, 0, 0, height());
    sky.setColorAt(0, QColor(6, 9, 26));
    sky.setColorAt(0.55, QColor(14, 21, 52));
    sky.setColorAt(1, QColor(30, 40, 86));
    p.fillRect(rect(), sky);

    if (stars_.empty()) {
        QRandomGenerator rng(7);
        for (int i = 0; i < 180; ++i) stars_.push_back(QPointF(rng.generateDouble(), rng.generateDouble()));
    }
    p.setPen(Qt::NoPen);
    for (size_t i = 0; i < stars_.size(); ++i) {
        const double a = 0.25 + 0.6 * std::fmod(i * 0.618, 1.0);
        const double r = (i % 7 == 0) ? 1.4 : 0.8;
        p.setBrush(QColor(255, 255, 255, int(a * 255 * (1.0 - stars_[i].y() * 0.6))));
        p.drawEllipse(QPointF(stars_[i].x() * width(), stars_[i].y() * (height() - kBottomBar)), r, r);
    }

    const QPointF v = vanishingPoint();
    const double rad = std::max(220.0, height() * 0.42);
    QRadialGradient glow(v + QPointF(0, 40), rad);
    glow.setColorAt(0, QColor(150, 175, 255, 150));
    glow.setColorAt(0.25, QColor(108, 140, 255, 70));
    glow.setColorAt(1, QColor(108, 140, 255, 0));
    p.setBrush(glow);
    p.drawEllipse(v + QPointF(0, 40), rad, rad);
    QRadialGradient core(v + QPointF(0, 30), 46);
    core.setColorAt(0, QColor(255, 255, 255, 170));
    core.setColorAt(1, QColor(255, 255, 255, 0));
    p.setBrush(core);
    p.drawEllipse(v + QPointF(0, 30), 46, 46);
}

const QPixmap& VersionBrowser::shadow() {
    const QRectF fr = frontRect();
    const double dpr = devicePixelRatioF();
    const QSize want = (fr.size() + QSizeF(2 * kShadowMargin, 2 * kShadowMargin)).toSize() * dpr;
    if (shadow_.size() != want) {
        QImage img(want, QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(dpr);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        const QRectF card(kShadowMargin, kShadowMargin, fr.width(), fr.height());
        const int layers = 16;
        for (int k = layers; k >= 1; --k) {
            const double grow = k * kShadowMargin / layers;
            p.setBrush(QColor(0, 0, 0, int(110.0 / layers * (1.0 - double(k) / (layers + 1)) * 2)));
            p.drawRoundedRect(card.adjusted(-grow, -grow, grow, grow), 10 + grow, 10 + grow);
        }
        p.end();
        shadow_ = QPixmap::fromImage(img);
    }
    return shadow_;
}

double VersionBrowser::scaleAt(double depth) {
    // Versões mais novas que a escolhida (profundidade negativa) passam "por cima" de quem olha.
    return depth >= 0 ? std::pow(kShrink, depth) : 1.0 - depth * 0.35;
}

double VersionBrowser::alphaAt(double depth) {
    // Opacas, menos as que somem perto do brilho e as mais novas que passam por cima.
    if (depth < 0) return std::clamp(1.0 + depth * 1.4, 0.0, 1.0);
    return std::clamp(1.0 - (depth - 9.0) * 0.25, 0.0, 1.0);
}

QRectF VersionBrowser::cardRectAt(double depth) const {
    const double s = scaleAt(depth);
    const QRectF fr = frontRect();
    const QPointF v = vanishingPoint();
    return QRectF(v.x() + (fr.left() - v.x()) * s, v.y() + (fr.top() - v.y()) * s, fr.width() * s, fr.height() * s);
}

void VersionBrowser::paintCard(QPainter& p, int i, double depth, double visibleBottom) {
    const double s = scaleAt(depth);
    const double alpha = alphaAt(depth);
    if (alpha <= 0.01) return;
    const QRectF r = cardRectAt(depth);
    // Só a faixa acima do cartão da frente aparece: o resto nem é pintado.
    const bool clipped = visibleBottom < r.bottom() + 40;

    // Névoa: as versões do fundo vão se misturando ao céu (misturada nas cores, sem pintar por cima).
    const QColor fog(20, 28, 64);
    const double fogA = depth > 0.2 ? std::min(0.55, depth * 0.06) : 0.0;
    auto mist = [&](const QColor& c) {
        return QColor::fromRgbF(c.redF() + (fog.redF() - c.redF()) * fogA, c.greenF() + (fog.greenF() - c.greenF()) * fogA,
                                c.blueF() + (fog.blueF() - c.blueF()) * fogA);
    };

    p.save();
    p.setOpacity(alpha);
    if (clipped) p.setClipRect(QRectF(0, 0, width(), visibleBottom));
    // Sombra: uma imagem pronta (desenhada uma vez no tamanho do cartão da frente), só esticada.
    const double rad = 10 * s;
    if (depth < 8) {
        const QPixmap& sh = shadow();
        const double m = kShadowMargin * s;
        p.drawPixmap(r.adjusted(-m, -m + 6 * s, m, m + 6 * s), sh, QRectF(sh.rect()));
    }
    p.setPen(Qt::NoPen);
    const double hh = kHeaderH * s;
    const QRectF header(r.left(), r.top(), r.width(), hh);
    const QRectF paper(r.left(), r.top() + hh, r.width(), r.height() - hh);
    // Papel inteiro arredondado, e a barra de título por cima (cantos de cima arredondados).
    p.setBrush(mist(Qt::white));
    p.drawRoundedRect(r, rad, rad);
    p.setBrush(mist(kHeaderColor));
    p.drawRoundedRect(QRectF(header.left(), header.top(), header.width(), hh + rad), rad, rad);
    p.fillRect(QRectF(paper.left(), paper.top(), paper.width(), rad), mist(Qt::white));
    if (depth > -1.0 && depth < 3.5 && visibleBottom > paper.top() + 1) {
        // A página não encosta nos cantos de baixo (é margem branca), então dá para cortar pela imagem.
        const QImage img = pageImage(i);
        const QRectF target = paper.adjusted(0, 0, 0, -rad);
        const QRectF source(0, 0, img.width(), img.height() * (target.height() / paper.height()));
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(target, img, source);
        if (fogA > 0) {
            QColor f = fog;
            f.setAlphaF(fogA);
            p.fillRect(target, f);
        }
    }
    p.fillRect(QRectF(header.left(), header.bottom() - 1, header.width(), 1), mist(QColor(210, 215, 228)));

    // Barra de título: página, nome do roteiro e data da versão.
    if (hh > 9) {
        QFont f = font();
        f.setPixelSize(std::max(6, int(13 * s)));
        QFont fb = f;
        fb.setWeight(QFont::DemiBold);
        const double pad = 14 * s;
        const QRectF in = header.adjusted(pad, 0, -pad, 0);
        p.setFont(f);
        p.setPen(mist(QColor(110, 118, 140)));
        // Esquerda: página e nome do roteiro; direita: data da versão.
        const QFontMetricsF fm(f), fmb(fb);
        const QString when = label(i);
        p.drawText(in, Qt::AlignRight | Qt::AlignVCenter, when);
        double x = in.left();
        const Editor* ed = i == 0 || entries_[i].ed ? editorAt(i) : nullptr;
        if (ed) {
            const QString pg = tr("p. %1").arg(std::clamp(page_, 0, ed->layout().pageCount - 1) + 1);
            p.drawText(in, Qt::AlignLeft | Qt::AlignVCenter, pg);
            x += fm.horizontalAdvance(pg) + 10 * s;
        }
        const double nameW = in.right() - fm.horizontalAdvance(when) - 14 * s - x;
        if (nameW > 30 * s) {
            p.setFont(fb);
            p.setPen(mist(kHeaderText));
            p.drawText(QRectF(x, in.top(), nameW, in.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       fmb.elidedText(scriptName_, Qt::ElideMiddle, nameW));
        }
    }
    p.restore();
}

void VersionBrowser::paintTimeline(QPainter& p) {
    const QRectF r = timelineRect();
    const QColor accent(108, 140, 255);
    QFont f = font();
    f.setPointSizeF(font().pointSizeF() * 0.92);
    QFont fb = f;
    fb.setWeight(QFont::DemiBold);
    p.setFont(f);

    const double tickX = r.left() + 70;  // as marcas saem daqui para a direita
    double lastLabelY = 1e9;
    for (int i = 0; i < count(); ++i) {
        const double y = tickY(i);
        const bool sel = i == target_;
        const bool hov = i == hoverTick_;
        const bool newDay = i == 0 || entries_[i].when.date() != entries_[i - 1].when.date();
        const double len = sel ? 46 : (hov ? 34 : (newDay ? 26 : 16));
        QColor c = sel ? accent : QColor(255, 255, 255, hov ? 230 : (newDay ? 170 : 90));
        p.setPen(QPen(c, sel ? 3 : 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(tickX + (46 - len), y), QPointF(tickX + 46, y));

        // Rótulos: "Agora", e o dia sempre que ele muda (sem encavalar).
        if ((newDay || i == 0) && std::abs(lastLabelY - y) > 18) {
            p.setFont(newDay && i > 0 ? f : fb);
            p.setPen(sel ? accent : QColor(255, 255, 255, 200));
            const QString t = i == 0 ? tr("Now") : dayLabel(entries_[i].when.date());
            p.drawText(QRectF(r.left() - 30, y - 10, tickX - r.left() + 30 - 6 + (46 - len), 20),
                       Qt::AlignRight | Qt::AlignVCenter, t);
            lastLabelY = y;
        }
    }
    // Dica com a hora da marca sob o mouse.
    if (hoverTick_ >= 0 && hoverTick_ != target_) {
        const double y = tickY(hoverTick_);
        const QString t = label(hoverTick_);
        p.setFont(fb);
        const QFontMetricsF fm(fb);
        const QRectF pill(tickX - fm.horizontalAdvance(t) - 34, y - 13, fm.horizontalAdvance(t) + 20, 26);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 235));
        p.drawRoundedRect(pill, 8, 8);
        p.setPen(kHeaderText);
        p.drawText(pill, Qt::AlignCenter, t);
    }
}

void VersionBrowser::paintArrows(QPainter& p) {
    for (int k = 0; k < 2; ++k) {
        const bool older = k == 0;
        const QRectF a = arrowRect(older);
        const bool enabled = older ? target_ < count() - 1 : target_ > 0;
        const bool hov = hoverArrow_ == k && enabled;
        p.setPen(QPen(QColor(255, 255, 255, enabled ? 90 : 35), 1));
        p.setBrush(QColor(255, 255, 255, hov ? 60 : (enabled ? 28 : 10)));
        p.drawEllipse(a);
        QPainterPath chev;
        const QPointF c = a.center();
        const double d = older ? -1 : 1;
        chev.moveTo(c.x() - 8, c.y() - 4 * d);
        chev.lineTo(c.x(), c.y() + 4 * d);
        chev.lineTo(c.x() + 8, c.y() - 4 * d);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, enabled ? 235 : 70), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(chev);
    }
}

void VersionBrowser::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    paintBackdrop(p);

    // Do fundo para a frente.
    const int last = std::min(count() - 1, int(std::floor(pos_)) + 14);
    const int first = std::max(0, int(std::floor(pos_)) - 1);
    for (int i = last; i >= first; --i) {
        // O cartão logo à frente, se for opaco, esconde tudo abaixo do topo dele.
        double visibleBottom = height();
        if (i - 1 >= first && alphaAt(i - 1 - pos_) >= 1.0) visibleBottom = cardRectAt(i - 1 - pos_).top();
        paintCard(p, i, i - pos_, visibleBottom);
    }

    paintArrows(p);
    paintTimeline(p);

    // Faixa de baixo: a versão escolhida, com páginas e cenas.
    QLinearGradient bar(0, height() - kBottomBar, 0, height());
    bar.setColorAt(0, QColor(10, 14, 36, 0));
    bar.setColorAt(0.35, QColor(10, 14, 36, 170));
    bar.setColorAt(1, QColor(10, 14, 36, 220));
    p.fillRect(QRectF(0, height() - kBottomBar, width(), kBottomBar), bar);

    const double cy = height() - kBottomBar / 2 + 6;
    QFont big = font();
    big.setPointSizeF(font().pointSizeF() * 1.45);
    big.setWeight(QFont::Bold);
    p.setFont(big);
    p.setPen(Qt::white);
    const double centerX = width() / 2.0;
    p.drawText(QRectF(centerX - 220, cy - 30, 440, 30), Qt::AlignCenter | Qt::AlignBottom, label(target_));
    const Editor* ed = editorAt(target_);
    QFont small = font();
    small.setPointSizeF(font().pointSizeF() * 0.92);
    p.setFont(small);
    p.setPen(QColor(200, 210, 240));
    const QString info = tr("%n page(s)", "", ed->layout().pageCount) + "  ·  " +
                         tr("%n scene(s)", "", static_cast<int>(ed->scenes().size()));
    p.drawText(QRectF(centerX - 220, cy + 2, 440, 22), Qt::AlignCenter, info);

    if (count() == 1) {
        QFont f = font();
        f.setPointSizeF(font().pointSizeF() * 1.05);
        p.setFont(f);
        p.setPen(QColor(230, 236, 255));
        const QRectF fr = frontRect();
        p.drawText(QRectF(40, fr.top() - 90, width() - kRightRail - 80, 70), Qt::AlignCenter | Qt::TextWordWrap,
                   tr("No saved versions yet. Every time you save, Owlio keeps a version here."));
    }
}

void VersionBrowser::resizeEvent(QResizeEvent* e) {
    QDialog::resizeEvent(e);
    warm_->start(0);
    const double cy = height() - kBottomBar / 2 + 6;
    const int centerX = width() / 2;
    cancel_->move(centerX - 260 - cancel_->width() / 2, int(cy - cancel_->height() / 2));
    restore_->move(centerX + 260 - restore_->width() / 2, int(cy - restore_->height() / 2));
}

// ------------------------------------------------------------------ teclado e mouse

void VersionBrowser::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
        case Qt::Key_Up:
        case Qt::Key_PageUp: select(target_ + 1); return;
        case Qt::Key_Down:
        case Qt::Key_PageDown: select(target_ - 1); return;
        case Qt::Key_Home: select(0); return;
        case Qt::Key_End: select(count() - 1); return;
        case Qt::Key_Left: setPage(page_ - 1); return;
        case Qt::Key_Right: setPage(page_ + 1); return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (restore_->isEnabled()) accept();
            return;
        default: QDialog::keyPressEvent(e);
    }
}

void VersionBrowser::wheelEvent(QWheelEvent* e) {
    static int acc = 0;
    acc += e->angleDelta().y();
    while (acc >= 120) { select(target_ + 1); acc -= 120; }
    while (acc <= -120) { select(target_ - 1); acc += 120; }
    e->accept();
}

void VersionBrowser::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const QPointF pt = e->position();
    if (arrowRect(true).contains(pt)) { select(target_ + 1); return; }
    if (arrowRect(false).contains(pt)) { select(target_ - 1); return; }
    if (onRail(pt)) {  // clicar ou arrastar ao longo da linha do tempo
        scrubbing_ = true;
        select(nearestTick(pt.y()));
        return;
    }
    // Clique na barra de título de uma versão do fundo: vai até ela.
    const QRectF fr = frontRect();
    const QPointF v = vanishingPoint();
    for (int i = target_ + 1; i < count() && i - pos_ < 14; ++i) {
        const double s = std::pow(kShrink, i - pos_);
        const QRectF r(v.x() + (fr.left() - v.x()) * s, v.y() + (fr.top() - v.y()) * s, fr.width() * s, kHeaderH * s);
        if (r.contains(pt)) { select(i); return; }
    }
}

void VersionBrowser::mouseReleaseEvent(QMouseEvent*) { scrubbing_ = false; }

void VersionBrowser::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pt = e->position();
    if (scrubbing_ && (e->buttons() & Qt::LeftButton)) {
        select(nearestTick(pt.y()));
        return;
    }
    const int t = onRail(pt) ? nearestTick(pt.y()) : -1;
    const int a = arrowRect(true).contains(pt) ? 0 : arrowRect(false).contains(pt) ? 1 : -1;
    if (t != hoverTick_ || a != hoverArrow_) {
        hoverTick_ = t;
        hoverArrow_ = a;
        update();
    }
    setCursor(t >= 0 || a >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void VersionBrowser::leaveEvent(QEvent*) {
    hoverTick_ = hoverArrow_ = -1;
    update();
}
