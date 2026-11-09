#include "editor_window.h"

#include "editor_filter.h"
#include "editor_open.h"
#include "editor_rows.h"
#include "editor_theme.h"

#include "document/clip.h"
#include "document/document.h"
#include "document/inputs.h"
#include "document/number_places.h"
#include "document/stage_bounds.h"
#include "formats/afp_animation.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QCompleter>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Editor {

namespace {

constexpr int kClipRole = Qt::UserRole + 40;
constexpr int kDepthRole = Qt::UserRole + 41;
constexpr int kFrameRole = Qt::UserRole + 42;
constexpr int kNumberRole = Qt::UserRole + 43;
constexpr uint32_t kDigits = 10;
constexpr int kPlaceIndent = 16;
constexpr int kUsualPlaces = 4;
constexpr int kWidestStep = 4000;
constexpr int kValuePad = 10;
constexpr int kValueGap = 5;
constexpr int kCaptionSize = 10;
constexpr int kLeastField = 40;

QLabel* Caption(const QString& text) {
    auto* made = new QLabel(text);
    made->setProperty("caption", true);
    QFont lettering(Theme::SansFamily());
    lettering.setPixelSize(kCaptionSize);
    lettering.setBold(true);
    made->setFont(lettering);
    return made;
}

void Wrapping(QLabel* one) {
    one->setWordWrap(true);
    QSizePolicy fits(QSizePolicy::Ignored, QSizePolicy::Minimum);
    fits.setHeightForWidth(true);
    one->setSizePolicy(fits);
}

void Shrinkable(QWidget* one) {
    one->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    one->setMinimumWidth(kLeastField);
    if (auto* box = qobject_cast<QComboBox*>(one)) {
        box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        box->setMinimumContentsLength(1);
    }
}

QString Family(const Document::InputNumber& number, const Document::InputSlot& slot) {
    return QObject::tr("%1 to %2")
        .arg(QString::fromStdString(Document::Digit(slot.texture, number.digit_at, 0)))
        .arg(QString::fromStdString(Document::Digit(slot.texture, number.digit_at, kDigits - 1)));
}

QString Detail(const Document::InputSlot& slot, const std::map<uint16_t, QString>& names,
               const Document::InputNumber* number) {
    QStringList said;
    const auto named = slot.character ? names.find(*slot.character) : names.end();
    if (named != names.end()) said.append(named->second);
    if (slot.driven == Document::InputDriven::Frames) {
        said.append(QObject::tr("%1 frames").arg(slot.frames));
    } else if (number != nullptr) {
        said.append(QObject::tr("one of %1").arg(Family(*number, slot)));
    } else {
        said.append(QObject::tr("one picture"));
    }
    if (slot.places > 1) said.append(QObject::tr("%1 depths").arg(slot.places));
    return said.join(", ");
}

QString Counted(int inputs) {
    if (inputs == 0) return QObject::tr("No inputs");
    if (inputs == 1) return QObject::tr("Filter 1 input");
    return QObject::tr("Filter %1 inputs").arg(inputs);
}

std::vector<std::string> ImageNames(const std::vector<ImageRow>& images) {
    std::vector<std::string> named;
    named.reserve(images.size());
    for (const ImageRow& one : images)
        named.push_back(one.name.toStdString());
    return named;
}

}

