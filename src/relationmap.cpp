#include "relationmap.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <set>

#include "avatars.h"
#include "theme.h"

namespace {
QString q(const std::u32string& s) { return QString::fromStdU32String(s); }
std::u32string u(const QString& s) { return s.toStdU32String(); }
QString pairKey(QString a, QString b) {
    if (b < a) std::swap(a, b);
    return a + '\n' + b;
}
double distToSegment(QPointF p, QPointF a, QPointF b) {
    const QPointF ab = b - a;
    const double len2 = QPointF::dotProduct(ab, ab);
    const double t = len2 > 0 ? std::clamp(QPointF::dotProduct(p - a, ab) / len2, 0.0, 1.0) : 0.0;
    const QPointF d = p - (a + ab * t);
    return std::hypot(d.x(), d.y());
}
}  // namespace

// ------------------------------------------------------------ janela da ligação

QString RelationDialog::kindName(RelationKind k) {
    switch (k) {
        case RelationKind::Family: return tr("Family");
        case RelationKind::Love: return tr("Love");
        case RelationKind::Friendship: return tr("Friendship");
        case RelationKind::Rivalry: return tr("Conflict");
        case RelationKind::Work: return tr("Work");
        default: return tr("Other");
    }
}

QColor RelationDialog::kindColor(RelationKind k) {
    switch (k) {
        case RelationKind::Family: return QColor("#3fa66b");
        case RelationKind::Love: return QColor("#e0567a");
        case RelationKind::Friendship: return QColor("#3f8fe0");
        case RelationKind::Rivalry: return QColor("#e8743b");
        case RelationKind::Work: return QColor("#8a6fd6");
        default: return QColor("#8b919c");
    }
}

RelationDialog::RelationDialog(const QString& a, const QString& b, const Relationship* current, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Relationship"));
    setMinimumWidth(420);
    auto* heading = new QLabel(tr("%1 and %2").arg(a, b));
    heading->setObjectName("tituloDialogo");
    kind_ = new QComboBox;
    for (int i = 0; i < static_cast<int>(RelationKind::Count); ++i) {
        const auto k = static_cast<RelationKind>(i);
        QPixmap dot(14, 14);
        dot.fill(Qt::transparent);
        QPainter p(&dot);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(kindColor(k));
        p.drawEllipse(QRectF(1, 1, 12, 12));
        p.end();
        kind_->addItem(QIcon(dot), kindName(k), i);
    }
    label_ = new QLineEdit;
    label_->setPlaceholderText(tr("e.g. sisters, ex-husband, boss"));
    if (current) {
        kind_->setCurrentIndex(static_cast<int>(current->kind));
        label_->setText(q(current->label));
    }
    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->addRow(tr("Type:"), kind_);
    form->addRow(tr("Label:"), label_);

    auto* save = new QPushButton(tr("Save"));
    save->setObjectName("primarioBotao");
    auto* cancel = new QPushButton(tr("Cancel"));
    connect(save, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* buttons = new QHBoxLayout;
    if (current) {
        auto* del = new QPushButton(tr("Remove Link"));
        del->setObjectName("perigo");
        connect(del, &QPushButton::clicked, this, [this] {
            remove_ = true;
            accept();
        });
        buttons->addWidget(del);
    }
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(save);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(14);
    root->addWidget(heading);
    root->addLayout(form);
    root->addLayout(buttons);
    label_->setFocus();
    save->setDefault(true);
}

RelationKind RelationDialog::kind() const { return static_cast<RelationKind>(kind_->currentData().toInt()); }
QString RelationDialog::label() const { return label_->text().trimmed(); }

// ------------------------------------------------------------ mapa

RelationMapView::RelationMapView(Editor* editor, QWidget* parent) : QWidget(parent), ed_(editor) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    ThemeManager& tm = ThemeManager::instance();
    auto button = [&](const char* icon, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setObjectName("botaoQuadro");
        b->setProperty("iconName", icon);
        b->setIcon(tm.icon(icon));
        b->setIconSize(QSize(18, 18));
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setAutoRaise(true);
        return b;
    };
    sharedBtn_ = button("cenas", tr("Show who shares scenes"));
    sharedBtn_->setCheckable(true);
    sharedBtn_->setChecked(true);
    connect(sharedBtn_, &QToolButton::toggled, this, [this](bool on) {
        showShared_ = on;
        update();
    });
    arrangeBtn_ = button("aleatorio", tr("Arrange in a circle"));
    connect(arrangeBtn_, &QToolButton::clicked, this, &RelationMapView::arrange);
    fitBtn_ = button("enquadrar", tr("Fit to window"));
    connect(fitBtn_, &QToolButton::clicked, this, &RelationMapView::zoomToFit);
    connect(&tm, &ThemeManager::changed, this, [this] {
        for (QToolButton* b : {sharedBtn_, arrangeBtn_, fitBtn_})
            b->setIcon(ThemeManager::instance().icon(b->property("iconName").toString()));
        update();
    });
}

