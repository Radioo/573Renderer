#include "editor_window.h"

#include "editor_files.h"
#include "editor_host.h"
#include "editor_layout.h"
#include "editor_timeline.h"
#include "editor_viewport.h"

#include "document/authored.h"
#include "document/camera_edit.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/history.h"
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
    auto* timeline_area = new QScrollArea;
    timeline_area->setWidget(timeline_);
    timeline_area->setWidgetResizable(true);

    resize_timer_ = new QTimer(this);
    resize_timer_->setSingleShot(true);
    resize_timer_->setInterval(kResizeDelayMs);
    connect(resize_timer_, &QTimer::timeout, this, &Window::ResizeViewport);
    connect(viewport_, &Viewport::Resized, this, [this](int, int) { resize_timer_->start(); });

    ads::CDockAreaWidget* centre = docks_->setCentralWidget(MakePanel(tr("Viewport"), viewport_));
    docks_->addDockWidget(ads::LeftDockWidgetArea, MakePanel(tr("Package"), package_tree_), centre);
    docks_->addDockWidget(ads::RightDockWidgetArea, MakePanel(tr("Inspector"), inspector_), centre);
    docks_->addDockWidget(ads::BottomDockWidgetArea, MakePanel(tr("Timeline"), timeline_area),
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

void Window::FillInspector(const std::vector<Document::Field>& fields, bool editable) {
    filling_inspector_ = true;
    inspector_->clearContents();
    inspector_->setRowCount(static_cast<int>(fields.size()));
    for (std::size_t row = 0; row < fields.size(); row++) {
        const Document::Field& field = fields[row];
        auto* name = new QTableWidgetItem(QString::fromStdString(field.name));
        name->setFlags(Qt::ItemIsEnabled);
        auto* value = new QTableWidgetItem(QString::fromStdString(field.value));
        const bool writable = editable && (Document::PlacementFieldIsEditable(field.name) ||
                                           Document::CameraFieldIsEditable(field.name) ||
                                           Document::CallArgumentIndex(field.name).has_value());
        value->setFlags(writable ? Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable
                                 : Qt::ItemIsEnabled);
        inspector_->setItem(static_cast<int>(row), 0, name);
        inspector_->setItem(static_cast<int>(row), 1, value);
    }
    inspector_->resizeColumnToContents(0);
    filling_inspector_ = false;
}

void Window::ShowSelectedEntry() {
    const QList<QTreeWidgetItem*> selected = package_tree_->selectedItems();
    if (selected.isEmpty() || !file_) {
        FillInspector({}, false);
        return;
    }
    const QString path = selected.front()->data(0, kPathRole).toString();
    const auto details = file_->Describe(path.toStdString());
    if (!details) {
        FillInspector({Document::Field{.name = "Problem", .value = details.error()}}, false);
        return;
    }
    depth_.reset();
    FillInspector(Document::Fields(*details), false);
    if (details->role != Document::Role::Animation) return;
    animation_path_ = path.toStdString();
    ShowAnimation(details->name);
}

void Window::ShowAnimation(const std::string& name) {
    animation_name_ = name;
    if (!host_.Running()) {
        viewport_->ShowMessage(
            tr("Choose a game install to preview %1").arg(QString::fromStdString(name)));
        return;
    }
    const auto encoded = file_->Encode();
    if (!encoded) {
        ReportOnce(QString::fromStdString(encoded.error()));
        return;
    }
    const auto loaded = host_.ShowAnimation(package_name_, name, *encoded);
    if (!loaded) {
        ReportOnce(QString::fromStdString(loaded.error()));
        return;
    }
    frame_count_ = loaded->frame_count;
    frame_ = 0;
    const auto details = file_->Describe(animation_path_);
    if (details && details->animation) {
        timeline_->ShowAnimation(loaded->frame_count, details->animation->depths,
                                 details->animation->labels);
    }
    ResizeViewport();
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
    std::vector<Document::Field> fields;
    const Document::AuthoredDepth* owned =
        depth_ ? AuthoredAt(static_cast<uint16_t>(*depth_), frame_) : nullptr;
    if (owned != nullptr) {
        fields.push_back(Document::Field{.name = "Owned by the project",
                                         .value = "frames " + std::to_string(owned->first_frame) +
                                                  " to " + std::to_string(owned->last_frame) +
                                                  ", edited through its keyframes"});
    }
    if (depth_) {
        const auto tag =
            Document::LivePlacementTag(animation->root, static_cast<uint16_t>(*depth_), frame_);
        const auto* placement =
            tag ? std::get_if<AfpAnimation::Placement>(&animation->root.tags[*tag].body) : nullptr;
        if (placement == nullptr) {
            fields.push_back(Document::Field{
                .name = "Depth", .value = std::to_string(*depth_) + " holds nothing here"});
        } else {
            fields = Document::PlacementFields(*animation, *placement);
            if (placement->clip_actions) {
                for (const AfpAnimation::ClipEvent& event : placement->clip_actions->events) {
                    const std::vector<Document::Field> script =
                        Document::ScriptFields(*animation, event.bytecode);
                    fields.insert(fields.end(), script.begin(), script.end());
                }
            }
        }
    }
    if (const auto tag = Document::CameraTag(animation->root, frame_)) {
        const auto* camera = std::get_if<AfpAnimation::Camera>(&animation->root.tags[*tag].body);
        if (camera != nullptr) {
            const std::vector<Document::Field> shot = Document::CameraFields(*camera);
            fields.insert(fields.end(), shot.begin(), shot.end());
        }
    }
    FillInspector(fields, owned == nullptr);
}

namespace {

Support::Expected<void, std::string> SetCallArgument(AfpAnimation::Animation& animation,
                                                     uint16_t depth, uint32_t frame,
                                                     std::size_t index, const std::string& value) {
    const auto tag = Document::LivePlacementTag(animation.root, depth, frame);
    if (!tag) return Support::Unexpected(std::string("that depth holds nothing on this frame"));
    auto* placement = std::get_if<AfpAnimation::Placement>(&animation.root.tags[*tag].body);
    if (placement == nullptr || !placement->clip_actions)
        return Support::Unexpected(std::string("that placement carries no script"));
    for (AfpAnimation::ClipEvent& event : placement->clip_actions->events) {
        std::optional<Document::LibraryCall> call =
            Document::ReadLibraryCall(animation, event.bytecode);
        if (!call || index >= call->arguments.size()) continue;
        call->arguments[index].text = value;
        auto written = Document::WriteLibraryCall(animation, event.bytecode, *call);
        if (!written) return Support::Unexpected(written.error());
        event.bytecode = std::move(*written);
        return {};
    }
    return Support::Unexpected(std::string("that script is not a library call"));
}

Support::Expected<void, std::string> SetCameraOn(AfpAnimation::Animation& animation, uint32_t frame,
                                                 const std::string& field,
                                                 const std::string& value) {
    const auto tag = Document::CameraTag(animation.root, frame);
    if (!tag) return Support::Unexpected(std::string("this frame places no camera"));
    auto* camera = std::get_if<AfpAnimation::Camera>(&animation.root.tags[*tag].body);
    if (camera == nullptr) return Support::Unexpected(std::string("that tag is not a camera"));
    return Document::SetCameraField(*camera, field, value);
}

}

void Window::EditAnimation(const QString& name, const AnimationChange& change) {
    if (!file_ || animation_path_.empty()) return;
    auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) {
        ReportOnce(QString::fromStdString(animation.error()));
        return;
    }
    const auto changed = change(*animation);
    if (!changed) {
        ReportProblem(QString::fromStdString(changed.error()));
        ShowFrame();
        return;
    }
    Document::File before = *file_;
    const auto written = file_->WriteAnimation(animation_path_, *animation);
    if (!written) {
        ReportProblem(QString::fromStdString(written.error()));
        ShowFrame();
        return;
    }
    history_.Record(name.toStdString(), std::move(before));
    RefreshState();
    Reload();
    ShowFrame();
}

