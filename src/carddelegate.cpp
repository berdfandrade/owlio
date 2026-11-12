#include "carddelegate.h"

#include <QPainter>
#include <QPainterPath>

#include "avatars.h"
#include "theme.h"

QSize CardDelegate::sizeHint(const QStyleOptionViewItem& opt, const QModelIndex&) const {
    const int h = opt.fontMetrics.height();
    return QSize(200, kind_ == Scene ? 2 * h + 18 : h + 20);
}

void CardDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    const ThemeColors& c = ThemeManager::instance().colors();
    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    const QRectF card = QRectF(opt.rect).adjusted(8, 1.5, -8, -1.5);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;
    if (selected || hover) {
        p->setPen(Qt::NoPen);
        p->setBrush(selected ? c.selected : c.hover);
        p->drawRoundedRect(card, 8, 8);
    }

    const QString title = idx.data(Qt::DisplayRole).toString();
    const QString subtitle = idx.data(kSubtitleRole).toString();
    const QString badge = idx.data(kBadgeRole).toString();

    QFont base = opt.font;
    QFont bold = base;
    bold.setWeight(QFont::DemiBold);
    QFont small = base;
    small.setPointSizeF(base.pointSizeF() * 0.88);

    if (kind_ == Scene) {
        // Selo com o número da cena.
        QFontMetricsF fm(small);
        const double bw = std::max(24.0, fm.horizontalAdvance(badge) + 12);
        QRectF b(card.left() + 8, card.top() + 9, bw, fm.height() + 4);
        QColor tint = c.accent;
        tint.setAlphaF(c.dark ? 0.22 : 0.12);
        p->setPen(Qt::NoPen);
        p->setBrush(tint);
        p->drawRoundedRect(b, 5, 5);
        p->setFont(small);
        p->setPen(c.accent);
        QFont sb = small;
        sb.setWeight(QFont::Bold);
        p->setFont(sb);
        p->drawText(b, Qt::AlignCenter, badge);

        const double x = b.right() + 10;
        const double w = card.right() - 10 - x;
        QFontMetricsF tm(bold);
        p->setFont(bold);
        p->setPen(c.text);
        QRectF tr(x, card.top() + 8, w, tm.height());
        p->drawText(tr, Qt::AlignLeft | Qt::AlignVCenter, tm.elidedText(title, Qt::ElideRight, w));
        p->setFont(small);
        p->setPen(c.muted);
        QFontMetricsF sm(small);
        QRectF sr(x, tr.bottom() + 2, w, sm.height());
        p->drawText(sr, Qt::AlignLeft | Qt::AlignVCenter, sm.elidedText(subtitle, Qt::ElideRight, w));
    } else {
        // Avatar: foto do personagem ou iniciais.
        const double d = card.height() - 12;
        QRectF av(card.left() + 8, card.top() + 6, d, d);
        avatars::draw(*p, av, title, idx.data(kAvatarRole).toByteArray().toStdString());

        QFontMetricsF sm(small);
        const double bw = sm.horizontalAdvance(badge);
        const double x = av.right() + 10;
        const double w = card.right() - 12 - bw - 8 - x;
        QFontMetricsF tm(bold);
        p->setFont(bold);
        p->setPen(c.text);
        p->drawText(QRectF(x, card.top(), w, card.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    tm.elidedText(title, Qt::ElideRight, w));
        p->setFont(small);
        p->setPen(c.muted);
        p->drawText(QRectF(card.right() - 12 - bw, card.top(), bw, card.height()),
                    Qt::AlignRight | Qt::AlignVCenter, badge);
    }
    p->restore();
}
