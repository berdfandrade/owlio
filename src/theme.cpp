#include "theme.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QEvent>
#include <QFontDatabase>
#include <QMenu>
#include <QPainter>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>
#include <QSvgRenderer>
#include <QTimer>

namespace {

ThemeColors lightColors() {
    ThemeColors c;
    c.dark = false;
    c.window = QColor("#f4f5f7");
    c.surface = QColor("#fbfbfc");
    c.raised = QColor("#ffffff");
    c.hover = QColor("#eaecf0");
    c.selected = QColor("#e1e9ff");
    c.border = QColor("#e1e4e9");
    c.text = QColor("#1f2329");
    c.muted = QColor("#6b7280");
    c.accent = QColor("#3b6cf6");
    c.accentHover = QColor("#2f5be0");
    c.accentText = QColor("#ffffff");
    c.canvas = QColor("#e3e6eb");
    c.scroll = QColor("#c5cad3");
    return c;
}

ThemeColors darkColors() {
    ThemeColors c;
    c.dark = true;
    c.window = QColor("#17181c");
    c.surface = QColor("#1c1d22");
    c.raised = QColor("#25272e");
    c.hover = QColor("#2a2c34");
    c.selected = QColor("#2c3654");
    c.border = QColor("#2c2e36");
    c.text = QColor("#e4e6eb");
    c.muted = QColor("#8d93a0");
    c.accent = QColor("#6c8cff");
    c.accentHover = QColor("#829dff");
    c.accentText = QColor("#ffffff");
    c.canvas = QColor("#101114");
    c.scroll = QColor("#3a3d47");
    return c;
}

QPalette paletteFor(const ThemeColors& c) {
    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.surface);
    p.setColor(QPalette::AlternateBase, c.hover);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.raised);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::ToolTipBase, c.raised);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::PlaceholderText, c.muted);
    p.setColor(QPalette::Highlight, c.accent);
    p.setColor(QPalette::HighlightedText, c.accentText);
    p.setColor(QPalette::Link, c.accent);
    p.setColor(QPalette::Accent, c.accent);
    p.setColor(QPalette::Mid, c.border);
    p.setColor(QPalette::Dark, c.border.darker(130));
    p.setColor(QPalette::Light, c.raised);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, c.muted);
    return p;
}

}  // namespace

ThemeManager& ThemeManager::instance() {
    static ThemeManager tm;
    return tm;
}

void ThemeManager::init() {
    // Fonte de interface moderna, se existir no sistema.
    const QStringList fams = QFontDatabase::families();
    for (const char* f : {"Inter", "Inter Variable", "Cantarell", "Ubuntu", "Noto Sans"}) {
        if (fams.contains(QString::fromUtf8(f))) {
            QFont font(QString::fromUtf8(f));
            font.setPointSizeF(10);
            QApplication::setFont(font);
            break;
        }
    }

    // Menus com cantos arredondados de verdade: janela translúcida e sem moldura.
    qApp->installEventFilter(this);

    mode_ = static_cast<Mode>(QSettings().value("theme", 0).toInt());
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (mode_ == Mode::System) apply();
    });

    // Recarga ao vivo do QSS durante o desenvolvimento.
    livePath_ = qEnvironmentVariable("OWLIO_QSS");
    if (!livePath_.isEmpty() && QFileInfo::exists(livePath_)) {
        livePath_ = QFileInfo(livePath_).absoluteFilePath();
        watcher_.addPath(livePath_);
        connect(&watcher_, &QFileSystemWatcher::fileChanged, this, [this] {
            // Editores costumam substituir o arquivo; espera e volta a observar.
            QTimer::singleShot(100, this, [this] {
                if (!watcher_.files().contains(livePath_)) watcher_.addPath(livePath_);
                apply();
            });
        });
    } else {
        livePath_.clear();
    }
    apply();
}

void ThemeManager::setMode(Mode m, bool persist) {
    mode_ = m;
    if (persist) QSettings().setValue("theme", static_cast<int>(m));
    apply();
}

bool ThemeManager::eventFilter(QObject* obj, QEvent* ev) {
    if (ev->type() == QEvent::Polish) {
        if (auto* menu = qobject_cast<QMenu*>(obj)) {
            menu->setAttribute(Qt::WA_TranslucentBackground);
            menu->setWindowFlag(Qt::FramelessWindowHint);
            menu->setWindowFlag(Qt::NoDropShadowWindowHint);
        }
    }
    return QObject::eventFilter(obj, ev);
}

QString ThemeManager::styleTemplate() {
    QFile f(livePath_.isEmpty() ? QStringLiteral(":/themes/estilo.qss") : livePath_);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(f.readAll());
}

void ThemeManager::apply() {
    bool dark = mode_ == Mode::Dark;
    if (mode_ == Mode::System) dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    colors_ = dark ? darkColors() : lightColors();
    const ThemeColors& c = colors_;

    QString qss = styleTemplate();
    const QString suffix = dark ? "escura" : "clara";
    // Tokens mais longos primeiro (@accentHover antes de @accent).
    const std::pair<const char*, QString> tokens[] = {
        {"@accentHover", c.accentHover.name()}, {"@accentText", c.accentText.name()},
        {"@accent", c.accent.name()},           {"@window", c.window.name()},
        {"@surface", c.surface.name()},         {"@raised", c.raised.name()},
        {"@hover", c.hover.name()},             {"@selected", c.selected.name()},
        {"@border", c.border.name()},           {"@text", c.text.name()},
        {"@muted", c.muted.name()},             {"@canvas", c.canvas.name()},
        {"@scroll", c.scroll.name()},           {"@arrow", ":/icons/seta-" + suffix + ".svg"},
        {"@checkWhite", ":/icons/check-branco.svg"},
        {"@check", ":/icons/check-" + suffix + ".svg"},
        {"@submenu", ":/icons/seta-direita-" + suffix + ".svg"},
    };
    for (const auto& [token, value] : tokens) qss.replace(QString::fromUtf8(token), value);

    QApplication::setPalette(paletteFor(c));
    qApp->setStyleSheet(qss);
    emit changed();
}

QIcon ThemeManager::icon(const QString& name, const QColor& color) const {
    QFile f(":/icons/" + name + ".svg");
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray svg = f.readAll();
    auto render = [&](const QColor& col, int size) {
        QByteArray data = svg;
        data.replace("currentColor", col.name().toUtf8());
        QSvgRenderer r(data);
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        r.render(&p);
        return pm;
    };
    const QColor normal = color.isValid() ? color : colors_.text;
    QColor disabled = colors_.muted;
    disabled.setAlphaF(0.5);
    QIcon icon;
    for (int s : {16, 18, 20, 24, 32, 36, 40, 48}) {
        icon.addPixmap(render(normal, s), QIcon::Normal);
        icon.addPixmap(render(disabled, s), QIcon::Disabled);
    }
    return icon;
}
