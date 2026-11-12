#include "settingsdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

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

    for (QComboBox* c : {language_, script_, theme_}) c->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Rótulos das três seções com a mesma largura, para os campos alinharem.
    int labelWidth = 0;
    for (QFormLayout* f : {f1, f2, f3})
        for (int r = 0; r < f->rowCount(); ++r)
            if (auto* l = f->itemAt(r, QFormLayout::LabelRole))
                labelWidth = std::max(labelWidth, l->widget()->sizeHint().width());
    for (QFormLayout* f : {f1, f2, f3})
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
    root->addSpacing(18);
    root->addStretch(1);
    root->addWidget(bb);
    // Só depois de estar na janela: antes disso o QDialog escolheria outro botão padrão.
    bb->button(QDialogButtonBox::Save)->setDefault(true);
}

QString SettingsDialog::uiLanguage() const { return language_->currentData().toString(); }

ScriptLanguage SettingsDialog::scriptLanguage() const {
    return static_cast<ScriptLanguage>(script_->currentData().toInt());
}

ThemeManager::Mode SettingsDialog::themeMode() const {
    return static_cast<ThemeManager::Mode>(theme_->currentData().toInt());
}
