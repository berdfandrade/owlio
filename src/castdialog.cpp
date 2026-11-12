#include "castdialog.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

#include "avatars.h"
#include "pixelavatar.h"
#include "theme.h"

namespace {
enum Column { Name, Role, Age, Occupation, Speeches, Scenes, Words, ColumnCount };
constexpr int kNameRole = Qt::UserRole + 1;  // nome original, para saber quem renomear

QString q(const std::u32string& s) { return QString::fromStdU32String(s); }
std::u32string u(const QString& s) { return s.trimmed().toStdU32String(); }

QIcon avatarIcon(const QString& name, const std::string& photo) {
    const qreal dpr = 2.0;
    QPixmap pm(QSize(32, 32) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    avatars::draw(p, QRectF(0, 0, 32, 32), name, photo);
    return QIcon(pm);
}

// Papel: lista com os papéis comuns, mas aceita qualquer texto.
class RoleDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override {
        auto* box = new QComboBox(parent);
        box->setEditable(true);
        box->addItems({QString(), CastDialog::tr("Protagonist"), CastDialog::tr("Antagonist"),
                       CastDialog::tr("Supporting"), CastDialog::tr("Minor"), CastDialog::tr("Extra")});
        return box;
    }
    void setEditorData(QWidget* editor, const QModelIndex& idx) const override {
        static_cast<QComboBox*>(editor)->setCurrentText(idx.data().toString());
    }
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& idx) const override {
        model->setData(idx, static_cast<QComboBox*>(editor)->currentText().trimmed());
    }
};

// Números à direita e um pouco mais altos que o padrão, para caber a foto.
class RowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem& o, const QModelIndex& i) const override {
        QSize s = QStyledItemDelegate::sizeHint(o, i);
        s.setHeight(std::max(s.height(), 44));
        return s;
    }
};
}  // namespace

