#include "settingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QSettings>
#include <QUrl>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "autosave.h"
#include "i18n.h"

namespace {
QLabel* sectionLabel(const QString& text) {
    auto* l = new QLabel(text.toUpper());
    l->setObjectName("secao");
    QFont f = l->font();
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
    l->setFont(f);
    return l;
}

QLabel* hint(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("subtituloDialogo");
    l->setWordWrap(true);
    return l;
}

QFormLayout* makeForm() {
    auto* f = new QFormLayout;
    f->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    f->setHorizontalSpacing(14);
    f->setVerticalSpacing(8);
    f->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    return f;
}
}  // namespace

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Settings"));
    setMinimumWidth(560);

    auto* heading = new QLabel(tr("Settings"));
    heading->setObjectName("tituloDialogo");

    // ---- Idioma
    language_ = new QComboBox;
    for (const i18n::Language& l : i18n::uiLanguages()) language_->addItem(l.name, l.code);
    language_->setCurrentIndex(std::max(0, language_->findData(i18n::uiLanguage())));
    restartNote_ = hint(tr("Owlio restarts to switch the interface language."));
    restartNote_->setVisible(false);
    connect(language_, &QComboBox::currentIndexChanged, this,
            [this] { restartNote_->setVisible(uiLanguage() != i18n::activeLanguage()); });
    auto* f1 = makeForm();
    f1->addRow(tr("Interface language"), language_);

    // ---- Roteiro
    script_ = new QComboBox;
    script_->addItem(tr("English: (MORE), (CONT'D), DAY, NIGHT, CUT TO:"),
                     static_cast<int>(ScriptLanguage::English));
    script_->addItem(tr("Portuguese: (MAIS), (CONT.), DIA, NOITE, CORTA PARA:"),
                     static_cast<int>(ScriptLanguage::Portuguese));
    script_->setCurrentIndex(i18n::scriptLanguage() == ScriptLanguage::Portuguese ? 1 : 0);
    auto* f2 = makeForm();
    f2->addRow(tr("Script conventions"), script_);

    // ---- Aparência
    theme_ = new QComboBox;
    theme_->addItem(tr("Follow System"), static_cast<int>(ThemeManager::Mode::System));
    theme_->addItem(tr("Light"), static_cast<int>(ThemeManager::Mode::Light));
    theme_->addItem(tr("Dark"), static_cast<int>(ThemeManager::Mode::Dark));
    theme_->setCurrentIndex(theme_->findData(static_cast<int>(ThemeManager::instance().mode())));
    auto* f3 = makeForm();
    f3->addRow(tr("Theme"), theme_);

    // ---- Salvamento
    autosave_ = new QComboBox;
    autosave_->addItem(tr("Off"), 0);
    autosave_->addItem(tr("Every minute"), 1);
    autosave_->addItem(tr("Every 5 minutes"), 5);
    autosave_->addItem(tr("Every 10 minutes"), 10);
    autosave_->setCurrentIndex(std::max(0, autosave_->findData(QSettings().value("autosaveMinutes", 5).toInt())));
    backups_ = new QCheckBox(tr("Keep a dated backup copy every time the script is saved"));
    backups_->setChecked(QSettings().value("keepBackups", true).toBool());
    auto* openBackups = new QPushButton(tr("Open Backups Folder"));
    openBackups->setObjectName("link");
    connect(openBackups, &QPushButton::clicked, this, [] {
        QDir().mkpath(autosave::backupRoot());
        QDesktopServices::openUrl(QUrl::fromLocalFile(autosave::backupRoot()));
    });
    auto* f4 = makeForm();
    f4->addRow(tr("Save automatically"), autosave_);
    f4->addRow(QString(), backups_);
    f4->addRow(QString(), openBackups);

    for (QComboBox* c : {language_, script_, theme_, autosave_}) c->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Rótulos das três seções com a mesma largura, para os campos alinharem.
    int labelWidth = 0;
    for (QFormLayout* f : {f1, f2, f3, f4})
        for (int r = 0; r < f->rowCount(); ++r)
            if (auto* l = f->itemAt(r, QFormLayout::LabelRole))
                labelWidth = std::max(labelWidth, l->widget()->sizeHint().width());
    for (QFormLayout* f : {f1, f2, f3, f4})
        for (int r = 0; r < f->rowCount(); ++r)
            if (auto* l = f->itemAt(r, QFormLayout::LabelRole)) l->widget()->setMinimumWidth(labelWidth);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText(tr("Save"));
    bb->button(QDialogButtonBox::Save)->setObjectName("primarioBotao");
    bb->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    for (QAbstractButton* b : bb->buttons()) b->setIcon(QIcon());
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(8);
    root->addWidget(heading);
    root->addSpacing(10);
    root->addWidget(sectionLabel(tr("Language")));
    root->addLayout(f1);
    root->addWidget(restartNote_);
    root->addSpacing(10);
    root->addWidget(sectionLabel(tr("Script")));
    root->addLayout(f2);
    root->addWidget(hint(tr("Used for page breaks inside dialogue and for autocomplete suggestions.")));
    root->addSpacing(10);
    root->addWidget(sectionLabel(tr("Appearance")));
    root->addLayout(f3);
    root->addSpacing(10);
    root->addWidget(sectionLabel(tr("Saving")));
    root->addLayout(f4);
    root->addWidget(hint(tr("Untitled scripts are not saved automatically, but if Owlio closes unexpectedly "
                            "your unsaved changes are offered back the next time it starts.")));
    root->addSpacing(18);
    root->addStretch(1);
    root->addWidget(bb);
    // Só depois de estar na janela: antes disso o QDialog escolheria outro botão padrão.
    bb->button(QDialogButtonBox::Save)->setDefault(true);
}

int SettingsDialog::autosaveMinutes() const { return autosave_->currentData().toInt(); }
bool SettingsDialog::keepBackups() const { return backups_->isChecked(); }

QString SettingsDialog::uiLanguage() const { return language_->currentData().toString(); }

ScriptLanguage SettingsDialog::scriptLanguage() const {
    return static_cast<ScriptLanguage>(script_->currentData().toInt());
}

ThemeManager::Mode SettingsDialog::themeMode() const {
    return static_cast<ThemeManager::Mode>(theme_->currentData().toInt());
}
