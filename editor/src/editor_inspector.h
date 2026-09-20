#pragma once

#include "editor_ease_editor.h"

#include "document/inspector_view.h"

#include <QLabel>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <cstdint>
#include <optional>
#include <vector>

class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QSpinBox;
class QTableWidget;
class QToolButton;
class QVBoxLayout;

namespace Editor {

class CharacterDrop : public QLabel {
    Q_OBJECT

public:
    explicit CharacterDrop(QWidget* parent = nullptr);

signals:
    void CharacterDropped(uint16_t character);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};

class ScrubLabel : public QWidget {
    Q_OBJECT

public:
    ScrubLabel(const QString& text, QWidget* parent = nullptr);

signals:
    void Scrubbed(int steps);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QString text_;
    std::optional<int> pressed_x_;
    int sent_ = 0;
};

struct PlacementExtras {
    QString character;
    int blend = 0;
    int clip_depth = 0;
    QStringList filters;
    bool owned = false;
};

struct InspectorSubject {
    QString title;
    QString detail;
    bool depth_chosen = false;
    bool owned = false;
};

class Inspector : public QWidget {
    Q_OBJECT

public:
    explicit Inspector(QWidget* parent = nullptr);

    [[nodiscard]] QTableWidget* Raw() const { return raw_; }

    void ShowSubject(const InspectorSubject& subject);
    void ShowView(const std::optional<Document::PlacementView>& view, uint32_t frame);
    void ShowEase(const std::optional<EaseView>& ease);
    void ShowExtras(const std::optional<PlacementExtras>& extras);

signals:
    void ValueEdited(const QString& label, const std::vector<double>& values);
    void KeyingToggled(const QString& label, bool animated);
    void ColourPicked(const QString& label);
    void EaseChosen(Document::Ease ease, const Document::Bezier& bezier);
    void CharacterReplaceAsked();
    void CharacterDropped(uint16_t character);
    void BlendChosen(int value);
    void ClipDepthEdited(int value);
    void FilterAdded(bool hsv);
    void FilterRemoved(const QString& name);

private:
    void AddRows(QVBoxLayout* into, const std::vector<Document::ViewRow>& rows, uint32_t frame);
    void AddRow(QVBoxLayout* into, const Document::ViewRow& row, uint32_t frame);
    [[nodiscard]] QWidget* NumberBoxes(const Document::ViewRow& row);
    [[nodiscard]] QWidget* ColourBoxes(const Document::ViewRow& row);

    QLabel* title_;
    QLabel* thumbnail_ = nullptr;
    QLabel* detail_;
    QLabel* badge_;
    QWidget* transform_;
    QWidget* appearance_;
    QWidget* keyframes_;
    EaseEditor* ease_;
    QWidget* content_;
    CharacterDrop* character_;
    QComboBox* blend_;
    QSpinBox* clip_depth_;
    int shown_clip_depth_ = 0;
    QVBoxLayout* filters_;
    QWidget* adding_;
    QVBoxLayout* transform_rows_;
    QVBoxLayout* appearance_rows_;
    QTableWidget* raw_;
    QToolButton* raw_heading_;
};

}
