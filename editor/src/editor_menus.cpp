#include "editor_window.h"

#include "editor_commands.h"
#include "editor_graph.h"
#include "editor_files.h"
#include "editor_notices.h"
#include "editor_search.h"
#include "editor_timeline.h"
#include "editor_viewport.h"

#include "document/playback.h"

#include <DockManager.h>
#include <DockWidget.h>

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>
#include <QString>

#include <cstdint>
#include <functional>
#include <utility>
#include <optional>

namespace Editor {

QMenu* Window::AddMenu(QMenu* parent, const QString& title) {
    QMenu* menu = parent != nullptr ? parent->addMenu(title) : menuBar()->addMenu(title);
    if (parent == nullptr) menus_.push_back(menu);
    commands_->ShowAvailabilityIn(menu);
    return menu;
}

void Window::BuildMenus() {
    search_ = new CommandSearch(this);
    connect(commands_, &Commands::Refused, this, &Window::ShowRefusal);
    AddFileMenu();
    AddEditMenu();
    AddDepthMenu();
    AddKeyframeMenu();
    AddClipMenu();
    AddToolsMenu();
    AddViewMenu();
    AddPlaybackMenu();
}

void Window::AddFileMenu() {
    QMenu* file = AddMenu(nullptr, tr("&File"));
    const auto document = [this] { return NeedsDocument(); };
    const auto animation = [this] { return NeedsAnimation(); };
    file->addAction(commands_->Add(
        {.id = "file.open", .text = tr("&Open IFS..."), .keys = QKeySequence::Open, .run = [this] {
             ChooseDocument();
         }}));
    file->addAction(commands_->Add({.id = "file.save",
                                    .text = tr("&Save"),
                                    .keys = QKeySequence::Save,
                                    .run = [this] { Save(); },
                                    .refusal = document}));
    file->addAction(commands_->Add({.id = "file.save_as",
                                    .text = tr("Save &as..."),
                                    .keys = QKeySequence::SaveAs,
                                    .run = [this] { SaveAs(); },
                                    .refusal = document}));
    file->addAction(commands_->Add({.id = "file.save_frame",
                                    .text = tr("Save the &frame as PNG..."),
                                    .keys = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S),
                                    .run = [this] { SaveFrameAs(); },
                                    .refusal = animation}));
    file->addAction(
        commands_->Add({.id = "file.save_frames",
                        .text = tr("Save the work area as PNG f&rames..."),
                        .keys = QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_S),
                        .run = [this] { SaveFramesAs(); },
                        .refusal = animation}));
    file->addSeparator();
    file->addAction(commands_->Add({.id = "project.new",
                                    .text = tr("&New project..."),
                                    .run = [this] { CreateProject(); },
                                    .refusal = [this]() -> std::optional<QString> {
                                        if (const auto refused = NeedsDocument()) return refused;
                                        if (project_) return tr("A project is already open");
                                        return std::nullopt;
                                    }}));
    file->addAction(commands_->Add({.id = "project.open",
                                    .text = tr("Open &project..."),
                                    .run = [this] { ChooseProject(); }}));
    file->addAction(commands_->Add({.id = "project.show_folder",
                                    .text = tr("Show the project &folder"),
                                    .run = [this] { ShowProjectFolder(); },
                                    .refusal = [this]() -> std::optional<QString> {
                                        if (!project_) return tr("No project is open");
                                        return std::nullopt;
                                    }}));
    file->addAction(commands_->Add({.id = "project.close",
                                    .text = tr("&Close project"),
                                    .run = [this] { CloseProject(); },
                                    .refusal = [this]() -> std::optional<QString> {
                                        if (!project_) return tr("No project is open");
                                        return std::nullopt;
                                    }}));
    file->addAction(commands_->Add({.id = "project.export",
                                    .text = tr("&Export into the IFS"),
                                    .keys = QKeySequence(Qt::CTRL | Qt::Key_E),
                                    .run = [this] { ExportToPackage(); },
                                    .refusal = [this]() -> std::optional<QString> {
                                        if (const auto refused = NeedsProject()) return refused;
                                        if (authored_.empty() && project_->images.empty())
                                            return tr("The project has nothing to export");
                                        return std::nullopt;
                                    }}));
    file->addSeparator();
    file->addAction(commands_->Add({.id = "file.game_install",
                                    .text = tr("Choose &game install..."),
                                    .run = [this] { ChooseGameDirectory(); }}));
    file->addSeparator();
    file->addAction(
        commands_->Add({.id = "file.quit", .text = tr("&Quit"), .run = [this] { close(); }}));
}

