#pragma once
#include <QDialog>

#include "editor.h"
#include "theme.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QListWidget;
class QSpinBox;
class QStackedWidget;

// Configurações em páginas (Geral, Aparência, Roteiro, Salvamento), com navegação à esquerda.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    QString uiLanguage() const;
    ScriptLanguage scriptLanguage() const;
    bool spellCheck() const;
    QString spellLanguage() const;  // "auto", "pt" ou "en"
    ThemeManager::Mode themeMode() const;
    int autosaveMinutes() const;  // 0 = desligado
    bool keepBackups() const;
    bool versionsFullScreen() const;
    bool reopenLastScript() const;
    QString interfaceFont() const;  // vazio = padrão
    double interfaceFontSize() const;
    QString scriptFont() const;  // vazio = automática
    bool numberScenes() const;
    void showPage(int index);  // 0 Geral, 1 Aparência, 2 Roteiro, 3 Salvamento

private:
    QWidget* page(const QString& title, const QString& subtitle, QList<QWidget*> sections);
    QWidget* section(const QString& title, QFormLayout* form, const QString& hint = {});
    void refreshFontPreviews();

    QListWidget* nav_;
    QStackedWidget* pages_;
    QComboBox* language_;
    QCheckBox* reopen_;
    QComboBox* script_;
    QComboBox* theme_;
    QComboBox* uiFont_;
    QSpinBox* uiSize_;
    QLabel* uiPreview_;
    QComboBox* scriptFont_;
    QLabel* scriptPreview_;
    QCheckBox* sceneNumbers_;
    QCheckBox* spellCheck_;
    QComboBox* spellLanguage_;
    QComboBox* autosave_;
    QCheckBox* backups_;
    QCheckBox* versionsFull_;
    QLabel* restartNote_;
};
