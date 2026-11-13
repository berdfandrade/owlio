#include "timeline.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
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
constexpr int kActs = 96;
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
    std::function<void(const StoryStructure&)> onStructure;  // a forma narrativa mudou (grava no editor)
    QList<Scene> scenes;
    QList<Lane> lanes;
    StoryStructure st;  // nunca vazia: sem nada no documento, vale a convencional
    double total = 0;  // páginas escritas (pela soma das cenas)
    int target = 110;
    bool fitWritten = false;  // escala só nas páginas escritas
    int current = -1;  // cena do cursor
    int pages = 0;     // páginas do roteiro paginado
    int hover = -1;
    int dragAct = -1, dragBeat = -1, hoverAct = -1, hoverBeat = -1;

    double scale() const { return fitWritten ? std::max(total, 1.0) : std::max<double>({total, double(target), 1.0}); }
    double xOf(double page) const { return kLeft + page / scale() * (width() - kLeft - kRight); }
    double xOfFrac(double f) const { return xOf(f * target); }
    double fracAt(double x) const {
        return std::clamp((x - kLeft) / std::max(1, width() - kLeft - kRight) * scale() / std::max(1, target), 0.0, 1.0);
    }
    int actsTop() const { return kTop + kRuler; }
    int scenesTop() const { return kTop + kRuler + kActs + 16; }
    int lanesTop() const { return scenesTop() + kScenes + 28; }
    int preferredHeight() const { return lanesTop() + int(lanes.size()) * kLane + 40; }
    QFont smallFont() const {
        QFont f = font();
        f.setPointSizeF(std::max(7.0, f.pointSizeF() * 0.88));
        return f;
    }

    // Rótulos dos pontos de virada em até três fileiras, sem se sobreporem; perto da borda direita o
    // rótulo vai para a esquerda do losango. Devolve fileira + 10 quando o rótulo fica à esquerda.
    std::vector<int> beatRows() const {
        std::vector<int> order(st.beats.size()), rows(st.beats.size(), 0);
        for (size_t i = 0; i < order.size(); ++i) order[i] = int(i);
        std::sort(order.begin(), order.end(), [&](int a, int b) { return st.beats[a].at < st.beats[b].at; });
        const QFontMetricsF fm(smallFont());
        const double edge = width() - kRight;
        double right[3] = {-1e9, -1e9, -1e9};
        for (int i : order) {
            const double x = xOfFrac(st.beats[i].at);
            const double w = fm.horizontalAdvance(q(st.beats[i].name));
            const bool left = x + 12 + w > edge;
            const double leftEdge = left ? x - 12 - w : x - 6;
            int row = -1;
            for (int r = 0; r < 3 && row < 0; ++r)
                if (leftEdge > right[r]) row = r;
            if (row < 0) row = int(std::min_element(right, right + 3) - right);
            right[row] = left ? x + 8 : x + 12 + w;
            rows[i] = row + (left ? 10 : 0);
        }
        return rows;
    }
    int boundaryAt(QPoint p) const {
        if (p.y() < actsTop() || p.y() > actsTop() + 40) return -1;
        for (int i = 0; i + 1 < int(st.acts.size()); ++i)
            if (std::abs(p.x() - xOfFrac(st.acts[i].end)) <= 6) return i;
        return -1;
    }
    int beatAt(QPoint p) const {
        const std::vector<int> rows = beatRows();
        for (int j = 0; j < int(st.beats.size()); ++j)
            if (std::abs(p.x() - xOfFrac(st.beats[j].at)) <= 7 &&
                std::abs(p.y() - (actsTop() + 47 + (rows[j] % 10) * 16)) <= 7)
                return j;
        return -1;
    }
    int actAt(QPoint p) const {
        if (p.y() < actsTop() || p.y() > actsTop() + 40) return -1;
        const double f = fracAt(p.x());
        for (int i = 0; i < int(st.acts.size()); ++i)
            if (f <= st.acts[i].end) return i;
        return -1;
    }

    int sceneAt(QPoint p) const {
        if (p.y() < scenesTop() || p.y() > lanesTop() + int(lanes.size()) * kLane) return -1;
        for (int i = 0; i < scenes.size(); ++i) {
            const double x0 = xOf(scenes[i].start), x1 = xOf(scenes[i].start + scenes[i].length);
            if (p.x() >= x0 && p.x() < std::max(x1, x0 + 3)) return i;
        }
        return -1;
    }

    void commit() {
        st.preset = "custom";
        if (onStructure) onStructure(st);
    }
    void renameAct(int i);
    void renameBeat(int j);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton) return;
        dragBeat = beatAt(e->pos());
        if (dragBeat < 0) dragAct = boundaryAt(e->pos());
        if (dragBeat >= 0 || dragAct >= 0) setCursor(Qt::SizeHorCursor);
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        const double f = fracAt(e->position().x());
        if (dragBeat >= 0) {
            st.beats[dragBeat].at = f;
            update();
            return;
        }
        if (dragAct >= 0) {
            const double lo = (dragAct > 0 ? st.acts[dragAct - 1].end : 0.0) + 0.02;
            const double hi = st.acts[dragAct + 1].end - 0.02;
            st.acts[dragAct].end = std::clamp(f, lo, hi);
            update();
            return;
        }
        const int b = beatAt(e->pos()), a = b < 0 ? boundaryAt(e->pos()) : -1;
        if (b != hoverBeat || a != hoverAct) {
            hoverBeat = b;
            hoverAct = a;
            update();
        }
        if (a >= 0 || b >= 0) {
            setCursor(Qt::SizeHorCursor);
            QToolTip::hideText();
            return;
        }
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
        hover = hoverAct = hoverBeat = -1;
        update();
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (dragBeat >= 0 || dragAct >= 0) {
            dragBeat = dragAct = -1;
            unsetCursor();
            commit();
            return;
        }
        const int s = sceneAt(e->pos());
        if (e->button() == Qt::LeftButton && s >= 0 && onOpen) onOpen(scenes[s].para);
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if (const int b = beatAt(e->pos()); b >= 0) renameBeat(b);
        else if (const int a = actAt(e->pos()); a >= 0) renameAct(a);
    }
    void contextMenuEvent(QContextMenuEvent* e) override;
};