QWidget* Window::BuildInputsPanel() {
    inputs_ = new QTreeWidget;
    inputs_->setObjectName("inputs");
    inputs_->setHeaderHidden(true);
    inputs_->setRootIsDecorated(false);
    inputs_->setIndentation(kPlaceIndent);
    inputs_->setUniformRowHeights(false);
    inputs_->setColumnCount(1);
    inputs_->setMouseTracking(true);
    inputs_->setItemDelegate(new Rows::Delegate(inputs_));
    connect(inputs_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        if (item->childCount() > 0) {
            item->setExpanded(!item->isExpanded());
            item->setData(0, Rows::kOpenRole, item->isExpanded());
        }
        const QVariant depth = item->data(0, kDepthRole);
        if (!depth.isValid()) return;
        const int clip = item->data(0, kClipRole).toInt();
        const uint32_t frame = item->data(0, kFrameRole).toUInt();
        QTimer::singleShot(0, this, [this, clip, depth, frame] {
            ShowInputAt(clip, static_cast<uint16_t>(depth.toUInt()), frame);
        });
    });
    QWidget* around = WithFilter(inputs_, inputs_filter_ = new QLineEdit, tr("Filter inputs"));
    inputs_filter_->setObjectName("inputs_filter");
    connect(inputs_, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* item) { ShowInputValue(item); });

    auto* holding = new QWidget;
    auto* down = new QVBoxLayout(holding);
    down->setContentsMargins(0, 0, 0, 0);
    down->setSpacing(0);
    down->addWidget(around, 1);
    down->addWidget(BuildInputValue());
    return holding;
}

QWidget* Window::BuildInputValue() {
    input_value_ = new QWidget;
    input_value_->setObjectName("input_value");
    auto* down = new QVBoxLayout(input_value_);
    down->setContentsMargins(kValuePad, kValuePad, kValuePad, kValuePad);
    down->setSpacing(kValueGap);

    input_chosen_ = new QLabel;
    input_chosen_->setObjectName("input_chosen");
    Wrapping(input_chosen_);
    down->addWidget(input_chosen_);
    input_hint_ = new QLabel;
    input_hint_->setObjectName("input_hint");
    Wrapping(input_hint_);
    down->addWidget(input_hint_);

    input_number_row_ = Caption(tr("Number"));
    down->addWidget(input_number_row_);
    input_number_ = new QLineEdit;
    input_number_->setObjectName("input_number");
    input_number_->setPlaceholderText(tr("a number, such as 123"));
    Shrinkable(input_number_);
    connect(input_number_, &QLineEdit::returnPressed, this, [this] { SetInputNumber(); });
    down->addWidget(input_number_);
    input_blank_ = new QCheckBox(tr("Blank leading zeros"));
    input_blank_->setObjectName("input_blank");
    connect(input_blank_, &QCheckBox::toggled, this, [this] { SetInputNumber(); });
    Shrinkable(input_blank_);
    down->addWidget(input_blank_);

    input_texture_row_ = Caption(tr("Picture"));
    down->addWidget(input_texture_row_);
    input_texture_ = new QComboBox;
    input_texture_->setObjectName("input_texture");
    input_texture_->setEditable(true);
    input_texture_->setInsertPolicy(QComboBox::NoInsert);
    input_texture_->completer()->setCompletionMode(QCompleter::PopupCompletion);
    Shrinkable(input_texture_);
    connect(input_texture_, &QComboBox::activated, this, [this] { SetInputTexture(); });
    down->addWidget(input_texture_);

    down->addWidget(BuildInputSpread());

    auto* clear = new QPushButton(tr("Put it back"));
    clear->setObjectName("input_clear");
    connect(clear, &QPushButton::clicked, this, [this] { ClearInputValue(); });
    Shrinkable(clear);
    down->addWidget(clear);
    input_value_->setVisible(false);
    return input_value_;
}

