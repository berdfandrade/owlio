#include "mainwindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDir>
#include <QLocale>
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
#include <QProcess>
#include <QStackedWidget>

#include "avatars.h"
#include "characterdialog.h"
#include "i18n.h"
#include "settingsdialog.h"
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
    Q_OBJECT
public:
    FindDialog(Editor* ed, ScriptView* view, QWidget* parent) : QDialog(parent), ed_(ed), view_(view) {
        setWindowTitle(tr("Find and Replace"));
        auto* form = new QFormLayout;
        findEdit_ = new QLineEdit;
        replaceEdit_ = new QLineEdit;
        form->addRow(tr("Find:"), findEdit_);
        form->addRow(tr("Replace with:"), replaceEdit_);
        status_ = new QLabel;
        auto* prev = new QPushButton(tr("Previous"));
        auto* next = new QPushButton(tr("Next"));
        auto* rep = new QPushButton(tr("Replace"));
        auto* all = new QPushButton(tr("Replace All"));
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
            status_->setText(tr("%n replacement(s).", "", n));
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
        status_->setText(found ? QString() : tr("Nothing found."));
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
        statusBar()->showMessage(tr("Scene %1 moved to position %2.").arg(from + 1).arg(now + 1), 2500);
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
    addDockWidget(Qt::LeftDockWidgetArea, makeDock(tr("Scenes"), "scenes", sceneList_));
    connect(sceneList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        if (cardsMode()) board_->selectScene(sceneList_->row(it));
        else view_->gotoParagraph(it->data(Qt::UserRole).toInt());
    });

    // Painel de personagens.
    charList_ = new QListWidget;
    charList_->setItemDelegate(new CardDelegate(CardDelegate::Character, charList_));
    addDockWidget(Qt::LeftDockWidgetArea, makeDock(tr("Characters"), "characters", charList_));
    connect(charList_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* it) { editCharacter(it->data(Qt::UserRole).toString()); });
    charList_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(charList_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* it = charList_->itemAt(pos);
        if (!it) return;
        const QString name = it->data(Qt::UserRole).toString();
        const bool hasPhoto = !editor_.doc().avatar(name.toStdU32String()).empty();
        QMenu menu(this);
        menu.addAction(tr("Open Character Sheet..."), this, [this, name] { editCharacter(name); });
        menu.addSeparator();
        menu.addAction(hasPhoto ? tr("Change Photo...") : tr("Choose Photo..."), this,
                       [this, name] { chooseAvatar(name); });
        menu.addAction(tr("Remove Photo"), this, [this, name] {
            editor_.setAvatar(name.toStdU32String(), {});
            view_->refreshAfterExternalEdit();
        })->setEnabled(hasPhoto);
        menu.addSeparator();
        menu.addAction(tr("Go to First Line"), this, [this, name] { gotoFirstLine(name); });
        menu.exec(charList_->viewport()->mapToGlobal(pos));
    });

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
    recoveryTimer_.setInterval(20 * 1000);
    connect(&recoveryTimer_, &QTimer::timeout, this, &MainWindow::writeRecoveryNow);
    recoveryTimer_.start();
    connect(&autosaveTimer_, &QTimer::timeout, this, &MainWindow::autosaveNow);
    applySaveSettings();
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

    editor_.setScriptLanguage(i18n::scriptLanguage());
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
    QMenu* file = menuBar()->addMenu(tr("&File"));
    QAction* newAct = iconAction(file->addAction(tr("&New"), this, &MainWindow::newFile), "novo");
    newAct->setShortcut(QKeySequence::New);
    QAction* openAct = iconAction(file->addAction(tr("&Open..."), this, &MainWindow::open), "abrir");
    openAct->setShortcut(QKeySequence::Open);
    QAction* saveAct = iconAction(file->addAction(tr("&Save"), this, &MainWindow::save), "salvar");
    saveAct->setShortcut(QKeySequence::Save);
    QAction* saveAsAct = file->addAction(tr("Save &As..."), this, &MainWindow::saveAs);
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    file->addSeparator();
    QAction* pdfAct = iconAction(file->addAction(tr("Export &PDF..."), this, &MainWindow::exportPdf), "pdf");
    pdfAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    file->addAction(tr("Export to Final Draft (.fdx)..."), this, &MainWindow::exportFdx);
    QAction* printAct = iconAction(file->addAction(tr("&Print..."), this, &MainWindow::print), "imprimir");
    printAct->setShortcut(QKeySequence::Print);
    file->addAction(tr("&Backups..."), this, &MainWindow::browseBackups);
    file->addSeparator();
    file->addAction(tr("&Quit"), this, &QWidget::close)->setShortcut(QKeySequence::Quit);

    // ---- Editar
    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    undoAct_ = iconAction(edit->addAction(tr("&Undo"), view_, &ScriptView::undo), "desfazer");
    undoAct_->setShortcut(QKeySequence::Undo);
    redoAct_ = iconAction(edit->addAction(tr("&Redo"), view_, &ScriptView::redo), "refazer");
    redoAct_->setShortcuts({QKeySequence::Redo, QKeySequence(Qt::CTRL | Qt::Key_Y)});
    edit->addSeparator();
    edit->addAction(tr("Cu&t"), view_, &ScriptView::cut)->setShortcut(QKeySequence::Cut);
    edit->addAction(tr("&Copy"), view_, &ScriptView::copy)->setShortcut(QKeySequence::Copy);
    edit->addAction(tr("&Paste"), view_, &ScriptView::paste)->setShortcut(QKeySequence::Paste);
    edit->addAction(tr("Select &All"), view_, &ScriptView::selectAll)->setShortcut(QKeySequence::SelectAll);
    edit->addSeparator();
    QAction* findAct = iconAction(edit->addAction(tr("&Find and Replace..."), this, &MainWindow::showFind), "localizar");
    findAct->setShortcut(QKeySequence::Find);
    edit->addSeparator();
    QAction* settingsAct = edit->addAction(tr("Se&ttings..."), this, &MainWindow::showSettings);
    settingsAct->setShortcuts({QKeySequence::Preferences, QKeySequence(Qt::CTRL | Qt::Key_Comma)});
    settingsAct->setMenuRole(QAction::PreferencesRole);

    // ---- Formatar (elementos)
    QMenu* fmt = menuBar()->addMenu(tr("F&ormat"));
    auto* elementGroup = new QActionGroup(this);
    for (int i = 0; i < kElementCount; ++i) {
        Element e = static_cast<Element>(i);
        QAction* a = fmt->addAction(i18n::elementLabel(e), this, [this, e] { view_->setElement(e); });
        a->setShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_1 + i)));
        a->setCheckable(true);
        elementGroup->addAction(a);
        elementActions_.append(a);
    }
    fmt->addSeparator();
    dualAct_ = fmt->addAction(tr("&Dual Dialogue"), this, [this] {
        if (!view_->toggleDualDialogue())
            statusBar()->showMessage(tr("Dual dialogue needs two speeches in a row: put the cursor in one of them."),
                                     4000);
        onCursorMoved();
    });
    dualAct_->setCheckable(true);
    dualAct_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    dualAct_->setToolTip(tr("Puts this speech side by side with the one before it"));

    // ---- Documento
    QMenu* docMenu = menuBar()->addMenu(tr("&Document"));
    QAction* titleAct = iconAction(docMenu->addAction(tr("&Title Page..."), this, &MainWindow::editTitlePage), "rosto");
    QAction* sceneNums = docMenu->addAction(tr("&Number Scenes"));
    sceneNums->setCheckable(true);
    connect(sceneNums, &QAction::toggled, view_, &ScriptView::setSceneNumbers);

    // ---- Exibir
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    auto* modeGroup = new QActionGroup(this);
    scriptModeAct_ = iconAction(viewMenu->addAction(tr("&Script")), "roteiro");
    scriptModeAct_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    scriptModeAct_->setToolTip(tr("Script  (Ctrl+Shift+R)"));
    cardsModeAct_ = iconAction(viewMenu->addAction(tr("&Cards")), "cartoes");
    cardsModeAct_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    cardsModeAct_->setToolTip(tr("Scenes as index cards  (Ctrl+Shift+C)"));
    for (QAction* a : {scriptModeAct_, cardsModeAct_}) {
        a->setCheckable(true);
        modeGroup->addAction(a);
    }
    scriptModeAct_->setChecked(true);
    connect(scriptModeAct_, &QAction::triggered, this, [this] { setCardsMode(false); });
    connect(cardsModeAct_, &QAction::triggered, this, [this] { setCardsMode(true); });
    viewMenu->addSeparator();
    QMenu* themeMenu = viewMenu->addMenu(tr("&Theme"));
    actionIcons_.insert(themeMenu->menuAction(), "tema");
    auto* themeGroup = new QActionGroup(this);
    const std::pair<QString, ThemeManager::Mode> modes[] = {
        {tr("Follow System"), ThemeManager::Mode::System},
        {tr("Light"), ThemeManager::Mode::Light},
        {tr("Dark"), ThemeManager::Mode::Dark}};
    for (const auto& [label, mode] : modes) {
        QAction* a = themeMenu->addAction(label, this, [mode] { ThemeManager::instance().setMode(mode); });
        a->setCheckable(true);
        a->setChecked(ThemeManager::instance().mode() == mode);
        a->setData(static_cast<int>(mode));
        themeGroup->addAction(a);
    }
    connect(&ThemeManager::instance(), &ThemeManager::changed, themeMenu, [themeGroup] {
        for (QAction* a : themeGroup->actions())
            a->setChecked(a->data().toInt() == static_cast<int>(ThemeManager::instance().mode()));
    });
    QAction* focusAct = iconAction(viewMenu->addAction(tr("&Focus Mode")), "foco");
    focusAct->setCheckable(true);
    focusAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    focusAct->setToolTip(tr("Focus mode — dims everything except the current passage (Ctrl+Shift+F)"));
    connect(focusAct, &QAction::toggled, view_, &ScriptView::setFocusMode);
    viewMenu->addSeparator();
    viewMenu->addAction(tr("Zoom In"), view_, &ScriptView::zoomIn)->setShortcuts({QKeySequence::ZoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal)});
    viewMenu->addAction(tr("Zoom Out"), view_, &ScriptView::zoomOut)->setShortcut(QKeySequence::ZoomOut);
    viewMenu->addAction(tr("Zoom 100%"), view_, &ScriptView::zoomReset)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    viewMenu->addSeparator();
    for (QDockWidget* d : findChildren<QDockWidget*>()) viewMenu->addAction(d->toggleViewAction());

    // ---- Ajuda
    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&Keyboard Shortcuts"), this, &MainWindow::showShortcuts)->setShortcut(QKeySequence::HelpContents);
    help->addAction(tr("&About Owlio"), this, &MainWindow::about);

    // ---- Barra de ferramentas
    QToolBar* tb = addToolBar(tr("Main"));
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
    tip(newAct, tr("New script"));
    tip(openAct, tr("Open"));
    tip(saveAct, tr("Save"));
    tip(undoAct_, tr("Undo"));
    tip(redoAct_, tr("Redo"));
    tip(findAct, tr("Find and Replace"));
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
    elementCombo_->setToolTip(tr("Element of the current paragraph (Ctrl+1 … Ctrl+6)"));
    elementCombo_->setIconSize(QSize(16, 16));
    elementCombo_->setMinimumWidth(170);
    for (int i = 0; i < kElementCount; ++i)
        elementCombo_->addItem(i18n::elementLabel(static_cast<Element>(i)));
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
    titleAct->setIconText(tr("Title Page"));

    QAction* pdfTb = new QAction(tr("Export PDF"), this);
    pdfTb->setProperty("primario", true);
    pdfTb->setToolTip(tr("Export PDF  (Ctrl+Shift+E)"));
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
    zoomCombo_->setToolTip(tr("Zoom (Ctrl+mouse wheel)"));
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
                                  tr("The script has unsaved changes.\nDo you want to save them?"),
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
    clearRecovery();
    view_->refresh();
    updateTitle();
}

