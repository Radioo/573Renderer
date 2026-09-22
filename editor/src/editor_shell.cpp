#include "editor_window.h"

#include "editor_commands.h"
#include "editor_icons.h"
#include "editor_search.h"
#include "editor_theme.h"
#include "editor_timeline.h"
#include "editor_viewport.h"

#include "document/animation_settings.h"
#include "document/characters.h"
#include "document/clip.h"
#include "document/outline.h"
#include "document/timeline.h"
#include "document/playback.h"
#include "document/project_drift.h"

#include <QAction>
#include <QDesktopServices>
#include <QEvent>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QMenuBar>
#include <QSizePolicy>
#include <QStatusBar>
#include <QString>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <string_view>
#include <utility>
#include <vector>
#include <cstddef>
#include <optional>

namespace Editor {

namespace {

QWidget* Divider() {
    auto* line = new QWidget;
    line->setObjectName("divider");
    line->setFixedSize(1, 20);
    return line;
}

QWidget* Margin(int left, int right, QWidget* inside) {
    auto* held = new QWidget;
    auto* line = new QHBoxLayout(held);
    line->setContentsMargins(left, 0, right, 0);
    line->setSpacing(0);
    line->addWidget(inside);
    return held;
}

QWidget* Spacer() {
    auto* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return spacer;
}

constexpr std::array<std::pair<std::string_view, const char*>, 8> kCategories{{
    {"file", QT_TRANSLATE_NOOP("Editor::Window", "File")},
    {"project", QT_TRANSLATE_NOOP("Editor::Window", "File")},
    {"edit", QT_TRANSLATE_NOOP("Editor::Window", "Edit")},
    {"depth", QT_TRANSLATE_NOOP("Editor::Window", "Depth")},
    {"key", QT_TRANSLATE_NOOP("Editor::Window", "Keyframe")},
    {"clip", QT_TRANSLATE_NOOP("Editor::Window", "Clip")},
    {"view", QT_TRANSLATE_NOOP("Editor::Window", "View")},
    {"play", QT_TRANSLATE_NOOP("Editor::Window", "Playback")},
}};

QString CategoryOf(const QString& id) {
    const QString prefix = id.section('.', 0, 0);
    for (const auto& [name, title] : kCategories) {
        if (prefix == QLatin1StringView(name.data(), static_cast<qsizetype>(name.size())))
            return Window::tr(title);
    }
    return {};
}

void AddAnimations(const std::vector<Document::Node>& nodes, std::vector<Document::Node>& out) {
    for (const Document::Node& node : nodes) {
        if (node.role == Document::Role::Animation) out.push_back(node);
        AddAnimations(node.children, out);
    }
}

constexpr int kGlassSide = 14;

class SearchHover : public QObject {
public:
    SearchHover(QLabel* glass, QLabel* said, QObject* parent)
        : QObject(parent), glass_(glass), said_(said) {
        Paint(Theme::kFaint);
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Enter) Paint(Theme::kSoft);
        if (event->type() == QEvent::Leave) Paint(Theme::kFaint);
        return QObject::eventFilter(watched, event);
    }

private:
    void Paint(const QColor& ink) {
        glass_->setPixmap(Icons::Drawn(Icons::Glyph::Search, ink, kGlassSide));
        said_->setStyleSheet(QString("color: %1;").arg(ink.name()));
    }

    QLabel* glass_;
    QLabel* said_;
};

constexpr int kDotSide = 7;
constexpr int kStatusGap = 12;

QLabel* StatusLabel(const QString& name) {
    auto* label = new QLabel;
    label->setObjectName(name);
    label->setContentsMargins(0, 0, 8, 0);
    return label;
}

}

QWidget* Window::BuildSearchField() {
    auto* field = new QPushButton;
    field->setObjectName("search");
    field->setCursor(Qt::PointingHandCursor);
    field->setFixedSize(Theme::kSearchWidth, Theme::kBarControl);
    connect(field, &QPushButton::clicked, this, [this] { commands_->Run("edit.search"); });

    auto* line = new QHBoxLayout(field);
    line->setContentsMargins(10, 0, 10, 0);
    line->setSpacing(8);
    auto* glass = new QLabel;
    glass->setObjectName("search_glass");
    auto* said = new QLabel(tr("Search commands, depths and animations"));
    said->setObjectName("search_text");
    field->installEventFilter(new SearchHover(glass, said, field));
    auto* chip = new QLabel(
        QKeySequence(Qt::CTRL | Qt::Key_K).toString(QKeySequence::NativeText).replace("+", " "));
    chip->setObjectName("search_chip");
    QFont mono(Theme::MonoFamily());
    mono.setPixelSize(11);
    chip->setFont(mono);
    line->addWidget(glass);
    line->addWidget(said, 1);
    line->addWidget(chip);
    return field;
}

