#include "scriptview.h"

#include "theme.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <climits>
#include <cmath>
#include <tuple>

ScriptView::ScriptView(Editor* editor, QWidget* parent)
    : QAbstractScrollArea(parent), ed_(editor), painter_(*editor) {
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled);
    viewport()->setCursor(Qt::IBeamCursor);
    setFrameShape(QFrame::NoFrame);

    connect(&ThemeManager::instance(), &ThemeManager::changed, this, [this] { viewport()->update(); });

    blink_.setInterval(530);
    connect(&blink_, &QTimer::timeout, this, [this] {
        cursorOn_ = !cursorOn_;
        viewport()->update(cursorRect().adjusted(-4, -4, 4, 4).toAlignedRect());
    });
    blink_.start();

    autoScroll_.setInterval(30);
    connect(&autoScroll_, &QTimer::timeout, this, [this] {
        const double h = viewport()->height();
        int dy = 0;
        if (lastMouse_.y() < 0) dy = static_cast<int>(lastMouse_.y());
        else if (lastMouse_.y() > h) dy = static_cast<int>(lastMouse_.y() - h);
        if (dy == 0) return;
        verticalScrollBar()->setValue(verticalScrollBar()->value() + dy);
        ed_->setCursor(posFromPoint(lastMouse_), true);
        emit cursorMoved();
        viewport()->update();
    });
    updateScrollBars();
}

void ScriptView::refresh() {
    preedit_.clear();
    updateScrollBars();
    verticalScrollBar()->setValue(0);
    viewport()->update();
    emit documentChanged();
    emit cursorMoved();
}

void ScriptView::refreshAfterExternalEdit() {
    updateScrollBars();
    ensureCursorVisible();
    restartBlink();
    viewport()->update();
    emit documentChanged();
    emit cursorMoved();
}

void ScriptView::setZoom(double z) {
    z = std::clamp(z, 0.5, 3.0);
    if (std::abs(z - zoom_) < 1e-6) return;
    QScrollBar* vb = verticalScrollBar();
    double frac = vb->maximum() > 0 ? double(vb->value()) / vb->maximum() : 0.0;
    zoom_ = z;
    updateScrollBars();
    vb->setValue(static_cast<int>(frac * vb->maximum()));
    viewport()->update();
    emit zoomChanged(zoom_);
}

void ScriptView::setSceneNumbers(bool on) {
    sceneNumbers_ = on;
    viewport()->update();
}

QPointF ScriptView::pageOrigin(int pg) const {
    double x = std::max(gap(), (viewport()->width() - pageW()) / 2.0) - horizontalScrollBar()->value();
    double y = gap() + pg * (pageH() + gap()) - verticalScrollBar()->value();
    return {std::floor(x), std::floor(y)};
}

QRectF ScriptView::cursorRect() const {
    const Layout& L = ed_->layout();
    const Pos c = ed_->cursor();
    const LayoutLine& l = L.lines[ed_->lineOf(c)];
    QPointF o = pageOrigin(l.page);
    const double charW = ppi() / 10.0, lineH = ppi() / 6.0;
    double x = o.x() + (ed_->columnOf(c) + static_cast<double>(preedit_.size())) * charW;
    double y = o.y() + (page::kTopRow + l.row) * lineH;
    return QRectF(x - 1, y, 3, lineH);
}

int ScriptView::currentPage() const {
    return ed_->layout().lines[ed_->lineOf(ed_->cursor())].page;
}

Pos ScriptView::posFromPoint(QPointF pt) const {
    const Layout& L = ed_->layout();
    const double contentY = pt.y() + verticalScrollBar()->value();
    int pg = static_cast<int>(std::floor((contentY - gap()) / (pageH() + gap())));
    pg = std::clamp(pg, 0, L.pageCount - 1);
    QPointF o = pageOrigin(pg);
    const double charW = ppi() / 10.0, lineH = ppi() / 6.0;
    int row = static_cast<int>(std::floor((pt.y() - o.y()) / lineH)) - page::kTopRow;
    int col = static_cast<int>(std::floor((pt.x() - o.x()) / charW + 0.5));

    int best = -1, bestDist = INT_MAX;
    for (size_t li = 0; li < L.lines.size(); ++li) {
        const LayoutLine& l = L.lines[li];
        if (l.para < 0) continue;
        if (l.page != pg) {
            if (l.page > pg && best >= 0) break;
            continue;
        }
        int d = std::abs(l.row - row);
        if (d < bestDist) { bestDist = d; best = static_cast<int>(li); }
    }
    if (best < 0) {
        // Página só com linhas virtuais: pega a linha real mais próxima.
        for (size_t li = 0; li < L.lines.size(); ++li)
            if (L.lines[li].para >= 0 && L.lines[li].page <= pg) best = static_cast<int>(li);
        if (best < 0) return ed_->cursor();
    }
    return ed_->posAtLine(best, col);
}

