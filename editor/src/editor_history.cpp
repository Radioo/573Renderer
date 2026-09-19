#include "editor_window.h"

#include "document/history.h"

#include <QBrush>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPalette>
#include <QSignalBlocker>
#include <QString>
#include <QTimer>

#include <cstddef>
#include <string>
#include <utility>

namespace Editor {

QListWidget* Window::BuildHistory() {
    history_list_ = new QListWidget;
    history_list_->setObjectName("history");
    connect(history_list_, &QListWidget::itemClicked, this, [this](const QListWidgetItem* item) {
        const int row = history_list_->row(item);
        QTimer::singleShot(0, this, [this, row] { JumpInHistory(row); });
    });
    return history_list_;
}

void Window::FillHistory() {
    const QSignalBlocker blocked(history_list_);
    history_list_->clear();
    history_list_->addItem(tr("Start"));
    for (const std::string& name : history_.Names())
        history_list_->addItem(QString::fromStdString(name));
    const auto position = static_cast<int>(history_.Position());
    const QBrush undone = history_list_->palette().brush(QPalette::Disabled, QPalette::Text);
    for (int row = position + 1; row < history_list_->count(); row++)
        history_list_->item(row)->setForeground(undone);
    history_list_->setCurrentRow(position);
}

void Window::JumpInHistory(int row) {
    if (!file_ || row < 0) return;
    const auto position = static_cast<std::size_t>(row);
    if (position == history_.Position()) return;
    auto restored =
        history_.Jump(position, Document::Snapshot{.file = *file_, .authored = authored_});
    if (!restored) return;
    file_ = std::move(restored->file);
    authored_ = std::move(restored->authored);
    SaveProject();
    ShowRestored();
}

}
