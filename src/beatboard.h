#pragma once
#include <QGraphicsView>
#include <QHash>

#include "editor.h"

class QGraphicsPathItem;
class QToolButton;
class BeatItem;
class FlowItem;

// Beat Board: quadro livre de ideias. Cada beat é um cartão que se arrasta, redimensiona e pode
// ter imagem; as linhas de fluxo ligam um beat a outro. Arraste da bolinha na borda direita de um
// beat até outro para ligar os dois. Tudo fica no próprio .fountain e cada mudança é um passo de desfazer.
class BeatBoardView : public QGraphicsView {
    Q_OBJECT
public:
    explicit BeatBoardView(Editor* editor, QWidget* parent = nullptr);

    void reload();  // reconstrói o quadro a partir do editor
    int beatCount() const { return static_cast<int>(beats_.size()); }
    QList<int> selectedBeats() const;
    void selectBeat(int id);
    QPoint beatCenter(int id) const;     // no viewport (para testes)
    QPoint connectorPoint(int id) const;  // bolinha de ligação, no viewport
    QPoint resizePoint(int id) const;     // canto de redimensionar, no viewport
    void editBeat(int id);                // abre a janela do beat
    int createBeatAt(QPointF scenePos, const QString& text = {}, const std::string& image = {});
    static QStringList colors();          // paleta das faixas

public slots:
    void newBeat();  // no centro da área visível, e abre a janela
    void zoomToFit();
    void zoomIn();
    void zoomOut();
    void deleteSelection();
    void pasteFromClipboard();  // imagem, arquivos de imagem ou texto viram beats
    void selectAllBeats();

signals:
    void changed();  // o editor mudou: a janela principal atualiza o resto

protected:
    void drawBackground(QPainter* p, const QRectF& rect) override;
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    friend class BeatItem;
    BeatItem* beatAt(QPoint viewPos) const;
    FlowItem* flowAt(QPoint viewPos) const;
    void flowsChanged();  // reposiciona as linhas quando um beat se move
    void commitGeometry();
    void setZoom(double z);
    void paste(QPointF scenePos);
    void chooseImage(int id);
    void setBeatColor(const QList<int>& ids, const QString& color);
    void updateSceneRect();
    void placeButtons();

    Editor* ed_;
    QHash<int, BeatItem*> beats_;
    QList<FlowItem*> flows_;
    double zoom_ = 1.0;

    // Interações em andamento.
    BeatItem* resizing_ = nullptr;
    QSizeF resizeStart_;
    QPointF pressScene_;
    BeatItem* linkFrom_ = nullptr;
    QGraphicsPathItem* linkPreview_ = nullptr;
    bool panning_ = false;
    bool spaceDown_ = false;
    QPoint panLast_;
    bool moved_ = false;

    QToolButton* addButton_;
    QToolButton* fitButton_;
};
