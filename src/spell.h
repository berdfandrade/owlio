#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Verificador ortográfico que lê dicionários no formato Hunspell (.aff + .dic, em UTF-8).
// Entende prefixos e sufixos com condições e produto cruzado, que é o que os dicionários
// pt_BR (VERO) e en_US usam. Não gera sugestões.
class SpellChecker {
public:
    // `aff` e `dic` são o conteúdo dos arquivos. Devolve false se o .dic não trouxer palavras.
    bool load(const std::string& aff, std::string dic);
    bool loaded() const { return !entries_.empty(); }
    // true se a palavra existe como está escrita ou, no começo de frase e em CAIXA ALTA, em minúsculas.
    bool check(const std::u32string& word) const;

private:
    struct Entry {
        uint32_t word, flags;
        uint16_t wordLen, flagsLen;
    };
    struct Cond {  // um caractere da condição: qualquer um, um conjunto ou fora de um conjunto
        bool any = false, negate = false;
        std::u32string set;
    };
    struct Affix {
        std::string flag;
        std::u32string strip, add;
        std::vector<Cond> cond;
        bool cross = false;
    };

    void loadAffixes(const std::string& aff);
    bool has(const std::u32string& word, const std::string& flagA = {}, const std::string& flagB = {}) const;
    bool exact(const std::u32string& word) const;
    static bool matches(const std::vector<Cond>& cond, const std::u32string& stem, bool atEnd);

    std::string dic_;             // o arquivo inteiro; as entradas apontam para dentro dele
    std::vector<Entry> entries_;  // ordenadas pela palavra
    std::vector<Affix> prefixes_, suffixes_;
    std::unordered_map<std::u32string, std::vector<int>> prefixByAdd_, suffixByAdd_;
    size_t maxPrefix_ = 0, maxSuffix_ = 0;
};

struct WordSpan {
    int start, len;
};
// Palavras de um texto: letras, com hífen e apóstrofo no meio. Trechos com dígitos ficam de fora.
std::vector<WordSpan> wordSpans(const std::u32string& text);