void Window::BuildTopBar() {
    auto* bar = new QToolBar(tr("Top bar"));
    bar->setObjectName("top_bar");
    bar->setMovable(false);
    bar->setFloatable(false);
    bar->setFixedHeight(Theme::kBarHeight);
    menuBar()->setVisible(false);
    addToolBar(Qt::TopToolBarArea, bar);

    auto* logo = new QLabel(tr("IFS"));
    logo->setObjectName("logo");
    logo->setAlignment(Qt::AlignCenter);
    logo->setFixedSize(22, 22);
    QFont chipped(Theme::MonoFamily());
    chipped.setPixelSize(10);
    chipped.setBold(true);
    logo->setFont(chipped);
    logo->setContentsMargins(0, 0, 0, 0);
    bar->addWidget(Margin(4, 8, logo));

    for (QMenu* menu : menus_) {
        auto* opens = new QToolButton;
        opens->setObjectName("menu_button");
        opens->setText(menu->title());
        opens->setMenu(menu);
        opens->setPopupMode(QToolButton::InstantPopup);
        opens->setToolButtonStyle(Qt::ToolButtonTextOnly);
        opens->setFixedHeight(Theme::kBarControl);
        bar->addWidget(opens);
    }
    bar->addWidget(Divider());

    undo_action_->setIcon(Icons::Of(Icons::Glyph::Undo, Theme::kSoft));
    redo_action_->setIcon(Icons::Of(Icons::Glyph::Redo, Theme::kSoft));
    QAction* history = commands_->Action("view.history");
    history->setIcon(Icons::Of(Icons::Glyph::Clock, Theme::kSoft));
    for (QAction* action : {undo_action_, redo_action_, history}) {
        auto* button = new QToolButton;
        button->setObjectName("bar_icon");
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setFixedSize(Theme::kBarControl, Theme::kBarControl);
        bar->addWidget(button);
    }

    bar->addWidget(Spacer());
    bar->addWidget(BuildSearchField());
    bar->addWidget(Spacer());

    auto* trailing = new QWidget;
    trailing->setObjectName("trailing");
    auto* trailing_line = new QHBoxLayout(trailing);
    trailing_line->setContentsMargins(0, 0, 0, 0);
    trailing_line->setSpacing(6);
    document_icon_ = new QLabel;
    document_icon_->setPixmap(Icons::Drawn(Icons::Glyph::File, Theme::kSoft, 14));
    trailing_line->addWidget(document_icon_);
    document_state_ = new QLabel;
    document_state_->setObjectName("document_state");
    trailing_line->addWidget(document_state_);
    document_edits_ = new QLabel;
    document_edits_->setObjectName("document_edits");
    trailing_line->addWidget(document_edits_);
    trailing_line->addWidget(Divider());
    trailing_ = bar->addWidget(trailing);

    project_button_ = new QToolButton;
    project_button_->setObjectName("project");
    project_button_->setPopupMode(QToolButton::InstantPopup);
    project_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    project_button_->setIcon(Icons::Of(Icons::Glyph::Folder, Theme::kSoft, 14));
    project_button_->setFixedHeight(Theme::kBarControl);
    auto* project_menu = new QMenu(project_button_);
    commands_->ShowAvailabilityIn(project_menu);
    project_menu->addAction(commands_->Action("project.show_folder"));
    project_menu->addAction(commands_->Action("project.export"));
    project_menu->addAction(commands_->Action("project.close"));
    project_button_->setMenu(project_menu);
    project_button_action_ = bar->addWidget(project_button_);

    export_button_ = new QToolButton;
    export_button_->setObjectName("export");
    export_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    export_button_->setIcon(Icons::Of(Icons::Glyph::Upload, Theme::kAmber, 15));
    export_button_->setFixedHeight(Theme::kBarControl);
    connect(export_button_, &QToolButton::clicked, this,
            [this] { commands_->Run("project.export"); });
    export_button_action_ = bar->addWidget(export_button_);

    auto* save = new QToolButton;
    save->setObjectName("save");
    save->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    QAction* save_action = commands_->Action("file.save");
    save_action->setIcon(Icons::Of(Icons::Glyph::Save, Theme::OnAccent(), 15));
    save->setDefaultAction(save_action);
    save->setFixedHeight(Theme::kBarControl);
    save_ = bar->addWidget(save);
}

