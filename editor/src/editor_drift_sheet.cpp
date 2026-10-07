#include "editor_drift_sheet.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <vector>

namespace Editor {

DriftSheet::DriftSheet(const std::vector<DriftRow>& rows, QWidget* parent)
    : QDialog(parent), rows_(new QTableWidget(static_cast<int>(rows.size()), 2)) {
    setObjectName("drift_sheet");
    setWindowTitle(tr("Changed since the last export"));
    auto* said = new QLabel(
        tr("These entries changed in the IFS since the project last exported them. Tick the ones "
           "to keep as the IFS holds them, which drops the project's source for them. The rest "
           "are exported again the next time you export."));
    said->setWordWrap(true);

    rows_->setObjectName("drift_rows");
    rows_->setHorizontalHeaderLabels({tr("Entry"), tr("What changed")});
    rows_->verticalHeader()->setVisible(false);
    rows_->horizontalHeader()->setStretchLastSection(true);
    for (int at = 0; at < static_cast<int>(rows.size()); at++) {
        const DriftRow& row = rows[static_cast<std::size_t>(at)];
        auto* path = new QTableWidgetItem(row.path);
        path->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        path->setCheckState(Qt::Checked);
        auto* what = new QTableWidgetItem(row.missing ? tr("No longer in the IFS")
                                                      : tr("Changed in the IFS"));
        what->setFlags(Qt::ItemIsEnabled);
        rows_->setItem(at, 0, path);
        rows_->setItem(at, 1, what);
    }
    rows_->resizeColumnToContents(0);

    auto* all = new QHBoxLayout;
    auto* keep_all = new QPushButton(tr("Keep every IFS version"));
    keep_all->setObjectName("drift_keep_all");
    connect(keep_all, &QPushButton::clicked, this, [this] { ChooseAll(true); });
    auto* export_all = new QPushButton(tr("Export every one again"));
    export_all->setObjectName("drift_export_all");
    connect(export_all, &QPushButton::clicked, this, [this] { ChooseAll(false); });
    all->addWidget(keep_all);
    all->addWidget(export_all);
    all->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    buttons->setObjectName("drift_buttons");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(said);
    layout->addWidget(rows_, 1);
    layout->addLayout(all);
    layout->addWidget(buttons);
}

void DriftSheet::ChooseAll(bool keep) {
    for (int at = 0; at < rows_->rowCount(); at++)
        rows_->item(at, 0)->setCheckState(keep ? Qt::Checked : Qt::Unchecked);
}

std::vector<QString> DriftSheet::Kept() const {
    std::vector<QString> kept;
    for (int at = 0; at < rows_->rowCount(); at++) {
        if (rows_->item(at, 0)->checkState() == Qt::Checked)
            kept.push_back(rows_->item(at, 0)->text());
    }
    return kept;
}

}
