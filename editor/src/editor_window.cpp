#include "editor_window.h"

#include "editor_files.h"
#include "editor_host.h"
#include "editor_layout.h"
#include "editor_timeline.h"
#include "editor_viewport.h"

#include "document/authored.h"
#include "document/camera_edit.h"
#include "document/clip_edit.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/history.h"
#include "document/inspector.h"
#include "document/frame_edit.h"
#include "document/label_edit.h"
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
#include <QMenu>
#include <QMenuBar>
#include <QInputDialog>
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
#include <QLineEdit>
#include <QVariant>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
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

    timeline_ = new Timeline;
    connect(timeline_, &Timeline::FrameChosen, this, &Window::SeekTo);
    connect(timeline_, &Timeline::DepthChosen, this, &Window::ChooseDepth);
    connect(timeline_, &Timeline::MenuRequested, this, &Window::ShowTimelineMenu);
    connect(timeline_, &Timeline::KeyChosen, this, &Window::FocusKey);
    connect(timeline_, &Timeline::KeysShifted, this, &Window::ShiftSelectedKeys);
    AddKeyActions();
    connect(timeline_, &Timeline::KeyMenuRequested, this, &Window::ShowKeyMenu);
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
    connect(viewport_, &Viewport::Picked, this, &Window::PickOnStage);
    connect(viewport_, &Viewport::Dragged, this, &Window::MoveOnStage);
    connect(viewport_, &Viewport::Reshaped, this, &Window::ReshapeOnStage);

    ads::CDockAreaWidget* centre = docks_->setCentralWidget(MakePanel(tr("Viewport"), viewport_));
    docks_->addDockWidget(ads::LeftDockWidgetArea, MakePanel(tr("Package"), package_tree_), centre);
    docks_->addDockWidget(ads::RightDockWidgetArea, MakePanel(tr("Inspector"), inspector_), centre);
    docks_->addDockWidget(ads::BottomDockWidgetArea, MakePanel(tr("Timeline"), timeline_panel),
                          centre);
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

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    undo_action_ = edit->addAction(tr("&Undo"));
    undo_action_->setShortcut(QKeySequence::Undo);
    connect(undo_action_, &QAction::triggered, this, &Window::Undo);
    redo_action_ = edit->addAction(tr("&Redo"));
    redo_action_->setShortcut(QKeySequence::Redo);
    connect(redo_action_, &QAction::triggered, this, &Window::Redo);
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
    document_path_ = path;
    package_name_ = QFileInfo(path).completeBaseName().toStdString();
    animation_name_.clear();
    animation_path_.clear();
    depth_.reset();
    frame_count_ = 0;
    frame_ = 0;
    RefreshState();
    viewport_->ShowMessage(tr("No animation selected"));
    timeline_->Clear();
    FillTree();
    if (const Document::Node* first = FirstAnimation(file_->Nodes()); first != nullptr) {
        QTreeWidgetItem* item = ItemForPath(package_tree_, QString::fromStdString(first->path));
        if (item != nullptr) package_tree_->setCurrentItem(item);
    }
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
    timeline_->SelectDepth(depth_ ? std::optional<uint16_t>(static_cast<uint16_t>(*depth_))
                                  : std::nullopt);
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