void Window::BuildStatusBar() {
    host_status_ = StatusLabel("host_status");
    stage_status_ = StatusLabel("stage_status");
    frame_status_ = StatusLabel("frame_status");
    chosen_status_ = StatusLabel("chosen_status");
    snap_status_ = StatusLabel("snap_status");
    zoom_status_ = StatusLabel("zoom_status");
    statusBar()->setFixedHeight(Theme::kStatusHeight);
    statusBar()->setSizeGripEnabled(false);

    auto* left = new QWidget;
    left->setObjectName("status_left");
    auto* line = new QHBoxLayout(left);
    line->setContentsMargins(4, 0, 0, 0);
    line->setSpacing(6);
    host_dot_ = new QLabel;
    host_dot_->setObjectName("host_dot");
    host_dot_->setFixedSize(kDotSide, kDotSide);
    line->addWidget(host_dot_);
    auto* named = new QLabel(tr("Preview host"));
    named->setObjectName("host_named");
    line->addWidget(named);
    line->addWidget(host_status_);
    line->addSpacing(kStatusGap);
    line->addWidget(stage_status_);
    line->addSpacing(kStatusGap);
    line->addWidget(frame_status_);
    statusBar()->addWidget(left);
    connect(statusBar(), &QStatusBar::messageChanged, left,
            [left](const QString& message) { left->setVisible(message.isEmpty()); });

    for (QLabel* label : {chosen_status_, snap_status_, zoom_status_})
        statusBar()->addPermanentWidget(label);
    connect(commands_->Action("view.snap"), &QAction::toggled, this, [this] { RefreshStatus(); });
}

void Window::OpenSearch() {
    std::vector<SearchItem> items;
    for (const QString& id : commands_->Ids()) {
        if (id == QStringLiteral("edit.search")) continue;
        const QAction* action = commands_->Action(id);
        const std::optional<QString> refused = commands_->Refusal(id);
        QString detail = refused.value_or(QString());
        if (!refused && action->isCheckable()) detail = action->isChecked() ? tr("On") : tr("Off");
        items.push_back(SearchItem{.category = CategoryOf(id),
                                   .text = QString(action->text()).remove('&'),
                                   .detail = detail,
                                   .keys = action->shortcut().toString(QKeySequence::NativeText),
                                   .available = !refused,
                                   .run = [this, id] { commands_->Run(id); }});
    }
    AddDepthResults(items);
    if (file_) {
        std::vector<Document::Node> animations;
        AddAnimations(file_->Nodes(), animations);
        for (const Document::Node& node : animations) {
            const QString path = QString::fromStdString(node.path);
            items.push_back(
                SearchItem{.category = tr("Open"),
                           .text = tr("Animation %1").arg(QString::fromStdString(node.name)),
                           .detail = {},
                           .keys = {},
                           .available = true,
                           .run = [this, path] { SelectEntry(path); }});
        }
    }
    search_->Open(std::move(items));
}

void Window::AddDepthResults(std::vector<SearchItem>& items) {
    if (!file_ || animation_path_.empty()) return;
    const auto animation = file_->ReadAnimation(animation_path_);
    if (!animation) return;
    const auto clip = Document::DescribeClip(*animation, clip_);
    if (!clip) return;
    std::map<uint16_t, QString> names;
    for (const Document::CharacterSummary& one :
         Document::Characters(*animation, file_->ShapeImages(animation_path_)))
        names.emplace(one.id, QString::fromStdString(one.label));
    for (const Document::DepthRow& row : clip->depths) {
        if (row.spans.empty()) continue;
        const auto first = row.shows.begin();
        const QString shows = first != row.shows.end() && names.contains(first->second)
                                  ? names.at(first->second)
                                  : QString();
        const uint16_t depth = row.depth;
        const std::vector<Document::Span> spans = row.spans;
        items.push_back(SearchItem{
            .category = tr("Go to"),
            .text = shows.isEmpty() ? tr("Depth %1").arg(depth)
                                    : tr("Depth %1, %2").arg(depth).arg(shows),
            .detail =
                tr("Frames %1 to %2").arg(spans.front().first_frame).arg(spans.back().last_frame),
            .keys = {},
            .available = true,
            .run = [this, depth, spans] {
                const bool held = std::ranges::any_of(spans, [this](const Document::Span& span) {
                    return span.first_frame <= frame_ && frame_ <= span.last_frame;
                });
                if (!held) JumpToFrame(spans.front().first_frame);
                ChooseDepth(depth);
            }});
    }
}

