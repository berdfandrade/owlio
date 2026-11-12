#pragma once
#include <QDialog>

#include "editor.h"
#include "theme.h"

class QComboBox;
class QLabel;

// Configurações: idioma da interface, convenções do roteiro e tema.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    QString uiLanguage() const;
    ScriptLanguage scriptLanguage() const;
    ThemeManager::Mode themeMode() const;

private:
    QComboBox* language_;
    QComboBox* script_;
    QComboBox* theme_;
    QLabel* restartNote_;
};