void MainWindow::open() {
    if (!maybeSave()) return;
    QSettings s;
    QString p = QFileDialog::getOpenFileName(
        this, tr("Open Script"), s.value("lastDir").toString(),
        tr("Scripts (*.fountain *.fdx *.txt *.spmd);;Fountain (*.fountain);;Final Draft (*.fdx);;All files (*)"));
    if (!p.isEmpty()) openFile(p);
}

bool MainWindow::openFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not open %1:\n%2").arg(path, f.errorString()));
        return false;
    }
    const std::string data = f.readAll().toStdString();
    Document d = path.endsWith(".fdx", Qt::CaseInsensitive) ? parseFdx(data) : parseFountain(data);
    editor_.setDocument(std::move(d));
    path_ = path;
    clearRecovery();
    QSettings().setValue("lastDir", QFileInfo(path).absolutePath());
    view_->refresh();
    updateTitle();
    statusBar()->showMessage(tr("Opened: %1").arg(QFileInfo(path).fileName()), 3000);
    return true;
}

std::string MainWindow::serialize(const QString& path) const {
    return path.endsWith(".fdx", Qt::CaseInsensitive) ? writeFdx(editor_.doc()) : writeFountain(editor_.doc());
}

bool MainWindow::writeTo(const QString& path, bool quiet) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (quiet) statusBar()->showMessage(tr("Could not save %1: %2").arg(path, f.errorString()), 6000);
        else QMessageBox::critical(this, tr("Error"), tr("Could not save %1:\n%2").arg(path, f.errorString()));
        return false;
    }
    const std::string data = serialize(path);
    f.write(data.data(), static_cast<qint64>(data.size()));
    if (!f.commit()) {
        if (quiet) statusBar()->showMessage(tr("Failed to write %1.").arg(path), 6000);
        else QMessageBox::critical(this, tr("Error"), tr("Failed to write %1.").arg(path));
        return false;
    }
    return true;
}

