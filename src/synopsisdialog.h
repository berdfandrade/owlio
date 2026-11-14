#pragma once
#include <QDialog>

#include "editor.h"

class QLabel;
class QPlainTextEdit;

// Edita a sinopse de uma cena: cabeçalho da cena no topo, texto livre e contador de palavras.
class SynopsisDialog : public QDialog {
    Q_OBJECT
public:
    SynopsisDialog(int sceneIndex, const SceneSummary& scene, QWidget* parent = nullptr);
    QString synopsis() const;

private:
    void updateCount();

    QPlainTextEdit* text_ = nullptr;
    QLabel* count_ = nullptr;
};