void Window::ShowProjectFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(project_folder_));
}

void Window::RefreshTopBar() {
    if (document_state_ == nullptr) return;
    trailing_->setVisible(file_.has_value());
    save_->setVisible(file_.has_value());
    if (!file_) {
        document_state_->clear();
        document_edits_->clear();
    } else {
        document_state_->setText(QFileInfo(document_path_).fileName());
        const std::optional<std::size_t> steps = history_.StepsFromSaved();
        if (steps == std::size_t{0}) {
            document_edits_->setText(tr("saved"));
        } else if (steps == std::size_t{1}) {
            document_edits_->setText(tr("unsaved, 1 edit"));
        } else if (steps) {
            document_edits_->setText(tr("unsaved, %1 edits").arg(*steps));
        } else {
            document_edits_->setText(tr("unsaved"));
        }
        document_edits_->setStyleSheet(
            QString("color: %1;")
                .arg((steps == std::size_t{0} ? Theme::kFaint : Theme::kAmber).name()));
    }
    project_button_action_->setVisible(project_.has_value());
    export_button_action_->setVisible(project_.has_value());
    if (!project_) return;
    project_button_->setText(tr("Project %1").arg(QFileInfo(project_folder_).fileName()));
    const std::size_t waiting = file_ ? Document::AwaitingExport(*file_, *project_) : 0;
    export_button_->setText(waiting == 0 ? tr("Export to IFS")
                                         : tr("Export to IFS (%1)").arg(waiting));
    const std::optional<QString> refused = commands_->Refusal("project.export");
    export_button_->setEnabled(!refused);
    export_button_->setToolTip(refused.value_or(
        waiting == 0 ? tr("The IFS holds everything the project writes")
                     : tr("%1 IFS entries differ from what the last export wrote").arg(waiting)));
}

void Window::RefreshStatus() {
    if (host_status_ == nullptr) return;
    host_status_->setText(host_.Running() ? tr("ready, %1 ms a frame").arg(render_ms_)
                                          : tr("not running"));
    host_dot_->setStyleSheet(
        QString("background: %1;").arg((host_.Running() ? Theme::kGreen : Theme::kEdge).name()));
    snap_status_->setText(commands_->Action("view.snap")->isChecked() ? tr("Snap on")
                                                                      : tr("Snap off"));
    const double scale = viewport_->StageScale();
    zoom_status_->setText(scale > 0 ? tr("Stage %1%").arg(std::lround(scale * 100)) : QString());
    RefreshStageBar();
    const std::vector<uint16_t> chosen = SelectedDepths();
    const std::size_t keys = timeline_->SelectedKeys().size();
    if (keys == 1) {
        chosen_status_->setText(tr("1 keyframe selected"));
    } else if (keys > 1) {
        chosen_status_->setText(tr("%1 keyframes selected").arg(keys));
    } else if (chosen.size() > 1) {
        chosen_status_->setText(tr("%1 depths chosen").arg(chosen.size()));
    } else if (chosen.size() == 1) {
        chosen_status_->setText(tr("Depth %1 chosen").arg(chosen.front()));
    } else {
        chosen_status_->clear();
    }
    if (!file_ || animation_path_.empty()) {
        stage_status_->clear();
        frame_status_->clear();
        return;
    }
    frame_status_->setText(tr("Frame %1 of %2").arg(frame_).arg(ClipFrameCount()));
}

void Window::ShowStageStatus(const AfpAnimation::Animation& animation) {
    const Document::StageSize stage = Document::StageSizeOf(animation);
    stage_status_->setText(tr("Stage %1 × %2, %3 fps")
                               .arg(stage.width)
                               .arg(stage.height)
                               .arg(Document::FrameRate(animation), 0, 'g', 4));
}

}
