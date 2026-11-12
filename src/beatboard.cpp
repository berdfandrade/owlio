#include "beatboard.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QSettings>
#include <QTextLayout>
#include <QToolButton>
#include <QUrl>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "beatdialog.h"
#include "theme.h"

namespace {
QString q(const std::u32string& s) { return QString::fromStdU32String(s); }

constexpr double kRadius = 12;
constexpr double kBand = 6;          // faixa colorida no topo
constexpr double kConnector = 7;     // raio da bolinha de ligação
constexpr double kGrip = 16;         // canto de redimensionar
constexpr double kGrid = 24;         // espaçamento dos pontos do fundo
constexpr double kHeader = 52;       // faixa do título do quadro, no viewport

// Texto com quebra de linha e reticências na última linha que cabe.
void drawWrapped(QPainter* p, const QRectF& r, const QString& text, const QFont& f, const QColor& c) {
    if (r.height() <= 0 || text.isEmpty()) return;
    QFontMetricsF fm(f);
    const int maxLines = std::max(1, static_cast<int>(r.height() / fm.lineSpacing()));
    QTextLayout layout(text, f);
    layout.beginLayout();
    QList<QPair<int, int>> lines;
    while (lines.size() < maxLines) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(r.width());
        lines.append({line.textStart(), line.textLength()});
    }
    const bool more = layout.createLine().isValid();
    layout.endLayout();
    p->setFont(f);
    p->setPen(c);
    double y = r.top();
    for (int i = 0; i < lines.size(); ++i) {
        QString s = text.mid(lines[i].first, lines[i].second);
        s.replace('\n', ' ');
        if (i == lines.size() - 1 && more) s = fm.elidedText(s.trimmed() + QStringLiteral("…"), Qt::ElideRight, r.width());
        p->drawText(QRectF(r.left(), y, r.width(), fm.lineSpacing()), Qt::AlignLeft | Qt::AlignVCenter, s.trimmed());
        y += fm.lineSpacing();
    }
}
}  // namespace

// ------------------------------------------------------------ beat

class BeatItem : public QGraphicsItem {
public:
    BeatItem(BeatBoardView* view, const Beat& b) : view_(view), beat_(b) {
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
        setPos(b.x, b.y);
        image_ = beatimage::decode(b.image);
        setCursor(Qt::OpenHandCursor);
    }

    const Beat& beat() const { return beat_; }
    Beat current() const {
        Beat b = beat_;
        b.x = std::round(pos().x());
        b.y = std::round(pos().y());
        b.w = std::round(size_.width() > 0 ? size_.width() : beat_.w);
        b.h = std::round(size_.height() > 0 ? size_.height() : beat_.h);
        return b;
    }
    QSizeF size() const { return size_.isValid() ? size_ : QSizeF(beat_.w, beat_.h); }
    void setSize(QSizeF s) {
        prepareGeometryChange();
        size_ = QSizeF(std::max(140.0, s.width()), std::max(90.0, s.height()));
        view_->flowsChanged();
    }
    QRectF body() const { return QRectF(QPointF(0, 0), size()); }
    QRectF boundingRect() const override { return body().adjusted(-10, -10, kConnector + 12, 12); }
    QPainterPath shape() const override {
        QPainterPath path;
        path.addRoundedRect(body(), kRadius, kRadius);
        path.addEllipse(connectorCenter(), kConnector + 4, kConnector + 4);
        return path;
    }
    QPointF connectorCenter() const { return QPointF(size().width(), size().height() / 2); }
    bool onConnector(QPointF local) const {
        return (hover_ || isSelected()) && QLineF(local, connectorCenter()).length() <= kConnector + 4;
    }
    bool onGrip(QPointF local) const {
        const QRectF b = body();
        return local.x() > b.right() - kGrip && local.y() > b.bottom() - kGrip && local.x() <= b.right() + 2;
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        const ThemeColors& c = ThemeManager::instance().colors();
        const QRectF r = body();
        p->setRenderHint(QPainter::Antialiasing);

        // Sombra suave em camadas.
        p->setPen(Qt::NoPen);
        for (int i = 6; i >= 1; --i) {
            p->setBrush(QColor(0, 0, 0, static_cast<int>((c.dark ? 0.10 : 0.035) * (1.0 - i / 7.0) * 255)));
            p->drawRoundedRect(r.adjusted(-i, -i + 3, i, i + 3), kRadius + i, kRadius + i);
        }

        QPainterPath card;
        card.addRoundedRect(r, kRadius, kRadius);
        p->setBrush(c.dark ? c.raised : QColor(Qt::white));
        p->setPen(isSelected() ? QPen(c.accent, 2) : QPen(c.dark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 22), 1));
        p->drawPath(card);