bool MainWindow::save() {
    if (path_.isEmpty()) return saveAs();
    return saveNow(false);
}

bool MainWindow::saveNow(bool automatic) {
    if (path_.isEmpty() || !writeTo(path_, automatic)) return false;
    editor_.markSaved();
    clearRecovery();
    if (QSettings().value("keepBackups", true).toBool())
        autosave::backup(path_, QByteArray::fromStdString(serialize(path_)), automatic);
    updateTitle();
    statusBar()->showMessage(automatic ? tr("Saved automatically.") : tr("Saved."), 2000);
    return true;
}

// ------------------------------------------------------------ salvamento automático e recuperação

void MainWindow::applySaveSettings() {
    const int minutes = QSettings().value("autosaveMinutes", 5).toInt();
    if (minutes > 0) autosaveTimer_.start(minutes * 60 * 1000);
    else autosaveTimer_.stop();
}

void MainWindow::autosaveNow() {
    // Só salva no próprio arquivo quando ele já existe; sem título, fica a cópia de recuperação.
    if (editor_.modified() && !path_.isEmpty()) saveNow(true);
}

void MainWindow::writeRecoveryNow() {
    if (!editor_.modified()) {
        if (recoveryRevision_ >= 0) clearRecovery();
        return;
    }
    if (editor_.revision() == recoveryRevision_) return;
    session_.write(QByteArray::fromStdString(writeFountain(editor_.doc())), path_);
    recoveryRevision_ = editor_.revision();
}

