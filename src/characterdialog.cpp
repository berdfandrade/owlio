#include "characterdialog.h"
#include "pixelavatar.h"
#include "theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include "avatars.h"
#include "sceneheading.h"
#include "theme.h"

namespace {
QString q(const std::u32string& s) { return QString::fromStdU32String(s); }
std::u32string u(const QString& s) { return s.trimmed().toStdU32String(); }

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
    e->setMinimumHeight(e->fontMetrics().lineSpacing() * lines + 18);
    return e;
}

QLabel* statLabel(const QString& value, const QString& caption) {
    auto* l = new QLabel(QString("<div style='font-size:18px;font-weight:700'>%1</div>"
                                 "<div style='font-size:11px;color:%3'>%2</div>")
                             .arg(value.toHtmlEscaped(), caption.toHtmlEscaped(),
                                  ThemeManager::instance().colors().muted.name()));
    l->setAlignment(Qt::AlignCenter);
    l->setTextFormat(Qt::RichText);
    return l;
}
// Cena da ficha como um cartãozinho: número, local e, embaixo, horário (com cor) e INT/EXT.
enum SceneRole { kNumberRole = Qt::UserRole + 1, kPrefixRole, kTimeRole, kPageRole };

class SceneRowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex&) const override {
        return {200, opt.fontMetrics.height() * 2 + 26};
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        const ThemeColors& c = ThemeManager::instance().colors();
        p->save();
        p->setRenderHint(QPainter::Antialiasing);

        const QRectF card = QRectF(opt.rect).adjusted(0, 3, -2, -3);
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hover = opt.state & QStyle::State_MouseOver;
        QColor bg = c.dark ? c.surface : QColor(Qt::white);
        if (hover) bg = c.dark ? c.surface.lighter(118) : c.hover.lighter(103);
        p->setPen(selected ? QPen(c.accent, 1.5) : QPen(c.dark ? QColor(255, 255, 255, 14) : QColor(0, 0, 0, 16), 1));
        p->setBrush(bg);
        p->drawRoundedRect(card, 9, 9);

        const QString number = idx.data(kNumberRole).toString();
        const QString location = idx.data(Qt::DisplayRole).toString();
        const QString prefix = idx.data(kPrefixRole).toString();
        const QString time = idx.data(kTimeRole).toString();
        const QString page = idx.data(kPageRole).toString();

        QFont bold = opt.font;
        bold.setWeight(QFont::DemiBold);
        QFont small = opt.font;
        small.setPointSizeF(opt.font.pointSizeF() * 0.86);
        QFont smallBold = small;
        smallBold.setWeight(QFont::Bold);
        const QFontMetricsF bm(bold), sm(small), sbm(smallBold);

        // Número da cena numa pastilha com a cor de destaque.
        const double pad = 10;
        const double bw = std::max(26.0, sbm.horizontalAdvance(number) + 12);
        const QRectF badge(card.left() + pad, card.center().y() - 11, bw, 22);
        QColor tint = c.accent;
        tint.setAlphaF(c.dark ? 0.24 : 0.12);
        p->setPen(Qt::NoPen);
        p->setBrush(tint);
        p->drawRoundedRect(badge, 6, 6);
        p->setFont(smallBold);
        p->setPen(c.accent);
        p->drawText(badge, Qt::AlignCenter, number);

        const double x = badge.right() + 10;
        const double right = card.right() - pad;
        const double top = card.center().y() - (bm.height() + sm.height() + 2) / 2;

        p->setFont(bold);
        p->setPen(c.text);
        p->drawText(QRectF(x, top, right - x, bm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    bm.elidedText(location, Qt::ElideRight, right - x));

        // Segunda linha: ● HORÁRIO · INT. e a página à direita.
        const double y2 = top + bm.height() + 2;
        p->setFont(small);
        const double pw = sm.horizontalAdvance(page);
        p->setPen(c.muted);
        p->drawText(QRectF(right - pw, y2, pw, sm.height()), Qt::AlignRight | Qt::AlignVCenter, page);
        double cx = x;
        if (!time.isEmpty()) {
            p->setPen(Qt::NoPen);
            p->setBrush(timeColor(time));
            p->drawEllipse(QPointF(cx + 3.5, y2 + sm.height() / 2), 3.5, 3.5);
            cx += 12;
        }
        QString meta = time;
        if (!prefix.isEmpty()) meta += (meta.isEmpty() ? "" : QStringLiteral("  ·  ")) + prefix;
        p->setFont(small);
        p->setPen(c.muted);
        const double mw = right - pw - 10 - cx;
        p->drawText(QRectF(cx, y2, mw, sm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    sm.elidedText(meta, Qt::ElideRight, mw));
        p->restore();
    }
};

}  // namespace