void Window::ApplyFieldEdit(QTableWidgetItem* item) {
    if (filling_inspector_ || item == nullptr || item->column() != 1) return;
    if (!file_ || animation_path_.empty()) return;
    const QTableWidgetItem* name = inspector_->item(item->row(), 0);
    if (name == nullptr) return;
    const std::string field = name->text().toStdString();
    const std::string value = item->text().toStdString();
    const uint32_t frame = frame_;
    if (Document::CameraFieldIsEditable(field)) {
        EditAnimation(tr("%1 on frame %2").arg(name->text()).arg(frame),
                      [field, value, frame](AfpAnimation::Animation& animation) {
                          return SetCameraOn(animation, frame, field, value);
                      });
        return;
    }
    if (!depth_) return;
    const uint16_t depth = static_cast<uint16_t>(*depth_);
    if (const std::optional<std::size_t> argument = Document::CallArgumentIndex(field)) {
        const std::size_t index = *argument;
        EditAnimation(tr("%1 on depth %2").arg(name->text()).arg(*depth_),
                      [index, value, depth, frame](AfpAnimation::Animation& animation) {
                          return SetCallArgument(animation, depth, frame, index, value);
                      });
        return;
    }
    EditAnimation(tr("%1 on depth %2").arg(name->text()).arg(*depth_),
                  [field, value, depth, frame](AfpAnimation::Animation& animation) {
                      const auto tag = Document::LivePlacementTag(animation.root, depth, frame);
                      if (!tag)
                          return Support::Expected<void, std::string>(Support::Unexpected(
                              std::string("that depth holds nothing on this frame")));
                      auto* placement =
                          std::get_if<AfpAnimation::Placement>(&animation.root.tags[*tag].body);
                      if (placement == nullptr)
                          return Support::Expected<void, std::string>(
                              Support::Unexpected(std::string("that tag is not a placement")));
                      return Document::SetPlacementField(animation, *placement, field, value);
                  });
}

