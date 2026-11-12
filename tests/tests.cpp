// Core tests (no Qt). Run: ./build/owlio_tests
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
            std::printf("FAILED %s:%d  %s\n", __FILE__, __LINE__, #cond); \
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

    // Diálogo longo cruzando a página gera (MORE) e (CONT'D); em português, (MAIS) e (CONT.).
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
        if (L.lines[i].para < 0 && L.lines[i].virtText == U"(MORE)") hasMore = true;
        if (L.lines[i].para < 0 && L.lines[i].virtText == U"JOÃO (CONT'D)") hasContd = true;
    }
    CHECK(hasMore);
    CHECK(hasContd);
    LayoutOptions pt;
    pt.moreText = U"(MAIS)";
    pt.contdText = U"(CONT.)";
    Layout Lpt = computeLayout(d, pt);
    bool ptMore = false, ptContd = false;
    for (const auto& l : Lpt.lines) {
        if (l.para < 0 && l.virtText == U"(MAIS)") ptMore = true;
        if (l.para < 0 && l.virtText == U"JOÃO (CONT.)") ptContd = true;
    }
    CHECK(ptMore && ptContd);
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

    // Cabeçalho autocompleta horário, conforme o idioma do roteiro.
    Editor e3;
    e3.insertText(U"INT. QUARTO - N");
    CHECK(e3.suggestion() == U"IGHT");
    e3.setScriptLanguage(ScriptLanguage::Portuguese);
    CHECK(e3.suggestion() == U"OITE");
    CHECK(e3.layoutOptions().moreText == U"(MAIS)");
}