void Window::AddEditMenu() {
    QMenu* edit = AddMenu(nullptr, tr("&Edit"));
    undo_action_ = commands_->Add({.id = "edit.undo",
                                   .text = tr("&Undo"),
                                   .keys = QKeySequence::Undo,
                                   .run = [this] { Undo(); },
                                   .refusal = [this]() -> std::optional<QString> {
                                       if (!history_.CanUndo()) return tr("Nothing to undo");
                                       return std::nullopt;
                                   }});
    redo_action_ = commands_->Add({.id = "edit.redo",
                                   .text = tr("&Redo"),
                                   .keys = QKeySequence::Redo,
                                   .run = [this] { Redo(); },
                                   .refusal = [this]() -> std::optional<QString> {
                                       if (!history_.CanRedo()) return tr("Nothing to redo");
                                       return std::nullopt;
                                   }});
    edit->addAction(undo_action_);
    edit->addAction(redo_action_);
    edit->addSeparator();
    edit->addAction(commands_->Add({.id = "edit.search",
                                    .text = tr("&Search commands, depths and animations..."),
                                    .keys = QKeySequence(Qt::CTRL | Qt::Key_K),
                                    .run = [this] { OpenSearch(); }}));
    edit->addSeparator();
    const auto depth_or_keys = [this]() -> std::optional<QString> {
        if (!timeline_->SelectedKeys().empty()) return std::nullopt;
        if (NeedsDepth()) return tr("Choose a depth or select keyframes first");
        return std::nullopt;
    };
    edit->addAction(commands_->Add({.id = "edit.copy",
                                    .text = tr("Copy"),
                                    .keys = QKeySequence::Copy,
                                    .run = [this] { CopySelection(); },
                                    .refusal = depth_or_keys,
                                    .scope = timeline_}));
    edit->addAction(commands_->Add({.id = "edit.cut",
                                    .text = tr("Cut"),
                                    .keys = QKeySequence::Cut,
                                    .run = [this] { CutSelection(); },
                                    .refusal = depth_or_keys,
                                    .scope = timeline_}));
    edit->addAction(commands_->Add({.id = "edit.paste",
                                    .text = tr("Paste"),
                                    .keys = QKeySequence::Paste,
                                    .run = [this] { PasteClipboard(); },
                                    .refusal = [this]() -> std::optional<QString> {
                                        if (const auto refused = NeedsAnimation()) return refused;
                                        if (keys_copied_last_ ? !copied_keys_ : !copied_span_)
                                            return tr("Copy a depth or keyframes first");
                                        return std::nullopt;
                                    },
                                    .scope = timeline_}));
    edit->addAction(commands_->Add({.id = "edit.delete",
                                    .text = tr("Delete keyframes"),
                                    .keys = QKeySequence::Delete,
                                    .run = [this] { RemoveSelectedKeys(); },
                                    .refusal = depth_or_keys,
                                    .scope = timeline_}));
    edit->addAction(commands_->Add({.id = "edit.select_all_keys",
                                    .text = tr("Select every keyframe"),
                                    .keys = QKeySequence::SelectAll,
                                    .run = [this] { SelectAllKeys(); },
                                    .refusal = [this] { return NeedsOwnedDepth(); },
                                    .scope = timeline_}));
}

