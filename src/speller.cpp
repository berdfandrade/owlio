#include "speller.h"

#include <QFile>

namespace {
// Carregado na primeira palavra conferida (uns 100 ms para o português).
const SpellChecker& dictionary(ScriptLanguage lang) {
    static SpellChecker checkers[2];
    const int i = lang == ScriptLanguage::Portuguese ? 1 : 0;
    if (!checkers[i].loaded()) {
        const QString base = i == 1 ? QStringLiteral(":/dict/pt_BR") : QStringLiteral(":/dict/en_US");
        QFile aff(base + ".aff"), dic(base + ".dic");
        if (aff.open(QIODevice::ReadOnly) && dic.open(QIODevice::ReadOnly))
            checkers[i].load(aff.readAll().toStdString(), dic.readAll().toStdString());
    }
    return checkers[i];
}

std::u32string lowered(std::u32string w) {
    for (char32_t& c : w) c = toLower(c);
    return w;
}

bool sceneWord(const std::u32string& lower) { return lower == U"int" || lower == U"ext" || lower == U"est"; }
}  // namespace

void Speller::syncNames() {
    if (namesRevision_ == ed_.revision()) return;
    namesRevision_ = ed_.revision();
    names_.clear();
    auto add = [this](const std::u32string& text) {
        for (const WordSpan& w : wordSpans(text)) names_.insert(lowered(text.substr(w.start, w.len)));
    };
    for (const Paragraph& p : ed_.doc().paragraphs)
        if (p.type == Element::Character) add(p.text);
    for (const CharacterInfo& c : ed_.doc().cast) add(c.name);
}

bool Speller::unknown(ScriptLanguage lang, const std::u32string& word) {
    const SpellChecker& dict = dictionary(lang);
    if (!dict.loaded()) return false;
    auto& cache = cache_[lang == ScriptLanguage::Portuguese ? 1 : 0];
    auto it = cache.find(word);
    if (it == cache.end()) {
        if (cache.size() >= kMaxCached) cache.clear();  // teto: um roteiro enorme não acumula sem fim
        it = cache.emplace(word, !dict.check(word)).first;
    }
    return it->second;
}

ScriptLanguage Speller::dictionaryLanguage() {
    if (language_ == Language::Portuguese) return ScriptLanguage::Portuguese;
    if (language_ == Language::English) return ScriptLanguage::English;
    if (detectedRevision_ == ed_.revision()) return detected_;
    detectedRevision_ = ed_.revision();
    // Até 400 palavras de ação e diálogo; em caso de empate (ou sem texto), vale a convenção do roteiro.
    int pt = 0, en = 0, seen = 0;
    for (const Paragraph& p : ed_.doc().paragraphs) {
        if (p.type != Element::Action && p.type != Element::Dialogue) continue;
        for (const WordSpan& w : wordSpans(p.text)) {
            if (w.len < 3) continue;
            const std::u32string word = p.text.substr(w.start, w.len);
            pt += !unknown(ScriptLanguage::Portuguese, word);
            en += !unknown(ScriptLanguage::English, word);
            if (++seen >= 400) break;
        }
        if (seen >= 400) break;
    }
    detected_ = pt == en ? ed_.scriptLanguage() : pt > en ? ScriptLanguage::Portuguese : ScriptLanguage::English;
    return detected_;
}

bool Speller::misspelled(const std::u32string& word) {
    if (!enabled_ || !unknown(dictionaryLanguage(), word)) return false;
    syncNames();
    const std::u32string lower = lowered(word);
    return !sceneWord(lower) && !names_.count(lower) && !ed_.isCustomWord(word);
}

namespace {
// Deixas de personagem e transições nunca são conferidas (são nomes e termos técnicos).
bool checked(Element type) { return type != Element::Character && type != Element::Transition; }
}  // namespace

std::vector<std::u32string> Speller::suggestions(const std::u32string& word) {
    return dictionary(dictionaryLanguage()).suggest(word);
}

std::vector<Editor::TextFix> Speller::corrections(std::vector<std::u32string>* unfixable) {
    std::vector<Editor::TextFix> out;
    if (!enabled_) return out;
    std::unordered_map<std::u32string, std::u32string> best;  // a mesma palavra errada várias vezes: uma busca
    const auto& paragraphs = ed_.doc().paragraphs;
    for (int i = 0; i < static_cast<int>(paragraphs.size()); ++i) {
        if (!checked(paragraphs[i].type)) continue;
        const std::u32string& text = paragraphs[i].text;
        for (const WordSpan& w : wordSpans(text)) {
            const std::u32string word = text.substr(w.start, w.len);
            if (!misspelled(word)) continue;
            auto it = best.find(word);
            if (it == best.end()) {
                const std::vector<std::u32string> s = suggestions(word);
                it = best.emplace(word, s.empty() ? std::u32string() : s.front()).first;
                if (s.empty() && unfixable) unfixable->push_back(word);
            }
            if (!it->second.empty()) out.push_back({i, w.start, w.len, word, it->second});
        }
    }
    return out;
}

std::vector<WordSpan> Speller::marks(int line) {
    std::vector<WordSpan> out;
    if (!enabled_) return out;
    const Layout& L = ed_.layout();
    if (line < 0 || line >= static_cast<int>(L.lines.size())) return out;
    const LayoutLine& l = L.lines[line];
    if (l.para < 0) return out;
    if (!checked(ed_.doc().paragraphs[l.para].type)) return out;
    const std::u32string text = L.lineText(ed_.doc(), line);
    const Pos c = ed_.cursor();
    for (const WordSpan& w : wordSpans(text)) {
        const int from = l.start + w.start;
        if (c.para == l.para && c.off >= from && c.off <= from + w.len) continue;  // ainda sendo digitada
        if (misspelled(text.substr(w.start, w.len))) out.push_back(w);
    }
    return out;
}

WordSpan Speller::spanAt(Pos p) const {
    const auto& paragraphs = ed_.doc().paragraphs;
    if (p.para < 0 || p.para >= static_cast<int>(paragraphs.size())) return {0, 0};
    for (const WordSpan& w : wordSpans(paragraphs[p.para].text))
        if (p.off >= w.start && p.off <= w.start + w.len) return w;
    return {0, 0};
}

std::u32string Speller::wordAt(Pos p) const {
    const WordSpan w = spanAt(p);
    return w.len ? ed_.doc().paragraphs[p.para].text.substr(w.start, w.len) : std::u32string();
}