QWidget* Window::BuildInputSpread() {
    auto* spreading = new QWidget;
    auto* stacked = new QVBoxLayout(spreading);
    stacked->setContentsMargins(0, 0, 0, 0);
    stacked->setSpacing(kValueGap);
    stacked->addWidget(Caption(tr("Make it a number")));

    auto* pair = new QWidget;
    auto* across = new QHBoxLayout(pair);
    across->setContentsMargins(0, 0, 0, 0);
    across->setSpacing(kValueGap);
    input_spread_ = new QSpinBox;
    input_spread_->setObjectName("input_spread");
    input_spread_->setRange(2, static_cast<int>(Document::kMostNumberPlaces));
    input_spread_->setValue(kUsualPlaces);
    input_spread_->setSuffix(tr(" digits"));
    Shrinkable(input_spread_);
    across->addWidget(input_spread_, 1);
    input_step_ = new QSpinBox;
    input_step_->setObjectName("input_step");
    input_step_->setRange(1, kWidestStep);
    input_step_->setSuffix(tr(" px"));
    Shrinkable(input_step_);
    across->addWidget(input_step_, 1);
    stacked->addWidget(pair);

    input_grows_ = new QComboBox;
    input_grows_->setObjectName("input_grows");
    input_grows_->addItem(tr("growing right"));
    input_grows_->addItem(tr("growing left"));
    Shrinkable(input_grows_);
    stacked->addWidget(input_grows_);

    auto* spread = new QPushButton(tr("Make it a number"));
    spread->setObjectName("input_spread_go");
    connect(spread, &QPushButton::clicked, this, [this] { SpreadChosenInput(); });
    Shrinkable(spread);
    stacked->addWidget(spread);
    input_spread_row_ = spreading;
    return spreading;
}

void Window::SpreadChosenInput() {
    if (input_chosen_number_ < 0 || ChosenNumber() != nullptr) return;
    const Document::InputSlot* slot = InputNamed(input_chosen_name_);
    if (slot == nullptr) return;
    const double advance = InputAdvance(*slot);
    if (advance <= 0) {
        ShowRefusal(
            tr("%1 draws nothing wide enough to lay digits out from").arg(input_chosen_name_));
        return;
    }
    const auto places = static_cast<uint32_t>(input_spread_->value());
    const Document::NumberSpread spread{.clip = slot->clip,
                                        .name = input_chosen_name_.toStdString(),
                                        .depth = slot->depth,
                                        .frame = slot->frame,
                                        .places = places,
                                        .advance = static_cast<double>(input_step_->value()),
                                        .grows = input_grows_->currentIndex() == 0
                                                     ? Document::NumberGrows::Right
                                                     : Document::NumberGrows::Left};
    if (!EditAnimation(tr("Make %1 a number of %2 digits").arg(input_chosen_name_).arg(places),
                       [spread](AfpAnimation::Animation& edited) {
                           return Document::SpreadIntoPlaces(edited, spread);
                       })) {
        return;
    }
    const QString stem = input_chosen_name_;
    ShowFrame();
    QTimer::singleShot(0, this, [this, stem] { SelectInputNamed(stem); });
}

void Window::SelectInputNamed(const QString& name) {
    for (QTreeWidgetItemIterator row(inputs_); *row != nullptr; ++row) {
        if ((*row)->text(0) != name) continue;
        inputs_->setCurrentItem(*row);
        inputs_->scrollToItem(*row);
        return;
    }
}

double Window::InputAdvance(const Document::InputSlot& slot) const {
    if (!file_ || animation_path_.empty()) return 0;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) return 0;
    const std::vector<Document::StageOutline> outlines = Document::StageOutlines(
        *animation, slot.clip, slot.frame, file_->ShapeBounds(animation_path_));
    for (const Document::StageOutline& one : outlines) {
        if (one.depth != slot.depth) continue;
        return std::abs(one.corners[1][0] - one.corners[0][0]);
    }
    return 0;
}