void Window::AddClipMenu() {
    QMenu* clip = AddMenu(nullptr, tr("&Clip"));
    const auto animation = [this] { return NeedsAnimation(); };
    const auto work_area = [this] { return WorkAreaRefusal(); };
    clip->addAction(commands_->Add({.id = "clip.insert_frame",
                                    .text = tr("&Insert a frame at the playhead"),
                                    .run = [this] { InsertFrameAt(frame_); },
                                    .refusal = animation}));
    clip->addAction(commands_->Add({.id = "clip.remove_frame",
                                    .text = tr("&Remove the frame at the playhead"),
                                    .run = [this] { RemoveFrameAt(frame_); },
                                    .refusal = animation}));
    clip->addAction(commands_->Add({.id = "clip.add_label",
                                    .text = tr("Add a &label at the playhead..."),
                                    .run = [this] { AddLabelAt(frame_); },
                                    .refusal = animation}));
    context_action_ =
        commands_->AddToggle({.id = "clip.in_context",
                              .text = tr("Edit a clip &in place on the root"),
                              .checked = QSettings().value(kInContextKey, true).toBool(),
                              .set = [this](bool on) {
                                  QSettings().setValue(kInContextKey, on);
                                  if (file_)
                                      LoadIntoHost(false, *file_, tr("Updating the preview"),
                                                   [this](bool) { ShowFrame(); });
                                  ShowFrame();
                              }});
    clip->addAction(context_action_);
    clip->addAction(commands_->Add({.id = "clip.leave",
                                    .text = tr("&Leave this clip"),
                                    .keys = QKeySequence(Qt::Key_Escape),
                                    .run = [this] { LeaveClip(); },
                                    .refusal = [this]() -> std::optional<QString> {
                                        if (!clip_.sprite) return tr("The root is already open");
                                        return std::nullopt;
                                    }}));
    clip->addAction(commands_->Add({.id = "clip.camera",
                                    .text = tr("Add or remove the &camera at the playhead..."),
                                    .run = [this] { ToggleCameraAt(frame_); },
                                    .refusal = animation}));
    clip->addSeparator();
    clip->addAction(commands_->Add({.id = "clip.work_start",
                                    .text = tr("Start the &work area here"),
                                    .keys = QKeySequence(Qt::Key_B),
                                    .run =
                                        [this] {
                                            SetWorkArea(Document::WithWorkAreaStart(
                                                work_area_, frame_, ClipFrameCount()));
                                        },
                                    .refusal = animation}));
    clip->addAction(commands_->Add({.id = "clip.work_end",
                                    .text = tr("En&d the work area here"),
                                    .keys = QKeySequence(Qt::Key_N),
                                    .run =
                                        [this] {
                                            SetWorkArea(Document::WithWorkAreaEnd(
                                                work_area_, frame_, ClipFrameCount()));
                                        },
                                    .refusal = animation}));
    clip->addAction(commands_->Add({.id = "clip.work_clear",
                                    .text = tr("Clear the work area"),
                                    .run = [this] { SetWorkArea(std::nullopt); },
                                    .refusal = [this]() -> std::optional<QString> {
                                        if (!work_area_) return tr("There is no work area");
                                        return std::nullopt;
                                    }}));
    clip->addAction(commands_->Add({.id = "clip.trim",
                                    .text = tr("&Trim the clip to the work area"),
                                    .keys = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X),
                                    .run = [this] { TrimClipToWorkArea(); },
                                    .refusal = work_area}));
    clip->addAction(commands_->Add({.id = "clip.extract",
                                    .text = tr("E&xtract the work area"),
                                    .run = [this] { ExtractWorkArea(); },
                                    .refusal = work_area}));
    clip->addAction(commands_->Add({.id = "clip.lift",
                                    .text = tr("&Lift the work area"),
                                    .run = [this] { LiftWorkArea(); },
                                    .refusal = work_area}));
    clip->addSeparator();
    clip->addAction(commands_->Add({.id = "clip.name_export",
                                    .text = tr("Name the &export of this sprite..."),
                                    .run = [this] { NameShownSpriteExport(); },
                                    .refusal = [this]() -> std::optional<QString> {
                                        if (const auto refused = NeedsAnimation()) return refused;
                                        if (!clip_.sprite) return tr("Open a sprite first");
                                        return std::nullopt;
                                    }}));
}