// ------------------------------------------------------------ foto

AvatarButton::AvatarButton(QWidget* parent) : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
    setToolTip(tr("Click to choose a photo"));
}

void AvatarButton::set(const QString& name, const std::string& photo) {
    name_ = name;
    photo_ = photo;
    update();
}

void AvatarButton::paintEvent(QPaintEvent*) {
    const ThemeColors& tc = ThemeManager::instance().colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(4, 4, -4, -4);
    avatars::draw(p, r, name_.isEmpty() ? QStringLiteral("?") : name_, photo_, 3,
                  hover_ ? tc.accent : (tc.dark ? tc.raised : QColor(Qt::white)));
    if (hover_) {
        // Faixa escura com o convite para trocar a foto.
        QPainterPath clip;
        clip.addEllipse(r);
        p.setClipPath(clip);
        p.fillRect(QRectF(r.left(), r.bottom() - r.height() * 0.30, r.width(), r.height() * 0.30),
                   QColor(0, 0, 0, 140));
        p.setPen(Qt::white);
        QFont f = font();
        f.setPixelSize(12);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.drawText(QRectF(r.left(), r.bottom() - r.height() * 0.30, r.width(), r.height() * 0.24),
                   Qt::AlignCenter, photo_.empty() ? tr("Add photo") : tr("Change"));
    }
}

void AvatarButton::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint())) emit clicked();
}

// ------------------------------------------------------------ ficha

