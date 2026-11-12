#include "mainwindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPageLayout>
#include <QPainter>
#include <QPdfWriter>
#include <QPlainTextEdit>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QVBoxLayout>

#include <QActionGroup>
#include <QScrollBar>
#include <QSet>
#include <QToolButton>
#include <algorithm>

#include <QInputDialog>
#include <QStackedWidget>

#include "avatars.h"
#include "printdialog.h"
#include "titlepagedialog.h"
#include "carddelegate.h"
#include "sceneboard.h"
#include "fdx.h"
#include "fountain.h"
#include "scriptview.h"
#include "theme.h"

namespace {
QString q(const std::u32string& s) { return QString::fromStdU32String(s); }
QString q(const std::string& s) { return QString::fromStdString(s); }
}  // namespace

// ------------------------------------------------------------ Localizar / substituir

class FindDialog : public QDialog {
public:
    FindDialog(Editor* ed, ScriptView* view, QWidget* parent) : QDialog(parent), ed_(ed), view_(view) {
        setWindowTitle(tr("Localizar e substituir"));
        auto* form = new QFormLayout;
        findEdit_ = new QLineEdit;
        replaceEdit_ = new QLineEdit;
        form->addRow(tr("Localizar:"), findEdit_);
        form->addRow(tr("Substituir por:"), replaceEdit_);
        status_ = new QLabel;
        auto* prev = new QPushButton(tr("Anterior"));
        auto* next = new QPushButton(tr("Próximo"));
        auto* rep = new QPushButton(tr("Substituir"));
        auto* all = new QPushButton(tr("Substituir tudo"));
        next->setDefault(true);
        auto* buttons = new QHBoxLayout;
        for (auto* b : {prev, next, rep, all}) buttons->addWidget(b);
        auto* lay = new QVBoxLayout(this);
        lay->addLayout(form);
        lay->addLayout(buttons);
        lay->addWidget(status_);

        connect(next, &QPushButton::clicked, this, [this] { run(ed_->find(query(), true)); });
        connect(prev, &QPushButton::clicked, this, [this] { run(ed_->find(query(), false)); });
        connect(rep, &QPushButton::clicked, this, [this] {
            run(ed_->replaceCurrent(query(), replaceEdit_->text().toStdU32String()));
        });
        connect(all, &QPushButton::clicked, this, [this] {
            int n = ed_->replaceAll(query(), replaceEdit_->text().toStdU32String());
            view_->refreshAfterExternalEdit();
            status_->setText(tr("%n substituição(ões).", "", n));
        });
    }
    void activate() {
        if (ed_->hasSelection()) {
            auto [a, b] = ed_->selection();
            if (a.para == b.para)
                findEdit_->setText(q(ed_->doc().paragraphs[a.para].text.substr(a.off, b.off - a.off)));
        }
        show();
        raise();
        activateWindow();
        findEdit_->setFocus();
        findEdit_->selectAll();
    }

private:
    std::u32string query() const { return findEdit_->text().toStdU32String(); }
    void run(bool found) {
        status_->setText(found ? QString() : tr("Nada encontrado."));
        view_->refreshAfterExternalEdit();
    }
    Editor* ed_;
    ScriptView* view_;
    QLineEdit* findEdit_;
    QLineEdit* replaceEdit_;
    QLabel* status_;
};

// ------------------------------------------------------------ Janela principal