void ScriptView::updateScrollBars() {
    const int pages = ed_->layout().pageCount;
    const double totalH = gap() + pages * (pageH() + gap());
    const double totalW = pageW() + 2 * gap();
    QScrollBar* vb = verticalScrollBar();
    vb->setRange(0, std::max(0, static_cast<int>(totalH - viewport()->height())));
    vb->setPageStep(viewport()->height());
    vb->setSingleStep(static_cast<int>(ppi() / 6.0 * 3));
    QScrollBar* hb = horizontalScrollBar();
    hb->setRange(0, std::max(0, static_cast<int>(totalW - viewport()->width())));
    hb->setPageStep(viewport()->width());
    hb->setSingleStep(20);
}

void ScriptView::ensureCursorVisible() {
    QRectF r = cursorRect();
    const double margin = ppi() / 6.0 * 3;
    QScrollBar* vb = verticalScrollBar();
    if (r.top() < margin) vb->setValue(vb->value() - static_cast<int>(margin - r.top()));
    else if (r.bottom() > viewport()->height() - margin)
        vb->setValue(vb->value() + static_cast<int>(r.bottom() - (viewport()->height() - margin)));
    QScrollBar* hb = horizontalScrollBar();
    if (r.left() < 20) hb->setValue(hb->value() - static_cast<int>(20 - r.left()));
    else if (r.right() > viewport()->width() - 20)
        hb->setValue(hb->value() + static_cast<int>(r.right() - (viewport()->width() - 20)));
}

void ScriptView::restartBlink() {
    cursorOn_ = true;
    blink_.start();
}

void ScriptView::afterEdit(int oldRevision) {
    if (ed_->revision() != oldRevision) {
        updateScrollBars();
        emit documentChanged();
    }
    ensureCursorVisible();
    restartBlink();
    viewport()->update();
    emit cursorMoved();
}

void ScriptView::gotoParagraph(int para) {
    if (para < 0 || para >= static_cast<int>(ed_->doc().paragraphs.size())) return;
    ed_->setCursor({para, 0});
    const Layout& L = ed_->layout();
    const LayoutLine& l = L.lines[L.paraLines[para].front()];
    double y = gap() + l.page * (pageH() + gap()) + (page::kTopRow + l.row) * (ppi() / 6.0);
    verticalScrollBar()->setValue(static_cast<int>(y - ppi() / 6.0 * 6));
    restartBlink();
    viewport()->update();
    emit cursorMoved();
    setFocus();
}

// ------------------------------------------------------------------ pintura

void ScriptView::setFocusMode(bool on) {
    focusMode_ = on;
    viewport()->update();
}

std::pair<int, int> ScriptView::focusRange() const {
    const auto& ps = ed_->doc().paragraphs;
    const int para = ed_->cursor().para;
    auto inSpeech = [&](int i) {
        return ps[i].type == Element::Parenthetical || ps[i].type == Element::Dialogue;
    };
    int from = para, to = para;
    if (ps[para].type == Element::Character || inSpeech(para)) {
        // Destaca a fala inteira: personagem + parentéticos + diálogos.
        while (from > 0 && inSpeech(from)) --from;
        if (ps[from].type != Element::Character && from < para) ++from;
        while (to + 1 < static_cast<int>(ps.size()) && inSpeech(to + 1)) ++to;
    }
    return {from, to};
}