CharacterDialog::CharacterDialog(const Editor& ed, const QString& name, QWidget* parent)
    : QDialog(parent), ed_(ed), original_(name) {
    setWindowTitle(tr("Character — %1").arg(name));
    setMinimumSize(820, 600);

    CharacterInfo info;
    if (const CharacterInfo* c = ed.doc().character(name.toStdU32String())) info = *c;
    photo_ = info.photo;
    const CharacterStats st = ed.characterStats(name.toStdU32String());
    const auto summaries = ed.sceneSummaries();

    // ---- Coluna da esquerda: foto, nome, números e cenas.
    avatar_ = new AvatarButton;
    connect(avatar_, &AvatarButton::clicked, this, &CharacterDialog::choosePhoto);
    ThemeManager& tm = ThemeManager::instance();
    auto* choose = new QPushButton(tm.icon("enviar", tm.colors().accent), tr("Upload Photo…"));
    choose->setObjectName("link");
    connect(choose, &QPushButton::clicked, this, &CharacterDialog::choosePhoto);
    auto* create = new QPushButton(tm.icon("avatar", tm.colors().accent), tr("Create Avatar…"));
    create->setObjectName("link");
    connect(create, &QPushButton::clicked, this, [this] {
        const QString n = name_->text().trimmed().toUpper();
        PixelAvatarDialog dlg(n.isEmpty() ? original_ : n, this);
        if (dlg.exec() != QDialog::Accepted) return;
        photo_ = dlg.base64();
        updatePhoto();
    });
    removePhoto_ = new QPushButton(tr("Remove"));
    removePhoto_->setObjectName("perigo");
    connect(removePhoto_, &QPushButton::clicked, this, [this] {
        photo_.clear();
        updatePhoto();
    });
    auto* photoButtons = new QHBoxLayout;
    photoButtons->addStretch(1);
    photoButtons->setSpacing(12);
    photoButtons->addWidget(choose);
    photoButtons->addWidget(create);
    for (QPushButton* b : {choose, create, removePhoto_}) b->setCursor(Qt::PointingHandCursor);
    photoButtons->addStretch(1);

    nameLabel_ = new QLabel;
    nameLabel_->setObjectName("nomeFicha");
    nameLabel_->setAlignment(Qt::AlignCenter);
    nameLabel_->setWordWrap(true);
    roleLabel_ = new QLabel;
    roleLabel_->setObjectName("papelFicha");
    roleLabel_->setAlignment(Qt::AlignCenter);

    int firstScene = st.scenes.empty() ? -1 : st.scenes.front();
    auto* statsBox = new QFrame;
    statsBox->setObjectName("estatisticas");
    auto* stats = new QHBoxLayout(statsBox);
    stats->setContentsMargins(6, 10, 6, 10);
    stats->setSpacing(0);
    auto divider = [] {
        auto* d = new QFrame;
        d->setObjectName("divisor");
        d->setFixedWidth(1);
        return d;
    };
    stats->addWidget(statLabel(QString::number(st.speeches), tr("line(s)", "", st.speeches)), 1);
    stats->addWidget(divider());
    stats->addWidget(statLabel(QString::number(static_cast<int>(st.scenes.size())),
                               tr("scene(s)", "", static_cast<int>(st.scenes.size()))), 1);
    stats->addWidget(divider());
    stats->addWidget(statLabel(QString::number(st.words), tr("word(s)", "", st.words)), 1);

    auto* firstLabel = new QLabel;
    firstLabel->setObjectName("subtituloDialogo");
    firstLabel->setAlignment(Qt::AlignCenter);
    firstLabel->setWordWrap(true);
    if (firstScene >= 0 && firstScene < static_cast<int>(summaries.size()))
        firstLabel->setText(tr("First appears in scene %1, page %2")
                                .arg(firstScene + 1)
                                .arg(summaries[firstScene].page + 1));

    scenes_ = new QListWidget;
    scenes_->setObjectName("cenasFicha");
    scenes_->setFrameShape(QFrame::NoFrame);
    scenes_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scenes_->setItemDelegate(new SceneRowDelegate(scenes_));
    scenes_->setMouseTracking(true);
    scenes_->viewport()->setAttribute(Qt::WA_Hover);
    scenes_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    scenes_->setToolTip(tr("Double-click to go to the scene"));
    for (int s : st.scenes) {
        if (s >= static_cast<int>(summaries.size())) continue;
        const QString heading = q(summaries[s].heading);
        const HeadingParts h = splitHeading(heading);
        auto* it = new QListWidgetItem(h.location.isEmpty() ? (heading.isEmpty() ? tr("(untitled)") : heading)
                                                            : h.location);
        it->setData(Qt::UserRole, summaries[s].para);
        it->setData(kNumberRole, QString::number(s + 1));
        it->setData(kPrefixRole, h.prefix);
        it->setData(kTimeRole, h.time);
        it->setData(kPageRole, tr("p. %1").arg(summaries[s].page + 1));
        it->setToolTip(heading);
        scenes_->addItem(it);
    }
    connect(scenes_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        gotoPara_ = it->data(Qt::UserRole).toInt();
        accept();
    });

    auto* side = new QFrame;
    side->setObjectName("ficha");
    side->setFixedWidth(300);
    auto* sl = new QVBoxLayout(side);
    sl->setContentsMargins(18, 22, 18, 14);
    sl->setSpacing(8);
    sl->addWidget(avatar_, 0, Qt::AlignHCenter);
    sl->addLayout(photoButtons);
    sl->addWidget(removePhoto_, 0, Qt::AlignHCenter);  // linha própria: as três não cabem lado a lado
    sl->addWidget(nameLabel_);
    sl->addWidget(roleLabel_);
    sl->addSpacing(6);
    sl->addWidget(statsBox);
    sl->addWidget(firstLabel);
    sl->addSpacing(10);
    sl->addWidget(sectionLabel(tr("Scenes") + QStringLiteral("  ·  %1").arg(scenes_->count())));
    sl->addWidget(scenes_, 1);

    // ---- Coluna da direita: campos.
    auto* heading = new QLabel(tr("Character"));
    heading->setObjectName("tituloDialogo");
    auto* sub = new QLabel(tr("Saved inside the script file. Renaming updates every cue in the script."));
    sub->setObjectName("subtituloDialogo");
    sub->setWordWrap(true);

    name_ = new QLineEdit(name);
    name_->setObjectName("campoTitulo");
    name_->setPlaceholderText(tr("Name as it appears in the cues"));
    connect(name_, &QLineEdit::textChanged, this, &CharacterDialog::updatePhoto);

    role_ = new QComboBox;
    role_->setEditable(true);
    role_->addItems({QString(), tr("Protagonist"), tr("Antagonist"), tr("Supporting"), tr("Minor"), tr("Extra")});
    role_->setCurrentText(q(info.role));
    role_->lineEdit()->setPlaceholderText(tr("Protagonist, antagonist, supporting…"));
    connect(role_, &QComboBox::currentTextChanged, this, &CharacterDialog::updatePhoto);

    age_ = new QLineEdit(q(info.age));
    age_->setPlaceholderText(tr("E.g. 34, early 40s"));
    occupation_ = new QLineEdit(q(info.occupation));
    occupation_->setPlaceholderText(tr("E.g. Detective"));
    auto* ageRow = new QHBoxLayout;
    ageRow->setContentsMargins(0, 0, 0, 0);
    ageRow->setSpacing(14);
    ageRow->addWidget(age_, 2);
    auto* occLabel = new QLabel(tr("Occupation"));
    ageRow->addWidget(occLabel);
    ageRow->addWidget(occupation_, 3);

    traits_ = multiLine(q(info.traits), 3, tr("Appearance, personality, habits, the way they talk…"));
    synopsis_ = multiLine(q(info.synopsis), 6,
                          tr("Backstory, what they want, what stands in the way and how they change "
                             "over the story…"));

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->addRow(tr("Name"), name_);
    form->addRow(tr("Role"), role_);
    form->addRow(tr("Age"), ageRow);

    auto* fields = new QVBoxLayout;
    fields->setSpacing(8);
    fields->addWidget(heading);
    fields->addWidget(sub);
    fields->addSpacing(12);
    fields->addWidget(sectionLabel(tr("Identity")));
    fields->addLayout(form);
    fields->addSpacing(8);
    fields->addWidget(sectionLabel(tr("Characteristics")));
    fields->addWidget(traits_, 2);
    fields->addSpacing(8);
    fields->addWidget(sectionLabel(tr("Synopsis")));
    fields->addWidget(synopsis_, 4);

    auto* columns = new QHBoxLayout;
    columns->setSpacing(26);
    columns->addWidget(side);
    columns->addLayout(fields, 1);

    // ---- Botões
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText(tr("Save"));
    bb->button(QDialogButtonBox::Save)->setObjectName("primarioBotao");
    bb->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    for (QAbstractButton* b : bb->buttons()) b->setIcon(QIcon());
    connect(bb, &QDialogButtonBox::accepted, this, &CharacterDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 28, 22);
    root->setSpacing(16);
    root->addLayout(columns, 1);
    root->addWidget(bb);

    updatePhoto();
    synopsis_->setFocus();
    // Só depois de estar na janela: antes disso o QDialog escolheria outro botão padrão.
    bb->button(QDialogButtonBox::Save)->setDefault(true);
}