        p->save();
        p->setClipPath(card);
        const QColor band(QString::fromStdString(beat_.color));
        p->fillRect(QRectF(r.left(), r.top(), r.width(), kBand), band.isValid() ? band : c.accent);

        double top = r.top() + kBand;
        if (!image_.isNull()) {
            // Imagem cobrindo a área de cima, recortada no centro.
            const double ih = std::min(r.height() - 46, r.height() * 0.62);
            const QRectF target(r.left(), top, r.width(), ih);
            const double scale = std::max(target.width() / image_.width(), target.height() / image_.height());
            const QSizeF src(target.width() / scale, target.height() / scale);
            const QRectF source((image_.width() - src.width()) / 2, (image_.height() - src.height()) / 2,
                                src.width(), src.height());
            p->setRenderHint(QPainter::SmoothPixmapTransform);
            p->drawImage(target, image_, source);
            top += ih;
        }
        p->restore();

        QFont title = QApplication::font();
        title.setPixelSize(14);
        title.setWeight(QFont::DemiBold);
        QFont text = QApplication::font();
        text.setPixelSize(12);
        const QRectF content = QRectF(r.left() + 12, top + 9, r.width() - 24, r.bottom() - top - 18);
        QFontMetricsF tm(title);
        double y = content.top();
        const QString t = q(beat_.title).trimmed();
        const QString body = q(beat_.text).trimmed();
        if (!t.isEmpty()) {
            p->setFont(title);
            p->setPen(c.text);
            p->drawText(QRectF(content.left(), y, content.width(), tm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                        tm.elidedText(t, Qt::ElideRight, content.width()));
            y += tm.height() + 3;
        }
        if (!body.isEmpty()) {
            drawWrapped(p, QRectF(content.left(), y, content.width(), content.bottom() - y), body, text, c.muted);
        } else if (t.isEmpty() && image_.isNull()) {
            text.setItalic(true);
            drawWrapped(p, content, QObject::tr("Double-click to write this beat."), text, c.muted);
        }

        if (hover_ || isSelected()) {
            // Bolinha de ligação (linhas de fluxo) e canto de redimensionar.
            p->setPen(QPen(c.dark ? c.raised : QColor(Qt::white), 2));
            p->setBrush(c.accent);
            p->drawEllipse(connectorCenter(), kConnector, kConnector);
            p->setPen(QPen(c.accentText, 1.6, Qt::SolidLine, Qt::RoundCap));
            const QPointF cc = connectorCenter();
            p->drawLine(cc + QPointF(-3, 0), cc + QPointF(3, 0));
            p->drawLine(cc + QPointF(0, -3), cc + QPointF(0, 3));
            p->setPen(QPen(c.muted, 1.3, Qt::SolidLine, Qt::RoundCap));
            for (int k = 1; k <= 3; ++k)
                p->drawLine(QPointF(r.right() - 4, r.bottom() - 4 - k * 3.5), QPointF(r.right() - 4 - k * 3.5, r.bottom() - 4));
        }
    }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
        if (change == ItemPositionHasChanged) view_->flowsChanged();
        if (change == ItemSelectedHasChanged) update();
        return QGraphicsItem::itemChange(change, value);
    }
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override { hover_ = true; update(); }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override { hover_ = false; update(); }
    void hoverMoveEvent(QGraphicsSceneHoverEvent* e) override {
        const QPointF l = e->pos();
        setCursor(onConnector(l) ? Qt::CrossCursor : onGrip(l) ? Qt::SizeFDiagCursor : Qt::OpenHandCursor);
    }

private:
    BeatBoardView* view_;
    Beat beat_;
    QImage image_;
    QSizeF size_;
    bool hover_ = false;
};

// ------------------------------------------------------------ linha de fluxo

