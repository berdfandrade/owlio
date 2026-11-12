// Teste de interface: digita uma cena na janela real usando eventos de teclado do Qt.
// Uso: owlio_gui_test [captura.png]
#include <QApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QScrollBar>
#include <QStyleFactory>
#include <QTest>
#include <cstdio>

#include "mainwindow.h"
#include "theme.h"
#include "sceneboard.h"
#include "scriptview.h"

static int failures = 0;

// Digita texto qualquer (inclusive acentos) como eventos de tecla com texto.
static void type(QWidget* w, const QString& text) {
    for (QChar c : text) {
        if (c.unicode() < 128) {
            QTest::keyClicks(w, QString(c));
        } else {
            QKeyEvent press(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString(c));
            QApplication::sendEvent(w, &press);
            QKeyEvent release(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, QString(c));
            QApplication::sendEvent(w, &release);
        }
    }
}
#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FALHOU linha %d: %s\n", __LINE__, #cond); \
            ++failures;                                            \
        }                                                          \
    } while (0)

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("OwlioTest");
    QApplication::setApplicationName("OwlioTest");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setWindowIcon(QIcon(":/owlio.png"));
    ThemeManager::instance().init();

    MainWindow w;
    w.resize(1280, 900);
    w.show();
    if (!QTest::qWaitForWindowExposed(&w)) std::printf("janela não exposta\n");
    ScriptView* v = w.view();
    v->setFocus();
    const Editor& ed = w.editor();
    auto para = [&](int i) { return ed.doc().paragraphs.at(i); };

    type(v, QString::fromUtf8("int. apartamento de ana - noite"));
    QTest::keyClick(v, Qt::Key_Return);
    type(v, QString::fromUtf8("Ana abre a janela. A cidade inteira está sem luz."));
    QTest::keyClick(v, Qt::Key_Return);
    QTest::keyClick(v, Qt::Key_Tab);  // Ação vazia -> Personagem
    type(v, QString::fromUtf8("ana"));
    QTest::keyClick(v, Qt::Key_Return);  // -> Diálogo
    QTest::keyClick(v, Qt::Key_Tab);     // -> Parentético "()"
    type(v, QString::fromUtf8("para si mesma"));
    QTest::keyClick(v, Qt::Key_Return);  // -> Diálogo
    type(v, QString::fromUtf8("Finalmente dá pra ver as estrelas."));
    QTest::keyClick(v, Qt::Key_Return);  // -> Personagem
    type(v, QString::fromUtf8("bruno (o.s.)"));
    QTest::keyClick(v, Qt::Key_Return);
    type(v, QString::fromUtf8("Ana? Cadê a lanterna?"));
    QTest::keyClick(v, Qt::Key_Return);  // -> Personagem
    type(v, QString::fromUtf8("a"));  // autocompletar sugere "NA"
    CHECK(ed.suggestion() == U"NA");
    QTest::keyClick(v, Qt::Key_Return);  // aceita e vai para Diálogo
    type(v, QString::fromUtf8("Deixa apagada."));
    QTest::keyClick(v, Qt::Key_Return);
    QTest::keyClick(v, Qt::Key_Return);  // Personagem vazio -> Ação
    QTest::keyClick(v, Qt::Key_Tab);     // Ação vazia -> Personagem
    QTest::keyClick(v, Qt::Key_Tab);     // -> Transição
    type(v, QString::fromUtf8("cor"));
    QTest::keyClick(v, Qt::Key_Return);  // aceita "CORTA PARA:" e vai para Cabeçalho
    type(v, QString::fromUtf8("ext. telhado - n"));
    QTest::keyClick(v, Qt::Key_Tab);  // aceita "OITE"

    const Element expected[] = {Element::SceneHeading, Element::Action,   Element::Character,
                                Element::Parenthetical, Element::Dialogue, Element::Character,
                                Element::Dialogue,     Element::Character, Element::Dialogue,
                                Element::Transition,   Element::SceneHeading};
    CHECK(ed.doc().paragraphs.size() == std::size(expected));
    for (size_t i = 0; i < std::size(expected) && i < ed.doc().paragraphs.size(); ++i) {
        if (para(i).type != expected[i])
            std::printf("  parágrafo %zu: tipo %s\n", i, elementName(para(i).type));
        CHECK(para(i).type == expected[i]);
    }
    CHECK(para(3).text == U"(para si mesma)");
    CHECK(para(7).text == U"aNA");
    CHECK(toUpper(para(9).text) == U"CORTA PARA:");
    CHECK(toUpper(para(10).text) == U"EXT. TELHADO - NOITE");

    // Desfazer pelo atalho da janela.
    type(v, QString::fromUtf8(" xyz"));
    QTest::keyClick(v, Qt::Key_Z, Qt::ControlModifier);
    CHECK(toUpper(para(10).text) == U"EXT. TELHADO - NOITE");

    // Ctrl+2 transforma em Ação; Ctrl+1 volta.
    QTest::keyClick(v, Qt::Key_2, Qt::ControlModifier);
    CHECK(para(10).type == Element::Action);
    QTest::keyClick(v, Qt::Key_1, Qt::ControlModifier);
    CHECK(para(10).type == Element::SceneHeading);

    // Clique do mouse posiciona o cursor no início do cabeçalho da 1ª cena.
    QTest::keyClick(v, Qt::Key_Home, Qt::ControlModifier);
    QTest::keyClick(v, Qt::Key_End, Qt::ShiftModifier);
    CHECK(ed.hasSelection());
    QTest::keyClick(v, Qt::Key_Down);
    CHECK(!ed.hasSelection());
    CHECK(ed.cursor().para == 1);

    QTest::keyClick(v, Qt::Key_End, Qt::ControlModifier);
    QTest::keyClick(v, Qt::Key_Return);
    type(v, QString::fromUtf8("Ana sobe a escada de incêndio. Lá em cima, Bruno já está esperando com duas cervejas."));
    QTest::qWait(200);

    // Modo foco ligado pelo atalho.
    QTest::keyClick(v, Qt::Key_F, Qt::ControlModifier | Qt::ShiftModifier);
    CHECK(v->focusMode());
    QTest::keyClick(v, Qt::Key_Up);
    QTest::keyClick(v, Qt::Key_Up);
    QTest::keyClick(v, Qt::Key_Up);
    QTest::qWait(400);  // deixa os painéis atualizarem

    // ---- Tela de cartões
    QTest::keyClick(v, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
    CHECK(w.cardsMode());
    SceneBoard* b = w.board();
    CHECK(b->sceneCount() == 2);
    auto heading = [&](int k) { return toUpper(ed.sceneSummaries().at(k).heading); };
    QTest::keyClick(b, Qt::Key_Home);
    CHECK(b->selectedScene() == 0);
    QTest::keyClick(b, Qt::Key_Right, Qt::AltModifier);  // move a cena 1 para depois da 2
    CHECK(heading(0) == U"EXT. TELHADO - NOITE");
    CHECK(b->selectedScene() == 1);

    // Arrastar o cartão 2 de volta para antes do 1, com eventos reais de mouse.
    QWidget* vp = b->viewport();
    auto mouse = [&](QEvent::Type t, QPointF pos, Qt::MouseButtons buttons) {
        QMouseEvent ev(t, pos, vp->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
        QApplication::sendEvent(vp, &ev);
    };
    const QRectF c0 = b->cardRect(0), c1 = b->cardRect(1);
    mouse(QEvent::MouseButtonPress, c1.center(), Qt::LeftButton);
    mouse(QEvent::MouseMove, c1.center() + QPointF(-30, 5), Qt::LeftButton);
    mouse(QEvent::MouseMove, QPointF(c0.left() + 20, c0.center().y()), Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, QPointF(c0.left() + 20, c0.center().y()), Qt::NoButton);
    CHECK(heading(0) == U"INT. APARTAMENTO DE ANA - NOITE");
    CHECK(b->selectedScene() == 0);

    // Desfazer pelo atalho da janela, com o quadro em foco.
    QTest::keyClick(b, Qt::Key_Z, Qt::ControlModifier);
    CHECK(heading(0) == U"EXT. TELHADO - NOITE");
    QTest::keyClick(b, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    CHECK(heading(0) == U"INT. APARTAMENTO DE ANA - NOITE");

    // Enter abre a cena selecionada no roteiro.
    QTest::keyClick(b, Qt::Key_End);
    QTest::keyClick(b, Qt::Key_Return);
    CHECK(!w.cardsMode());
    CHECK(ed.cursor().para == ed.sceneSummaries().at(1).para);
    QTest::keyClick(v, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
    QTest::qWait(300);

    if (argc > 1) {
        w.grab().save(QString::fromLocal8Bit(argv[1]));
        std::printf("Captura salva em %s\n", argv[1]);
    }
    if (argc > 2) {
        // Captura de um menu aberto (Exibir).
        for (QAction* a : w.menuBar()->actions()) {
            if (a->text().contains("xibir")) {
                QMenu* m = a->menu();
                m->popup(w.mapToGlobal(QPoint(300, 40)));
                QTest::qWait(300);
                m->grab().save(QString::fromLocal8Bit(argv[2]));
                m->hide();
            }
        }
    }
    std::printf(failures == 0 ? "Teste de interface passou.\n" : "%d falha(s).\n", failures);
    return failures == 0 ? 0 : 1;
}