void MainWindow::clearRecovery() {
    session_.clear();
    recoveryRevision_ = -1;
}

void MainWindow::offerRecovery() {
    for (const autosave::Recovery& r : autosave::orphans()) {
        const QString name = r.originalPath.isEmpty() ? tr("an untitled script") : QFileInfo(r.originalPath).fileName();
        QMessageBox box(QMessageBox::Warning, tr("Recover Unsaved Changes"),
                        tr("Owlio closed before saving your changes to %1.").arg(name), QMessageBox::NoButton, this);
        box.setInformativeText(tr("The last copy was kept on %1. Do you want to recover it?")
                                   .arg(QLocale().toString(r.when, QLocale::ShortFormat)));
        QPushButton* recoverBtn = box.addButton(tr("Recover"), QMessageBox::AcceptRole);
        QPushButton* discardBtn = box.addButton(tr("Discard"), QMessageBox::DestructiveRole);
        box.addButton(tr("Decide Later"), QMessageBox::RejectRole);
        box.setDefaultButton(recoverBtn);
        box.exec();
        if (box.clickedButton() == recoverBtn) {
            if (!maybeSave()) return;
            recover(r);
            return;  // uma janela, um roteiro: as outras cópias ficam para o próximo início
        }
        if (box.clickedButton() != discardBtn) return;
        autosave::discard(r);
    }
}

