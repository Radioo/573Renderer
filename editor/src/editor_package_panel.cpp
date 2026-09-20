#include "editor_window.h"

#include "editor_files.h"
#include "editor_filter.h"
#include "editor_icons.h"
#include "editor_panel_tabs.h"
#include "editor_rows.h"
#include "editor_start_screen.h"
#include "editor_theme.h"

#include "document/animation_settings.h"
#include "document/document.h"
#include "document/outline.h"
#include "formats/afp_animation.h"

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QLineEdit>
#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QSettings>
#include <QSize>
#include <QStackedWidget>
#include <QStringList>
#include <QString>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>

#include <cstdint>
#include <utility>
#include <vector>

namespace Editor {

namespace {

constexpr int kPathRole = Qt::UserRole;
constexpr int kNameRole = Qt::UserRole + 2;
constexpr int kPanelIcon = 16;
constexpr int kPanelButton = 26;
constexpr int kRecentKept = 8;

}

void Window::RememberRecent(const QString& path) {
    QSettings settings;
    QStringList recent = settings.value(kRecentKey).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > kRecentKept)
        recent.removeLast();
    settings.setValue(kRecentKey, recent);

    int animations = 0;
    for (const Document::Node& node : file_->Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role == Document::Role::Animation) animations++;
        }
    }
    QVariantMap counted = settings.value(kRecentCountsKey).toMap();
    counted.insert(path, animations);
    for (const QString& known : counted.keys()) {
        if (!recent.contains(known)) counted.remove(known);
    }
    settings.setValue(kRecentCountsKey, counted);
    RefreshStartScreen();
}

void Window::RefreshStartScreen() {
    if (start_ == nullptr || centre_ == nullptr) return;
    centre_->setCurrentIndex(file_ ? 1 : 0);
    const QSettings settings;
    const QVariantMap counted = settings.value(kRecentCountsKey).toMap();
    std::vector<RecentFile> recent;
    for (const QString& path : settings.value(kRecentKey).toStringList()) {
        const QFileInfo about(path);
        if (!about.exists()) continue;
        const QVariant animations = counted.value(path);
        recent.push_back(RecentFile{.path = path,
                                    .folder = about.absolutePath(),
                                    .animations = animations.isValid() ? animations.toInt() : -1,
                                    .project = QFileInfo::exists(about.dir().filePath(
                                        about.completeBaseName() + " project/project.json"))});
    }
    start_->ShowRecent(recent);
    start_->ShowHost(HostCard{.install = settings.value(kGameDirKey).toString(),
                              .build = QString::fromStdString(TargetBuild()),
                              .running = host_.Running()});
}

QWidget* Window::BuildPackageTabs() {
    animations_tree_ = new QTreeWidget;
    animations_tree_->setObjectName("package_animations");
    animations_tree_->setHeaderLabels({tr("Animation"), tr("Frames"), tr("Stage"), tr("Rate")});
    animations_tree_->setRootIsDecorated(false);
    images_tree_ = new QTreeWidget;
    images_tree_->setObjectName("package_images");
    images_tree_->setHeaderLabels({tr("Image"), tr("Size")});
    images_tree_->setRootIsDecorated(false);
    for (QTreeWidget* tree : {animations_tree_, images_tree_}) {
        tree->setHeaderHidden(true);
        tree->setIconSize(QSize(Rows::kThumbWidth, Rows::kThumbHeight));
        tree->setItemDelegate(new Rows::Delegate(tree));
        tree->setMouseTracking(true);
        for (int column = 1; column < tree->columnCount(); column++)
            tree->setColumnHidden(column, true);
        tree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tree, &QTreeWidget::customContextMenuRequested, this,
                [this, tree](const QPoint& at) {
                    ChooseEntryFrom(*tree);
                    ShowPackageMenu(tree->mapToGlobal(at));
                });
        connect(tree, &QTreeWidget::itemSelectionChanged, this,
                [this, tree] { ChooseEntryFrom(*tree); });
        connect(tree, &QTreeWidget::itemChanged, this,
                [this](QTreeWidgetItem* item, int) { RenameEntryRow(item); });
    }
    connect(animations_tree_, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* item, int) {
                if (item->data(0, kPathRole).toString().isEmpty()) AddNewAnimation();
            });

    package_tabs_ = new PanelTabs;
    package_tabs_->setObjectName("package_tabs");
    package_tabs_->addTab(
        WithFilter(animations_tree_, animations_filter_ = new QLineEdit, tr("Filter animations")),
        tr("Animations"));
    package_tabs_->addTab(
        WithFilter(images_tree_, images_filter_ = new QLineEdit, tr("Filter images")),
        tr("Images"));
    package_tabs_->addTab(
        WithFilter(package_tree_, package_filter_ = new QLineEdit, tr("Filter files")),
        tr("Files"));
    package_tabs_->setCornerWidget(BuildPackageActions(), Qt::TopRightCorner);
    return package_tabs_;
}

