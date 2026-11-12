// Testes do núcleo (sem Qt). Rode: ./build/owlio_tests
#include <cstdio>
#include <cstdlib>

#include "editor.h"
#include "fdx.h"
#include "fountain.h"
#include "layout.h"

static int failures = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FALHOU %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

static const char* kSample =
    "Title: O Teste\n"
    "Author: Alguém\n"
    "\n"
    "INT. CASA DE JOÃO - NOITE\n"
    "\n"
    "João entra na sala, cansado.\n"
    "\n"
    "JOÃO\n"
    "(baixinho)\n"
    "Tem alguém aí?\n"
    "\n"
    "MARIA (O.S.)\n"
    "Aqui na cozinha!\n"
    "\n"
    "CORTA PARA:\n"
    "\n"
    "EXT. RUA - DIA\n";

static void testFountain() {
    Document d = parseFountain(kSample);
    CHECK(d.titleValue("Title") == "O Teste");
    CHECK(d.titleValue("Author") == "Alguém");
    CHECK(d.paragraphs.size() == 9);
    CHECK(d.paragraphs[0].type == Element::SceneHeading);
    CHECK(d.paragraphs[1].type == Element::Action);
    CHECK(d.paragraphs[2].type == Element::Character);
    CHECK(d.paragraphs[3].type == Element::Parenthetical);
    CHECK(d.paragraphs[4].type == Element::Dialogue);
    CHECK(d.paragraphs[5].type == Element::Character);
    CHECK(d.paragraphs[6].type == Element::Dialogue);
    CHECK(d.paragraphs[7].type == Element::Transition);
    CHECK(d.paragraphs[8].type == Element::SceneHeading);

    // Ida e volta preserva tudo.
    Document d2 = parseFountain(writeFountain(d));
    CHECK(d2.paragraphs.size() == d.paragraphs.size());
    for (size_t i = 0; i < d.paragraphs.size() && i < d2.paragraphs.size(); ++i) {
        CHECK(d2.paragraphs[i].type == d.paragraphs[i].type);
        CHECK(toUpper(d2.paragraphs[i].text) == toUpper(d.paragraphs[i].text));
    }

    // Roteiro sem página de rosto começando com FADE IN: não perde a primeira linha.
    Document noTitle = parseFountain("FADE IN:\n\nINT. CASA - DIA\n");
    CHECK(noTitle.titlePage.empty());
    CHECK(noTitle.paragraphs.size() == 2);
    CHECK(noTitle.paragraphs[0].text == U"FADE IN:");

    // Elementos "forçados" sobrevivem.
    Document f;
    f.paragraphs = {{Element::SceneHeading, U"flashback"},
                    {Element::Action, U"INT. ISSO É AÇÃO"},
                    {Element::Transition, U"fade out"},
                    {Element::Character, U"McGregor"},
                    {Element::Dialogue, U"Oi."}};
    Document f2 = parseFountain(writeFountain(f));
    CHECK(f2.paragraphs.size() == 5);
    for (size_t i = 0; i < 5 && i < f2.paragraphs.size(); ++i)
        CHECK(f2.paragraphs[i].type == f.paragraphs[i].type);
}

static void testFdx() {
    Document d = parseFountain(kSample);
    Document d2 = parseFdx(writeFdx(d));
    CHECK(d2.paragraphs.size() == d.paragraphs.size());
    for (size_t i = 0; i < d.paragraphs.size() && i < d2.paragraphs.size(); ++i) {
        CHECK(d2.paragraphs[i].type == d.paragraphs[i].type);
        CHECK(d2.paragraphs[i].text == d.paragraphs[i].text);
    }
    CHECK(d2.titleValue("Title") == "O Teste");
}

