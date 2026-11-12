#include "timeline.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QToolButton>
#include <QToolTip>
#include <functional>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "avatars.h"
#include "sceneheading.h"
#include "theme.h"

namespace {
QString q(const std::u32string& s) { return QString::fromStdU32String(s); }

constexpr int kLeft = 176;   // coluna dos nomes
constexpr int kRight = 32;
constexpr int kTop = 18;
constexpr int kRuler = 34;
constexpr int kActs = 64;
constexpr int kScenes = 58;
constexpr int kLane = 36;
constexpr int kMaxLanes = 14;

QColor laneColor(const QString& name) {  // a mesma matiz para o mesmo nome
    const double h = (qHash(name) % 360) / 360.0;
    return QColor::fromHsvF(float(h), 0.55f, 0.85f);
}
}  // namespace

class TimelineCanvas : public QWidget {
public:
    struct Scene {
        int para;
        double start, length;  // em páginas
        QString heading, synopsis, time;
        QStringList cast;
    };
    struct Lane {
        QString name;
        std::string photo;
        QList<int> scenes;
    };

    explicit TimelineCanvas(QWidget* parent) : QWidget(parent) { setMouseTracking(true); }

    std::function<void(int)> onOpen;
    QList<Scene> scenes;
    QList<Lane> lanes;
    double total = 0;  // páginas escritas (pela soma das cenas)
    int target = 110;
    bool fitWritten = false;  // escala só nas páginas escritas
    int current = -1;  // cena do cursor
    int pages = 0;     // páginas do roteiro paginado
    int hover = -1;

    double scale() const { return fitWritten ? std::max(total, 1.0) : std::max<double>({total, double(target), 1.0}); }
    double xOf(double page) const { return kLeft + page / scale() * (width() - kLeft - kRight); }
    int scenesTop() const { return kTop + kRuler + kActs + 16; }
    int lanesTop() const { return scenesTop() + kScenes + 28; }
    int preferredHeight() const { return lanesTop() + int(lanes.size()) * kLane + 40; }

    int sceneAt(QPoint p) const {
        if (p.y() < scenesTop() || p.y() > lanesTop() + int(lanes.size()) * kLane) return -1;
        for (int i = 0; i < scenes.size(); ++i) {
            const double x0 = xOf(scenes[i].start), x1 = xOf(scenes[i].start + scenes[i].length);
            if (p.x() >= x0 && p.x() < std::max(x1, x0 + 3)) return i;
        }
        return -1;
    }

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent* e) override {
        const int s = sceneAt(e->pos());
        if (s != hover) {
            hover = s;
            update();
        }
        setCursor(s >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        if (s >= 0) {
            const Scene& sc = scenes[s];
            QString tip = QString("<b>%1. %2</b><br>").arg(s + 1).arg(sc.heading.toHtmlEscaped());
            tip += TimelineView::tr("Page %1 · %2 page(s)").arg(int(sc.start) + 1).arg(sc.length, 0, 'f', 1);
            if (!sc.cast.isEmpty()) tip += "<br>" + sc.cast.join(", ").toHtmlEscaped();
            if (!sc.synopsis.isEmpty()) tip += "<br><i>" + sc.synopsis.toHtmlEscaped() + "</i>";
            QToolTip::showText(e->globalPosition().toPoint(), tip, this);
        } else {
            QToolTip::hideText();
        }
    }
    void leaveEvent(QEvent*) override {
        hover = -1;
        update();
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        const int s = sceneAt(e->pos());
        if (e->button() == Qt::LeftButton && s >= 0 && onOpen) onOpen(scenes[s].para);
    }
};