void ScriptView::paintEvent(QPaintEvent* ev) {
    const ThemeColors& tc = ThemeManager::instance().colors();
    QPainter p(viewport());
    p.fillRect(ev->rect(), tc.canvas);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    PagePainter::Extras ex;
    ex.showCursor = hasFocus() && cursorOn_;
    ex.showSelection = true;
    ex.sceneNumbers = sceneNumbers_;
    ex.preedit = preedit_;
    if (preedit_.empty() && hasFocus()) ex.ghost = ed_->suggestion();
    if (!hasFocus()) ex.selectionColor = QColor(226, 229, 234);
    if (focusMode_) std::tie(ex.focusFrom, ex.focusTo) = focusRange();

    const int pages = ed_->layout().pageCount;
    const double stride = pageH() + gap();
    const int curPage = currentPage();
    int first = std::max(0, static_cast<int>((verticalScrollBar()->value() - gap()) / stride));
    for (int pg = first; pg < pages; ++pg) {
        QPointF o = pageOrigin(pg);
        if (o.y() > viewport()->height()) break;
        QRectF pr(o, QSizeF(pageW(), pageH()));
        if (!pr.adjusted(-12, -12, 12, 12).intersects(ev->rect())) continue;

        // Sombra suave em camadas.
        p.setPen(Qt::NoPen);
        const int layers = 10;
        for (int i = layers; i >= 1; --i) {
            const double a = (tc.dark ? 0.09 : 0.035) * (1.0 - double(i) / (layers + 1));
            p.setBrush(QColor(0, 0, 0, static_cast<int>(a * 255)));
            p.drawRoundedRect(pr.adjusted(-i, -i + 3, i, i + 3), 3 + i, 3 + i);
        }
        p.setBrush(Qt::white);
        p.drawRoundedRect(pr, 2, 2);
        painter_.paintPage(p, pg, o, ppi(), ex);

        // Etiqueta do elemento atual na margem esquerda (só na tela).
        if (pg == curPage && hasFocus() && !ed_->hasSelection() && zoom_ >= 0.85) {
            const LayoutLine& l = ed_->layout().lines[ed_->lineOf(ed_->cursor())];
            const Element el = ed_->currentElement();
            if (!(sceneNumbers_ && el == Element::SceneHeading)) {
                QFont f = font();
                f.setPointSizeF(std::max(6.5, 7.5 * zoom_));
                f.setWeight(QFont::DemiBold);
                QFontMetricsF fm(f);
                const QString tag = QString::fromUtf8(elementName(el));
                const double lineH = ppi() / 6.0;
                const double w = fm.horizontalAdvance(tag) + 12 * zoom_;
                const double h = std::min(lineH, fm.height() + 4 * zoom_);
                const double right = o.x() + (page::kLeftCol - 2) * (ppi() / 10.0);
                const double top = o.y() + (page::kTopRow + l.row) * lineH + (lineH - h) / 2;
                QRectF pill(right - w, top, w, h);
                QColor bg = tc.accent;
                bg.setAlphaF(0.10);
                p.setBrush(bg);
                p.drawRoundedRect(pill, h / 2, h / 2);
                p.setFont(f);
                p.setPen(QColor(59, 108, 246));
                p.drawText(pill, Qt::AlignCenter, tag);
                p.setPen(Qt::NoPen);
            }
        }
    }
}

void ScriptView::resizeEvent(QResizeEvent* e) {
    QAbstractScrollArea::resizeEvent(e);
    updateScrollBars();
}

// ------------------------------------------------------------------ teclado

void ScriptView::keyPressEvent(QKeyEvent* e) {
    const int rev = ed_->revision();
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    const bool alt = e->modifiers() & Qt::AltModifier;

    switch (e->key()) {
        case Qt::Key_Left: ed_->moveLeft(shift, ctrl); break;
        case Qt::Key_Right:
            if (!shift && !ctrl && !ed_->suggestion().empty()) ed_->acceptSuggestion();
            else ed_->moveRight(shift, ctrl);
            break;
        case Qt::Key_Up: ed_->moveUp(shift); break;
        case Qt::Key_Down: ed_->moveDown(shift); break;
        case Qt::Key_Home: ctrl ? ed_->moveDocStart(shift) : ed_->moveLineStart(shift); break;
        case Qt::Key_End:
            if (!shift && !ctrl && !ed_->suggestion().empty()) ed_->acceptSuggestion();
            else ctrl ? ed_->moveDocEnd(shift) : ed_->moveLineEnd(shift);
            break;
        case Qt::Key_PageUp: ed_->movePage(-1, shift); break;
        case Qt::Key_PageDown: ed_->movePage(1, shift); break;
        case Qt::Key_Return:
        case Qt::Key_Enter: ed_->enter(); break;
        case Qt::Key_Tab: ed_->tab(false); break;
        case Qt::Key_Backtab: ed_->tab(true); break;
        case Qt::Key_Backspace: ed_->backspace(ctrl); break;
        case Qt::Key_Delete: ed_->deleteForward(ctrl); break;
        case Qt::Key_Escape: ed_->setCursor(ed_->cursor()); break;
        default: {
            const QString t = e->text();
            if (t.isEmpty() || (ctrl && !alt) || !t.at(0).isPrint()) {
                QAbstractScrollArea::keyPressEvent(e);
                return;
            }
            ed_->insertText(t.toStdU32String());
        }
    }
    e->accept();
    afterEdit(rev);
}