void Window::EditDocument(const QString& name, const DocumentChange& change) {
    if (!file_) return;
    Document::File before = *file_;
    const auto changed = change(*file_);
    if (!changed) {
        file_ = std::move(before);
        ReportProblem(QString::fromStdString(changed.error()));
        return;
    }
    history_.Record(name.toStdString(), std::move(before));
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
    QAction* replace = path.isEmpty() ? nullptr : menu.addAction(tr("Replace %1...").arg(name));
    QAction* remove = path.isEmpty() ? nullptr : menu.addAction(tr("Remove %1").arg(name));
    const QAction* chosen = menu.exec(where);
    if (chosen == nullptr) return;

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
    menu.addSeparator();
    const auto animation = file_->ReadAnimation(animation_path_);
    const bool has_camera = animation && Document::CameraTag(animation->root, frame).has_value();
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
    if (chosen == add_camera) {
        bool answered = false;
        const int id = QInputDialog::getInt(this, tr("Add a camera"), tr("Camera"), 0, 0,
                                            std::numeric_limits<uint16_t>::max(), 1, &answered);
        if (!answered) return;
        const auto number = static_cast<uint16_t>(id);
        EditAnimation(tr("Add camera %1").arg(id), [frame, number](AfpAnimation::Animation& clip) {
            return Document::AddCamera(clip, frame, number);
        });
        return;
    }
    if (chosen == remove_camera) {
        EditAnimation(
            tr("Remove the camera on frame %1").arg(frame),
            [frame](AfpAnimation::Animation& clip) { return Document::RemoveCamera(clip, frame); });
        return;
    }

    if (chosen == insert_frame) {
        EditAnimation(tr("Insert frame %1").arg(frame),
                      [frame](AfpAnimation::Animation& animation) {
                          return Document::InsertFrame(animation, frame);
                      });
        return;
    }
    if (chosen == remove_frame) {
        EditAnimation(tr("Remove frame %1").arg(frame),
                      [frame](AfpAnimation::Animation& animation) {
                          return Document::RemoveFrame(animation, frame);
                      });
        return;
    }
    if (chosen == add_depth) {
        bool answered = false;
        const int last = QInputDialog::getInt(this, tr("Add a depth"), tr("Last frame"),
                                              static_cast<int>(frame), static_cast<int>(frame),
                                              static_cast<int>(frame_count_), 1, &answered);
        if (!answered) return;
        const auto depth = static_cast<uint16_t>(*depth_);
        const auto until = static_cast<uint32_t>(last);
        EditAnimation(tr("Add depth %1").arg(*depth_),
                      [depth, frame, until](AfpAnimation::Animation& animation) {
                          return Document::AddDepth(animation, depth, 0, frame, until);
                      });
        return;
    }
    if (chosen == remove_depth) {
        const auto depth = static_cast<uint16_t>(*depth_);
        EditAnimation(tr("Remove depth %1").arg(*depth_),
                      [depth, frame](AfpAnimation::Animation& animation) {
                          return Document::RemoveDepth(animation, depth, frame);
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
                      [text, frame](AfpAnimation::Animation& animation) {
                          return Document::AddLabel(animation, text, frame);
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
                      [named, text](AfpAnimation::Animation& animation) {
                          return Document::RenameLabel(animation, named, text);
                      });
        return;
    }
    if (chosen == move) {
        EditAnimation(tr("Move %1").arg(label), [named, frame](AfpAnimation::Animation& animation) {
            return Document::MoveLabel(animation, named, frame);
        });
        return;
    }
    if (chosen == remove) {
        EditAnimation(tr("Remove %1").arg(label), [named](AfpAnimation::Animation& animation) {
            return Document::RemoveLabel(animation, named);
        });
    }
}

void Window::Undo() {
    if (!file_) return;
    auto restored = history_.Undo(*file_);
    if (!restored) return;
    file_ = std::move(*restored);
    ShowRestored();
}

void Window::Redo() {
    if (!file_) return;
    auto restored = history_.Redo(*file_);
    if (!restored) return;
    file_ = std::move(*restored);
    ShowRestored();
}

void Window::ShowRestored() {
    RefreshState();
    ShowFrame();
    Reload();
}

void Window::Reload() {
    if (!host_.Running() || animation_name_.empty() || !file_) return;
    const auto encoded = file_->Encode();
    if (!encoded) {
        ReportOnce(QString::fromStdString(encoded.error()));
        return;
    }
    const auto loaded = host_.Reload(package_name_, animation_name_, *encoded);
    if (!loaded) {
        ReportOnce(QString::fromStdString(loaded.error()));
        return;
    }
    frame_count_ = loaded->frame_count;
    const auto details = file_->Describe(animation_path_);
    if (details && details->animation) {
        timeline_->ShowAnimation(loaded->frame_count, details->animation->depths,
                                 details->animation->labels);
    }
    SeekTo(frame_);
}

void Window::ResizeViewport() {
    if (!host_.Running() || animation_name_.empty()) return;
    const auto resized = host_.Resize(static_cast<uint32_t>(viewport_->width()),
                                      static_cast<uint32_t>(viewport_->height()));
    if (!resized) {
        ReportOnce(QString::fromStdString(resized.error()));
        return;
    }
    RenderFrame();
}

void Window::SeekTo(uint32_t frame) {
    frame_ = frame;
    if (!host_.Running()) return;
    const auto sought = host_.Seek(frame);
    if (!sought) {
        ReportOnce(QString::fromStdString(sought.error()));
        return;
    }
    RenderFrame();
    if (depth_) ShowFrame();
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
    viewport_->ShowFrame(image.copy());
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
    create_project_action_->setEnabled(file_.has_value() && !project_);
    close_project_action_->setEnabled(project_.has_value());
    export_action_->setEnabled(project_.has_value() && !authored_.empty());
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