MainWindow::MainWindow() {
    view_ = new ScriptView(&editor_, this);
    board_ = new SceneBoard(&editor_, this);
    stack_ = new QStackedWidget(this);
    stack_->addWidget(view_);
    stack_->addWidget(board_);
    setCentralWidget(stack_);

    connect(board_, &SceneBoard::openScene, this, [this](int para) {
        setCardsMode(false);
        view_->gotoParagraph(para);
    });
    connect(board_, &SceneBoard::moveRequested, this, [this](int from, int to) {
        const int now = editor_.moveScene(from, to);
        view_->refreshAfterExternalEdit();  // recarrega o quadro via documentChanged
        board_->selectScene(now);
        statusBar()->showMessage(tr("Cena %1 movida para a posição %2.").arg(from + 1).arg(now + 1), 2500);
    });
    connect(board_, &SceneBoard::editSynopsisRequested, this, &MainWindow::editSynopsis);
    connect(board_, &SceneBoard::selectionChanged, this, [this](int i) {
        const auto scenes = editor_.scenes();
        if (i < 0 || i >= static_cast<int>(scenes.size())) return;
        editor_.setCursor({scenes[i], 0});
        onCursorMoved();
    });
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);

    // Painel de cenas (navegador).
    sceneList_ = new QListWidget;
    sceneList_->setItemDelegate(new CardDelegate(CardDelegate::Scene, sceneList_));
    addDockWidget(Qt::LeftDockWidgetArea, makeDock(tr("Cenas"), "scenes", sceneList_));
    connect(sceneList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        if (cardsMode()) board_->selectScene(sceneList_->row(it));
        else view_->gotoParagraph(it->data(Qt::UserRole).toInt());
    });

    // Painel de personagens.
    charList_ = new QListWidget;
    charList_->setItemDelegate(new CardDelegate(CardDelegate::Character, charList_));
    addDockWidget(Qt::LeftDockWidgetArea, makeDock(tr("Personagens"), "characters", charList_));
    connect(charList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        editor_.setCursor({0, 0});
        editor_.find(it->data(Qt::UserRole).toString().toStdU32String());
        view_->refreshAfterExternalEdit();
        view_->setFocus();
    });
    charList_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(charList_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* it = charList_->itemAt(pos);
        if (!it) return;
        const QString name = it->data(Qt::UserRole).toString();
        const bool hasPhoto = !editor_.doc().avatar(name.toStdU32String()).empty();
        QMenu menu(this);
        menu.addAction(hasPhoto ? tr("Trocar foto...") : tr("Escolher foto..."), this,
                       [this, name] { chooseAvatar(name); });
        menu.addAction(tr("Remover foto"), this, [this, name] {
            editor_.setAvatar(name.toStdU32String(), {});
            view_->refreshAfterExternalEdit();
        })->setEnabled(hasPhoto);
        menu.addSeparator();
        menu.addAction(tr("Ir para a primeira fala"), this, [this, it] { charList_->itemClicked(it); });
        menu.exec(charList_->viewport()->mapToGlobal(pos));
    });
    connect(charList_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* it) { chooseAvatar(it->data(Qt::UserRole).toString()); });

    for (QListWidget* l : {sceneList_, charList_}) {
        l->setMouseTracking(true);
        l->viewport()->setAttribute(Qt::WA_Hover);
        l->setFrameShape(QFrame::NoFrame);
        l->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    // Barra de status.
    sceneLabel_ = new QLabel;
    pageLabel_ = new QLabel;
    wordLabel_ = new QLabel;
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addPermanentWidget(sceneLabel_);
    statusBar()->addPermanentWidget(pageLabel_);
    statusBar()->addPermanentWidget(wordLabel_);

    buildActions();

    panelTimer_.setSingleShot(true);
    panelTimer_.setInterval(250);
    connect(&panelTimer_, &QTimer::timeout, this, &MainWindow::refreshPanels);
    connect(view_, &ScriptView::documentChanged, this, &MainWindow::onDocumentChanged);
    connect(view_, &ScriptView::cursorMoved, this, &MainWindow::onCursorMoved);
    connect(&ThemeManager::instance(), &ThemeManager::changed, this, &MainWindow::refreshIcons);
    refreshIcons();

    QSettings s;
    restoreGeometry(s.value("geometry").toByteArray());
    restoreState(s.value("windowState").toByteArray());
    view_->setZoom(s.value("zoom", 1.0).toDouble());
    if (!s.contains("geometry")) {
        resize(1320, 900);
        resizeDocks({findChild<QDockWidget*>("scenes")}, {290}, Qt::Horizontal);
    }

    newFile();
    view_->setFocus();
}

QDockWidget* MainWindow::makeDock(const QString& title, const QString& objectName, QWidget* content) {
    auto* dock = new QDockWidget(title, this);
    dock->setObjectName(objectName);
    auto* header = new QLabel(title.toUpper());
    header->setObjectName("tituloPainel");
    QFont f = header->font();
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    header->setFont(f);
    dock->setTitleBarWidget(header);
    dock->setWidget(content);
    dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    return dock;
}

QAction* MainWindow::iconAction(QAction* a, const QString& icon) {
    actionIcons_.insert(a, icon);
    return a;
}

void MainWindow::refreshIcons() {
    ThemeManager& tm = ThemeManager::instance();
    for (auto it = actionIcons_.begin(); it != actionIcons_.end(); ++it) {
        QColor color;
        if (it.key()->property("primario").toBool()) color = tm.colors().accentText;
        it.key()->setIcon(tm.icon(it.value(), color));
    }
    static const char* elementIcons[] = {"el-cabecalho", "el-acao", "el-personagem",
                                         "el-parentetico", "el-dialogo", "el-transicao"};
    for (int i = 0; i < kElementCount; ++i) {
        QIcon ic = tm.icon(elementIcons[i], tm.colors().muted);
        if (elementCombo_) elementCombo_->setItemIcon(i, ic);
        if (i < elementActions_.size()) elementActions_[i]->setIcon(ic);
    }
    panelTimer_.start();
}

