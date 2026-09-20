#include "editor_window.h"

#include "editor_commands.h"
#include "editor_timeline.h"

#include "document/span_arrange.h"
#include "document/stage_align.h"
#include "document/stage_fit.h"
#include "document/stage_bounds.h"

#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QString>

#include <array>
#include <cstdint>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace Editor {

void Window::AddDepthMenu() {
    QMenu* depth = AddMenu(nullptr, tr("&Depth"));
    const auto add = [this](QMenu* menu, Command command) {
        menu->addAction(commands_->Add(std::move(command)));
    };
    const auto chosen = [this] { return NeedsDepth(); };
    add(depth, {.id = "depth.add",
                .text = tr("&Add a depth at the playhead..."),
                .brief = tr("Add a depth"),
                .run = [this] { AddDepthHere(); },
                .refusal = [this] { return NeedsAnimation(); }});
    add(depth, {.id = "depth.split",
                .text = tr("&Split depth at the playhead"),
                .brief = tr("Split"),
                .keys = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D),
                .run = [this] { SplitDepthAt(static_cast<uint16_t>(*depth_), frame_); },
                .refusal = [this] { return ChosenDepthRefusal(tr("splitting it")); }});
    add(depth, {.id = "depth.duplicate",
                .text = tr("&Duplicate depth"),
                .brief = tr("Duplicate"),
                .keys = QKeySequence(Qt::CTRL | Qt::Key_D),
                .run = [this] { DuplicateChosenDepth(); },
                .refusal = chosen});
    add(depth, {.id = "depth.remove",
                .text = tr("&Remove the chosen depths here"),
                .brief = tr("Remove"),
                .run = [this] { RemoveChosenDepths(frame_); },
                .refusal = [this] { return ChosenDepthsRefusal(tr("removing it")); }});
    depth->addSeparator();

    QMenu* arrange = AddMenu(depth, tr("A&rrange"));
    const std::array<std::tuple<QString, QString, QKeySequence, Document::Arrange, QString>, 4>
        arrangements{{
            {"depth.forward", tr("Bring &forward"), QKeySequence(Qt::CTRL | Qt::Key_BracketRight),
             Document::Arrange::Forward, tr("Bring depth %1 forward")},
            {"depth.backward", tr("Send &backward"), QKeySequence(Qt::CTRL | Qt::Key_BracketLeft),
             Document::Arrange::Backward, tr("Send depth %1 backward")},
            {"depth.front", tr("Bring to f&ront"),
             QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight), Document::Arrange::Front,
             tr("Bring depth %1 to the front")},
            {"depth.back", tr("Send to bac&k"),
             QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft), Document::Arrange::Back,
             tr("Send depth %1 to the back")},
        }};
    for (const auto& [id, text, keys, how, name] : arrangements) {
        add(arrange, {.id = id,
                      .text = text,
                      .keys = keys,
                      .run = [this, how, name] { ArrangeDepth(how, name); },
                      .refusal = chosen});
    }

    QMenu* align = AddMenu(depth, tr("&Align"));
    const std::array<std::tuple<QString, QString, Document::AlignTo>, 6> alignments{{
        {"depth.align_left", tr("&Left edges"), Document::AlignTo::Left},
        {"depth.align_centre", tr("&Horizontal centres"), Document::AlignTo::HorizontalCentre},
        {"depth.align_right", tr("&Right edges"), Document::AlignTo::Right},
        {"depth.align_top", tr("&Top edges"), Document::AlignTo::Top},
        {"depth.align_middle", tr("&Vertical centres"), Document::AlignTo::VerticalCentre},
        {"depth.align_bottom", tr("&Bottom edges"), Document::AlignTo::Bottom},
    }};
    for (const auto& [id, text, how] : alignments) {
        const QString name = tr("Align %1").arg(QString(text).remove('&').toLower());
        add(align, {.id = id,
                    .text = text,
                    .run =
                        [this, name, how] {
                            ArrangeChosen(name,
                                          [how](const std::vector<Document::StageOutline>& picked) {
                                              return Document::AlignOffsets(picked, how);
                                          });
                        },
                    .refusal = [this] { return StageDepthsRefusal(2); }});
    }
    align->addSeparator();
    const std::array<std::tuple<QString, QString, Document::Spread>, 2> spreads{{
        {"depth.spread_across", tr("Spread centres &across"), Document::Spread::Across},
        {"depth.spread_down", tr("Spread centres &down"), Document::Spread::Down},
    }};
    for (const auto& [id, text, how] : spreads) {
        const QString name = QString(text).remove('&');
        add(align, {.id = id,
                    .text = text,
                    .run =
                        [this, name, how] {
                            ArrangeChosen(name,
                                          [how](const std::vector<Document::StageOutline>& picked) {
                                              return Document::SpreadOffsets(picked, how);
                                          });
                        },
                    .refusal = [this] { return StageDepthsRefusal(3); }});
    }

    QMenu* here = AddMenu(depth, tr("At the &playhead"));
    add(here, {.id = "depth.start_here",
               .text = tr("Move the depth's &start here"),
               .keys = QKeySequence(Qt::Key_BracketLeft),
               .run = [this] { MoveEdgeToPlayhead(SpanEnd::Start); },
               .refusal = chosen});
    add(here, {.id = "depth.end_here",
               .text = tr("Move the depth's &end here"),
               .keys = QKeySequence(Qt::Key_BracketRight),
               .run = [this] { MoveEdgeToPlayhead(SpanEnd::End); },
               .refusal = chosen});
    add(here, {.id = "depth.trim_start_here",
               .text = tr("&Trim the depth's start here"),
               .keys = QKeySequence(Qt::ALT | Qt::Key_BracketLeft),
               .run = [this] { TrimEdgeToPlayhead(SpanEnd::Start); },
               .refusal = chosen});
    add(here, {.id = "depth.trim_end_here",
               .text = tr("T&rim the depth's end here"),
               .keys = QKeySequence(Qt::ALT | Qt::Key_BracketRight),
               .run = [this] { TrimEdgeToPlayhead(SpanEnd::End); },
               .refusal = chosen});
    depth->addSeparator();

    add(depth, {.id = "depth.centre_anchor",
                .text = tr("&Centre the anchor in the content"),
                .brief = tr("Centre anchor"),
                .keys = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Home),
                .run = [this] { CentreChosenAnchor(); },
                .refusal = [this]() -> std::optional<QString> {
                    if (const auto refused = NeedsDepth()) return refused;
                    return AnchorRefusal(static_cast<uint16_t>(*depth_));
                }});
    const std::array<std::tuple<QString, QString, QKeySequence, Document::StageFit>, 3> fits{{
        {"depth.fit", tr("&Fit to the stage"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F),
         Document::StageFit::Both},
        {"depth.fit_width", tr("Fit to the stage's &width"),
         QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_H), Document::StageFit::Width},
        {"depth.fit_height", tr("Fit to the stage's &height"),
         QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_G), Document::StageFit::Height},
    }};
    for (const auto& [id, text, keys, fit] : fits) {
        add(depth, {.id = id,
                    .text = text,
                    .brief = tr("Fit"),
                    .keys = keys,
                    .run = [this, fit] { FitChosenToStage(fit); },
                    .refusal = [this] { return FitRefusal(); }});
    }
    add(depth, {.id = "depth.flip_across",
                .text = tr("Flip horizontally"),
                .brief = tr("Flip across"),
                .run = [this] { ReshapeOnStage(static_cast<uint16_t>(*depth_), -1, 1, 0, true); },
                .refusal = chosen});
    add(depth, {.id = "depth.flip_over",
                .text = tr("Flip vertically"),
                .brief = tr("Flip over"),
                .run = [this] { ReshapeOnStage(static_cast<uint16_t>(*depth_), 1, -1, 0, true); },
                .refusal = chosen});
    depth->addSeparator();

    add(depth, {.id = "depth.sequence",
                .text = tr("Se&quence the chosen depths one after another"),
                .brief = tr("Sequence"),
                .run = [this] { SequenceChosenDepths(frame_); },
                .refusal = [this] { return NeedsDepths(2); }});
    add(depth, {.id = "depth.group",
                .text = tr("&Group into a sprite..."),
                .brief = tr("Group"),
                .run = [this] { GroupDepthsIntoSprite(static_cast<uint16_t>(*depth_), frame_); },
                .refusal = chosen});
    add(depth, {.id = "depth.ungroup",
                .text = tr("&Ungroup the sprite here"),
                .run = [this] { UngroupSpriteAt(static_cast<uint16_t>(*depth_), frame_); },
                .refusal = [this] { return ChosenDepthRefusal(tr("ungrouping it")); }});
    add(depth, {.id = "depth.move_to",
                .text = tr("&Move to another depth..."),
                .run = [this] { MoveSpanToDepth(static_cast<uint16_t>(*depth_), frame_); },
                .refusal = chosen});
    add(depth, {.id = "depth.duplicate_to",
                .text = tr("Duplicate &onto another depth..."),
                .run = [this] { DuplicateSpanToDepth(static_cast<uint16_t>(*depth_), frame_); },
                .refusal = chosen});
    depth->addSeparator();

    add(depth, {.id = "depth.hide",
                .text = tr("&Hide or show in the view"),
                .run = [this] { ToggleHidden(static_cast<uint16_t>(*depth_)); },
                .refusal = chosen});
    add(depth, {.id = "depth.solo",
                .text = tr("S&olo in the view"),
                .run = [this] { SoloDepth(static_cast<uint16_t>(*depth_)); },
                .refusal = chosen});
    add(depth, {.id = "depth.lock",
                .text = tr("&Lock or unlock on stage"),
                .run = [this] { ToggleLocked(static_cast<uint16_t>(*depth_)); },
                .refusal = chosen});
    add(depth, {.id = "depth.show_all",
                .text = tr("Show &every hidden depth"),
                .run = [this] { ShowEveryDepth(); },
                .refusal = [this]() -> std::optional<QString> {
                    if (hidden_.empty()) return tr("No depth is hidden");
                    return std::nullopt;
                }});
    add(depth, {.id = "depth.unlock_all",
                .text = tr("&Unlock every locked depth"),
                .run = [this] { UnlockEveryDepth(); },
                .refusal = [this]() -> std::optional<QString> {
                    if (locked_.empty()) return tr("No depth is locked");
                    return std::nullopt;
                }});
    depth->addSeparator();

    add(depth, {.id = "depth.own",
                .text = tr("&Keyframe this depth"),
                .brief = tr("Keyframe this depth"),
                .run = [this] { OwnSelectedDepth(frame_); },
                .refusal = [this]() -> std::optional<QString> {
                    if (const auto refused = NeedsDepth()) return refused;
                    if (AuthoredAt(static_cast<uint16_t>(*depth_), frame_) != nullptr)
                        return tr("The project already owns this depth here");
                    return std::nullopt;
                }});
    add(depth, {.id = "depth.detach",
                .text = tr("De&tach to baked data"),
                .brief = tr("Detach"),
                .run = [this] { DetachSelectedDepth(); },
                .refusal = [this] { return NeedsOwnedDepth(); }});
    add(depth, {.id = "depth.edit_script",
                .text = tr("Edit the scr&ipt..."),
                .run = [this] { EditOwnedScript(); },
                .refusal = [this] { return NeedsOwnedDepth(); }});
    add(depth, {.id = "depth.start_animating",
                .text = tr("Start &animating a property..."),
                .run = [this] { StartAnimating(); },
                .refusal = [this] { return NeedsOwnedDepth(); }});
}