// Curva entre as bordas dos dois beats que se olham, com seta no destino.
static QPainterPath flowPath(QRectF a, QRectF b, QPointF* tipOut = nullptr, QPointF* dirOut = nullptr) {
    const QPointF ca = a.center(), cb = b.center();
    const double dx = cb.x() - ca.x(), dy = cb.y() - ca.y();
    QPointF p1, p2, d1, d2;
    if (std::abs(dx) * a.height() >= std::abs(dy) * a.width()) {
        p1 = dx >= 0 ? QPointF(a.right(), ca.y()) : QPointF(a.left(), ca.y());
        p2 = dx >= 0 ? QPointF(b.left(), cb.y()) : QPointF(b.right(), cb.y());
        d1 = QPointF(dx >= 0 ? 1 : -1, 0);
    } else {
        p1 = dy >= 0 ? QPointF(ca.x(), a.bottom()) : QPointF(ca.x(), a.top());
        p2 = dy >= 0 ? QPointF(cb.x(), b.top()) : QPointF(cb.x(), b.bottom());
        d1 = QPointF(0, dy >= 0 ? 1 : -1);
    }
    d2 = -d1;
    const double k = std::max(40.0, QLineF(p1, p2).length() * 0.4);
    QPainterPath path(p1);
    const QPointF tip = p2 + (-d2) * -10;  // a seta termina encostada na borda
    path.cubicTo(p1 + d1 * k, p2 + d2 * k, tip);
    if (tipOut) *tipOut = p2;
    if (dirOut) *dirOut = -d2;
    return path;
}

class FlowItem : public QGraphicsPathItem {
public:
    FlowItem(BeatItem* from, BeatItem* to) : from_(from), to_(to) {
        setFlags(ItemIsSelectable);
        setZValue(-1);
        setAcceptHoverEvents(true);
        refresh();
    }
    FlowLine flow() const { return {from_->beat().id, to_->beat().id}; }
    void refresh() {
        const QRectF a(from_->pos(), from_->size()), b(to_->pos(), to_->size());
        setPath(flowPath(a, b, &tip_, &dir_));
    }
    QPainterPath shape() const override {
        QPainterPathStroker s;
        s.setWidth(12);
        return s.createStroke(path());
    }
    QRectF boundingRect() const override { return QGraphicsPathItem::boundingRect().adjusted(-12, -12, 12, 12); }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override {
        const ThemeColors& c = ThemeManager::instance().colors();
        QColor color = isSelected() || hover_ ? c.accent : c.muted;
        if (!isSelected() && !hover_) color.setAlphaF(0.85);
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(QPen(color, isSelected() ? 3 : 2, Qt::SolidLine, Qt::RoundCap));
        p->setBrush(Qt::NoBrush);
        p->drawPath(path());
        // Seta.
        const QPointF n(-dir_.y(), dir_.x());
        QPolygonF head;
        head << tip_ << tip_ - dir_ * 12 + n * 6 << tip_ - dir_ * 12 - n * 6;
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawPolygon(head);
    }

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override { hover_ = true; update(); }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override { hover_ = false; update(); }

private:
    BeatItem* from_;
    BeatItem* to_;
    QPointF tip_, dir_;
    bool hover_ = false;
};

// ------------------------------------------------------------ quadro

QStringList BeatBoardView::colors() {
    return {"#f2b13b", "#f0855a", "#e5484d", "#d55fd6", "#6e7bf2", "#3b9cf6", "#36b39b", "#8fbf4a", "#9aa0ab"};
}

BeatBoardView::BeatBoardView(Editor* editor, QWidget* parent) : QGraphicsView(parent), ed_(editor) {
    setScene(new QGraphicsScene(this));
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::RubberBandDrag);
    setRubberBandSelectionMode(Qt::IntersectsItemShape);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setFrameShape(QFrame::NoFrame);
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);

    auto makeButton = [this](const QString& text, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setText(text);
        b->setToolTip(tip);
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        b->setFocusPolicy(Qt::NoFocus);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };
    addButton_ = makeButton(tr("New Beat"), tr("Add a beat in the middle of the board (or double-click the board)"));
    addButton_->setObjectName("primario");
    fitButton_ = makeButton(tr("Fit"), tr("Zoom to show every beat"));
    connect(addButton_, &QToolButton::clicked, this, &BeatBoardView::newBeat);
    connect(fitButton_, &QToolButton::clicked, this, &BeatBoardView::zoomToFit);
    auto refreshIcons = [this] {
        ThemeManager& tm = ThemeManager::instance();
        addButton_->setIcon(tm.icon("mais", tm.colors().accentText));
        fitButton_->setIcon(tm.icon("enquadrar"));
        viewport()->update();
        for (BeatItem* b : beats_) b->update();
        for (FlowItem* f : flows_) f->update();
    };
    connect(&ThemeManager::instance(), &ThemeManager::changed, this, refreshIcons);
    refreshIcons();
    updateSceneRect();
}