void MainWindow::buildActions() {
    // ---- Arquivo
    QMenu* file = menuBar()->addMenu(tr("&Arquivo"));
    QAction* newAct = iconAction(file->addAction(tr("&Novo"), this, &MainWindow::newFile), "novo");
    newAct->setShortcut(QKeySequence::New);
    QAction* openAct = iconAction(file->addAction(tr("&Abrir..."), this, &MainWindow::open), "abrir");
    openAct->setShortcut(QKeySequence::Open);
    QAction* saveAct = iconAction(file->addAction(tr("&Salvar"), this, &MainWindow::save), "salvar");
    saveAct->setShortcut(QKeySequence::Save);
    QAction* saveAsAct = file->addAction(tr("Salvar &como..."), this, &MainWindow::saveAs);
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    file->addSeparator();
    QAction* pdfAct = iconAction(file->addAction(tr("Exportar &PDF..."), this, &MainWindow::exportPdf), "pdf");
    pdfAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    file->addAction(tr("Exportar para Final Draft (.fdx)..."), this, &MainWindow::exportFdx);
    QAction* printAct = iconAction(file->addAction(tr("&Imprimir..."), this, &MainWindow::print), "imprimir");
    printAct->setShortcut(QKeySequence::Print);
    file->addSeparator();
    file->addAction(tr("Sai&r"), this, &QWidget::close)->setShortcut(QKeySequence::Quit);

    // ---- Editar
    QMenu* edit = menuBar()->addMenu(tr("&Editar"));
    undoAct_ = iconAction(edit->addAction(tr("&Desfazer"), view_, &ScriptView::undo), "desfazer");
    undoAct_->setShortcut(QKeySequence::Undo);
    redoAct_ = iconAction(edit->addAction(tr("&Refazer"), view_, &ScriptView::redo), "refazer");
    redoAct_->setShortcuts({QKeySequence::Redo, QKeySequence(Qt::CTRL | Qt::Key_Y)});
    edit->addSeparator();
    edit->addAction(tr("Recor&tar"), view_, &ScriptView::cut)->setShortcut(QKeySequence::Cut);
    edit->addAction(tr("&Copiar"), view_, &ScriptView::copy)->setShortcut(QKeySequence::Copy);
    edit->addAction(tr("C&olar"), view_, &ScriptView::paste)->setShortcut(QKeySequence::Paste);
    edit->addAction(tr("Selecionar &tudo"), view_, &ScriptView::selectAll)->setShortcut(QKeySequence::SelectAll);
    edit->addSeparator();
    QAction* findAct = iconAction(edit->addAction(tr("&Localizar e substituir..."), this, &MainWindow::showFind), "localizar");
    findAct->setShortcut(QKeySequence::Find);

    // ---- Formatar (elementos)
    QMenu* fmt = menuBar()->addMenu(tr("&Formatar"));
    auto* elementGroup = new QActionGroup(this);
    for (int i = 0; i < kElementCount; ++i) {
        Element e = static_cast<Element>(i);
        QAction* a = fmt->addAction(q(std::string(elementName(e))), this, [this, e] { view_->setElement(e); });
        a->setShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_1 + i)));
        a->setCheckable(true);
        elementGroup->addAction(a);
        elementActions_.append(a);
    }

    // ---- Documento
    QMenu* docMenu = menuBar()->addMenu(tr("&Documento"));
    QAction* titleAct = iconAction(docMenu->addAction(tr("Página de &rosto..."), this, &MainWindow::editTitlePage), "rosto");
    QAction* sceneNums = docMenu->addAction(tr("&Numerar cenas"));
    sceneNums->setCheckable(true);
    connect(sceneNums, &QAction::toggled, view_, &ScriptView::setSceneNumbers);
    QAction* enContd = docMenu->addAction(tr("Usar (MORE)/(CONT'D) em inglês"));
    enContd->setCheckable(true);
    connect(enContd, &QAction::toggled, this, [this](bool en) {
        LayoutOptions o;
        o.moreText = en ? U"(MORE)" : U"(MAIS)";
        o.contdText = en ? U"(CONT'D)" : U"(CONT.)";
        editor_.setLayoutOptions(o);
        view_->refreshAfterExternalEdit();
    });

    // ---- Exibir
    QMenu* viewMenu = menuBar()->addMenu(tr("E&xibir"));
    auto* modeGroup = new QActionGroup(this);
    scriptModeAct_ = iconAction(viewMenu->addAction(tr("&Roteiro")), "roteiro");
    scriptModeAct_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    scriptModeAct_->setToolTip(tr("Roteiro  (Ctrl+Shift+R)"));
    cardsModeAct_ = iconAction(viewMenu->addAction(tr("&Cartões")), "cartoes");
    cardsModeAct_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    cardsModeAct_->setToolTip(tr("Cenas como cartões  (Ctrl+Shift+C)"));
    for (QAction* a : {scriptModeAct_, cardsModeAct_}) {
        a->setCheckable(true);
        modeGroup->addAction(a);
    }
    scriptModeAct_->setChecked(true);
    connect(scriptModeAct_, &QAction::triggered, this, [this] { setCardsMode(false); });
    connect(cardsModeAct_, &QAction::triggered, this, [this] { setCardsMode(true); });
    viewMenu->addSeparator();
    QMenu* themeMenu = viewMenu->addMenu(tr("&Tema"));
    actionIcons_.insert(themeMenu->menuAction(), "tema");
    auto* themeGroup = new QActionGroup(this);
    const std::pair<QString, ThemeManager::Mode> modes[] = {
        {tr("Seguir o sistema"), ThemeManager::Mode::System},
        {tr("Claro"), ThemeManager::Mode::Light},
        {tr("Escuro"), ThemeManager::Mode::Dark}};
    for (const auto& [label, mode] : modes) {
        QAction* a = themeMenu->addAction(label, this, [mode] { ThemeManager::instance().setMode(mode); });
        a->setCheckable(true);
        a->setChecked(ThemeManager::instance().mode() == mode);
        themeGroup->addAction(a);
    }
    QAction* focusAct = iconAction(viewMenu->addAction(tr("Modo &foco")), "foco");
    focusAct->setCheckable(true);
    focusAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    focusAct->setToolTip(tr("Modo foco — esmaece tudo menos o trecho atual (Ctrl+Shift+F)"));
    connect(focusAct, &QAction::toggled, view_, &ScriptView::setFocusMode);
    viewMenu->addSeparator();
    viewMenu->addAction(tr("Aumentar zoom"), view_, &ScriptView::zoomIn)->setShortcuts({QKeySequence::ZoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal)});
    viewMenu->addAction(tr("Diminuir zoom"), view_, &ScriptView::zoomOut)->setShortcut(QKeySequence::ZoomOut);
    viewMenu->addAction(tr("Zoom 100%"), view_, &ScriptView::zoomReset)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    viewMenu->addSeparator();
    for (QDockWidget* d : findChildren<QDockWidget*>()) viewMenu->addAction(d->toggleViewAction());

    // ---- Ajuda
    QMenu* help = menuBar()->addMenu(tr("Aj&uda"));
    help->addAction(tr("&Atalhos de teclado"), this, &MainWindow::showShortcuts)->setShortcut(QKeySequence::HelpContents);
    help->addAction(tr("&Sobre o Owlio"), this, &MainWindow::about);

    // ---- Barra de ferramentas
    QToolBar* tb = addToolBar(tr("Principal"));
    tb->setObjectName("main");
    tb->setMovable(false);
    tb->setFloatable(false);
    tb->setIconSize(QSize(18, 18));
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tb->setContextMenuPolicy(Qt::PreventContextMenu);
    auto tip = [](QAction* a, const QString& text) {
        a->setToolTip(QString("%1  <span style='opacity:.6'>%2</span>")
                          .arg(text, a->shortcut().toString(QKeySequence::NativeText)));
    };
    tip(newAct, tr("Novo roteiro"));
    tip(openAct, tr("Abrir"));
    tip(saveAct, tr("Salvar"));
    tip(undoAct_, tr("Desfazer"));
    tip(redoAct_, tr("Refazer"));
    tip(findAct, tr("Localizar e substituir"));
    for (QAction* a : {newAct, openAct, saveAct}) tb->addAction(a);
    tb->addSeparator();
    tb->addAction(undoAct_);
    tb->addAction(redoAct_);
    tb->addSeparator();
    for (QAction* a : {scriptModeAct_, cardsModeAct_}) {
        tb->addAction(a);
        if (auto* b = qobject_cast<QToolButton*>(tb->widgetForAction(a))) {
            b->setObjectName("segmento");
            b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        }
    }
    tb->addSeparator();

    elementCombo_ = new QComboBox;
    elementCombo_->setToolTip(tr("Elemento do parágrafo atual (Ctrl+1 … Ctrl+6)"));
    elementCombo_->setIconSize(QSize(16, 16));
    elementCombo_->setMinimumWidth(170);
    for (int i = 0; i < kElementCount; ++i)
        elementCombo_->addItem(q(std::string(elementName(static_cast<Element>(i)))));
    elementCombo_->setFocusPolicy(Qt::NoFocus);
    connect(elementCombo_, &QComboBox::activated, this,
            [this](int i) { view_->setElement(static_cast<Element>(i)); });
    elementComboAct_ = tb->addWidget(elementCombo_);

    auto* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    tb->addAction(findAct);
    tb->addAction(focusAct);
    tb->addSeparator();
    tb->addAction(titleAct);
    qobject_cast<QToolButton*>(tb->widgetForAction(titleAct))->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    titleAct->setIconText(tr("Página de rosto"));

    QAction* pdfTb = new QAction(tr("Exportar PDF"), this);
    pdfTb->setProperty("primario", true);
    pdfTb->setToolTip(tr("Exportar PDF  (Ctrl+Shift+E)"));
    connect(pdfTb, &QAction::triggered, this, &MainWindow::exportPdf);
    iconAction(pdfTb, "pdf");
    tb->addAction(pdfTb);
    if (auto* b = qobject_cast<QToolButton*>(tb->widgetForAction(pdfTb))) {
        b->setObjectName("primario");
        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }

    // Zoom discreto na barra de status.
    zoomCombo_ = new QComboBox;
    zoomCombo_->setObjectName("plano");
    for (int z : {75, 100, 125, 150, 175, 200}) zoomCombo_->addItem(QString("%1%").arg(z), z / 100.0);
    zoomCombo_->setFocusPolicy(Qt::NoFocus);
    zoomCombo_->setToolTip(tr("Zoom (Ctrl+roda do mouse)"));
    connect(zoomCombo_, &QComboBox::activated, this,
            [this](int i) { view_->setZoom(zoomCombo_->itemData(i).toDouble()); });
    connect(view_, &ScriptView::zoomChanged, this, [this](double z) {
        QString t = QString("%1%").arg(qRound(z * 100));
        zoomCombo_->setPlaceholderText(t);
        zoomCombo_->setCurrentIndex(zoomCombo_->findText(t));
    });
    zoomCombo_->setCurrentIndex(1);
    statusBar()->addPermanentWidget(zoomCombo_);
}


