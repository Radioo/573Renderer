#include "editor_window.h"

#include "editor_files.h"
#include "editor_filter.h"
#include "editor_graph.h"
#include "editor_host.h"
#include "editor_layout.h"
#include "editor_timeline.h"
#include "editor_viewport.h"

#include "document/animation_settings.h"
#include "document/authored.h"
#include "document/camera_edit.h"
#include "document/clip_edit.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/history.h"
#include "document/inspector.h"
#include "document/library_call.h"
#include "document/placement_edit.h"
#include "document/project.h"

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QImage>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QSettings>
#include <QSize>
#include <QStatusBar>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVariant>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Editor {

namespace {

constexpr int kPathRole = Qt::UserRole;
constexpr int kEditsRole = Qt::UserRole + 1;
constexpr int kResizeDelayMs = 120;

ads::CDockWidget* MakePanel(const QString& title, QWidget* content) {
    auto* dock = new ads::CDockWidget(title);
    dock->setWidget(content);
    return dock;
}

QString RoleText(Document::Role role) {
    const std::string_view name = Document::RoleName(role);
    return QString::fromUtf8(name.data(), static_cast<int>(name.size()));
}

QString KindText(const Document::Node& node) {
    if (node.kind == Ifs::EntryKind::Directory) return QObject::tr("directory");
    if (node.kind == Ifs::EntryKind::Special) return QObject::tr("special");
    if (node.super_index)
        return QObject::tr("in super image %1").arg(static_cast<int>(*node.super_index));
    return RoleText(node.role);
}

void AddNodes(const std::vector<Document::Node>& nodes, QTreeWidget* tree,
              QTreeWidgetItem* parent) {
    for (const Document::Node& node : nodes) {
        auto* item = parent == nullptr ? new QTreeWidgetItem(tree) : new QTreeWidgetItem(parent);
        item->setText(0, QString::fromStdString(node.name));
        item->setText(1, KindText(node));
        if (node.kind != Ifs::EntryKind::Directory)
            item->setText(2, QString::number(node.stored_size));
        item->setData(0, kPathRole, QString::fromStdString(node.path));
        AddNodes(node.children, tree, item);
    }
}

int CountNodes(const std::vector<Document::Node>& nodes) {
    int total = 0;
    for (const Document::Node& node : nodes)
        total += 1 + CountNodes(node.children);
    return total;
}

const Document::Node* FirstAnimation(const std::vector<Document::Node>& nodes) {
    for (const Document::Node& node : nodes) {
        if (node.role == Document::Role::Animation) return &node;
        if (const Document::Node* found = FirstAnimation(node.children); found != nullptr)
            return found;
    }
    return nullptr;
}

QTreeWidgetItem* ItemForPath(QTreeWidget* tree, const QString& path) {
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        if ((*it)->data(0, kPathRole).toString() == path) return *it;
    }
    return nullptr;
}

}

Window::Window() {
    BuildPanels();
    BuildMenus();
    RefreshState();
    resize(1600, 900);
    RestoreLayout(*this, *docks_);
    const QString game_dir = QSettings().value(kGameDirKey).toString();
    if (game_dir.isEmpty()) {
        statusBar()->showMessage(tr("No preview host running"));
        return;
    }
    StartHost(game_dir);
}

Window::~Window() = default;

