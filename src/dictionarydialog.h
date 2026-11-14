#pragma once
#include <QDialog>
#include <string>
#include <vector>

class QLineEdit;
class QListWidget;
class QPushButton;

// Dicionário do roteiro: as palavras que o corretor aceita só neste roteiro (nomes, lugares, gírias).
class DictionaryDialog : public QDialog {
    Q_OBJECT
public:
    explicit DictionaryDialog(const std::vector<std::u32string>& words, QWidget* parent = nullptr);
    std::vector<std::u32string> words() const;

private:
    void addWord();
    void updateButtons();

    QListWidget* list_;
    QLineEdit* input_;
    QPushButton* add_;
    QPushButton* remove_;
};
