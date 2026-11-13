#include "pagepainter.h"

#include <QFontDatabase>
#include <QGlyphRun>
#include <QSettings>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace {
QString toQ(const std::u32string& s) {
    return QString::fromUcs4(s.data(), static_cast<qsizetype>(s.size()));
}
}  // namespace

namespace {
QString& cachedFamily() { static QString f; return f; }
int& generation() { static int g = 0; return g; }
}  // namespace

QString PagePainter::defaultFontFamily() {
    static QString family;
    if (!family.isEmpty()) return family;
    const QStringList available = QFontDatabase::families();
    for (const char* f : {"Courier Prime", "Courier Final Draft", "Courier Screenplay", "Nimbus Mono PS",
                          "Nimbus Mono L", "Courier New", "Liberation Mono", "Courier 10 Pitch",
                          "DejaVu Sans Mono"}) {
        // Famílias com mais de um fabricante aparecem como "Nome [fabricante]".
        const QString want = QString::fromUtf8(f);
        QString match;
        for (const QString& a : available) {
            if (a.compare(want, Qt::CaseInsensitive) == 0) { match = a; break; }
            if (a.startsWith(want + " [", Qt::CaseInsensitive) && (match.isEmpty() || a.contains("[urw]")))
                match = a;
        }
        if (!match.isEmpty()) {
            family = match;
            return family;
        }
    }
    family = QStringLiteral("Monospace");
    return family;
}

QString PagePainter::fontFamily() {
    QString& family = cachedFamily();
    if (family.isEmpty()) {
        const QString wanted = QSettings().value("scriptFont").toString();
        family = !wanted.isEmpty() && QFontDatabase::families().contains(wanted) ? wanted : defaultFontFamily();
    }
    return family;
}

void PagePainter::setPreferredFontFamily(const QString& family) {
    QSettings().setValue("scriptFont", family);
    cachedFamily().clear();
    ++generation();
}

QRawFont PagePainter::rawFont(double ppi) {
    if (fontGeneration_ != generation()) {
        fonts_.clear();
        fontGeneration_ = generation();
    }
    int key = static_cast<int>(std::lround(ppi * 100));
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;
    QFont f(fontFamily());
    f.setStyleHint(QFont::TypeWriter);
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setPixelSize(100);
    QRawFont raw = QRawFont::fromFont(f);
    raw.setPixelSize(ppi / 6.0);  // 12 pt = 1/6 de polegada
    if (fonts_.size() > 8) fonts_.clear();
    fonts_[key] = raw;
    return raw;
}

void PagePainter::drawText(QPainter& p, const QRawFont& f, const std::u32string& text, double x,
                           double baseline, double charW, const QColor& color) {
    if (text.empty()) return;
    QString s = toQ(text);
    QList<quint32> glyphs = f.glyphIndexesForString(s);
    QList<QPointF> pos;
    pos.reserve(glyphs.size());
    for (qsizetype i = 0; i < glyphs.size(); ++i) pos.append(QPointF(x + i * charW, baseline));
    QGlyphRun run;
    run.setRawFont(f);
    run.setGlyphIndexes(glyphs);
    run.setPositions(pos);
    p.setPen(color);
    p.drawGlyphRun(QPointF(0, 0), run);
}

bool PagePainter::hasTitlePage() const { return !ed_.doc().titleValue("Title").empty(); }