namespace {
const char* const kActColors[] = {"#3f8fe0", "#8a6fd6", "#e0567a", "#3fa66b", "#e8743b", "#2f93ad", "#d6a23a"};
}

void TimelineCanvas::renameAct(int i) {
    QDialog dlg(this);
    dlg.setWindowTitle(TimelineView::tr("Act"));
    auto* name = new QLineEdit(q(st.acts[i].name));
    auto* sub = new QLineEdit(q(st.acts[i].subtitle));
    auto* form = new QFormLayout(&dlg);
    form->setContentsMargins(22, 20, 22, 18);
    form->setSpacing(10);
    form->addRow(TimelineView::tr("Name:"), name);
    form->addRow(TimelineView::tr("Subtitle:"), sub);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    dlg.setMinimumWidth(380);
    if (dlg.exec() != QDialog::Accepted) return;
    st.acts[i].name = name->text().trimmed().toStdU32String();
    st.acts[i].subtitle = sub->text().trimmed().toStdU32String();
    commit();
}

void TimelineCanvas::renameBeat(int j) {
    bool ok = false;
    const QString t = QInputDialog::getText(this, TimelineView::tr("Plot Point"), TimelineView::tr("Name:"),
                                            QLineEdit::Normal, q(st.beats[j].name), &ok);
    if (!ok || t.trimmed().isEmpty()) return;
    st.beats[j].name = t.trimmed().toStdU32String();
    commit();
}

