#pragma once
#include <QHash>
#include <QMainWindow>
#include <QTimer>

#include "editor.h"

class QComboBox;
class QLabel;
class QListWidget;
class QAction;
class QDockWidget;
class QPagedPaintDevice;
class ScriptView;
class SceneBoard;
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
    bool cardsMode() const;
    void setCardsMode(bool cards);
    const Editor& editor() const { return editor_; }

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

private:
    void buildActions();
    bool maybeSave();
    bool writeTo(const QString& path);
    void renderAll(QPagedPaintDevice& device, double ppi);
    void updateTitle();
    void refreshIcons();
    QAction* iconAction(QAction* a, const QString& icon);
    QDockWidget* makeDock(const QString& title, const QString& objectName, QWidget* content);

    Editor editor_;
    ScriptView* view_ = nullptr;
    SceneBoard* board_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QAction* scriptModeAct_ = nullptr;
    QAction* cardsModeAct_ = nullptr;
    QAction* elementComboAct_ = nullptr;
    QString path_;
    QTimer panelTimer_;

    QListWidget* sceneList_ = nullptr;
    QListWidget* charList_ = nullptr;
    QComboBox* elementCombo_ = nullptr;
    QComboBox* zoomCombo_ = nullptr;
    QLabel* pageLabel_ = nullptr;
    QLabel* sceneLabel_ = nullptr;
    QLabel* wordLabel_ = nullptr;
    QList<QAction*> elementActions_;
    QAction* undoAct_ = nullptr;
    QAction* redoAct_ = nullptr;
    FindDialog* findDialog_ = nullptr;
    QHash<QAction*, QString> actionIcons_;  // ação -> nome do ícone (recolorido ao trocar o tema)
};
