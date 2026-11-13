// UI test: types a scene into the real window using Qt keyboard and mouse events.
// Usage: owlio_gui_test [script.png] [menu.png] [cards.png] [title.png] [print.png] [character.png] [settings.png]
//                       [beatboard.png] [beat.png]
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
#include "pagepainter.h"
#include "pixelavatar.h"
#include <QFile>
#include <QFontDatabase>
#include <QToolButton>
#include <QLineEdit>
#include <QLinearGradient>
#include <QPainter>
#include <QTemporaryDir>

#include <QClipboard>
#include <QStatusBar>
#include <QDir>
#include <QDockWidget>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QTranslator>

#include "autosave.h"
#include "beatboard.h"
#include "beatdialog.h"
#include "avatars.h"
#include "characterdialog.h"
#include "fountain.h"
#include "i18n.h"
#include "settingsdialog.h"
#include "printdialog.h"
#include "sceneboard.h"
#include "titlepagedialog.h"
#include "castdialog.h"
#include "relationmap.h"
#include "timeline.h"
#include <QTreeWidget>
#include <QComboBox>
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
    QApplication::setStyle(ThemeManager::createStyle());
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

    // Modo lista: uma faixa por cena, uma embaixo da outra; o alternador do cabeçalho troca o modo.
    {
        const QRectF gridToggle = b->cardRect(0);  // só para garantir que o quadro já tem tamanho
        CHECK(!b->listMode() && gridToggle.width() > 0);
        QWidget* bvp = b->viewport();
        const QPointF listBtn(bvp->width() - 32 - 17, 24 + 15);  // botão "lista", à direita do cabeçalho
        QTest::mouseClick(bvp, Qt::LeftButton, Qt::NoModifier, listBtn.toPoint());
        CHECK(b->listMode());
        const QRectF r0 = b->cardRect(0), r1 = b->cardRect(1);
        CHECK(r1.top() > r0.bottom() && r0.left() == r1.left() && r0.height() < 100);
        QTest::keyClick(b, Qt::Key_Home);
        QTest::keyClick(b, Qt::Key_Down);
        CHECK(b->selectedScene() == 1);
        if (argc > 18) b->grab().save(QString::fromLocal8Bit(argv[18]));
        b->setListMode(false);
    }

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
            CHECK(settings.scriptFont().isEmpty());
            CHECK(settings.interfaceFont().isEmpty());
            CHECK(settings.interfaceFontSize() == ThemeManager::kDefaultFontSize);
            CHECK(!settings.reopenLastScript());
            if (argc > 7) settings.grab().save(QString::fromLocal8Bit(argv[7]));
            settings.showPage(2);
            QTest::qWait(100);
            if (argc > 10) settings.grab().save(QString::fromLocal8Bit(argv[10]));
            if (argc > 11) {  // janela de atalhos com as teclas desenhadas
                QTimer::singleShot(300, &w, [&] {
                    if (QWidget* d = QApplication::activeModalWidget()) {
                        d->grab().save(QString::fromLocal8Bit(argv[11]));
                        d->close();
                    }
                });
                for (QAction* a : w.findChildren<QAction*>())
                    if (a->shortcut() == QKeySequence(QKeySequence::HelpContents)) a->trigger();
            }
            settings.reject();

            // ---- Criador de avatar em pixel art
            {
                PixelAvatarDialog pav("HELENA", &w);
                pav.show();
                QTest::qWait(100);
                QImage img;
                CHECK(img.loadFromData(QByteArray::fromBase64(QByteArray::fromStdString(pav.base64()))));
                CHECK(img.width() == 192 && img.height() == 192);
                // Trocar uma cor e um estilo de cabelo, desfazer e refazer.
                const PixelAvatar before = pav.avatar();
                auto swatches = pav.findChildren<QToolButton*>("corAvatar");
                CHECK(swatches.size() == 12);  // começa pelo cabelo: cores do cabelo
                const int target = before.hairColor == 3 ? 4 : 3;
                swatches[target]->click();
                QTest::qWait(20);
                CHECK(pav.avatar().hairColor == target);
                auto* opts = pav.findChild<QListWidget*>("opcoesAvatar");
                CHECK(opts && opts->count() == 15);
                emit opts->itemClicked(opts->item(7));
                CHECK(pav.avatar().hairStyle == 7);
                QPushButton* undoB = nullptr;
                for (QPushButton* pb : pav.findChildren<QPushButton*>())
                    if (pb->toolTip() == "Undo") undoB = pb;
                CHECK(undoB);
                if (undoB) { undoB->click(); undoB->click(); }
                CHECK(pav.avatar().hairColor == before.hairColor && pav.avatar().hairStyle == before.hairStyle);
                pav.setCategory(8);
                CHECK(opts->count() == 7);
                // O tipo de corpo fica à vista, fora das categorias.
                QToolButton* masc = pav.findChildren<QToolButton*>("corpo").value(1);
                CHECK(masc && masc->isVisible());
                if (masc) masc->click();
                CHECK(pav.avatar().body == 1 && masc && masc->isChecked());
                QTest::qWait(60);
                if (argc > 12) pav.grab().save(QString::fromLocal8Bit(argv[12]));
                pav.reject();
                if (argc > 13) {  // folha com variações, para conferir o desenho
                    QImage sheet(8 * 100, 5 * 100, QImage::Format_RGB32);
                    sheet.fill(Qt::white);
                    QPainter sp(&sheet);
                    for (int i = 0; i < 24; ++i)
                        sp.drawImage((i % 8) * 100 + 2, (i / 8) * 100 + 2, PixelAvatar::random(i * 7919u + 3).render(96));
                    for (int i = 0; i < 15; ++i) {  // todos os cabelos
                        PixelAvatar h = PixelAvatar::random(11);
                        h.hairStyle = i;
                        h.hat = 0;
                        h.glasses = 0;
                        sp.drawImage((i % 8) * 100 + 2, 300 + (i / 8) * 100 + 2, h.render(96));
                    }
                    sp.end();
                    sheet.save(QString::fromLocal8Bit(argv[13]));
                }
            }

            // ---- Fonte das páginas: qualquer monoespaçada, sem mudar a paginação.
            const int pagesBefore = ed.layout().pageCount;
            const QString autoFont = PagePainter::fontFamily();
            PagePainter::setPreferredFontFamily("DejaVu Sans Mono");
            if (QFontDatabase::families().contains("DejaVu Sans Mono"))
                CHECK(PagePainter::fontFamily() == "DejaVu Sans Mono");
            CHECK(ed.layout().pageCount == pagesBefore);
            PagePainter::setPreferredFontFamily(QString());
            CHECK(PagePainter::fontFamily() == autoFont);

            // ---- Todo item de menu tem ícone.
            for (QAction* top : w.menuBar()->actions()) {
                if (!top->menu()) continue;
                for (QAction* a : top->menu()->actions()) {
                    if (a->isSeparator()) continue;
                    if (a->icon().isNull()) std::printf("Menu item without icon: %s\n", qPrintable(a->text()));
                    CHECK(!a->icon().isNull());
                }
            }

            // ---- Botão da barra lateral esconde e mostra os dois painéis.
            CHECK(w.sidebarVisible() && w.sidebarAction()->isChecked());
            w.sidebarAction()->trigger();
            QTest::qWait(50);
            CHECK(!w.sidebarVisible() && !w.sidebarAction()->isChecked());
            CHECK(w.findChild<QDockWidget*>("scenes")->isHidden());
            w.sidebarAction()->trigger();
            QTest::qWait(50);
            CHECK(w.sidebarVisible() && w.sidebarAction()->isChecked());
            w.findChild<QDockWidget*>("scenes")->hide();
            w.findChild<QDockWidget*>("characters")->hide();
            QTest::qWait(50);
            CHECK(!w.sidebarAction()->isChecked());
            w.setSidebarVisible(true);

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

            // ---- Beat Board
            // Depois das janelas modais a principal pode não estar ativa: dispara as ações pelo atalho delas.
            auto shortcut = [&](const QKeySequence& k) {
                for (QAction* a : w.findChildren<QAction*>())
                    if (a->shortcuts().contains(k)) {
                        a->trigger();
                        return;
                    }
            };
            shortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
            CHECK(w.viewMode() == MainWindow::ViewMode::Beats);
            BeatBoardView* bb = w.beatBoard();
            CHECK(bb->beatCount() == 6 && ed.doc().board.flows.size() == 5);
            QWidget* bvp = bb->viewport();
            auto bmouse = [&](QEvent::Type t, QPoint pos, Qt::MouseButtons buttons, Qt::MouseButton b = Qt::LeftButton) {
                QMouseEvent ev(t, QPointF(pos), bvp->mapToGlobal(QPointF(pos)), b, buttons, Qt::NoModifier);
                QApplication::sendEvent(bvp, &ev);
            };
            auto drag = [&](QPoint from, QPoint to) {
                bmouse(QEvent::MouseButtonPress, from, Qt::LeftButton);
                bmouse(QEvent::MouseMove, from + (to - from) / 3, Qt::LeftButton);
                bmouse(QEvent::MouseMove, to, Qt::LeftButton);
                bmouse(QEvent::MouseButtonRelease, to, Qt::NoButton);
            };
            auto click = [&](QPoint at) {
                bmouse(QEvent::MouseButtonPress, at, Qt::LeftButton);
                bmouse(QEvent::MouseButtonRelease, at, Qt::NoButton);
            };
            bb->zoomToFit();
            QTest::qWait(150);

            // Arrastar o beat 1 move o cartão e é um passo de desfazer.
            const double x1 = ed.doc().board.beat(1)->x;
            QPoint c1 = bb->beatCenter(1);
            drag(c1, c1 + QPoint(0, -60));
            CHECK(ed.doc().board.beat(1)->x == x1 && ed.doc().board.beat(1)->y < 0);
            shortcut(QKeySequence(QKeySequence::Undo));
            CHECK(ed.doc().board.beat(1)->y == 0);

            // Ligar o beat 6 ao beat 1 arrastando a bolinha da borda.
            click(bb->beatCenter(6));
            CHECK(bb->selectedBeats() == QList<int>({6}));
            drag(bb->connectorPoint(6), bb->beatCenter(1));
            CHECK(ed.doc().board.flows.size() == 6);
            CHECK(ed.doc().board.flows.back() == (FlowLine{6, 1}));

            // Redimensionar pelo canto.
            const double w6 = ed.doc().board.beat(6)->w;
            drag(bb->resizePoint(6), bb->resizePoint(6) + QPoint(40, 30));
            CHECK(ed.doc().board.beat(6)->w > w6);

            // Duplo clique no vazio cria um beat e abre a janela dele.
            bool sawBeatDialog = false;
            QTimer::singleShot(300, &w, [&] {
                auto* dlg = qobject_cast<BeatDialog*>(QApplication::activeModalWidget());
                if (!dlg) return;
                sawBeatDialog = true;
                if (auto* t = dlg->findChild<QLineEdit*>()) t->setText("Interval");
                if (auto* t = dlg->findChild<QPlainTextEdit*>()) t->setPlainText("Popcorn and silence.");
                for (QPushButton* pb : dlg->findChildren<QPushButton*>()) CHECK(pb->isDefault() == (pb->text() == "Save"));
                if (argc > 9) dlg->grab().save(QString::fromLocal8Bit(argv[9]));
                for (QPushButton* pb : dlg->findChildren<QPushButton*>())
                    if (pb->text() == "Save") QTest::mouseClick(pb, Qt::LeftButton);
            });
            const QPoint empty(bvp->width() - 60, bvp->height() - 40);
            QMouseEvent dbl(QEvent::MouseButtonDblClick, QPointF(empty), bvp->mapToGlobal(QPointF(empty)), Qt::LeftButton,
                            Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(bvp, &dbl);
            CHECK(sawBeatDialog);
            CHECK(bb->beatCount() == 7);
            const int interval = ed.doc().board.beats.back().id;
            CHECK(ed.doc().board.beats.back().title == U"Interval");
            CHECK(ed.doc().board.beats.back().text == U"Popcorn and silence.");

            // Colar uma imagem cria um beat com ela.
            QImage pasted(320, 200, QImage::Format_RGB32);
            {
                QPainter ip(&pasted);
                QLinearGradient g(0, 0, 320, 200);
                g.setColorAt(0, QColor("#1d4e89"));
                g.setColorAt(0.55, QColor("#4fb0c6"));
                g.setColorAt(1, QColor("#f6d7a7"));
                ip.fillRect(pasted.rect(), g);
            }
            QApplication::clipboard()->setImage(pasted);
            QTest::qWait(100);
            QTest::keyClick(bb, Qt::Key_V, Qt::ControlModifier);
            CHECK(bb->beatCount() == 8);
            CHECK(ed.doc().board.beats.back().image != nullptr);

            // Apagar com Delete; desfazer volta.
            bb->selectBeat(interval);
            QTest::keyClick(bb, Qt::Key_Delete);
            CHECK(bb->beatCount() == 7 && !ed.doc().board.beat(interval));
            shortcut(QKeySequence(QKeySequence::Undo));
            CHECK(bb->beatCount() == 8);

            // O quadro vai junto ao salvar e volta ao abrir.
            Document reread = parseFountain(writeFountain(ed.doc()));
            CHECK(reread.board.beats.size() == 8 && reread.board.flows.size() == 6);
            CHECK(reread.board.beats.back().image && *reread.board.beats.back().image == *ed.doc().board.beats.back().image);

            // Captura para o README: imagens em dois beats, organizados.
            {
                std::vector<Beat> tidy;
                Beat last = ed.doc().board.beats.back();
                last.x = 936; last.y = 0; last.w = 240; last.h = 366;
                last.title = U"The real sea"; last.text = U"Where Helena is going.";
                tidy.push_back(last);
                Beat five = *ed.doc().board.beat(5);
                five.image = std::make_shared<const std::string>(beatimage::fromImage(pasted.mirrored(true, false)));
                five.h = 240;
                tidy.push_back(five);
                Beat iv = *ed.doc().board.beat(interval);
                iv.x = 936; iv.y = 420; iv.h = 120;
                tidy.push_back(iv);
                const_cast<Editor&>(ed).updateBeats(tidy);
                w.view()->refreshAfterExternalEdit();
                bb->scene()->clearSelection();
                bb->zoomToFit();
                w.statusBar()->clearMessage();
                QTest::qWait(250);
                if (argc > 8) w.grab().save(QString::fromLocal8Bit(argv[8]));
            }
            const_cast<Editor&>(ed).markSaved();
        }
    }

    // ---- Personagens, mapa de relações e linha do tempo
    {
        const_cast<Editor&>(ed).markSaved();
        CHECK(w.openFile("examples/the_last_show.fountain"));
        const auto chars = ed.characters();
        CHECK(chars.size() >= 3);
        if (chars.size() >= 3) {
            const QString a = QString::fromStdU32String(chars[0].first);
            const QString b = QString::fromStdU32String(chars[1].first);
            const QString third = QString::fromStdU32String(chars[2].first);
            const int cues = chars[1].second;

            // Edição na tabela vale para o roteiro inteiro.
            CastDialog cast(const_cast<Editor*>(&ed), &w);
            cast.resize(1040, 640);
            cast.show();
            QTest::qWait(100);
            CHECK(cast.table()->topLevelItemCount() >= 3);
            CHECK(cast.editCell(b, 2, "34"));
            CHECK(ed.doc().character(b.toStdU32String()) && ed.doc().character(b.toStdU32String())->age == U"34");
            CHECK(cast.editCell(b, 0, "Bruna"));
            CHECK(ed.characterStats(U"BRUNA").speeches == cues && ed.characterStats(b.toStdU32String()).speeches == 0);
            CHECK(ed.doc().character(U"BRUNA") && ed.doc().character(U"BRUNA")->age == U"34");
            CHECK(!cast.editCell("BRUNA", 0, "  "));  // nome vazio é recusado
            QTest::qWait(60);
            if (argc > 14) cast.grab().save(QString::fromLocal8Bit(argv[14]));
            cast.reject();

            // Mapa: ligar, arrastar e renomear sem perder as ligações.
            w.resize(1400, 900);
            w.setViewMode(MainWindow::ViewMode::Relations);
            QTest::qWait(250);
            RelationMapView* map = w.relationMap();
            CHECK(w.viewMode() == MainWindow::ViewMode::Relations);
            CHECK(map->names().contains(a) && map->names().contains("BRUNA"));
            map->link(a, "BRUNA", RelationKind::Family, "siblings");
            CHECK(ed.doc().relations.link(a.toStdU32String(), U"BRUNA") != nullptr);
            // Arrastar do "+" de um personagem até outro abre a janela da ligação.
            auto* closer = new QTimer(&w);  // fecha a janela da ligação assim que ela abrir
            QObject::connect(closer, &QTimer::timeout, closer, [closer] {
                if (auto* d = qobject_cast<RelationDialog*>(QApplication::activeModalWidget())) {
                    d->accept();
                    closer->deleteLater();
                }
            });
            closer->start(50);
            const QPoint from = map->nodeCenter(third);
            QTest::mouseMove(map, from);
            QTest::mouseMove(map, map->handlePoint(third));
            QTest::mousePress(map, Qt::LeftButton, {}, map->handlePoint(third));
            QTest::mouseMove(map, map->nodeCenter(a));
            QTest::mouseRelease(map, Qt::LeftButton, {}, map->nodeCenter(a));
            QTest::qWait(400);
            CHECK(ed.doc().relations.link(third.toStdU32String(), a.toStdU32String()) != nullptr);
            // Arrastar um personagem grava a posição.
            const QPoint c0 = map->nodeCenter(third);
            QTest::mousePress(map, Qt::LeftButton, {}, c0);
            QTest::mouseMove(map, c0 + QPoint(40, 20));
            QTest::mouseMove(map, c0 + QPoint(80, 40));
            QTest::mouseRelease(map, Qt::LeftButton, {}, c0 + QPoint(80, 40));
            CHECK(map->nodeCenter(third) != c0);
            CharacterInfo bia;
            bia.name = U"BEATRIZ";
            const_cast<Editor&>(ed).setCharacter(U"BRUNA", bia);
            w.view()->refreshAfterExternalEdit();
            CHECK(ed.doc().relations.link(a.toStdU32String(), U"BEATRIZ") != nullptr);
            CHECK(map->names().contains("BEATRIZ") && !map->names().contains("BRUNA"));
            map->zoomToFit();
            QTest::qWait(150);
            if (argc > 15) w.grab().save(QString::fromLocal8Bit(argv[15]));

            // Linha do tempo: clicar numa cena abre o roteiro nela.
            w.setViewMode(MainWindow::ViewMode::Timeline);
            QTest::qWait(250);
            TimelineView* tl = w.timeline();
            CHECK(w.viewMode() == MainWindow::ViewMode::Timeline);
            // O cabeçalho da linha do tempo não pode travar a largura da barra lateral.
            CHECK(tl->minimumSizeHint().width() < 900);
            // Zoom nas páginas escritas (o roteiro de exemplo é curto).
            QToolButton* fitW = tl->findChild<QToolButton*>("alternarLinha");
            CHECK(fitW);
            if (fitW && !fitW->isChecked()) fitW->click();
            QTest::qWait(100);
            if (argc > 16) w.grab().save(QString::fromLocal8Bit(argv[16]));
            QPoint hit;
            int idx = -1;
            for (int y = 0; y < tl->canvas()->height() && idx < 0; y += 4)
                for (int x = 200; x < tl->canvas()->width() && idx < 0; x += 6)
                    if ((idx = tl->sceneAt(QPoint(x, y))) >= 0) hit = QPoint(x, y);
            CHECK(idx >= 0);
            if (idx >= 0) {
                QTest::mouseClick(tl->canvas(), Qt::LeftButton, {}, hit);
                CHECK(w.viewMode() == MainWindow::ViewMode::Script);
                CHECK(ed.cursor().para == ed.sceneSummaries().at(idx).para);
            }
            if (fitW) fitW->setChecked(false);

            // Forma narrativa: preset e arraste da divisa entre atos.
            w.setViewMode(MainWindow::ViewMode::Timeline);
            QTest::qWait(150);
            CHECK(ed.doc().structure.empty());  // padrão: convencional, fora do arquivo
            QComboBox* shape = nullptr;
            for (QComboBox* cb : tl->findChildren<QComboBox*>())
                if (cb->findData("episode") >= 0) shape = cb;
            CHECK(shape);
            if (shape) {
                const int ep = shape->findData("episode");
                shape->setCurrentIndex(ep);
                emit shape->activated(ep);
                CHECK(ed.doc().structure.preset == "episode" && ed.doc().structure.acts.size() == 6);
                const double before = ed.doc().structure.acts[1].end;
                const QPoint d = tl->dividerPoint(1);
                QTest::mouseMove(tl->canvas(), d);
                QTest::mousePress(tl->canvas(), Qt::LeftButton, {}, d);
                QTest::mouseMove(tl->canvas(), d + QPoint(40, 0));
                QTest::mouseMove(tl->canvas(), d + QPoint(80, 0));
                QTest::mouseRelease(tl->canvas(), Qt::LeftButton, {}, d + QPoint(80, 0));
                CHECK(ed.doc().structure.preset == "custom" && ed.doc().structure.acts[1].end > before);
                CHECK(shape->currentData().toString() == "custom");
                const_cast<Editor&>(ed).undo();
                w.view()->refreshAfterExternalEdit();
                CHECK(ed.doc().structure.acts[1].end == before);
                const int stc = shape->findData("savethecat");
                shape->setCurrentIndex(stc);
                emit shape->activated(stc);
                QTest::qWait(150);
                if (argc > 17) w.grab().save(QString::fromLocal8Bit(argv[17]));
            }
        }
        const_cast<Editor&>(ed).markSaved();
    }

    std::printf(failures == 0 ? "UI test passed.\n" : "%d failure(s).\n", failures);
    return failures == 0 ? 0 : 1;
}