void Window::BuildPanels() {
    docks_ = new ads::CDockManager(this);

    viewport_ = new Viewport;
    package_tree_ = new QTreeWidget;
    package_tree_->setObjectName("package");
    package_tree_->setHeaderLabels({tr("Entry"), tr("Kind"), tr("Size")});
    connect(package_tree_, &QTreeWidget::itemSelectionChanged, this, &Window::ShowSelectedEntry);
    package_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(package_tree_, &QTreeWidget::customContextMenuRequested, this,
            [this](const QPoint& at) { ShowPackageMenu(package_tree_->mapToGlobal(at)); });

    inspector_ = new QTableWidget(0, 2);
    inspector_->setHorizontalHeaderLabels({tr("Field"), tr("Value")});
    inspector_->verticalHeader()->setVisible(false);
    inspector_->horizontalHeader()->setStretchLastSection(true);
    inspector_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(inspector_, &QTableWidget::itemChanged, this, &Window::ApplyFieldEdit);
    inspector_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(inspector_, &QTableWidget::customContextMenuRequested, this,
            [this](const QPoint& at) { ShowInspectorMenu(at); });

    timeline_ = new Timeline;
    connect(timeline_, &Timeline::FrameChosen, this, &Window::SeekTo);
    connect(timeline_, &Timeline::DepthChosen, this, &Window::ChooseDepth);
    connect(timeline_, &Timeline::DepthsChosen, this, &Window::ChooseDepths);
    connect(timeline_, &Timeline::MenuRequested, this, &Window::ShowTimelineMenu);
    connect(timeline_, &Timeline::KeyChosen, this, &Window::FocusKey);
    connect(timeline_, &Timeline::KeysShifted, this, &Window::ShiftSelectedKeys);
    connect(timeline_, &Timeline::SpanMoved, this, &Window::MoveSpanInTime);
    connect(timeline_, &Timeline::SpanTrimmed, this, &Window::TrimSpanOnTimeline);
    connect(timeline_, &Timeline::VisibilityToggled, this, &Window::ToggleHidden);
    connect(timeline_, &Timeline::LockToggled, this, &Window::ToggleLocked);
    AddKeyActions();
    connect(timeline_, &Timeline::KeyMenuRequested, this, &Window::ShowKeyMenu);
    connect(timeline_, &Timeline::CharacterDropped, this, &Window::DropCharacterOnTimeline);
    connect(timeline_, &Timeline::LabelMoved, this, &Window::MoveLabelTo);
    graph_ = new GraphEditor;
    connect(graph_, &GraphEditor::FrameChosen, this, &Window::SeekTo);
    connect(graph_, &GraphEditor::KeyChosen, this, &Window::ChooseKey);
    connect(graph_, &GraphEditor::KeyMoved, this, &Window::ApplyGraphMove);
    auto* timeline_area = new QScrollArea;
    timeline_area->setWidget(timeline_);
    timeline_area->setWidgetResizable(true);
    QWidget* timeline_panel = BuildTimelinePanel(timeline_area);

    play_timer_ = new QTimer(this);
    connect(play_timer_, &QTimer::timeout, this, &Window::StepPlayback);

    resize_timer_ = new QTimer(this);
    resize_timer_->setSingleShot(true);
    resize_timer_->setInterval(kResizeDelayMs);
    connect(resize_timer_, &QTimer::timeout, this, &Window::ResizeViewport);
    connect(viewport_, &Viewport::Resized, this, [this](int, int) { resize_timer_->start(); });
    connect(viewport_, &Viewport::ZoomChanged, this, [this] { resize_timer_->start(); });
    connect(viewport_, &Viewport::Picked, this, &Window::PickOnStage);
    connect(viewport_, &Viewport::Dragged, this, &Window::MoveOnStage);
    connect(viewport_, &Viewport::Reshaped, this, &Window::ReshapeOnStage);
    connect(viewport_, &Viewport::CharacterDropped, this, &Window::PlaceDroppedCharacter);
    connect(viewport_, &Viewport::DepthsBanded, this, &Window::ChooseDepths);

    ads::CDockAreaWidget* centre = docks_->setCentralWidget(MakePanel(tr("Viewport"), viewport_));
    ads::CDockAreaWidget* package_area = docks_->addDockWidget(
        ads::LeftDockWidgetArea,
        MakePanel(tr("Package"), WithFilter(package_tree_, package_filter_ = new QLineEdit)),
        centre);
    QTreeWidget* library = BuildLibrary();
    docks_->addDockWidget(
        ads::BottomDockWidgetArea,
        MakePanel(tr("Library"), WithFilter(library, library_filter_ = new QLineEdit)),
        package_area);
    ads::CDockAreaWidget* inspector_area = docks_->addDockWidget(
        ads::RightDockWidgetArea, MakePanel(tr("Inspector"), inspector_), centre);
    docks_->addDockWidget(ads::BottomDockWidgetArea, MakePanel(tr("History"), BuildHistory()),
                          inspector_area);
    package_filter_->setObjectName("package_filter");
    library_filter_->setObjectName("library_filter");
    ads::CDockWidget* timeline_dock = MakePanel(tr("Timeline"), timeline_panel);
    ads::CDockAreaWidget* timing_area =
        docks_->addDockWidget(ads::BottomDockWidgetArea, timeline_dock, centre);
    docks_->addDockWidget(ads::CenterDockWidgetArea, MakePanel(tr("Graph"), graph_), timing_area);
    timeline_dock->setAsCurrentTab();
}

