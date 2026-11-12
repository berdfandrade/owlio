#pragma once
#include <QWidget>

#include "editor.h"

class QLabel;
class QProgressBar;
class QScrollArea;
class QSpinBox;
class QToolButton;
class TimelineCanvas;

// Linha do tempo do filme: páginas escritas contra a duração planejada, os três atos com os pontos de
// virada, cada cena do tamanho que ocupa (na cor do horário) e uma faixa por personagem mostrando em
// que cenas ele aparece. Clicar numa cena abre o roteiro nela.
class TimelineView : public QWidget {
    Q_OBJECT
public:
    explicit TimelineView(Editor* editor, QWidget* parent = nullptr);
    void reload();
    int targetPages() const;
    void setTargetPages(int pages);
    int sceneAt(QPoint canvasPos) const;  // índice da cena, -1 se nenhuma (para testes)
    QWidget* canvas() const;

signals:
    void openScene(int para);

private:
    Editor* ed_;
    QLabel* stats_;
    QProgressBar* progress_;
    QSpinBox* target_;
    QToolButton* fit_;
    QScrollArea* scroll_;
    TimelineCanvas* canvas_;
};