CharacterInfo CharacterDialog::info() const {
    CharacterInfo c;
    c.name = toUpper(u(name_->text()));
    if (c.name.empty()) c.name = original_.toStdU32String();
    c.photo = photo_;
    c.role = u(role_->currentText());
    c.age = u(age_->text());
    c.occupation = u(occupation_->text());
    c.traits = trim(traits_->toPlainText().toStdU32String());
    c.synopsis = trim(synopsis_->toPlainText().toStdU32String());
    return c;
}

void CharacterDialog::accept() {
    const QString newName = q(info().name);
    if (newName != original_) {
        bool exists = false;
        for (const auto& [n, count] : ed_.characters()) exists = exists || q(n) == newName;
        if (exists &&
            QMessageBox::question(this, tr("Rename Character"),
                                  tr("There is already a character called %1. Merge %2 into %1?\n\n"
                                     "Every cue of %2 will become %1, and this sheet replaces theirs.")
                                      .arg(newName, original_)) != QMessageBox::Yes) {
            gotoPara_ = -1;
            return;
        }
    }
    QDialog::accept();
}

void CharacterDialog::choosePhoto() {
    QSettings settings;
    const QString path = QFileDialog::getOpenFileName(this, tr("Photo of %1").arg(q(info().name)),
                                                      settings.value("lastImageDir").toString(),
                                                      avatars::fileFilter());
    if (path.isEmpty()) return;
    settings.setValue("lastImageDir", QFileInfo(path).absolutePath());
    QString error;
    const std::string data = avatars::fromImageFile(path, &error);
    if (data.empty()) {
        QMessageBox::warning(this, tr("Photo"), tr("Could not open the image:\n%1").arg(error));
        return;
    }
    photo_ = data;
    updatePhoto();
}

void CharacterDialog::updatePhoto() {
    const QString n = name_->text().trimmed().toUpper();
    avatar_->set(n.isEmpty() ? original_ : n, photo_);
    nameLabel_->setText(n.isEmpty() ? original_ : n);
    removePhoto_->setVisible(!photo_.empty());
    roleLabel_->setText(role_->currentText().trimmed());
    roleLabel_->setVisible(!roleLabel_->text().isEmpty());
}