void PagePainter::paintPage(QPainter& p, int pg, QPointF o, double ppi, const Extras& ex) {
    const Document& doc = ed_.doc();
    const Layout& L = ed_.layout();
    const double charW = ppi / 10.0;
    const double lineH = ppi / 6.0;
    QRawFont f = rawFont(ppi);
    const double baseOff = (lineH - (f.ascent() + f.descent())) / 2.0 + f.ascent();
    const QColor ink(20, 20, 20);

    auto rowTop = [&](int textRow) { return o.y() + (page::kTopRow + textRow) * lineH; };

    // Número da página (a partir da 2ª).
    if (pg > 0) {
        std::u32string num = utf8ToU32(std::to_string(pg + 1) + ".");
        drawText(p, f, num, o.x() + (page::kRightCol - static_cast<int>(num.size())) * charW,
                 o.y() + page::kPageNumberRow * lineH + baseOff, charW, ink);
    }

    std::pair<Pos, Pos> sel = ed_.selection();
    const bool selecting = ex.showSelection && ed_.hasSelection();
    std::vector<int> sceneList = ex.sceneNumbers ? ed_.scenes() : std::vector<int>{};

    for (size_t li = 0; li < L.lines.size(); ++li) {
        const LayoutLine& l = L.lines[li];
        if (l.page != pg) continue;
        const double top = rowTop(l.row);

        if (selecting && l.para >= 0 && l.para >= sel.first.para && l.para <= sel.second.para) {
            int a = (l.para == sel.first.para) ? std::max(sel.first.off, l.start) : l.start;
            int b = (l.para == sel.second.para) ? std::min(sel.second.off, l.end) : l.end;
            bool lastLineOfPara = L.paraLines[l.para].back() == static_cast<int>(li);
            double extra = (l.para < sel.second.para && lastLineOfPara) ? 0.5 : 0.0;
            if (b > a || extra > 0) {
                QRectF r(o.x() + (l.col + (a - l.start)) * charW, top,
                         std::max(0, b - a) * charW + extra * charW, lineH);
                p.fillRect(r, ex.selectionColor);
            }
        }

        std::u32string text = L.lineText(doc, static_cast<int>(li));
        const bool dimmed = ex.focusFrom >= 0 && (l.para < ex.focusFrom || l.para > ex.focusTo);
        drawText(p, f, text, o.x() + l.col * charW, top + baseOff, charW,
                 dimmed ? QColor(196, 199, 204) : ink);

        // Números de cena nas duas margens.
        if (ex.sceneNumbers && l.para >= 0 && doc.paragraphs[l.para].type == Element::SceneHeading &&
            L.paraLines[l.para].front() == static_cast<int>(li)) {
            auto it = std::find(sceneList.begin(), sceneList.end(), l.para);
            if (it != sceneList.end()) {
                std::u32string n = utf8ToU32(std::to_string(it - sceneList.begin() + 1));
                drawText(p, f, n, o.x() + (page::kLeftCol - 4 - static_cast<int>(n.size())) * charW,
                         top + baseOff, charW, ink);
                drawText(p, f, n, o.x() + (page::kRightCol + 2) * charW, top + baseOff, charW, ink);
            }
        }
    }

    // Cursor, composição do IME e sugestão do autocompletar.
    const Pos c = ed_.cursor();
    const int cl = ed_.lineOf(c);
    if (cl >= 0 && L.lines[cl].page == pg && (ex.showCursor || !ex.ghost.empty() || !ex.preedit.empty())) {
        const LayoutLine& l = L.lines[cl];
        double x = o.x() + ed_.columnOf(c) * charW;
        double top = rowTop(l.row);
        if (!ex.preedit.empty()) {
            drawText(p, f, ex.preedit, x, top + baseOff, charW, QColor(40, 90, 200));
            p.fillRect(QRectF(x, top + lineH - std::max(1.0, ppi / 96.0), ex.preedit.size() * charW,
                              std::max(1.0, ppi / 96.0)),
                       QColor(40, 90, 200));
            x += ex.preedit.size() * charW;
        } else if (!ex.ghost.empty()) {
            drawText(p, f, toUpper(ex.ghost), x, top + baseOff, charW, QColor(160, 160, 160));
        }
        if (ex.showCursor)
            p.fillRect(QRectF(x - 0.5, top + lineH * 0.05, std::max(2.0, ppi / 48.0), lineH * 0.9),
                       ex.cursorColor);
    }
}