void Window::AddKeyframeMenu() {
    QMenu* keys = AddMenu(nullptr, tr("&Keyframe"));
    const auto add = [this, keys](Command command) {
        command.scope = timeline_;
        keys->addAction(commands_->Add(std::move(command)));
    };
    add({.id = "key.hold",
         .text = tr("Toggle hold"),
         .brief = tr("Hold"),
         .keys = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H),
         .run = [this] { ToggleHoldSelectedKeys(); },
         .refusal = [this] { return NeedsKeys(1); }});
    add({.id = "key.ease",
         .text = tr("Easy ease"),
         .brief = tr("Easy ease"),
         .keys = QKeySequence(Qt::Key_F9),
         .run = [this] { EasyEaseSelectedKeys(Document::EasySide::Both); },
         .refusal = [this] { return NeedsKeys(1); }});
    add({.id = "key.ease_in",
         .text = tr("Easy ease in"),
         .keys = QKeySequence(Qt::SHIFT | Qt::Key_F9),
         .run = [this] { EasyEaseSelectedKeys(Document::EasySide::In); },
         .refusal = [this] { return NeedsKeys(1); }});
    add({.id = "key.ease_out",
         .text = tr("Easy ease out"),
         .keys = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9),
         .run = [this] { EasyEaseSelectedKeys(Document::EasySide::Out); },
         .refusal = [this] { return NeedsKeys(1); }});
    keys->addSeparator();
    add({.id = "key.reverse",
         .text = tr("Time-&reverse"),
         .brief = tr("Reverse"),
         .run = [this] { ReverseSelectedKeys(); },
         .refusal = [this] { return NeedsKeys(2); }});
    add({.id = "key.stretch",
         .text = tr("Time-&stretch..."),
         .brief = tr("Stretch"),
         .run = [this] { StretchSelectedKeys(); },
         .refusal = [this] { return NeedsKeys(2); }});
    add({.id = "key.wiggle",
         .text = tr("&Wiggle..."),
         .brief = tr("Wiggle"),
         .run = [this] { WiggleSelectedKeys(); },
         .refusal = [this] { return NeedsKeys(2); }});
    add({.id = "key.simplify",
         .text = tr("Si&mplify..."),
         .brief = tr("Simplify"),
         .run = [this] { SimplifySelectedKeys(); },
         .refusal = [this] { return NeedsKeys(3); }});
}

}
