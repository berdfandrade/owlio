#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Elementos de roteiro (equivalentes aos do Final Draft).
enum class Element {
    SceneHeading = 0,  // Cabeçalho de cena:  INT. CASA - DIA
    Action,            // Ação
    Character,         // Personagem
    Parenthetical,     // Parentético: (baixinho)
    Dialogue,          // Diálogo
    Transition,        // Transição: CORTA PARA:
    Count
};

constexpr int kElementCount = static_cast<int>(Element::Count);

const char* elementName(Element e);       // nome em inglês (a interface traduz)
const char* elementFdxName(Element e);    // nome usado no formato .fdx
bool elementIsUppercase(Element e);       // exibido/exportado em maiúsculas

struct Paragraph {
    Element type = Element::Action;
    std::u32string text;
    std::u32string synopsis;  // só para Cabeçalho: sinopse mostrada no cartão da cena
    bool dual = false;        // só para Personagem: esta fala fica ao lado da anterior (diálogo duplo)

    Paragraph() = default;
    Paragraph(Element t, std::u32string tx, std::u32string syn = {})
        : type(t), text(std::move(tx)), synopsis(std::move(syn)) {}
};

// Ficha de um personagem. O nome fica em maiúsculas e sem extensão, como nas deixas.
struct CharacterInfo {
    std::u32string name;
    std::string photo;  // imagem em base64 (JPEG/PNG), vazia = iniciais
    std::u32string age;
    std::u32string occupation;
    std::u32string role;      // protagonista, antagonista, coadjuvante...
    std::u32string traits;    // características: aparência, personalidade
    std::u32string synopsis;  // história e arco do personagem
    bool blank() const;       // nada além do nome
};

// Beat Board: cartões livres de ideias num quadro, ligados por linhas de fluxo.
struct Beat {
    int id = 0;
    double x = 0, y = 0;          // canto superior esquerdo, em pixels do quadro (zoom 100%)
    double w = 220, h = 150;
    std::string color = "#f2b13b";  // cor da faixa do cartão
    std::u32string title;
    std::u32string text;
    // Imagem em base64 (JPEG/PNG). Compartilhada para que as cópias do desfazer não dupliquem os bytes.
    std::shared_ptr<const std::string> image;
};

struct FlowLine {
    int from = 0, to = 0;  // ids dos beats
    bool operator==(const FlowLine& o) const { return from == o.from && to == o.to; }
};

struct BeatBoard {
    std::vector<Beat> beats;
    std::vector<FlowLine> flows;
    bool empty() const { return beats.empty(); }
    const Beat* beat(int id) const;
    int nextId() const;  // maior id + 1
};

// Mapa de relações: onde cada personagem fica no quadro e as ligações entre eles.
enum class RelationKind { Family = 0, Love, Friendship, Rivalry, Work, Other, Count };
const char* relationKindKey(RelationKind k);       // "family", "love"... (no arquivo)
RelationKind relationKindFromKey(const std::string& key);

struct Relationship {
    std::u32string a, b;  // nomes em maiúsculas; a ordem não importa
    RelationKind kind = RelationKind::Other;
    std::u32string label;  // "irmã", "ex-marido"...
    bool links(const std::u32string& x, const std::u32string& y) const {
        return (a == x && b == y) || (a == y && b == x);
    }
};

struct CharacterNode {
    std::u32string name;
    double x = 0, y = 0;  // centro, em pixels do quadro
};

struct RelationMap {
    std::vector<CharacterNode> nodes;
    std::vector<Relationship> links;
    bool empty() const { return nodes.empty() && links.empty(); }
    const CharacterNode* node(const std::u32string& name) const;
    const Relationship* link(const std::u32string& a, const std::u32string& b) const;
    void rename(const std::u32string& from, const std::u32string& to);
};

struct Document {
    std::vector<Paragraph> paragraphs;
    // Página de rosto (chave/valor, no estilo Fountain: Title, Credit, Author, Contact...)
    std::vector<std::pair<std::string, std::string>> titlePage;
    // Fichas dos personagens (só os que têm algo preenchido).
    std::vector<CharacterInfo> cast;
    BeatBoard board;
    RelationMap relations;

    std::string titleValue(const std::string& key) const;
    void setTitleValue(const std::string& key, const std::string& value);
    const CharacterInfo* character(const std::u32string& name) const;
    void setCharacter(const CharacterInfo& info);  // ficha em branco remove
    std::string avatar(const std::u32string& name) const;
    void setAvatar(const std::u32string& name, const std::string& base64);
    void ensureNotEmpty();
};

// ---- UTF-8 / UTF-32 ----
std::u32string utf8ToU32(const std::string& s);
std::string u32ToUtf8(const std::u32string& s);
std::string u32ToUtf8(char32_t c);

char32_t toUpper(char32_t c);
char32_t toLower(char32_t c);
std::u32string toUpper(const std::u32string& s);
bool isSpace(char32_t c);
bool isWordChar(char32_t c);
std::u32string trim(const std::u32string& s);
bool startsWithNoCase(const std::u32string& s, const std::u32string& prefix);

// Diálogo duplo: para cada parágrafo, 0 = normal, 1 = coluna da esquerda, 2 = coluna da direita.
// Uma fala com `dual` vai para a direita e a fala logo antes dela, para a esquerda.
std::vector<int> dualSides(const Document& doc);

// Texto como aparece na página (maiúsculas quando o elemento pede).
std::u32string displayText(const Paragraph& p);