void RelationMapView::reload() {
    const Document& d = ed_->doc();
    std::vector<std::pair<std::u32string, int>> chars = ed_->characters();
    for (const CharacterInfo& c : d.cast)
        if (std::none_of(chars.begin(), chars.end(), [&](auto& p) { return p.first == c.name; }))
            chars.push_back({c.name, 0});
    const int maxSpeeches = chars.empty() ? 1 : std::max(1, chars.front().second);

    QHash<QString, Node> old = nodes_;
    nodes_.clear();
    order_.clear();
    QStringList unplaced;
    for (const auto& [n, speeches] : chars) {
        const QString name = q(n);
        Node node;
        node.speeches = speeches;
        node.radius = 26 + 18 * std::sqrt(double(speeches) / maxSpeeches);
        if (const CharacterInfo* info = d.character(n)) {
            node.role = q(info->role);
            node.photo = info->photo;
        }
        if (const CharacterNode* saved = d.relations.node(n)) node.pos = {saved->x, saved->y};
        else if (old.contains(name)) node.pos = old[name].pos;
        else unplaced << name;
        nodes_.insert(name, node);
        order_ << name;
    }
    // Quem ainda não tem lugar vai para um círculo em volta dos outros.
    if (!unplaced.isEmpty()) {
        QPointF center;
        double spread = 0;
        const int placed = int(order_.size() - unplaced.size());
        for (const QString& n : order_)
            if (!unplaced.contains(n)) center += nodes_[n].pos;
        if (placed > 0) center /= placed;
        for (const QString& n : order_)
            if (!unplaced.contains(n)) spread = std::max(spread, std::hypot(nodes_[n].pos.x() - center.x(), nodes_[n].pos.y() - center.y()));
        const double radius = placed > 0 ? spread + 160 : std::max(180.0, unplaced.size() * 52.0);
        for (int i = 0; i < unplaced.size(); ++i) {
            const double a = -M_PI / 2 + 2 * M_PI * i / unplaced.size();
            nodes_[unplaced[i]].pos = center + QPointF(std::cos(a), std::sin(a)) * radius;
        }
    }

    // Cenas em comum, a partir de quem fala em cada cena.
    shared_.clear();
    for (const SceneSummary& s : ed_->sceneSummaries()) {
        std::set<QString> who;
        for (const auto& n : s.cast) who.insert(q(n));
        for (auto i = who.begin(); i != who.end(); ++i)
            for (auto j = std::next(i); j != who.end(); ++j) shared_[pairKey(*i, *j)] += 1;
    }
    if (selectedLink_ >= static_cast<int>(d.relations.links.size())) selectedLink_ = -1;
    if (!fitted_ && isVisible() && !order_.isEmpty()) {
        fitted_ = true;
        zoomToFit();
    }
    update();
}

void RelationMapView::arrange() {
    const int n = int(order_.size());
    if (n == 0) return;
    const double radius = std::max(170.0, n * 46.0);
    for (int i = 0; i < n; ++i) {
        const double a = -M_PI / 2 + 2 * M_PI * i / n;
        nodes_[order_[i]].pos = QPointF(std::cos(a), std::sin(a)) * (n == 1 ? 0 : radius);
    }
    commitPositions();
    zoomToFit();
}

void RelationMapView::zoomToFit() {
    if (order_.isEmpty()) return;
    QRectF box;
    for (const QString& n : order_) {
        const Node& node = nodes_[n];
        const QRectF r(node.pos - QPointF(node.radius + 70, node.radius + 20), QSizeF(2 * node.radius + 140, 2 * node.radius + 70));
        box = box.isNull() ? r : box.united(r);
    }
    const QRectF area = QRectF(rect()).adjusted(30, 60, -30, -60);
    zoom_ = std::clamp(std::min(area.width() / box.width(), area.height() / box.height()), 0.3, 1.4);
    pan_ = area.center() - box.center() * zoom_;
    update();
}

