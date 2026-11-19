#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "editor.h"
#include "spell.h"

// Corretor do roteiro: um dicionário embutido (português ou inglês), mais os nomes dos personagens e o
// dicionário do próprio roteiro, que nunca são marcados. No modo automático o idioma vem do texto:
// vale o dicionário que reconhece mais palavras do roteiro, qualquer que seja a convenção das páginas.
class Speller {
public:
    enum class Language { Automatic, Portuguese, English };
    explicit Speller(const Editor& ed) : ed_(ed) {}
    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_; }
    void setLanguage(Language l) { language_ = l; }
    ScriptLanguage dictionaryLanguage();  // o dicionário em uso agora

    bool misspelled(const std::u32string& word);
    // Palavras erradas numa linha da página, em colunas a partir do começo da linha. Deixas de
    // personagem e transições ficam de fora, assim como a palavra que está sendo digitada.
    std::vector<WordSpan> marks(int line);
    // A palavra do roteiro que cobre a posição: onde fica no parágrafo (len 0 se não houver) e o texto.
    WordSpan spanAt(Pos p) const;
    std::u32string wordAt(Pos p) const;
    // Sugestões para uma palavra errada, no dicionário em uso, a melhor primeiro.
    std::vector<std::u32string> suggestions(const std::u32string& word);
    // Todas as palavras erradas do roteiro (as mesmas que ficam sublinhadas) que têm sugestão, cada uma
    // com a melhor delas. As que não têm sugestão vão para `unfixable`, se pedido.
    std::vector<Editor::TextFix> corrections(std::vector<std::u32string>* unfixable = nullptr);

private:
    void syncNames();
    bool unknown(ScriptLanguage lang, const std::u32string& word);  // fora do dicionário (com cache)

    const Editor& ed_;
    bool enabled_ = true;
    Language language_ = Language::Automatic;
    ScriptLanguage detected_ = ScriptLanguage::English;
    int detectedRevision_ = -1;
    static constexpr size_t kMaxCached = 16384;
    std::unordered_map<std::u32string, bool> cache_[2];  // por idioma: palavra -> errada no dicionário
    std::unordered_set<std::u32string> names_;           // palavras das deixas e das fichas, em minúsculas
    int namesRevision_ = -1;
};