static void testCharacters() {
    Document d;
    d.paragraphs = {{Element::SceneHeading, U"INT. A - DIA"},
                    {Element::Character, U"ANA"},
                    {Element::Dialogue, U"Oi, tudo bem?"},
                    {Element::Character, U"BETO"},
                    {Element::Dialogue, U"Tudo."},
                    {Element::SceneHeading, U"EXT. B - NOITE"},
                    {Element::Character, U"ANA (V.O.)"},
                    {Element::Parenthetical, U"(rindo)"},
                    {Element::Dialogue, U"Mentira."}};
    Editor e;
    e.setDocument(d);

    CharacterStats st = e.characterStats(U"ANA");
    CHECK(st.speeches == 2);
    CHECK(st.words == 4);
    CHECK(st.scenes == std::vector<int>({0, 1}));
    CHECK(st.firstPara == 1);
    CHECK(e.characterStats(U"BETO").scenes == std::vector<int>({0}));

    // Ficha completa, com quebras de linha e "*/" que não podem fechar o comentário do Fountain.
    CharacterInfo info;
    info.name = U"ANA";
    info.age = U"34";
    info.occupation = U"Detetive";
    info.role = U"Protagonist";
    info.traits = U"Teimosa.\nUsa óculos */ redondos.";
    info.synopsis = U"Volta à cidade natal\npara resolver um caso \\ antigo.";
    info.photo = "aGVsbG8=";
    CHECK(e.setCharacter(U"ANA", info) == 0);
    const std::string text = writeFountain(e.doc());
    Document back = parseFountain(text);
    CHECK(back.paragraphs.size() == e.doc().paragraphs.size());
    const CharacterInfo* c = back.character(U"ANA");
    CHECK(c != nullptr);
    if (c) {
        CHECK(c->age == info.age && c->occupation == info.occupation && c->role == info.role);
        CHECK(c->traits == info.traits);
        CHECK(c->synopsis == info.synopsis);
        CHECK(c->photo == info.photo);
    }

    // Renomear troca as deixas, mantém a extensão e leva a ficha junto, num só desfazer.
    info.name = U"Ana Lúcia";
    CHECK(e.setCharacter(U"ANA", info) == 2);
    CHECK(e.doc().paragraphs[1].text == U"ANA LÚCIA");
    CHECK(e.doc().paragraphs[6].text == U"ANA LÚCIA (V.O.)");
    CHECK(e.doc().character(U"ANA") == nullptr);
    CHECK(e.doc().character(U"ANA LÚCIA") && e.doc().character(U"ANA LÚCIA")->age == U"34");
    e.undo();
    CHECK(e.doc().paragraphs[6].text == U"ANA (V.O.)");
    CHECK(e.doc().character(U"ANA") && e.doc().character(U"ANA")->photo == "aGVsbG8=");

    // Ficha em branco some do arquivo.
    CHECK(e.setCharacter(U"ANA", CharacterInfo{U"ANA", {}, {}, {}, {}, {}, {}}) == 0);
    CHECK(e.doc().cast.empty());
    CHECK(writeFountain(e.doc()).find("owlio-characters") == std::string::npos);

    // Arquivos antigos, com o bloco só de fotos, continuam abrindo.
    Document old = parseFountain("INT. A - DIA\n\nANA\nOi.\n\n/* owlio-avatars\nANA: aGVsbG8=\n*/\n");
    CHECK(old.avatar(U"ANA") == "aGVsbG8=");
    CHECK(old.paragraphs.size() == 3);
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

static void testDualDialogue() {
    Document d;
    d.paragraphs = {{Element::SceneHeading, U"INT. BAR - NOITE"},
                    {Element::Character, U"ANA"},
                    {Element::Dialogue, U"Você de novo?"},
                    {Element::Character, U"BETO"},
                    {Element::Parenthetical, U"(ao mesmo tempo)"},
                    {Element::Dialogue, U"Eu de novo. Mas desta vez trouxe o dinheiro, juro."},
                    {Element::Action, U"Os dois riem."}};
    Editor e;
    e.setDocument(d);
    CHECK(!e.isDualDialogue());

    // Cursor na segunda fala: liga o diálogo duplo.
    e.setCursor({5, 0});
    CHECK(e.toggleDualDialogue());
    CHECK(e.doc().paragraphs[3].dual);
    CHECK(e.isDualDialogue());
    CHECK(dualSides(e.doc()) == std::vector<int>({0, 1, 1, 2, 2, 2, 0}));

    // Leiaute: as duas falas começam na mesma linha, em colunas diferentes.
    const Layout& L = e.layout();
    const LayoutLine& ana = L.lines[L.paraLines[1].front()];
    const LayoutLine& beto = L.lines[L.paraLines[3].front()];
    CHECK(ana.row == beto.row && ana.page == beto.page);
    CHECK(ana.col < 40 && beto.col > 45);
    for (int li : L.paraLines[5]) CHECK(L.lines[li].col == 46 && L.lines[li].row > beto.row);
    for (int li : L.paraLines[2]) CHECK(L.lines[li].col == 15);
    // A ação seguinte vem depois da coluna mais longa (a da direita), com uma linha em branco.
    int lowest = 0;
    for (int k = 1; k <= 5; ++k)
        for (int li : L.paraLines[k]) lowest = std::max(lowest, L.lines[li].row);
    CHECK(L.lines[L.paraLines[6].front()].row == lowest + 2);

    // Fountain e FDX de ida e volta.
    const std::string fountain = writeFountain(e.doc());
    CHECK(fountain.find("BETO ^") != std::string::npos);
    Document f = parseFountain(fountain);
    CHECK(f.paragraphs.size() == 7 && f.paragraphs[3].dual && !f.paragraphs[1].dual);
    const std::string fdx = writeFdx(e.doc());
    CHECK(fdx.find("<DualDialogue>") != std::string::npos);
    Document x = parseFdx(fdx);
    CHECK(x.paragraphs.size() == 7);
    if (x.paragraphs.size() == 7) {
        CHECK(x.paragraphs[3].type == Element::Character && x.paragraphs[3].dual);
        CHECK(x.paragraphs[1].type == Element::Character && !x.paragraphs[1].dual);
        CHECK(x.paragraphs[5].text == d.paragraphs[5].text);
        CHECK(x.paragraphs[6].type == Element::Action);
    }

    // Na fala da esquerda, desliga o par; desfazer volta.
    e.setCursor({2, 0});
    CHECK(e.toggleDualDialogue());
    CHECK(!e.doc().paragraphs[3].dual && !e.isDualDialogue());
    e.undo();
    CHECK(e.doc().paragraphs[3].dual);
    e.undo();
    CHECK(!e.doc().paragraphs[3].dual);

    // Na primeira fala, liga com a seguinte.
    e.setCursor({1, 0});
    CHECK(e.toggleDualDialogue());
    CHECK(e.doc().paragraphs[3].dual);

    // Fora de uma fala, ou sem fala vizinha, não faz nada.
    e.setCursor({6, 0});
    CHECK(!e.toggleDualDialogue());
    Editor solo;
    Document s1;
    s1.paragraphs = {{Element::Character, U"ANA"}, {Element::Dialogue, U"Oi."}, {Element::Action, U"Fim."}};
    solo.setDocument(s1);
    solo.setCursor({1, 0});
    CHECK(!solo.toggleDualDialogue());

    // "^" num arquivo sem fala anterior é ignorado no leiaute.
    Document lone = parseFountain("INT. A - DIA\n\nANA ^\nOi.\n");
    CHECK(lone.paragraphs.size() == 3 && lone.paragraphs[1].dual);
    CHECK(dualSides(lone) == std::vector<int>({0, 0, 0}));
}

static void testBeatBoard() {
    Editor e;
    Document d;
    d.paragraphs = {{Element::SceneHeading, U"INT. A - DIA"}, {Element::Action, U"Algo."}};
    e.setDocument(d);

    Beat a;
    a.x = 10; a.y = 20; a.w = 240; a.h = 160;
    a.title = U"Abertura";
    a.text = U"Helena chega atrasada.\nChove */ muito.";
    a.color = "#6e7bf2";
    a.image = std::make_shared<const std::string>("aGVsbG8=");
    const int ia = e.addBeat(a);
    Beat b;
    b.x = 400.5; b.y = -30;
    b.title = U"Virada";
    const int ib = e.addBeat(b);
    CHECK(ia == 1 && ib == 2);
    CHECK(e.addFlow(ia, ib));
    CHECK(!e.addFlow(ia, ib));   // repetida
    CHECK(!e.addFlow(ib, ia));   // invertida
    CHECK(!e.addFlow(ia, ia));   // para si mesma
    CHECK(!e.addFlow(ia, 99));   // beat que não existe

    // Fountain de ida e volta; o roteiro não muda.
    const std::string text = writeFountain(e.doc());
    CHECK(text.find("owlio-beatboard") != std::string::npos);
    Document back = parseFountain(text);
    CHECK(back.paragraphs.size() == 2);
    CHECK(back.board.beats.size() == 2 && back.board.flows.size() == 1);
    if (const Beat* r = back.board.beat(ia)) {
        CHECK(r->x == 10 && r->y == 20 && r->w == 240 && r->h == 160);
        CHECK(r->title == a.title && r->text == a.text && r->color == a.color);
        CHECK(r->image && *r->image == "aGVsbG8=");
    } else {
        CHECK(false);
    }
    if (const Beat* r = back.board.beat(ib)) CHECK(r->x == 400.5 && r->y == -30 && !r->image);
    CHECK(back.board.flows[0] == (FlowLine{ia, ib}));

    // Mover é um passo de desfazer; mover para o mesmo lugar não cria passo.
    Beat moved = *e.doc().board.beat(ib);
    moved.x += 50;
    e.updateBeats({moved});
    CHECK(e.doc().board.beat(ib)->x == 450.5);
    const int rev = e.revision();
    e.updateBeats({moved});
    CHECK(e.revision() == rev);
    e.undo();
    CHECK(e.doc().board.beat(ib)->x == 400.5);

    // Apagar um beat leva junto a linha; desfazer volta os dois.
    e.removeBeats({ia});
    CHECK(e.doc().board.beats.size() == 1 && e.doc().board.flows.empty());
    e.undo();
    CHECK(e.doc().board.beats.size() == 2 && e.doc().board.flows.size() == 1);
    e.removeFlows({FlowLine{ia, ib}});
    CHECK(e.doc().board.flows.empty());

    // Copiar e colar texto do roteiro não leva o quadro junto.
    e.selectAll();
    CHECK(e.copySelection().find("owlio-beatboard") == std::string::npos);

    // Quadro vazio não escreve bloco; linhas inválidas e ids repetidos são corrigidos ao ler.
    Editor empty;
    CHECK(writeFountain(empty.doc()).find("owlio-beatboard") == std::string::npos);
    Document odd = parseFountain(
        "INT. A - DIA\n\n/* owlio-beatboard\nbeat: 3\nframe: 0 0 10 10\nbeat: 3\ntitle: B\nflow: 3 9\n*/\n");
    CHECK(odd.board.beats.size() == 2 && odd.board.beats[0].id != odd.board.beats[1].id);
    CHECK(odd.board.beats[0].w >= 80 && odd.board.flows.empty());
}

int main() {
    testScenes();
    testFountain();
    testFdx();
    testWrapAndLayout();
    testEditor();
    testCharacters();
    testDualDialogue();
    testBeatBoard();
    if (failures == 0) std::printf("All tests passed.\n");
    return failures == 0 ? 0 : 1;
}