CastDialog::CastDialog(Editor* editor, QWidget* parent) : QDialog(parent), ed_(editor) {
    setWindowTitle(tr("Characters"));
    setMinimumSize(900, 560);
    ThemeManager& tm = ThemeManager::instance();

    auto* heading = new QLabel(tr("Characters"));
    heading->setObjectName("tituloDialogo");
    auto* sub = new QLabel(tr("Double-click a cell to edit it. Changes apply to the whole script: a new name "
                              "renames every cue, the sheet and the relationship map."));
    sub->setObjectName("subtituloDialogo");
    sub->setWordWrap(true);

    search_ = new QLineEdit;
    search_->setPlaceholderText(tr("Search characters…"));
    search_->setClearButtonEnabled(true);
    search_->addAction(tm.icon("localizar"), QLineEdit::LeadingPosition);
    search_->setFixedWidth(260);
    auto* add = new QPushButton(tm.icon("mais"), tr("New Character…"));
    connect(add, &QPushButton::clicked, this, &CastDialog::addCharacter);
    auto* top = new QHBoxLayout;
    top->addWidget(search_);
    top->addStretch(1);
    top->addWidget(add);

    table_ = new QTreeWidget;
    table_->setObjectName("tabelaElenco");
    table_->setColumnCount(ColumnCount);
    table_->setHeaderLabels({tr("Character"), tr("Role"), tr("Age"), tr("Occupation"), tr("Speeches"),
                             tr("Scenes"), tr("Words")});
    table_->setRootIsDecorated(false);
    table_->setUniformRowHeights(true);
    table_->setAlternatingRowColors(true);
    table_->setIconSize(QSize(32, 32));
    table_->setSortingEnabled(true);
    table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                            QAbstractItemView::SelectedClicked);
    table_->setItemDelegate(new RowDelegate(table_));
    table_->setItemDelegateForColumn(Role, new RoleDelegate(table_));
    QHeaderView* h = table_->header();
    h->setStretchLastSection(false);
    h->setSectionResizeMode(Name, QHeaderView::Stretch);
    h->setSectionResizeMode(Occupation, QHeaderView::Stretch);
    for (int c : {Role, Age, Speeches, Scenes, Words}) h->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    h->setMinimumSectionSize(70);

    summary_ = new QLabel;
    summary_->setObjectName("subtituloDialogo");

    sheet_ = new QPushButton(tm.icon("elenco"), tr("Open Sheet…"));
    avatar_ = new QPushButton(tm.icon("avatar"), tr("Create Avatar…"));
    photo_ = new QPushButton(tm.icon("enviar"), tr("Upload Photo…"));
    first_ = new QPushButton(tm.icon("roteiro"), tr("Go to First Line"));
    remove_ = new QPushButton(tr("Clear Sheet"));
    remove_->setObjectName("perigo");
    remove_->setToolTip(tr("Erase role, age, notes and photo. The cues in the script stay."));
    for (QPushButton* b : {add, sheet_, avatar_, photo_, first_, remove_}) {
        b->setMinimumHeight(36);
        b->setCursor(Qt::PointingHandCursor);
    }
    connect(sheet_, &QPushButton::clicked, this, [this] { if (!current().isEmpty()) emit openSheet(current()); });
    connect(avatar_, &QPushButton::clicked, this, &CastDialog::createAvatar);
    connect(photo_, &QPushButton::clicked, this, [this] { if (!current().isEmpty()) emit choosePhoto(current()); });
    connect(first_, &QPushButton::clicked, this, [this] {
        if (current().isEmpty()) return;
        const QString n = current();
        accept();
        emit gotoFirstLine(n);
    });
    connect(remove_, &QPushButton::clicked, this, &CastDialog::removeSheet);

    auto* close = new QPushButton(tr("Close"));
    close->setMinimumSize(110, 36);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    for (QPushButton* b : {sheet_, avatar_, photo_, first_}) actions->addWidget(b);
    actions->addWidget(remove_);
    actions->addStretch(1);
    actions->addWidget(close);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(12);
    root->addWidget(heading);
    root->addWidget(sub);
    root->addSpacing(4);
    root->addLayout(top);
    root->addWidget(table_, 1);
    root->addWidget(summary_);
    root->addLayout(actions);

    connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) {
        for (int i = 0; i < table_->topLevelItemCount(); ++i) {
            QTreeWidgetItem* it = table_->topLevelItem(i);
            bool match = text.isEmpty();
            for (int c : {Name, Role, Occupation}) match = match || it->text(c).contains(text, Qt::CaseInsensitive);
            it->setHidden(!match);
        }
    });
    connect(table_, &QTreeWidget::itemSelectionChanged, this, &CastDialog::updateButtons);
    connect(table_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* it, int column) {
        if (loading_) return;
        const QString name = it->data(Name, kNameRole).toString();
        // A tabela é recarregada depois (inclusive quando a edição é recusada).
        QMetaObject::invokeMethod(this, [this, name, column, value = it->text(column)] {
            editCell(name, column, value);
        }, Qt::QueuedConnection);
    });
    connect(table_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem*, int column) {
        if (column >= Speeches && !current().isEmpty()) emit openSheet(current());
    });

    reload();
    close->setDefault(true);
}

void CastDialog::reload() {
    const QString keep = current();
    loading_ = true;
    table_->setSortingEnabled(false);
    table_->clear();
    const Document& d = ed_->doc();
    std::vector<std::u32string> names;
    for (const auto& [n, count] : ed_->characters()) names.push_back(n);
    for (const CharacterInfo& c : d.cast)
        if (std::find(names.begin(), names.end(), c.name) == names.end()) names.push_back(c.name);

    int speeches = 0;
    for (const std::u32string& n : names) {
        const CharacterStats st = ed_->characterStats(n);
        const CharacterInfo* info = d.character(n);
        auto* it = new QTreeWidgetItem;
        it->setText(Name, q(n));
        it->setData(Name, kNameRole, q(n));
        it->setIcon(Name, avatarIcon(q(n), info ? info->photo : std::string()));
        if (info) {
            it->setText(Role, q(info->role));
            it->setText(Age, q(info->age));
            it->setText(Occupation, q(info->occupation));
        }
        it->setData(Speeches, Qt::DisplayRole, st.speeches);
        it->setData(Scenes, Qt::DisplayRole, static_cast<int>(st.scenes.size()));
        it->setData(Words, Qt::DisplayRole, st.words);
        for (int c : {Speeches, Scenes, Words}) it->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
        it->setFlags(it->flags() | Qt::ItemIsEditable);
        it->setToolTip(Name, info && !info->synopsis.empty() ? q(info->synopsis) : QString());
        table_->addTopLevelItem(it);
        speeches += st.speeches;
        if (q(n) == keep) table_->setCurrentItem(it);
    }
    table_->setSortingEnabled(true);
    table_->sortByColumn(Speeches, Qt::DescendingOrder);
    if (!table_->currentItem() && table_->topLevelItemCount() > 0) table_->setCurrentItem(table_->topLevelItem(0));
    summary_->setText(tr("%n character(s)", "", static_cast<int>(names.size())) + "  ·  " +
                      tr("%n speech(es)", "", speeches));
    loading_ = false;
    if (!search_->text().isEmpty()) emit search_->textChanged(search_->text());
    updateButtons();
}

