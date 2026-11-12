#include "settingsdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

#include "autosave.h"
#include "i18n.h"
#include "pagepainter.h"

namespace {
QLabel* hintLabel(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("subtituloDialogo");
    l->setWordWrap(true);
    return l;
}

QFormLayout* makeForm() {
    auto* f = new QFormLayout;
    f->setContentsMargins(0, 0, 0, 0);
    f->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    f->setHorizontalSpacing(14);
    f->setVerticalSpacing(10);
    f->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    return f;
}

// Famílias instaladas; só as monoespaçadas quando pedido (a página é uma grade de 10 colunas por polegada).
QStringList families(bool fixedPitchOnly) {
    QStringList out;
    for (const QString& f : QFontDatabase::families()) {
        if (QFontDatabase::isPrivateFamily(f)) continue;
        if (fixedPitchOnly && !QFontDatabase::isFixedPitch(f)) continue;
        out << f;
    }
    return out;
}

void selectData(QComboBox* c, const QVariant& v) { c->setCurrentIndex(std::max(0, c->findData(v))); }
}  // namespace

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Settings"));
    setMinimumSize(820, 580);
    QSettings settings;
    ThemeManager& tm = ThemeManager::instance();

    // ================= Geral
    language_ = new QComboBox;
    for (const i18n::Language& l : i18n::uiLanguages()) language_->addItem(l.name, l.code);
    selectData(language_, i18n::uiLanguage());
    restartNote_ = hintLabel(tr("Owlio restarts to switch the interface language."));
    restartNote_->setVisible(false);
    connect(language_, &QComboBox::currentIndexChanged, this,
            [this] { restartNote_->setVisible(uiLanguage() != i18n::activeLanguage()); });
    auto* langForm = makeForm();
    langForm->addRow(tr("Interface language"), language_);
    langForm->addRow(QString(), restartNote_);

    reopen_ = new QCheckBox(tr("Reopen the last script when Owlio starts"));
    reopen_->setChecked(settings.value("reopenLast", false).toBool());
    auto* startForm = makeForm();
    startForm->addRow(QString(), reopen_);

    // ================= Aparência
    theme_ = new QComboBox;
    theme_->addItem(tm.icon("sistema"), tr("Follow System"), static_cast<int>(ThemeManager::Mode::System));
    theme_->addItem(tm.icon("claro"), tr("Light"), static_cast<int>(ThemeManager::Mode::Light));
    theme_->addItem(tm.icon("escuro"), tr("Dark"), static_cast<int>(ThemeManager::Mode::Dark));
    selectData(theme_, static_cast<int>(tm.mode()));
    auto* themeForm = makeForm();
    themeForm->addRow(tr("Theme"), theme_);

    uiFont_ = new QComboBox;
    const QString uiDefault = ThemeManager::defaultInterfaceFamily();
    uiFont_->addItem(uiDefault.isEmpty() ? tr("Default") : tr("Default (%1)").arg(uiDefault), QString());
    for (const QString& f : families(false)) {
        uiFont_->addItem(f, f);
        uiFont_->setItemData(uiFont_->count() - 1, QFont(f), Qt::FontRole);
    }
    selectData(uiFont_, settings.value("uiFont").toString());
    uiSize_ = new QSpinBox;
    uiSize_->setRange(8, 16);
    uiSize_->setSuffix(" pt");
    uiSize_->setValue(qRound(settings.value("uiFontSize", ThemeManager::kDefaultFontSize).toDouble()));
    uiSize_->setFixedWidth(80);
    auto* uiRow = new QHBoxLayout;
    uiRow->setSpacing(8);
    uiRow->addWidget(uiFont_, 1);
    uiRow->addWidget(uiSize_);
    uiPreview_ = new QLabel(tr("The quick brown fox jumps over the lazy dog — 0123456789"));
    uiPreview_->setObjectName("amostraInterface");
    uiPreview_->setWordWrap(true);
    auto* fontForm = makeForm();
    fontForm->addRow(tr("Font"), uiRow);
    fontForm->addRow(QString(), uiPreview_);

    // ================= Roteiro
    script_ = new QComboBox;
    script_->addItem(tr("English: (MORE), (CONT'D), DAY, NIGHT, CUT TO:"),
                     static_cast<int>(ScriptLanguage::English));
    script_->addItem(tr("Portuguese: (MAIS), (CONT.), DIA, NOITE, CORTA PARA:"),
                     static_cast<int>(ScriptLanguage::Portuguese));
    script_->setCurrentIndex(i18n::scriptLanguage() == ScriptLanguage::Portuguese ? 1 : 0);
    auto* convForm = makeForm();
    convForm->addRow(tr("Language"), script_);

    scriptFont_ = new QComboBox;
    scriptFont_->addItem(tr("Automatic (%1)").arg(PagePainter::defaultFontFamily()), QString());
    for (const QString& f : families(true)) {
        scriptFont_->addItem(f, f);
        scriptFont_->setItemData(scriptFont_->count() - 1, QFont(f), Qt::FontRole);
    }
    selectData(scriptFont_, settings.value("scriptFont").toString());
    scriptPreview_ = new QLabel;
    scriptPreview_->setObjectName("amostraRoteiro");
    scriptPreview_->setTextFormat(Qt::PlainText);
    scriptPreview_->setText(tr("INT. DINER - NIGHT") + "\n\n" + tr("Eleanor slides into the booth.") + "\n\n" +
                            QString(22, ' ') + tr("ELEANOR") + "\n" + QString(10, ' ') +
                            tr("I'm looking for the chain") + "\n" + QString(10, ' ') + tr("of title."));
    // Amostras não ditam a largura da janela: cortam o excesso em vez de empurrar os campos.
    for (QLabel* l : {scriptPreview_, uiPreview_}) {
        l->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        l->setMinimumWidth(1);
    }
    sceneNumbers_ = new QCheckBox(tr("Number scenes"));
    sceneNumbers_->setChecked(settings.value("sceneNumbers", false).toBool());
    auto* scriptFontForm = makeForm();
    scriptFontForm->addRow(tr("Font"), scriptFont_);
    scriptFontForm->addRow(QString(), scriptPreview_);
    scriptFontForm->addRow(QString(), sceneNumbers_);

    // ================= Salvamento
    autosave_ = new QComboBox;
    autosave_->addItem(tr("Off"), 0);
    autosave_->addItem(tr("Every minute"), 1);
    autosave_->addItem(tr("Every 5 minutes"), 5);
    autosave_->addItem(tr("Every 10 minutes"), 10);
    selectData(autosave_, settings.value("autosaveMinutes", 5).toInt());
    backups_ = new QCheckBox(tr("Keep a dated backup copy every time the script is saved"));
    backups_->setChecked(settings.value("keepBackups", true).toBool());
    auto* openBackups = new QPushButton(tm.icon("historico", tm.colors().accent), tr("Open Backups Folder"));
    openBackups->setObjectName("link");
    openBackups->setCursor(Qt::PointingHandCursor);
    connect(openBackups, &QPushButton::clicked, this, [] {
        QDir().mkpath(autosave::backupRoot());
        QDesktopServices::openUrl(QUrl::fromLocalFile(autosave::backupRoot()));
    });
    auto* saveForm = makeForm();
    saveForm->addRow(tr("Save automatically"), autosave_);
    saveForm->addRow(QString(), backups_);
    saveForm->addRow(QString(), openBackups);

    for (QComboBox* c : {language_, script_, theme_, uiFont_, scriptFont_, autosave_}) {
        c->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        c->setMaxVisibleItems(14);
        c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->setMinimumContentsLength(16);
    }

    // Rótulos de todos os formulários com a mesma largura, para os campos alinharem entre seções.
    const QList<QFormLayout*> forms = {langForm, startForm, themeForm, fontForm, convForm, scriptFontForm, saveForm};
    int labelWidth = 0;
    for (QFormLayout* f : forms)
        for (int r = 0; r < f->rowCount(); ++r)
            if (auto* l = f->itemAt(r, QFormLayout::LabelRole); l && l->widget())
                labelWidth = std::max(labelWidth, l->widget()->sizeHint().width());
    for (QFormLayout* f : forms)
        for (int r = 0; r < f->rowCount(); ++r)
            if (auto* l = f->itemAt(r, QFormLayout::LabelRole); l && l->widget())
                l->widget()->setMinimumWidth(labelWidth);
    // Linhas sem rótulo também precisam do recuo, senão o campo vai até a borda esquerda.
    for (QFormLayout* f : forms)
        for (int r = 0; r < f->rowCount(); ++r)
            if (!f->itemAt(r, QFormLayout::LabelRole) && f->itemAt(r, QFormLayout::FieldRole)) {
                auto* spacer = new QWidget;
                spacer->setFixedWidth(labelWidth);
                f->setWidget(r, QFormLayout::LabelRole, spacer);
            }

    // ================= Páginas
    pages_ = new QStackedWidget;
    pages_->addWidget(page(tr("General"), tr("Language and startup."),
                           {section(tr("Language"), langForm), section(tr("Startup"), startForm)}));
    pages_->addWidget(page(tr("Appearance"), tr("Theme and the font of menus, panels and dialogs."),
                           {section(tr("Theme"), themeForm),
                            section(tr("Font"), fontForm,
                                    tr("Changes apply right away to the whole interface."))}));
    pages_->addWidget(page(
        tr("Script"), tr("How the pages look and which conventions they follow."),
        {section(tr("Conventions"), convForm,
                 tr("Used for page breaks inside dialogue and for autocomplete suggestions.")),
         section(tr("Page font"), scriptFontForm,
                 tr("Only monospaced fonts keep the industry layout: the page is always set at 12 points, "
                    "10 characters per inch, so page count stays the same whatever font you pick."))}));
    pages_->addWidget(page(tr("Saving"), tr("Autosave, backups and crash recovery."),
                           {section(tr("Saving"), saveForm,
                                    tr("Untitled scripts are not saved automatically, but if Owlio closes "
                                       "unexpectedly your unsaved changes are offered back the next time it "
                                       "starts."))}));

    nav_ = new QListWidget;
    nav_->setObjectName("navConfig");
    nav_->setFixedWidth(196);
    nav_->setIconSize(QSize(18, 18));
    nav_->setFrameShape(QFrame::NoFrame);
    const std::pair<const char*, QString> navItems[] = {{"configuracoes", tr("General")},
                                                         {"tema", tr("Appearance")},
                                                         {"roteiro", tr("Script")},
                                                         {"salvar", tr("Saving")}};
    for (const auto& [icon, label] : navItems) {
        auto* it = new QListWidgetItem(tm.icon(icon, tm.colors().muted), label);
        it->setSizeHint(QSize(0, 38));
        nav_->addItem(it);
    }
    connect(nav_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);

    auto* navTitle = new QLabel(tr("Settings"));
    navTitle->setObjectName("tituloNav");
    auto* side = new QWidget;
    side->setObjectName("ladoConfig");
    auto* sideLay = new QVBoxLayout(side);
    sideLay->setContentsMargins(12, 20, 12, 12);
    sideLay->setSpacing(10);
    sideLay->addWidget(navTitle);
    sideLay->addWidget(nav_, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText(tr("Save"));
    bb->button(QDialogButtonBox::Save)->setObjectName("primarioBotao");
    bb->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    for (QAbstractButton* b : bb->buttons()) b->setIcon(QIcon());
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 24, 20);
    right->setSpacing(12);
    right->addWidget(pages_, 1);
    right->addWidget(bb);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(24);
    root->addWidget(side);
    root->addLayout(right, 1);

    connect(uiFont_, &QComboBox::currentIndexChanged, this, &SettingsDialog::refreshFontPreviews);
    connect(uiSize_, &QSpinBox::valueChanged, this, &SettingsDialog::refreshFontPreviews);
    connect(scriptFont_, &QComboBox::currentIndexChanged, this, &SettingsDialog::refreshFontPreviews);
    refreshFontPreviews();
    nav_->setCurrentRow(0);
    // Só depois de estar na janela: antes disso o QDialog escolheria outro botão padrão.
    bb->button(QDialogButtonBox::Save)->setDefault(true);
}

