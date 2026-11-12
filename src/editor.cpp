#include "editor.h"

#include <algorithm>
#include <map>

#include "fountain.h"

namespace {

// Enter no fim de um elemento: qual vem a seguir (comportamento do Final Draft).
Element nextOnEnter(Element e) {
    switch (e) {
        case Element::SceneHeading: return Element::Action;
        case Element::Action: return Element::Action;
        case Element::Character: return Element::Dialogue;
        case Element::Parenthetical: return Element::Dialogue;
        case Element::Dialogue: return Element::Character;
        case Element::Transition: return Element::SceneHeading;
        default: return Element::Action;
    }
}

Element nextOnTab(Element e) {
    switch (e) {
        case Element::Action: return Element::Character;
        case Element::Character: return Element::Transition;
        case Element::Transition: return Element::SceneHeading;
        case Element::SceneHeading: return Element::Action;
        case Element::Dialogue: return Element::Parenthetical;
        case Element::Parenthetical: return Element::Dialogue;
        default: return Element::Action;
    }
}

Element prevOnTab(Element e) {
    switch (e) {
        case Element::Action: return Element::SceneHeading;
        case Element::SceneHeading: return Element::Transition;
        case Element::Transition: return Element::Character;
        case Element::Character: return Element::Action;
        case Element::Dialogue: return Element::Character;
        case Element::Parenthetical: return Element::Dialogue;
        default: return Element::Action;
    }
}

bool isEmptyish(const Paragraph& p) {
    std::u32string t = trim(p.text);
    return t.empty() || (p.type == Element::Parenthetical && t == U"()");
}

std::u32string stripExtension(const std::u32string& name) {
    size_t p = name.find(U'(');
    return trim(p == std::u32string::npos ? name : name.substr(0, p));
}

const size_t kMaxUndo = 300;

}  // namespace

Editor::Editor() { doc_.ensureNotEmpty(); }

void Editor::setDocument(Document d) {
    doc_ = std::move(d);
    doc_.ensureNotEmpty();
    cursor_ = anchor_ = {0, 0};
    undo_.clear();
    redo_.clear();
    lastKind_ = EditKind::None;
    modified_ = false;
    layoutDirty_ = true;
    ++revision_;
}

const Layout& Editor::layout() const {
    if (layoutDirty_) {
        layout_ = computeLayout(doc_, layoutOpt_);
        layoutDirty_ = false;
    }
    return layout_;
}

void Editor::setTitleValue(const std::string& key, const std::string& value) {
    if (doc_.titleValue(key) == value) return;
    beginEdit(EditKind::Other);
    doc_.setTitleValue(key, value);
    changed();
}

void Editor::setAvatar(const std::u32string& name, const std::string& base64) {
    if (doc_.avatar(name) == base64) return;
    beginEdit(EditKind::Other);
    doc_.setAvatar(name, base64);
    changed();
}

void Editor::setScriptLanguage(ScriptLanguage lang) {
    lang_ = lang;
    LayoutOptions o;
    if (lang == ScriptLanguage::Portuguese) {
        o.moreText = U"(MAIS)";
        o.contdText = U"(CONT.)";
    }
    setLayoutOptions(o);
}

int Editor::setCharacter(const std::u32string& oldName, const CharacterInfo& in) {
    CharacterInfo info = in;
    info.name = toUpper(trim(info.name));
    if (info.name.empty()) info.name = oldName;
    const bool rename = info.name != oldName;
    const CharacterInfo* cur = doc_.character(oldName);
    const CharacterInfo before = cur ? *cur : CharacterInfo{oldName, {}, {}, {}, {}, {}, {}};
    if (!rename && before.photo == info.photo && before.age == info.age &&
        before.occupation == info.occupation && before.role == info.role && before.traits == info.traits &&
        before.synopsis == info.synopsis)
        return 0;

    beginEdit(EditKind::Other);
    int renamed = 0;
    if (rename) {
        for (Paragraph& p : doc_.paragraphs) {
            if (p.type != Element::Character) continue;
            const std::u32string t = trim(p.text);
            if (stripExtension(toUpper(t)) != oldName) continue;
            const size_t paren = t.find(U'(');
            p.text = paren == std::u32string::npos ? info.name : info.name + U" " + t.substr(paren);
            ++renamed;
        }
        CharacterInfo gone;
        gone.name = oldName;
        doc_.setCharacter(gone);  // remove a ficha antiga
        doc_.relations.rename(oldName, info.name);
    }
    doc_.setCharacter(info);
    changed();
    return renamed;
}

CharacterStats Editor::characterStats(const std::u32string& name) const {
    CharacterStats st;
    int scene = -1;
    bool speaking = false;
    for (int i = 0; i < static_cast<int>(doc_.paragraphs.size()); ++i) {
        const Paragraph& p = doc_.paragraphs[i];
        if (p.type == Element::SceneHeading) ++scene;
        if (p.type == Element::Character) {
            speaking = stripExtension(toUpper(trim(p.text))) == name;
            if (!speaking) continue;
            ++st.speeches;
            if (st.firstPara < 0) st.firstPara = i;
            if (scene >= 0 && (st.scenes.empty() || st.scenes.back() != scene)) st.scenes.push_back(scene);
        } else if (p.type == Element::Dialogue && speaking) {
            bool in = false;
            for (char32_t c : p.text) {
                const bool w = isWordChar(c);
                if (w && !in) ++st.words;
                in = w;
            }
        } else if (p.type != Element::Parenthetical) {
            speaking = false;
        }
    }
    return st;
}

