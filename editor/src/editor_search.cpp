#include "editor_search.h"

#include <QEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPoint>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <functional>
#include <utility>

namespace Editor {

namespace {

constexpr int kWidth = 640;
constexpr int kHeight = 420;
constexpr int kFromTop = 60;

bool Matches(const SearchItem& item, const QStringList& words) {
    const QString haystack = item.category + ' ' + item.text;
    return std::ranges::all_of(words, [&haystack](const QString& word) {
        return haystack.contains(word, Qt::CaseInsensitive);
    });
}

}

CommandSearch::CommandSearch(QWidget* owner)
    : QFrame(owner, Qt::Popup), owner_(owner), query_(new QLineEdit), results_(new QTreeWidget) {
    setObjectName("command_search");
    setFrameShape(QFrame::StyledPanel);
    query_->setObjectName("search_query");
    query_->setPlaceholderText(tr("Type a command, a depth or an animation"));
    query_->installEventFilter(this);
    results_->setObjectName("search_results");
    results_->setColumnCount(3);
    results_->setHeaderHidden(true);
    results_->setRootIsDecorated(false);
    results_->setUniformRowHeights(false);
    results_->header()->setStretchLastSection(false);
    results_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    results_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    results_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    results_->setFocusPolicy(Qt::NoFocus);
    connect(query_, &QLineEdit::textChanged, this, &CommandSearch::Filter);
    connect(results_, &QTreeWidget::itemActivated, this, [this] { RunCurrent(); });
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(query_);
    layout->addWidget(results_);
    resize(kWidth, kHeight);
}

void CommandSearch::Open(std::vector<SearchItem> items) {
    items_ = std::move(items);
    query_->clear();
    Filter(QString());
    const QPoint corner = owner_->mapToGlobal(QPoint((owner_->width() - kWidth) / 2, kFromTop));
    move(corner);
    show();
    query_->setFocus();
}

void CommandSearch::Filter(const QString& query) {
    const QStringList words = query.split(' ', Qt::SkipEmptyParts);
    results_->clear();
    shown_.clear();
    for (std::size_t index = 0; index < items_.size(); index++) {
        const SearchItem& item = items_[index];
        if (!Matches(item, words)) continue;
        const QString text = item.detail.isEmpty() ? item.text : item.text + '\n' + item.detail;
        auto* row = new QTreeWidgetItem(results_, {item.category, text, item.keys});
        if (!item.available) row->setFlags(row->flags() & ~Qt::ItemIsEnabled);
        shown_.push_back(index);
    }
    results_->setCurrentItem(nullptr);
    Step(1);
}

void CommandSearch::Step(int by) {
    const int count = results_->topLevelItemCount();
    int at = results_->currentItem() != nullptr
                 ? results_->indexOfTopLevelItem(results_->currentItem())
                 : (by > 0 ? -1 : count);
    for (at += by; at >= 0 && at < count; at += by) {
        QTreeWidgetItem* row = results_->topLevelItem(at);
        if (!row->flags().testFlag(Qt::ItemIsEnabled)) continue;
        results_->setCurrentItem(row);
        return;
    }
}

void CommandSearch::RunCurrent() {
    QTreeWidgetItem* row = results_->currentItem();
    if (row == nullptr || !row->flags().testFlag(Qt::ItemIsEnabled)) return;
    const std::size_t index =
        shown_.at(static_cast<std::size_t>(results_->indexOfTopLevelItem(row)));
    const std::function<void()> run = items_.at(index).run;
    hide();
    run();
}

bool CommandSearch::eventFilter(QObject* watched, QEvent* event) {
    if (watched != query_ || event->type() != QEvent::KeyPress)
        return QFrame::eventFilter(watched, event);
    switch (static_cast<QKeyEvent*>(event)->key()) {
    case Qt::Key_Down:
        Step(1);
        return true;
    case Qt::Key_Up:
        Step(-1);
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        RunCurrent();
        return true;
    case Qt::Key_Escape:
        hide();
        return true;
    default:
        return QFrame::eventFilter(watched, event);
    }
}

}