void RelationMapView::commitPositions() {
    RelationMap m = ed_->doc().relations;
    m.nodes.clear();
    for (const QString& n : order_) m.nodes.push_back({u(n), std::round(nodes_[n].pos.x()), std::round(nodes_[n].pos.y())});
    ed_->setRelations(m);
    emit changed();
}

void RelationMapView::link(const QString& a, const QString& b, RelationKind kind, const QString& label) {
    if (a == b || a.isEmpty() || b.isEmpty()) return;
    RelationMap m = ed_->doc().relations;
    auto it = std::find_if(m.links.begin(), m.links.end(), [&](const Relationship& r) { return r.links(u(a), u(b)); });
    if (it == m.links.end()) {
        m.links.push_back({u(a), u(b), kind, u(label)});
    } else {
        it->kind = kind;
        it->label = u(label);
    }
    // As posições atuais ficam gravadas junto, para o mapa não se rearrumar depois.
    m.nodes.clear();
    for (const QString& n : order_) m.nodes.push_back({u(n), std::round(nodes_[n].pos.x()), std::round(nodes_[n].pos.y())});
    ed_->setRelations(m);
    emit changed();
    update();
}

void RelationMapView::editLink(const QString& a, const QString& b) {
    const Relationship* cur = ed_->doc().relations.link(u(a), u(b));
    const Relationship copy = cur ? *cur : Relationship{};
    RelationDialog dlg(a, b, cur ? &copy : nullptr, this);
    if (dlg.exec() != QDialog::Accepted) return;
    if (dlg.removeRequested()) {
        RelationMap m = ed_->doc().relations;
        m.links.erase(std::remove_if(m.links.begin(), m.links.end(), [&](const Relationship& r) { return r.links(u(a), u(b)); }),
                      m.links.end());
        selectedLink_ = -1;
        ed_->setRelations(m);
        emit changed();
        update();
        return;
    }
    link(a, b, dlg.kind(), dlg.label());
}

QPoint RelationMapView::nodeCenter(const QString& name) const {
    return nodes_.contains(name) ? toWidget(nodes_[name].pos).toPoint() : QPoint();
}

QPointF RelationMapView::handleOf(const QString& name) const {
    const Node& n = nodes_[name];
    return n.pos + QPointF(n.radius * 0.72, n.radius * 0.72);
}

QPoint RelationMapView::handlePoint(const QString& name) const {
    return nodes_.contains(name) ? toWidget(handleOf(name)).toPoint() : QPoint();
}

QString RelationMapView::nodeAt(QPointF w) const {
    const QPointF m = toMap(w);
    for (int i = int(order_.size()) - 1; i >= 0; --i) {
        const Node& n = nodes_[order_[i]];
        if (std::hypot(m.x() - n.pos.x(), m.y() - n.pos.y()) <= n.radius + 2) return order_[i];
    }
    return {};
}

QString RelationMapView::handleAt(QPointF w) const {
    for (const QString& n : order_) {
        const QPointF h = toWidget(handleOf(n));
        if (std::hypot(w.x() - h.x(), w.y() - h.y()) <= 12) return n;
    }
    return {};
}

int RelationMapView::linkAt(QPointF w) const {
    const auto& links = ed_->doc().relations.links;
    for (int i = 0; i < static_cast<int>(links.size()); ++i) {
        const QString a = q(links[i].a), b = q(links[i].b);
        if (!nodes_.contains(a) || !nodes_.contains(b)) continue;
        if (distToSegment(w, toWidget(nodes_[a].pos), toWidget(nodes_[b].pos)) < 7) return i;
    }
    return -1;
}

void RelationMapView::placeButtons() {
    int x = width() - 16;
    for (QToolButton* b : {fitBtn_, arrangeBtn_, sharedBtn_}) {
        b->resize(34, 34);
        x -= 34;
        b->move(x, 14);
        x -= 6;
    }
}

void RelationMapView::resizeEvent(QResizeEvent*) {
    placeButtons();
    if (!fitted_ && !order_.isEmpty()) {
        fitted_ = true;
        zoomToFit();
    }
}