void Window::BuildMenus() {
    QMenu* file = menuBar()->addMenu(tr("&File"));
    QAction* open = file->addAction(tr("&Open IFS..."));
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, &Window::ChooseDocument);
    QAction* save = file->addAction(tr("&Save"));
    save->setShortcut(QKeySequence::Save);
    connect(save, &QAction::triggered, this, [this] { Save(); });
    QAction* save_as = file->addAction(tr("Save &as..."));
    save_as->setShortcut(QKeySequence::SaveAs);
    connect(save_as, &QAction::triggered, this, [this] { SaveAs(); });
    QAction* save_frame = file->addAction(tr("Save the &frame as PNG..."));
    save_frame->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    connect(save_frame, &QAction::triggered, this, &Window::SaveFrameAs);
    QAction* save_frames = file->addAction(tr("Save the work area as PNG f&rames..."));
    save_frames->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_S));
    connect(save_frames, &QAction::triggered, this, &Window::SaveFramesAs);
    file->addSeparator();
    create_project_action_ = file->addAction(tr("&New project..."));
    connect(create_project_action_, &QAction::triggered, this, &Window::CreateProject);
    QAction* open_project = file->addAction(tr("Open &project..."));
    connect(open_project, &QAction::triggered, this, &Window::ChooseProject);
    close_project_action_ = file->addAction(tr("&Close project"));
    connect(close_project_action_, &QAction::triggered, this, &Window::CloseProject);
    export_action_ = file->addAction(tr("&Export into the IFS"));
    export_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(export_action_, &QAction::triggered, this, &Window::ExportToPackage);
    file->addSeparator();
    QAction* choose = file->addAction(tr("Choose &game install..."));
    connect(choose, &QAction::triggered, this, &Window::ChooseGameDirectory);
    file->addSeparator();
    QAction* quit = file->addAction(tr("&Quit"));
    connect(quit, &QAction::triggered, this, &QWidget::close);

    QMenu* play = menuBar()->addMenu(tr("&Playback"));
    play_action_ = play->addAction(tr("&Play"));
    play_action_->setShortcut(QKeySequence(Qt::Key_Space));
    play_action_->setShortcutContext(Qt::ApplicationShortcut);
    connect(play_action_, &QAction::triggered, this, &Window::TogglePlay);
    loop_action_ = play->addAction(tr("&Loop"));
    loop_action_->setCheckable(true);
    loop_action_->setChecked(QSettings().value(kLoopKey, true).toBool());
    connect(loop_action_, &QAction::toggled, this,
            [this](bool on) { QSettings().setValue(kLoopKey, on); });
    background_action_ = play->addAction(tr("Draw the &background colour"));
    background_action_->setCheckable(true);
    background_action_->setChecked(QSettings().value(kBackgroundKey, false).toBool());
    connect(background_action_, &QAction::toggled, this, &Window::DrawBackground);
    AddStepActions(play);

    AddViewMenu();

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    undo_action_ = edit->addAction(tr("&Undo"));
    undo_action_->setShortcut(QKeySequence::Undo);
    connect(undo_action_, &QAction::triggered, this, &Window::Undo);
    redo_action_ = edit->addAction(tr("&Redo"));
    redo_action_->setShortcut(QKeySequence::Redo);
    connect(redo_action_, &QAction::triggered, this, &Window::Redo);
    edit->addSeparator();
    AddAlignMenu(edit);
    AddArrangeMenu(edit);
    AddPlayheadMenu(edit);
    QAction* split = edit->addAction(tr("&Split depth at the playhead"));
    split->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(split, &QAction::triggered, this, [this] {
        if (depth_) SplitDepthAt(static_cast<uint16_t>(*depth_), frame_);
    });
    QAction* duplicate = edit->addAction(tr("&Duplicate depth"));
    duplicate->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(duplicate, &QAction::triggered, this, &Window::DuplicateChosenDepth);
    QAction* trim = edit->addAction(tr("&Trim the clip to the work area"));
    trim->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X));
    connect(trim, &QAction::triggered, this, &Window::TrimClipToWorkArea);
    QAction* extract = edit->addAction(tr("E&xtract the work area"));
    connect(extract, &QAction::triggered, this, &Window::ExtractWorkArea);
    QAction* lift = edit->addAction(tr("&Lift the work area"));
    connect(lift, &QAction::triggered, this, &Window::LiftWorkArea);
}

