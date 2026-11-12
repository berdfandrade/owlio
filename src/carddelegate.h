#pragma once
#include <QStyledItemDelegate>

// Desenha os itens dos painéis laterais como "cartões".
//   Qt::DisplayRole  -> título
//   kSubtitleRole    -> linha secundária
//   kBadgeRole       -> número da cena / contagem de falas
class CardDelegate : public QStyledItemDelegate {
public:
    enum Kind { Scene, Character };
    static constexpr int kSubtitleRole = Qt::UserRole + 1;
    static constexpr int kBadgeRole = Qt::UserRole + 2;
    static constexpr int kAvatarRole = Qt::UserRole + 3;  // foto do personagem (base64, QByteArray)

    explicit CardDelegate(Kind kind, QObject* parent = nullptr) : QStyledItemDelegate(parent), kind_(kind) {}

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;
    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;

private:
    Kind kind_;
};