void RelationMapView::paintEvent(QPaintEvent*) {
    const ThemeColors& c = ThemeManager::instance().colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), c.canvas);

    // Pontinhos de fundo, como no Beat Board.
    const double step = 28 * zoom_;
    if (step > 8) {
        p.setPen(Qt::NoPen);
        p.setBrush(c.border);
        const double ox = std::fmod(pan_.x(), step), oy = std::fmod(pan_.y(), step);
        for (double y = oy; y < height(); y += step)
            for (double x = ox; x < width(); x += step) p.drawEllipse(QPointF(x, y), 1.1, 1.1);
    }

    if (order_.isEmpty()) {
        p.setPen(c.muted);
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() + 1);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, tr("Characters show up here as soon as they speak in the script.\n"
                                                "Then drag from the + of one character to another to link them."));
        return;
    }

    const auto& links = ed_->doc().relations.links;
    std::set<QString> linked;
    for (const Relationship& r : links) linked.insert(pairKey(q(r.a), q(r.b)));

    // Cenas em comum (tracejado), atrás de tudo.
    if (showShared_) {
        int maxShared = 1;
        for (int v : shared_) maxShared = std::max(maxShared, v);
        for (auto it = shared_.cbegin(); it != shared_.cend(); ++it) {
            const QStringList ab = it.key().split('\n');
            if (ab.size() != 2 || !nodes_.contains(ab[0]) || !nodes_.contains(ab[1]) || linked.count(it.key())) continue;
            QColor col = c.muted;
            col.setAlphaF(0.25f + 0.35f * float(it.value()) / float(maxShared));
            QPen pen(col, (1.0 + 2.5 * it.value() / maxShared) * std::min(1.0, zoom_ + 0.2), Qt::DashLine);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.drawLine(toWidget(nodes_[ab[0]].pos), toWidget(nodes_[ab[1]].pos));
        }
    }

    // Ligações com tipo e rótulo.
    QFont small = font();
    small.setPointSizeF(std::max(7.0, small.pointSizeF() * 0.92));
    small.setBold(true);
    for (int i = 0; i < static_cast<int>(links.size()); ++i) {
        const Relationship& r = links[i];
        const QString a = q(r.a), b = q(r.b);
        if (!nodes_.contains(a) || !nodes_.contains(b)) continue;
        const QPointF pa = toWidget(nodes_[a].pos), pb = toWidget(nodes_[b].pos);
        const QColor col = RelationDialog::kindColor(r.kind);
        if (i == selectedLink_) {
            QColor glow = col;
            glow.setAlpha(70);
            p.setPen(QPen(glow, 10, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(pa, pb);
        }
        p.setPen(QPen(col, i == selectedLink_ ? 3.5 : 2.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(pa, pb);
        const QString text = r.label.empty() ? RelationDialog::kindName(r.kind) : q(r.label);
        p.setFont(small);
        const QFontMetricsF fm(small);
        const QSizeF sz(fm.horizontalAdvance(text) + 18, fm.height() + 8);
        const QRectF pill(QPointF((pa + pb) / 2) - QPointF(sz.width() / 2, sz.height() / 2), sz);
        p.setPen(QPen(col, 1.4));
        p.setBrush(c.surface);
        p.drawRoundedRect(pill, sz.height() / 2, sz.height() / 2);
        p.setPen(col.darker((c.window.lightness() < 128) ? 70 : 115));
        p.drawText(pill, Qt::AlignCenter, text);
    }

    // Linha sendo puxada.
    if (!linking_.isEmpty()) {
        QPen pen(c.accent, 2.4, Qt::DashLine, Qt::RoundCap);
        p.setPen(pen);
        p.drawLine(toWidget(nodes_[linking_].pos), linkEnd_);
    }

    // Personagens.
    QFont nameFont = font();
    nameFont.setBold(true);
    QFont roleFont = font();
    roleFont.setPointSizeF(std::max(7.0, roleFont.pointSizeF() * 0.9));
    const QString target = !linking_.isEmpty() ? nodeAt(linkEnd_) : QString();
    for (const QString& name : order_) {
        const Node& n = nodes_[name];
        const QPointF ctr = toWidget(n.pos);
        const double r = n.radius * zoom_;
        // Sombra suave.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, (c.window.lightness() < 128) ? 90 : 40));
        p.drawEllipse(ctr + QPointF(0, 3), r + 3, r + 3);
        const bool hot = name == hover_ || name == dragging_ || name == target || name == linking_;
        p.setBrush(hot ? c.accent : c.surface);
        p.drawEllipse(ctr, r + 3, r + 3);
        avatars::draw(p, QRectF(ctr - QPointF(r, r), QSizeF(2 * r, 2 * r)), name, n.photo);

        p.setFont(nameFont);
        const QFontMetricsF fm(nameFont);
        const double w = fm.horizontalAdvance(name) + 16;
        const QRectF label(ctr.x() - w / 2, ctr.y() + r + 8, w, fm.height() + 4);
        p.setPen(Qt::NoPen);
        p.setBrush(c.surface);
        p.drawRoundedRect(label, 6, 6);
        p.setPen(c.text);
        p.drawText(label, Qt::AlignCenter, name);
        QString sub = n.role;
        if (n.speeches > 0) sub = (sub.isEmpty() ? QString() : sub + "  ·  ") + tr("%n speech(es)", "", n.speeches);
        if (!sub.isEmpty()) {
            p.setFont(roleFont);
            const QFontMetricsF rf(roleFont);
            const double sw = rf.horizontalAdvance(sub) + 12;
            const QRectF subRect(ctr.x() - sw / 2, label.bottom() + 3, sw, rf.height() + 2);
            QColor back = c.canvas;
            back.setAlphaF(0.9f);
            p.setPen(Qt::NoPen);
            p.setBrush(back);
            p.drawRoundedRect(subRect, 5, 5);
            p.setPen(c.muted);
            p.drawText(subRect, Qt::AlignCenter, sub);
        }
        // "+" de ligar, só com o mouse em cima.
        if (name == hover_ && dragging_.isEmpty() && linking_.isEmpty()) {
            const QPointF h = toWidget(handleOf(name));
            p.setPen(QPen(c.surface, 2));
            p.setBrush(c.accent);
            p.drawEllipse(h, 10, 10);
            p.setPen(QPen(c.accentText, 2, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(h - QPointF(4.5, 0), h + QPointF(4.5, 0));
            p.drawLine(h - QPointF(0, 4.5), h + QPointF(0, 4.5));
        }
    }

    // Legenda.
    QFont legendFont = font();
    legendFont.setPointSizeF(std::max(7.0, legendFont.pointSizeF() * 0.9));
    p.setFont(legendFont);
    const QFontMetricsF lf(legendFont);
    double x = 18;
    const double y = height() - 30;
    double total = 16;
    for (int i = 0; i < static_cast<int>(RelationKind::Count); ++i)
        total += 22 + lf.horizontalAdvance(RelationDialog::kindName(static_cast<RelationKind>(i))) + 10;
    p.setPen(Qt::NoPen);
    p.setBrush(c.surface);
    p.drawRoundedRect(QRectF(10, y - 10, total, 30), 10, 10);
    for (int i = 0; i < static_cast<int>(RelationKind::Count); ++i) {
        const auto k = static_cast<RelationKind>(i);
        p.setPen(Qt::NoPen);
        p.setBrush(RelationDialog::kindColor(k));
        p.drawEllipse(QPointF(x + 6, y + 5), 5, 5);
        p.setPen(c.text);
        const QString t = RelationDialog::kindName(k);
        p.drawText(QPointF(x + 16, y + 5 + lf.ascent() / 2 - 1), t);
        x += 22 + lf.horizontalAdvance(t) + 10;
    }
}

void RelationMapView::mousePressEvent(QMouseEvent* e) {
    setFocus();
    pressWidget_ = e->position();
    moved_ = false;
    if (e->button() != Qt::LeftButton) return;
    const QString h = handleAt(e->position());
    if (!h.isEmpty() && h == hover_) {
        linking_ = h;
        linkEnd_ = e->position();
        update();
        return;
    }
    const QString n = nodeAt(e->position());
    if (!n.isEmpty()) {
        dragging_ = n;
        grabOffset_ = nodes_[n].pos - toMap(e->position());
        order_.removeAll(n);  // vai para cima dos outros
        order_.append(n);
        selectedLink_ = -1;
        update();
        return;
    }
    const int l = linkAt(e->position());
    selectedLink_ = l;
    if (l < 0) {
        panning_ = true;
        panStart_ = pan_;
        setCursor(Qt::ClosedHandCursor);
    }
    update();
}

void RelationMapView::mouseMoveEvent(QMouseEvent* e) {
    const QPointF w = e->position();
    if ((w - pressWidget_).manhattanLength() > 3) moved_ = true;
    if (!linking_.isEmpty()) {
        linkEnd_ = w;
        update();
        return;
    }
    if (!dragging_.isEmpty()) {
        nodes_[dragging_].pos = toMap(w) + grabOffset_;
        update();
        return;
    }
    if (panning_) {
        pan_ = panStart_ + (w - pressWidget_);
        update();
        return;
    }
    QString h = nodeAt(w);
    if (h.isEmpty() && !hover_.isEmpty() && handleAt(w) == hover_) h = hover_;  // o "+" fica um pouco fora do círculo
    if (h != hover_) {
        hover_ = h;
        update();
    }
    if (!handleAt(w).isEmpty() && handleAt(w) == hover_) setCursor(Qt::CrossCursor);
    else if (!h.isEmpty()) setCursor(Qt::OpenHandCursor);
    else if (linkAt(w) >= 0) setCursor(Qt::PointingHandCursor);
    else unsetCursor();
}

void RelationMapView::mouseReleaseEvent(QMouseEvent* e) {
    if (!linking_.isEmpty()) {
        const QString from = linking_;
        const QString to = nodeAt(e->position());
        linking_.clear();
        update();
        if (!to.isEmpty() && to != from) editLink(from, to);
        return;
    }
    if (!dragging_.isEmpty()) {
        dragging_.clear();
        if (moved_) commitPositions();
        update();
        return;
    }
    if (panning_) {
        panning_ = false;
        unsetCursor();
    }
}

void RelationMapView::mouseDoubleClickEvent(QMouseEvent* e) {
    const QString n = nodeAt(e->position());
    if (!n.isEmpty()) {
        emit openSheet(n);
        return;
    }
    const int l = linkAt(e->position());
    if (l >= 0) {
        const Relationship r = ed_->doc().relations.links[l];
        editLink(q(r.a), q(r.b));
    }
}

void RelationMapView::wheelEvent(QWheelEvent* e) {
    const double factor = std::pow(1.0015, e->angleDelta().y());
    const double z = std::clamp(zoom_ * factor, 0.25, 2.5);
    const QPointF anchor = toMap(e->position());
    zoom_ = z;
    pan_ = e->position() - anchor * zoom_;
    update();
}

void RelationMapView::keyPressEvent(QKeyEvent* e) {
    if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && selectedLink_ >= 0) {
        RelationMap m = ed_->doc().relations;
        if (selectedLink_ < static_cast<int>(m.links.size())) m.links.erase(m.links.begin() + selectedLink_);
        selectedLink_ = -1;
        ed_->setRelations(m);
        emit changed();
        update();
        return;
    }
    QWidget::keyPressEvent(e);
}

void RelationMapView::contextMenuEvent(QContextMenuEvent* e) {
    ThemeManager& tm = ThemeManager::instance();
    QMenu menu(this);
    const QString n = nodeAt(e->pos());
    const int l = linkAt(e->pos());
    if (!n.isEmpty()) {
        menu.addAction(tm.icon("elenco"), tr("Open Character Sheet..."), this, [this, n] { emit openSheet(n); });
        QMenu* to = menu.addMenu(tm.icon("relacoes"), tr("Link To"));
        for (const QString& other : order_)
            if (other != n) to->addAction(other, this, [this, n, other] { editLink(n, other); });
        to->setEnabled(order_.size() > 1);
    } else if (l >= 0) {
        const Relationship r = ed_->doc().relations.links[l];
        menu.addAction(tr("Edit Relationship..."), this, [this, r] { editLink(q(r.a), q(r.b)); });
        menu.addAction(tr("Remove Link"), this, [this, l] {
            selectedLink_ = l;
            QKeyEvent del(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
            keyPressEvent(&del);
        });
    } else {
        menu.addAction(tm.icon("aleatorio"), tr("Arrange in a Circle"), this, &RelationMapView::arrange);
        menu.addAction(tm.icon("enquadrar"), tr("Fit to Window"), this, &RelationMapView::zoomToFit);
    }
    menu.exec(e->globalPos());
}