void Window::FillInputs(const Document::InputSurface& surface,
                        const std::map<uint16_t, QString>& names) {
    std::vector<std::string> images = ImageNames(rows_.images);
    if (inputs_->topLevelItemCount() > 0 && surface == shown_inputs_ && images == input_images_)
        return;
    shown_inputs_ = surface;
    input_images_ = std::move(images);
    input_numbers_ = Document::Numbers(surface, input_images_);
    inputs_->clear();
    input_value_->setVisible(false);
    int shown_group = -1;
    const auto group = [this, &shown_group](Document::ClipId clip) {
        const int index = ClipIndexOf(clip);
        if (index == shown_group) return;
        shown_group = index;
        const QString named = ClipName(index);
        AddInputHeader(named.isEmpty() ? tr("Sprite %1").arg(clip.sprite.value_or(0)) : named);
    };

    std::map<std::size_t, int> owners;
    for (std::size_t at = 0; at < input_numbers_.size(); at++) {
        for (const std::size_t place : input_numbers_[at].places)
            owners.emplace(place, static_cast<int>(at));
    }
    std::set<int> built;
    for (std::size_t at = 0; at < surface.names.size(); at++) {
        const Document::InputSlot& slot = surface.names[at];
        const auto owned = owners.find(at);
        const int number = owned == owners.end() ? -1 : owned->second;
        const Document::InputNumber* owning = number < 0 ? nullptr : &input_numbers_[number];
        group(slot.clip);
        if (owning == nullptr || owning->places.size() < 2) {
            auto* row = new QTreeWidgetItem(inputs_);
            MarkInput(row, slot, Detail(slot, names, owning), number);
            row->setIcon(0, InputPicture(InputTextureOf(slot)));
            continue;
        }
        if (!built.insert(number).second) continue;
        auto* holder = new QTreeWidgetItem(inputs_);
        holder->setText(0, QString::fromStdString(owning->stem));
        MarkInput(holder, slot, tr("a number of %1 digits").arg(owning->places.size()), number);
        holder->setData(0, Rows::kChipRole, InputNumberShown(*owning));
        holder->setData(0, Rows::kOpenRole, false);
        for (const std::size_t place : owning->places) {
            const Document::InputSlot& digit = surface.names[place];
            auto* under = new QTreeWidgetItem(holder);
            MarkInput(under, digit, Detail(digit, names, owning), number);
            under->setIcon(0, InputPicture(InputTextureOf(digit)));
        }
    }
    FillInputLabels(surface);
    ShowInputCount(surface);
}

void Window::AddInputHeader(const QString& text) {
    auto* made = new QTreeWidgetItem(inputs_);
    made->setText(0, text);
    made->setData(0, Rows::kHeaderRole, true);
    made->setFlags(Qt::ItemIsEnabled);
}

