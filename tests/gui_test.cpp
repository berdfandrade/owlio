// UI test: types a scene into the real window using Qt keyboard and mouse events.
// Usage: owlio_gui_test [script.png] [menu.png] [cards.png] [title.png] [print.png] [character.png] [settings.png]
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
#include <QFile>
#include <QLineEdit>
#include <QLinearGradient>
#include <QPainter>
#include <QTemporaryDir>

#include <QDir>
#include <QDockWidget>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QTranslator>

#include "autosave.h"
#include "avatars.h"
#include "characterdialog.h"
#include "fountain.h"
#include "i18n.h"
#include "settingsdialog.h"
#include "printdialog.h"
#include "sceneboard.h"
#include "titlepagedialog.h"
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
            std::printf("FAILED line %d: %s\n", __LINE__, #cond); \
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
    i18n::install(app, "en");

    // Inglês é o padrão; o arquivo owlio_en só traz os plurais.
    CHECK(QCoreApplication::translate("MainWindow", "%n scene(s)", nullptr, 1) == "1 scene");
    CHECK(QCoreApplication::translate("MainWindow", "%n scene(s)", nullptr, 3) == "3 scenes");
    {
        QTranslator pt;
        CHECK(pt.load(":/i18n/owlio_pt_BR.qm"));
        QCoreApplication::installTranslator(&pt);
        CHECK(QCoreApplication::translate("MainWindow", "&File") == QString::fromUtf8("&Arquivo"));
        CHECK(QCoreApplication::translate("MainWindow", "%n scene(s)", nullptr, 2) == "2 cenas");
        CHECK(QCoreApplication::translate("CharacterDialog", "Synopsis") == "Sinopse");
        CHECK(i18n::elementLabel(Element::Parenthetical) == QString::fromUtf8("Parentético"));
        QCoreApplication::removeTranslator(&pt);
    }
    CHECK(i18n::elementLabel(Element::Parenthetical) == "Parenthetical");

    // Começa sem cópias de recuperação nem backups de execuções anteriores.
    QDir(autosave::recoveryDir()).removeRecursively();
    QDir(autosave::backupRoot()).removeRecursively();

    MainWindow w;
    w.resize(1280, 900);
    w.show();
    if (!QTest::qWaitForWindowExposed(&w)) std::printf("window not exposed\n");
    ScriptView* v = w.view();
    v->setFocus();
    const Editor& ed = w.editor();
    auto para = [&](int i) { return ed.doc().paragraphs.at(i); };

    type(v, QString::fromUtf8("int. ana's apartment - night"));
    QTest::keyClick(v, Qt::Key_Return);
    type(v, QString::fromUtf8("Ana opens the window. The whole city is dark. Café closed, señor."));
    QTest::keyClick(v, Qt::Key_Return);
    QTest::keyClick(v, Qt::Key_Tab);  // Ação vazia -> Personagem
    type(v, QString::fromUtf8("ana"));
    QTest::keyClick(v, Qt::Key_Return);  // -> Diálogo
    QTest::keyClick(v, Qt::Key_Tab);     // -> Parentético "()"
    type(v, QString::fromUtf8("to herself"));
    QTest::keyClick(v, Qt::Key_Return);  // -> Diálogo
    type(v, QString::fromUtf8("Finally you can see the stars."));
    QTest::keyClick(v, Qt::Key_Return);  // -> Personagem
    type(v, QString::fromUtf8("bruno (o.s.)"));
    QTest::keyClick(v, Qt::Key_Return);
    type(v, QString::fromUtf8("Ana? Where's the flashlight?"));
    QTest::keyClick(v, Qt::Key_Return);  // -> Personagem
    type(v, QString::fromUtf8("a"));  // autocompletar sugere "NA"
    CHECK(ed.suggestion() == U"NA");
    QTest::keyClick(v, Qt::Key_Return);  // aceita e vai para Diálogo
    type(v, QString::fromUtf8("Leave it off."));
    QTest::keyClick(v, Qt::Key_Return);
    QTest::keyClick(v, Qt::Key_Return);  // Personagem vazio -> Ação
    QTest::keyClick(v, Qt::Key_Tab);     // Ação vazia -> Personagem
    QTest::keyClick(v, Qt::Key_Tab);     // -> Transição
    type(v, QString::fromUtf8("cu"));
    QTest::keyClick(v, Qt::Key_Return);  // aceita "CUT TO:" e vai para Cabeçalho
    type(v, QString::fromUtf8("ext. rooftop - n"));
    QTest::keyClick(v, Qt::Key_Tab);  // aceita "IGHT"

    const Element expected[] = {Element::SceneHeading, Element::Action,   Element::Character,
                                Element::Parenthetical, Element::Dialogue, Element::Character,
                                Element::Dialogue,     Element::Character, Element::Dialogue,
                                Element::Transition,   Element::SceneHeading};
    CHECK(ed.doc().paragraphs.size() == std::size(expected));
    for (size_t i = 0; i < std::size(expected) && i < ed.doc().paragraphs.size(); ++i) {
        if (para(i).type != expected[i])
            std::printf("  paragraph %zu: type %s\n", i, elementName(para(i).type));
        CHECK(para(i).type == expected[i]);
    }
    CHECK(para(3).text == U"(to herself)");
    CHECK(para(7).text == U"aNA");
    CHECK(toUpper(para(9).text) == U"CUT TO:");
    CHECK(toUpper(para(10).text) == U"EXT. ROOFTOP - NIGHT");

    // Desfazer pelo atalho da janela.
    type(v, QString::fromUtf8(" xyz"));
    QTest::keyClick(v, Qt::Key_Z, Qt::ControlModifier);
    CHECK(toUpper(para(10).text) == U"EXT. ROOFTOP - NIGHT");

    // Diálogo duplo pelo atalho, com o cursor na fala do Bruno (parágrafos 5 e 6).
    const_cast<Editor&>(ed).setCursor({6, 0});
    QTest::keyClick(v, Qt::Key_D, Qt::ControlModifier | Qt::ShiftModifier);
    CHECK(para(5).dual);
    {
        const Layout& L = ed.layout();
        const LayoutLine& left = L.lines[L.paraLines[2].front()];
        const LayoutLine& right = L.lines[L.paraLines[5].front()];
        CHECK(left.row == right.row && right.col > left.col);
    }
    QTest::keyClick(v, Qt::Key_D, Qt::ControlModifier | Qt::ShiftModifier);  // desliga
    CHECK(!para(5).dual);
    QTest::keyClick(v, Qt::Key_D, Qt::ControlModifier | Qt::ShiftModifier);
    QTest::keyClick(v, Qt::Key_Z, Qt::ControlModifier);
    CHECK(!para(5).dual);
    QTest::keyClick(v, Qt::Key_End, Qt::ControlModifier);

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
    type(v, QString::fromUtf8("Ana climbs the fire escape. Up top, Bruno is already waiting with two beers."));
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
    CHECK(heading(0) == U"EXT. ROOFTOP - NIGHT");
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
    CHECK(heading(0) == U"INT. ANA'S APARTMENT - NIGHT");
    CHECK(b->selectedScene() == 0);

    // Desfazer pelo atalho da janela, com o quadro em foco.
    QTest::keyClick(b, Qt::Key_Z, Qt::ControlModifier);
    CHECK(heading(0) == U"EXT. ROOFTOP - NIGHT");
    QTest::keyClick(b, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    CHECK(heading(0) == U"INT. ANA'S APARTMENT - NIGHT");

    // Enter abre a cena selecionada no roteiro.
    QTest::keyClick(b, Qt::Key_End);
    QTest::keyClick(b, Qt::Key_Return);
    CHECK(!w.cardsMode());
    CHECK(ed.cursor().para == ed.sceneSummaries().at(1).para);
    QTest::keyClick(v, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
    QTest::qWait(300);

    if (argc > 1) {
        w.grab().save(QString::fromLocal8Bit(argv[1]));
        std::printf("Screenshot saved to %s\n", argv[1]);
    }
    if (argc > 2) {
        // Captura de um menu aberto (View).
        for (QAction* a : w.menuBar()->actions()) {
            if (a->text().contains("View")) {
                QMenu* m = a->menu();
                m->popup(w.mapToGlobal(QPoint(300, 40)));
                QTest::qWait(300);
                m->grab().save(QString::fromLocal8Bit(argv[2]));
                m->hide();
            }
        }
    }
    // ---- Fotos dos personagens, página de rosto e impressão (com capturas opcionais)
    {
        QFile f("examples/the_last_show.fountain");
        if (f.open(QIODevice::ReadOnly)) {
            Document sample = parseFountain(f.readAll().toStdString());
            QString err;
            const std::string owl = avatars::fromImageFile(":/owlio.png", &err);
            CHECK(!owl.empty());
            sample.setAvatar(U"HELENA", owl);
            // Uma "foto" gerada para o Tomás.
            QImage img(256, 256, QImage::Format_RGB32);
            QPainter ip(&img);
            QLinearGradient g(0, 0, 256, 256);
            g.setColorAt(0, QColor("#4f7cff"));
            g.setColorAt(1, QColor("#9b5cff"));
            ip.fillRect(img.rect(), g);
            ip.setPen(Qt::white);
            QFont big = ip.font();
            big.setPixelSize(120);
            big.setBold(true);
            ip.setFont(big);
            ip.drawText(img.rect(), Qt::AlignCenter, "T");
            ip.end();
            QTemporaryDir tmp;
            img.save(tmp.filePath("tomas.png"));
            const std::string tomas = avatars::fromImageFile(tmp.filePath("tomas.png"), &err);
            CHECK(!tomas.empty());
            sample.setAvatar(U"TOMAS", tomas);
            QFile out(tmp.filePath("com_fotos.fountain"));
            if (out.open(QIODevice::WriteOnly)) {
                out.write(QByteArray::fromStdString(writeFountain(sample)));
                out.close();
            }
            // Abre em modo "sem alterações" (o editor atual tem mudanças: descarta).
            const_cast<Editor&>(ed).markSaved();
            CHECK(w.openFile(tmp.filePath("com_fotos.fountain")));
            CHECK(ed.doc().avatar(U"HELENA") == owl);
            CHECK(!ed.doc().titleValue("Title").empty());
            w.setCardsMode(true);
            QTest::qWait(400);
            if (argc > 3) w.grab().save(QString::fromLocal8Bit(argv[3]));

            TitlePageDialog title(ed.doc(), &w);
            title.resize(980, 660);
            title.show();
            QTest::qWait(250);
            CHECK(!title.values().empty() && title.values()[0].second == ed.doc().titleValue("Title"));
            // Enter num campo salva; não pode acionar "Remove Title Page".
            for (QPushButton* pb : title.findChildren<QPushButton*>())
                CHECK(pb->isDefault() == (pb->text() == "Save"));
            if (argc > 4) title.grab().save(QString::fromLocal8Bit(argv[4]));
            title.reject();

            PrintDialog print(ed, PrintDialog::Target::Pdf, tmp.filePath("x.pdf"), true, &w);
            print.resize(1100, 720);
            print.show();
            QTest::qWait(250);
            CHECK(print.options().titlePage);
            CHECK(print.options().sceneNumbers);
            if (auto* wm = print.findChild<QLineEdit*>()) wm->setText("draft");
            QTest::qWait(150);
            CHECK(print.options().watermark == "draft");
            if (argc > 5) print.grab().save(QString::fromLocal8Bit(argv[5]));
            print.reject();

            // ---- Ficha do personagem: clicar no painel abre a janela; renomear troca as deixas.
            CHECK(ed.doc().character(U"HELENA") && ed.doc().character(U"HELENA")->role == U"Protagonist");
            const int helenaCues = ed.characterStats(U"HELENA").speeches;
            CHECK(helenaCues == 6);
            CHECK(ed.characterStats(U"HELENA").scenes.size() == 3);
            auto* dock = w.findChild<QDockWidget*>("characters");
            auto* list = dock ? qobject_cast<QListWidget*>(dock->widget()) : nullptr;
            CHECK(list != nullptr);
            QListWidgetItem* helena = nullptr;
            for (int i = 0; list && i < list->count(); ++i)
                if (list->item(i)->data(Qt::UserRole).toString() == "HELENA") helena = list->item(i);
            CHECK(helena != nullptr);
            bool sawDialog = false;
            QTimer::singleShot(400, &w, [&] {
                auto* dlg = qobject_cast<CharacterDialog*>(QApplication::activeModalWidget());
                if (!dlg) return;
                sawDialog = true;
                auto edits = dlg->findChildren<QLineEdit*>();
                QLineEdit* nameEdit = nullptr;
                for (QLineEdit* e : edits)
                    if (e->text() == "HELENA") nameEdit = e;
                CHECK(nameEdit != nullptr);
                QPlainTextEdit* synopsis = nullptr;
                for (QPlainTextEdit* e : dlg->findChildren<QPlainTextEdit*>())
                    if (e->toPlainText().startsWith("Has come")) synopsis = e;
                CHECK(synopsis != nullptr);
                CHECK(dlg->findChild<QListWidget*>()->count() == 3);  // cenas em que fala
                for (QPushButton* pb : dlg->findChildren<QPushButton*>())
                    CHECK(pb->isDefault() == (pb->text() == "Save"));
                if (argc > 6) dlg->grab().save(QString::fromLocal8Bit(argv[6]));
                if (synopsis) synopsis->setPlainText("Wants to see the sea.");
                if (nameEdit) {
                    nameEdit->selectAll();
                    nameEdit->setFocus();
                    type(nameEdit, QString::fromUtf8("Helena Sá"));
                }
                CHECK(dlg->info().name == U"HELENA SÁ");
                for (QPushButton* b : dlg->findChildren<QPushButton*>())
                    if (b->text() == "Save") QTest::mouseClick(b, Qt::LeftButton);
            });
            if (list && helena)
                QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                                  list->visualItemRect(helena).center());
            CHECK(sawDialog);
            CHECK(ed.doc().character(U"HELENA") == nullptr);
            const CharacterInfo* renamed = ed.doc().character(U"HELENA SÁ");
            CHECK(renamed && renamed->synopsis == U"Wants to see the sea." && renamed->photo == owl);
            CHECK(ed.characterStats(U"HELENA SÁ").speeches == helenaCues);
            CHECK(ed.characterStats(U"HELENA").speeches == 0);
            for (QAction* a : w.findChildren<QAction*>())
                if (a->shortcut() == QKeySequence(QKeySequence::Undo)) a->trigger();
            CHECK(ed.characterStats(U"HELENA").speeches == helenaCues);
            CHECK(ed.doc().character(U"HELENA") && ed.doc().character(U"HELENA")->photo == owl);

            // ---- Configurações
            SettingsDialog settings(&w);
            settings.show();
            QTest::qWait(200);
            CHECK(settings.uiLanguage() == "en");
            CHECK(settings.scriptLanguage() == ScriptLanguage::English);
            CHECK(settings.autosaveMinutes() == 5);
            CHECK(settings.keepBackups());
            if (argc > 7) settings.grab().save(QString::fromLocal8Bit(argv[7]));
            settings.reject();

            // ---- Salvamento automático, backups e recuperação
            const QString scriptPath = w.currentPath();
            CHECK(!scriptPath.isEmpty());
            w.setCardsMode(false);
            v->setFocus();
            QTest::keyClick(v, Qt::Key_End, Qt::ControlModifier);
            QTest::keyClick(v, Qt::Key_Return);
            type(v, "Autosaved line.");
            CHECK(ed.modified());
            w.writeRecoveryNow();
            CHECK(autosave::orphans().isEmpty());  // a cópia desta janela está travada por ela
            CHECK(QDir(autosave::recoveryDir()).entryList({"*.fountain"}, QDir::Files).size() == 1);
            w.autosaveNow();
            CHECK(!ed.modified());
            QFile saved(scriptPath);
            CHECK(saved.open(QIODevice::ReadOnly) && saved.readAll().contains("Autosaved line."));
            CHECK(autosave::backups(scriptPath).size() == 1);
            CHECK(QDir(autosave::recoveryDir()).entryList({"*.fountain"}, QDir::Files).isEmpty());
            // Outro salvamento automático logo depois não cria backup novo (no máximo um a cada 10 min).
            type(v, " More.");
            w.autosaveNow();
            CHECK(autosave::backups(scriptPath).size() == 1);

            // Uma "execução" que não fechou direito deixa uma cópia órfã.
            {
                autosave::Session crashed;
                crashed.write(QByteArray::fromStdString(writeFountain(sample)), scriptPath);
            }
            const auto lost = autosave::orphans();
            CHECK(lost.size() == 1);
            if (!lost.isEmpty()) {
                CHECK(lost.first().originalPath == scriptPath);
                w.recover(lost.first());
                CHECK(ed.modified());
                CHECK(w.currentPath() == scriptPath);
                CHECK(ed.doc().paragraphs.size() == sample.paragraphs.size());
                CHECK(autosave::orphans().isEmpty());
            }
            const_cast<Editor&>(ed).markSaved();
        }
    }

    std::printf(failures == 0 ? "UI test passed.\n" : "%d failure(s).\n", failures);
    return failures == 0 ? 0 : 1;
}
