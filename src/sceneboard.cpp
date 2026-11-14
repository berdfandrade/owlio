#include "sceneboard.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QSettings>
#include <QTextLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "avatars.h"
#include "sceneheading.h"
#include "theme.h"

QString eighthsText(int eighths) {
    const int whole = eighths / 8, rest = eighths % 8;
    if (whole == 0) return QString("%1/8").arg(rest);
    if (rest == 0) return QString::number(whole);
    return QString("%1 %2/8").arg(whole).arg(rest);
}

namespace {

QString q(const std::u32string& s) { return QString::fromStdU32String(s); }

// Texto com quebra de linha, limitado a `maxLines`; a última linha recebe reticências.
double drawWrapped(QPainter& p, const QRectF& r, const QString& text, const QFont& f, const QColor& c,
                   int maxLines) {
    QFontMetricsF fm(f);
    maxLines = std::min(maxLines, static_cast<int>(r.height() / fm.lineSpacing()));
    if (maxLines <= 0 || text.isEmpty()) return 0;
    QTextLayout tl(text, f);
    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    tl.setTextOption(opt);
    tl.beginLayout();
    double y = 0;
    while (true) {
        QTextLine line = tl.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(r.width());
        line.setPosition(QPointF(0, y));
        y += fm.lineSpacing();
    }
    tl.endLayout();
    p.setPen(c);
    p.setFont(f);
    const int n = tl.lineCount();
    const int shown = std::min(n, maxLines);
    for (int i = 0; i < shown; ++i) {
        QTextLine line = tl.lineAt(i);
        if (i == shown - 1 && n > maxLines) {
            QString rest = text.mid(line.textStart()).simplified();
            p.drawText(QPointF(r.left(), r.top() + line.y() + fm.ascent()),
                       fm.elidedText(rest, Qt::ElideRight, r.width()));
        } else {
            line.draw(&p, r.topLeft());
        }
    }
    return shown * fm.lineSpacing();
}

void softShadow(QPainter& p, const QRectF& r, double radius, double strength, int layers, double dy) {
    p.setPen(Qt::NoPen);
    for (int i = layers; i >= 1; --i) {
        const double a = strength * (1.0 - double(i) / (layers + 1));
        p.setBrush(QColor(0, 0, 0, static_cast<int>(a * 255)));
        p.drawRoundedRect(r.adjusted(-i, -i + dy, i, i + dy), radius + i, radius + i);
    }
}

}  // namespace

SceneBoard::SceneBoard(Editor* editor, QWidget* parent) : QAbstractScrollArea(parent), ed_(editor) {
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    viewport()->setMouseTracking(true);
    horizontalScrollBar()->setRange(0, 0);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(&ThemeManager::instance(), &ThemeManager::changed, this, [this] { viewport()->update(); });

    list_ = QSettings().value("cardsAsList", false).toBool();

    autoScroll_.setInterval(25);
    connect(&autoScroll_, &QTimer::timeout, this, [this] {
        if (!dragging_) { autoScroll_.stop(); return; }
        const double edge = 60, h = viewport()->height();
        int dy = 0;
        if (dragPos_.y() < edge) dy = -static_cast<int>((edge - dragPos_.y()) / 3 + 2);
        else if (dragPos_.y() > h - edge) dy = static_cast<int>((dragPos_.y() - (h - edge)) / 3 + 2);
        if (dy == 0) return;
        verticalScrollBar()->setValue(verticalScrollBar()->value() + dy);
        dropIndex_ = dropIndexAt(dragPos_, &dropBar_);
        viewport()->update();
    });
}

void SceneBoard::reload() {
    cards_ = ed_->sceneSummaries();
    pages_ = ed_->layout().pageCount;
    if (selected_ >= static_cast<int>(cards_.size())) selected_ = static_cast<int>(cards_.size()) - 1;
    hover_ = -1;
    relayout();
    viewport()->update();
}

void SceneBoard::setListMode(bool on) {
    if (on == list_) return;
    list_ = on;
    QSettings().setValue("cardsAsList", on);
    relayout();
    if (selected_ >= 0) selectScene(selected_);
    viewport()->update();
}

QRectF SceneBoard::toggleRect(bool listButton) const {
    const double w = 34, h = 30;
    const double x = viewport()->width() - kMargin - 2 * w + (listButton ? w : 0);
    return QRectF(x, 24 - verticalScrollBar()->value(), w, h);
}

