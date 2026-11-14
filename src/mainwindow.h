#pragma once
#include <QHash>
#include <QMainWindow>
#include <QTimer>

#include "autosave.h"
#include "editor.h"

class QComboBox;
class QLabel;
class QListWidget;
class QAction;
class QDockWidget;
class QToolButton;
class QPagedPaintDevice;
class ScriptView;
class SceneBoard;
class BeatBoardView;
class RelationMapView;
class TimelineView;
class QStackedWidget;
class FindDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    bool openFile(const QString& path);
    bool exportPdfTo(const QString& path);
    ScriptView* view() const { return view_; }
    SceneBoard* board() const { return board_; }
    enum class ViewMode { Script, Cards, Beats, Relations, Timeline };
    ViewMode viewMode() const;
    void setViewMode(ViewMode mode);
    bool cardsMode() const { return viewMode() == ViewMode::Cards; }
    void setCardsMode(bool cards) { setViewMode(cards ? ViewMode::Cards : ViewMode::Script); }
    BeatBoardView* beatBoard() const { return beats_; }
    RelationMapView* relationMap() const { return relations_; }
    TimelineView* timeline() const { return timeline_; }
    const Editor& editor() const { return editor_; }
    // Oferece as cópias de recuperação deixadas por execuções que não fecharam direito.
    void offerRecovery();
    void recover(const autosave::Recovery& r);  // abre a cópia nesta janela
    void writeRecoveryNow();                    // o que o timer faz a cada 20 s
    void autosaveNow();                         // o que o timer de salvamento automático faz
    QString currentPath() const { return path_; }
    bool sidebarVisible() const;
    void setSidebarVisible(bool visible);  // os painéis de cenas e de personagens juntos
    QAction* sidebarAction() const { return sidebarAct_; }

protected:
    void closeEvent(QCloseEvent* e) override;

private slots:
    void newFile();
    void open();
    bool save();
    bool saveAs();
    void exportPdf();
    void exportFdx();
    void print();
    void editTitlePage();
    void showFind();
    void showShortcuts();
    void about();
    void onDocumentChanged();
    void onCursorMoved();
    void refreshPanels();
    void editSynopsis(int sceneIndex);
    void chooseAvatar(const QString& characterName);
    void editCharacter(const QString& characterName);
    void gotoFirstLine(const QString& characterName);
    void showSettings();
    void showDictionary();
    void browseBackups();
    void browseVersions();
    void showCast();

private:
    void buildActions();
    bool maybeSave();
    bool writeTo(const QString& path, bool quiet = false);
    std::string serialize(const QString& path) const;
    bool saveNow(bool automatic);
    void applySaveSettings();
    void clearRecovery();
    QString suggestedPdfPath() const;
    void openPrintDialog(bool pdf);
    void updateTitle();
    void syncSidebarAction();
    void refreshIcons();
    QAction* iconAction(QAction* a, const QString& icon);
    QDockWidget* makeDock(const QString& title, const QString& objectName, QWidget* content);
    QToolButton* addDockButton(QDockWidget* dock, const QString& icon, const QString& tip);

    Editor editor_;
    ScriptView* view_ = nullptr;
    SceneBoard* board_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QAction* scriptModeAct_ = nullptr;
    QAction* cardsModeAct_ = nullptr;
    QAction* beatsModeAct_ = nullptr;
    BeatBoardView* beats_ = nullptr;
    RelationMapView* relations_ = nullptr;
    TimelineView* timeline_ = nullptr;
    QAction* relationsModeAct_ = nullptr;
    QAction* timelineModeAct_ = nullptr;
    QAction* elementComboAct_ = nullptr;
    QString path_;
    QTimer panelTimer_;
    QTimer recoveryTimer_;
    QTimer autosaveTimer_;
    autosave::Session session_;
    int recoveryRevision_ = -1;

    QListWidget* sceneList_ = nullptr;
    QListWidget* charList_ = nullptr;
    QComboBox* elementCombo_ = nullptr;
    QComboBox* zoomCombo_ = nullptr;
    QLabel* pageLabel_ = nullptr;
    QLabel* sceneLabel_ = nullptr;
    QLabel* wordLabel_ = nullptr;
    QList<QAction*> elementActions_;
    QAction* dualAct_ = nullptr;
    QAction* sceneNumsAct_ = nullptr;
    QAction* sidebarAct_ = nullptr;
    QAction* themeToggleAct_ = nullptr;
    QString rememberedPath_;
    QAction* undoAct_ = nullptr;
    QAction* redoAct_ = nullptr;
    FindDialog* findDialog_ = nullptr;
    QHash<QAction*, QString> actionIcons_;  // ação -> nome do ícone (recolorido ao trocar o tema)
};