void MainWindow::recover(const autosave::Recovery& r) {
    QFile f(r.file);
    if (!f.open(QIODevice::ReadOnly)) return;
    editor_.setDocument(parseFountain(f.readAll().toStdString()));
    f.close();
    path_ = r.originalPath;
    editor_.markModified();  // ainda não foi salvo no arquivo original
    autosave::discard(r);
    recoveryRevision_ = -1;
    writeRecoveryNow();  // a cópia agora pertence a esta janela
    view_->refresh();
    updateTitle();
    statusBar()->showMessage(tr("Changes recovered. Save to keep them."), 5000);
}

void MainWindow::browseBackups() {
    QString dir = path_.isEmpty() ? autosave::backupRoot() : autosave::backupDir(path_);
    if (!QFileInfo::exists(dir)) dir = autosave::backupRoot();
    QDir().mkpath(dir);
    const QString p = QFileDialog::getOpenFileName(this, tr("Open a Backup"), dir,
                                                   tr("Scripts (*.fountain *.fdx);;All files (*)"));
    if (p.isEmpty() || !maybeSave() || !openFile(p)) return;
    // Abre como cópia sem título, para não sobrescrever o backup ao salvar.
    path_.clear();
    editor_.markModified();
    updateTitle();
    statusBar()->showMessage(tr("Opened a copy of the backup. Use Save As to keep it."), 6000);
}

bool MainWindow::saveAs() {
    QSettings s;
    QString suggested = path_.isEmpty() ? s.value("lastDir").toString() + "/" + tr("script") + ".fountain" : path_;
    QString p = QFileDialog::getSaveFileName(this, tr("Save Script"), suggested,
                                             tr("Fountain (*.fountain);;Final Draft (*.fdx)"));
    if (p.isEmpty()) return false;
    if (!p.endsWith(".fountain", Qt::CaseInsensitive) && !p.endsWith(".fdx", Qt::CaseInsensitive))
        p += ".fountain";
    path_ = p;
    s.setValue("lastDir", QFileInfo(p).absolutePath());
    return save();
}

void MainWindow::exportFdx() {
    QString base = path_.isEmpty() ? QSettings().value("lastDir").toString() + "/" + tr("script")
                                   : QFileInfo(path_).absolutePath() + "/" + QFileInfo(path_).completeBaseName();
    QString p = QFileDialog::getSaveFileName(this, tr("Export to Final Draft"), base + ".fdx",
                                             tr("Final Draft (*.fdx)"));
    if (p.isEmpty()) return;
    if (!p.endsWith(".fdx", Qt::CaseInsensitive)) p += ".fdx";
    if (writeTo(p)) statusBar()->showMessage(tr("Exported: %1").arg(QFileInfo(p).fileName()), 4000);
}

QString MainWindow::suggestedPdfPath() const {
    const QString base = path_.isEmpty()
                             ? QSettings().value("lastDir").toString() + "/" + tr("script")
                             : QFileInfo(path_).absolutePath() + "/" + QFileInfo(path_).completeBaseName();
    return base + ".pdf";
}

