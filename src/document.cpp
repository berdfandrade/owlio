#include "document.h"

#include <cwctype>

const char* elementName(Element e) {
    switch (e) {
        case Element::SceneHeading: return "Cabeçalho";
        case Element::Action: return "Ação";
        case Element::Character: return "Personagem";
        case Element::Parenthetical: return "Parentético";
        case Element::Dialogue: return "Diálogo";
        case Element::Transition: return "Transição";
        default: return "?";
    }
}

const char* elementFdxName(Element e) {
    switch (e) {
        case Element::SceneHeading: return "Scene Heading";
        case Element::Action: return "Action";
        case Element::Character: return "Character";
        case Element::Parenthetical: return "Parenthetical";
        case Element::Dialogue: return "Dialogue";
        case Element::Transition: return "Transition";
        default: return "Action";
    }
}

bool elementIsUppercase(Element e) {
    return e == Element::SceneHeading || e == Element::Character || e == Element::Transition;
}

std::string Document::titleValue(const std::string& key) const {
    for (auto& kv : titlePage)
        if (kv.first == key) return kv.second;
    return {};
}

void Document::setTitleValue(const std::string& key, const std::string& value) {
    for (auto it = titlePage.begin(); it != titlePage.end(); ++it) {
        if (it->first == key) {
            if (value.empty()) titlePage.erase(it);
            else it->second = value;
            return;
        }
    }
    if (!value.empty()) titlePage.emplace_back(key, value);
}

std::string Document::avatar(const std::u32string& name) const {
    for (auto& kv : avatars)
        if (kv.first == name) return kv.second;
    return {};
}

void Document::setAvatar(const std::u32string& name, const std::string& base64) {
    for (auto it = avatars.begin(); it != avatars.end(); ++it) {
        if (it->first == name) {
            if (base64.empty()) avatars.erase(it);
            else it->second = base64;
            return;
        }
    }
    if (!base64.empty()) avatars.emplace_back(name, base64);
}

void Document::ensureNotEmpty() {
    if (paragraphs.empty()) paragraphs.push_back({Element::SceneHeading, U""});
}

// ---------------------------------------------------------------- UTF-8

std::u32string utf8ToU32(const std::string& s) {
    std::u32string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp;
        int extra;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c >> 5) == 0x6) { cp = c & 0x1F; extra = 1; }
        else if ((c >> 4) == 0xE) { cp = c & 0x0F; extra = 2; }
        else if ((c >> 3) == 0x1E) { cp = c & 0x07; extra = 3; }
        else { ++i; out.push_back(0xFFFD); continue; }
        bool ok = true;
        for (int k = 1; k <= extra; ++k) {
            if (i + k >= s.size()) { ok = false; break; }
            unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc >> 6) != 0x2) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { out.push_back(0xFFFD); ++i; continue; }
        out.push_back(cp);
        i += extra + 1;
    }
    return out;
}

std::string u32ToUtf8(char32_t c) {
    std::string out;
    if (c < 0x80) out.push_back(static_cast<char>(c));
    else if (c < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (c >> 6)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else if (c < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (c >> 12)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (c >> 18)));
        out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
    return out;
}

std::string u32ToUtf8(const std::u32string& s) {
    std::string out;
    out.reserve(s.size());
    for (char32_t c : s) out += u32ToUtf8(c);
    return out;
}

// Maiúsculas/minúsculas sem depender do locale para Latin-1 (cobre o português).
char32_t toUpper(char32_t c) {
    if (c >= U'a' && c <= U'z') return c - 32;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
    if (c == 0xFF) return 0x178;
    if (c < 0x80) return c;
    return static_cast<char32_t>(std::towupper(static_cast<wint_t>(c)));
}

char32_t toLower(char32_t c) {
    if (c >= U'A' && c <= U'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    if (c < 0x80) return c;
    return static_cast<char32_t>(std::towlower(static_cast<wint_t>(c)));
}

std::u32string toUpper(const std::u32string& s) {
    std::u32string out(s);
    for (auto& c : out) c = toUpper(c);
    return out;
}

bool isSpace(char32_t c) { return c == U' ' || c == U'\t' || c == 0xA0; }

bool isWordChar(char32_t c) {
    if ((c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9')) return true;
    if (c == U'_' || c == U'\'') return true;
    return c >= 0xC0 && c != 0xD7 && c != 0xF7 && c < 0x2000;
}

std::u32string trim(const std::u32string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isSpace(s[a])) ++a;
    while (b > a && isSpace(s[b - 1])) --b;
    return s.substr(a, b - a);
}

bool startsWithNoCase(const std::u32string& s, const std::u32string& prefix) {
    if (prefix.size() > s.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i)
        if (toUpper(s[i]) != toUpper(prefix[i])) return false;
    return true;
}

std::u32string displayText(const Paragraph& p) {
    return elementIsUppercase(p.type) ? toUpper(p.text) : p.text;
}
