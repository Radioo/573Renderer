#pragma once

#include <QDialog>
#include <QString>

#include <vector>

class QTableWidget;

namespace Editor {

struct DriftRow {
    QString path;
    bool missing = false;
};

class DriftSheet : public QDialog {
    Q_OBJECT

public:
    DriftSheet(const std::vector<DriftRow>& rows, QWidget* parent);

    [[nodiscard]] std::vector<QString> Kept() const;

private:
    void ChooseAll(bool keep);

    QTableWidget* rows_;
};

}
