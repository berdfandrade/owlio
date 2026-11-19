#include "spellfixdialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>
#include <map>

namespace {
QString q(const std::u32string& s) { return QString::fromStdU32String(s); }
}  // namespace

SpellFixDialog::SpellFixDialog(const std::vector<Editor::TextFix>& fixes, const std::vector<std::u32string>& unfixable,
                               QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Correct Spelling"));
    setMinimumSize(460, 420);

    // Uma linha por troca, com quantas vezes ela aparece no roteiro.
    std::map<std::pair<std::u32string, std::u32string>, int> changes;
    for (const Editor::TextFix& f : fixes) ++changes[{f.expected, f.replacement}];

    auto* heading = new QLabel(tr("Correct %n word(s)?", "", static_cast<int>(fixes.size())));
    heading->setObjectName("tituloDialogo");
    auto* hint = new QLabel(tr("Each word gets the first suggestion of the dictionary. Check the list before "
                               "you confirm: names and slang may need the Script Dictionary instead. "
                               "One Ctrl+Z undoes all of it."));
    hint->setObjectName("subtituloDialogo");
    hint->setWordWrap(true);

    auto* list = new QListWidget;
    list->setObjectName("listaPalavras");
    list->setSelectionMode(QAbstractItemView::NoSelection);
    list->setFocusPolicy(Qt::NoFocus);
    for (const auto& [change, count] : changes) {
        QString row = q(change.first) + QStringLiteral("  →  ") + q(change.second);
        if (count > 1) row += QStringLiteral("   ×%1").arg(count);
        list->addItem(row);
    }

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(12);
    root->addWidget(heading);
    root->addWidget(hint);
    root->addWidget(list, 1);
    if (!unfixable.empty()) {
        QStringList words;
        for (const std::u32string& w : unfixable) words << q(w);
        auto* left = new QLabel(tr("No suggestion, left as they are: %1").arg(words.join(", ")));
        left->setObjectName("subtituloDialogo");
        left->setTextFormat(Qt::PlainText);  // palavras do roteiro, nunca HTML
        left->setWordWrap(true);
        root->addWidget(left);
    }

    auto* fix = new QPushButton(tr("Correct All"));
    fix->setObjectName("primarioBotao");
    auto* cancel = new QPushButton(tr("Cancel"));
    for (QPushButton* b : {fix, cancel}) b->setMinimumSize(110, 36);
    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(8);
    bottom->addStretch(1);
    bottom->addWidget(fix);
    bottom->addWidget(cancel);
    root->addLayout(bottom);

    connect(fix, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    fix->setDefault(true);
}