// ------------------------------------------------------------ arquivos

bool MainWindow::maybeSave() {
    if (!editor_.modified()) return true;
    auto r = QMessageBox::warning(this, tr("Owlio"),
                                  tr("O roteiro tem alterações não salvas.\nDeseja salvá-las?"),
                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (r == QMessageBox::Save) return save();
    return r == QMessageBox::Discard;
}

void MainWindow::newFile() {
    if (!maybeSave()) return;
    Document d;
    d.paragraphs.push_back({Element::SceneHeading, U""});
    editor_.setDocument(d);
    path_.clear();
    view_->refresh();
    updateTitle();
}

void MainWindow::open() {
    if (!maybeSave()) return;
    QSettings s;
    QString p = QFileDialog::getOpenFileName(
        this, tr("Abrir roteiro"), s.value("lastDir").toString(),
        tr("Roteiros (*.fountain *.fdx *.txt *.spmd);;Fountain (*.fountain);;Final Draft (*.fdx);;Todos (*)"));
    if (!p.isEmpty()) openFile(p);
}

bool MainWindow::openFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Erro"), tr("Não foi possível abrir %1:\n%2").arg(path, f.errorString()));
        return false;
    }
    const std::string data = f.readAll().toStdString();
    Document d = path.endsWith(".fdx", Qt::CaseInsensitive) ? parseFdx(data) : parseFountain(data);
    editor_.setDocument(std::move(d));
    path_ = path;
    QSettings().setValue("lastDir", QFileInfo(path).absolutePath());
    view_->refresh();
    updateTitle();
    statusBar()->showMessage(tr("Aberto: %1").arg(QFileInfo(path).fileName()), 3000);
    return true;
}