void Window::EditDocument(const QString& name, const DocumentChange& change) {
    if (!file_) return;
    StopPlayback();
    Document::File before = *file_;
    const auto changed = change(*file_);
    if (!changed) {
        file_ = std::move(before);
        ReportProblem(QString::fromStdString(changed.error()));
        return;
    }
    history_.Record(name.toStdString(),
                    Document::Snapshot{.file = std::move(before), .authored = authored_});
    RefreshState();
    FillTree();
    Reload();
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
    const QString name = details ? QString::fromStdString(details->name) : QString();

    QMenu menu(this);
    QAction* add_image = menu.addAction(tr("Add an image from a file..."));
    QAction* own_image =
        project_ ? menu.addAction(tr("Add an image the project owns...")) : nullptr;
    QAction* replace = path.isEmpty() ? nullptr : menu.addAction(tr("Replace %1...").arg(name));
    QAction* remove = path.isEmpty() ? nullptr : menu.addAction(tr("Remove %1").arg(name));
    const QAction* chosen = menu.exec(where);
    if (chosen == nullptr) return;

    if (chosen == own_image) {
        AddProjectImage();
        return;
    }
    if (chosen == add_image) {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Add an image"), QString(), tr("Images (*.png *.bmp *.jpg);;All files (*)"));
        if (file.isEmpty()) return;
        QImage picture(file);
        if (picture.isNull()) {
            ReportProblem(tr("%1 is not an image Qt can read").arg(file));
            return;
        }
        picture = picture.convertToFormat(QImage::Format_ARGB32);
        std::vector<uint8_t> bgra;
        bgra.reserve(static_cast<std::size_t>(picture.width()) * picture.height() * 4);
        for (int y = 0; y < picture.height(); y++) {
            const auto* row = picture.constScanLine(y);
            bgra.insert(bgra.end(), row, row + static_cast<std::ptrdiff_t>(picture.width()) * 4);
        }
        const std::string logical = QFileInfo(file).completeBaseName().toStdString();
        const auto width = static_cast<uint32_t>(picture.width());
        const auto height = static_cast<uint32_t>(picture.height());
        EditDocument(tr("Add image %1").arg(QString::fromStdString(logical)),
                     [logical, width, height, bgra](Document::File& document) {
                         return document.AddImage(logical, width, height, bgra);
                     });
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
    if (chosen == remove) {
        const std::string target = path.toStdString();
        const std::string logical = name.toStdString();
        EditDocument(
            tr("Remove %1").arg(name), [target, logical, is_image](Document::File& document) {
                return is_image ? document.RemoveImage(logical) : document.RemoveEntry(target);
            });
    }
}

void Window::ShowTimelineMenu(const QPoint& where, uint32_t frame, const QString& label) {
    if (!file_ || animation_path_.empty()) return;
    const Document::ClipId clip = clip_;
    QMenu menu(this);
    QAction* add = menu.addAction(tr("Add a label at frame %1...").arg(frame));
    QAction* rename = label.isEmpty() ? nullptr : menu.addAction(tr("Rename %1...").arg(label));
    QAction* move =
        label.isEmpty() ? nullptr : menu.addAction(tr("Move %1 to frame %2").arg(label).arg(frame));
    QAction* remove = label.isEmpty() ? nullptr : menu.addAction(tr("Remove %1").arg(label));
    menu.addSeparator();
    QAction* insert_frame = menu.addAction(tr("Insert a frame at %1").arg(frame));
    QAction* remove_frame = menu.addAction(tr("Remove frame %1").arg(frame));
    QAction* add_depth =
        depth_ ? menu.addAction(tr("Add depth %1 from frame %2...").arg(*depth_).arg(frame))
               : nullptr;
    QAction* remove_depth =
        depth_ ? menu.addAction(tr("Remove depth %1 here").arg(*depth_)) : nullptr;
    menu.addSeparator();
    const bool authored =
        depth_ != std::nullopt && AuthoredAt(static_cast<uint16_t>(*depth_), frame) != nullptr;
    QAction* own = (project_ && depth_ && !authored)
                       ? menu.addAction(tr("Let the project own depth %1 from here").arg(*depth_))
                       : nullptr;
    QAction* detach =
        authored ? menu.addAction(tr("Detach depth %1 back to baked data").arg(*depth_)) : nullptr;
    QAction* script =
        authored ? menu.addAction(tr("Edit the script of depth %1...").arg(*depth_)) : nullptr;
    QAction* start =
        authored ? menu.addAction(tr("Start animating depth %1...").arg(*depth_)) : nullptr;
    menu.addSeparator();
    const auto animation = file_->ReadAnimation(animation_path_);
    const AfpAnimation::Container* shown =
        animation ? Document::FindClip(*animation, clip) : nullptr;
    const bool has_camera = shown != nullptr && Document::CameraTag(*shown, frame).has_value();
    const auto clip_frames = static_cast<int>(shown != nullptr ? shown->frames.size() : 0);
    QAction* add_camera =
        has_camera ? nullptr : menu.addAction(tr("Add a camera on frame %1...").arg(frame));
    QAction* remove_camera =
        has_camera ? menu.addAction(tr("Remove the camera on frame %1").arg(frame)) : nullptr;
    const QAction* chosen = menu.exec(where);
    if (chosen == nullptr) return;

    if (chosen == own) {
        OwnSelectedDepth(frame);
        return;
    }
    if (chosen == detach) {
        DetachSelectedDepth();
        return;
    }
    if (chosen == script) {
        EditOwnedScript();
        return;
    }
    if (chosen == start) {
        StartAnimating();
        return;
    }
    if (chosen == add_camera) {
        bool answered = false;
        const int id = QInputDialog::getInt(this, tr("Add a camera"), tr("Camera"), 0, 0,
                                            std::numeric_limits<uint16_t>::max(), 1, &answered);
        if (!answered) return;
        const auto number = static_cast<uint16_t>(id);
        EditAnimation(tr("Add camera %1").arg(id),
                      [clip, frame, number](AfpAnimation::Animation& edited) {
                          return Document::AddCamera(edited, clip, frame, number);
                      });
        return;
    }
    if (chosen == remove_camera) {
        EditAnimation(tr("Remove the camera on frame %1").arg(frame),
                      [clip, frame](AfpAnimation::Animation& edited) {
                          return Document::RemoveCamera(edited, clip, frame);
                      });
        return;
    }

    if (chosen == insert_frame) {
        EditAnimation(tr("Insert frame %1").arg(frame),
                      [clip, frame](AfpAnimation::Animation& edited) {
                          return Document::InsertFrame(edited, clip, frame);
                      });
        return;
    }
    if (chosen == remove_frame) {
        EditAnimation(tr("Remove frame %1").arg(frame),
                      [clip, frame](AfpAnimation::Animation& edited) {
                          return Document::RemoveFrame(edited, clip, frame);
                      });
        return;
    }
    if (chosen == add_depth) {
        bool answered = false;
        const int last =
            QInputDialog::getInt(this, tr("Add a depth"), tr("Last frame"), static_cast<int>(frame),
                                 static_cast<int>(frame),
                                 std::max(clip_frames - 1, static_cast<int>(frame)), 1, &answered);
        if (!answered || !animation) return;
        const std::optional<Placeable> choice = ChoosePlaceable(*animation);
        if (!choice) return;
        const auto depth = static_cast<uint16_t>(*depth_);
        const auto until = static_cast<uint32_t>(last);
        if (!choice->character) {
            PlaceImage(choice->image, Document::DepthSpan{.clip = clip,
                                                          .depth = depth,
                                                          .first_frame = frame,
                                                          .last_frame = until});
            return;
        }
        const uint16_t placed = *choice->character;
        EditAnimation(tr("Add depth %1").arg(*depth_),
                      [clip, depth, placed, frame, until](AfpAnimation::Animation& edited) {
                          return Document::AddDepth(edited, clip, depth, placed, frame, until);
                      });
        return;
    }
    if (chosen == remove_depth) {
        const auto depth = static_cast<uint16_t>(*depth_);
        EditAnimation(tr("Remove depth %1").arg(*depth_),
                      [clip, depth, frame](AfpAnimation::Animation& edited) {
                          return Document::RemoveDepth(edited, clip, depth, frame);
                      });
        return;
    }

    if (chosen == add) {
        bool answered = false;
        const QString name = QInputDialog::getText(this, tr("Add a label"), tr("Name"),
                                                   QLineEdit::Normal, QString(), &answered);
        if (!answered || name.isEmpty()) return;
        const std::string text = name.toStdString();
        EditAnimation(tr("Add label %1").arg(name),
                      [clip, text, frame](AfpAnimation::Animation& edited) {
                          return Document::AddLabel(edited, clip, text, frame);
                      });
        return;
    }
    const std::string named = label.toStdString();
    if (chosen == rename) {
        bool answered = false;
        const QString renamed = QInputDialog::getText(this, tr("Rename a label"), tr("Name"),
                                                      QLineEdit::Normal, label, &answered);
        if (!answered || renamed.isEmpty()) return;
        const std::string text = renamed.toStdString();
        EditAnimation(tr("Rename %1").arg(label),
                      [clip, named, text](AfpAnimation::Animation& edited) {
                          return Document::RenameLabel(edited, clip, named, text);
                      });
        return;
    }
    if (chosen == move) {
        EditAnimation(tr("Move %1").arg(label),
                      [clip, named, frame](AfpAnimation::Animation& edited) {
                          return Document::MoveLabel(edited, clip, named, frame);
                      });
        return;
    }
    if (chosen == remove) {
        EditAnimation(tr("Remove %1").arg(label), [clip, named](AfpAnimation::Animation& edited) {
            return Document::RemoveLabel(edited, clip, named);
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
    const auto frame = host_.Render();
    if (!frame) {
        ReportOnce(QString::fromStdString(frame.error()));
        return;
    }
    if (!reader_) {
        auto reader = SharedTexture::Reader::Create();
        if (!reader) {
            ReportOnce(QString::fromStdString(reader.error()));
            return;
        }
        reader_ = std::move(*reader);
    }
    const auto pixels = reader_->Read(frame->shared_handle, frame->width, frame->height);
    if (!pixels) {
        ReportOnce(QString::fromStdString(pixels.error()));
        return;
    }
    const QImage image(pixels->data(), static_cast<int>(frame->width),
                       static_cast<int>(frame->height), QImage::Format_ARGB32);
    viewport_->ShowFrame(image.copy(), QSize(static_cast<int>(frame->stage_width),
                                             static_cast<int>(frame->stage_height)));
    const QSize shown(static_cast<int>(frame->width), static_cast<int>(frame->height));
    if (shown != viewport_->FittedSize(viewport_->size())) resize_timer_->start();
    timeline_->SetFrame(frame->frame);
    statusBar()->showMessage(tr("Frame %1 of %2").arg(frame->frame).arg(frame_count_));
    last_error_.clear();
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