void MainWindow::openPrintDialog(bool pdf) {
    PrintDialog dlg(editor_, pdf ? PrintDialog::Target::Pdf : PrintDialog::Target::Printer, suggestedPdfPath(),
                    view_->sceneNumbers(), this);
    dlg.resize(1100, 720);
    if (dlg.exec() != QDialog::Accepted) return;
    if (!dlg.exportedPath().isEmpty())
        statusBar()->showMessage(tr("PDF exported: %1").arg(QFileInfo(dlg.exportedPath()).fileName()), 5000);
    else
        statusBar()->showMessage(tr("Sent to the printer."), 4000);
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
    auto row = [](const QString& keys, const QString& what) {
        return QString("<tr><td><b>%1</b></td><td>%2</td></tr>").arg(keys, what);
    };
    QString html = "<h3>" + tr("Writing flow") + "</h3><table cellpadding=3>";
    html += row(tr("Enter"), tr("Next element: Scene Heading → Action, Character → Dialogue, "
                                "Dialogue → Character, Transition → Scene Heading"));
    html += row(tr("Tab"), tr("Action → Character → Transition → Scene Heading; Dialogue ↔ Parenthetical. "
                              "At the end of a speech, inserts a parenthetical."));
    html += row(tr("Shift+Tab"), tr("Previous element"));
    html += row(tr("Enter on an empty line"), tr("Switches to Action (or Scene Heading)"));
    html += row("Ctrl+1 … Ctrl+6", tr("Scene Heading, Action, Character, Parenthetical, Dialogue, Transition"));
    html += row(tr("Tab / Enter / →"),
                tr("Accepts the grey suggestion (characters, locations, times of day, transitions)"));
    html += "</table><h3>" + tr("General") + "</h3><table cellpadding=3>";
    html += row("Ctrl+S / Ctrl+Shift+S", tr("Save / Save As (.fountain or .fdx)"));
    html += row("Ctrl+Shift+E", tr("Export PDF"));
    html += row("Ctrl+F", tr("Find and replace"));
    html += row("Ctrl+Shift+C / Ctrl+Shift+R", tr("Index cards / script"));
    html += row("Ctrl+Shift+F", tr("Focus mode"));
    html += row("Ctrl+,", tr("Settings"));
    html += row(tr("Ctrl+wheel / Ctrl+= / Ctrl+-"), tr("Zoom"));
    html += "</table><h3>" + tr("Index cards") + "</h3><table cellpadding=3>";
    html += row(tr("Drag"), tr("Reorders scenes (the whole scene text moves with the card)"));
    html += row(tr("Alt+arrows"), tr("Moves the selected card"));
    html += row(tr("Enter / double-click"), tr("Opens the scene in the script"));
    html += row("F2", tr("Edits the scene synopsis"));
    html += row(tr("Ctrl+wheel"), tr("Bigger or smaller cards"));
    html += row("Ctrl+Z / Ctrl+Y", tr("Undo / redo"));
    html += "</table>";
    QMessageBox::information(this, tr("Keyboard Shortcuts"), html);
}

void MainWindow::about() {
    QMessageBox::about(this, tr("About Owlio"),
                       tr("<h3>Owlio %1</h3><p>A screenwriting app written in C++ and Qt %2.</p>"
                          "<p>Industry-standard formatting (12 pt Courier, US Letter), page breaks with "
                          "(MORE)/(CONT'D), Fountain and Final Draft (.fdx) files, PDF export.</p>")
                           .arg(QApplication::applicationVersion(), qVersion()));
}

void MainWindow::showSettings() {
    SettingsDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    ThemeManager::instance().setMode(dlg.themeMode());
    QSettings().setValue("autosaveMinutes", dlg.autosaveMinutes());
    QSettings().setValue("keepBackups", dlg.keepBackups());
    applySaveSettings();
    if (dlg.scriptLanguage() != editor_.scriptLanguage()) {
        i18n::setScriptLanguage(dlg.scriptLanguage());
        editor_.setScriptLanguage(dlg.scriptLanguage());
        view_->refreshAfterExternalEdit();
    }

    const QString lang = dlg.uiLanguage();
    if (lang == i18n::uiLanguage() && lang == i18n::activeLanguage()) return;
    i18n::setUiLanguage(lang);
    if (lang == i18n::activeLanguage()) return;

    QMessageBox box(QMessageBox::Question, tr("Interface Language"),
                    tr("The new language is used after Owlio restarts. Restart now?"),
                    QMessageBox::NoButton, this);
    QPushButton* now = box.addButton(tr("Restart Now"), QMessageBox::AcceptRole);
    box.addButton(tr("Later"), QMessageBox::RejectRole);
    box.setDefaultButton(now);
    box.exec();
    if (box.clickedButton() != now || !maybeSave()) return;
    editor_.markSaved();  // já salvo ou descartado: não perguntar de novo ao fechar
    QStringList args;
    if (!path_.isEmpty()) args << path_;
    if (QProcess::startDetached(QCoreApplication::applicationFilePath(), args)) close();
}

void MainWindow::editCharacter(const QString& name) {
    if (name.isEmpty()) return;
    CharacterDialog dlg(editor_, name, this);
    dlg.resize(900, 680);
    if (dlg.exec() != QDialog::Accepted) return;
    const CharacterInfo info = dlg.info();
    const int renamed = editor_.setCharacter(name.toStdU32String(), info);
    view_->refreshAfterExternalEdit();
    if (renamed > 0)
        statusBar()->showMessage(tr("%1 renamed to %2 in %n cue(s).", "", renamed).arg(name, q(info.name)), 4000);
    if (dlg.gotoParagraph() >= 0) {
        setCardsMode(false);
        view_->gotoParagraph(dlg.gotoParagraph());
        view_->setFocus();
    }
}

