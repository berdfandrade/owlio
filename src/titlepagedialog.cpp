#include "titlepagedialog.h"

#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "theme.h"

namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }

QLabel* sectionLabel(const QString& text) {
    auto* l = new QLabel(text.toUpper());
    l->setObjectName("secao");
    QFont f = l->font();
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
    l->setFont(f);
    return l;
}

QPlainTextEdit* multiLine(const QString& text, int lines, const QString& placeholder) {
    auto* e = new QPlainTextEdit(text);
    e->setPlaceholderText(placeholder);
    e->setTabChangesFocus(true);
    const int lh = e->fontMetrics().lineSpacing();
    e->setFixedHeight(lh * lines + 18);
    return e;
}
}  // namespace

// ------------------------------------------------------------ prévia

TitlePagePreview::TitlePagePreview(QWidget* parent) : QWidget(parent), painter_(ed_) {
    setMinimumSize(300, 388);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void TitlePagePreview::setDocument(const Document& doc) {
    Document d;
    d.titlePage = doc.titlePage;
    ed_.setDocument(d);
    update();
}

void TitlePagePreview::paintEvent(QPaintEvent*) {
    const ThemeColors& tc = ThemeManager::instance().colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(tc.canvas);
    p.drawRoundedRect(rect(), 12, 12);

    // Página centralizada, mantendo a proporção do papel Carta.
    const double margin = 26;
    double w = width() - 2 * margin, h = height() - 2 * margin;
    if (w * 11.0 / 8.5 > h) w = h * 8.5 / 11.0;
    else h = w * 11.0 / 8.5;
    const QRectF page((width() - w) / 2, (height() - h) / 2, w, h);
    for (int i = 8; i >= 1; --i) {
        p.setBrush(QColor(0, 0, 0, static_cast<int>((tc.dark ? 0.12 : 0.05) * (1.0 - i / 9.0) * 255)));
        p.drawRoundedRect(page.adjusted(-i, -i + 3, i, i + 3), 2 + i, 2 + i);
    }
    p.setBrush(Qt::white);
    p.drawRect(page);
    const double ppi = w / 8.5;
    if (painter_.hasTitlePage()) {
        painter_.paintTitlePage(p, page.topLeft(), ppi);
    } else {
        p.setPen(QColor(160, 164, 172));
        QFont f = font();
        f.setPointSizeF(font().pointSizeF() * 0.95);
        p.setFont(f);
        p.drawText(page.adjusted(30, 30, -30, -30), Qt::AlignCenter | Qt::TextWordWrap,
                   QObject::tr("Digite um título para criar a página de rosto."));
    }
}

// ------------------------------------------------------------ janela

TitlePageDialog::TitlePageDialog(const Document& doc, QWidget* parent) : QDialog(parent), base_(doc) {
    setWindowTitle(tr("Página de rosto"));
    setMinimumSize(860, 600);

    auto value = [&](const char* key) { return q(doc.titleValue(key)); };

    // ---- Cabeçalho
    auto* heading = new QLabel(tr("Página de rosto"));
    heading->setObjectName("tituloDialogo");
    auto* sub = new QLabel(tr("É a primeira folha do PDF e da impressão. Ela só aparece quando o roteiro tem um título."));
    sub->setObjectName("subtituloDialogo");
    sub->setWordWrap(true);

    // ---- Campos
    title_ = new QLineEdit(value("Title"));
    title_->setObjectName("campoTitulo");
    title_->setPlaceholderText(tr("Título do roteiro"));

    credit_ = new QComboBox;
    credit_->setEditable(true);
    credit_->addItems({tr("Escrito por"), tr("Roteiro de"), tr("Roteiro e argumento de"), tr("Argumento de"),
                       "Written by", "Screenplay by", "Story by"});
    credit_->setCurrentText(doc.titleValue("Credit").empty() ? tr("Escrito por") : value("Credit"));

    authors_ = multiLine(value("Author"), 2, tr("Um nome por linha"));
    source_ = new QLineEdit(value("Source"));
    source_->setPlaceholderText(tr("Ex.: Baseado no romance de…"));

    revision_ = new QLineEdit(value("Revision"));
    revision_->setPlaceholderText(tr("Ex.: 2º tratamento"));
    draftDate_ = new QLineEdit(value("Draft date"));
    draftDate_->setPlaceholderText(QDate::currentDate().toString("dd/MM/yyyy"));
    auto* today = new QToolButton;
    today->setText(tr("Hoje"));
    today->setToolTip(tr("Usar a data de hoje"));
    connect(today, &QToolButton::clicked, this,
            [this] { draftDate_->setText(QDate::currentDate().toString("dd/MM/yyyy")); });
    auto* dateRow = new QHBoxLayout;
    dateRow->setContentsMargins(0, 0, 0, 0);
    dateRow->addWidget(draftDate_, 1);
    dateRow->addWidget(today);

    contact_ = multiLine(value("Contact"), 3, tr("Nome\nTelefone\nE-mail"));
    notes_ = multiLine(value("Notes"), 2, tr("Ex.: Registro na Biblioteca Nacional nº …"));

    auto makeForm = [] {
        auto* f = new QFormLayout;
        f->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        f->setHorizontalSpacing(14);
        f->setVerticalSpacing(10);
        f->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        return f;
    };
    auto* f1 = makeForm();
    f1->addRow(tr("Título"), title_);
    f1->addRow(tr("Crédito"), credit_);
    f1->addRow(tr("Autor(es)"), authors_);
    f1->addRow(tr("Baseado em"), source_);
    auto* f2 = makeForm();
    f2->addRow(tr("Versão"), revision_);
    f2->addRow(tr("Data"), dateRow);
    f2->addRow(tr("Observações"), notes_);
    auto* f3 = makeForm();
    f3->addRow(tr("Contato"), contact_);

    auto* form = new QVBoxLayout;
    form->setSpacing(10);
    form->addWidget(sectionLabel(tr("Créditos")));
    form->addLayout(f1);
    form->addSpacing(8);
    form->addWidget(sectionLabel(tr("Versão")));
    form->addLayout(f2);
    form->addSpacing(8);
    form->addWidget(sectionLabel(tr("Contato")));
    form->addLayout(f3);
    form->addStretch(1);

    preview_ = new TitlePagePreview;

    auto* columns = new QHBoxLayout;
    columns->setSpacing(24);
    columns->addLayout(form, 11);
    columns->addWidget(preview_, 9);

    // ---- Botões
    auto* clear = new QPushButton(tr("Remover página de rosto"));
    clear->setObjectName("perigo");
    connect(clear, &QPushButton::clicked, this, [this] {
        for (QLineEdit* e : {title_, source_, revision_, draftDate_}) e->clear();
        for (QPlainTextEdit* e : {authors_, contact_, notes_}) e->clear();
        credit_->setCurrentText(tr("Escrito por"));
    });
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText(tr("Salvar"));
    bb->button(QDialogButtonBox::Save)->setDefault(true);
    bb->button(QDialogButtonBox::Save)->setObjectName("primarioBotao");
    bb->button(QDialogButtonBox::Save)->setIcon(QIcon());
    bb->button(QDialogButtonBox::Cancel)->setIcon(QIcon());
    bb->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(clear);
    buttons->addStretch(1);
    buttons->addWidget(bb);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(6);
    root->addWidget(heading);
    root->addWidget(sub);
    root->addSpacing(16);
    root->addLayout(columns, 1);
    root->addSpacing(14);
    root->addLayout(buttons);

    for (QLineEdit* e : {title_, source_, revision_, draftDate_})
        connect(e, &QLineEdit::textChanged, this, &TitlePageDialog::updatePreview);
    for (QPlainTextEdit* e : {authors_, contact_, notes_})
        connect(e, &QPlainTextEdit::textChanged, this, &TitlePageDialog::updatePreview);
    connect(credit_, &QComboBox::currentTextChanged, this, &TitlePageDialog::updatePreview);
    updatePreview();
    title_->setFocus();
}

std::vector<std::pair<std::string, std::string>> TitlePageDialog::values() const {
    auto clean = [](const QString& s) {
        QStringList lines;
        for (const QString& l : s.split('\n')) if (!l.trimmed().isEmpty()) lines << l.trimmed();
        return lines.join('\n').toStdString();
    };
    const bool hasTitle = !title_->text().trimmed().isEmpty();
    return {
        {"Title", clean(title_->text())},
        {"Credit", hasTitle ? clean(credit_->currentText()) : std::string()},
        {"Author", clean(authors_->toPlainText())},
        {"Source", clean(source_->text())},
        {"Revision", clean(revision_->text())},
        {"Draft date", clean(draftDate_->text())},
        {"Contact", clean(contact_->toPlainText())},
        {"Notes", clean(notes_->toPlainText())},
    };
}

void TitlePageDialog::updatePreview() {
    Document d;
    for (const auto& [k, v] : values()) d.setTitleValue(k, v);
    preview_->setDocument(d);
}