void ScriptView::inputMethodEvent(QInputMethodEvent* e) {
    const int rev = ed_->revision();
    if (!e->commitString().isEmpty()) ed_->insertText(e->commitString().toStdU32String());
    preedit_ = e->preeditString().toStdU32String();
    e->accept();
    afterEdit(rev);
}

QVariant ScriptView::inputMethodQuery(Qt::InputMethodQuery q) const {
    switch (q) {
        case Qt::ImEnabled: return true;
        case Qt::ImCursorRectangle: return cursorRect().toAlignedRect();
        case Qt::ImFont: return QFont(PagePainter::fontFamily());
        default: return QAbstractScrollArea::inputMethodQuery(q);
    }
}

// ------------------------------------------------------------------ mouse

void ScriptView::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) { QAbstractScrollArea::mousePressEvent(e); return; }
    setFocus();
    const int rev = ed_->revision();
    Pos p = posFromPoint(e->position());
    if (lastDoubleClick_.isValid() && lastDoubleClick_.elapsed() < 450) {
        // Clique triplo: seleciona o parágrafo inteiro.
        ed_->setCursor({p.para, 0});
        ed_->setCursor({p.para, static_cast<int>(ed_->doc().paragraphs[p.para].text.size())}, true);
        lastDoubleClick_.invalidate();
    } else {
        ed_->setCursor(p, e->modifiers() & Qt::ShiftModifier);
        dragging_ = true;
    }
    lastMouse_ = e->position();
    afterEdit(rev);
}

void ScriptView::mouseMoveEvent(QMouseEvent* e) {
    if (!dragging_) return;
    lastMouse_ = e->position();
    ed_->setCursor(posFromPoint(e->position()), true);
    if (e->position().y() < 0 || e->position().y() > viewport()->height()) {
        if (!autoScroll_.isActive()) autoScroll_.start();
    } else {
        autoScroll_.stop();
    }
    restartBlink();
    viewport()->update();
    emit cursorMoved();
}

void ScriptView::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false;
    autoScroll_.stop();
}

void ScriptView::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    ed_->selectWordAt(posFromPoint(e->position()));
    lastDoubleClick_.start();
    dragging_ = false;
    viewport()->update();
    emit cursorMoved();
}

void ScriptView::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        if (e->angleDelta().y() > 0) zoomIn();
        else if (e->angleDelta().y() < 0) zoomOut();
        e->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(e);
}

void ScriptView::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);
    menu.addAction(tr("Recortar"), this, &ScriptView::cut)->setEnabled(ed_->hasSelection());
    menu.addAction(tr("Copiar"), this, &ScriptView::copy)->setEnabled(ed_->hasSelection());
    menu.addAction(tr("Colar"), this, &ScriptView::paste);
    menu.addSeparator();
    QMenu* el = menu.addMenu(tr("Elemento"));
    for (int i = 0; i < kElementCount; ++i) {
        Element t = static_cast<Element>(i);
        QAction* a = el->addAction(QString::fromUtf8(elementName(t)), this, [this, t] { setElement(t); });
        a->setCheckable(true);
        a->setChecked(ed_->currentElement() == t);
    }
    menu.exec(e->globalPos());
}

void ScriptView::focusInEvent(QFocusEvent* e) {
    QAbstractScrollArea::focusInEvent(e);
    restartBlink();
    viewport()->update();
}

void ScriptView::focusOutEvent(QFocusEvent* e) {
    QAbstractScrollArea::focusOutEvent(e);
    viewport()->update();
}

// ------------------------------------------------------------------ ações

void ScriptView::undo() { int r = ed_->revision(); ed_->undo(); afterEdit(r); }
void ScriptView::redo() { int r = ed_->revision(); ed_->redo(); afterEdit(r); }

void ScriptView::copy() {
    if (ed_->hasSelection()) QApplication::clipboard()->setText(QString::fromStdString(ed_->copySelection()));
}

void ScriptView::cut() {
    if (!ed_->hasSelection()) return;
    copy();
    int r = ed_->revision();
    ed_->backspace(false);
    afterEdit(r);
}

void ScriptView::paste() {
    int r = ed_->revision();
    ed_->paste(QApplication::clipboard()->text().toStdString());
    afterEdit(r);
}

void ScriptView::selectAll() {
    ed_->selectAll();
    viewport()->update();
    emit cursorMoved();
}

void ScriptView::setElement(Element e) {
    int r = ed_->revision();
    ed_->setElement(e);
    afterEdit(r);
    setFocus();
}