void MainWindow::gotoFirstLine(const QString& name) {
    const CharacterStats st = editor_.characterStats(name.toStdU32String());
    if (st.firstPara < 0) return;
    setCardsMode(false);
    view_->gotoParagraph(st.firstPara);
    view_->setFocus();
}

// ------------------------------------------------------------ atualização da UI

bool MainWindow::cardsMode() const { return stack_ && stack_->currentWidget() == board_; }

void MainWindow::setCardsMode(bool cards) {
    scriptModeAct_->setChecked(!cards);
    cardsModeAct_->setChecked(cards);
    elementComboAct_->setVisible(!cards);
    for (QAction* a : elementActions_) a->setEnabled(!cards);
    if (dualAct_) dualAct_->setEnabled(!cards);
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
        this, tr("Scene %1 Synopsis").arg(sceneIndex + 1), q(s.heading), q(s.synopsis), &ok);
    if (!ok) return;
    editor_.setSynopsis(s.para, text.toStdU32String());
    view_->refreshAfterExternalEdit();
    board_->selectScene(sceneIndex);
}

void MainWindow::chooseAvatar(const QString& name) {
    QSettings settings;
    const QString path = QFileDialog::getOpenFileName(this, tr("Photo of %1").arg(name),
                                                      settings.value("lastImageDir").toString(),
                                                      avatars::fileFilter());
    if (path.isEmpty()) return;
    settings.setValue("lastImageDir", QFileInfo(path).absolutePath());
    QString error;
    const std::string data = avatars::fromImageFile(path, &error);
    if (data.empty()) {
        QMessageBox::warning(this, tr("Photo"), tr("Could not open the image:\n%1").arg(error));
        return;
    }
    editor_.setAvatar(name.toStdU32String(), data);
    view_->refreshAfterExternalEdit();
    statusBar()->showMessage(tr("Photo of %1 updated.").arg(name), 2500);
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
    if (dualAct_) dualAct_->setChecked(editor_.isDualDialogue());
    undoAct_->setEnabled(editor_.canUndo());
    redoAct_->setEnabled(editor_.canRedo());

    const int pages = editor_.layout().pageCount;
    pageLabel_->setText(tr("Page %1 of %2").arg(view_->currentPage() + 1).arg(pages));
    const int scene = editor_.sceneIndexAt(editor_.cursor().para);
    const int scenes = static_cast<int>(editor_.scenes().size());
    sceneLabel_->setText(scene >= 0 ? tr("Scene %1 of %2").arg(scene + 1).arg(scenes)
                                    : tr("%n scene(s)", "", scenes));
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
        QString sub = tr("Page %1").arg(L.pageOfPara(scenes[i].para) + 1);
        if (!cast.isEmpty()) sub += "  ·  " + cast.join(", ");
        auto* it = new QListWidgetItem(h.empty() ? tr("(untitled)") : q(h));
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
        it->setData(CardDelegate::kBadgeRole, tr("%n line(s)", "", count));
        it->setData(CardDelegate::kAvatarRole, QByteArray::fromStdString(editor_.doc().avatar(name)));
        if (const CharacterInfo* info = editor_.doc().character(name))
            it->setData(CardDelegate::kSubtitleRole, q(info->role));
        it->setToolTip(tr("%n line(s)", "", count) + "  ·  " +
                       tr("%n scene(s)", "", static_cast<int>(scenesOf.value(q(name)).size())) +
                       "\n" + tr("Click to open the character sheet"));
        charList_->addItem(it);
    }
    wordLabel_->setText(tr("%n word(s)", "", editor_.wordCount()));
    onCursorMoved();
}


void MainWindow::updateTitle() {
    QString name = path_.isEmpty() ? tr("Untitled") : QFileInfo(path_).fileName();
    std::string t = editor_.doc().titleValue("Title");
    QString full = name + "[*]";
    if (!t.empty()) full = q(t) + " (" + name + ")[*]";
    setWindowTitle(full + " — Owlio");
    setWindowModified(editor_.modified());
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (!maybeSave()) { e->ignore(); return; }
    clearRecovery();  // fechamento normal: nada a recuperar
    QSettings s;
    s.setValue("geometry", saveGeometry());
    s.setValue("windowState", saveState());
    s.setValue("zoom", view_->zoom());
    e->accept();
}

#include "mainwindow.moc"