bool MainWindow::writeTo(const QString& path) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Erro"), tr("Não foi possível salvar %1:\n%2").arg(path, f.errorString()));
        return false;
    }
    const std::string data = path.endsWith(".fdx", Qt::CaseInsensitive) ? writeFdx(editor_.doc())
                                                                         : writeFountain(editor_.doc());
    f.write(data.data(), static_cast<qint64>(data.size()));
    if (!f.commit()) {
        QMessageBox::critical(this, tr("Erro"), tr("Falha ao gravar %1.").arg(path));
        return false;
    }
    return true;
}

bool MainWindow::save() {
    if (path_.isEmpty()) return saveAs();
    if (!writeTo(path_)) return false;
    editor_.markSaved();
    updateTitle();
    statusBar()->showMessage(tr("Salvo."), 2000);
    return true;
}

bool MainWindow::saveAs() {
    QSettings s;
    QString suggested = path_.isEmpty() ? s.value("lastDir").toString() + "/roteiro.fountain" : path_;
    QString p = QFileDialog::getSaveFileName(this, tr("Salvar roteiro"), suggested,
                                             tr("Fountain (*.fountain);;Final Draft (*.fdx)"));
    if (p.isEmpty()) return false;
    if (!p.endsWith(".fountain", Qt::CaseInsensitive) && !p.endsWith(".fdx", Qt::CaseInsensitive))
        p += ".fountain";
    path_ = p;
    s.setValue("lastDir", QFileInfo(p).absolutePath());
    return save();
}

