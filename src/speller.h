#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "editor.h"
#include "spell.h"

// Corretor do roteiro: o dicionário do idioma do roteiro (embutido no programa), mais os nomes dos
// personagens e o dicionário do próprio roteiro, que nunca são marcados.
class Speller {
public:
    explicit Speller(const Editor& ed) : ed_(ed) {}
    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_; }

    bool misspelled(const std::u32string& word);
    // Palavras erradas numa linha da página, em colunas a partir do começo da linha. Deixas de
    // personagem e transições ficam de fora, assim como a palavra que está sendo digitada.
    std::vector<WordSpan> marks(int line);
    // A palavra do roteiro que cobre a posição (vazia se não houver).
    std::u32string wordAt(Pos p) const;

private:
    void syncNames();

    const Editor& ed_;
    bool enabled_ = true;
    std::unordered_map<std::u32string, bool> cache_[2];  // por idioma: palavra -> errada no dicionário
    std::unordered_set<std::u32string> names_;           // palavras das deixas e das fichas, em minúsculas
    int namesRevision_ = -1;
};