void BeatBoardView::reload() {
    QList<int> keep = selectedBeats();
    QList<FlowLine> keepFlows;
    for (FlowItem* f : flows_)
        if (f->isSelected()) keepFlows << f->flow();
    scene()->clear();
    beats_.clear();
    flows_.clear();
    linkPreview_ = nullptr;
    resizing_ = nullptr;
    linkFrom_ = nullptr;
    const BeatBoard& bb = ed_->doc().board;
    for (const Beat& b : bb.beats) {
        auto* item = new BeatItem(this, b);
        scene()->addItem(item);
        beats_.insert(b.id, item);
        item->setSelected(keep.contains(b.id));
    }
    for (const FlowLine& f : bb.flows) {
        BeatItem* a = beats_.value(f.from);
        BeatItem* b = beats_.value(f.to);
        if (!a || !b) continue;
        auto* item = new FlowItem(a, b);
        scene()->addItem(item);
        flows_ << item;
        item->setSelected(keepFlows.contains(f));
    }
    updateSceneRect();
    viewport()->update();
}

QList<int> BeatBoardView::selectedBeats() const {
    QList<int> out;
    for (auto it = beats_.begin(); it != beats_.end(); ++it)
        if (it.value()->isSelected()) out << it.key();
    std::sort(out.begin(), out.end());
    return out;
}

void BeatBoardView::selectBeat(int id) {
    scene()->clearSelection();
    if (BeatItem* b = beats_.value(id)) {
        b->setSelected(true);
        ensureVisible(b, 40, 40);
    }
}

QPoint BeatBoardView::beatCenter(int id) const {
    BeatItem* b = beats_.value(id);
    return b ? mapFromScene(b->mapToScene(b->body().center())) : QPoint();
}

QPoint BeatBoardView::connectorPoint(int id) const {
    BeatItem* b = beats_.value(id);
    return b ? mapFromScene(b->mapToScene(b->connectorCenter())) : QPoint();
}

QPoint BeatBoardView::resizePoint(int id) const {
    BeatItem* b = beats_.value(id);
    return b ? mapFromScene(b->mapToScene(b->body().bottomRight() - QPointF(6, 6))) : QPoint();
}

void BeatBoardView::flowsChanged() {
    for (FlowItem* f : flows_) f->refresh();
}

void BeatBoardView::updateSceneRect() {
    QRectF r = scene()->itemsBoundingRect().adjusted(-1600, -1200, 1600, 1200);
    r = r.united(QRectF(-2000, -1500, 4000, 3000));
    scene()->setSceneRect(r);
}

void BeatBoardView::placeButtons() {
    const int h = 32;
    const int y = static_cast<int>((kHeader - h) / 2);
    const int addW = addButton_->sizeHint().width() + 8;
    const int fitW = fitButton_->sizeHint().width() + 8;
    addButton_->setGeometry(width() - 24 - addW, y, addW, h);
    fitButton_->setGeometry(width() - 24 - addW - 8 - fitW, y, fitW, h);
}

void BeatBoardView::resizeEvent(QResizeEvent* e) {
    QGraphicsView::resizeEvent(e);
    placeButtons();
}

void BeatBoardView::drawBackground(QPainter* p, const QRectF& rect) {
    const ThemeColors& c = ThemeManager::instance().colors();
    p->fillRect(rect, c.canvas);
    if (zoom_ < 0.45) return;
    QColor dot = c.dark ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 30);
    p->setPen(QPen(dot, 2.0 / zoom_, Qt::SolidLine, Qt::RoundCap));
    const double left = std::floor(rect.left() / kGrid) * kGrid;
    const double top = std::floor(rect.top() / kGrid) * kGrid;
    QVector<QPointF> pts;
    for (double x = left; x < rect.right(); x += kGrid)
        for (double y = top; y < rect.bottom(); y += kGrid) pts << QPointF(x, y);
    p->drawPoints(pts.constData(), static_cast<int>(pts.size()));
}

