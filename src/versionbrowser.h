#pragma once
#include <QDateTime>
#include <QDialog>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QList>
#include <memory>
#include <vector>

#include "editor.h"

class QPushButton;
class QVariantAnimation;

// Máquina do tempo do roteiro: as versões guardadas (backups) empilhadas em perspectiva,
// da mais nova (na frente) para a mais antiga (ao fundo), com uma linha do tempo ao lado.
// Escolha uma e clique em Restaurar; um Ctrl+Z desfaz a restauração.
class VersionBrowser : public QDialog {
    Q_OBJECT
public:
    struct Version {
        QString file;
        QDateTime when;
    };
    // Versões guardadas de um roteiro, da mais nova para a mais antiga.
    static QList<Version> versionsOf(const QString& scriptPath);

    // `current` é o roteiro aberto agora (a primeira entrada, "Agora"); `page` é a página mostrada.
    VersionBrowser(const Editor& current, const QString& scriptName, const QList<Version>& versions, int page,
                   QWidget* parent = nullptr);
    ~VersionBrowser() override;

    int count() const { return static_cast<int>(entries_.size()); }
    int selected() const { return target_; }
    void select(int index, bool animate = true);
    int page() const { return page_; }
    void setPage(int page);
    Document selectedDocument();     // conteúdo da versão escolhida
    QString label(int index) const;  // "Hoje (agora)", "Ontem, 14:32"…
    // Em vez de tela cheia, fica exatamente sobre `area` (a área do roteiro) e a acompanha
    // se a janela principal mudar de lugar ou de tamanho.
    void cover(QWidget* area);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    struct Entry {
        QString file;
        QDateTime when;
        std::unique_ptr<Editor> ed;  // carregado sob demanda; vazio na entrada "Agora"
        QHash<int, QImage> pages;    // página -> imagem, na largura atual do cartão
    };

    const Editor* editorAt(int i);
    QImage pageImage(int i);
    QRectF frontRect() const;     // cartão da frente
    QPointF vanishingPoint() const;
    QRectF timelineRect() const;  // trilho da linha do tempo
    double tickY(int i) const;
    int nearestTick(double y) const;  // marca mais próxima na altura y
    bool onRail(QPointF pt) const;     // ponto sobre a coluna da linha do tempo
    QRectF arrowRect(bool older) const;
    QString dayLabel(const QDate& d) const;
    void paintBackdrop(QPainter& p);
    void drawSky(QPainter& p);
    const QPixmap& shadow();  // sombra pronta no tamanho do cartão da frente
    void warmUp();  // renderiza, fora da animação, as páginas que vão aparecer em seguida
    void paintCard(QPainter& p, int i, double depth, double visibleBottom);
    static double scaleAt(double depth);
    static double alphaAt(double depth);
    QRectF cardRectAt(double depth) const;
    void paintTimeline(QPainter& p);
    void paintArrows(QPainter& p);
    void updateBottom();

    void fitToArea();

    QWidget* area_ = nullptr;
    const Editor& current_;
    QString scriptName_;
    std::vector<Entry> entries_;
    int target_ = 0;
    double pos_ = 0;  // posição animada da pilha (índice fracionário)
    int page_ = 0;
    int hoverTick_ = -1;
    bool scrubbing_ = false;  // arrastando ao longo da linha do tempo
    int hoverArrow_ = -1;  // 0 = mais antiga, 1 = mais nova
    double cachedWidth_ = 0;
    std::vector<QPointF> stars_;
    QPixmap backdrop_;
    QPixmap shadow_;
    QTimer* warm_;
    QVariantAnimation* anim_;
    QPushButton* cancel_;
    QPushButton* restore_;
};
