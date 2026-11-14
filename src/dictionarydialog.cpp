#include "dictionarydialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

DictionaryDialog::DictionaryDialog(const std::vector<std::u32string>& words, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Script Dictionary"));
    setMinimumSize(420, 480);

    auto* heading = new QLabel(tr("Script Dictionary"));
    heading->setObjectName("tituloDialogo");
    auto* hint = new QLabel(tr("Words listed here are never marked as misspelled in this script: names, places, slang. "
                               "They are saved with the script."));
    hint->setObjectName("subtituloDialogo");
    hint->setWordWrap(true);

    input_ = new QLineEdit;
    input_->setPlaceholderText(tr("New word"));
    input_->setMinimumHeight(36);
    add_ = new QPushButton(tr("Add"));
    add_->setMinimumHeight(36);
    auto* addRow = new QHBoxLayout;
    addRow->setSpacing(8);
    addRow->addWidget(input_, 1);
    addRow->addWidget(add_);

    list_ = new QListWidget;
    list_->setObjectName("listaPalavras");
    list_->setSortingEnabled(true);
    list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    for (const std::u32string& w : words) list_->addItem(QString::fromStdU32String(w));

    remove_ = new QPushButton(tr("Remove"));
    auto* cancel = new QPushButton(tr("Cancel"));
    auto* done = new QPushButton(tr("Save"));
    done->setObjectName("primarioBotao");
    for (QPushButton* b : {remove_, cancel, done}) b->setMinimumSize(96, 36);
    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(8);
    bottom->addWidget(remove_);
    bottom->addStretch(1);
    bottom->addWidget(done);
    bottom->addWidget(cancel);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(12);
    root->addWidget(heading);
    root->addWidget(hint);
    root->addLayout(addRow);
    root->addWidget(list_, 1);
    root->addLayout(bottom);

    connect(add_, &QPushButton::clicked, this, &DictionaryDialog::addWord);
    connect(input_, &QLineEdit::returnPressed, this, &DictionaryDialog::addWord);
    connect(input_, &QLineEdit::textChanged, this, &DictionaryDialog::updateButtons);
    connect(list_, &QListWidget::itemSelectionChanged, this, &DictionaryDialog::updateButtons);
    connect(remove_, &QPushButton::clicked, this, [this] {
        qDeleteAll(list_->selectedItems());
        updateButtons();
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
    updateButtons();
    add_->setAutoDefault(false);
    done->setDefault(true);
}

void DictionaryDialog::addWord() {
    const QString w = input_->text().trimmed();
    if (w.isEmpty()) return;
    if (list_->findItems(w, Qt::MatchFixedString).isEmpty()) list_->addItem(w);  // sem repetir, ignorando maiúsculas
    input_->clear();
    input_->setFocus();
}

void DictionaryDialog::updateButtons() {
    add_->setEnabled(!input_->text().trimmed().isEmpty());
    remove_->setEnabled(!list_->selectedItems().isEmpty());
}

std::vector<std::u32string> DictionaryDialog::words() const {
    std::vector<std::u32string> out;
    for (int i = 0; i < list_->count(); ++i) out.push_back(list_->item(i)->text().toStdU32String());
    return out;
}
