#include "editor_window.h"

#include <DockManager.h>
#include <DockWidget.h>

#include "document/clip_edit.h"

#include "editor_inspector.h"
#include "editor_script_editor.h"
#include "editor_script_ide.h"
#include "editor_selection_bar.h"
#include "editor_timeline.h"

#include "document/authored.h"
#include "document/characters.h"
#include "document/inputs.h"
#include "document/script_index.h"
#include "document/script_source.h"
#include "document/clip.h"
#include "document/inspector.h"
#include "document/clip_edit.h"
#include "document/inspector_view.h"
#include "document/keyframe_edit.h"
#include "document/span_edit.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "formats/afp_script.h"

#include <QColor>
#include <QColorDialog>
#include <QStackedWidget>
#include <QFileInfo>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Editor {

namespace {

std::optional<uint16_t> ShownCharacter(const AfpAnimation::Container& clip, uint16_t depth,
                                       uint32_t frame) {
    const std::vector<Document::DepthRow> rows = Document::DepthRows(clip);
    const auto row = std::ranges::find(rows, depth, &Document::DepthRow::depth);
    if (row == rows.end()) return std::nullopt;
    std::optional<uint16_t> shown;
    for (const auto& [at, character] : row->shows) {
        if (at <= frame) shown = character;
    }
    return shown;
}

}

QStringList Window::NamesFor(const AfpAnimation::Animation& animation,
                             const std::map<uint16_t, std::string>& shape_images) {
    const Document::InputSurface surface = Document::Inputs(animation, shape_images);
    QStringList names;
    for (const Document::InputLabel& label : surface.labels)
        names.append(QString::fromStdString(label.name));
    for (const Document::InputSlot& slot : surface.names)
        names.append(QString::fromStdString(slot.name));
    names.removeDuplicates();
    names.sort();
    return names;
}

void Window::ShowScriptAt(uint32_t frame) {
    SeekTo(frame);
    ChooseDepths({});
    if (docks_ == nullptr) return;
    if (ads::CDockWidget* shown = docks_->findDockWidget(tr("Inspector"))) shown->setAsCurrentTab();
}

void Window::ShowScriptIde(bool open) {
    if (script_ide_ == nullptr || centre_ == nullptr) return;
    if (!open) {
        ide_place_.reset();
        centre_->setCurrentWidget(docks_);
        return;
    }
    ide_place_ = Document::ScriptPlace{.clip = clip_, .frame = frame_, .depth = std::nullopt};
    centre_->setCurrentWidget(script_ide_);
    FillScriptIde();
}

void Window::ChooseIdeScript(const Document::ScriptPlace& place) {
    ide_place_ = place;
    FillScriptIde();
}

void Window::GoToName(const QString& name) {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) return;
    const Document::InputSurface surface =
        Document::Inputs(*animation, file_->ShapeImages(animation_path_));
    const std::string wanted = name.toStdString();
    for (const Document::InputLabel& label : surface.labels) {
        if (label.name != wanted) continue;
        ShowScriptIde(false);
        SeekTo(label.frame);
        return;
    }
    for (const Document::InputSlot& slot : surface.names) {
        if (slot.name != wanted) continue;
        ShowScriptIde(false);
        ChooseDepth(slot.depth);
        return;
    }
}

void Window::FillScriptIde() {
    if (script_ide_ == nullptr || !file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) return;

    script_ide_->ShowPackage(QFileInfo(document_path_).fileName(),
                             QString::fromStdString(animation_name_));
    script_ide_->ShowScripts(Document::ScriptsIn(*animation), ide_place_);

    std::vector<IdeName> names;
    const Document::InputSurface surface =
        Document::Inputs(*animation, file_->ShapeImages(animation_path_));
    for (const Document::InputLabel& label : surface.labels) {
        names.push_back(IdeName{.kind = "L",
                                .name = QString::fromStdString(label.name),
                                .where = tr("frame label, %1").arg(label.frame)});
    }
    for (const Document::InputSlot& slot : surface.names) {
        names.push_back(IdeName{.kind = "C",
                                .name = QString::fromStdString(slot.name),
                                .where = tr("depth %1").arg(slot.depth)});
    }
    for (const std::string& call : Document::CallsIn(*animation)) {
        names.push_back(IdeName{.kind = "f", .name = QString::fromStdString(call), .where = {}});
    }
    script_ide_->ShowNames(names);

    if (!ide_place_) {
        script_ide_->ShowNothing(tr("No script"), tr("choose a script on the left"));
        return;
    }
    const QString title = ide_place_->depth ? tr("depth %1 load").arg(*ide_place_->depth)
                                            : tr("frame %1").arg(ide_place_->frame);
    const std::optional<AfpAnimation::Bytecode> code = Document::ScriptAt(*animation, *ide_place_);
    if (!code) {
        script_ide_->ShowNothing(title, tr("this frame holds no script"));
        return;
    }
    const std::optional<std::string> source = Document::ScriptSourceText(*animation, *code);
    if (!source) {
        script_ide_->ShowNothing(title,
                                 tr("this script holds a value the editor cannot write back"));
        return;
    }
    AfpAnimation::Animation trial = *animation;
    const auto again = Document::CompileScript(trial, *source);
    const auto read = AfpScript::Read(code->code);
    const std::size_t slack = read ? read->trailing.size() : 0;
    const std::vector<uint8_t> instructions(code->code.begin(),
                                            code->code.end() - static_cast<std::ptrdiff_t>(slack));
    const bool round_trips = again && again->code == instructions;
    script_ide_->ShowScript(title, QString::fromStdString(*source), code->code, round_trips,
                            *ide_place_);

    script_ide_->ShowHistory(HistoryNames(), static_cast<int>(history_.Position()));
}