QWidget* SettingsDialog::section(const QString& title, QFormLayout* form, const QString& hint) {
    auto* box = new QWidget;
    auto* lay = new QVBoxLayout(box);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);
    auto* label = new QLabel(title.toUpper());
    label->setObjectName("secao");
    QFont f = label->font();
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
    label->setFont(f);
    lay->addWidget(label);
    lay->addLayout(form);
    if (!hint.isEmpty()) lay->addWidget(hintLabel(hint));
    return box;
}

QWidget* SettingsDialog::page(const QString& title, const QString& subtitle, QList<QWidget*> sections) {
    auto* content = new QWidget;
    auto* lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 22, 8, 8);
    lay->setSpacing(6);
    auto* heading = new QLabel(title);
    heading->setObjectName("tituloDialogo");
    lay->addWidget(heading);
    lay->addWidget(hintLabel(subtitle));
    for (QWidget* s : sections) {
        lay->addSpacing(16);
        lay->addWidget(s);
    }
    lay->addStretch(1);
    auto* scroll = new QScrollArea;
    scroll->setObjectName("paginaConfig");
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return scroll;
}

void SettingsDialog::refreshFontPreviews() {
    QFont ui = uiFont_->currentIndex() == 0 ? QApplication::font() : QFont(interfaceFont());
    if (uiFont_->currentIndex() == 0 && !ThemeManager::defaultInterfaceFamily().isEmpty())
        ui.setFamily(ThemeManager::defaultInterfaceFamily());
    ui.setPointSizeF(interfaceFontSize());
    uiPreview_->setFont(ui);

    QFont page(scriptFont().isEmpty() ? PagePainter::defaultFontFamily() : scriptFont());
    page.setStyleHint(QFont::TypeWriter);
    page.setPointSizeF(10.5);
    scriptPreview_->setFont(page);
}

void SettingsDialog::showPage(int index) { nav_->setCurrentRow(std::clamp(index, 0, pages_->count() - 1)); }

int SettingsDialog::autosaveMinutes() const { return autosave_->currentData().toInt(); }
bool SettingsDialog::keepBackups() const { return backups_->isChecked(); }
bool SettingsDialog::reopenLastScript() const { return reopen_->isChecked(); }
QString SettingsDialog::interfaceFont() const { return uiFont_->currentData().toString(); }
double SettingsDialog::interfaceFontSize() const { return uiSize_->value(); }
QString SettingsDialog::scriptFont() const { return scriptFont_->currentData().toString(); }
bool SettingsDialog::numberScenes() const { return sceneNumbers_->isChecked(); }

QString SettingsDialog::uiLanguage() const { return language_->currentData().toString(); }

ScriptLanguage SettingsDialog::scriptLanguage() const {
    return static_cast<ScriptLanguage>(script_->currentData().toInt());
}

ThemeManager::Mode SettingsDialog::themeMode() const {
    return static_cast<ThemeManager::Mode>(theme_->currentData().toInt());
}
