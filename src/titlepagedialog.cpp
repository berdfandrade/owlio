#include "titlepagedialog.h"

#include <QComboBox>
#include <QDate>
#include <QLocale>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <algorithm>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "i18n.h"
#include "theme.h"

namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }

bool ptScript() { return i18n::scriptLanguage() == ScriptLanguage::Portuguese; }
QString defaultCredit() { return ptScript() ? QString::fromUtf8("Escrito por") : QString("Written by"); }
QString todayText() {
    const QDate d = QDate::currentDate();
    return ptScript() ? d.toString("dd/MM/yyyy") : QLocale(QLocale::English).toString(d, "MMMM d, yyyy");
}

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

    // Página centralizada, mantendo a proporção do papel.
    const double margin = 26, ratio = ed_.paper().height / ed_.paper().width;
    double w = width() - 2 * margin, h = height() - 2 * margin;
    if (w * ratio > h) w = h / ratio;
    else h = w * ratio;
    const QRectF page((width() - w) / 2, (height() - h) / 2, w, h);
    for (int i = 8; i >= 1; --i) {
        p.setBrush(QColor(0, 0, 0, static_cast<int>((tc.dark ? 0.12 : 0.05) * (1.0 - i / 9.0) * 255)));
        p.drawRoundedRect(page.adjusted(-i, -i + 3, i, i + 3), 2 + i, 2 + i);
    }
    p.setBrush(Qt::white);
    p.drawRect(page);
    const double ppi = w / ed_.paper().width;
    if (painter_.hasTitlePage()) {
        painter_.paintTitlePage(p, page.topLeft(), ppi);
    } else {
        p.setPen(QColor(160, 164, 172));
        QFont f = font();
        f.setPointSizeF(font().pointSizeF() * 0.95);
        p.setFont(f);
        p.drawText(page.adjusted(30, 30, -30, -30), Qt::AlignCenter | Qt::TextWordWrap,
                   tr("Type a title to create the title page."));
    }
}

// ------------------------------------------------------------ janela

TitlePageDialog::TitlePageDialog(const Document& doc, Paper paper, QWidget* parent) : QDialog(parent), base_(doc) {
    setWindowTitle(tr("Title Page"));
    setMinimumSize(860, 600);

    auto value = [&](const char* key) { return q(doc.titleValue(key)); };
    const bool pt = i18n::scriptLanguage() == ScriptLanguage::Portuguese;

    // ---- Cabeçalho
    auto* heading = new QLabel(tr("Title Page"));
    heading->setObjectName("tituloDialogo");
    auto* sub = new QLabel(tr("It is the first sheet of the PDF and of the printout. It only appears when the script has a title."));
    sub->setObjectName("subtituloDialogo");
    sub->setWordWrap(true);

    // ---- Campos
    title_ = new QLineEdit(value("Title"));
    title_->setObjectName("campoTitulo");
    title_->setPlaceholderText(tr("Script title"));

    credit_ = new QComboBox;
    credit_->setEditable(true);
    // Os créditos são texto do roteiro: seguem o idioma do roteiro, não o da interface.
    const QStringList enCredits = {"Written by", "Screenplay by", "Story by", "Screenplay and story by"};
    const QStringList ptCredits = {QString::fromUtf8("Escrito por"), "Roteiro de", "Argumento de",
                                   "Roteiro e argumento de"};
    credit_->addItems(pt ? ptCredits + enCredits : enCredits + ptCredits);
    credit_->setCurrentText(doc.titleValue("Credit").empty() ? defaultCredit() : value("Credit"));

    authors_ = multiLine(value("Author"), 2, tr("One name per line"));
    source_ = new QLineEdit(value("Source"));
    source_->setPlaceholderText(tr("E.g. Based on the novel by…"));

    revision_ = new QLineEdit(value("Revision"));
    revision_->setPlaceholderText(tr("E.g. Second draft"));
    draftDate_ = new QLineEdit(value("Draft date"));
    draftDate_->setPlaceholderText(todayText());
    auto* today = new QToolButton;
    today->setText(tr("Today"));
    today->setToolTip(tr("Use today's date"));
    connect(today, &QToolButton::clicked, this,
            [this] { draftDate_->setText(todayText()); });
    auto* dateRow = new QHBoxLayout;
    dateRow->setContentsMargins(0, 0, 0, 0);
    dateRow->addWidget(draftDate_, 1);
    dateRow->addWidget(today);

    contact_ = multiLine(value("Contact"), 3, tr("Name\nPhone\nEmail"));
    notes_ = multiLine(value("Notes"), 2, tr("E.g. WGA Registration No. …"));

    auto makeForm = [] {
        auto* f = new QFormLayout;
        f->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        f->setHorizontalSpacing(14);
        f->setVerticalSpacing(10);
        f->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        return f;
    };
    auto* f1 = makeForm();
    f1->addRow(tr("Title"), title_);
    f1->addRow(tr("Credit"), credit_);
    f1->addRow(tr("Author(s)"), authors_);
    f1->addRow(tr("Source"), source_);
    auto* f2 = makeForm();
    f2->addRow(tr("Revision"), revision_);
    f2->addRow(tr("Draft date"), dateRow);
    f2->addRow(tr("Notes"), notes_);
    auto* f3 = makeForm();
    f3->addRow(tr("Contact"), contact_);
    // Os três blocos com a mesma largura de rótulo, para os campos começarem na mesma linha.
    int labelW = 0;
    QList<QWidget*> labels;
    for (QFormLayout* f : {f1, f2, f3})
        for (int r = 0; r < f->rowCount(); ++r)
            if (QLayoutItem* it = f->itemAt(r, QFormLayout::LabelRole); it && it->widget()) {
                labels << it->widget();
                labelW = std::max(labelW, it->widget()->sizeHint().width());
            }
    for (QWidget* l : labels) l->setMinimumWidth(labelW);

    auto* form = new QVBoxLayout;
    form->setSpacing(10);
    form->addWidget(sectionLabel(tr("Credits")));
    form->addLayout(f1);
    form->addSpacing(8);
    form->addWidget(sectionLabel(tr("Revision")));
    form->addLayout(f2);
    form->addSpacing(8);
    form->addWidget(sectionLabel(tr("Contact")));
    form->addLayout(f3);
    form->addStretch(1);

    preview_ = new TitlePagePreview;
    preview_->setPaper(paper);

    auto* columns = new QHBoxLayout;
    columns->setSpacing(24);
    columns->addLayout(form, 11);
    columns->addWidget(preview_, 9);

    // ---- Botões
    auto* clear = new QPushButton(tr("Remove Title Page"));
    clear->setObjectName("perigo");
    connect(clear, &QPushButton::clicked, this, [this] {
        for (QLineEdit* e : {title_, source_, revision_, draftDate_}) e->clear();
        for (QPlainTextEdit* e : {authors_, contact_, notes_}) e->clear();
        credit_->setCurrentText(defaultCredit());
    });
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText(tr("Save"));
    bb->button(QDialogButtonBox::Save)->setObjectName("primarioBotao");
    bb->button(QDialogButtonBox::Save)->setIcon(QIcon());
    bb->button(QDialogButtonBox::Cancel)->setIcon(QIcon());
    bb->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
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
    // Só depois de estar na janela: antes disso o QDialog escolheria outro botão padrão.
    bb->button(QDialogButtonBox::Save)->setDefault(true);
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