static void testWrapAndLayout() {
    auto starts = wrapText(U"aaa bbb ccc", 7);
    CHECK(starts.size() == 2 && starts[1] == 8);
    CHECK(wrapText(U"curto", 60).size() == 1);

    // Diálogo longo cruzando a página gera (MAIS) e (CONT.).
    Document d;
    for (int i = 0; i < 25; ++i) d.paragraphs.push_back({Element::Action, U"Uma linha de ação."});
    d.paragraphs.push_back({Element::Character, U"JOÃO"});
    std::u32string longLine;
    for (int i = 0; i < 60; ++i) longLine += U"palavra ";
    d.paragraphs.push_back({Element::Dialogue, longLine});
    Layout L = computeLayout(d);
    CHECK(L.pageCount == 2);
    bool hasMore = false, hasContd = false;
    for (size_t i = 0; i < L.lines.size(); ++i) {
        if (L.lines[i].para < 0 && L.lines[i].virtText == U"(MAIS)") hasMore = true;
        if (L.lines[i].para < 0 && L.lines[i].virtText == U"JOÃO (CONT.)") hasContd = true;
    }
    CHECK(hasMore);
    CHECK(hasContd);
    for (const auto& l : L.lines) CHECK(l.row >= 0 && l.row < page::kLinesPerPage);
    // Personagem nunca fica sozinho no fim da página.
    for (size_t i = 0; i + 1 < L.lines.size(); ++i)
        if (L.lines[i].para >= 0 && d.paragraphs[L.lines[i].para].type == Element::Character)
            CHECK(L.lines[i + 1].page == L.lines[i].page);
}

static void testEditor() {
    Editor e;
    // Documento novo começa em Cabeçalho de cena.
    CHECK(e.currentElement() == Element::SceneHeading);
    e.insertText(U"int. casa - dia");
    e.enter();
    CHECK(e.currentElement() == Element::Action);
    e.insertText(U"Ele senta.");
    e.enter();
    e.tab(false);  // Ação vazia + Tab -> Personagem
    CHECK(e.currentElement() == Element::Character);
    e.insertText(U"joão");
    e.enter();
    CHECK(e.currentElement() == Element::Dialogue);
    e.tab(false);  // Diálogo vazio + Tab -> Parentético com ()
    CHECK(e.currentElement() == Element::Parenthetical);
    CHECK(e.doc().paragraphs[e.cursor().para].text == U"()");
    e.insertText(U"sorrindo");
    e.insertText(U")");  // pula o ")" existente
    CHECK(e.doc().paragraphs[e.cursor().para].text == U"(sorrindo)");
    e.enter();
    CHECK(e.currentElement() == Element::Dialogue);
    e.insertText(U"Olá!");
    e.enter();
    CHECK(e.currentElement() == Element::Character);
    e.insertText(U"j");
    CHECK(e.suggestion() == U"OÃO");  // autocompletar
    e.enter();
    CHECK(e.doc().paragraphs[e.cursor().para - 1].text == U"jOÃO");
    CHECK(e.currentElement() == Element::Dialogue);

    // Desfazer / refazer.
    size_t n = e.doc().paragraphs.size();
    e.insertText(U"abc");
    e.undo();
    CHECK(e.doc().paragraphs[e.cursor().para].text.empty());
    e.redo();
    CHECK(e.doc().paragraphs[e.cursor().para].text == U"abc");
    CHECK(e.doc().paragraphs.size() == n);

    // Backspace no início junta parágrafos.
    e.setCursor({1, 0});
    e.backspace(false);
    CHECK(e.doc().paragraphs[0].text == U"int. casa - diaEle senta.");
    e.undo();
    CHECK(e.doc().paragraphs[1].text == U"Ele senta.");

    // Localizar / substituir.
    e.setCursor({0, 0});
    CHECK(e.find(U"SENTA"));
    CHECK(e.cursor().para == 1);
    CHECK(e.replaceAll(U"senta", U"levanta") == 1);
    CHECK(e.doc().paragraphs[1].text == U"Ele levanta.");

    // Copiar várias linhas e colar mantém os elementos.
    e.selectAll();
    std::string copied = e.copySelection();
    Editor e2;
    e2.paste(copied);
    CHECK(e2.doc().paragraphs.size() == e.doc().paragraphs.size());
    for (size_t i = 0; i < e2.doc().paragraphs.size() && i < e.doc().paragraphs.size(); ++i)
        CHECK(e2.doc().paragraphs[i].type == e.doc().paragraphs[i].type);

    // Cabeçalho autocompleta horário.
    Editor e3;
    e3.insertText(U"INT. QUARTO - N");
    CHECK(e3.suggestion() == U"OITE");
}

