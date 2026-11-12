#pragma once
#include <QDialog>

#include "editor.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

// Personagens: todos numa tabela, com foto, papel, idade, ocupação e números. Nome, papel, idade e
// ocupação se editam ali mesmo (duplo clique ou F2) e valem para o roteiro inteiro: trocar o nome
// renomeia todas as deixas, a ficha e o mapa de relações, num só passo de desfazer.
class CastDialog : public QDialog {
    Q_OBJECT
public:
    explicit CastDialog(Editor* editor, QWidget* parent = nullptr);
    void reload();
    QTreeWidget* table() const { return table_; }
    // Aplica uma edição de célula (também usada pelos testes). Devolve false se foi recusada.
    bool editCell(const QString& name, int column, const QString& value);

signals:
    void changed();                         // o editor mudou: a janela principal atualiza o resto
    void openSheet(const QString& name);    // ficha completa
    void choosePhoto(const QString& name);
    void gotoFirstLine(const QString& name);

private:
    QString current() const;
    void updateButtons();
    void addCharacter();
    void createAvatar();
    void removeSheet();

    Editor* ed_;
    QLineEdit* search_;
    QTreeWidget* table_;
    QLabel* summary_;
    QPushButton* sheet_;
    QPushButton* avatar_;
    QPushButton* photo_;
    QPushButton* first_;
    QPushButton* remove_;
    bool loading_ = false;
};