QStringList Window::HistoryNames() const {
    QStringList steps;
    for (const std::string& name : history_.Names())
        steps.append(QString::fromStdString(name));
    return steps;
}

void Window::CompileIdeScript(const QString& source) {
    if (!ide_place_ || !file_ || animation_path_.empty() || script_ide_ == nullptr) return;
    const Document::ScriptPlace place = *ide_place_;
    const std::string wanted = source.toStdString();
    auto written = [place, wanted](AfpAnimation::Animation& edited) {
        if (place.depth) {
            return Document::WritePlacementScript(edited, place.clip, *place.depth, place.frame,
                                                  wanted);
        }
        return Document::WriteFrameScript(edited, place.clip, place.frame, wanted);
    };

    auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        script_ide_->ShowProblems({IdeProblem{.said = QString::fromStdString(animation.error())}});
        return;
    }
    const auto tried = written(*animation);
    if (!tried) {
        script_ide_->ShowProblems({IdeProblem{.said = QString::fromStdString(tried.error())}});
        return;
    }
    const QString named = place.depth ? tr("The script of depth %1").arg(*place.depth)
                                      : tr("The script on frame %1").arg(place.frame);
    if (!EditAnimation(named, written)) return;
    FillScriptIde();
}

void Window::ShowInspectorScript(const AfpAnimation::Animation& animation,
                                 const AfpAnimation::Container& clip) {
    if (inspector_panel_ == nullptr) return;
    const std::optional<std::size_t> tag = Document::FrameScriptTag(clip, frame_);
    const auto* action = tag ? std::get_if<AfpAnimation::Action>(&clip.tags[*tag].body) : nullptr;
    if (action == nullptr) {
        inspector_panel_->ShowScript(ScriptView{});
        return;
    }
    ScriptView view{
        .shown = true, .title = tr("Frame %1").arg(frame_), .source = {}, .refusal = {}};
    const std::optional<std::string> source =
        Document::ScriptSourceText(animation, action->bytecode);
    if (source) {
        view.source = QString::fromStdString(*source);
    } else {
        view.refusal = tr("this script holds a value the editor cannot write back, so it is left "
                          "as it is");
    }
    const QStringList names = NamesFor(animation, file_->ShapeImages(animation_path_));
    inspector_panel_->ShowScript(view);
    inspector_panel_->KnowScriptNames(names);
}

namespace {

std::vector<uint8_t> FrameScriptCode(const AfpAnimation::Animation& animation,
                                     const Document::ClipId& clip, uint32_t frame) {
    const AfpAnimation::Container* body = Document::FindClip(animation, clip);
    if (body == nullptr) return {};
    const std::optional<std::size_t> tag = Document::FrameScriptTag(*body, frame);
    if (!tag) return {};
    const auto* action = std::get_if<AfpAnimation::Action>(&body->tags[*tag].body);
    if (action == nullptr) return {};
    return action->bytecode.code;
}

}