void BeatBoardView::paintEvent(QPaintEvent* e) {
    QGraphicsView::paintEvent(e);
    const ThemeColors& c = ThemeManager::instance().colors();
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing);

    // Cabeçalho por cima do quadro, como na tela de cartões.
    QLinearGradient fade(0, 0, 0, kHeader + 14);
    QColor solid = c.canvas;
    QColor clear = c.canvas;
    clear.setAlpha(0);
    fade.setColorAt(0, solid);
    fade.setColorAt(kHeader / (kHeader + 14), solid);
    fade.setColorAt(1, clear);
    p.fillRect(QRectF(0, 0, viewport()->width(), kHeader + 14), fade);
    QFont hf = font();
    hf.setPixelSize(20);
    hf.setWeight(QFont::Bold);
    p.setFont(hf);
    p.setPen(c.text);
    const QFontMetricsF hfm(hf);
    const double baseline = (kHeader - hfm.height()) / 2 + hfm.ascent();
    p.drawText(QPointF(24, baseline), tr("Beat Board"));
    QFont sf = font();
    sf.setPixelSize(12);
    p.setFont(sf);
    p.setPen(c.muted);
    const QString info = tr("%n beat(s)", "", static_cast<int>(beats_.size())) + "  ·  " +
                         tr("%n flow line(s)", "", static_cast<int>(flows_.size())) + "  ·  " +
                         tr("double-click to add, drag the + to connect, Ctrl+wheel to zoom");
    const double x = 24 + hfm.horizontalAdvance(tr("Beat Board")) + 16;
    const double right = fitButton_->geometry().left() - 16;
    p.drawText(QRectF(x, 0, std::max(0.0, right - x), kHeader), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetricsF(sf).elidedText(info, Qt::ElideRight, std::max(0.0, right - x)));

    if (!beats_.isEmpty()) return;
    // Quadro vazio: convite no meio.
    QFont f = font();
    f.setPointSizeF(font().pointSizeF() * 1.05);
    p.setFont(f);
    p.setPen(c.muted);
    p.drawText(viewport()->rect().adjusted(40, 40 + static_cast<int>(kHeader), -40, -40),
               Qt::AlignCenter | Qt::TextWordWrap,
               tr("Your beat board is empty.\nDouble-click anywhere to add a beat, drop an image here, "
                  "or paste one with Ctrl+V.\nDrag the + on the edge of a beat onto another to draw a flow line."));
}

BeatItem* BeatBoardView::beatAt(QPoint viewPos) const {
    for (QGraphicsItem* it : items(viewPos))
        if (auto* b = dynamic_cast<BeatItem*>(it)) return b;
    return nullptr;
}

FlowItem* BeatBoardView::flowAt(QPoint viewPos) const {
    for (QGraphicsItem* it : items(viewPos))
        if (auto* f = dynamic_cast<FlowItem*>(it)) return f;
    return nullptr;
}