QString CastDialog::current() const {
    QTreeWidgetItem* it = table_->currentItem();
    return it ? it->data(Name, kNameRole).toString() : QString();
}

void CastDialog::updateButtons() {
    const QString n = current();
    const bool any = !n.isEmpty();
    for (QPushButton* b : {sheet_, avatar_, photo_}) b->setEnabled(any);
    first_->setEnabled(any && ed_->characterStats(n.toStdU32String()).firstPara >= 0);
    remove_->setEnabled(any && ed_->doc().character(n.toStdU32String()) != nullptr);
}

bool CastDialog::editCell(const QString& name, int column, const QString& value) {
    const std::u32string old = name.toStdU32String();
    CharacterInfo info;
    if (const CharacterInfo* c = ed_->doc().character(old)) info = *c;
    info.name = old;
    switch (column) {
        case Name: {
            const QString newName = value.trimmed().toUpper();
            if (newName.isEmpty() || newName == name) {
                reload();
                return false;
            }
            bool exists = false;
            for (const auto& [n, count] : ed_->characters()) exists = exists || q(n) == newName;
            exists = exists || ed_->doc().character(newName.toStdU32String()) != nullptr;
            if (exists && isVisible() &&
                QMessageBox::question(this, tr("Rename Character"),
                                      tr("There is already a character called %1. Merge %2 into %1?")
                                          .arg(newName, name)) != QMessageBox::Yes) {
                reload();
                return false;
            }
            info.name = newName.toStdU32String();
            break;
        }
        case Role: info.role = u(value); break;
        case Age: info.age = u(value); break;
        case Occupation: info.occupation = u(value); break;
        default: reload(); return false;
    }
    const int renamed = ed_->setCharacter(old, info);
    emit changed();
    reload();
    if (column == Name) {
        for (int i = 0; i < table_->topLevelItemCount(); ++i)
            if (table_->topLevelItem(i)->text(Name) == q(info.name)) table_->setCurrentItem(table_->topLevelItem(i));
        summary_->setText(tr("%1 renamed to %2 in %n cue(s).", "", renamed).arg(name, q(info.name)));
    }
    return true;
}

void CastDialog::addCharacter() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Character"), tr("Name:"), QLineEdit::Normal, {}, &ok)
                             .trimmed()
                             .toUpper();
    if (!ok || name.isEmpty()) return;
    if (!ed_->doc().character(name.toStdU32String())) {
        CharacterInfo info;
        info.name = name.toStdU32String();
        info.role = tr("Supporting").toStdU32String();  // ficha não pode ficar vazia
        ed_->setCharacter(info.name, info);
        emit changed();
    }
    reload();
    for (int i = 0; i < table_->topLevelItemCount(); ++i)
        if (table_->topLevelItem(i)->text(Name) == name) table_->setCurrentItem(table_->topLevelItem(i));
    emit openSheet(name);
}

void CastDialog::createAvatar() {
    const QString n = current();
    if (n.isEmpty()) return;
    PixelAvatarDialog dlg(n, this);
    if (dlg.exec() != QDialog::Accepted) return;
    ed_->setAvatar(n.toStdU32String(), dlg.base64());
    emit changed();
    reload();
}

void CastDialog::removeSheet() {
    const QString n = current();
    if (n.isEmpty()) return;
    if (QMessageBox::question(this, tr("Clear Sheet"),
                              tr("Erase the sheet of %1 (role, age, notes and photo)? The cues stay in the script.")
                                  .arg(n)) != QMessageBox::Yes)
        return;
    CharacterInfo blank;
    blank.name = n.toStdU32String();
    ed_->setCharacter(blank.name, blank);
    emit changed();
    reload();
}
