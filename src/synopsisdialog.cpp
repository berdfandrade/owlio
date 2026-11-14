#include "synopsisdialog.h"

#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QVBoxLayout>

#include "sceneboard.h"

namespace {
QString q(const std::u32string& s) { return QString::fromStdU32String(s); }
}  // namespace

SynopsisDialog::SynopsisDialog(int sceneIndex, const SceneSummary& scene, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Scene %1 Synopsis").arg(sceneIndex + 1));
    setMinimumSize(520, 420);

    auto* eyebrow = new QLabel(tr("SCENE %1").arg(sceneIndex + 1));
    eyebrow->setObjectName("papelFicha");
    auto* heading = new QLabel(scene.heading.empty() ? tr("(scene without a heading)") : q(scene.heading));
    heading->setObjectName("tituloDialogo");
    heading->setWordWrap(true);

    QStringList meta{tr("Page %1").arg(scene.page + 1), eighthsText(scene.eighths) + " " + tr("pg.")};
    QStringList cast;
    for (const std::u32string& c : scene.cast) cast << q(c);
    if (!cast.isEmpty()) meta << cast.join(", ");
    auto* info = new QLabel(meta.join("  ·  "));
    info->setObjectName("subtituloDialogo");
    info->setWordWrap(true);

    text_ = new QPlainTextEdit(q(scene.synopsis));
    text_->setObjectName("textoSinopse");
    text_->setPlaceholderText(tr("What happens in this scene? A sentence or two is enough."));
    text_->setTabChangesFocus(true);
    auto* hint = new QLabel(tr("Shown on the index cards and the timeline, never on the printed page."));
    hint->setObjectName("subtituloDialogo");
    hint->setWordWrap(true);

    count_ = new QLabel;
    count_->setObjectName("subtituloDialogo");
    auto* save = new QPushButton(tr("Save"));
    save->setObjectName("primarioBotao");
    save->setToolTip(tr("Ctrl+Enter"));
    auto* cancel = new QPushButton(tr("Cancel"));
    for (QPushButton* b : {save, cancel}) b->setMinimumSize(96, 36);
    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(8);
    bottom->addWidget(count_);
    bottom->addStretch(1);
    bottom->addWidget(save);
    bottom->addWidget(cancel);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(6);
    root->addWidget(eyebrow);
    root->addWidget(heading);
    root->addWidget(info);
    root->addSpacing(10);
    root->addWidget(text_, 1);
    root->addWidget(hint);
    root->addSpacing(6);
    root->addLayout(bottom);

    connect(text_, &QPlainTextEdit::textChanged, this, &SynopsisDialog::updateCount);
    connect(save, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    for (const QKeySequence& k : {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)})
        connect(new QShortcut(k, this), &QShortcut::activated, this, &QDialog::accept);
    updateCount();

    text_->setFocus();
    text_->moveCursor(QTextCursor::End);
    save->setDefault(true);
}

QString SynopsisDialog::synopsis() const { return text_->toPlainText().trimmed(); }

void SynopsisDialog::updateCount() {
    const int words = text_->toPlainText().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
    count_->setText(words == 0 ? tr("Empty: the card shows the start of the action")
                               : tr("%n word(s)", nullptr, words));
}