void BeatBoardView::mousePressEvent(QMouseEvent* e) {
    setFocus();
    const QPoint vp = e->position().toPoint();
    if (e->button() == Qt::MiddleButton || (e->button() == Qt::LeftButton && spaceDown_)) {
        panning_ = true;
        panLast_ = vp;
        viewport()->setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (e->button() == Qt::LeftButton) {
        if (BeatItem* b = beatAt(vp)) {
            const QPointF local = b->mapFromScene(mapToScene(vp));
            if (b->onConnector(local)) {
                linkFrom_ = b;
                linkPreview_ = scene()->addPath(QPainterPath(), QPen(ThemeManager::instance().colors().accent, 2,
                                                                     Qt::DashLine, Qt::RoundCap));
                linkPreview_->setZValue(10);
                return;
            }
            if (b->onGrip(local)) {
                resizing_ = b;
                resizeStart_ = b->size();
                pressScene_ = mapToScene(vp);
                if (!b->isSelected()) {
                    scene()->clearSelection();
                    b->setSelected(true);
                }
                return;
            }
        }
    }
    moved_ = false;
    QGraphicsView::mousePressEvent(e);
}

void BeatBoardView::mouseMoveEvent(QMouseEvent* e) {
    const QPoint vp = e->position().toPoint();
    if (panning_) {
        const QPoint d = vp - panLast_;
        panLast_ = vp;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        return;
    }
    if (linkFrom_ && linkPreview_) {
        const QPointF start = linkFrom_->mapToScene(linkFrom_->connectorCenter());
        const QPointF end = mapToScene(vp);
        QPainterPath path(start);
        const double k = std::max(40.0, QLineF(start, end).length() * 0.4);
        path.cubicTo(start + QPointF(k, 0), end - QPointF(k, 0), end);
        linkPreview_->setPath(path);
        return;
    }
    if (resizing_) {
        const QPointF d = mapToScene(vp) - pressScene_;
        resizing_->setSize(QSizeF(resizeStart_.width() + d.x(), resizeStart_.height() + d.y()));
        return;
    }
    if (e->buttons() & Qt::LeftButton) moved_ = true;
    QGraphicsView::mouseMoveEvent(e);
}

void BeatBoardView::mouseReleaseEvent(QMouseEvent* e) {
    const QPoint vp = e->position().toPoint();
    if (panning_) {
        panning_ = false;
        viewport()->setCursor(spaceDown_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    if (linkFrom_) {
        BeatItem* target = beatAt(vp);
        const int from = linkFrom_->beat().id;
        delete linkPreview_;
        linkPreview_ = nullptr;
        linkFrom_ = nullptr;
        if (target && target->beat().id != from && ed_->addFlow(from, target->beat().id)) emit changed();
        return;
    }
    if (resizing_) {
        resizing_ = nullptr;
        commitGeometry();
        return;
    }
    QGraphicsView::mouseReleaseEvent(e);
    if (moved_) {
        moved_ = false;
        commitGeometry();
    }
}

// Grava no editor a posição e o tamanho dos beats que mudaram (um passo de desfazer).
void BeatBoardView::commitGeometry() {
    std::vector<Beat> changedBeats;
    for (BeatItem* b : beats_) {
        const Beat now = b->current();
        const Beat& was = b->beat();
        if (now.x != was.x || now.y != was.y || now.w != was.w || now.h != was.h) changedBeats.push_back(now);
    }
    if (changedBeats.empty()) return;
    ed_->updateBeats(changedBeats);
    emit changed();
}

void BeatBoardView::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const QPoint vp = e->position().toPoint();
    if (BeatItem* b = beatAt(vp)) {
        editBeat(b->beat().id);
        return;
    }
    if (flowAt(vp)) return;
    const QPointF at = mapToScene(vp) - QPointF(110, 40);
    const int id = createBeatAt(at);
    editBeat(id);
}

int BeatBoardView::createBeatAt(QPointF scenePos, const QString& text, const std::string& image) {
    Beat b;
    b.x = std::round(scenePos.x() / kGrid) * kGrid;
    b.y = std::round(scenePos.y() / kGrid) * kGrid;
    // Cor seguinte da paleta, para os beats novos não ficarem todos iguais.
    const QStringList palette = colors();
    b.color = palette.at(static_cast<int>(ed_->doc().board.beats.size()) % palette.size()).toStdString();
    b.text = text.toStdU32String();
    if (!image.empty()) {
        b.image = std::make_shared<const std::string>(image);
        b.h = 220;
    }
    const int id = ed_->addBeat(b);
    emit changed();
    selectBeat(id);
    return id;
}

void BeatBoardView::newBeat() {
    const QPointF center = mapToScene(viewport()->rect().center());
    // Não empilha exatamente em cima de outro beat.
    QPointF at = center - QPointF(110, 75);
    for (int tries = 0; tries < 20; ++tries) {
        bool clash = false;
        for (const Beat& b : ed_->doc().board.beats)
            if (std::abs(b.x - at.x()) < 12 && std::abs(b.y - at.y()) < 12) clash = true;
        if (!clash) break;
        at += QPointF(kGrid, kGrid);
    }
    editBeat(createBeatAt(at));
}

void BeatBoardView::editBeat(int id) {
    const Beat* b = ed_->doc().board.beat(id);
    if (!b) return;
    BeatItem* item = beats_.value(id);
    Beat base = item ? item->current() : *b;
    BeatDialog dlg(base, this);
    if (dlg.exec() != QDialog::Accepted) return;
    ed_->updateBeats({dlg.beat()});
    emit changed();
    selectBeat(id);
}

void BeatBoardView::deleteSelection() {
    std::vector<int> ids;
    for (int id : selectedBeats()) ids.push_back(id);
    std::vector<FlowLine> flows;
    for (FlowItem* f : flows_)
        if (f->isSelected()) flows.push_back(f->flow());
    if (ids.empty() && flows.empty()) return;
    if (!flows.empty()) ed_->removeFlows(flows);
    if (!ids.empty()) ed_->removeBeats(ids);
    emit changed();
}

void BeatBoardView::setZoom(double z) {
    z = std::clamp(z, 0.25, 2.5);
    const double f = z / zoom_;
    zoom_ = z;
    scale(f, f);
}

void BeatBoardView::zoomIn() { setZoom(zoom_ * 1.2); }
void BeatBoardView::zoomOut() { setZoom(zoom_ / 1.2); }

void BeatBoardView::zoomToFit() {
    if (beats_.isEmpty()) {
        resetTransform();
        zoom_ = 1.0;
        centerOn(0, 0);
        return;
    }
    QRectF r;
    for (BeatItem* b : beats_) r = r.united(QRectF(b->pos(), b->size()));
    r.adjust(-60, -60, 60, 60);
    resetTransform();
    zoom_ = 1.0;
    const double z = std::min({viewport()->width() / r.width(), viewport()->height() / r.height(), 1.0});
    setZoom(z);
    centerOn(r.center());
}

void BeatBoardView::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        setZoom(zoom_ * (e->angleDelta().y() > 0 ? 1.12 : 1 / 1.12));
        e->accept();
        return;
    }
    QGraphicsView::wheelEvent(e);
}