void SceneBoard::relayout() {
    const double avail = std::max(200.0, viewport()->width() - 2 * kMargin);
    if (list_) {
        cols_ = 1;
        cardW_ = avail;
        cardH_ = 100;
    } else {
        cols_ = std::max(1, static_cast<int>((avail + gap()) / (minCardW_ + gap())));
        cardW_ = (avail - (cols_ - 1) * gap()) / cols_;
        cardH_ = std::clamp(cardW_ * 0.74, 196.0, 280.0);
    }
    const int rows = (static_cast<int>(cards_.size()) + cols_ - 1) / cols_;
    const double contentH = kHeader + rows * (cardH_ + gap()) + kMargin;
    verticalScrollBar()->setRange(0, std::max(0, static_cast<int>(contentH - viewport()->height())));
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setSingleStep(40);
}

QRectF SceneBoard::cardRect(int i) const {
    const int row = i / cols_, col = i % cols_;
    return QRectF(kMargin + col * (cardW_ + gap()), kHeader + row * (cardH_ + gap()) - verticalScrollBar()->value(),
                  cardW_, cardH_);
}

int SceneBoard::cardAt(QPointF pt) const {
    for (int i = 0; i < static_cast<int>(cards_.size()); ++i)
        if (cardRect(i).contains(pt)) return i;
    return -1;
}

int SceneBoard::dropIndexAt(QPointF pt, QRectF* bar) const {
    const int n = static_cast<int>(cards_.size());
    if (n == 0) return -1;
    const double y = pt.y() + verticalScrollBar()->value() - kHeader;
    const int rows = (n + cols_ - 1) / cols_;
    int row = std::clamp(static_cast<int>(std::floor(y / (cardH_ + gap()))), 0, rows - 1);
    const double x = pt.x() - kMargin;
    int col = std::clamp(static_cast<int>(std::floor((x + gap() / 2) / (cardW_ + gap()))), 0, cols_ - 1);
    int idx = std::min(row * cols_ + col, n - 1);
    const QRectF r = cardRect(idx);
    if (list_) {
        // Lista: a barra de destino é horizontal, entre duas faixas.
        const bool below = pt.y() > r.center().y();
        if (bar) {
            const double by = below ? r.bottom() + gap() / 2 : r.top() - gap() / 2;
            *bar = QRectF(r.left() + 6, by - 2, r.width() - 12, 4);
        }
        return below ? idx + 1 : idx;
    }
    const bool after = pt.x() > r.center().x() || (idx == n - 1 && pt.y() > r.bottom());
    const int drop = after ? idx + 1 : idx;
    if (bar) {
        const double bx = after ? r.right() + gap() / 2 : r.left() - gap() / 2;
        *bar = QRectF(bx - 2, r.top() + 6, 4, r.height() - 12);
    }
    return drop;
}

void SceneBoard::selectScene(int index, bool scrollTo) {
    if (cards_.empty()) { selected_ = -1; viewport()->update(); return; }
    index = std::clamp(index, 0, static_cast<int>(cards_.size()) - 1);
    const bool changed = index != selected_;
    selected_ = index;
    if (scrollTo) {
        QRectF r = cardRect(index);
        QScrollBar* vb = verticalScrollBar();
        if (r.top() < gap()) vb->setValue(vb->value() + static_cast<int>(r.top() - (index < cols_ ? kHeader : gap())));
        else if (r.bottom() > viewport()->height() - gap())
            vb->setValue(vb->value() + static_cast<int>(r.bottom() - viewport()->height() + gap()));
    }
    viewport()->update();
    if (changed) emit selectionChanged(selected_);
}

void SceneBoard::requestMove(int from, int to) {
    to = std::clamp(to, 0, static_cast<int>(cards_.size()));
    if (from < 0 || to == from || to == from + 1) return;
    emit moveRequested(from, to);
}

// ------------------------------------------------------------------ pintura