QWidget* Window::BuildPackageActions() {
    auto* corner = new QWidget;
    auto* line = new QHBoxLayout(corner);
    line->setContentsMargins(0, 0, 4, 0);
    line->setSpacing(0);
    auto* adding = new QToolButton;
    adding->setObjectName("package_add");
    adding->setProperty("panel_icon", true);
    adding->setIcon(Icons::Of(Icons::Glyph::Plus, Theme::kSoft, kPanelIcon));
    adding->setIconSize(QSize(kPanelIcon, kPanelIcon));
    adding->setFixedSize(kPanelButton, kPanelButton);
    adding->setToolTip(tr("New animation, add image"));
    connect(adding, &QToolButton::clicked, this, [this] {
        if (package_tabs_->currentIndex() == 1) {
            AddImageFromFile();
            return;
        }
        AddNewAnimation();
    });
    line->addWidget(adding);
    auto* more = new QToolButton;
    more->setObjectName("package_more");
    more->setProperty("panel_icon", true);
    more->setIcon(Icons::Of(Icons::Glyph::Dots, Theme::kSoft, kPanelIcon));
    more->setIconSize(QSize(kPanelIcon, kPanelIcon));
    more->setFixedSize(kPanelButton, kPanelButton);
    more->setToolTip(tr("Package actions"));
    connect(more, &QToolButton::clicked, this,
            [this, more] { ShowPackageMenu(more->mapToGlobal(QPoint(0, more->height()))); });
    line->addWidget(more);
    return corner;
}

void Window::ChooseEntryFrom(QTreeWidget& tree) {
    const QList<QTreeWidgetItem*> chosen = tree.selectedItems();
    if (chosen.isEmpty()) return;
    const QString path = chosen.front()->data(0, kPathRole).toString();
    if (path.isEmpty()) return;
    SelectEntry(path);
}

void Window::RenameEntryRow(QTreeWidgetItem* item) {
    if (filling_tree_ || item == nullptr || !file_) return;
    const QString path = item->data(0, kPathRole).toString();
    const QString was = item->data(0, kNameRole).toString();
    const QString name = item->text(0);
    if (path.isEmpty() || name == was) return;
    if (name.isEmpty()) {
        item->setText(0, was);
        return;
    }
    ApplyAnimationRename(path.toStdString(), was, name);
}

void Window::FillAnimationRows() {
    if (animations_tree_ == nullptr) return;
    animations_tree_->clear();
    int seed = 0;
    for (const Document::Node& node : file_->Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role != Document::Role::Animation) continue;
            auto* row = new QTreeWidgetItem(animations_tree_);
            row->setText(0, QString::fromStdString(child.name));
            row->setData(0, kPathRole, QString::fromStdString(child.path));
            row->setData(0, kNameRole, QString::fromStdString(child.name));
            row->setFlags(row->flags() | Qt::ItemIsEditable);
            row->setIcon(0, QIcon(Rows::Stripes(Rows::kThumbWidth, Rows::kThumbHeight, seed++)));
            const auto animation = file_->ReadAnimation(child.path);
            if (!animation) continue;
            row->setText(1, QString::number(animation->root.frames.size()));
            const Document::StageSize stage = Document::StageSizeOf(*animation);
            row->setText(2, tr("%1x%2").arg(stage.width).arg(stage.height));
            const QString rate = QString::number(Document::FrameRate(*animation), 'g', 4);
            row->setText(3, rate);
            row->setData(0, Rows::kDetailRole,
                         tr("%1 frames, %2 %5 %3, %4 fps")
                             .arg(animation->root.frames.size())
                             .arg(stage.width)
                             .arg(stage.height)
                             .arg(rate)
                             .arg(QChar(0x00D7)));
        }
    }
    auto* adding = new QTreeWidgetItem(animations_tree_);
    adding->setText(0, tr("+ New animation"));
    adding->setFlags(Qt::ItemIsEnabled);
    if (package_tabs_ != nullptr)
        package_tabs_->ShowCount(0, animations_tree_->topLevelItemCount() - 1);
}

void Window::FillImageRows() {
    if (images_tree_ == nullptr) return;
    images_tree_->clear();
    for (const Document::Node& node : file_->Nodes()) {
        for (const Document::Node& child : node.children) {
            if (child.role != Document::Role::Texture) continue;
            auto* row = new QTreeWidgetItem(images_tree_);
            row->setText(0, QString::fromStdString(child.name));
            row->setData(0, kPathRole, QString::fromStdString(child.path));
            row->setData(0, kNameRole, QString::fromStdString(child.name));
            const auto pixels = file_->ReadImage(child.name);
            if (!pixels) continue;
            row->setText(1, tr("%1x%2").arg(pixels->width).arg(pixels->height));
            row->setData(0, Rows::kDetailRole,
                         tr("%1 %3 %2").arg(pixels->width).arg(pixels->height).arg(QChar(0x00D7)));
            const QImage picture(pixels->bgra.data(), static_cast<int>(pixels->width),
                                 static_cast<int>(pixels->height), QImage::Format_ARGB32);
            row->setIcon(0, QIcon(QPixmap::fromImage(
                                picture.scaled(Rows::kThumbWidth, Rows::kThumbHeight,
                                               Qt::KeepAspectRatio, Qt::SmoothTransformation))));
        }
    }
    if (package_tabs_ != nullptr) package_tabs_->ShowCount(1, images_tree_->topLevelItemCount());
}

}