void Window::ChooseGameDirectory() {
    QSettings settings;
    const QString start = settings.value(kGameDirKey).toString();
    const QString dir =
        QFileDialog::getExistingDirectory(this, tr("Choose the game install"), start);
    if (dir.isEmpty()) return;
    settings.setValue(kGameDirKey, dir);
    StartHost(dir);
}

void Window::StartHost(const QString& game_dir) {
    const std::vector<std::string> search = {
        QApplication::applicationDirPath().toStdString(),
        QDir(QApplication::applicationDirPath() + "/../build").absolutePath().toStdString()};
    const std::string host_exe = FindPreviewHost(search);
    if (host_exe.empty()) {
        ReportProblem(tr("preview_host.exe was not found next to the editor"));
        return;
    }
    statusBar()->showMessage(tr("Booting the preview host..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto started = host_.Start(host_exe, game_dir.toStdString(), TargetBuild());
    QApplication::restoreOverrideCursor();
    if (!started) {
        ReportProblem(QString::fromStdString(started.error()));
        statusBar()->showMessage(tr("No preview host running"));
        return;
    }
    statusBar()->showMessage(tr("Preview host running on %1").arg(game_dir));
    DrawBackground(background_action_->isChecked());
    if (!animation_name_.empty()) ShowAnimation(animation_name_);
}

void Window::ChooseDocument() {
    if (!OfferToSave()) return;
    const QSettings settings;
    const QString start = settings.value(kDocumentDirKey).toString();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open an IFS"), start,
                                                      tr("IFS files (*.ifs);;All files (*)"));
    if (path.isEmpty()) return;
    OpenDocument(path);
}

void Window::OpenDocument(const QString& path) {
    StopPlayback();
    const std::vector<uint8_t> bytes = ReadFileBytes(path);
    if (bytes.empty()) {
        ReportProblem(tr("%1 is empty or cannot be read").arg(path));
        return;
    }
    auto file = Document::File::Open(bytes);
    if (!file) {
        ReportProblem(QString::fromStdString(file.error()));
        return;
    }
    QSettings().setValue(kDocumentDirKey, QFileInfo(path).absolutePath());
    file_ = std::move(*file);
    history_.Clear();
    copied_span_.reset();
    viewport_->ClearGuides();
    hidden_.clear();
    locked_.clear();
    document_path_ = path;
    package_name_ = QFileInfo(path).completeBaseName().toStdString();
    CloseAnimation();
    FillTree();
    if (const Document::Node* first = FirstAnimation(file_->Nodes()); first != nullptr)
        SelectEntry(QString::fromStdString(first->path));
    const std::vector<std::string>& problems = file_->Problems();
    if (problems.empty()) {
        statusBar()->showMessage(tr("%1 entries").arg(CountNodes(file_->Nodes())));
        return;
    }
    statusBar()->showMessage(tr("%1 problems in the package, the first is: %2")
                                 .arg(problems.size())
                                 .arg(QString::fromStdString(problems.front())));
}

void Window::FillTree() {
    package_tree_->clear();
    AddNodes(file_->Nodes(), package_tree_, nullptr);
    for (int column = 0; column < package_tree_->columnCount(); column++)
        package_tree_->resizeColumnToContents(column);
    ApplyFilter(*package_tree_, package_filter_->text());
}

void Window::FillInspector(const std::vector<Document::InspectedRow>& rows) {
    filling_inspector_ = true;
    inspector_->clearContents();
    inspector_->setRowCount(static_cast<int>(rows.size()));
    for (std::size_t row = 0; row < rows.size(); row++) {
        const Document::InspectedRow& shown = rows[row];
        auto* name = new QTableWidgetItem(QString::fromStdString(shown.field.name));
        name->setFlags(Qt::ItemIsEnabled);
        auto* value = new QTableWidgetItem(QString::fromStdString(shown.field.value));
        value->setData(kEditsRole, static_cast<int>(shown.edits));
        value->setFlags(shown.edits == Document::EditTarget::None
                            ? Qt::ItemIsEnabled
                            : Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        inspector_->setItem(static_cast<int>(row), 0, name);
        inspector_->setItem(static_cast<int>(row), 1, value);
    }
    inspector_->resizeColumnToContents(0);
    filling_inspector_ = false;
}

std::vector<Document::InspectedRow>
Window::ReadOnlyRows(const std::vector<Document::Field>& fields) {
    std::vector<Document::InspectedRow> rows;
    rows.reserve(fields.size());
    for (const Document::Field& field : fields)
        rows.push_back(Document::InspectedRow{.field = field, .edits = Document::EditTarget::None});
    return rows;
}

void Window::ShowSelectedEntry() {
    const QList<QTreeWidgetItem*> selected = package_tree_->selectedItems();
    if (selected.isEmpty() || !file_) {
        FillInspector({});
        return;
    }
    const QString path = selected.front()->data(0, kPathRole).toString();
    const auto details = file_->Describe(path.toStdString());
    if (!details) {
        FillInspector(ReadOnlyRows({Document::Field{.name = "Problem", .value = details.error()}}));
        return;
    }
    depth_.reset();
    FillInspector(ReadOnlyRows(Document::Fields(*details)));
    if (details->role != Document::Role::Animation) return;
    animation_path_ = path.toStdString();
    ShowAnimation(details->name);
}

void Window::ChooseDepth(uint32_t depth) {
    depth_ = depth;
    selected_depths_ = {static_cast<uint16_t>(depth)};
    ShowFrame();
}

void Window::ChooseDepths(std::vector<uint16_t> depths) {
    if (depths.empty()) {
        depth_.reset();
    } else {
        depth_ = depths.back();
    }
    selected_depths_ = std::move(depths);
    ShowFrame();
}

void Window::ShowFrame() {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    const Document::AuthoredDepth* owned =
        depth_ ? AuthoredAt(static_cast<uint16_t>(*depth_), frame_) : nullptr;
    ShowKeysForDepth(owned);
    timeline_->SelectDepths(SelectedDepths());
    FillInspector(Document::InspectFrame(
        *animation, Document::Selection{
                        .depth = depth_ ? std::optional<uint16_t>(static_cast<uint16_t>(*depth_))
                                        : std::nullopt,
                        .frame = frame_,
                        .owned = owned,
                        .key_property = key_property_.toStdString(),
                        .key_frame = key_frame_,
                        .clip = clip_}));
    UpdateOutlines(*animation);
}

bool Window::EditAnimation(const QString& name, const AnimationChange& change) {
    if (!file_ || animation_path_.empty()) return false;
    StopPlayback();
    auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return false;
    }
    const auto changed = change(*animation);
    if (!changed) {
        ReportProblem(QString::fromStdString(changed.error()));
        ShowFrame();
        return false;
    }
    Document::File before = *file_;
    const auto written = file_->WriteAnimation(animation_path_, *animation);
    if (!written) {
        ReportProblem(QString::fromStdString(written.error()));
        ShowFrame();
        return false;
    }
    history_.Record(name.toStdString(),
                    Document::Snapshot{.file = std::move(before), .authored = authored_});
    RefreshState();
    Reload();
    ShowFrame();
    return true;
}

void Window::ApplyFieldEdit(QTableWidgetItem* item) {
    if (filling_inspector_ || item == nullptr || item->column() != 1) return;
    if (!file_ || animation_path_.empty()) return;
    const QTableWidgetItem* name = inspector_->item(item->row(), 0);
    if (name == nullptr) return;
    const auto edits = static_cast<Document::EditTarget>(item->data(kEditsRole).toInt());
    const std::string field = name->text().toStdString();
    const std::string value = item->text().toStdString();
    const uint32_t frame = frame_;
    const Document::ClipId clip = clip_;

    if (edits == Document::EditTarget::KeyFilter) {
        if (!ApplyKeyFilterEdit(name->text(), item->text())) ShowFrame();
        return;
    }
    if (edits == Document::EditTarget::KeyValue) {
        if (!ApplyKeyEdit(item->text())) ShowFrame();
        return;
    }
    if (edits == Document::EditTarget::Camera) {
        EditAnimation(tr("%1 on frame %2").arg(name->text()).arg(frame),
                      [clip, field, value, frame](AfpAnimation::Animation& animation) {
                          return Document::EditCameraField(animation, clip, frame, field, value);
                      });
        return;
    }
    if (edits == Document::EditTarget::Animation) {
        EditAnimation(name->text(), [field, value](AfpAnimation::Animation& animation) {
            return Document::SetAnimationSetting(animation, field, value);
        });
        return;
    }
    if (!depth_) return;
    const uint16_t depth = static_cast<uint16_t>(*depth_);
    if (edits == Document::EditTarget::CallArgument) {
        const std::optional<std::size_t> argument = Document::CallArgumentIndex(field);
        if (!argument) return;
        const std::size_t index = *argument;
        EditAnimation(tr("%1 on depth %2").arg(name->text()).arg(*depth_),
                      [clip, index, value, depth, frame](AfpAnimation::Animation& animation) {
                          return Document::EditCallArgument(animation, clip, depth, frame, index,
                                                            value);
                      });
        return;
    }
    if (edits != Document::EditTarget::Placement) return;
    EditAnimation(tr("%1 on depth %2").arg(name->text()).arg(*depth_),
                  [clip, field, value, depth, frame](AfpAnimation::Animation& animation) {
                      return Document::EditPlacementField(animation, clip, depth, frame, field,
                                                          value);
                  });
}

bool Window::EditDocument(const QString& name, const DocumentChange& change) {
    if (!file_) return false;
    StopPlayback();
    Document::File before = *file_;
    const auto changed = change(*file_);
    if (!changed) {
        file_ = std::move(before);
        ReportProblem(QString::fromStdString(changed.error()));
        return false;
    }
    history_.Record(name.toStdString(),
                    Document::Snapshot{.file = std::move(before), .authored = authored_});
    RefreshState();
    FillTree();
    Reload();
    return true;
}

void Window::CloseAnimation() {
    animation_name_.clear();
    animation_path_.clear();
    depth_.reset();
    frame_count_ = 0;
    frame_ = 0;
    RefreshState();
    viewport_->ShowMessage(tr("No animation selected"));
    timeline_->Clear();
    library_->clear();
}

void Window::SelectEntry(const QString& path) {
    QTreeWidgetItem* item = ItemForPath(package_tree_, path);
    if (item != nullptr) package_tree_->setCurrentItem(item);
}

void Window::ShowPackageMenu(const QPoint& where) {
    if (!file_) return;
    const QList<QTreeWidgetItem*> selected = package_tree_->selectedItems();
    const QString path =
        selected.isEmpty() ? QString() : selected.front()->data(0, kPathRole).toString();
    const auto details =
        path.isEmpty()
            ? Support::Expected<Document::Details, std::string>(Support::Unexpected(std::string()))
            : file_->Describe(path.toStdString());
    const bool is_image = details && details->role == Document::Role::Texture;
    const bool is_animation = details && details->role == Document::Role::Animation;
    const QString name = details ? QString::fromStdString(details->name) : QString();

    QMenu menu(this);
    QAction* new_animation = menu.addAction(tr("New animation..."));
    QAction* add_image = menu.addAction(tr("Add an image from a file..."));
    QAction* own_image =
        project_ ? menu.addAction(tr("Add an image the project owns...")) : nullptr;
    QAction* save_image = is_image ? menu.addAction(tr("Save %1 as PNG...").arg(name)) : nullptr;
    const QString replace_text =
        is_image ? tr("Replace %1 with a picture...") : tr("Replace %1...");
    QAction* replace = path.isEmpty() ? nullptr : menu.addAction(replace_text.arg(name));
    QAction* rename = is_animation ? menu.addAction(tr("Rename %1...").arg(name)) : nullptr;
    QAction* duplicate = is_animation ? menu.addAction(tr("Duplicate %1...").arg(name)) : nullptr;
    QAction* tidy =
        is_animation ? menu.addAction(tr("Remove unused definitions from %1").arg(name)) : nullptr;
    QAction* remove = path.isEmpty() ? nullptr : menu.addAction(tr("Remove %1").arg(name));
    const QAction* chosen = menu.exec(where);
    if (chosen == nullptr) return;

    if (chosen == new_animation) {
        AddNewAnimation();
        return;
    }
    if (chosen == own_image) {
        AddProjectImage();
        return;
    }
    if (chosen == save_image) {
        SaveImageAs(name);
        return;
    }
    if (chosen == add_image) {
        AddImageFromFile();
        return;
    }
    if (chosen == replace && is_image) {
        ReplaceImageWithPicture(name);
        return;
    }
    if (chosen == replace) {
        const QString file = QFileDialog::getOpenFileName(this, tr("Replace an entry"), QString());
        if (file.isEmpty()) return;
        std::vector<uint8_t> bytes = ReadFileBytes(file);
        if (bytes.empty()) {
            ReportProblem(tr("%1 is empty or cannot be read").arg(file));
            return;
        }
        const std::string target = path.toStdString();
        EditDocument(tr("Replace %1").arg(name), [target, bytes](Document::File& document) {
            return document.ReplaceEntry(target, bytes);
        });
        return;
    }
    if (chosen == rename) {
        RenameAnimationEntry(path.toStdString(), name);
        return;
    }
    if (chosen == duplicate) {
        DuplicateAnimationEntry(path.toStdString(), name);
        return;
    }
    if (chosen == tidy) {
        RemoveUnusedDefinitionsFrom(path.toStdString(), name);
        return;
    }
    if (chosen == remove && is_animation) {
        RemoveAnimation(path.toStdString(), name);
        return;
    }
    if (chosen == remove) {
        const std::string target = path.toStdString();
        const std::string logical = name.toStdString();
        EditDocument(
            tr("Remove %1").arg(name), [target, logical, is_image](Document::File& document) {
                return is_image ? document.RemoveImage(logical) : document.RemoveEntry(target);
            });
    }
}

void Window::Undo() {
    if (!file_) return;
    auto restored = history_.Undo(Document::Snapshot{.file = *file_, .authored = authored_});
    if (!restored) return;
    file_ = std::move(restored->file);
    authored_ = std::move(restored->authored);
    SaveProject();
    ShowRestored();
}

void Window::Redo() {
    if (!file_) return;
    auto restored = history_.Redo(Document::Snapshot{.file = *file_, .authored = authored_});
    if (!restored) return;
    file_ = std::move(restored->file);
    authored_ = std::move(restored->authored);
    SaveProject();
    ShowRestored();
}

void Window::ShowRestored() {
    RefreshState();
    ShowFrame();
    Reload();
}

void Window::ResizeViewport() {
    if (!host_.Running() || animation_name_.empty()) return;
    const QSize fitted = viewport_->FittedSize(viewport_->size());
    const auto resized =
        host_.Resize(static_cast<uint32_t>(fitted.width()), static_cast<uint32_t>(fitted.height()));
    if (!resized) {
        ReportOnce(QString::fromStdString(resized.error()));
        return;
    }
    RenderFrame();
}

void Window::RenderFrame() {
    auto read = ReadFrame();
    if (!read) {
        ReportOnce(QString::fromStdString(read.error()));
        return;
    }
    const PreviewClient::Frame* frame = &read->frame;
    stage_size_ =
        QSize(static_cast<int>(frame->stage_width), static_cast<int>(frame->stage_height));
    viewport_->ShowFrame(std::move(read->image), stage_size_);
    const QSize shown(static_cast<int>(frame->width), static_cast<int>(frame->height));
    if (shown != viewport_->FittedSize(viewport_->size())) resize_timer_->start();
    timeline_->SetFrame(frame->frame);
    statusBar()->showMessage(tr("Frame %1 of %2").arg(frame->frame).arg(frame_count_));
    last_error_.clear();
    if (onion_action_ != nullptr && onion_action_->isChecked() && !Playing())
        ShowGhostsAround(frame->frame);
}

bool Window::Save() {
    if (!file_) return true;
    if (document_path_.isEmpty()) return SaveAs();
    const auto encoded = file_->Encode();
    if (!encoded) {
        ReportProblem(QString::fromStdString(encoded.error()));
        return false;
    }
    if (!WriteFileBytes(document_path_, *encoded)) {
        ReportProblem(tr("%1 could not be written").arg(document_path_));
        return false;
    }
    history_.MarkSaved();
    RefreshState();
    statusBar()->showMessage(tr("Saved %1").arg(document_path_));
    return true;
}

bool Window::SaveAs() {
    if (!file_) return true;
    const QString path = QFileDialog::getSaveFileName(this, tr("Save the IFS"), document_path_,
                                                      tr("IFS files (*.ifs);;All files (*)"));
    if (path.isEmpty()) return false;
    document_path_ = path;
    return Save();
}

bool Window::OfferToSave() {
    if (!file_ || history_.Saved()) return true;
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("IFS Editor"),
        tr("%1 has unsaved changes.").arg(QFileInfo(document_path_).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Discard) return true;
    return Save();
}

void Window::RefreshState() {
    shape_bounds_path_.clear();
    create_project_action_->setEnabled(file_.has_value() && !project_);
    close_project_action_->setEnabled(project_.has_value());
    export_action_->setEnabled(project_.has_value() &&
                               (!authored_.empty() || (project_ && !project_->images.empty())));
    undo_action_->setEnabled(history_.CanUndo());
    redo_action_->setEnabled(history_.CanRedo());
    undo_action_->setText(history_.CanUndo()
                              ? tr("&Undo %1").arg(QString::fromStdString(history_.UndoName()))
                              : tr("&Undo"));
    redo_action_->setText(history_.CanRedo()
                              ? tr("&Redo %1").arg(QString::fromStdString(history_.RedoName()))
                              : tr("&Redo"));
    FillHistory();
    if (!file_) {
        setWindowTitle(tr("IFS Editor"));
        return;
    }
    const QString name = QFileInfo(document_path_).fileName();
    const QString shown =
        project_ ? tr("%1 in %2").arg(name, QFileInfo(project_folder_).fileName()) : name;
    setWindowTitle(history_.Saved() ? tr("IFS Editor - %1").arg(shown)
                                    : tr("IFS Editor - %1 (unsaved)").arg(shown));
}

void Window::ReportProblem(const QString& what) {
    QMessageBox::warning(this, tr("IFS Editor"), what);
}

void Window::ReportOnce(const QString& what) {
    statusBar()->showMessage(what);
    if (what == last_error_) return;
    last_error_ = what;
    ReportProblem(what);
}

void Window::closeEvent(QCloseEvent* event) {
    if (!OfferToSave()) {
        event->ignore();
        return;
    }
    SaveLayout(*this, *docks_);
    host_.Stop();
    QMainWindow::closeEvent(event);
}

}