int Editor::speechCue(int para) const {
    if (para < 0 || para >= static_cast<int>(doc_.paragraphs.size())) return -1;
    int i = para;
    while (i >= 0 && (doc_.paragraphs[i].type == Element::Parenthetical || doc_.paragraphs[i].type == Element::Dialogue))
        --i;
    return i >= 0 && doc_.paragraphs[i].type == Element::Character ? i : -1;
}

bool Editor::isDualDialogue() const {
    const int cue = speechCue(cursor_.para);
    return cue >= 0 && dualSides(doc_)[cue] != 0;
}

bool Editor::toggleDualDialogue() {
    const int cue = speechCue(cursor_.para);
    if (cue < 0) return false;
    const std::vector<int> sides = dualSides(doc_);
    const int n = static_cast<int>(doc_.paragraphs.size());
    int target = -1;
    bool on = true;
    if (sides[cue] == 2) {
        target = cue, on = false;
    } else if (sides[cue] == 1) {
        // Na fala da esquerda: desliga o par desligando a fala da direita.
        target = cue + 1;
        while (target < n && sides[target] == 1) ++target;
        on = false;
    } else {
        // Põe esta fala ao lado da anterior; se não houver, a próxima ao lado desta.
        Document probe = doc_;
        probe.paragraphs[cue].dual = true;
        if (dualSides(probe)[cue] == 2) {
            target = cue;
        } else {
            int next = cue + 1;
            while (next < n && (doc_.paragraphs[next].type == Element::Parenthetical ||
                                doc_.paragraphs[next].type == Element::Dialogue))
                ++next;
            if (next < n && doc_.paragraphs[next].type == Element::Character && sides[next] == 0) {
                probe = doc_;
                probe.paragraphs[next].dual = true;
                if (dualSides(probe)[next] == 2) target = next;
            }
        }
    }
    if (target < 0 || target >= n) return false;
    beginEdit(EditKind::Other);
    doc_.paragraphs[target].dual = on;
    changed();
    return true;
}

// ------------------------------------------------------------ Beat Board

int Editor::addBeat(Beat b) {
    beginEdit(EditKind::Other);
    b.id = doc_.board.nextId();
    doc_.board.beats.push_back(b);
    changed();
    return b.id;
}

void Editor::updateBeats(const std::vector<Beat>& beats) {
    bool any = false;
    for (const Beat& nb : beats)
        for (const Beat& ob : doc_.board.beats)
            if (ob.id == nb.id) {
                const bool sameImage = ob.image == nb.image || (ob.image && nb.image && *ob.image == *nb.image);
                if (ob.x != nb.x || ob.y != nb.y || ob.w != nb.w || ob.h != nb.h || ob.color != nb.color ||
                    ob.title != nb.title || ob.text != nb.text || !sameImage)
                    any = true;
            }
    if (!any) return;
    beginEdit(EditKind::Other);
    for (const Beat& nb : beats)
        for (Beat& ob : doc_.board.beats)
            if (ob.id == nb.id) ob = nb;
    changed();
}

void Editor::removeBeats(const std::vector<int>& ids) {
    auto gone = [&](int id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };
    auto& bs = doc_.board.beats;
    if (std::none_of(bs.begin(), bs.end(), [&](const Beat& b) { return gone(b.id); })) return;
    beginEdit(EditKind::Other);
    bs.erase(std::remove_if(bs.begin(), bs.end(), [&](const Beat& b) { return gone(b.id); }), bs.end());
    auto& fs = doc_.board.flows;
    fs.erase(std::remove_if(fs.begin(), fs.end(), [&](const FlowLine& f) { return gone(f.from) || gone(f.to); }),
             fs.end());
    changed();
}

bool Editor::addFlow(int from, int to) {
    const BeatBoard& bb = doc_.board;
    if (from == to || !bb.beat(from) || !bb.beat(to)) return false;
    for (const FlowLine& f : bb.flows)
        if ((f.from == from && f.to == to) || (f.from == to && f.to == from)) return false;
    beginEdit(EditKind::Other);
    doc_.board.flows.push_back({from, to});
    changed();
    return true;
}

void Editor::removeFlows(const std::vector<FlowLine>& flows) {
    auto& fs = doc_.board.flows;
    auto gone = [&](const FlowLine& f) { return std::find(flows.begin(), flows.end(), f) != flows.end(); };
    if (std::none_of(fs.begin(), fs.end(), gone)) return;
    beginEdit(EditKind::Other);
    fs.erase(std::remove_if(fs.begin(), fs.end(), gone), fs.end());
    changed();
}

void Editor::setRelations(const RelationMap& map) {
    auto sameNodes = [](const std::vector<CharacterNode>& x, const std::vector<CharacterNode>& y) {
        if (x.size() != y.size()) return false;
        for (size_t i = 0; i < x.size(); ++i)
            if (x[i].name != y[i].name || x[i].x != y[i].x || x[i].y != y[i].y) return false;
        return true;
    };
    auto sameLinks = [](const std::vector<Relationship>& x, const std::vector<Relationship>& y) {
        if (x.size() != y.size()) return false;
        for (size_t i = 0; i < x.size(); ++i)
            if (x[i].a != y[i].a || x[i].b != y[i].b || x[i].kind != y[i].kind || x[i].label != y[i].label)
                return false;
        return true;
    };
    if (sameNodes(map.nodes, doc_.relations.nodes) && sameLinks(map.links, doc_.relations.links)) return;
    beginEdit(EditKind::Other);
    doc_.relations = map;
    changed();
}