void BeatBoardView::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        spaceDown_ = true;
        viewport()->setCursor(Qt::OpenHandCursor);
        return;
    }
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        deleteSelection();
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_F2) {
        const QList<int> sel = selectedBeats();
        if (sel.size() == 1) editBeat(sel.first());
        return;
    }
    if (e->matches(QKeySequence::Paste)) {
        pasteFromClipboard();
        return;
    }
    if (e->matches(QKeySequence::SelectAll)) {
        selectAllBeats();
        return;
    }
    if (e->key() == Qt::Key_Escape) {
        scene()->clearSelection();
        return;
    }
    QGraphicsView::keyPressEvent(e);
}

void BeatBoardView::keyReleaseEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        spaceDown_ = false;
        viewport()->unsetCursor();
        return;
    }
    QGraphicsView::keyReleaseEvent(e);
}

void BeatBoardView::pasteFromClipboard() { paste(mapToScene(viewport()->rect().center()) - QPointF(110, 75)); }

void BeatBoardView::selectAllBeats() {
    for (BeatItem* b : beats_) b->setSelected(true);
}

void BeatBoardView::paste(QPointF scenePos) {
    const QMimeData* md = QApplication::clipboard()->mimeData();
    if (!md) return;
    if (md->hasImage()) {
        const std::string data = beatimage::fromImage(qvariant_cast<QImage>(md->imageData()));
        if (!data.empty()) createBeatAt(scenePos, {}, data);
        return;
    }
    if (md->hasUrls()) {
        for (const QUrl& u : md->urls()) {
            QString err;
            const std::string data = u.isLocalFile() ? beatimage::fromFile(u.toLocalFile(), &err) : std::string();
            if (!data.empty()) {
                createBeatAt(scenePos, {}, data);
                scenePos += QPointF(kGrid, kGrid);
            }
        }
        return;
    }
    if (md->hasText() && !md->text().trimmed().isEmpty()) createBeatAt(scenePos, md->text().trimmed());
}

void BeatBoardView::chooseImage(int id) {
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
    BeatItem* item = beats_.value(id);
    if (!item) return;
    Beat b = item->current();
    b.image = std::make_shared<const std::string>(data);
    if (b.h < 200) b.h = 220;
    ed_->updateBeats({b});
    emit changed();
}

void BeatBoardView::setBeatColor(const QList<int>& ids, const QString& color) {
    std::vector<Beat> changedBeats;
    for (int id : ids)
        if (BeatItem* item = beats_.value(id)) {
            Beat b = item->current();
            b.color = color.toStdString();
            changedBeats.push_back(b);
        }
    ed_->updateBeats(changedBeats);
    emit changed();
}

