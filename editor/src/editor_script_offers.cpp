#include "editor_script_offers.h"

#include "editor_theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace Editor {

namespace {

constexpr int kOfferWidth = 366;
constexpr int kHelpWidth = 300;
constexpr int kHelpGap = 6;
constexpr int kRowHeight = 28;
constexpr int kFootHeight = 26;
constexpr int kBadgeSide = 15;
constexpr int kMostRows = 8;

QColor InkForKind(const QString& kind) {
    if (kind == QString("C")) return Theme::kViolet;
    if (kind == QString("L")) return Theme::kGreen;
    if (kind == QString("k")) return Theme::kViolet;
    return Theme::Accent();
}

QString NameInk(bool chosen) {
    return QString("background: transparent; color: %1; font-family: %2; font-size: 12px;")
        .arg(chosen ? Theme::kText.name() : Theme::kSoft.name(), Theme::MonoFamily());
}

QLabel* Badge(const QString& kind) {
    auto* badge = new QLabel(kind);
    badge->setFixedSize(kBadgeSide, kBadgeSide);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QString("background: %1; color: %2; font-size: 9px; font-weight: 700;")
                             .arg(InkForKind(kind).name(), Theme::kInk.name()));
    return badge;
}

QWidget* Row(const ScriptOffer& offer) {
    auto* row = new QWidget;
    row->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->setStyleSheet(QString("background: transparent;"));
    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(10, 0, 10, 0);
    line->setSpacing(9);

    auto* name = new QLabel(offer.name);
    name->setObjectName("script_offer_name");
    name->setStyleSheet(NameInk(false));

    auto* detail = new QLabel(offer.detail);
    detail->setStyleSheet(
        QString("background: transparent; color: %1; font-size: 10px;").arg(Theme::kFaint.name()));

    line->addWidget(Badge(offer.kind), 0);
    line->addWidget(name, 0);
    line->addStretch(1);
    line->addWidget(detail, 0);
    return row;
}

QFrame* Box(const QString& name) {
    auto* box = new QFrame;
    box->setObjectName(name);
    box->setStyleSheet(QString("QFrame#%1 { background: %2; border: 1px solid %3; }")
                           .arg(name, Theme::kPanel.name(), Theme::kEdge.name()));
    return box;
}

QLabel* Said(const QString& name, const QColor& ink, int size, bool mono) {
    auto* said = new QLabel;
    said->setObjectName(name);
    said->setWordWrap(true);
    said->setTextFormat(Qt::RichText);
    said->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    said->setStyleSheet(
        QString("background: transparent; color: %1; font-size: %2px; %3")
            .arg(ink.name())
            .arg(size)
            .arg(mono ? QString("font-family: %1;").arg(Theme::MonoFamily()) : QString()));
    return said;
}

QString Heading(const QString& word) {
    return QString("<div style=\"color:%1;font-size:9px;font-weight:700\">%2</div>")
        .arg(Theme::kFaint.name(), word);
}

}

ScriptOffers::ScriptOffers(QWidget* parent) : QWidget(parent) {
    setObjectName("script_offers");

    box_ = Box("script_offer_box");
    box_->setFixedWidth(kOfferWidth);

    rows_ = new QListWidget;
    rows_->setObjectName("script_offer_rows");
    rows_->setFocusPolicy(Qt::NoFocus);
    rows_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rows_->setFrameShape(QFrame::NoFrame);
    rows_->setStyleSheet(
        QString("QListWidget { background: %1; border: 0; } QListWidget::item:selected { "
                "background: %2; }")
            .arg(Theme::kPanel.name(), Theme::Chosen().name()));

    counted_ = new QLabel;
    counted_->setObjectName("script_offer_counted");
    counted_->setFixedHeight(kFootHeight);
    counted_->setContentsMargins(10, 0, 10, 0);
    counted_->setStyleSheet(
        QString("background: %1; color: %2; font-size: 10px; border: 0; border-top: 1px solid %3;")
            .arg(Theme::kCodeMargin.name(), Theme::kFaint.name(), Theme::kLine.name()));

    auto* stack = new QVBoxLayout(box_);
    stack->setContentsMargins(1, 1, 1, 1);
    stack->setSpacing(0);
    stack->addWidget(rows_, 1);
    stack->addWidget(counted_, 0);

    help_ = Box("script_offer_help");
    help_->setFixedWidth(kHelpWidth);
    named_ = Said("script_offer_named", Theme::kText, 12, true);
    said_ = Said("script_offer_said", Theme::kSoft, 11, false);
    takes_ = Said("script_offer_takes", Theme::kSoft, 11, false);
    gives_ = Said("script_offer_gives", Theme::kSoft, 11, false);
    seen_ = Said("script_offer_seen", Theme::kSoft, 11, true);
    note_ = Said("script_offer_note", Theme::kFaint, 10, true);

    helping_ = new QVBoxLayout(help_);
    helping_->setContentsMargins(11, 9, 11, 9);
    helping_->setSpacing(7);
    helping_->addWidget(named_, 0);
    helping_->addWidget(said_, 0);
    helping_->addWidget(takes_, 0);
    helping_->addWidget(gives_, 0);
    helping_->addWidget(seen_, 0);
    helping_->addWidget(note_, 0);
    helping_->addStretch(1);

    auto* beside = new QHBoxLayout(this);
    beside->setContentsMargins(0, 0, 0, 0);
    beside->setSpacing(kHelpGap);
    beside->addWidget(box_, 0, Qt::AlignTop);
    beside->addWidget(help_, 0, Qt::AlignTop);
    beside->addStretch(1);

    connect(rows_, &QListWidget::currentRowChanged, this, [this](int) { Explain(-1); });
    connect(rows_, &QListWidget::itemClicked, this, [this] { Take(); });
    hide();
}