QIcon Window::InputPicture(const QString& texture) const {
    if (texture.isEmpty()) return {};
    for (const ImageRow& one : rows_.images) {
        if (one.name != texture) continue;
        if (one.picture.isNull()) return {};
        return QIcon(QPixmap::fromImage(one.picture.scaled(
            Rows::kThumbWidth, Rows::kThumbHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }
    return {};
}

void Window::MarkInput(QTreeWidgetItem* row, const Document::InputSlot& slot, const QString& detail,
                       int number) {
    if (row->text(0).isEmpty()) row->setText(0, QString::fromStdString(slot.name));
    row->setData(0, Rows::kDetailRole, detail);
    row->setData(0, kClipRole, ClipIndexOf(slot.clip));
    row->setData(0, kDepthRole, slot.depth);
    row->setData(0, kFrameRole, slot.frame);
    if (number >= 0) row->setData(0, kNumberRole, number);
}

void Window::FillInputLabels(const Document::InputSurface& surface) {
    if (surface.labels.empty()) return;
    AddInputHeader(tr("Frame labels"));
    for (const Document::InputLabel& label : surface.labels) {
        auto* row = new QTreeWidgetItem(inputs_);
        row->setText(0, QString::fromStdString(label.name));
        const int index = ClipIndexOf(label.clip);
        const QString named = ClipName(index);
        const QString where = named.isEmpty() ? tr("a sprite") : named;
        row->setData(0, Rows::kDetailRole, tr("frame %1 of %2").arg(label.frame).arg(where));
        row->setData(0, kClipRole, index);
        row->setData(0, kFrameRole, label.frame);
    }
}

void Window::ShowInputCount(const Document::InputSurface& surface) {
    const auto named = static_cast<int>(surface.names.size());
    const auto labelled = static_cast<int>(surface.labels.size());
    inputs_filter_->setPlaceholderText(Counted(named + labelled));
}

void Window::ShowInputValue(QTreeWidgetItem* item) {
    input_chosen_name_.clear();
    input_chosen_number_ = -1;
    if (item == nullptr || !item->data(0, kDepthRole).isValid()) {
        input_value_->setVisible(false);
        return;
    }
    input_chosen_name_ = item->text(0);
    const QVariant number = item->data(0, kNumberRole);
    if (number.isValid()) input_chosen_number_ = number.toInt();
    const Document::InputSlot* slot = InputNamed(input_chosen_name_);
    if (slot == nullptr && input_chosen_number_ < 0) {
        input_value_->setVisible(false);
        return;
    }
    input_value_->setVisible(true);
    input_chosen_->setText(input_chosen_name_);
    ShowInputNumber();
    ShowInputTexture(slot);
    const bool spreadable = ChosenNumber() == nullptr && input_chosen_number_ >= 0;
    input_spread_row_->setVisible(spreadable);
    input_blank_->setVisible(ChosenNumber() != nullptr);
    if (spreadable && slot != nullptr) {
        const auto measured = static_cast<int>(std::lround(InputAdvance(*slot)));
        input_step_->setValue(std::max(1, measured));
    }
    input_hint_->setText(InputHint(slot));
}

QString Window::InputHint(const Document::InputSlot* slot) const {
    if (ChosenNumber() != nullptr) return tr("Type a number and press Enter.");
    if (input_chosen_number_ < 0) return tr("Pick the picture this clip draws.");
    const Document::InputNumber& one = input_numbers_[input_chosen_number_];
    return tr("This name has one place in the file, so it draws one of the %1 pictures "
              "%2. A number of several digits is several named places.")
        .arg(kDigits)
        .arg(Family(one, *slot));
}

const Document::InputNumber* Window::ChosenNumber() const {
    if (input_chosen_number_ < 0) return nullptr;
    const Document::InputNumber& one = input_numbers_[input_chosen_number_];
    return one.places.size() > 1 ? &one : nullptr;
}

void Window::ShowInputNumber() {
    const Document::InputNumber* number = ChosenNumber();
    input_number_row_->setVisible(number != nullptr);
    input_number_->setVisible(number != nullptr);
    if (number != nullptr) input_number_->setText(InputNumberShown(*number));
}

void Window::ShowInputTexture(const Document::InputSlot* slot) {
    input_texture_row_->setVisible(slot != nullptr);
    input_texture_->setVisible(slot != nullptr);
    if (slot == nullptr) return;
    const QSignalBlocker quiet(input_texture_);
    input_texture_->clear();
    for (const ImageRow& one : rows_.images)
        input_texture_->addItem(one.name);
    input_texture_->setCurrentText(InputTextureOf(*slot));
}

QString Window::InputTextureOf(const Document::InputSlot& slot) const {
    const auto told = input_values_.find(QString::fromStdString(slot.name));
    return told == input_values_.end() ? QString::fromStdString(slot.texture) : told->second;
}

const Document::InputSlot* Window::InputNamed(const QString& name) const {
    const std::string wanted = name.toStdString();
    for (const Document::InputSlot& slot : shown_inputs_.names) {
        if (slot.name == wanted) return &slot;
    }
    return nullptr;
}

QString Window::InputNumberShown(const Document::InputNumber& number) const {
    uint32_t shown = 0;
    for (std::size_t at = 0; at < number.places.size(); at++) {
        const Document::InputSlot& slot = shown_inputs_.names[number.places[at]];
        shown += number.weights[at] *
                 Document::DigitOf(InputTextureOf(slot).toStdString(), number.digit_at);
    }
    return QString::number(shown);
}

void Window::SetInputNumber() {
    const Document::InputNumber* number = ChosenNumber();
    if (number == nullptr) return;
    bool digits = false;
    const uint32_t wanted = input_number_->text().toUInt(&digits);
    if (!digits) {
        ShowRefusal(tr("%1 is not a number").arg(input_number_->text()));
        return;
    }
    if (wanted > (number->weights.front() * kDigits) - 1) {
        ShowRefusal(tr("%1 has %2 digit places in the file, so it cannot show %3")
                        .arg(QString::fromStdString(number->stem))
                        .arg(number->places.size())
                        .arg(wanted));
        return;
    }
    for (std::size_t at = 0; at < number->places.size(); at++) {
        const Document::InputSlot& slot = shown_inputs_.names[number->places[at]];
        const QString name = QString::fromStdString(slot.name);
        const std::string glyph = Document::Digit(slot.texture, number->digit_at,
                                                  (wanted / number->weights[at]) % kDigits);
        if (!glyph.empty()) input_values_[name] = QString::fromStdString(glyph);
        const bool leading = wanted < number->weights[at] && number->weights[at] > 1;
        if (leading && input_blank_->isChecked()) {
            input_hidden_.insert(name);
        } else {
            input_hidden_.erase(name);
        }
    }
    ApplyInputValues();
}

void Window::SetInputTexture() {
    if (input_chosen_name_.isEmpty()) return;
    input_values_[input_chosen_name_] = input_texture_->currentText();
    ApplyInputValues();
}

void Window::ClearInputValue() {
    if (const Document::InputNumber* number = ChosenNumber()) {
        for (const std::size_t place : number->places) {
            const QString name = QString::fromStdString(shown_inputs_.names[place].name);
            input_values_.erase(name);
            input_hidden_.erase(name);
        }
    }
    input_values_.erase(input_chosen_name_);
    input_hidden_.erase(input_chosen_name_);
    ApplyInputValues();
}

void Window::ApplyInputValues() {
    std::vector<PreviewClient::InputValue> values;
    values.reserve(input_values_.size());
    for (const auto& [name, texture] : input_values_) {
        values.push_back(PreviewClient::InputValue{.path = name.toStdString(),
                                                   .texture = texture.toStdString(),
                                                   .hidden = input_hidden_.contains(name)});
    }
    for (const QString& name : input_hidden_) {
        if (input_values_.contains(name)) continue;
        values.push_back(
            PreviewClient::InputValue{.path = name.toStdString(), .texture = {}, .hidden = true});
    }
    if (host_.Running()) {
        const auto told = host_.SetInputs(values);
        if (!told) {
            ReportOnce(QString::fromStdString(told.error()));
            return;
        }
        ShowFrame();
    }
    RefreshInputRows();
    ShowInputValue(inputs_->currentItem());
}

void Window::RefreshInputRows() {
    for (QTreeWidgetItemIterator row(inputs_); *row != nullptr; ++row) {
        const QVariant number = (*row)->data(0, kNumberRole);
        if ((*row)->data(0, Rows::kChipRole).isValid() && number.isValid()) {
            (*row)->setData(0, Rows::kChipRole, InputNumberShown(input_numbers_[number.toInt()]));
            continue;
        }
        if (!(*row)->data(0, kDepthRole).isValid()) continue;
        const Document::InputSlot* slot = InputNamed((*row)->text(0));
        if (slot == nullptr) continue;
        (*row)->setIcon(0, input_hidden_.contains((*row)->text(0))
                               ? QIcon()
                               : InputPicture(InputTextureOf(*slot)));
    }
}

void Window::ShowInputAt(int clip, uint16_t depth, uint32_t frame) {
    if (clip >= 0 && clip != clip_index_) ChooseClip(clip);
    SeekTo(frame);
    ChooseDepth(depth);
}

}