void BeatBoardView::contextMenuEvent(QContextMenuEvent* e) {
    const QPoint vp = e->pos();
    const QPointF at = mapToScene(vp);
    QMenu menu(this);
    if (BeatItem* b = beatAt(vp)) {
        if (!b->isSelected()) {
            scene()->clearSelection();
            b->setSelected(true);
        }
        const int id = b->beat().id;
        const QList<int> sel = selectedBeats();
        menu.addAction(tr("Edit Beat...") + "\tEnter", this, [this, id] { editBeat(id); });
        QMenu* colorMenu = menu.addMenu(tr("Color"));
        for (const QString& c : colors()) {
            QPixmap swatch(14, 14);
            swatch.fill(Qt::transparent);
            QPainter sp(&swatch);
            sp.setRenderHint(QPainter::Antialiasing);
            sp.setBrush(QColor(c));
            sp.setPen(Qt::NoPen);
            sp.drawEllipse(1, 1, 12, 12);
            sp.end();
            QAction* a = colorMenu->addAction(QIcon(swatch), QString(), this, [this, sel, c] { setBeatColor(sel, c); });
            a->setIconVisibleInMenu(true);
            a->setCheckable(true);
            a->setChecked(QString::fromStdString(b->beat().color) == c);
        }
        const bool hasImage = static_cast<bool>(b->beat().image);
        menu.addAction(ThemeManager::instance().icon("imagem"), hasImage ? tr("Change Image...") : tr("Add Image..."),
                       this, [this, id] { chooseImage(id); });
        if (hasImage)
            menu.addAction(tr("Remove Image"), this, [this, id] {
                if (BeatItem* item = beats_.value(id)) {
                    Beat nb = item->current();
                    nb.image.reset();
                    ed_->updateBeats({nb});
                    emit changed();
                }
            });
        menu.addSeparator();
        menu.addAction(sel.size() > 1 ? tr("Delete %n Beat(s)", "", static_cast<int>(sel.size())) : tr("Delete Beat"),
                       this, &BeatBoardView::deleteSelection)
            ->setShortcut(QKeySequence::Delete);
    } else if (FlowItem* f = flowAt(vp)) {
        const FlowLine flow = f->flow();
        menu.addAction(tr("Delete Flow Line"), this, [this, flow] {
            ed_->removeFlows({flow});
            emit changed();
        });
    } else {
        menu.addAction(tr("New Beat Here"), this, [this, at] { editBeat(createBeatAt(at - QPointF(110, 40))); });
        const QMimeData* md = QApplication::clipboard()->mimeData();
        menu.addAction(tr("Paste"), this, [this, at] { paste(at - QPointF(110, 75)); })
            ->setEnabled(md && (md->hasImage() || md->hasUrls() || md->hasText()));
        menu.addSeparator();
        menu.addAction(tr("Fit All Beats"), this, &BeatBoardView::zoomToFit);
    }
    menu.exec(e->globalPos());
}

void BeatBoardView::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasImage() || e->mimeData()->hasUrls()) e->acceptProposedAction();
    else QGraphicsView::dragEnterEvent(e);
}

void BeatBoardView::dragMoveEvent(QDragMoveEvent* e) {
    if (e->mimeData()->hasImage() || e->mimeData()->hasUrls()) e->acceptProposedAction();
    else QGraphicsView::dragMoveEvent(e);
}

void BeatBoardView::dropEvent(QDropEvent* e) {
    QPointF at = mapToScene(e->position().toPoint()) - QPointF(110, 110);
    const QMimeData* md = e->mimeData();
    if (md->hasImage()) {
        const std::string data = beatimage::fromImage(qvariant_cast<QImage>(md->imageData()));
        if (!data.empty()) createBeatAt(at, {}, data);
    } else {
        for (const QUrl& u : md->urls()) {
            QString err;
            if (!u.isLocalFile()) continue;
            const std::string data = beatimage::fromFile(u.toLocalFile(), &err);
            if (data.empty()) continue;
            createBeatAt(at, QFileInfo(u.toLocalFile()).completeBaseName(), data);
            at += QPointF(kGrid * 2, kGrid * 2);
        }
    }
    e->acceptProposedAction();
}