void Window::AddToolsMenu() {
    QMenu* tools = AddMenu(nullptr, tr("&Tools"));
    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    const auto add = [&](const QString& id, const QString& text, const QKeySequence& keys,
                         Tool tool) {
        QAction* action =
            commands_->Add({.id = id, .text = text, .keys = keys, .run = [this, id, tool] {
                                viewport_->SetTool(tool);
                                commands_->Action(id)->setChecked(true);
                            }});
        action->setCheckable(true);
        group->addAction(action);
        tools->addAction(action);
    };
    add("tool.select", tr("&Select"), QKeySequence(Qt::Key_V), Tool::Select);
    add("tool.anchor", tr("&Anchor"), QKeySequence(Qt::Key_A), Tool::Anchor);
    add("tool.pan", tr("&Pan"), QKeySequence(Qt::Key_H), Tool::Pan);
    add("tool.zoom", tr("&Zoom"), QKeySequence(Qt::Key_Z), Tool::Zoom);
    add("tool.sketch", tr("&Motion sketch"), QKeySequence(Qt::Key_Y), Tool::Sketch);
    commands_->Run("tool.select");
}

void Window::AddViewMenu() {
    QMenu* view = AddMenu(nullptr, tr("&View"));
    view->addAction(commands_->AddToggle({.id = "view.snap",
                                          .text = tr("&Snap while moving on stage"),
                                          .checked = QSettings().value(kSnapKey, true).toBool(),
                                          .set = [this](bool on) {
                                              QSettings().setValue(kSnapKey, on);
                                              viewport_->SetSnapping(on);
                                          }}));
    viewport_->SetSnapping(commands_->Action("view.snap")->isChecked());
    view->addAction(commands_->AddToggle({.id = "view.rulers",
                                          .text = tr("&Rulers"),
                                          .keys = QKeySequence(Qt::CTRL | Qt::Key_R),
                                          .checked = QSettings().value(kRulersKey, false).toBool(),
                                          .set = [this](bool on) {
                                              QSettings().setValue(kRulersKey, on);
                                              viewport_->SetRulers(on);
                                          }}));
    viewport_->SetRulers(commands_->Action("view.rulers")->isChecked());
    view->addAction(commands_->Add({.id = "view.clear_guides",
                                    .text = tr("Clear &guides"),
                                    .run = [this] { viewport_->ClearGuides(); }}));
    onion_action_ = commands_->AddToggle({.id = "view.onion",
                                          .text = tr("&Onion skin"),
                                          .checked = QSettings().value(kOnionKey, false).toBool(),
                                          .set = [this](bool on) {
                                              QSettings().setValue(kOnionKey, on);
                                              if (!on) {
                                                  viewport_->ShowGhosts({});
                                                  return;
                                              }
                                              if (host_.Running() && !animation_name_.empty())
                                                  RenderFrame();
                                          }});
    view->addAction(onion_action_);
    path_action_ = commands_->AddToggle({.id = "view.path",
                                         .text = tr("Motion &path"),
                                         .checked = QSettings().value(kPathKey, true).toBool(),
                                         .set = [this](bool on) {
                                             QSettings().setValue(kPathKey, on);
                                             ShowFrame();
                                         }});
    view->addAction(path_action_);
    background_action_ =
        commands_->AddToggle({.id = "view.background",
                              .text = tr("Draw the &background colour"),
                              .checked = QSettings().value(kBackgroundKey, false).toBool(),
                              .set = [this](bool drawn) { DrawBackground(drawn); }});
    view->addAction(background_action_);
    view->addSeparator();
    view->addAction(commands_->Add({.id = "view.fit_stage",
                                    .text = tr("&Fit the stage in the view"),
                                    .keys = QKeySequence(Qt::CTRL | Qt::Key_0),
                                    .run = [this] { viewport_->FitStage(); }}));
    view->addAction(commands_->Add({.id = "view.zoom_in",
                                    .text = tr("Zoom the stage &in"),
                                    .keys = QKeySequence(Qt::CTRL | Qt::Key_Plus),
                                    .run = [this] { viewport_->ZoomStep(1); }}));
    view->addAction(commands_->Add({.id = "view.zoom_out",
                                    .text = tr("Zoom the stage &out"),
                                    .keys = QKeySequence(Qt::CTRL | Qt::Key_Minus),
                                    .run = [this] { viewport_->ZoomStep(-1); }}));
    view->addAction(commands_->Add({.id = "graph.fit_all",
                                    .text = tr("Fit the &graph to every shown property"),
                                    .run = [this] { graph_->Fit(false); }}));
    view->addAction(commands_->Add({.id = "graph.fit_keys",
                                    .text = tr("Fit the graph to the &keyframes"),
                                    .run = [this] { graph_->Fit(true); }}));
    view->addAction(commands_->Add({.id = "view.timeline_in",
                                    .text = tr("Zoom the timeline &in"),
                                    .keys = QKeySequence(Qt::Key_Equal),
                                    .run = [this] { timeline_->ZoomIn(); }}));
    view->addAction(commands_->Add({.id = "view.timeline_out",
                                    .text = tr("Zoom the timeline ou&t"),
                                    .keys = QKeySequence(Qt::Key_Minus),
                                    .run = [this] { timeline_->ZoomOut(); }}));
    view->addSeparator();
    view->addAction(
        commands_->Add({.id = "view.history", .text = tr("Show the &history"), .run = [this] {
                            history_dock_->toggleView(true);
                            history_dock_->setAsCurrentTab();
                        }}));
    QMenu* panels = view->addMenu(tr("&Panels"));
    for (ads::CDockWidget* dock : docks_->dockWidgetsMap())
        panels->addAction(dock->toggleViewAction());
}