void PagePainter::paintTitlePage(QPainter& p, QPointF o, double ppi) {
    const Document& doc = ed_.doc();
    const double charW = ppi / 10.0;
    const double lineH = ppi / 6.0;
    QRawFont f = rawFont(ppi);
    const double baseOff = (lineH - (f.ascent() + f.descent())) / 2.0 + f.ascent();
    const QColor ink(20, 20, 20);

    auto splitLines = [](const std::string& s) {
        std::vector<std::u32string> out;
        std::u32string cur;
        for (char32_t c : utf8ToU32(s)) {
            if (c == U'\n') { out.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        out.push_back(cur);
        return out;
    };
    auto centered = [&](const std::u32string& t, int row) {
        int col = 45 - static_cast<int>(t.size()) / 2;
        drawText(p, f, t, o.x() + col * charW, o.y() + row * lineH + baseOff, charW, ink);
    };

    int row = 22;
    for (auto& t : splitLines(doc.titleValue("Title"))) centered(t, row++);  // como o autor escreveu, sem forçar maiúsculas
    row += 3;
    std::string credit = doc.titleValue("Credit");
    if (credit.empty()) credit = "Escrito por";
    for (auto& t : splitLines(credit)) centered(t, row++);
    row += 1;
    for (auto& t : splitLines(doc.titleValue("Author"))) centered(t, row++);
    std::string source = doc.titleValue("Source");
    if (!source.empty()) {
        row += 2;
        for (auto& t : splitLines(source)) centered(t, row++);
    }

    auto contact = splitLines(doc.titleValue("Contact"));
    if (doc.titleValue("Contact").empty()) contact.clear();
    int crow = 60 - static_cast<int>(contact.size());
    for (auto& t : contact)
        drawText(p, f, t, o.x() + page::kLeftCol * charW, o.y() + (crow++) * lineH + baseOff, charW, ink);

    // Bloco à direita, alinhado à base: observações, versão e data.
    std::vector<std::u32string> right;
    if (!doc.titleValue("Notes").empty())
        for (auto& t : splitLines(doc.titleValue("Notes"))) right.push_back(t);
    if (!doc.titleValue("Revision").empty()) right.push_back(utf8ToU32(doc.titleValue("Revision")));
    if (!doc.titleValue("Draft date").empty()) right.push_back(utf8ToU32(doc.titleValue("Draft date")));
    int rrow = 60 - static_cast<int>(right.size());
    for (auto& t : right)
        drawText(p, f, t, o.x() + (page::kRightCol - static_cast<int>(t.size())) * charW,
                 o.y() + (rrow++) * lineH + baseOff, charW, ink);
}

void PagePainter::drawWatermark(QPainter& p, QPointF o, double ppi, const QString& text) {
    if (text.trimmed().isEmpty()) return;
    p.save();
    QFont f(QStringLiteral("Sans Serif"));
    f.setWeight(QFont::Bold);
    f.setPixelSize(std::max(8, static_cast<int>(ppi * 0.85)));
    f.setLetterSpacing(QFont::PercentageSpacing, 108);
    QFontMetricsF fm(f);
    const double diag = std::hypot(8.5, 11.0) * ppi * 0.82;
    const double w = fm.horizontalAdvance(text.trimmed().toUpper());
    if (w > diag) f.setPixelSize(std::max(8, static_cast<int>(f.pixelSize() * diag / w)));
    p.setFont(f);
    p.setPen(QColor(0, 0, 0, 26));
    p.translate(o.x() + 4.25 * ppi, o.y() + 5.5 * ppi);
    p.rotate(-52);
    p.drawText(QRectF(-diag / 2, -ppi, diag, 2 * ppi), Qt::AlignCenter, text.trimmed().toUpper());
    p.restore();
}

void renderScript(QPagedPaintDevice& device, double ppi, const Editor& ed, const PrintOptions& opt) {
    QPainter p(&device);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    PagePainter pp(ed);
    PagePainter::Extras ex;
    ex.sceneNumbers = opt.sceneNumbers;
    bool first = true;
    if (opt.titlePage && pp.hasTitlePage()) {
        pp.paintTitlePage(p, QPointF(0, 0), ppi);
        PagePainter::drawWatermark(p, QPointF(0, 0), ppi, opt.watermark);
        first = false;
    }
    const int pages = ed.layout().pageCount;
    const int from = std::clamp(opt.fromPage, 1, pages), to = std::clamp(opt.toPage, from, pages);
    for (int i = from - 1; i < to; ++i) {
        if (!first) device.newPage();
        pp.paintPage(p, i, QPointF(0, 0), ppi, ex);
        PagePainter::drawWatermark(p, QPointF(0, 0), ppi, opt.watermark);
        first = false;
    }
}
