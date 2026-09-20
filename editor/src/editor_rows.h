#pragma once

#include <QPixmap>
#include <QStyledItemDelegate>

namespace Editor::Rows {

inline constexpr int kDetailRole = Qt::UserRole + 20;
inline constexpr int kThumbWidth = 52;
inline constexpr int kThumbHeight = 29;
inline constexpr int kHeight = 40;

[[nodiscard]] QPixmap Stripes(int width, int height, int seed);

class Delegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit Delegate(QObject* parent = nullptr);

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};

}