void SceneBoard::paintCard(QPainter& p, const QRectF& r, int index, bool selected, bool hover) const {
    const ThemeColors& tc = ThemeManager::instance().colors();
    const SceneSummary& s = cards_[index];
    const HeadingParts h = splitHeading(q(s.heading));
    const QColor stripe = timeColor(h.time);

    softShadow(p, r, 10, (tc.dark ? 0.10 : 0.045) * (hover ? 1.6 : 1.0), hover ? 9 : 6, hover ? 4 : 2);

    QPainterPath path;
    path.addRoundedRect(r, 10, 10);
    p.setPen(Qt::NoPen);
    p.setBrush(tc.dark ? tc.raised : QColor(Qt::white));
    p.drawPath(path);

    // Faixa colorida no topo (na lista, à esquerda).
    p.save();
    p.setClipPath(path);
    p.fillRect(list_ ? QRectF(r.left(), r.top(), 5, r.height()) : QRectF(r.left(), r.top(), r.width(), 5), stripe);
    p.restore();

    if (list_) {
        paintRow(p, r, index);
    } else {

    const double pad = 16;
    const QRectF in = r.adjusted(pad, pad + 4, -pad, -pad + 2);
    QFont base = font();
    QFont small = base;
    small.setPointSizeF(base.pointSizeF() * 0.86);
    QFont smallBold = small;
    smallBold.setWeight(QFont::Bold);
    QFont title = base;
    title.setPointSizeF(base.pointSizeF() * 1.08);
    title.setWeight(QFont::DemiBold);
    QFontMetricsF sfm(small), tfm(title);

    // Linha 1: número, INT/EXT, página.
    double y = in.top();
    const QString num = QString::number(index + 1);
    const double bw = std::max(26.0, sfm.horizontalAdvance(num) + 14);
    const QRectF badge(in.left(), y, bw, sfm.height() + 6);
    QColor tint = tc.accent;
    tint.setAlphaF(tc.dark ? 0.22 : 0.12);
    p.setBrush(tint);
    p.drawRoundedRect(badge, 6, 6);
    p.setFont(smallBold);
    p.setPen(tc.accent);
    p.drawText(badge, Qt::AlignCenter, num);

    double x = badge.right() + 8;
    if (!h.prefix.isEmpty()) {
        const QString pre = h.prefix.toUpper().remove('.');
        const QRectF chip(x, y, QFontMetricsF(smallBold).horizontalAdvance(pre) + 14, badge.height());
        p.setPen(QPen(tc.border, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(chip.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
        p.setPen(tc.muted);
        p.drawText(chip, Qt::AlignCenter, pre);
    }
    p.setFont(small);
    p.setPen(tc.muted);
    p.drawText(QRectF(in.left(), y, in.width(), badge.height()), Qt::AlignRight | Qt::AlignVCenter,
               tr("p. %1").arg(s.page + 1));
    y = badge.bottom() + 10;

    // Local (até 2 linhas) e horário.
    const QString loc = h.location.isEmpty() ? (s.heading.empty() ? tr("(scene without a heading)") : q(s.heading))
                                             : h.location;
    y += drawWrapped(p, QRectF(in.left(), y, in.width(), tfm.lineSpacing() * 2), loc, title, tc.text, 2);
    if (!h.time.isEmpty()) {
        const double dot = 7;
        p.setPen(Qt::NoPen);
        p.setBrush(stripe);
        p.drawEllipse(QRectF(in.left(), y + (sfm.height() - dot) / 2 + 1, dot, dot));
        p.setFont(smallBold);
        p.setPen(tc.muted);
        p.drawText(QPointF(in.left() + dot + 6, y + sfm.ascent() + 1), h.time.toUpper());
        y += sfm.height() + 2;
    }
    y += 8;

    // Rodapé: personagens e duração.
    const double footH = 24;
    const QRectF foot(in.left(), in.bottom() - footH, in.width(), footH);
    p.setPen(QPen(tc.border, 1));
    p.drawLine(QPointF(in.left(), foot.top() - 8), QPointF(in.right(), foot.top() - 8));
    const QString len = eighthsText(s.eighths) + " " + tr("pg.");
    p.setFont(small);
    p.setPen(tc.muted);
    p.drawText(foot, Qt::AlignRight | Qt::AlignVCenter, len);
    double ax = foot.left();
    const double d = 22;
    const int maxAvatars = std::max(1, static_cast<int>((foot.width() - sfm.horizontalAdvance(len) - 40) / (d - 5)));
    for (int k = 0; k < static_cast<int>(s.cast.size()) && k < maxAvatars; ++k) {
        const QString name = q(s.cast[k]);
        const QRectF av(ax, foot.center().y() - d / 2, d, d);
        avatars::draw(p, av, name, ed_->doc().avatar(s.cast[k]), 2, tc.dark ? tc.raised : QColor(Qt::white));
        ax += d - 5;
    }
    if (static_cast<int>(s.cast.size()) > maxAvatars) {
        p.setFont(small);
        p.setPen(tc.muted);
        p.drawText(QPointF(ax + 8, foot.center().y() + sfm.ascent() / 2 - 1),
                   QString("+%1").arg(s.cast.size() - maxAvatars));
    } else if (s.cast.size() == 1) {
        p.setFont(small);
        p.setPen(tc.muted);
        p.drawText(QRectF(ax + 6, foot.top(), foot.width() / 2, foot.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   q(s.cast[0]));
    }

    // Corpo: sinopse do autor ou começo da ação.
    const QRectF body(in.left(), y, in.width(), foot.top() - 14 - y);
    if (!s.synopsis.empty()) {
        drawWrapped(p, body, q(s.synopsis), base, tc.text, 6);
    } else if (!s.preview.empty()) {
        QFont it = base;
        it.setItalic(true);
        drawWrapped(p, body, q(s.preview), it, tc.muted, 6);
    } else {
        drawWrapped(p, body, tr("No action yet. Press F2 to write a synopsis."), small, tc.muted, 2);
    }
    }  // grade

    // Seleção.
    if (selected) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(tc.accent, 2));
        p.drawRoundedRect(r.adjusted(-1, -1, 1, 1), 11, 11);
    } else if (!tc.dark) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(tc.border, 1));
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
    }
}

