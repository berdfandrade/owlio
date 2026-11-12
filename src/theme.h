#pragma once
#include <QColor>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QObject>

struct ThemeColors {
    bool dark = false;
    QColor window;       // fundo da janela, barra de ferramentas e status
    QColor surface;      // painéis laterais e campos
    QColor raised;       // menus, popups
    QColor hover;
    QColor selected;
    QColor border;
    QColor text;
    QColor muted;        // texto secundário
    QColor accent;
    QColor accentHover;
    QColor accentText;
    QColor canvas;       // fundo atrás das páginas
    QColor scroll;
};

// Aplica a paleta + folha de estilo e fornece ícones recoloridos para o tema atual.
class ThemeManager : public QObject {
    Q_OBJECT
public:
    enum class Mode { System = 0, Light, Dark };

    static ThemeManager& instance();
    void init();  // lê a preferência salva e acompanha o tema do sistema
    void setMode(Mode m, bool persist = true);
    Mode mode() const { return mode_; }
    const ThemeColors& colors() const { return colors_; }

    // Ícone de resources/icons/<nome>.svg na cor do tema (ou na cor indicada).
    QIcon icon(const QString& name, const QColor& color = QColor()) const;

signals:
    void changed();

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    ThemeManager() = default;
    void apply();
    QString styleTemplate();

    Mode mode_ = Mode::System;
    ThemeColors colors_;
    QFileSystemWatcher watcher_;
    QString livePath_;
};
