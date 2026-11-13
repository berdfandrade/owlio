#pragma once
#include <QAbstractScrollArea>
#include <QTimer>
#include <vector>

#include "editor.h"

// Quadro de cartões: cada cena do roteiro como um cartão, em grade ou em lista (uma faixa por cena).
// Arraste para reordenar, duplo clique para abrir a cena, F2 para editar a sinopse.
class SceneBoard : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit SceneBoard(Editor* editor, QWidget* parent = nullptr);

    void reload();  // relê as cenas do editor
    int selectedScene() const { return selected_; }
    void selectScene(int index, bool scrollTo = true);
    int sceneCount() const { return static_cast<int>(cards_.size()); }
    QRectF cardRect(int i) const;  // em coordenadas do viewport
    bool listMode() const { return list_; }
    void setListMode(bool on);

signals:
    void openScene(int para);
    void moveRequested(int from, int to);
    void editSynopsisRequested(int index);
    void selectionChanged(int index);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void leaveEvent(QEvent*) override;
    void scrollContentsBy(int, int) override { viewport()->update(); }

private:
    void relayout();
    int cardAt(QPointF pt) const;
    int dropIndexAt(QPointF pt, QRectF* bar) const;
    void paintCard(QPainter& p, const QRectF& r, int index, bool selected, bool hover) const;
    void paintRow(QPainter& p, const QRectF& r, int index) const;  // corpo do cartão no modo lista
    QRectF toggleRect(bool listButton) const;                      // alternador grade/lista, no cabeçalho
    double gap() const { return list_ ? 10 : kGap; }
    void requestMove(int from, int to);

    Editor* ed_;
    std::vector<SceneSummary> cards_;
    int pages_ = 1;

    double minCardW_ = 250;
    double cardW_ = 250, cardH_ = 210;
    int cols_ = 1;
    bool list_ = false;
    int hoverToggle_ = -1;  // 0 = grade, 1 = lista
    static constexpr double kGap = 22, kMargin = 32, kHeader = 78;

    int selected_ = -1;
    int hover_ = -1;
    int pressIndex_ = -1;
    bool dragging_ = false;
    int dropIndex_ = -1;
    QRectF dropBar_;
    QPointF pressPos_, dragPos_, grabOffset_;
    QTimer autoScroll_;
};
