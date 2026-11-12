#pragma once
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

struct Document {
    std::vector<Paragraph> paragraphs;
    // Página de rosto (chave/valor, no estilo Fountain: Title, Credit, Author, Contact...)
    std::vector<std::pair<std::string, std::string>> titlePage;
    // Fichas dos personagens (só os que têm algo preenchido).
    std::vector<CharacterInfo> cast;

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

// Texto como aparece na página (maiúsculas quando o elemento pede).
std::u32string displayText(const Paragraph& p);
