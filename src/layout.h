#pragma once
#include <string>
#include <vector>

#include "document.h"

// Geometria de página no padrão da indústria (US Letter, Courier 12pt):
//   10 caracteres por polegada, 6 linhas por polegada.
//   Página = 85 colunas x 66 linhas. Margens: 1,5" esquerda, 1" demais.
namespace page {
constexpr int kCols = 85;           // 8,5"
constexpr int kRows = 66;           // 11"
constexpr int kTopRow = 6;          // texto começa a 1" do topo
constexpr int kLinesPerPage = 54;   // 9" de texto
constexpr int kPageNumberRow = 3;   // número da página a 0,5" do topo
constexpr int kRightCol = 75;       // margem direita (7,5")
constexpr int kLeftCol = 15;        // margem esquerda (1,5")
}  // namespace page

struct ElementStyle {
    int indent;       // coluna inicial (a partir da borda da página)
    int width;        // largura em caracteres
    int spaceBefore;  // linhas em branco antes
    bool rightAlign;
};

const ElementStyle& elementStyle(Element e);

struct LayoutLine {
    int para = -1;        // -1 para linhas virtuais, ex.: (MAIS) / (CONT.)
    int start = 0;        // offset no texto do parágrafo
    int end = 0;          // offset final (inclui o espaço onde a linha quebrou)
    int col = 0;          // coluna onde o texto começa
    int page = 0;
    int row = 0;          // linha dentro da área de texto (0..kLinesPerPage-1)
    std::u32string virtText;
};

struct LayoutOptions {
    std::u32string moreText = U"(MORE)";
    std::u32string contdText = U"(CONT'D)";
};

struct Layout {
    std::vector<LayoutLine> lines;
    std::vector<std::vector<int>> paraLines;  // para -> índices em lines
    int pageCount = 1;

    // Texto exibido de uma linha (já em maiúsculas quando necessário, sem espaço final).
    std::u32string lineText(const Document& doc, int lineIndex) const;
    // Página em que o parágrafo começa.
    int pageOfPara(int para) const;
};

Layout computeLayout(const Document& doc, const LayoutOptions& opt = {});

// Quebra um texto em linhas de até `width` caracteres respeitando palavras.
// Retorna os offsets de início de cada linha.
std::vector<int> wrapText(const std::u32string& text, int width);