void TimelineCanvas::contextMenuEvent(QContextMenuEvent* e) {
    if (e->pos().y() < actsTop() || e->pos().y() > actsTop() + kActs) return;
    const double f = fracAt(e->pos().x());
    const int b = beatAt(e->pos());
    const int a = b < 0 ? actAt(e->pos()) : -1;
    QMenu menu(this);
    if (b >= 0) {
        menu.addAction(TimelineView::tr("Rename Plot Point..."), this, [this, b] { renameBeat(b); });
        menu.addAction(TimelineView::tr("Remove Plot Point"), this, [this, b] {
            st.beats.erase(st.beats.begin() + b);
            commit();
        });
        menu.addSeparator();
    }
    if (a >= 0) {
        menu.addAction(TimelineView::tr("Rename Act..."), this, [this, a] { renameAct(a); });
        menu.addAction(TimelineView::tr("Split Act Here"), this, [this, a, f] {
            const double start = a > 0 ? st.acts[a - 1].end : 0.0;
            if (f - start < 0.02 || st.acts[a].end - f < 0.02) return;
            StoryAct n;
            n.name = TimelineView::tr("New act").toStdU32String();
            n.end = st.acts[a].end;
            n.color = kActColors[st.acts.size() % std::size(kActColors)];
            st.acts[a].end = f;
            st.acts.insert(st.acts.begin() + a + 1, n);
            commit();
        });
        QAction* merge = menu.addAction(TimelineView::tr("Merge with Next Act"), this, [this, a] {
            st.acts[a].end = st.acts[a + 1].end;
            st.acts.erase(st.acts.begin() + a + 1);
            commit();
        });
        merge->setEnabled(a + 1 < int(st.acts.size()));
        menu.addSeparator();
    }
    menu.addAction(TimelineView::tr("Add Plot Point Here"), this, [this, f] {
        bool ok = false;
        const QString t = QInputDialog::getText(this, TimelineView::tr("Plot Point"), TimelineView::tr("Name:"),
                                                QLineEdit::Normal, {}, &ok);
        if (!ok || t.trimmed().isEmpty()) return;
        st.beats.push_back({t.trimmed().toStdU32String(), f});
        commit();
    });
    menu.exec(e->globalPos());
}

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

    // ---- Atos e pontos de virada, na forma narrativa escolhida (arraste as divisas e os losangos).
    y = actsTop();
    sectionLabel(y + 8, TimelineView::tr("Structure"));
    p.save();
    p.setClipRect(QRectF(x0 - 8, 0, x1 - x0 + 16, height()));
    double from = 0;
    for (int i = 0; i < int(st.acts.size()); ++i) {
        const StoryAct& a = st.acts[i];
        const double ax = xOfFrac(from), bx = xOfFrac(a.end);
        from = a.end;
        if (ax >= x1) continue;
        QRectF r(QPointF(ax + 1, y), QPointF(std::min(bx, x1) - 1, y + 40));
        const QColor tint(QString::fromStdString(a.color));
        QColor fill = tint;
        fill.setAlphaF(dark ? 0.22f : 0.13f);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(r, 8, 8);
        p.setPen(tint.darker(dark ? 60 : 125));
        p.setFont(bold);
        p.drawText(r.adjusted(10, 3, -6, -18), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(bold).elidedText(q(a.name), Qt::ElideRight, int(r.width()) - 14));
        p.setFont(small);
        p.setPen(c.muted);
        p.drawText(r.adjusted(10, 20, -6, -2), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(small).elidedText(q(a.subtitle), Qt::ElideRight, int(r.width()) - 14));
        if (i + 1 < int(st.acts.size())) {  // alça da divisa
            const bool hot = i == dragAct || i == hoverAct;
            p.setPen(Qt::NoPen);
            p.setBrush(hot ? c.accent : c.border);
            p.drawRoundedRect(QRectF(bx - 2, y + 12, 4, 16), 2, 2);
        }
    }
    const std::vector<int> rows = beatRows();
    p.setFont(small);
    for (int j = 0; j < int(st.beats.size()); ++j) {
        const double x = xOfFrac(st.beats[j].at);
        if (x > x1) continue;
        const double cy = y + 47 + (rows[j] % 10) * 16;
        QPainterPath diamond;
        diamond.moveTo(x, cy - 5);
        diamond.lineTo(x + 5, cy);
        diamond.lineTo(x, cy + 5);
        diamond.lineTo(x - 5, cy);
        diamond.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(j == dragBeat || j == hoverBeat ? c.accent : c.text);
        p.drawPath(diamond);
        p.setPen(c.text);
        if (rows[j] >= 10) p.drawText(QRectF(x - 208, cy - 8, 200, 16), Qt::AlignRight | Qt::AlignVCenter, q(st.beats[j].name));
        else p.drawText(QRectF(x + 8, cy - 8, 200, 16), Qt::AlignLeft | Qt::AlignVCenter, q(st.beats[j].name));
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

// ------------------------------------------------------------ formas narrativas prontas

QList<QPair<QString, QString>> TimelineView::presets() {
    return {{"conventional", tr("Conventional (three acts)")}, {"fragmented", tr("Fragmented (nonlinear)")},
            {"episode", tr("TV episode (drama)")},           {"sitcom", tr("Sitcom")},
            {"fiveact", tr("Five acts (Freytag)")},         {"savethecat", tr("Save the Cat")},
            {"hero", tr("Hero's journey")},                  {"kishotenketsu", tr("Kishōtenketsu")}};
}

StoryStructure TimelineView::preset(const QString& key) {
    StoryStructure st;
    st.preset = key.toStdString();
    auto act = [&](const QString& name, const QString& sub, double end) {
        st.acts.push_back({name.toStdU32String(), sub.toStdU32String(), end,
                           kActColors[st.acts.size() % std::size(kActColors)]});
    };
    auto beat = [&](const QString& name, double at) { st.beats.push_back({name.toStdU32String(), at}); };
    if (key == "fragmented") {
        act(tr("Present"), tr("Hook"), 0.18);
        act(tr("Past"), tr("Fragment"), 0.36);
        act(tr("Present"), tr("Escalation"), 0.54);
        act(tr("Past"), tr("Fragment"), 0.72);
        act(tr("Present"), tr("Convergence"), 1.0);
        beat(tr("Mystery set up"), 0.08);
        beat(tr("First reveal"), 0.36);
        beat(tr("Timelines cross"), 0.72);
        beat(tr("Full picture"), 0.92);
    } else if (key == "episode") {
        act(tr("Teaser"), tr("Cold open"), 0.08);
        act(tr("Act One"), QString(), 0.31);
        act(tr("Act Two"), QString(), 0.54);
        act(tr("Act Three"), QString(), 0.77);
        act(tr("Act Four"), QString(), 0.96);
        act(tr("Tag"), QString(), 1.0);
        beat(tr("Act out"), 0.31);
        beat(tr("Act out"), 0.54);
        beat(tr("Act out"), 0.77);
    } else if (key == "sitcom") {
        act(tr("Cold open"), QString(), 0.1);
        act(tr("Act One"), tr("A and B stories"), 0.5);
        act(tr("Act Two"), tr("Complications"), 0.92);
        act(tr("Tag"), QString(), 1.0);
        beat(tr("Problem"), 0.15);
        beat(tr("Act break"), 0.5);
        beat(tr("Resolution"), 0.88);
    } else if (key == "fiveact") {
        act(tr("Exposition"), QString(), 0.2);
        act(tr("Rising action"), QString(), 0.45);
        act(tr("Climax"), QString(), 0.6);
        act(tr("Falling action"), QString(), 0.82);
        act(tr("Denouement"), QString(), 1.0);
        beat(tr("Inciting incident"), 0.15);
        beat(tr("Turning point"), 0.52);
        beat(tr("Catastrophe"), 0.88);
    } else if (key == "savethecat") {
        act(tr("Act One"), tr("Thesis"), 0.2);
        act(tr("Act Two A"), tr("Fun and games"), 0.5);
        act(tr("Act Two B"), tr("Bad guys close in"), 0.75);
        act(tr("Act Three"), tr("Synthesis"), 1.0);
        beat(tr("Opening image"), 0.0);
        beat(tr("Theme stated"), 0.05);
        beat(tr("Catalyst"), 0.1);
        beat(tr("Debate"), 0.13);
        beat(tr("Break into Two"), 0.2);
        beat(tr("B story"), 0.22);
        beat(tr("Midpoint"), 0.5);
        beat(tr("All is lost"), 0.68);
        beat(tr("Dark night of the soul"), 0.7);
        beat(tr("Break into Three"), 0.77);
        beat(tr("Finale"), 0.8);
        beat(tr("Final image"), 0.99);
    } else if (key == "hero") {
        act(tr("Departure"), tr("Ordinary world"), 0.25);
        act(tr("Initiation"), tr("Special world"), 0.75);
        act(tr("Return"), tr("Master of two worlds"), 1.0);
        beat(tr("Call to adventure"), 0.08);
        beat(tr("Meeting the mentor"), 0.17);
        beat(tr("Crossing the threshold"), 0.25);
        beat(tr("Ordeal"), 0.5);
        beat(tr("Reward"), 0.62);
        beat(tr("The road back"), 0.75);
        beat(tr("Resurrection"), 0.9);
    } else if (key == "kishotenketsu") {
        act(tr("Ki"), tr("Introduction"), 0.25);
        act(tr("Shō"), tr("Development"), 0.5);
        act(tr("Ten"), tr("Twist"), 0.75);
        act(tr("Ketsu"), tr("Conclusion"), 1.0);
        beat(tr("The twist"), 0.5);
    } else {
        st.preset = "conventional";
        act(tr("Act I"), tr("Setup"), 0.25);
        act(tr("Act II"), tr("Confrontation"), 0.75);
        act(tr("Act III"), tr("Resolution"), 1.0);
        beat(tr("Inciting incident"), 0.12);
        beat(tr("Plot point 1"), 0.25);
        beat(tr("Midpoint"), 0.5);
        beat(tr("Plot point 2"), 0.75);
        beat(tr("Climax"), 0.9);
    }
    return st;
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

    shape_ = new QComboBox;
    shape_->setToolTip(tr("Narrative shape. Drag the act dividers and the plot points to reshape it; "
                          "double-click or right-click to rename, split or add."));
    for (const auto& [key, label] : presets()) shape_->addItem(label, key);
    shape_->addItem(tr("Custom"), "custom");
    connect(shape_, &QComboBox::activated, this, [this](int i) {
        const QString key = shape_->itemData(i).toString();
        if (key == "custom") return;
        ed_->setStructure(preset(key));
        emit changed();
        reload();
    });
    auto* shapeLabel = new QLabel(tr("Structure:"));
    shapeLabel->setObjectName("subtituloDialogo");

    // O cabeçalho não pode impor largura mínima grande: ela vira a largura mínima da janela central
    // e trava o redimensionamento da barra lateral. Título e números cortam quando falta espaço.
    heading->setMinimumWidth(1);
    stats_->setMinimumWidth(1);
    progress_->setMinimumWidth(40);
    shape_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    shape_->setMinimumContentsLength(14);

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
    header->addWidget(shapeLabel);
    header->addWidget(shape_);
    header->addSpacing(8);
    header->addWidget(fit_);
    header->addSpacing(8);
    header->addWidget(targetLabel);
    header->addWidget(target_);

    canvas_ = new TimelineCanvas(this);
    canvas_->onOpen = [this](int para) { emit openScene(para); };
    canvas_->onStructure = [this](const StoryStructure& st) {
        ed_->setStructure(st);
        emit changed();
        reload();
    };
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

TimelineCanvas* TimelineView::structureCanvas() const { return canvas_; }
QPoint TimelineView::dividerPoint(int act) const {
    if (act < 0 || act + 1 >= int(canvas_->st.acts.size())) return {};
    return QPointF(canvas_->xOfFrac(canvas_->st.acts[act].end), canvas_->actsTop() + 20).toPoint();
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
    const StoryStructure& doc = ed_->doc().structure;
    canvas_->st = doc.empty() ? preset("conventional") : doc;
    const int shapeIndex = shape_->findData(QString::fromStdString(canvas_->st.preset));
    shape_->setCurrentIndex(shapeIndex >= 0 ? shapeIndex : shape_->count() - 1);
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
