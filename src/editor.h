#pragma once
#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "document.h"
#include "layout.h"

struct Pos {
    int para = 0;
    int off = 0;
    bool operator==(const Pos& o) const { return para == o.para && off == o.off; }
    bool operator!=(const Pos& o) const { return !(*this == o); }
    bool operator<(const Pos& o) const { return para < o.para || (para == o.para && off < o.off); }
};

// Resumo de uma cena para a tela de cartões.
struct SceneSummary {
    int para = 0;                      // índice do parágrafo de cabeçalho
    std::u32string heading;            // em maiúsculas
    std::u32string synopsis;           // escrita pelo autor (pode ser vazia)
    std::u32string preview;            // começo da primeira ação, se não houver sinopse
    std::vector<std::u32string> cast;  // quem fala na cena, na ordem
    int page = 0;                      // página onde começa (0 = primeira)
    int eighths = 1;                   // tamanho em oitavos de página
};

// Números de uma personagem no roteiro.
struct CharacterStats {
    int speeches = 0;         // deixas
    int words = 0;            // palavras faladas
    std::vector<int> scenes;  // índices das cenas em que fala
    int firstPara = -1;       // primeira deixa
};

// Convenções do texto do roteiro: (MORE)/(CONT'D), DAY/NIGHT, CUT TO:...
enum class ScriptLanguage { English, Portuguese };

// Modelo de edição do roteiro, independente de interface gráfica.
class Editor {
public:
    Editor();

    // ---- documento
    const Document& doc() const { return doc_; }
    void setDocument(Document d);
    const Layout& layout() const;
    const LayoutOptions& layoutOptions() const { return layoutOpt_; }
    void setLayoutOptions(const LayoutOptions& o) { layoutOpt_ = o; layoutDirty_ = true; ++revision_; }
    int revision() const { return revision_; }
    bool modified() const { return modified_; }
    void markSaved() { modified_ = false; }
    void markModified() { modified_ = true; ++revision_; }
    void setTitleValue(const std::string& key, const std::string& value);
    void setAvatar(const std::u32string& name, const std::string& base64);  // vazio remove
    ScriptLanguage scriptLanguage() const { return lang_; }
    void setScriptLanguage(ScriptLanguage lang);

    // ---- personagens
    // Grava a ficha. Se o nome mudou, renomeia as deixas (mantendo extensões como (V.O.)).
    // Tudo num só passo de desfazer. Retorna quantas deixas foram renomeadas.
    int setCharacter(const std::u32string& oldName, const CharacterInfo& info);
    CharacterStats characterStats(const std::u32string& name) const;

    // ---- Beat Board (cada chamada é um passo de desfazer)
    int addBeat(Beat b);                                // devolve o id
    void updateBeats(const std::vector<Beat>& beats);   // substitui pelos ids
    void removeBeats(const std::vector<int>& ids);      // leva junto as linhas ligadas
    bool addFlow(int from, int to);                     // false se repetida, invertida ou para si mesmo
    void removeFlows(const std::vector<FlowLine>& flows);

    // ---- mapa de relações (um passo de desfazer; nada muda se for igual)
    void setRelations(const RelationMap& map);
    // ---- forma narrativa da linha do tempo (um passo de desfazer)
    void setStructure(const StoryStructure& st);

    // ---- cursor e seleção
    Pos cursor() const { return cursor_; }
    Pos anchor() const { return anchor_; }
    bool hasSelection() const { return cursor_ != anchor_; }
    std::pair<Pos, Pos> selection() const;
    void setCursor(Pos p, bool select = false);
    void selectAll();
    void selectWordAt(Pos p);
    Element currentElement() const { return doc_.paragraphs[cursor_.para].type; }

    // ---- movimento
    void moveLeft(bool select, bool word);
    void moveRight(bool select, bool word);
    void moveUp(bool select);
    void moveDown(bool select);
    void moveLineStart(bool select);
    void moveLineEnd(bool select);
    void moveDocStart(bool select);
    void moveDocEnd(bool select);
    void movePage(int dir, bool select);

    // ---- edição
    void insertText(const std::u32string& s);
    void enter();
    void tab(bool shift);
    void backspace(bool word);
    void deleteForward(bool word);
    void setElement(Element e);
    // Diálogo duplo da fala onde está o cursor: liga (ao lado da fala anterior ou da seguinte)
    // ou desliga. Retorna false se não houver duas falas seguidas.
    bool toggleDualDialogue();
    bool isDualDialogue() const;
    void deleteSelection();
    std::string copySelection() const;  // texto (Fountain quando atravessa parágrafos)
    void paste(const std::string& utf8);

    // ---- desfazer
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    void undo();
    void redo();

    // ---- autocompletar (SmartType)
    std::u32string suggestion() const;  // complemento que falta digitar
    bool acceptSuggestion();

    // ---- localizar / substituir (sem diferenciar maiúsculas)
    bool find(const std::u32string& query, bool forward = true);
    bool replaceCurrent(const std::u32string& query, const std::u32string& replacement);
    int replaceAll(const std::u32string& query, const std::u32string& replacement);

    // ---- cenas (tela de cartões)
    std::vector<SceneSummary> sceneSummaries() const;
    void setSynopsis(int para, const std::u32string& synopsis);
    // Move a cena `from` para antes da cena `to` (0..n, na numeração atual). Retorna o novo índice.
    int moveScene(int from, int to);

    // ---- informações
    std::vector<int> scenes() const;
    int sceneIndexAt(int para) const;  // -1 antes da primeira cena
    std::vector<std::pair<std::u32string, int>> characters() const;  // nome, falas
    int wordCount() const;
    int lineOf(Pos p) const;          // índice em layout().lines
    int columnOf(Pos p) const;        // coluna absoluta na página
    Pos posAtLine(int line, int col) const;

private:
    enum class EditKind { None, Typing, Deleting, Other };
    struct Snapshot {
        std::vector<Paragraph> paragraphs;
        std::vector<std::pair<std::string, std::string>> titlePage;
        std::vector<CharacterInfo> cast;
        BeatBoard board;
        RelationMap relations;
        StoryStructure structure;
        Pos cursor, anchor;
    };

    void beginEdit(EditKind kind);
    void changed();
    void clampCursor();
    Paragraph& cur() { return doc_.paragraphs[cursor_.para]; }
    void insertParagraph(int index, Element type, const std::u32string& text);
    void moveTo(Pos p, bool select, bool keepColumn = false);
    void moveVertical(int lines, bool select);
    std::vector<std::u32string> candidates() const;
    int speechCue(int para) const;  // deixa (Personagem) da fala que contém `para`, ou -1

    Document doc_;
    Pos cursor_, anchor_;
    int preferredCol_ = -1;
    mutable Layout layout_;
    mutable bool layoutDirty_ = true;
    LayoutOptions layoutOpt_;
    ScriptLanguage lang_ = ScriptLanguage::English;
    int revision_ = 0;
    bool modified_ = false;

    std::vector<Snapshot> undo_, redo_;
    EditKind lastKind_ = EditKind::None;
    std::chrono::steady_clock::time_point lastEditTime_;
};