void Window::CompileFrameScript(const QString& source) {
    if (!file_ || animation_path_.empty() || inspector_panel_ == nullptr) return;
    const Document::ClipId clip = clip_;
    const uint32_t frame = frame_;
    const std::string wanted = source.toStdString();
    auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        inspector_panel_->ShowScriptProblem(QString::fromStdString(animation.error()));
        return;
    }
    const std::vector<uint8_t> before = FrameScriptCode(*animation, clip, frame);
    const auto tried = Document::WriteFrameScript(*animation, clip, frame, wanted);
    if (!tried) {
        const QString refused = QString::fromStdString(tried.error());
        inspector_panel_->ShowScriptProblem(refused);
        if (script_ide_ != nullptr) script_ide_->ShowProblems({IdeProblem{.said = refused}});
        return;
    }
    const std::vector<uint8_t> after = FrameScriptCode(*animation, clip, frame);
    if (!EditAnimation(tr("The script on frame %1").arg(frame),
                       [clip, frame, wanted](AfpAnimation::Animation& edited) {
                           return Document::WriteFrameScript(edited, clip, frame, wanted);
                       })) {
        return;
    }
    inspector_panel_->ShowScriptWritten(static_cast<int>(after.size()), after == before);
}

void Window::ShowInspectorSubject(const AfpAnimation::Animation& animation) {
    const AfpAnimation::Container* clip = Document::FindClip(animation, clip_);
    if (clip == nullptr || inspector_panel_ == nullptr) return;
    ShowInspectorScript(animation, *clip);
    if (!depth_) {
        inspector_panel_->ShowSubject(
            InspectorSubject{.title = QString::fromStdString(animation_name_),
                             .detail = tr("No depth chosen"),
                             .depth_chosen = false,
                             .owned = false});
        inspector_panel_->ShowView(std::nullopt, frame_);
        inspector_panel_->ShowExtras(std::nullopt);
        return;
    }
    const auto depth = static_cast<uint16_t>(*depth_);
    const Document::AuthoredDepth* owned = AuthoredAt(depth, frame_);
    QString detail;
    QString character_name;
    if (const std::optional<uint16_t> character = ShownCharacter(*clip, depth, frame_)) {
        for (const Document::CharacterSummary& one :
             Document::Characters(animation, file_->ShapeImages(animation_path_))) {
            if (one.id == *character) character_name = QString::fromStdString(one.label);
        }
        detail = character_name;
    }
    if (const std::optional<Document::Span> span = Document::SpanOfDepth(*clip, depth, frame_)) {
        const QString frames = tr("frames %1 to %2").arg(span->first_frame).arg(span->last_frame);
        detail = detail.isEmpty() ? frames : detail + ", " + frames;
    }
    inspector_panel_->ShowSubject(InspectorSubject{.title = tr("Depth %1").arg(depth),
                                                   .detail = detail,
                                                   .depth_chosen = true,
                                                   .owned = owned != nullptr});
    inspector_panel_->ShowView(Document::ViewPlacement(*clip, depth, frame_, owned), frame_);
    ShowInspectorExtras(animation, depth, character_name);
}

QWidget* Window::BarAnchor(const QString& id) const {
    if (selection_bar_ == nullptr) return nullptr;
    return selection_bar_->findChild<QWidget*>("bar_" + id);
}

void Window::RefreshSelectionBar() {
    if (selection_bar_ == nullptr) return;
    const std::vector<uint16_t> chosen = SelectedDepths();
    const std::size_t keys = timeline_->SelectedKeys().size();
    if (keys > 0) {
        const QString property =
            key_property_.isEmpty() ? tr("keyframes") : key_property_.toLower();
        selection_bar_->Show(keys == 1 ? tr("1 keyframe, %1").arg(property)
                                       : tr("%1 keyframes, %2").arg(keys).arg(property),
                             tr("frame %1").arg(frame_),
                             {"key.hold", "key.ease", "key.reverse", "key.stretch", "key.wiggle",
                              "key.simplify", "edit.copy", "edit.delete"});
        return;
    }
    if (chosen.size() > 1) {
        selection_bar_->Show(tr("%1 depths chosen").arg(chosen.size()), tr("frame %1").arg(frame_),
                             {"depth.align_left", "depth.align_centre", "depth.align_top",
                              "depth.spread_across", "depth.sequence", "depth.remove"});
        return;
    }
    if (chosen.empty()) {
        selection_bar_->Show(tr("Nothing chosen"), QString(), {});
        return;
    }
    const bool owned = AuthoredAt(chosen.front(), frame_) != nullptr;
    selection_bar_->Show(
        tr("Depth %1").arg(chosen.front()),
        owned ? tr("owned, frame %1").arg(frame_) : tr("baked, frame %1").arg(frame_),
        {"depth.fit", "depth.centre_anchor", "depth.flip_across", "depth.flip_over",
         "depth.forward", "depth.backward", "depth.split", "depth.duplicate", "depth.group",
         owned ? QString("depth.detach") : QString("depth.own"), "depth.remove"});
}

