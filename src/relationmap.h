#pragma once
#include <QDialog>
#include <QHash>
#include <QPointF>
#include <QWidget>

#include "editor.h"

class QComboBox;
class QLineEdit;
class QToolButton;

// Janela de uma ligação: tipo (família, amor...) e um rótulo livre.
class RelationDialog : public QDialog {
    Q_OBJECT
public:
    RelationDialog(const QString& a, const QString& b, const Relationship* current, QWidget* parent = nullptr);
    RelationKind kind() const;
    QString label() const;
    bool removeRequested() const { return remove_; }
    static QString kindName(RelationKind k);
    static QColor kindColor(RelationKind k);

private:
    QComboBox* kind_;
    QLineEdit* label_;
    bool remove_ = false;
};

// Mapa de relações: cada personagem é um círculo com a foto, maior para quem fala mais. Arraste para
// mover; arraste do "+" de um personagem até outro para ligar os dois. As linhas tracejadas mostram
// quem divide cenas no roteiro. Posições e ligações ficam no .fountain, e cada mudança se desfaz.
class RelationMapView : public QWidget {
    Q_OBJECT
public:
    explicit RelationMapView(Editor* editor, QWidget* parent = nullptr);
    void reload();
    QStringList names() const { return order_; }
    QPoint nodeCenter(const QString& name) const;  // no widget (para testes)
    QPoint handlePoint(const QString& name) const;  // o "+" de ligar
    void link(const QString& a, const QString& b, RelationKind kind, const QString& label);  // sem janela

public slots:
    void zoomToFit();
    void arrange();  // em círculo, quem fala mais no alto

signals:
    void changed();
    void openSheet(const QString& name);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    struct Node {
        QPointF pos;  // no mapa
        double radius = 34;
        int speeches = 0;
        QString role;
        std::string photo;
    };
    QPointF toMap(QPointF w) const { return (w - pan_) / zoom_; }
    QPointF toWidget(QPointF m) const { return m * zoom_ + pan_; }
    QString nodeAt(QPointF w) const;
    QString handleAt(QPointF w) const;
    int linkAt(QPointF w) const;  // índice em doc().relations.links, -1 se nenhum
    void commitPositions();
    void editLink(const QString& a, const QString& b);
    void placeButtons();
    QPointF handleOf(const QString& name) const;  // no mapa

    Editor* ed_;
    QHash<QString, Node> nodes_;
    QStringList order_;
    QHash<QString, int> shared_;  // "A\nB" -> cenas em comum
    double zoom_ = 1.0;
    QPointF pan_;
    bool fitted_ = false;
    bool showShared_ = true;

    QString hover_, dragging_, linking_;
    int selectedLink_ = -1;
    QPointF pressMap_, grabOffset_, linkEnd_, panStart_, pressWidget_;
    bool panning_ = false, moved_ = false;

    QToolButton* sharedBtn_;
    QToolButton* arrangeBtn_;
    QToolButton* fitBtn_;
};
