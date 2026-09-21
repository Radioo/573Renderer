#include "editor_rows.h"

#include "editor_theme.h"

#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPolygon>
#include <QRect>
#include <QString>

namespace Editor::Rows {

namespace {

constexpr int kLeft = 8;
constexpr int kGap = 9;
constexpr int kNameSize = 12;
constexpr int kDetailSize = 11;
constexpr int kHeaderSize = 10;
constexpr int kChipSize = 13;
constexpr int kChipRound = 4;
constexpr int kMarkSide = 5;
constexpr int kMarkRight = 14;
constexpr int kStripeStep = 7;
constexpr int kStripeWidth = 2;
constexpr QColor kStripeBack(0x2f, 0x55, 0x7c);
constexpr QColor kStripeLine(0x5d, 0x8f, 0xc4);

}

QPixmap Stripes(int width, int height, int seed) {
    QPixmap drawn(width, height);
    drawn.fill(kStripeBack);
    QPainter painter(&drawn);
    const int lean = height / 2 + (seed % 3) * (height / 4);
    QColor line = kStripeLine;
    line.setAlpha(0x55);
    painter.setPen(QPen(line, kStripeWidth));
    for (int at = -height; at < width + height; at += kStripeStep)
        painter.drawLine(at, height, at + lean, 0);
    line.setAlpha(0x88);
    painter.setPen(QPen(line, 1));
    painter.drawRect(0, 0, width - 1, height - 1);
    return drawn;
}

Delegate::Delegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize Delegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const int tall = index.data(kHeaderRole).toBool() ? kHeaderHeight : kHeight;
    return {QStyledItemDelegate::sizeHint(option, index).width(), tall};
}

void Delegate::PaintHeader(QPainter* painter, const QRect& rect, const QString& text) const {
    painter->fillRect(rect, Theme::kPanel);
    QFont heading(Theme::SansFamily());
    heading.setPixelSize(kHeaderSize);
    heading.setBold(true);
    heading.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    painter->setFont(heading);
    painter->setPen(Theme::kFaint);
    painter->drawText(rect.adjusted(kLeft, 0, -kLeft, 0), Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(text.toUpper(), Qt::ElideRight,
                                                        rect.width() - (2 * kLeft)));
    painter->setPen(Theme::kLine);
    painter->drawLine(rect.left() + kLeft, rect.bottom(), rect.right() - kLeft, rect.bottom());
}

void Delegate::PaintChip(QPainter* painter, const QRect& box, const QString& text) const {
    painter->setPen(Qt::NoPen);
    painter->setBrush(Theme::kField);
    painter->drawRoundedRect(box, kChipRound, kChipRound);
    QFont figures(Theme::MonoFamily());
    figures.setPixelSize(kChipSize);
    painter->setFont(figures);
    painter->setPen(Theme::kText);
    painter->drawText(box, Qt::AlignCenter,
                      painter->fontMetrics().elidedText(text, Qt::ElideLeft, box.width() - kGap));
}

void Delegate::PaintMark(QPainter* painter, const QRect& rect, bool open) const {
    const int middle = rect.center().y();
    const int right = rect.right() - kMarkRight;
    QPolygon mark;
    if (open) {
        mark << QPoint(right - kMarkSide, middle - (kMarkSide / 2))
             << QPoint(right + kMarkSide, middle - (kMarkSide / 2))
             << QPoint(right, middle + kMarkSide);
    } else {
        mark << QPoint(right - (kMarkSide / 2), middle - kMarkSide)
             << QPoint(right - (kMarkSide / 2), middle + kMarkSide)
             << QPoint(right + kMarkSide, middle);
    }
    painter->setPen(Qt::NoPen);
    painter->setBrush(Theme::kFaint);
    painter->drawPolygon(mark);
}

void Delegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                     const QModelIndex& index) const {
    const bool chosen = (option.state & QStyle::State_Selected) != 0;
    const bool under = (option.state & QStyle::State_MouseOver) != 0;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (index.data(kHeaderRole).toBool()) {
        PaintHeader(painter, option.rect, index.data(Qt::DisplayRole).toString());
        painter->restore();
        return;
    }
    painter->fillRect(option.rect,
                      chosen ? Theme::Chosen() : (under ? Theme::kField : Theme::kPanel));
    QRect left = option.rect.adjusted(kLeft, 0, 0, 0);
    const QString chip = index.data(kChipRole).toString();
    const QPixmap thumbnail =
        qvariant_cast<QIcon>(index.data(Qt::DecorationRole)).pixmap(kThumbWidth, kThumbHeight);
    const bool slotted =
        index.data(kChipRole).isValid() || index.data(Qt::DecorationRole).isValid();
    if (slotted) {
        const int top = left.top() + ((left.height() - kThumbHeight) / 2);
        if (!chip.isEmpty()) {
            PaintChip(painter, QRect(left.left(), top, kThumbWidth, kThumbHeight), chip);
        } else if (!thumbnail.isNull()) {
            const int middle = left.top() + ((left.height() - thumbnail.height()) / 2);
            painter->drawPixmap(left.left() + ((kThumbWidth - thumbnail.width()) / 2), middle,
                                thumbnail);
        } else {
            painter->setPen(Qt::NoPen);
            painter->setBrush(Theme::kField);
            painter->drawRoundedRect(QRect(left.left(), top, kThumbWidth, kThumbHeight), kChipRound,
                                     kChipRound);
        }
        left.setLeft(left.left() + kThumbWidth + kGap);
    }
    const QVariant open = index.data(kOpenRole);
    if (open.isValid()) {
        PaintMark(painter, option.rect, open.toBool());
        left.setRight(left.right() - (2 * kMarkRight));
    }
    const QString detail = index.data(kDetailRole).toString();
    QFont named(Theme::SansFamily());
    named.setPixelSize(kNameSize);
    named.setBold(chosen);
    painter->setFont(named);
    painter->setPen(chosen ? Theme::kText : Theme::kSoft);
    const QString name = painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(),
                                                           Qt::ElideRight, left.width() - kLeft);
    if (detail.isEmpty()) {
        painter->drawText(left, Qt::AlignLeft | Qt::AlignVCenter, name);
        painter->restore();
        return;
    }
    QRect top = left;
    top.setHeight(left.height() / 2);
    painter->drawText(top, Qt::AlignLeft | Qt::AlignBottom, name);
    QFont said(Theme::SansFamily());
    said.setPixelSize(kDetailSize);
    painter->setFont(said);
    painter->setPen(chosen ? Theme::kSoft : Theme::kFaint);
    QRect under_name = left;
    under_name.setTop(top.bottom() + 1);
    painter->drawText(
        under_name, Qt::AlignLeft | Qt::AlignTop,
        painter->fontMetrics().elidedText(detail, Qt::ElideRight, under_name.width() - kLeft));
    painter->restore();
}

}