void MainWindow::exportFdx() {
    QString base = path_.isEmpty() ? QSettings().value("lastDir").toString() + "/roteiro"
                                   : QFileInfo(path_).absolutePath() + "/" + QFileInfo(path_).completeBaseName();
    QString p = QFileDialog::getSaveFileName(this, tr("Exportar para Final Draft"), base + ".fdx",
                                             tr("Final Draft (*.fdx)"));
    if (p.isEmpty()) return;
    if (!p.endsWith(".fdx", Qt::CaseInsensitive)) p += ".fdx";
    if (writeTo(p)) statusBar()->showMessage(tr("Exportado: %1").arg(QFileInfo(p).fileName()), 4000);
}

QString MainWindow::suggestedPdfPath() const {
    const QString base = path_.isEmpty()
                             ? QSettings().value("lastDir").toString() + "/roteiro"
                             : QFileInfo(path_).absolutePath() + "/" + QFileInfo(path_).completeBaseName();
    return base + ".pdf";
}

void MainWindow::openPrintDialog(bool pdf) {
    PrintDialog dlg(editor_, pdf ? PrintDialog::Target::Pdf : PrintDialog::Target::Printer, suggestedPdfPath(),
                    view_->sceneNumbers(), this);
    dlg.resize(1100, 720);
    if (dlg.exec() != QDialog::Accepted) return;
    if (!dlg.exportedPath().isEmpty())
        statusBar()->showMessage(tr("PDF exportado: %1").arg(QFileInfo(dlg.exportedPath()).fileName()), 5000);
    else
        statusBar()->showMessage(tr("Enviado para a impressora."), 4000);
}

void MainWindow::exportPdf() { openPrintDialog(true); }
void MainWindow::print() { openPrintDialog(false); }

bool MainWindow::exportPdfTo(const QString& p) {
    QPdfWriter w(p);
    w.setResolution(1200);
    w.setPageLayout(QPageLayout(QPageSize(QPageSize::Letter), QPageLayout::Portrait, QMarginsF(0, 0, 0, 0)));
    w.setTitle(q(editor_.doc().titleValue("Title")));
    w.setCreator("Owlio");
    PrintOptions opt;
    opt.sceneNumbers = view_->sceneNumbers();
    renderScript(w, w.resolution(), editor_, opt);
    return QFileInfo(p).size() > 0;
}

// ------------------------------------------------------------ diálogos

void MainWindow::editTitlePage() {
    TitlePageDialog dlg(editor_.doc(), this);
    dlg.resize(980, 660);
    if (dlg.exec() != QDialog::Accepted) return;
    for (const auto& [key, value] : dlg.values()) editor_.setTitleValue(key, value);
    updateTitle();
    view_->refreshAfterExternalEdit();
}

void MainWindow::showFind() {
    if (!findDialog_) findDialog_ = new FindDialog(&editor_, view_, this);
    findDialog_->activate();
}

