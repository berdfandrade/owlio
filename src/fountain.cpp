#include "fountain.h"

#include <sstream>
#include <vector>

namespace {

bool isLetter(char32_t c) { return toUpper(c) != toLower(c); }
bool isLowercase(char32_t c) { return isLetter(c) && toUpper(c) != c; }

bool hasLetter(const std::u32string& s) {
    for (char32_t c : s)
        if (isLetter(c)) return true;
    return false;
}

bool isAllCaps(const std::u32string& s) {
    for (char32_t c : s)
        if (isLowercase(c)) return false;
    return hasLetter(s);
}

bool endsWith(const std::u32string& s, const std::u32string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

bool looksLikeTransition(const std::u32string& s) {
    return isAllCaps(s) && (endsWith(s, U"TO:") || endsWith(s, U"PARA:"));
}

// Nome de personagem válido: tudo em maiúsculas antes de uma extensão "(V.O.)".
bool looksLikeCharacterCue(const std::u32string& s) {
    std::u32string name = s;
    size_t paren = name.find(U'(');
    if (paren != std::u32string::npos) name = name.substr(0, paren);
    name = trim(name);
    if (!name.empty() && name.back() == U'^') name.pop_back();  // diálogo duplo
    return !name.empty() && isAllCaps(name) && !looksLikeTransition(s);
}

// Chave de página de rosto: "Title", "Draft date"... Linhas em CAIXA ALTA como "FADE IN:"
// não contam, a não ser que sejam chaves conhecidas ("TITLE:").
bool isTitleKey(const std::u32string& key) {
    if (key.empty() || isSpace(key[0])) return false;
    for (char32_t c : key)
        if (!(isLetter(c) || c == U' ' || (c >= U'0' && c <= U'9'))) return false;
    static const char32_t* known[] = {U"TITLE", U"CREDIT", U"AUTHOR", U"AUTHORS", U"SOURCE",
                                      U"DRAFT DATE", U"DATE", U"CONTACT", U"COPYRIGHT", U"NOTES",
                                      U"REVISION", U"TÍTULO", U"AUTOR", U"AUTORES", U"CONTATO"};
    const std::u32string up = toUpper(trim(key));
    for (const char32_t* k : known)
        if (up == k) return true;
    return !isAllCaps(key);
}

std::u32string stripSceneNumber(const std::u32string& s) {
    // "INT. CASA - DIA #12#" -> "INT. CASA - DIA"
    if (s.size() > 2 && s.back() == U'#') {
        size_t open = s.rfind(U'#', s.size() - 2);
        if (open != std::u32string::npos && open > 0) return trim(s.substr(0, open));
    }
    return s;
}

std::string removeComments(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    size_t i = 0;
    while (i < src.size()) {
        if (src.compare(i, 2, "/*") == 0) {
            size_t e = src.find("*/", i + 2);
            i = (e == std::string::npos) ? src.size() : e + 2;
        } else if (src.compare(i, 2, "[[") == 0) {
            size_t e = src.find("]]", i + 2);
            i = (e == std::string::npos) ? src.size() : e + 2;
        } else {
            out.push_back(src[i++]);
        }
    }
    return out;
}

}  // namespace

bool looksLikeSceneHeading(const std::u32string& line) {
    std::u32string u = toUpper(trim(line));
    static const std::u32string prefixes[] = {U"INT./EXT", U"INT/EXT", U"EXT./INT", U"EXT/INT",
                                              U"I/E", U"INT", U"EXT", U"EST"};
    for (const auto& p : prefixes) {
        if (u.size() > p.size() && u.compare(0, p.size(), p) == 0) {
            char32_t c = u[p.size()];
            if (c == U'.' || c == U' ') return true;
        }
    }
    return false;
}

Document parseFountain(const std::string& utf8In, bool parseTitlePage) {
    std::string src = removeComments(utf8In);
    // Normaliza fins de linha e remove BOM.
    if (src.size() >= 3 && static_cast<unsigned char>(src[0]) == 0xEF &&
        static_cast<unsigned char>(src[1]) == 0xBB && static_cast<unsigned char>(src[2]) == 0xBF)
        src.erase(0, 3);
    std::vector<std::u32string> lines;
    {
        std::string cur;
        for (size_t i = 0; i < src.size(); ++i) {
            char c = src[i];
            if (c == '\r') {
                if (i + 1 < src.size() && src[i + 1] == '\n') ++i;
                lines.push_back(utf8ToU32(cur));
                cur.clear();
            } else if (c == '\n') {
                lines.push_back(utf8ToU32(cur));
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        lines.push_back(utf8ToU32(cur));
    }

    Document doc;
    size_t i = 0;

    // Página de rosto: "Chave: valor" no início do arquivo, até a primeira linha em branco.
    if (parseTitlePage && !lines.empty()) {
        const auto& first = lines[0];
        size_t colon = first.find(U':');
        bool isKey = colon != std::u32string::npos && colon > 0 && isTitleKey(first.substr(0, colon)) &&
                     !looksLikeSceneHeading(first) && !looksLikeTransition(trim(first));
        if (isKey) {
            std::string key;
            std::u32string value;
            auto flush = [&] {
                if (!key.empty()) doc.titlePage.emplace_back(key, u32ToUtf8(trim(value)));
                key.clear();
                value.clear();
            };
            for (; i < lines.size(); ++i) {
                const auto& l = lines[i];
                if (trim(l).empty()) { ++i; break; }
                bool continuation = !l.empty() && (l[0] == U' ' || l[0] == U'\t');
                size_t c = l.find(U':');
                if (!continuation && c != std::u32string::npos) {
                    flush();
                    key = u32ToUtf8(trim(l.substr(0, c)));
                    value = trim(l.substr(c + 1));
                } else {
                    if (!value.empty()) value += U"\n";
                    value += trim(l);
                }
            }
            flush();
        }
    }

    auto isBlank = [&](size_t k) { return k >= lines.size() || trim(lines[k]).empty(); };
    bool inDialogue = false;

    for (; i < lines.size(); ++i) {
        const std::u32string& raw = lines[i];
        std::u32string l = trim(raw);

        if (l.empty()) {
            if (inDialogue && raw.size() >= 2 && raw == U"  ") {
                doc.paragraphs.push_back({Element::Dialogue, U""});
                continue;
            }
            inDialogue = false;
            continue;
        }

        const bool prevBlank = i == 0 || isBlank(i - 1);
        const bool nextBlank = isBlank(i + 1);

        if (inDialogue) {
            if (l[0] == U'(')
                doc.paragraphs.push_back({Element::Parenthetical, l});
            else
                doc.paragraphs.push_back({Element::Dialogue, l});
            continue;
        }

        if (l[0] == U'=' && l.rfind(U"===", 0) != 0) {
            // Sinopse "= texto": pertence ao último cabeçalho de cena.
            for (auto it = doc.paragraphs.rbegin(); it != doc.paragraphs.rend(); ++it) {
                if (it->type != Element::SceneHeading) continue;
                std::u32string syn = trim(l.substr(1));
                if (!it->synopsis.empty() && !syn.empty()) it->synopsis += U" ";
                it->synopsis += syn;
                break;
            }
            continue;
        }
        if (l[0] == U'#') continue;  // seções
        if (l.rfind(U"===", 0) == 0) continue;                                        // quebra de página

        if (l[0] == U'!') {
            doc.paragraphs.push_back({Element::Action, trim(l.substr(1))});
            continue;
        }
        if (l[0] == U'.' && l.size() > 1 && l[1] != U'.') {
            doc.paragraphs.push_back({Element::SceneHeading, stripSceneNumber(trim(l.substr(1)))});
            continue;
        }
        if (l[0] == U'@') {
            std::u32string name = trim(l.substr(1));
            if (!name.empty() && name.back() == U'^') name = trim(name.substr(0, name.size() - 1));
            doc.paragraphs.push_back({Element::Character, name});
            inDialogue = !nextBlank;
            continue;
        }
        if (l[0] == U'>') {
            if (l.back() == U'<')
                doc.paragraphs.push_back({Element::Action, trim(l.substr(1, l.size() - 2))});
            else
                doc.paragraphs.push_back({Element::Transition, trim(l.substr(1))});
            continue;
        }
        if (prevBlank && looksLikeSceneHeading(l)) {
            doc.paragraphs.push_back({Element::SceneHeading, stripSceneNumber(l)});
            continue;
        }
        if (prevBlank && nextBlank && looksLikeTransition(l)) {
            doc.paragraphs.push_back({Element::Transition, l});
            continue;
        }
        if (prevBlank && !nextBlank && looksLikeCharacterCue(l)) {
            std::u32string name = l;
            if (!name.empty() && name.back() == U'^') name = trim(name.substr(0, name.size() - 1));
            doc.paragraphs.push_back({Element::Character, name});
            inDialogue = true;
            continue;
        }
        doc.paragraphs.push_back({Element::Action, l});
    }

    doc.ensureNotEmpty();
    return doc;
}

std::string writeFountain(const Document& doc, bool writeTitlePage) {
    std::ostringstream out;
    if (writeTitlePage && !doc.titlePage.empty()) {
        for (const auto& kv : doc.titlePage) {
            if (kv.second.find('\n') == std::string::npos) {
                out << kv.first << ": " << kv.second << "\n";
            } else {
                out << kv.first << ":\n";
                std::istringstream vs(kv.second);
                std::string vl;
                while (std::getline(vs, vl)) out << "   " << vl << "\n";
            }
        }
        out << "\n";
    }

    bool first = true;
    Element prev = Element::Action;
    for (const auto& p : doc.paragraphs) {
        std::u32string t = trim(p.text);
        if (t.empty() && p.type != Element::Dialogue) continue;

        const bool speechContinues =
            (p.type == Element::Dialogue || p.type == Element::Parenthetical) &&
            (prev == Element::Character || prev == Element::Dialogue || prev == Element::Parenthetical) &&
            !first;
        if (!first) out << (speechContinues ? "\n" : "\n\n");

        std::u32string line;
        switch (p.type) {
            case Element::SceneHeading:
                line = toUpper(t);
                if (!looksLikeSceneHeading(line)) line = U"." + line;
                break;
            case Element::Action:
                line = t;
                if (looksLikeSceneHeading(t) || looksLikeTransition(t) || looksLikeCharacterCue(t) ||
                    t[0] == U'.' || t[0] == U'@' || t[0] == U'>' || t[0] == U'!' || t[0] == U'#' ||
                    t[0] == U'=' || t[0] == U'~')
                    line = U"!" + t;
                break;
            case Element::Character:
                line = toUpper(t);
                if (!looksLikeCharacterCue(line) || looksLikeSceneHeading(line)) line = U"@" + line;
                break;
            case Element::Parenthetical:
                line = t;
                if (line.front() != U'(') line = U"(" + line;
                if (line.back() != U')') line += U")";
                break;
            case Element::Dialogue:
                line = t.empty() ? U"  " : t;
                break;
            case Element::Transition:
                line = toUpper(t);
                if (!looksLikeTransition(line)) line = U">" + line;
                break;
            default:
                line = t;
        }
        out << u32ToUtf8(line);
        if (p.type == Element::SceneHeading && !trim(p.synopsis).empty())
            out << "\n= " << u32ToUtf8(trim(p.synopsis));
        prev = p.type;
        first = false;
    }
    out << "\n";
    return out.str();
}