void Window::ShowInspectorExtras(const AfpAnimation::Animation& animation, uint16_t depth,
                                 const QString& character) {
    const Document::Selection selection{.depth = depth,
                                        .frame = frame_,
                                        .owned = AuthoredAt(depth, frame_),
                                        .key_property = {},
                                        .key_frame = std::nullopt,
                                        .clip = clip_};
    PlacementExtras extras{.character = character,
                           .blend = 0,
                           .clip_depth = 0,
                           .filters = {},
                           .owned = selection.owned != nullptr};
    for (const Document::InspectedRow& row : Document::InspectFrame(animation, selection)) {
        const QString name = QString::fromStdString(row.field.name);
        const QString value = QString::fromStdString(row.field.value);
        if (name == "Blend") extras.blend = value.toInt();
        if (name == "Clip depth") extras.clip_depth = value.toInt();
        if (name.startsWith("Filter ") && name.count(' ') == 1)
            extras.filters.append(name + ": " + value);
    }
    inspector_panel_->ShowExtras(extras);
}

void Window::EditPlacementFieldOnDepth(const QString& field, const QString& value) {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const auto depth = static_cast<uint16_t>(*depth_);
    const uint32_t frame = frame_;
    const Document::ClipId clip = clip_;
    const std::string named = field.toStdString();
    const std::string wanted = value.toStdString();
    EditAnimation(tr("%1 on depth %2").arg(field).arg(depth), [clip, depth, frame, &named, &wanted](
                                                                  AfpAnimation::Animation& edited) {
        return Document::EditPlacementField(edited, clip, depth, frame, named, wanted);
    });
}

void Window::ApplyViewEdit(const QString& label, const std::vector<double>& values) {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const auto depth = static_cast<uint16_t>(*depth_);
    const uint32_t frame = frame_;
    const Document::ClipId clip = clip_;
    const std::string wanted = label.toStdString();
    const QString name = tr("%1 of depth %2").arg(label).arg(depth);
    if (AuthoredAt(depth, frame) != nullptr) {
        EditOwned(name, [&wanted, frame, &values](Document::AuthoredDepth& authored,
                                                  const Document::BakedDepth& baked) {
            return Document::SetViewedOwned(authored, baked, frame, wanted, values);
        });
        return;
    }
    EditAnimation(name, [clip, depth, frame, &wanted, &values](AfpAnimation::Animation& edited) {
        return Document::SetViewedBaked(edited, clip, depth, frame, wanted, values);
    });
}

void Window::ToggleViewKeying(const QString& label, bool animated) {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const auto depth = static_cast<uint16_t>(*depth_);
    const Document::AuthoredDepth* owned = AuthoredAt(depth, frame_);
    if (owned == nullptr) return;
    const std::optional<std::string> track = Document::ViewTrack(*owned, label.toStdString());
    if (!track) return;
    const std::string property = *track;
    const uint32_t frame = frame_;
    if (!animated) {
        EditOwned(
            tr("Animate %1 on depth %2").arg(label).arg(depth),
            [&property](Document::AuthoredDepth& authored, const Document::BakedDepth& baked) {
                return Document::AddTrack(authored, baked, property);
            });
        return;
    }
    const bool keyed = Document::KeyAt(*owned, property, frame).has_value();
    const QString name = keyed ? tr("Remove the %1 keyframe at frame %2").arg(label).arg(frame)
                               : tr("Add a %1 keyframe at frame %2").arg(label).arg(frame);
    EditAuthored(name, [&property, frame, keyed](Document::AuthoredDepth& authored) {
        return keyed ? Document::RemoveKeyAt(authored, property, frame)
                     : Document::AddKeyAt(authored, property, frame);
    });
}

void Window::PickViewColour(const QString& label) {
    if (!file_ || !depth_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) return;
    const AfpAnimation::Container* clip = Document::FindClip(*animation, clip_);
    if (clip == nullptr) return;
    const auto depth = static_cast<uint16_t>(*depth_);
    const auto view = Document::ViewPlacement(*clip, depth, frame_, AuthoredAt(depth, frame_));
    if (!view) return;
    const auto row =
        std::ranges::find(view->colours, label.toStdString(), &Document::ViewRow::label);
    if (row == view->colours.end() || row->values.size() != 4) return;
    const QColor current(static_cast<int>(row->values[0]), static_cast<int>(row->values[1]),
                         static_cast<int>(row->values[2]), static_cast<int>(row->values[3]));
    const QColor chosen =
        QColorDialog::getColor(current, this, tr("Pick a colour"), QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) return;
    ApplyViewEdit(label, {static_cast<double>(chosen.red()), static_cast<double>(chosen.green()),
                          static_cast<double>(chosen.blue()), static_cast<double>(chosen.alpha())});
}

}