void Editor::setStructure(const StoryStructure& st) {
    if (st == doc_.structure) return;
    beginEdit(EditKind::Other);
    doc_.structure = st;
    changed();
}

// ------------------------------------------------------------ undo

void Editor::beginEdit(EditKind kind) {
    auto now = std::chrono::steady_clock::now();
    bool coalesce = kind != EditKind::Other && kind == lastKind_ &&
                    now - lastEditTime_ < std::chrono::milliseconds(1500) && !undo_.empty();
    if (!coalesce) {
        undo_.push_back({doc_.paragraphs, doc_.titlePage, doc_.cast, doc_.board, doc_.relations, doc_.structure, cursor_, anchor_});
        if (undo_.size() > kMaxUndo) undo_.erase(undo_.begin());
    }
    redo_.clear();
    lastKind_ = kind;
    lastEditTime_ = now;
}

void Editor::changed() {
    doc_.ensureNotEmpty();
    clampCursor();
    layoutDirty_ = true;
    modified_ = true;
    ++revision_;
}

void Editor::undo() {
    if (undo_.empty()) return;
    redo_.push_back({doc_.paragraphs, doc_.titlePage, doc_.cast, doc_.board, doc_.relations, doc_.structure, cursor_, anchor_});
    Snapshot s = std::move(undo_.back());
    undo_.pop_back();
    doc_.paragraphs = std::move(s.paragraphs);
    doc_.titlePage = std::move(s.titlePage);
    doc_.cast = std::move(s.cast);
    doc_.board = std::move(s.board);
    doc_.relations = std::move(s.relations);
    doc_.structure = std::move(s.structure);
    cursor_ = s.cursor;
    anchor_ = s.anchor;
    lastKind_ = EditKind::None;
    changed();
}

void Editor::redo() {
    if (redo_.empty()) return;
    undo_.push_back({doc_.paragraphs, doc_.titlePage, doc_.cast, doc_.board, doc_.relations, doc_.structure, cursor_, anchor_});
    Snapshot s = std::move(redo_.back());
    redo_.pop_back();
    doc_.paragraphs = std::move(s.paragraphs);
    doc_.titlePage = std::move(s.titlePage);
    doc_.cast = std::move(s.cast);
    doc_.board = std::move(s.board);
    doc_.relations = std::move(s.relations);
    doc_.structure = std::move(s.structure);
    cursor_ = s.cursor;
    anchor_ = s.anchor;
    lastKind_ = EditKind::None;
    changed();
}

// ------------------------------------------------------------ cursor

void Editor::clampCursor() {
    auto clamp = [&](Pos& p) {
        p.para = std::clamp(p.para, 0, static_cast<int>(doc_.paragraphs.size()) - 1);
        p.off = std::clamp(p.off, 0, static_cast<int>(doc_.paragraphs[p.para].text.size()));
    };
    clamp(cursor_);
    clamp(anchor_);
}

std::pair<Pos, Pos> Editor::selection() const {
    return anchor_ < cursor_ ? std::make_pair(anchor_, cursor_) : std::make_pair(cursor_, anchor_);
}

void Editor::moveTo(Pos p, bool select, bool keepColumn) {
    cursor_ = p;
    if (!select) anchor_ = p;
    clampCursor();
    if (!keepColumn) preferredCol_ = -1;
    lastKind_ = EditKind::None;
}

void Editor::setCursor(Pos p, bool select) { moveTo(p, select); }

void Editor::selectAll() {
    anchor_ = {0, 0};
    int last = static_cast<int>(doc_.paragraphs.size()) - 1;
    cursor_ = {last, static_cast<int>(doc_.paragraphs[last].text.size())};
    lastKind_ = EditKind::None;
}

void Editor::selectWordAt(Pos p) {
    const auto& t = doc_.paragraphs[p.para].text;
    int a = p.off, b = p.off;
    while (a > 0 && isWordChar(t[a - 1])) --a;
    while (b < static_cast<int>(t.size()) && isWordChar(t[b])) ++b;
    anchor_ = {p.para, a};
    cursor_ = {p.para, b};
    lastKind_ = EditKind::None;
}

int Editor::lineOf(Pos p) const {
    const Layout& L = layout();
    const auto& idx = L.paraLines[p.para];
    int best = idx.front();
    for (int li : idx)
        if (L.lines[li].start <= p.off) best = li;
    return best;
}

int Editor::columnOf(Pos p) const {
    const Layout& L = layout();
    const LayoutLine& l = L.lines[lineOf(p)];
    return l.col + (p.off - l.start);
}

Pos Editor::posAtLine(int line, int col) const {
    const Layout& L = layout();
    const LayoutLine& l = L.lines[line];
    const auto& idx = L.paraLines[l.para];
    const bool lastOfPara = idx.back() == line;
    int maxOff = l.end - l.start;
    if (!lastOfPara && maxOff > 0) maxOff -= 1;
    int off = std::clamp(col - l.col, 0, maxOff);
    return {l.para, l.start + off};
}

