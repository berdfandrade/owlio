#include "layout.h"

#include <algorithm>

const ElementStyle& elementStyle(Element e) {
    //                                  indent width before right
    static const ElementStyle kScene  {15, 60, 1, false};
    static const ElementStyle kAction {15, 60, 1, false};
    static const ElementStyle kChar   {35, 38, 1, false};
    static const ElementStyle kParen  {30, 25, 0, false};
    static const ElementStyle kDial   {25, 35, 0, false};
    static const ElementStyle kTrans  {55, 20, 1, true};
    switch (e) {
        case Element::SceneHeading: return kScene;
        case Element::Action: return kAction;
        case Element::Character: return kChar;
        case Element::Parenthetical: return kParen;
        case Element::Dialogue: return kDial;
        case Element::Transition: return kTrans;
        default: return kAction;
    }
}

std::vector<int> wrapText(const std::u32string& text, int width) {
    std::vector<int> starts{0};
    const int n = static_cast<int>(text.size());
    int pos = 0;
    while (n - pos > width) {
        // Se o texto restante cabe ignorando espaços finais, não quebra.
        int contentEnd = n;
        while (contentEnd > pos && isSpace(text[contentEnd - 1])) --contentEnd;
        if (contentEnd - pos <= width) break;

        int brk = -1;
        for (int j = std::min(pos + width, n - 1); j > pos; --j) {
            if (isSpace(text[j])) { brk = j; break; }
        }
        int next;
        if (brk > pos) {
            next = brk + 1;
            while (next < n && isSpace(text[next])) ++next;
        } else {
            next = pos + width;  // palavra maior que a linha: quebra forçada
        }
        if (next >= n) break;
        starts.push_back(next);
        pos = next;
    }
    return starts;
}

std::u32string Layout::lineText(const Document& doc, int lineIndex) const {
    const LayoutLine& l = lines[lineIndex];
    if (l.para < 0) return l.virtText;
    std::u32string t = displayText(doc.paragraphs[l.para]).substr(l.start, l.end - l.start);
    while (!t.empty() && isSpace(t.back())) t.pop_back();
    return t;
}

int Layout::pageOfPara(int para) const {
    if (para < 0 || para >= static_cast<int>(paraLines.size()) || paraLines[para].empty()) return 0;
    return lines[paraLines[para].front()].page;
}

Layout computeLayout(const Document& doc, const LayoutOptions& opt) {
    Layout L;
    const int n = static_cast<int>(doc.paragraphs.size());
    L.paraLines.resize(n);

    std::vector<std::vector<int>> wraps(n);
    for (int i = 0; i < n; ++i) {
        const auto& p = doc.paragraphs[i];
        wraps[i] = wrapText(p.text, elementStyle(p.type).width);
    }

    const int LPP = page::kLinesPerPage;
    int pg = 0, row = 0;
    bool inSpeech = false;
    std::u32string speaker;
    const int charCol = elementStyle(Element::Character).indent;

    auto newPage = [&] { ++pg; row = 0; };
    auto addVirtual = [&](const std::u32string& text) {
        LayoutLine l;
        l.para = -1;
        l.col = charCol;
        l.page = pg;
        l.row = row++;
        l.virtText = text;
        L.lines.push_back(l);
    };
    auto contdLine = [&] {
        std::u32string s = speaker;
        if (s.size() < opt.contdText.size() ||
            s.compare(s.size() - opt.contdText.size(), opt.contdText.size(), opt.contdText) != 0)
            s += U" " + opt.contdText;
        return s;
    };
    auto placeLines = [&](int para, int from, int to) {
        const auto& p = doc.paragraphs[para];
        const auto& st = elementStyle(p.type);
        const auto& starts = wraps[para];
        const int len = static_cast<int>(p.text.size());
        for (int k = from; k < to; ++k) {
            if (row >= LPP) newPage();
            LayoutLine l;
            l.para = para;
            l.start = starts[k];
            l.end = (k + 1 < static_cast<int>(starts.size())) ? starts[k + 1] : len;
            int visible = l.end - l.start;
            while (visible > 0 && isSpace(p.text[l.start + visible - 1])) --visible;
            l.col = st.rightAlign ? std::max(st.indent, page::kRightCol - visible) : st.indent;
            l.page = pg;
            l.row = row++;
            L.paraLines[para].push_back(static_cast<int>(L.lines.size()));
            L.lines.push_back(l);
        }
    };

    for (int i = 0; i < n; ++i) {
        const auto& p = doc.paragraphs[i];
        const auto& st = elementStyle(p.type);
        const int nlines = static_cast<int>(wraps[i].size());
        const bool speechPart =
            inSpeech && (p.type == Element::Parenthetical || p.type == Element::Dialogue);

        int sb = row == 0 ? 0 : st.spaceBefore;
        int need = sb + nlines;

        // "Manter com o próximo": cabeçalho, personagem e parentético não ficam órfãos no pé da página.
        bool keepsWithNext = false;
        if (i + 1 < n) {
            const auto& nx = doc.paragraphs[i + 1];
            const bool nextIsSpeech = nx.type == Element::Dialogue || nx.type == Element::Parenthetical;
            if (p.type == Element::SceneHeading ||
                ((p.type == Element::Character || p.type == Element::Parenthetical) && nextIsSpeech)) {
                keepsWithNext = true;
                need += elementStyle(nx.type).spaceBefore +
                        std::min<int>(2, static_cast<int>(wraps[i + 1].size()));
            }
        }

        if (row > 0 && row + need > LPP) {
            int availOwn = LPP - row - sb;
            if (p.type == Element::Dialogue && speechPart) availOwn -= 1;  // espaço para (MAIS)
            const bool canSplit = !keepsWithNext &&
                                  (p.type == Element::Action || p.type == Element::Dialogue) &&
                                  availOwn >= 2 && nlines - availOwn >= 2;
            if (canSplit) {
                row += sb;
                placeLines(i, 0, availOwn);
                if (p.type == Element::Dialogue && speechPart) {
                    addVirtual(opt.moreText);
                    newPage();
                    addVirtual(contdLine());
                } else {
                    newPage();
                }
                placeLines(i, availOwn, nlines);
            } else {
                if (speechPart) {
                    if (row < LPP) addVirtual(opt.moreText);
                    newPage();
                    addVirtual(contdLine());
                } else {
                    newPage();
                }
                placeLines(i, 0, nlines);
            }
        } else {
            row += sb;
            placeLines(i, 0, nlines);
        }

        if (p.type == Element::Character) {
            inSpeech = true;
            speaker = trim(displayText(p));
        } else if (p.type != Element::Parenthetical && p.type != Element::Dialogue) {
            inSpeech = false;
        }
    }
    L.pageCount = pg + 1;
    return L;
}