void TimelineCanvas::paintEvent(QPaintEvent*) {
    const ThemeColors& c = ThemeManager::instance().colors();
    const bool dark = c.window.lightness() < 128;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), c.canvas);
    const double x0 = kLeft, x1 = width() - kRight;
    QFont base = font();
    QFont small = base;
    small.setPointSizeF(std::max(7.0, base.pointSizeF() * 0.88));
    QFont bold = base;
    bold.setBold(true);
    QFont caps = small;
    caps.setBold(true);
    caps.setLetterSpacing(QFont::PercentageSpacing, 108);

    auto sectionLabel = [&](int y, const QString& text) {
        p.setFont(caps);
        p.setPen(c.muted);
        p.drawText(QRectF(24, y, kLeft - 36, 20), Qt::AlignLeft | Qt::AlignVCenter, text.toUpper());
    };

    // ---- Régua: páginas e minutos (1 página ≈ 1 minuto).
    int y = kTop;
    const double sc = scale();
    const int stepPages = sc > 150 ? 20 : sc > 60 ? 10 : sc > 20 ? 5 : sc > 8 ? 2 : 1;
    p.setFont(small);
    for (int page = 0; page <= int(sc); page += stepPages) {
        const double x = xOf(page);
        p.setPen(QPen(c.border, 1));
        p.drawLine(QPointF(x, y + 20), QPointF(x, y + kRuler - 2));
        p.setPen(c.muted);
        p.drawText(QRectF(x - 40, y, 80, 18), Qt::AlignCenter, page == 0 ? TimelineView::tr("p. 1") : QString::number(page));
    }
    sectionLabel(y, TimelineView::tr("Pages"));

    // ---- Atos e pontos de virada, pela duração planejada.
    y += kRuler;
    sectionLabel(y + 8, TimelineView::tr("Structure"));
    const double T = target;
    struct Act { double from, to; QString name, sub; QColor tint; };
    const Act acts[] = {
        {0, 0.25 * T, TimelineView::tr("Act I"), TimelineView::tr("Setup"), QColor("#3f8fe0")},
        {0.25 * T, 0.75 * T, TimelineView::tr("Act II"), TimelineView::tr("Confrontation"), QColor("#8a6fd6")},
        {0.75 * T, T, TimelineView::tr("Act III"), TimelineView::tr("Resolution"), QColor("#e0567a")},
    };
    p.save();
    p.setClipRect(QRectF(x0 - 2, 0, x1 - x0 + 4, height()));
    for (const Act& a : acts) {
        if (xOf(a.from) >= x1) continue;
        QRectF r(QPointF(xOf(a.from) + 1, y), QPointF(std::min(xOf(a.to), x1) - 1, y + 40));
        QColor fill = a.tint;
        fill.setAlphaF(dark ? 0.22f : 0.13f);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(r, 8, 8);
        p.setPen(a.tint.darker(dark ? 60 : 125));
        p.setFont(bold);
        p.drawText(r.adjusted(10, 3, -6, -18), Qt::AlignLeft | Qt::AlignVCenter, a.name);
        p.setFont(small);
        p.setPen(c.muted);
        p.drawText(r.adjusted(10, 20, -6, -2), Qt::AlignLeft | Qt::AlignVCenter, a.sub);
    }
    struct Beat { double at; QString name; };
    const Beat beats[] = {{0.12, TimelineView::tr("Inciting incident")}, {0.25, TimelineView::tr("Plot point 1")},
                          {0.5, TimelineView::tr("Midpoint")}, {0.75, TimelineView::tr("Plot point 2")},
                          {0.9, TimelineView::tr("Climax")}};
    p.setFont(small);
    for (const Beat& b : beats) {
        const double x = xOf(b.at * T);
        if (x > x1) continue;
        QPainterPath diamond;
        diamond.moveTo(x, y + 42);
        diamond.lineTo(x + 5, y + 47);
        diamond.lineTo(x, y + 52);
        diamond.lineTo(x - 5, y + 47);
        diamond.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(c.text);
        p.drawPath(diamond);
        p.setPen(c.text);
        p.drawText(QRectF(x + 8, y + 40, 160, 16), Qt::AlignLeft | Qt::AlignVCenter, b.name);
    }
    p.restore();

    // ---- Cenas, do tamanho de cada uma, na cor do horário.
    const int sy = scenesTop();
    sectionLabel(sy + kScenes / 2 - 10, TimelineView::tr("Scenes"));
    p.setPen(Qt::NoPen);
    p.setBrush(c.surface);
    p.drawRoundedRect(QRectF(x0 - 4, sy - 4, x1 - x0 + 8, kScenes + 8), 10, 10);
    for (int i = 0; i < scenes.size(); ++i) {
        const Scene& s = scenes[i];
        QRectF r(QPointF(xOf(s.start) + 0.5, sy), QPointF(std::max(xOf(s.start + s.length) - 0.5, xOf(s.start) + 2.5), sy + kScenes));
        QColor col = timeColor(s.time);
        if (i != hover && i != current) col.setAlphaF(0.82f);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawRoundedRect(r, 3, 3);
        if (r.width() > 18) {
            p.setPen(col.lightness() > 150 ? QColor("#1d1a24") : Qt::white);
            p.setFont(small);
            p.drawText(r.adjusted(2, 4, -2, -4), Qt::AlignHCenter | Qt::AlignTop, QString::number(i + 1));
        }
        if (i == current) {
            p.setPen(QPen(c.text, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(-1, -1, 1, 1), 4, 4);
        }
    }

    // ---- Faixas dos personagens.
    const int ly = lanesTop();
    if (!lanes.isEmpty()) sectionLabel(ly - 24, TimelineView::tr("Characters"));
    for (int li = 0; li < lanes.size(); ++li) {
        const Lane& lane = lanes[li];
        const int top = ly + li * kLane;
        if (li % 2 == 0) {
            QColor stripe = c.surface;
            stripe.setAlphaF(0.55f);
            p.setPen(Qt::NoPen);
            p.setBrush(stripe);
            p.drawRoundedRect(QRectF(16, top, width() - 32, kLane), 8, 8);
        }
        avatars::draw(p, QRectF(24, top + 6, 24, 24), lane.name, lane.photo);
        p.setFont(bold);
        p.setPen(c.text);
        const QString shown = QFontMetrics(bold).elidedText(lane.name, Qt::ElideRight, kLeft - 66);
        p.drawText(QRectF(56, top, kLeft - 60, kLane), Qt::AlignLeft | Qt::AlignVCenter, shown);
        const QColor col = laneColor(lane.name);
        for (int s : lane.scenes) {
            const Scene& sc2 = scenes[s];
            QRectF r(QPointF(xOf(sc2.start) + 0.5, top + 10),
                     QPointF(std::max(xOf(sc2.start + sc2.length) - 0.5, xOf(sc2.start) + 3), top + kLane - 10));
            p.setPen(Qt::NoPen);
            p.setBrush(s == hover ? col.darker(115) : col);
            p.drawRoundedRect(r, 4, 4);
        }
    }

    // ---- Onde o roteiro está agora e coluna da cena sob o mouse.
    const int bottom = ly + int(lanes.size()) * kLane;
    if (hover >= 0) {
        const Scene& s = scenes[hover];
        QColor band = c.accent;
        band.setAlphaF(0.10f);
        p.setPen(Qt::NoPen);
        p.setBrush(band);
        p.drawRect(QRectF(QPointF(xOf(s.start), sy - 6), QPointF(std::max(xOf(s.start + s.length), xOf(s.start) + 3), bottom)));
    }
    const double wx = xOf(total);
    p.setPen(QPen(c.accent, 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(wx, kTop + kRuler - 4), QPointF(wx, bottom + 6));
    const QString here = TimelineView::tr("Written: %n page(s)", "", pages);
    p.setFont(caps);
    const double tw = QFontMetricsF(caps).horizontalAdvance(here) + 16;
    QRectF tag(std::min(wx - tw / 2, width() - tw - 8.0), bottom + 8, tw, 20);
    tag.moveLeft(std::max(tag.left(), double(kLeft) - 20));
    p.setPen(Qt::NoPen);
    p.setBrush(c.accent);
    p.drawRoundedRect(tag, 10, 10);
    p.setPen(c.accentText);
    p.drawText(tag, Qt::AlignCenter, here);
}

// ------------------------------------------------------------ visão

TimelineView::TimelineView(Editor* editor, QWidget* parent) : QWidget(parent), ed_(editor) {
    auto* heading = new QLabel(tr("Timeline"));
    heading->setObjectName("tituloDialogo");
    stats_ = new QLabel;
    stats_->setObjectName("subtituloDialogo");
    progress_ = new QProgressBar;
    progress_->setObjectName("progressoFilme");
    progress_->setTextVisible(false);
    progress_->setFixedHeight(8);
    progress_->setMaximumWidth(340);
    target_ = new QSpinBox;
    target_->setRange(5, 400);
    target_->setSuffix(tr(" pages"));
    target_->setValue(QSettings().value("targetPages", 110).toInt());
    target_->setToolTip(tr("Planned length. One page is about one minute of screen time."));
    fit_ = new QToolButton;
    fit_->setObjectName("alternarLinha");
    fit_->setText(tr("Zoom to Written Pages"));
    fit_->setCheckable(true);
    fit_->setCursor(Qt::PointingHandCursor);
    fit_->setChecked(QSettings().value("timelineFitWritten", false).toBool());
    connect(fit_, &QToolButton::toggled, this, [this](bool on) {
        QSettings().setValue("timelineFitWritten", on);
        reload();
    });
    auto* targetLabel = new QLabel(tr("Planned length:"));
    targetLabel->setObjectName("subtituloDialogo");

    auto* titleCol = new QVBoxLayout;
    titleCol->setSpacing(4);
    titleCol->addWidget(heading);
    titleCol->addWidget(stats_);
    auto* header = new QHBoxLayout;
    header->setContentsMargins(24, 18, 24, 10);
    header->setSpacing(12);
    header->addLayout(titleCol);
    header->addStretch(1);
    header->addWidget(progress_, 1, Qt::AlignVCenter);
    header->addSpacing(12);
    header->addWidget(fit_);
    header->addSpacing(8);
    header->addWidget(targetLabel);
    header->addWidget(target_);

    canvas_ = new TimelineCanvas(this);
    canvas_->onOpen = [this](int para) { emit openScene(para); };
    scroll_ = new QScrollArea;
    scroll_->setWidget(canvas_);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addLayout(header);
    root->addWidget(scroll_, 1);

    connect(target_, &QSpinBox::valueChanged, this, [this](int v) {
        QSettings().setValue("targetPages", v);
        reload();
    });
    connect(&ThemeManager::instance(), &ThemeManager::changed, canvas_, qOverload<>(&QWidget::update));
}

int TimelineView::targetPages() const { return target_->value(); }
void TimelineView::setTargetPages(int pages) { target_->setValue(pages); }
int TimelineView::sceneAt(QPoint canvasPos) const { return canvas_->sceneAt(canvasPos); }
QWidget* TimelineView::canvas() const { return canvas_; }

void TimelineView::reload() {
    const auto summaries = ed_->sceneSummaries();
    canvas_->scenes.clear();
    double at = 0;
    QHash<QString, int> laneIndex;
    QList<TimelineCanvas::Lane> lanes;
    for (const auto& [n, count] : ed_->characters()) {
        if (lanes.size() >= kMaxLanes) break;
        TimelineCanvas::Lane l;
        l.name = q(n);
        if (const CharacterInfo* info = ed_->doc().character(n)) l.photo = info->photo;
        laneIndex.insert(l.name, int(lanes.size()));
        lanes << l;
    }
    for (int i = 0; i < static_cast<int>(summaries.size()); ++i) {
        const SceneSummary& s = summaries[i];
        TimelineCanvas::Scene sc;
        sc.para = s.para;
        sc.start = at;
        sc.length = std::max(1, s.eighths) / 8.0;
        sc.heading = q(s.heading);
        sc.synopsis = q(s.synopsis);
        sc.time = splitHeading(sc.heading).time;
        for (const auto& n : s.cast) {
            sc.cast << q(n);
            if (laneIndex.contains(q(n))) lanes[laneIndex[q(n)]].scenes << i;
        }
        canvas_->scenes << sc;
        at += sc.length;
    }
    const int pages = ed_->layout().pageCount;
    if (at > 0 && pages > 0) {  // os oitavos são aproximados: a soma passa a bater com as páginas
        const double k = pages / at;
        for (auto& sc : canvas_->scenes) {
            sc.start *= k;
            sc.length *= k;
        }
        at = pages;
    }
    canvas_->lanes = lanes;
    canvas_->total = at;
    canvas_->pages = pages;
    canvas_->fitWritten = fit_->isChecked();
    canvas_->target = target_->value();
    canvas_->current = ed_->sceneIndexAt(ed_->cursor().para);
    canvas_->setMinimumHeight(canvas_->preferredHeight());
    canvas_->update();

    const int pct = int(std::lround(100.0 * pages / std::max(1, target_->value())));
    stats_->setText(tr("%n page(s)", "", pages) + "  ·  " + tr("about %n minute(s)", "", pages) + "  ·  " +
                    tr("%n scene(s)", "", static_cast<int>(summaries.size())) + "  ·  " +
                    tr("%1% of the planned length").arg(pct));
    progress_->setRange(0, 100);
    progress_->setValue(std::min(100, pct));
}
