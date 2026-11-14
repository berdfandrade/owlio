#include "spell.h"

#include <algorithm>
#include <sstream>
#include <string_view>

#include "document.h"

namespace {
bool isLetter(char32_t c) {
    return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= 0xC0 && c != 0xD7 && c != 0xF7 && c < 0x2000);
}
bool isApostrophe(char32_t c) { return c == U'\'' || c == 0x2019; }

void stripBom(std::string& s) {
    if (s.compare(0, 3, "\xEF\xBB\xBF") == 0) s.erase(0, 3);
}

std::vector<std::u32string> split(const std::u32string& w, bool (*sep)(char32_t)) {
    std::vector<std::u32string> parts(1);
    for (char32_t c : w) {
        if (sep(c)) parts.emplace_back();
        else parts.back() += c;
    }
    return parts;
}
}  // namespace

bool SpellChecker::load(const std::string& aff, std::string dic) {
    entries_.clear();
    prefixes_.clear();
    suffixes_.clear();
    prefixByAdd_.clear();
    suffixByAdd_.clear();
    maxPrefix_ = maxSuffix_ = 0;
    loadAffixes(aff);

    stripBom(dic);
    dic_ = std::move(dic);
    size_t pos = dic_.find('\n');  // a primeira linha é só a contagem
    while (pos != std::string::npos && pos + 1 < dic_.size()) {
        const size_t start = pos + 1;
        size_t end = dic_.find('\n', start);
        if (end == std::string::npos) end = dic_.size();
        pos = end;
        size_t wordEnd = start;
        while (wordEnd < end && dic_[wordEnd] != '/' && dic_[wordEnd] != '\t' && dic_[wordEnd] != '\r' && dic_[wordEnd] != ' ') ++wordEnd;
        if (wordEnd == start) continue;
        size_t flagsStart = wordEnd, flagsEnd = wordEnd;
        if (wordEnd < end && dic_[wordEnd] == '/') {
            flagsStart = flagsEnd = wordEnd + 1;
            while (flagsEnd < end && dic_[flagsEnd] != '\t' && dic_[flagsEnd] != '\r' && dic_[flagsEnd] != ' ') ++flagsEnd;
        }
        entries_.push_back({uint32_t(start), uint32_t(flagsStart), uint16_t(wordEnd - start), uint16_t(flagsEnd - flagsStart)});
    }
    auto view = [this](const Entry& e) { return std::string_view(dic_).substr(e.word, e.wordLen); };
    std::sort(entries_.begin(), entries_.end(), [&](const Entry& a, const Entry& b) { return view(a) < view(b); });
    return loaded();
}

void SpellChecker::loadAffixes(const std::string& affIn) {
    std::string aff = affIn;
    stripBom(aff);
    std::istringstream in(aff);
    std::string line;
    int remaining = 0;
    bool cross = false;
    auto text = [](const std::string& t) { return t == "0" ? std::u32string() : utf8ToU32(t); };
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        std::string kind, flag, a, b, cond;
        if (!(fields >> kind >> flag >> a >> b) || (kind != "PFX" && kind != "SFX")) continue;
        if (remaining == 0) {  // cabeçalho: PFX/SFX letra Y/N quantidade
            cross = a == "Y";
            remaining = std::atoi(b.c_str());
            continue;
        }
        --remaining;
        if (!(fields >> cond)) cond = ".";
        if (const size_t slash = b.find('/'); slash != std::string::npos) b.erase(slash);  // sem afixos em cadeia
        Affix af;
        af.flag = flag;
        af.strip = text(a);
        af.add = text(b);
        af.cross = cross;
        const std::u32string c32 = utf8ToU32(cond);
        for (size_t i = 0; i < c32.size(); ++i) {
            Cond c;
            if (c32[i] == U'.') {
                c.any = true;
            } else if (c32[i] == U'[') {
                ++i;
                if (i < c32.size() && c32[i] == U'^') { c.negate = true; ++i; }
                while (i < c32.size() && c32[i] != U']') c.set += c32[i++];
            } else {
                c.set = c32[i];
            }
            af.cond.push_back(std::move(c));
        }
        if (af.cond.size() == 1 && af.cond[0].any) af.cond.clear();
        const bool prefix = kind == "PFX";
        auto& list = prefix ? prefixes_ : suffixes_;
        (prefix ? prefixByAdd_ : suffixByAdd_)[af.add].push_back(int(list.size()));
        size_t& longest = prefix ? maxPrefix_ : maxSuffix_;
        longest = std::max(longest, af.add.size());
        list.push_back(std::move(af));
    }
}

