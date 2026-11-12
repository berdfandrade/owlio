#pragma once
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPointF>
#include <QRawFont>
#include <map>

#include "editor.h"

// Desenha páginas do roteiro em qualquer QPaintDevice (tela, PDF, impressora).
// Tudo é posicionado numa grade exata de 10 colunas e 6 linhas por polegada.
class PagePainter {
public:
    struct Extras {
        bool showCursor = false;
        bool showSelection = false;
        bool sceneNumbers = false;
        std::u32string ghost;    // sugestão do autocompletar
        std::u32string preedit;  // composição do método de entrada (acentos, etc.)
        QColor selectionColor = QColor(207, 222, 255);
        QColor cursorColor = QColor(59, 108, 246);
        int focusFrom = -1, focusTo = -1;  // modo foco: parágrafos em destaque (inclusive)
    };

    explicit PagePainter(const Editor& ed) : ed_(ed) {}

    static QString fontFamily();
    QRawFont rawFont(double ppi);

    void paintPage(QPainter& p, int page, QPointF origin, double ppi, const Extras& ex);
    void paintTitlePage(QPainter& p, QPointF origin, double ppi);
    bool hasTitlePage() const;

private:
    void drawText(QPainter& p, const QRawFont& f, const std::u32string& text, double x, double baseline,
                  double charW, const QColor& color);

    const Editor& ed_;
    std::map<int, QRawFont> fonts_;
};