void SceneBoard::paintRow(QPainter& p, const QRectF& r, int index) const {
    const ThemeColors& tc = ThemeManager::instance().colors();
    const SceneSummary& s = cards_[index];
    const HeadingParts h = splitHeading(q(s.heading));
    const QColor stripe = timeColor(h.time);

    QFont base = font();
    QFont small = base;
    small.setPointSizeF(base.pointSizeF() * 0.86);
    QFont smallBold = small;
    smallBold.setWeight(QFont::Bold);
    QFont title = base;
    title.setPointSizeF(base.pointSizeF() * 1.04);
    title.setWeight(QFont::DemiBold);
    QFontMetricsF sfm(small), tfm(title), bfm(base);

    const QRectF in = r.adjusted(22, 0, -18, 0);

    // Esquerda: número da cena, centralizado na altura da faixa.
    const QString num = QString::number(index + 1);
    const double bw = std::max(30.0, sfm.horizontalAdvance(num) + 16);
    const QRectF badge(in.left(), r.center().y() - (sfm.height() + 8) / 2, bw, sfm.height() + 8);
    QColor tint = tc.accent;
    tint.setAlphaF(tc.dark ? 0.22 : 0.12);
    p.setPen(Qt::NoPen);
    p.setBrush(tint);
    p.drawRoundedRect(badge, 6, 6);
    p.setFont(smallBold);
    p.setPen(tc.accent);
    p.drawText(badge, Qt::AlignCenter, num);

    // Direita: personagens, duração e página.
    double right = in.right();
    p.setFont(small);
    p.setPen(tc.muted);
    const QString page = tr("p. %1").arg(s.page + 1);
    const double pageW = std::max(sfm.horizontalAdvance("p. 000"), sfm.horizontalAdvance(page));
    p.drawText(QRectF(right - pageW, r.top(), pageW, r.height()), Qt::AlignRight | Qt::AlignVCenter, page);
    right -= pageW + 18;
    const QString len = eighthsText(s.eighths) + " " + tr("pg.");
    const double lenW = std::max(sfm.horizontalAdvance("0 0/8 pg."), sfm.horizontalAdvance(len));
    p.drawText(QRectF(right - lenW, r.top(), lenW, r.height()), Qt::AlignRight | Qt::AlignVCenter, len);
    right -= lenW + 18;
    const double d = 24;
    const int maxAvatars = 5;
    const int shown = std::min(static_cast<int>(s.cast.size()), maxAvatars);
    const double avW = d + 4 * (d - 6) + sfm.horizontalAdvance("+00") + 6;  // largura reservada, alinha as colunas
    double ax = right - avW;
    for (int k = 0; k < shown; ++k) {
        const QRectF av(ax, r.center().y() - d / 2, d, d);
        avatars::draw(p, av, q(s.cast[k]), ed_->doc().avatar(s.cast[k]), 2, tc.dark ? tc.raised : QColor(Qt::white));
        ax += d - 6;
    }
    if (static_cast<int>(s.cast.size()) > maxAvatars) {
        p.setFont(small);
        p.setPen(tc.muted);
        p.drawText(QPointF(ax + 10, r.center().y() + sfm.ascent() / 2 - 1),
                   QString("+%1").arg(s.cast.size() - maxAvatars));
    }
    right -= avW + 20;

    // Meio: título (INT/EXT, local e horário) e texto, centralizados na faixa.
    double x = badge.right() + 14;
    const double textRight = std::max(x + 60, right);
    // Sinopse ou começo da ação, em até duas linhas abaixo do título.
    QFont bodyFont = base;
    QColor bodyColor = tc.text;
    QString body;
    if (!s.synopsis.empty()) {
        body = q(s.synopsis);
    } else if (!s.preview.empty()) {
        body = q(s.preview);
        bodyFont.setItalic(true);
        bodyColor = tc.muted;
    } else {
        body = tr("No action yet. Press F2 to write a synopsis.");
        bodyFont = small;
        bodyColor = tc.muted;
    }
    body = body.simplified();
    const QFontMetricsF bodyFm(bodyFont);
    const double tx2 = x, bodyW = std::max(20.0, textRight - tx2);
    const int bodyLines = bodyFm.horizontalAdvance(body) > bodyW ? 2 : 1;
    const double line1 = r.top() + (r.height() - tfm.height() - 6 - bodyLines * bodyFm.lineSpacing()) / 2;
    const double y2 = line1 + tfm.height() + 6;

    const double chipH = sfm.height() + 4;
    const double y1 = line1 + (tfm.height() - chipH) / 2;
    if (!h.prefix.isEmpty()) {
        const QString pre = h.prefix.toUpper().remove('.');
        const QRectF chip(x, y1, QFontMetricsF(smallBold).horizontalAdvance(pre) + 12, chipH);
        p.setPen(QPen(tc.border, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(chip.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
        p.setFont(smallBold);
        p.setPen(tc.muted);
        p.drawText(chip, Qt::AlignCenter, pre);
        x = chip.right() + 10;
    }
    const QString loc = h.location.isEmpty() ? (s.heading.empty() ? tr("(scene without a heading)") : q(s.heading))
                                             : h.location;
    const QString time = h.time.toUpper();
    const double timeW = time.isEmpty() ? 0 : 7 + 6 + QFontMetricsF(smallBold).horizontalAdvance(time) + 14;
    const QString locShown = tfm.elidedText(loc, Qt::ElideRight, std::max(20.0, textRight - x - timeW));
    p.setFont(title);
    p.setPen(tc.text);
    p.drawText(QPointF(x, line1 + tfm.ascent()), locShown);
    if (!time.isEmpty()) {
        double tx = x + tfm.horizontalAdvance(locShown) + 14;
        const double dot = 7;
        p.setPen(Qt::NoPen);
        p.setBrush(stripe);
        p.drawEllipse(QRectF(tx, line1 + (tfm.height() - dot) / 2, dot, dot));
        p.setFont(smallBold);
        p.setPen(tc.muted);
        p.drawText(QPointF(tx + dot + 6, line1 + (tfm.height() + sfm.ascent() - sfm.descent()) / 2), time);
    }

    drawWrapped(p, QRectF(tx2, y2, bodyW, 2 * bodyFm.lineSpacing() + 1), body, bodyFont, bodyColor, 2);
}

void SceneBoard::paintEvent(QPaintEvent*) {
    const ThemeColors& tc = ThemeManager::instance().colors();
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.fillRect(viewport()->rect(), tc.canvas);

    // Cabeçalho do quadro.
    const double top = 26 - verticalScrollBar()->value();
    QFont h1 = font();
    h1.setPointSizeF(font().pointSizeF() * 1.55);
    h1.setWeight(QFont::Bold);
    p.setFont(h1);
    p.setPen(tc.text);
    QFontMetricsF hfm(h1);
    p.drawText(QPointF(kMargin, top + hfm.ascent()), tr("Index Cards"));
    QFont sub = font();
    sub.setPointSizeF(font().pointSizeF() * 0.92);
    p.setFont(sub);
    p.setPen(tc.muted);
    const QString info = tr("%n scene(s)", "", static_cast<int>(cards_.size())) + "  ·  " +
                         tr("%n page(s)", "", pages_) + "  ·  " +
                         tr("drag to reorder, double-click to open, F2 for the synopsis");
    const double infoX = kMargin + hfm.horizontalAdvance(tr("Index Cards")) + 16;
    const double infoW = std::max(0.0, toggleRect(false).left() - 16 - infoX);
    p.drawText(QRectF(infoX, top, infoW, hfm.height()), Qt::AlignLeft | Qt::AlignBottom,
               QFontMetricsF(sub).elidedText(info, Qt::ElideRight, infoW));

    // Alternador grade/lista, à direita do cabeçalho.
    {
        const QRectF g = toggleRect(false), l = toggleRect(true);
        const QRectF both = g.united(l);
        p.setPen(QPen(tc.border, 1));
        p.setBrush(tc.dark ? tc.raised : QColor(Qt::white));
        p.drawRoundedRect(both.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        for (int k = 0; k < 2; ++k) {
            const QRectF b = k ? l : g;
            const bool on = (k == 1) == list_;
            if (on || hoverToggle_ == k) {
                QColor fill = on ? tc.accent : tc.border;
                fill.setAlphaF(on ? (tc.dark ? 0.24 : 0.14) : 0.5);
                p.setPen(Qt::NoPen);
                p.setBrush(fill);
                p.drawRoundedRect(b.adjusted(3, 3, -3, -3), 6, 6);
            }
            const QIcon ic = ThemeManager::instance().icon(k ? "lista" : "cartoes", on ? tc.accent : tc.muted);
            const QRectF ir(b.center().x() - 8, b.center().y() - 8, 16, 16);
            ic.paint(&p, ir.toRect());
        }
    }

    if (cards_.empty()) {
        QFont f = font();
        f.setPointSizeF(font().pointSizeF() * 1.1);
        p.setFont(f);
        p.setPen(tc.muted);
        p.drawText(viewport()->rect().adjusted(40, 40, -40, -40), Qt::AlignCenter | Qt::TextWordWrap,
                   tr("No scenes yet.\nWrite a scene heading (INT. or EXT.) in the script and it shows up here as a card."));
        return;
    }

    const double vh = viewport()->height();
    for (int i = 0; i < static_cast<int>(cards_.size()); ++i) {
        QRectF r = cardRect(i);
        if (r.bottom() < -20 || r.top() > vh + 20) continue;
        if (dragging_ && i == pressIndex_) {
            // Lugar vazio de onde o cartão saiu.
            p.setPen(QPen(tc.border, 1.5, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 10, 10);
            continue;
        }
        paintCard(p, r, i, i == selected_ && !dragging_, i == hover_ && !dragging_);
    }

    if (dragging_ && pressIndex_ >= 0) {
        if (dropIndex_ >= 0 && dropIndex_ != pressIndex_ && dropIndex_ != pressIndex_ + 1) {
            p.setPen(Qt::NoPen);
            p.setBrush(tc.accent);
            p.drawRoundedRect(dropBar_, 2, 2);
        }
        // Cartão "flutuando" sob o mouse, levemente inclinado.
        QRectF r(dragPos_ - grabOffset_, QSizeF(cardW_, cardH_));
        p.save();
        p.translate(r.center());
        p.rotate(2.0);
        p.translate(-r.center());
        p.setOpacity(0.96);
        softShadow(p, r, 10, tc.dark ? 0.14 : 0.07, 14, 8);
        paintCard(p, r, pressIndex_, true, false);
        p.restore();
    }
}

void SceneBoard::resizeEvent(QResizeEvent* e) {
    QAbstractScrollArea::resizeEvent(e);
    relayout();
}

// ------------------------------------------------------------------ mouse e teclado

void SceneBoard::mousePressEvent(QMouseEvent* e) {
    setFocus();
    if (e->button() != Qt::LeftButton) return;
    if (toggleRect(false).contains(e->position())) { setListMode(false); return; }
    if (toggleRect(true).contains(e->position())) { setListMode(true); return; }
    pressIndex_ = cardAt(e->position());
    pressPos_ = e->position();
    if (pressIndex_ >= 0) selectScene(pressIndex_, false);
}

void SceneBoard::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pos = e->position();
    if (!(e->buttons() & Qt::LeftButton)) {
        int h = cardAt(pos);
        const int t = toggleRect(false).contains(pos) ? 0 : toggleRect(true).contains(pos) ? 1 : -1;
        if (h != hover_ || t != hoverToggle_) { hover_ = h; hoverToggle_ = t; viewport()->update(); }
        viewport()->setCursor(t >= 0 ? Qt::PointingHandCursor : h >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
        if (t >= 0) viewport()->setToolTip(t ? tr("Show as a list") : tr("Show as a grid"));
        else viewport()->setToolTip(QString());
        return;
    }
    if (pressIndex_ < 0) return;
    if (!dragging_ && (pos - pressPos_).manhattanLength() >= QApplication::startDragDistance()) {
        dragging_ = true;
        grabOffset_ = pressPos_ - cardRect(pressIndex_).topLeft();
        viewport()->setCursor(Qt::ClosedHandCursor);
        autoScroll_.start();
    }
    if (dragging_) {
        dragPos_ = pos;
        dropIndex_ = dropIndexAt(pos, &dropBar_);
        viewport()->update();
    }
}

void SceneBoard::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const bool wasDragging = dragging_;
    const int from = pressIndex_, to = dropIndex_;
    dragging_ = false;
    pressIndex_ = -1;
    dropIndex_ = -1;
    autoScroll_.stop();
    viewport()->setCursor(cardAt(e->position()) >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    viewport()->update();
    if (wasDragging && from >= 0 && to >= 0) requestMove(from, to);
}

void SceneBoard::mouseDoubleClickEvent(QMouseEvent* e) {
    int i = cardAt(e->position());
    if (i >= 0) emit openScene(cards_[i].para);
}

void SceneBoard::leaveEvent(QEvent*) {
    if (hover_ != -1 || hoverToggle_ != -1) { hover_ = -1; hoverToggle_ = -1; viewport()->update(); }
}

void SceneBoard::wheelEvent(QWheelEvent* e) {
    if ((e->modifiers() & Qt::ControlModifier) && !list_) {
        // Ctrl+roda: cartões maiores ou menores.
        minCardW_ = std::clamp(minCardW_ + (e->angleDelta().y() > 0 ? 30.0 : -30.0), 190.0, 460.0);
        relayout();
        viewport()->update();
        e->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(e);
}

void SceneBoard::keyPressEvent(QKeyEvent* e) {
    const int n = static_cast<int>(cards_.size());
    if (n == 0) { QAbstractScrollArea::keyPressEvent(e); return; }
    const int sel = selected_ < 0 ? 0 : selected_;
    const bool alt = e->modifiers() & Qt::AltModifier;
    int step = 0;
    switch (e->key()) {
        case Qt::Key_Left: step = -1; break;
        case Qt::Key_Right: step = 1; break;
        case Qt::Key_Up: step = -cols_; break;
        case Qt::Key_Down: step = cols_; break;
        case Qt::Key_Home: selectScene(0); return;
        case Qt::Key_End: selectScene(n - 1); return;
        case Qt::Key_Return:
        case Qt::Key_Enter: emit openScene(cards_[sel].para); return;
        case Qt::Key_F2: emit editSynopsisRequested(sel); return;
        default: QAbstractScrollArea::keyPressEvent(e); return;
    }
    if (alt) {
        // Alt+setas: move o cartão selecionado.
        int target = sel + step;
        if (target < 0 || target >= n) return;
        requestMove(sel, step > 0 ? target + 1 : target);
    } else {
        selectScene(std::clamp(sel + step, 0, n - 1));
    }
}

void SceneBoard::contextMenuEvent(QContextMenuEvent* e) {
    int i = cardAt(e->pos());
    if (i < 0) return;
    selectScene(i, false);
    QMenu menu(this);
    menu.addAction(tr("Open in Script"), this, [this, i] { emit openScene(cards_[i].para); });
    menu.addAction(tr("Edit Synopsis...") + "\tF2", this, [this, i] { emit editSynopsisRequested(i); });
    menu.addSeparator();
    QAction* first = menu.addAction(tr("Move to Start"), this, [this, i] { requestMove(i, 0); });
    first->setEnabled(i > 0);
    QAction* last = menu.addAction(tr("Move to End"), this,
                                   [this, i] { requestMove(i, static_cast<int>(cards_.size())); });
    last->setEnabled(i + 1 < static_cast<int>(cards_.size()));
    menu.exec(e->globalPos());
}