bool SpellChecker::has(const std::u32string& word, const std::string& flagA, const std::string& flagB) const {
    const std::string w = u32ToUtf8(word);
    auto view = [this](const Entry& e) { return std::string_view(dic_).substr(e.word, e.wordLen); };
    auto it = std::lower_bound(entries_.begin(), entries_.end(), std::string_view(w),
                               [&](const Entry& e, std::string_view key) { return view(e) < key; });
    for (; it != entries_.end() && view(*it) == w; ++it) {  // homônimos têm uma entrada cada
        if (flagA.empty()) return true;
        const std::string_view flags = std::string_view(dic_).substr(it->flags, it->flagsLen);
        if (flags.find(flagA) != std::string_view::npos && (flagB.empty() || flags.find(flagB) != std::string_view::npos)) return true;
    }
    return false;
}

bool SpellChecker::matches(const std::vector<Cond>& cond, const std::u32string& stem, bool atEnd) {
    if (stem.size() < cond.size()) return false;
    const size_t base = atEnd ? stem.size() - cond.size() : 0;
    for (size_t i = 0; i < cond.size(); ++i) {
        const Cond& c = cond[i];
        if (c.any) continue;
        if ((c.set.find(stem[base + i]) != std::u32string::npos) == c.negate) return false;
    }
    return true;
}

bool SpellChecker::exact(const std::u32string& w) const {
    if (has(w)) return true;
    const size_t n = w.size();
    for (size_t len = 0; len <= std::min(maxSuffix_, n); ++len) {
        const auto found = suffixByAdd_.find(w.substr(n - len));
        if (found == suffixByAdd_.end()) continue;
        for (int idx : found->second) {
            const Affix& s = suffixes_[idx];
            const std::u32string stem = w.substr(0, n - len) + s.strip;
            if (stem.empty() || !matches(s.cond, stem, true)) continue;
            if (has(stem, s.flag)) return true;
            if (!s.cross) continue;
            for (size_t plen = 0; plen <= std::min(maxPrefix_, stem.size()); ++plen) {  // prefixo + sufixo
                const auto pf = prefixByAdd_.find(stem.substr(0, plen));
                if (pf == prefixByAdd_.end()) continue;
                for (int pidx : pf->second) {
                    const Affix& p = prefixes_[pidx];
                    const std::u32string inner = p.strip + stem.substr(plen);
                    if (p.cross && !inner.empty() && matches(p.cond, inner, false) && has(inner, s.flag, p.flag)) return true;
                }
            }
        }
    }
    for (size_t plen = 0; plen <= std::min(maxPrefix_, n); ++plen) {
        const auto found = prefixByAdd_.find(w.substr(0, plen));
        if (found == prefixByAdd_.end()) continue;
        for (int idx : found->second) {
            const Affix& p = prefixes_[idx];
            const std::u32string stem = p.strip + w.substr(plen);
            if (!stem.empty() && matches(p.cond, stem, false) && has(stem, p.flag)) return true;
        }
    }
    return false;
}

bool SpellChecker::check(const std::u32string& wordIn) const {
    if (wordIn.size() <= 1) return true;
    std::u32string w = wordIn;
    for (char32_t& c : w)
        if (c == 0x2019) c = U'\'';
    if (exact(w)) return true;
    std::u32string lower = w;
    for (char32_t& c : lower) c = toLower(c);
    if (lower != w) {
        if (exact(lower)) return true;
        std::u32string cap = lower;  // nome próprio em CAIXA ALTA: "BRASIL" é "Brasil"
        cap[0] = toUpper(cap[0]);
        if (cap != w && exact(cap)) return true;
    }
    for (bool (*sep)(char32_t) : {+[](char32_t c) { return c == U'-'; }, +[](char32_t c) { return c == U'\''; }}) {
        const std::vector<std::u32string> parts = split(w, sep);  // "guarda-chuva", "d'água", "Ana's"
        if (parts.size() < 2) continue;
        if (std::all_of(parts.begin(), parts.end(), [this](const std::u32string& p) { return check(p); })) return true;
    }
    return false;
}

std::vector<WordSpan> wordSpans(const std::u32string& t) {
    std::vector<WordSpan> out;
    const int n = int(t.size());
    int i = 0;
    while (i < n) {
        const bool digit = t[i] >= U'0' && t[i] <= U'9';
        if (!isLetter(t[i]) && !digit) { ++i; continue; }
        const int start = i;
        bool hasDigit = false;
        while (i < n) {
            if (t[i] >= U'0' && t[i] <= U'9') hasDigit = true;
            else if (!isLetter(t[i])) {
                const bool joiner = (t[i] == U'-' || isApostrophe(t[i])) && i + 1 < n && isLetter(t[i + 1]);
                if (!joiner) break;
            }
            ++i;
        }
        if (!hasDigit) out.push_back({start, i - start});
    }
    return out;
}