void Editor::moveLeft(bool select, bool word) {
    if (!select && hasSelection()) { moveTo(selection().first, false); return; }
    Pos p = cursor_;
    if (p.off == 0) {
        if (p.para > 0) p = {p.para - 1, static_cast<int>(doc_.paragraphs[p.para - 1].text.size())};
    } else if (word) {
        const auto& t = doc_.paragraphs[p.para].text;
        while (p.off > 0 && !isWordChar(t[p.off - 1])) --p.off;
        while (p.off > 0 && isWordChar(t[p.off - 1])) --p.off;
    } else {
        --p.off;
    }
    moveTo(p, select);
}

void Editor::moveRight(bool select, bool word) {
    if (!select && hasSelection()) { moveTo(selection().second, false); return; }
    Pos p = cursor_;
    const auto& t = doc_.paragraphs[p.para].text;
    const int len = static_cast<int>(t.size());
    if (p.off >= len) {
        if (p.para + 1 < static_cast<int>(doc_.paragraphs.size())) p = {p.para + 1, 0};
    } else if (word) {
        while (p.off < len && isWordChar(t[p.off])) ++p.off;
        while (p.off < len && !isWordChar(t[p.off])) ++p.off;
    } else {
        ++p.off;
    }
    moveTo(p, select);
}

void Editor::moveVertical(int lines, bool select) {
    const Layout& L = layout();
    if (preferredCol_ < 0) preferredCol_ = columnOf(cursor_);
    int li = lineOf(cursor_);
    const int step = lines > 0 ? 1 : -1;
    int remaining = std::abs(lines);
    int target = li;
    for (int k = li + step; k >= 0 && k < static_cast<int>(L.lines.size()) && remaining > 0; k += step) {
        if (L.lines[k].para < 0) continue;  // linhas virtuais (MAIS)/(CONT.)
        target = k;
        --remaining;
    }
    if (target == li) {
        // Já na primeira/última linha: vai para o início/fim.
        Pos p = lines < 0 ? Pos{0, 0}
                          : Pos{static_cast<int>(doc_.paragraphs.size()) - 1,
                                static_cast<int>(doc_.paragraphs.back().text.size())};
        moveTo(p, select, true);
        return;
    }
    int col = preferredCol_;
    moveTo(posAtLine(target, col), select, true);
    preferredCol_ = col;
}

void Editor::moveUp(bool select) { moveVertical(-1, select); }
void Editor::moveDown(bool select) { moveVertical(1, select); }
void Editor::movePage(int dir, bool select) { moveVertical(dir * 40, select); }

void Editor::moveLineStart(bool select) {
    const LayoutLine& l = layout().lines[lineOf(cursor_)];
    moveTo({cursor_.para, l.start}, select);
}

void Editor::moveLineEnd(bool select) {
    moveTo(posAtLine(lineOf(cursor_), 1 << 20), select);
}

void Editor::moveDocStart(bool select) { moveTo({0, 0}, select); }

void Editor::moveDocEnd(bool select) {
    int last = static_cast<int>(doc_.paragraphs.size()) - 1;
    moveTo({last, static_cast<int>(doc_.paragraphs[last].text.size())}, select);
}

// ------------------------------------------------------------ edição

void Editor::insertParagraph(int index, Element type, const std::u32string& text) {
    doc_.paragraphs.insert(doc_.paragraphs.begin() + index, Paragraph{type, text});
}

void Editor::deleteSelection() {
    if (!hasSelection()) return;
    auto [a, b] = selection();
    auto& pa = doc_.paragraphs[a.para];
    if (a.para == b.para) {
        pa.text.erase(a.off, b.off - a.off);
    } else {
        std::u32string tail = doc_.paragraphs[b.para].text.substr(b.off);
        pa.text = pa.text.substr(0, a.off) + tail;
        doc_.paragraphs.erase(doc_.paragraphs.begin() + a.para + 1,
                              doc_.paragraphs.begin() + b.para + 1);
    }
    cursor_ = anchor_ = a;
}

void Editor::insertText(const std::u32string& in) {
    std::u32string s;
    for (char32_t c : in) {
        if (c == U'\n' || c == U'\r') { paste(u32ToUtf8(in)); return; }
        if (c == U'\t') c = U' ';
        if (c >= 0x20 && c != 0x7F) s.push_back(c);
    }
    if (s.empty()) return;

    Paragraph& p = cur();
    // Fechar parêntese que já existe: apenas pula.
    if (!hasSelection() && s == U")" && cursor_.off < static_cast<int>(p.text.size()) &&
        p.text[cursor_.off] == U')') {
        moveTo({cursor_.para, cursor_.off + 1}, false);
        return;
    }
    beginEdit(EditKind::Typing);
    deleteSelection();
    cur().text.insert(cursor_.off, s);
    cursor_.off += static_cast<int>(s.size());
    anchor_ = cursor_;
    preferredCol_ = -1;
    changed();
}