void Window::AddPlaybackMenu() {
    QMenu* play = AddMenu(nullptr, tr("&Playback"));
    const auto animation = [this] { return NeedsAnimation(); };
    const auto depth = [this] { return NeedsDepth(); };
    play_action_ = commands_->Add({.id = "play.toggle",
                                   .text = tr("&Play"),
                                   .keys = QKeySequence(Qt::Key_Space),
                                   .run = [this] { TogglePlay(); },
                                   .refusal = animation});
    play_action_->setShortcutContext(Qt::ApplicationShortcut);
    play->addAction(play_action_);
    loop_action_ =
        commands_->AddToggle({.id = "play.loop",
                              .text = tr("&Loop"),
                              .checked = QSettings().value(kLoopKey, true).toBool(),
                              .set = [](bool on) { QSettings().setValue(kLoopKey, on); }});
    play->addAction(loop_action_);
    play->addAction(commands_->Action("tool.sketch"));
    play->addSeparator();
    const auto step = [&](const QString& id, const QString& text, const QKeySequence& keys,
                          std::function<void()> run, Refusal refusal) {
        play->addAction(commands_->Add({.id = id,
                                        .text = text,
                                        .keys = keys,
                                        .run = std::move(run),
                                        .refusal = std::move(refusal)}));
    };
    step(
        "play.previous", tr("&Previous frame"), QKeySequence(Qt::Key_PageUp),
        [this] { JumpToFrame(static_cast<int64_t>(frame_) - 1); }, animation);
    step(
        "play.next", tr("&Next frame"), QKeySequence(Qt::Key_PageDown),
        [this] { JumpToFrame(static_cast<int64_t>(frame_) + 1); }, animation);
    step(
        "play.first", tr("&First frame"), QKeySequence(Qt::Key_Home), [this] { JumpToFrame(0); },
        animation);
    step(
        "play.last", tr("L&ast frame"), QKeySequence(Qt::Key_End),
        [this] { JumpToFrame(static_cast<int64_t>(ClipFrameCount()) - 1); }, animation);
    step(
        "play.go_to", tr("&Go to frame..."), QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_J),
        [this] { GoToFrame(); }, Refusal());
    step(
        "play.previous_change", tr("Previous &change on the depth"), QKeySequence(Qt::Key_J),
        [this] { StepToMark(Document::Direction::Back); }, depth);
    step(
        "play.next_change", tr("Next c&hange on the depth"), QKeySequence(Qt::Key_K),
        [this] { StepToMark(Document::Direction::Forward); }, depth);
}

void Window::ShowRefusal(const QString& reason) {
    notices_->Say(reason, false);
}

void Window::ShowResult(const QString& what, bool undoable) {
    notices_->Say(what, undoable);
}

}