void ScriptOffers::Offer(const QList<ScriptOffer>& offers, const QString& counted, QPoint at) {
    hinting_ = false;
    offers_ = offers;
    rows_->clear();
    for (const ScriptOffer& one : offers) {
        auto* row = new QListWidgetItem(rows_);
        row->setSizeHint(QSize(kOfferWidth - 2, kRowHeight));
        rows_->setItemWidget(row, Row(one));
    }
    if (rows_->count() > 0) rows_->setCurrentRow(0);
    counted_->setText(counted);
    box_->setVisible(true);
    Explain(-1);
    Fit();
    move(at);
    raise();
    show();
}

void ScriptOffers::Hint(const ScriptOffer& offer, int active, QPoint at) {
    hinting_ = true;
    offers_ = {offer};
    box_->setVisible(false);
    Explain(active);
    Fit();
    move(at);
    raise();
    show();
}

void ScriptOffers::Fit() {
    const int listed = std::min(static_cast<int>(offers_.size()), kMostRows);
    const int tall = hinting_ ? 0 : (listed * kRowHeight) + kFootHeight + 2;
    if (!hinting_) box_->setFixedHeight(tall);
    help_->setFixedHeight(help_->sizeHint().height());
    const int wide = hinting_ ? kHelpWidth : kOfferWidth + kHelpGap + kHelpWidth;
    setFixedSize(wide, std::max(help_->height(), tall));
}

void ScriptOffers::Put() {
    hide();
}

void ScriptOffers::Step(int by) {
    if (rows_->count() == 0) return;
    rows_->setCurrentRow(std::clamp(rows_->currentRow() + by, 0, rows_->count() - 1));
}

void ScriptOffers::Take() {
    const QString name = Chosen();
    if (name.isEmpty()) return;
    const bool call = ChosenIsCall();
    Put();
    Q_EMIT Took(name, call);
}

bool ScriptOffers::Offering() const {
    return !isHidden() && !hinting_;
}

bool ScriptOffers::Hinting() const {
    return !isHidden() && hinting_;
}

QString ScriptOffers::Chosen() const {
    const int at = rows_->currentRow();
    if (at < 0 || at >= offers_.size()) return {};
    return offers_.at(at).name;
}

bool ScriptOffers::ChosenIsCall() const {
    const int at = rows_->currentRow();
    if (at < 0 || at >= offers_.size()) return false;
    return offers_.at(at).call;
}

void ScriptOffers::Explain(int active) {
    const int at = hinting_ ? 0 : rows_->currentRow();
    for (int row = 0; !hinting_ && row < rows_->count(); row++) {
        QWidget* held = rows_->itemWidget(rows_->item(row));
        if (held == nullptr) continue;
        if (auto* name = held->findChild<QLabel*>("script_offer_name"))
            name->setStyleSheet(NameInk(row == at));
    }
    if (at < 0 || at >= offers_.size()) {
        help_->setVisible(false);
        return;
    }

    const ScriptOffer& one = offers_.at(at);
    named_->setText(one.signature.isEmpty() ? one.name : one.signature);
    said_->setText(one.said);
    said_->setVisible(!one.said.isEmpty());

    QString takes;
    for (int which = 0; which < one.params.size(); which++) {
        takes += QString("<div style=\"color:%1\">%2</div>")
                     .arg(which == active ? Theme::kText.name() : Theme::kSoft.name(),
                          one.params.at(which));
    }
    takes_->setText(takes.isEmpty() ? QString() : Heading(tr("TAKES")) + takes);
    takes_->setVisible(!takes.isEmpty());

    const QString gives = one.returns.isEmpty() ? QString()
                                                : Heading(tr("GIVES BACK")) +
                                                      QString("<div>%1</div>").arg(one.returns);
    gives_->setText(gives);
    gives_->setVisible(!gives.isEmpty());

    const QString seen = one.example.isEmpty() ? QString()
                                               : Heading(tr("SEEN IN THIS PACKAGE")) +
                                                     QString("<div>%1</div>").arg(one.example);
    seen_->setText(seen);
    seen_->setVisible(!seen.isEmpty());

    note_->setText(one.note);
    note_->setVisible(!one.note.isEmpty());
    help_->setVisible(true);
}

}