void Editor::enter() {
    if (acceptSuggestion()) { /* aceitou e continua para o próximo elemento */ }
    beginEdit(EditKind::Other);
    deleteSelection();
    Paragraph& p = cur();

    if (isEmptyish(p)) {
        // Enter num elemento vazio troca o tipo (sem criar linhas em branco).
        p.text.clear();
        p.type = p.type == Element::Action ? Element::SceneHeading : Element::Action;
        cursor_.off = 0;
        anchor_ = cursor_;
        changed();
        return;
    }

    const int len = static_cast<int>(p.text.size());
    bool atEnd = cursor_.off == len ||
                 (p.type == Element::Parenthetical && cursor_.off == len - 1 && p.text.back() == U')');
    // Espaços sobrando no fim também contam como "fim".
    if (!atEnd) {
        bool onlySpaces = true;
        for (int k = cursor_.off; k < len; ++k)
            if (!isSpace(p.text[k])) { onlySpaces = false; break; }
        atEnd = onlySpaces;
    }

    if (atEnd) {
        Element next = nextOnEnter(p.type);
        std::u32string text = next == Element::Parenthetical ? U"()" : U"";
        insertParagraph(cursor_.para + 1, next, text);
        cursor_ = {cursor_.para + 1, next == Element::Parenthetical ? 1 : 0};
    } else if (cursor_.off == 0) {
        insertParagraph(cursor_.para, p.type, U"");
        cursor_ = {cursor_.para + 1, 0};
    } else {
        std::u32string tail = p.text.substr(cursor_.off);
        p.text.erase(cursor_.off);
        Element t = p.type;
        insertParagraph(cursor_.para + 1, t, tail);
        cursor_ = {cursor_.para + 1, 0};
    }
    anchor_ = cursor_;
    preferredCol_ = -1;
    changed();
}

void Editor::tab(bool shift) {
    if (!shift && acceptSuggestion()) return;
    Paragraph& p = cur();
    const int len = static_cast<int>(p.text.size());
    const bool atEnd = cursor_.off >= len - (p.type == Element::Parenthetical ? 1 : 0);

    if (shift) { setElement(prevOnTab(p.type)); return; }

    if (!isEmptyish(p) && atEnd &&
        (p.type == Element::Dialogue || p.type == Element::Character)) {
        beginEdit(EditKind::Other);
        insertParagraph(cursor_.para + 1, Element::Parenthetical, U"()");
        cursor_ = anchor_ = {cursor_.para + 1, 1};
        changed();
        return;
    }
    if (!isEmptyish(p) && atEnd && p.type == Element::Parenthetical) {
        beginEdit(EditKind::Other);
        insertParagraph(cursor_.para + 1, Element::Dialogue, U"");
        cursor_ = anchor_ = {cursor_.para + 1, 0};
        changed();
        return;
    }
    setElement(nextOnTab(p.type));
}

void Editor::setElement(Element e) {
    Paragraph& p = cur();
    if (p.type == e) return;
    beginEdit(EditKind::Other);
    if (p.type == Element::Parenthetical) {
        std::u32string t = trim(p.text);
        if (t.size() >= 2 && t.front() == U'(' && t.back() == U')') {
            p.text = t.substr(1, t.size() - 2);
            cursor_.off = std::max(0, cursor_.off - 1);
        }
    }
    if (e == Element::Parenthetical) {
        std::u32string t = trim(p.text);
        if (t.empty()) {
            p.text = U"()";
            cursor_.off = 1;
        } else if (t.front() != U'(') {
            p.text = U"(" + t + U")";
            cursor_.off = std::min<int>(cursor_.off + 1, static_cast<int>(p.text.size()) - 1);
        }
    }
    p.type = e;
    anchor_ = cursor_;
    changed();
}

void Editor::backspace(bool word) {
    if (hasSelection()) {
        beginEdit(EditKind::Other);
        deleteSelection();
        changed();
        return;
    }
    Paragraph& p = cur();
    if (cursor_.off > 0) {
        beginEdit(EditKind::Deleting);
        if (p.type == Element::Parenthetical && p.text == U"()") {
            p.text.clear();
            cursor_.off = 0;
        } else {
            int from = cursor_.off - 1;
            if (word) {
                from = cursor_.off;
                while (from > 0 && !isWordChar(p.text[from - 1])) --from;
                while (from > 0 && isWordChar(p.text[from - 1])) --from;
            }
            p.text.erase(from, cursor_.off - from);
            cursor_.off = from;
        }
        anchor_ = cursor_;
        changed();
        return;
    }
    if (cursor_.para == 0) return;
    beginEdit(EditKind::Other);
    Paragraph& prev = doc_.paragraphs[cursor_.para - 1];
    if (isEmptyish(p)) {
        doc_.paragraphs.erase(doc_.paragraphs.begin() + cursor_.para);
        --cursor_.para;
        cursor_.off = static_cast<int>(doc_.paragraphs[cursor_.para].text.size());
    } else if (isEmptyish(prev)) {
        doc_.paragraphs.erase(doc_.paragraphs.begin() + cursor_.para - 1);
        --cursor_.para;
        cursor_.off = 0;
    } else {
        int at = static_cast<int>(prev.text.size());
        prev.text += p.text;
        doc_.paragraphs.erase(doc_.paragraphs.begin() + cursor_.para);
        --cursor_.para;
        cursor_.off = at;
    }
    anchor_ = cursor_;
    changed();
}