static void testScenes() {
    Document d;
    d.paragraphs = {{Element::Transition, U"FADE IN:"},
                    {Element::SceneHeading, U"INT. A - DIA", U"Primeira cena."},
                    {Element::Action, U"Ação A."},
                    {Element::Character, U"ANA"},
                    {Element::Dialogue, U"Oi."},
                    {Element::SceneHeading, U"EXT. B - NOITE"},
                    {Element::Action, U"Ação B."},
                    {Element::SceneHeading, U"INT. C - DIA"},
                    {Element::Character, U"BRUNO (V.O.)"},
                    {Element::Dialogue, U"Tchau."}};

    // Sinopse sobrevive ao Fountain e ao FDX.
    Document f = parseFountain(writeFountain(d));
    CHECK(f.paragraphs.size() == d.paragraphs.size());
    CHECK(f.paragraphs[1].synopsis == U"Primeira cena.");
    CHECK(f.paragraphs[5].synopsis.empty());
    Document x = parseFdx(writeFdx(d));
    CHECK(x.paragraphs.size() == d.paragraphs.size());
    CHECK(x.paragraphs[1].synopsis == U"Primeira cena.");
    CHECK(x.paragraphs[1].text == U"INT. A - DIA");

    Editor e;
    e.setDocument(d);
    auto sc = e.sceneSummaries();
    CHECK(sc.size() == 3);
    CHECK(sc[0].synopsis == U"Primeira cena.");
    CHECK(sc[0].cast.size() == 1 && sc[0].cast[0] == U"ANA");
    CHECK(sc[1].preview == U"Ação B.");
    CHECK(sc[2].cast.size() == 1 && sc[2].cast[0] == U"BRUNO");
    for (auto& s : sc) CHECK(s.eighths >= 1);

    // Mover a cena C para o início (antes da A); o prólogo FADE IN: fica onde está.
    CHECK(e.moveScene(2, 0) == 0);
    auto order = e.sceneSummaries();
    CHECK(order[0].heading == U"INT. C - DIA");
    CHECK(order[1].heading == U"INT. A - DIA");
    CHECK(order[2].heading == U"EXT. B - NOITE");
    CHECK(e.doc().paragraphs[0].text == U"FADE IN:");
    CHECK(e.doc().paragraphs.size() == d.paragraphs.size());
    CHECK(e.cursor().para == 1);

    // Mover A para o fim.
    CHECK(e.moveScene(1, 3) == 2);
    CHECK(e.sceneSummaries()[2].heading == U"INT. A - DIA");
    CHECK(e.doc().paragraphs.back().text == U"Oi.");

    // Movimentos sem efeito não mexem no documento.
    int rev = e.revision();
    CHECK(e.moveScene(0, 0) == 0);
    CHECK(e.moveScene(0, 1) == 0);
    CHECK(e.revision() == rev);

    // Desfazer volta a ordem original.
    e.undo();
    e.undo();
    auto back = e.sceneSummaries();
    CHECK(back[0].heading == U"INT. A - DIA" && back[2].heading == U"INT. C - DIA");

    // Fotos dos personagens: Fountain de ida e volta e desfazer.
    e.setAvatar(U"ANA", "aGVsbG8=");
    CHECK(e.doc().avatar(U"ANA") == "aGVsbG8=");
    Document withAvatar = parseFountain(writeFountain(e.doc()));
    CHECK(withAvatar.avatar(U"ANA") == "aGVsbG8=");
    CHECK(withAvatar.paragraphs.size() == e.doc().paragraphs.size());
    e.undo();
    CHECK(e.doc().avatar(U"ANA").empty());

    e.setSynopsis(sc[1].para, U"Linha 1\nLinha 2");
    CHECK(e.doc().paragraphs[sc[1].para].synopsis == U"Linha 1 Linha 2");
    e.undo();
    CHECK(e.doc().paragraphs[sc[1].para].synopsis.empty());
}

int main() {
    testScenes();
    testFountain();
    testFdx();
    testWrapAndLayout();
    testEditor();
    if (failures == 0) std::printf("Todos os testes passaram.\n");
    return failures == 0 ? 0 : 1;
}