void MainWindow::showShortcuts() {
    QMessageBox::information(this, tr("Atalhos de teclado"), tr(
        "<h3>Fluxo de escrita</h3>"
        "<table cellpadding=3>"
        "<tr><td><b>Enter</b></td><td>Próximo elemento: Cabeçalho → Ação, Personagem → Diálogo, "
        "Diálogo → Personagem, Transição → Cabeçalho</td></tr>"
        "<tr><td><b>Tab</b></td><td>Ação → Personagem → Transição → Cabeçalho; "
        "Diálogo ↔ Parentético. No fim de uma fala, cria um parentético.</td></tr>"
        "<tr><td><b>Shift+Tab</b></td><td>Elemento anterior</td></tr>"
        "<tr><td><b>Enter em linha vazia</b></td><td>Troca para Ação (ou Cabeçalho)</td></tr>"
        "<tr><td><b>Ctrl+1 … Ctrl+6</b></td><td>Cabeçalho, Ação, Personagem, Parentético, Diálogo, Transição</td></tr>"
        "<tr><td><b>Tab / Enter / →</b></td><td>Aceita a sugestão em cinza (personagens, locações, horários, transições)</td></tr>"
        "</table>"
        "<h3>Geral</h3>"
        "<table cellpadding=3>"
        "<tr><td><b>Ctrl+S / Ctrl+Shift+S</b></td><td>Salvar / Salvar como (.fountain ou .fdx)</td></tr>"
        "<tr><td><b>Ctrl+Shift+E</b></td><td>Exportar PDF</td></tr>"
        "<tr><td><b>Ctrl+F</b></td><td>Localizar e substituir</td></tr>"
        "<tr><td><b>Ctrl+Shift+C / Ctrl+Shift+R</b></td><td>Tela de cartões / roteiro</td></tr>"
        "<tr><td><b>Ctrl+Shift+F</b></td><td>Modo foco</td></tr>"
        "</table>"
        "<h3>Cartões</h3>"
        "<table cellpadding=3>"
        "<tr><td><b>Arrastar</b></td><td>Reordena as cenas (o texto inteiro da cena vai junto)</td></tr>"
        "<tr><td><b>Alt+setas</b></td><td>Move o cartão selecionado</td></tr>"
        "<tr><td><b>Enter / duplo clique</b></td><td>Abre a cena no roteiro</td></tr>"
        "<tr><td><b>F2</b></td><td>Edita a sinopse da cena</td></tr>"
        "<tr><td><b>Ctrl+roda</b></td><td>Cartões maiores ou menores</td></tr>"
        "<tr><td><b>Ctrl+Z / Ctrl+Y</b></td><td>Desfazer / refazer</td></tr>"
        "<tr><td><b>Ctrl+roda / Ctrl+= / Ctrl+-</b></td><td>Zoom</td></tr>"
        "</table>"));
}

void MainWindow::about() {
    QMessageBox::about(this, tr("Sobre o Owlio"),
                       tr("<h3>Owlio 0.1</h3><p>Editor de roteiros em C++ e Qt %1.</p>"
                          "<p>Formatação padrão da indústria (Courier 12, US Letter), "
                          "paginação com (MAIS)/(CONT.), arquivos Fountain e Final Draft (.fdx), "
                          "exportação para PDF.</p>").arg(qVersion()));
}

// ------------------------------------------------------------ atualização da UI

bool MainWindow::cardsMode() const { return stack_ && stack_->currentWidget() == board_; }

void MainWindow::setCardsMode(bool cards) {
    scriptModeAct_->setChecked(!cards);
    cardsModeAct_->setChecked(cards);
    elementComboAct_->setVisible(!cards);
    for (QAction* a : elementActions_) a->setEnabled(!cards);
    if (cards == cardsMode()) return;
    if (cards) {
        board_->reload();
        stack_->setCurrentWidget(board_);
        board_->selectScene(std::max(0, editor_.sceneIndexAt(editor_.cursor().para)));
        board_->setFocus();
    } else {
        stack_->setCurrentWidget(view_);
        view_->refreshAfterExternalEdit();
        view_->setFocus();
    }
}

void MainWindow::editSynopsis(int sceneIndex) {
    const auto scenes = editor_.sceneSummaries();
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(scenes.size())) return;
    const SceneSummary& s = scenes[sceneIndex];
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        this, tr("Sinopse da cena %1").arg(sceneIndex + 1), q(s.heading), q(s.synopsis), &ok);
    if (!ok) return;
    editor_.setSynopsis(s.para, text.toStdU32String());
    view_->refreshAfterExternalEdit();
    board_->selectScene(sceneIndex);
}

void MainWindow::chooseAvatar(const QString& name) {
    QSettings settings;
    const QString path = QFileDialog::getOpenFileName(this, tr("Foto de %1").arg(name),
                                                      settings.value("lastImageDir").toString(),
                                                      avatars::fileFilter());
    if (path.isEmpty()) return;
    settings.setValue("lastImageDir", QFileInfo(path).absolutePath());
    QString error;
    const std::string data = avatars::fromImageFile(path, &error);
    if (data.empty()) {
        QMessageBox::warning(this, tr("Foto"), tr("Não foi possível abrir a imagem:\n%1").arg(error));
        return;
    }
    editor_.setAvatar(name.toStdU32String(), data);
    view_->refreshAfterExternalEdit();
    statusBar()->showMessage(tr("Foto de %1 atualizada.").arg(name), 2500);
}

void MainWindow::onDocumentChanged() {
    if (cardsMode()) board_->reload();
    updateTitle();
    panelTimer_.start();
}