void Editor::deleteForward(bool word) {
    if (hasSelection()) {
        beginEdit(EditKind::Other);
        deleteSelection();
        changed();
        return;
    }
    Paragraph& p = cur();
    const int len = static_cast<int>(p.text.size());
    if (cursor_.off < len) {
        beginEdit(EditKind::Deleting);
        int to = cursor_.off + 1;
        if (word) {
            to = cursor_.off;
            while (to < len && isWordChar(p.text[to])) ++to;
            while (to < len && !isWordChar(p.text[to])) ++to;
        }
        p.text.erase(cursor_.off, to - cursor_.off);
        changed();
        return;
    }
    if (cursor_.para + 1 >= static_cast<int>(doc_.paragraphs.size())) return;
    beginEdit(EditKind::Other);
    if (isEmptyish(p)) {
        doc_.paragraphs.erase(doc_.paragraphs.begin() + cursor_.para);
        cursor_.off = 0;
    } else {
        p.text += doc_.paragraphs[cursor_.para + 1].text;
        doc_.paragraphs.erase(doc_.paragraphs.begin() + cursor_.para + 1);
    }
    anchor_ = cursor_;
    changed();
}

std::string Editor::copySelection() const {
    if (!hasSelection()) return {};
    auto [a, b] = selection();
    if (a.para == b.para)
        return u32ToUtf8(doc_.paragraphs[a.para].text.substr(a.off, b.off - a.off));
    Document part;
    for (int i = a.para; i <= b.para; ++i) {
        Paragraph p = doc_.paragraphs[i];
        if (i == b.para) p.text = p.text.substr(0, b.off);
        if (i == a.para) p.text = p.text.substr(a.off);
        part.paragraphs.push_back(p);
    }
    std::string s = writeFountain(part, false);
    while (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

void Editor::paste(const std::string& utf8) {
    if (utf8.empty()) return;
    if (utf8.find('\n') == std::string::npos && utf8.find('\r') == std::string::npos) {
        insertText(utf8ToU32(utf8));
        return;
    }
    Document frag = parseFountain(utf8, false);
    if (frag.paragraphs.empty()) return;

    beginEdit(EditKind::Other);
    deleteSelection();
    Paragraph& p = cur();
    std::u32string tail = p.text.substr(cursor_.off);
    p.text.erase(cursor_.off);
    if (trim(p.text).empty()) p.type = frag.paragraphs[0].type;
    p.text += frag.paragraphs[0].text;
    int idx = cursor_.para;
    for (size_t k = 1; k < frag.paragraphs.size(); ++k) {
        ++idx;
        doc_.paragraphs.insert(doc_.paragraphs.begin() + idx, frag.paragraphs[k]);
    }
    Paragraph& last = doc_.paragraphs[idx];
    cursor_ = {idx, static_cast<int>(last.text.size())};
    last.text += tail;
    anchor_ = cursor_;
    changed();
}

// ------------------------------------------------------------ autocompletar

std::vector<std::u32string> Editor::candidates() const {
    std::vector<std::u32string> out;
    const Paragraph& p = doc_.paragraphs[cursor_.para];
    auto addUnique = [&](const std::u32string& s) {
        if (s.empty()) return;
        if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
    };

    if (p.type == Element::Character) {
        std::map<std::u32string, int> count;
        for (size_t i = 0; i < doc_.paragraphs.size(); ++i) {
            if (static_cast<int>(i) == cursor_.para) continue;
            const auto& q = doc_.paragraphs[i];
            if (q.type != Element::Character) continue;
            std::u32string name = toUpper(trim(q.text));
            if (name.empty()) continue;
            count[stripExtension(name)] += 1;
        }
        std::vector<std::pair<int, std::u32string>> sorted;
        for (auto& kv : count) sorted.push_back({-kv.second, kv.first});
        std::sort(sorted.begin(), sorted.end());
        for (auto& s : sorted) addUnique(s.second);
        std::u32string typed = toUpper(p.text);
        // Extensões depois do nome: "JOÃO (" -> "JOÃO (V.O.)"
        size_t paren = typed.find(U'(');
        if (paren != std::u32string::npos) {
            std::u32string base = typed.substr(0, paren);
            const bool pt = lang_ == ScriptLanguage::Portuguese;
            for (const char32_t* ext : {U"(V.O.)", U"(O.S.)", pt ? U"(CONT.)" : U"(CONT'D)", U"(O.C.)"})
                addUnique(base + ext);
        }
    } else if (p.type == Element::SceneHeading) {
        std::u32string typed = toUpper(p.text);
        size_t dash = typed.rfind(U" - ");
        if (dash != std::u32string::npos) {
            std::u32string base = typed.substr(0, dash + 3);
            static const std::vector<std::u32string> en = {
                U"DAY", U"NIGHT", U"MORNING", U"AFTERNOON", U"EVENING", U"DAWN",
                U"DUSK", U"CONTINUOUS", U"LATER", U"MOMENTS LATER"};
            static const std::vector<std::u32string> pt = {
                U"DIA", U"NOITE", U"TARDE", U"MANHÃ", U"AMANHECER",
                U"ANOITECER", U"CONTÍNUO", U"MAIS TARDE", U"MOMENTOS DEPOIS"};
            for (const auto& t : lang_ == ScriptLanguage::Portuguese ? pt : en) addUnique(base + t);
        } else {
            for (const char32_t* t : {U"INT. ", U"EXT. ", U"INT./EXT. "}) addUnique(t);
            for (size_t i = 0; i < doc_.paragraphs.size(); ++i) {
                if (static_cast<int>(i) == cursor_.para) continue;
                const auto& q = doc_.paragraphs[i];
                if (q.type != Element::SceneHeading) continue;
                std::u32string h = toUpper(trim(q.text));
                size_t d = h.rfind(U" - ");
                addUnique(d == std::u32string::npos ? h : h.substr(0, d));
            }
        }
    } else if (p.type == Element::Transition) {
        static const std::vector<std::u32string> en = {
            U"CUT TO:", U"SMASH CUT TO:", U"DISSOLVE TO:", U"MATCH CUT TO:",
            U"FADE IN:", U"FADE OUT.", U"FADE TO BLACK."};
        static const std::vector<std::u32string> pt = {
            U"CORTA PARA:", U"CORTE SECO PARA:", U"FUSÃO PARA:", U"FADE IN:",
            U"FADE OUT.", U"FADE PARA PRETO.", U"CUT TO:", U"DISSOLVE TO:"};
        for (const auto& t : lang_ == ScriptLanguage::Portuguese ? pt : en) addUnique(t);
    }
    return out;
}

std::u32string Editor::suggestion() const {
    if (hasSelection()) return {};
    const Paragraph& p = doc_.paragraphs[cursor_.para];
    if (cursor_.off != static_cast<int>(p.text.size()) || p.text.empty()) return {};
    if (p.type != Element::Character && p.type != Element::SceneHeading &&
        p.type != Element::Transition)
        return {};
    for (const auto& c : candidates()) {
        if (c.size() <= p.text.size() || !startsWithNoCase(c, p.text)) continue;
        std::u32string rest = c.substr(p.text.size());
        if (trim(rest).empty()) continue;
        return rest;
    }
    return {};
}

bool Editor::acceptSuggestion() {
    std::u32string s = suggestion();
    if (s.empty()) return false;
    beginEdit(EditKind::Other);
    cur().text += s;
    cursor_.off = static_cast<int>(cur().text.size());
    anchor_ = cursor_;
    changed();
    return true;
}

// ------------------------------------------------------------ localizar

namespace {
int findNoCase(const std::u32string& hay, const std::u32string& needle, int from) {
    if (needle.empty()) return -1;
    for (int i = std::max(0, from); i + static_cast<int>(needle.size()) <= static_cast<int>(hay.size()); ++i) {
        bool ok = true;
        for (size_t k = 0; k < needle.size(); ++k)
            if (toUpper(hay[i + k]) != toUpper(needle[k])) { ok = false; break; }
        if (ok) return i;
    }
    return -1;
}
int rfindNoCase(const std::u32string& hay, const std::u32string& needle, int before) {
    int last = -1, i = 0;
    while ((i = findNoCase(hay, needle, i)) >= 0 && i + static_cast<int>(needle.size()) <= before) {
        last = i;
        ++i;
    }
    return last;
}
}  // namespace

bool Editor::find(const std::u32string& q, bool forward) {
    if (q.empty()) return false;
    const int n = static_cast<int>(doc_.paragraphs.size());
    auto [selA, selB] = selection();
    if (forward) {
        Pos start = selB;
        for (int k = 0; k <= n; ++k) {
            int para = (start.para + k) % n;
            int from = (k == 0) ? start.off : 0;
            int at = findNoCase(doc_.paragraphs[para].text, q, from);
            if (at >= 0) {
                anchor_ = {para, at};
                cursor_ = {para, at + static_cast<int>(q.size())};
                lastKind_ = EditKind::None;
                return true;
            }
        }
    } else {
        Pos start = selA;
        for (int k = 0; k <= n; ++k) {
            int para = ((start.para - k) % n + n) % n;
            const auto& t = doc_.paragraphs[para].text;
            int before = (k == 0) ? start.off : static_cast<int>(t.size());
            int at = rfindNoCase(t, q, before);
            if (at >= 0) {
                anchor_ = {para, at};
                cursor_ = {para, at + static_cast<int>(q.size())};
                lastKind_ = EditKind::None;
                return true;
            }
        }
    }
    return false;
}

bool Editor::replaceCurrent(const std::u32string& q, const std::u32string& rep) {
    if (hasSelection()) {
        auto [a, b] = selection();
        if (a.para == b.para && b.off - a.off == static_cast<int>(q.size()) &&
            findNoCase(doc_.paragraphs[a.para].text, q, a.off) == a.off) {
            beginEdit(EditKind::Other);
            doc_.paragraphs[a.para].text.replace(a.off, q.size(), rep);
            cursor_ = anchor_ = {a.para, a.off + static_cast<int>(rep.size())};
            changed();
        }
    }
    return find(q, true);
}

int Editor::replaceAll(const std::u32string& q, const std::u32string& rep) {
    if (q.empty()) return 0;
    int count = 0;
    bool began = false;
    for (auto& p : doc_.paragraphs) {
        int i = 0;
        while ((i = findNoCase(p.text, q, i)) >= 0) {
            if (!began) { beginEdit(EditKind::Other); began = true; }
            p.text.replace(i, q.size(), rep);
            i += static_cast<int>(rep.size());
            ++count;
        }
    }
    if (began) changed();
    return count;
}

// ------------------------------------------------------------ informações

std::vector<int> Editor::scenes() const {
    std::vector<int> out;
    for (size_t i = 0; i < doc_.paragraphs.size(); ++i)
        if (doc_.paragraphs[i].type == Element::SceneHeading) out.push_back(static_cast<int>(i));
    return out;
}

int Editor::sceneIndexAt(int para) const {
    int idx = -1;
    for (int i = 0; i <= para && i < static_cast<int>(doc_.paragraphs.size()); ++i)
        if (doc_.paragraphs[i].type == Element::SceneHeading) ++idx;
    return idx;
}

std::vector<std::pair<std::u32string, int>> Editor::characters() const {
    std::map<std::u32string, int> count;
    for (const auto& p : doc_.paragraphs) {
        if (p.type != Element::Character) continue;
        std::u32string n = stripExtension(toUpper(trim(p.text)));
        if (!n.empty()) count[n] += 1;
    }
    std::vector<std::pair<std::u32string, int>> out(count.begin(), count.end());
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    return out;
}

int Editor::wordCount() const {
    int words = 0;
    for (const auto& p : doc_.paragraphs) {
        bool in = false;
        for (char32_t c : p.text) {
            bool w = isWordChar(c);
            if (w && !in) ++words;
            in = w;
        }
    }
    return words;
}

// ------------------------------------------------------------ cenas

std::vector<SceneSummary> Editor::sceneSummaries() const {
    std::vector<SceneSummary> out;
    const Layout& L = layout();
    const auto& ps = doc_.paragraphs;
    const auto heads = scenes();
    auto absRow = [&](int lineIndex) {
        const LayoutLine& l = L.lines[lineIndex];
        return l.page * page::kLinesPerPage + l.row;
    };
    // A última linha guardada pode ser da coluna da direita de um diálogo duplo: usa a mais baixa.
    int endRowOfDoc = 0;
    for (int li = 0; li < static_cast<int>(L.lines.size()); ++li) endRowOfDoc = std::max(endRowOfDoc, absRow(li) + 1);

    for (size_t k = 0; k < heads.size(); ++k) {
        const int h = heads[k];
        const int next = k + 1 < heads.size() ? heads[k + 1] : static_cast<int>(ps.size());
        SceneSummary s;
        s.para = h;
        s.heading = trim(displayText(ps[h]));
        s.synopsis = trim(ps[h].synopsis);
        for (int i = h + 1; i < next; ++i) {
            const Paragraph& p = ps[i];
            if (p.type == Element::Action && s.preview.size() < 220) {
                if (!s.preview.empty()) s.preview += U" ";
                s.preview += trim(p.text);
            } else if (p.type == Element::Character) {
                std::u32string n = stripExtension(toUpper(trim(p.text)));
                if (!n.empty() && std::find(s.cast.begin(), s.cast.end(), n) == s.cast.end())
                    s.cast.push_back(n);
            }
        }
        const int startLine = L.paraLines[h].front();
        s.page = L.lines[startLine].page;
        const int startRow = absRow(startLine);
        const int endRow = next < static_cast<int>(ps.size()) ? absRow(L.paraLines[next].front()) : endRowOfDoc;
        // 1/8 de página = 54/8 linhas; arredonda e nunca fica abaixo de 1/8.
        s.eighths = std::max(1, static_cast<int>((endRow - startRow) * 8.0 / page::kLinesPerPage + 0.5));
        out.push_back(std::move(s));
    }
    return out;
}

void Editor::setSynopsis(int para, const std::u32string& synopsis) {
    if (para < 0 || para >= static_cast<int>(doc_.paragraphs.size())) return;
    std::u32string t = trim(synopsis);
    for (auto& c : t)
        if (c == U'\n' || c == U'\r' || c == U'\t') c = U' ';
    if (doc_.paragraphs[para].synopsis == t) return;
    beginEdit(EditKind::Other);
    doc_.paragraphs[para].synopsis = t;
    changed();
}

int Editor::moveScene(int from, int to) {
    const auto heads = scenes();
    const int n = static_cast<int>(heads.size());
    if (from < 0 || from >= n || to < 0 || to > n || to == from || to == from + 1) return from;

    auto blockStart = [&](int k) { return heads[k]; };
    auto blockEnd = [&](int k) {  // exclusivo
        return k + 1 < n ? heads[k + 1] : static_cast<int>(doc_.paragraphs.size());
    };

    beginEdit(EditKind::Other);
    std::vector<Paragraph> block(doc_.paragraphs.begin() + blockStart(from),
                                 doc_.paragraphs.begin() + blockEnd(from));
    // Posição de inserção calculada no documento original, depois ajustada pela remoção.
    int insertAt = to < n ? blockStart(to) : static_cast<int>(doc_.paragraphs.size());
    const int removed = static_cast<int>(block.size());
    doc_.paragraphs.erase(doc_.paragraphs.begin() + blockStart(from),
                          doc_.paragraphs.begin() + blockEnd(from));
    if (insertAt > blockStart(from)) insertAt -= removed;
    doc_.paragraphs.insert(doc_.paragraphs.begin() + insertAt, block.begin(), block.end());

    cursor_ = anchor_ = {insertAt, 0};
    preferredCol_ = -1;
    changed();
    return to > from ? to - 1 : to;
}