void MainWindow::onCursorMoved() {
    const int el = static_cast<int>(editor_.currentElement());
    elementCombo_->setCurrentIndex(el);
    for (int i = 0; i < elementActions_.size(); ++i) elementActions_[i]->setChecked(i == el);
    undoAct_->setEnabled(editor_.canUndo());
    redoAct_->setEnabled(editor_.canRedo());

    const int pages = editor_.layout().pageCount;
    pageLabel_->setText(tr("Página %1 de %2").arg(view_->currentPage() + 1).arg(pages));
    const int scene = editor_.sceneIndexAt(editor_.cursor().para);
    const int scenes = static_cast<int>(editor_.scenes().size());
    sceneLabel_->setText(scene >= 0 ? tr("Cena %1 de %2").arg(scene + 1).arg(scenes)
                                    : tr("%n cena(s)", "", scenes));
    if (scene >= 0 && scene < sceneList_->count()) {
        QSignalBlocker b(sceneList_);
        sceneList_->setCurrentRow(scene);
    } else {
        sceneList_->clearSelection();
    }
}

void MainWindow::refreshPanels() {
    const Document& d = editor_.doc();
    const Layout& L = editor_.layout();

    // Personagens que falam em cada cena, e em quantas cenas cada um aparece.
    struct SceneInfo { int para; std::vector<std::u32string> cast; };
    std::vector<SceneInfo> scenes;
    QHash<QString, QSet<int>> scenesOf;
    for (int i = 0; i < static_cast<int>(d.paragraphs.size()); ++i) {
        const Paragraph& p = d.paragraphs[i];
        if (p.type == Element::SceneHeading) scenes.push_back({i, {}});
        if (p.type == Element::Character && !scenes.empty()) {
            std::u32string name = toUpper(trim(p.text));
            size_t paren = name.find(U'(');
            if (paren != std::u32string::npos) name = trim(name.substr(0, paren));
            if (name.empty()) continue;
            auto& cast = scenes.back().cast;
            if (std::find(cast.begin(), cast.end(), name) == cast.end()) cast.push_back(name);
            scenesOf[q(name)].insert(static_cast<int>(scenes.size()));
        }
    }

    sceneList_->setUpdatesEnabled(false);
    const int keep = sceneList_->verticalScrollBar()->value();
    sceneList_->clear();
    for (size_t i = 0; i < scenes.size(); ++i) {
        std::u32string h = trim(displayText(d.paragraphs[scenes[i].para]));
        QStringList cast;
        for (const auto& n : scenes[i].cast) cast << q(n);
        QString sub = tr("Pág. %1").arg(L.pageOfPara(scenes[i].para) + 1);
        if (!cast.isEmpty()) sub += "  ·  " + cast.join(", ");
        auto* it = new QListWidgetItem(h.empty() ? tr("(sem título)") : q(h));
        it->setData(Qt::UserRole, scenes[i].para);
        it->setData(CardDelegate::kSubtitleRole, sub);
        it->setData(CardDelegate::kBadgeRole, QString::number(i + 1));
        it->setToolTip(q(h));
        sceneList_->addItem(it);
    }
    sceneList_->verticalScrollBar()->setValue(keep);
    sceneList_->setUpdatesEnabled(true);

    charList_->clear();
    for (const auto& [name, count] : editor_.characters()) {
        auto* it = new QListWidgetItem(q(name));
        it->setData(Qt::UserRole, q(name));
        it->setData(CardDelegate::kBadgeRole, tr("%n fala(s)", "", count));
        it->setData(CardDelegate::kAvatarRole, QByteArray::fromStdString(editor_.doc().avatar(name)));
        it->setToolTip(tr("%n fala(s) em %1 cena(s)", "", count).arg(scenesOf.value(q(name)).size()) +
                       "\n" + tr("Duplo clique ou clique direito para escolher uma foto"));
        charList_->addItem(it);
    }
    wordLabel_->setText(tr("%n palavra(s)", "", editor_.wordCount()));
    onCursorMoved();
}


void MainWindow::updateTitle() {
    QString name = path_.isEmpty() ? tr("Sem título") : QFileInfo(path_).fileName();
    std::string t = editor_.doc().titleValue("Title");
    QString full = name + "[*]";
    if (!t.empty()) full = q(t) + " (" + name + ")[*]";
    setWindowTitle(full + " — Owlio");
    setWindowModified(editor_.modified());
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (!maybeSave()) { e->ignore(); return; }
    QSettings s;
    s.setValue("geometry", saveGeometry());
    s.setValue("windowState", saveState());
    s.setValue("zoom", view_->zoom());
    e->accept();
}
