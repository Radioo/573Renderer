#pragma once

#include "editor_host.h"
#include "editor_clip_view.h"
#include "editor_open.h"

#include "document/document.h"
#include "document/authored.h"
#include "document/characters.h"
#include "document/clip.h"
#include "document/group_sprite.h"
#include "document/hidden_depths.h"
#include "document/span_clipboard.h"
#include "document/history.h"
#include "document/inspector.h"
#include "document/key_selection.h"
#include "document/motion_path.h"
#include "document/playback.h"
#include "document/project.h"
#include "formats/afp_animation.h"
#include "support/expected.h"
#include "document/outline.h"
#include "document/place_image.h"
#include "document/span_arrange.h"
#include "document/stage_align.h"
#include "document/stage_bounds.h"
#include "document/stage_fit.h"
#include "document/stage_move.h"
#include "document/timeline.h"
#include "preview/preview_client.h"
#include "preview/shared_texture.h"

#include <QImage>
#include <QMainWindow>
#include <QSize>
#include <QIcon>
#include <QString>
#include <QThreadPool>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ads {
class CDockManager;
class CDockWidget;
}

class QAction;
class QComboBox;
class QLabel;
class QToolButton;
class QMenu;
class QScrollArea;
class QWidget;
class QPoint;
class QTableWidget;
class QTableWidgetItem;
class QTimer;
class QLineEdit;
class QListWidget;
class QDragEnterEvent;
class QDropEvent;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace Editor {

using AnimationChange =
    std::function<Support::Expected<void, std::string>(AfpAnimation::Animation&)>;

enum class SpanEnd : uint8_t { Start, End };

struct Placeable {
    std::optional<uint16_t> character;
    std::string image;
};

using DocumentChange = std::function<Support::Expected<void, std::string>(Document::File&)>;

using AuthoredChange =
    std::function<Support::Expected<void, std::string>(Document::AuthoredDepth&)>;

using OwnedChange = std::function<Support::Expected<void, std::string>(
    Document::AuthoredDepth&, const Document::BakedDepth&)>;

class Commands;
class Inspector;
class SelectionBar;
class Notices;
class StageBar;
class StartScreen;
class PanelTabs;
class Busy;
struct OpenedPackage;

struct FrameExport {
    QString folder;
    QString base;
    uint32_t frame = 0;
    uint32_t first = 0;
    uint32_t last = 0;
    int saved = 0;
    bool stop = false;
};

struct ExportedProject {
    std::optional<Document::File> file;
    std::optional<Document::Project> project;
    QString refusal;
};

struct PendingLoad {
    bool fresh = false;
    Document::File document;
    QString what;
    std::function<void(bool)> then;
};
class ToolStrip;
class Popover;
class TimelineBar;
class CommandSearch;
struct SearchItem;
class GraphEditor;
class Timeline;
class Viewport;

class Window : public QMainWindow {
    Q_OBJECT

public:
    Window();
    ~Window() override;

    void OpenDocument(const QString& path);
    [[nodiscard]] bool Loading() const;

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void BuildPanels();
    void BuildMenus();
    void AddToolsMenu();
    QMenu* AddMenu(QMenu* parent, const QString& title);
    void AddFileMenu();
    void AddEditMenu();
    void AddDepthMenu();
    void AddKeyframeMenu();
    void AddClipMenu();
    void AddPlaybackMenu();
    void ShowRefusal(const QString& reason);
    void ShowResult(const QString& what, bool undoable);
    void BuildTopBar();
    [[nodiscard]] QWidget* BuildSearchField();
    std::vector<QMenu*> menus_;
    void BuildStatusBar();
    void ShowProjectFolder();
    void OpenSearch();
    void AddDepthResults(std::vector<SearchItem>& items);
    void RefreshTopBar();
    void RefreshStatus();
    void ShowStageStatus(const AfpAnimation::Animation& animation);
    [[nodiscard]] std::optional<QString> NeedsDocument() const;
    [[nodiscard]] std::optional<QString> NeedsAnimation() const;
    [[nodiscard]] std::optional<QString> NeedsDepth() const;
    [[nodiscard]] std::optional<QString> NeedsDepths(std::size_t fewest) const;
    [[nodiscard]] std::optional<QString> OwnedRefusal(uint16_t depth, uint32_t frame,
                                                      const QString& doing) const;
    [[nodiscard]] std::optional<QString> ChosenDepthRefusal(const QString& doing) const;
    [[nodiscard]] std::optional<QString> ChosenDepthsRefusal(const QString& doing) const;
    [[nodiscard]] std::optional<QString> NeedsOwnedDepth() const;
    [[nodiscard]] std::optional<QString> NeedsKeys(std::size_t fewest) const;
    [[nodiscard]] std::optional<QString> NeedsProject() const;
    [[nodiscard]] std::optional<QString> WorkAreaRefusal() const;
    [[nodiscard]] std::optional<QString> AnchorRefusal(uint16_t depth) const;
    [[nodiscard]] std::optional<QString> FitRefusal() const;
    [[nodiscard]] std::optional<QString> StageDepthsRefusal(std::size_t fewest) const;
    void InsertFrameAt(uint32_t frame);
    void RemoveFrameAt(uint32_t frame);
    void AddDepthHere();
    void AddLabelAt(uint32_t frame);
    void RenameLabel(const QString& label);
    void ToggleCameraAt(uint32_t frame);
    void ChooseGameDirectory();
    void StartHost(const QString& game_dir);
    void ChooseDocument();
    void CreateProject();
    [[nodiscard]] QString SuggestedProjectFolder() const;
    bool MakeProjectIn(const QString& folder);
    bool OpenOrMakeProject(const QString& folder);
    void ChooseProject();
    void CloseProject();
    void SaveProject();
    void ExportToPackage();
    void AddProjectImage();
    void EditOwnedScript();
    void ReportDrift();
    void FinishProjectOpen(const QString& ifs, const QString& folder, Document::Project project);
    void OwnSelectedDepth(uint32_t frame);
    void DetachSelectedDepth();
    [[nodiscard]] const Document::AuthoredDepth* AuthoredAt(uint16_t depth, uint32_t frame) const;
    [[nodiscard]] std::optional<std::size_t> AuthoredIndexAt(uint16_t depth, uint32_t frame) const;
    void ShowKeysForDepth(const Document::AuthoredDepth* owned);
    bool EditAuthored(const QString& name, const AuthoredChange& change);
    void ChooseKey(const QString& property, uint32_t frame);
    void FocusKey(const QString& property, uint32_t frame);
    void ApplyGraphMove(const QString& property, uint32_t frame, uint32_t to_frame,
                        const std::vector<int64_t>& value);
    bool EditOwned(const QString& name, const OwnedChange& change);
    bool CopySelectedKeys();
    void CopySelection();
    void CutSelection();
    void PasteClipboard();
    void PasteCopiedKeys();
    void RemoveSelectedKeys();
    void SelectAllKeys();
    void ShiftSelectedKeys(int64_t by);
    void ReverseSelectedKeys();
    void StretchSelectedKeys();
    void SimplifySelectedKeys();
    [[nodiscard]] std::optional<std::size_t>
    KeysAfterSimplify(const std::vector<Document::KeyRef>& chosen, int64_t tolerance) const;
    void WiggleSelectedKeys();
    void StretchSelectedKeysBy(const Document::KeyStretch& stretch);
    void SelectMovedKeys(const std::vector<Document::KeyRef>& chosen,
                         std::vector<Document::KeyRef> moved);
    void EasyEaseSelectedKeys(Document::EasySide side);
    void ToggleHoldSelectedKeys();
    void ShowKeyMenu(const QPoint& where, const QString& property, uint32_t frame, bool on_key);
    bool ApplyKeyEdit(const QString& value);
    bool ApplyKeyFilterEdit(const QString& field, const QString& value);
    void ShowInspectorMenu(const QPoint& where);
    void ShowInspectorSubject(const AfpAnimation::Animation& animation);
    void RefreshSelectionBar();
    void RefreshTimelineBar();
    void RefreshStageBar();
    void ShowGraphPanel(bool graph);
    [[nodiscard]] QWidget* BarAnchor(const QString& id) const;
    void PreviewAuthored(const AuthoredChange& change);
    void CancelPreview(std::vector<Document::KeyRef> keys);
    void ApplyViewEdit(const QString& label, const std::vector<double>& values);
    void RefreshEaseSection();
    [[nodiscard]] QWidget* BuildGraphPanel();
    void FillGraphProperties(const Document::AuthoredDepth& owned);
    void RefreshGraphTracks(const Document::AuthoredDepth* owned);
    void ApplyGraphEase(const QString& property, uint32_t frame, const Document::Bezier& bezier);
    void ShowInspectorExtras(const AfpAnimation::Animation& animation, uint16_t depth,
                             const QString& character);
    void ReplaceCharacterOnDepth();
    void EditPlacementFieldOnDepth(const QString& field, const QString& value);
    void AddFilterOnDepth(bool hsv);
    void RemoveFilterOnDepth(const QString& name);
    void ApplySelectedKeysEase(Document::Ease ease, const Document::Bezier& bezier);
    void ToggleViewKeying(const QString& label, bool animated);
    void PickViewColour(const QString& label);
    void PickColour(QTableWidgetItem* cell);
    void StartAnimating();
    void OpenProject(const QString& folder);
    [[nodiscard]] std::string TargetBuild() const;
    void FillTree();
    void ReloadRows();
    void ShowBusy(const QString& what);
    void JobStarted();
    void JobFinished();
    void FinishOpen(const QString& path, OpenedPackage opened);
    void RefreshStartScreen();
    [[nodiscard]] QWidget* BuildPackageActions();
    void RememberRecent(const QString& path);
    [[nodiscard]] QWidget* BuildPackageTabs();
    void FillAnimationRows();
    void FillImageRows();
    void ChooseEntryFrom(QTreeWidget& tree);
    void RenameEntryRow(QTreeWidgetItem* item);
    void ShowSelectedEntry();
    void ShowAnimation(const std::string& name);
    void ShowFrame();
    void OpenDropped(const QString& path);
    void ApplyFieldEdit(QTableWidgetItem* item);
    bool EditAnimation(const QString& name, const AnimationChange& change);
    void ShowTimelineMenu(const QPoint& where, uint32_t frame, const QString& label);
    void ShowPackageMenu(const QPoint& where);
    bool EditDocument(const QString& name, const DocumentChange& change);
    struct AnimationSource {
        std::optional<Document::File> other;
        std::string path;
    };
    [[nodiscard]] std::optional<AnimationSource> ChooseAnimationSource();
    void AddNewAnimation();
    void RemoveAnimation(const std::string& path, const QString& name);
    void RenameAnimationEntry(const std::string& path, const QString& name);
    void ApplyAnimationRename(const std::string& path, const QString& name, const QString& wanted);
    void DuplicateAnimationEntry(const std::string& path, const QString& name);
    void RemoveUnusedDefinitionsFrom(const std::string& path, const QString& name);
    [[nodiscard]] bool ProjectOwnsDepthsIn(const std::string& path) const;
    void CloseAnimation();
    void DrawBackground(bool drawn);
    void AddViewMenu();
    void SelectEntry(const QString& path);
    void Undo();
    void Redo();
    void ShowRestored();
    void FillInspector(const std::vector<Document::InspectedRow>& rows);
    [[nodiscard]] static std::vector<Document::InspectedRow>
    ReadOnlyRows(const std::vector<Document::Field>& fields);
    void RenderFrame();
    struct ShownFrame {
        QImage image;
        PreviewClient::Frame frame;
    };
    [[nodiscard]] Support::Expected<ShownFrame, std::string> ReadFrame();
    void SaveFrameAs();
    void SaveFramesAs();
    void SaveNextFrame();
    void FinishFrames(const QString& refusal);
    void StopFrames();
    [[nodiscard]] std::vector<uint16_t> SelectedDepths() const;
    void ChooseDepths(std::vector<uint16_t> depths);
    void RemoveChosenDepths(uint32_t frame);
    void MoveDepthsOnStage(const std::vector<Document::DepthOffset>& moves, const QString& name,
                           bool finished);
    void ArrangeChosen(const QString& name,
                       const std::function<std::vector<Document::DepthOffset>(
                           const std::vector<Document::StageOutline>&)>& offsets);
    void ShowGhostsAround(uint32_t frame);
    void SaveImageAs(const QString& name);
    void AddImageFromFile();
    void ReplaceImageWithPicture(const QString& name);
    void SeekTo(uint32_t frame);
    [[nodiscard]] uint32_t ClipFrameCount() const;
    void JumpToFrame(int64_t frame);
    void GoToFrame();
    void StepToMark(Document::Direction direction);
    void SetWorkArea(std::optional<Document::WorkArea> area);
    void TrimClipToWorkArea();
    void ExtractWorkArea();
    void LiftWorkArea();
    [[nodiscard]] std::optional<Document::Span> WorkAreaToEdit();
    void SeekViewport(uint32_t frame);
    QWidget* BuildTimelinePanel(QScrollArea* timeline_area);
    void FillClips();
    void RefillClipsKeepingChoice();
    void NameShownSpriteExport();
    void ChooseClip(int index);
    void EnterSprite(uint16_t character);
    [[nodiscard]] std::optional<Document::StageOutline>
    ContextOf(const AfpAnimation::Animation& animation) const;
    [[nodiscard]] Document::StageOffset UnderContext(double dx, double dy) const;
    void RefreshContext(const AfpAnimation::Animation& animation);
    [[nodiscard]] std::vector<Document::StageOutline>
    OutlinesOnStage(const AfpAnimation::Animation& animation) const;
    void EnterSpriteAt(double x, double y);
    void LeaveClip();
    [[nodiscard]] int ClipIndexOf(const Document::ClipId& wanted) const;
    [[nodiscard]] QString ClipName(int index) const;
    void ShowClipTimeline();
    void ApplyClipView(ClipView view);
    void SayWhichClip(int index);
    [[nodiscard]] std::optional<Placeable>
    ChoosePlaceable(const AfpAnimation::Animation& animation);
    void PlaceImage(const std::string& image, const Document::DepthSpan& span);
    void LoadIntoHost(bool fresh, Document::File document, const QString& what,
                      std::function<void(bool)> then);
    [[nodiscard]] bool HostReady() const;
    void TogglePlay();
    void StepPlayback();
    void StopPlayback();
    [[nodiscard]] bool Playing() const;
    void ChooseDepth(uint32_t depth);
    [[nodiscard]] bool OutlinesMatchView() const;
    void UpdateOutlines(const AfpAnimation::Animation& animation);
    void CentreChosenAnchor();
    void MoveAnchorOnStage(uint16_t depth, double dx, double dy);
    void FitChosenToStage(Document::StageFit fit);
    [[nodiscard]] bool RefuseOwnedAnchor(uint16_t depth);
    [[nodiscard]] std::vector<Document::PathPoint>
    PathOfDepth(const AfpAnimation::Animation& animation) const;
    void PickOnStage(double x, double y);
    void EditOnStage(uint16_t depth, const QString& name, const OwnedChange& owned,
                     const AnimationChange& baked, bool finished);
    void PreviewOnStage(AnimationChange change);
    void RunStagePreview();
    void MoveOnStage(uint16_t depth, double stage_dx, double stage_dy, bool finished);
    [[nodiscard]] bool SketchMove(uint16_t depth, double dx, double dy, bool finished);
    void ReshapeOnStage(uint16_t depth, double scale_x, double scale_y, double turn, bool finished);
    void MoveSpanInTime(uint16_t depth, uint32_t frame, int64_t by);
    void MoveSpanToDepth(uint16_t depth, uint32_t frame);
    void MoveSpanOntoDepth(uint16_t depth, uint32_t frame, uint16_t to);
    void ArrangeDepth(Document::Arrange how, const QString& name);
    void SplitDepthAt(uint16_t depth, uint32_t frame);
    void SequenceChosenDepths(uint32_t frame);
    [[nodiscard]] std::optional<Document::Span> SpanNearPlayhead();
    void MoveEdgeToPlayhead(SpanEnd end);
    void TrimEdgeToPlayhead(SpanEnd end);
    void DuplicateSpanToDepth(uint16_t depth, uint32_t frame);
    void DuplicateChosenDepth();
    void DuplicateSpanOnto(uint16_t depth, uint32_t frame, uint16_t to);
    [[nodiscard]] std::optional<uint16_t> NextFreeDepth(uint16_t fallback);
    [[nodiscard]] std::optional<uint16_t> AskForFreeDepth(const QString& title, uint16_t fallback);
    void PlaceDroppedCharacter(uint16_t character, double x, double y);
    void MoveLabelTo(const QString& label, uint32_t frame);
    void DropCharacterOnTimeline(uint16_t character, uint32_t frame, std::optional<uint16_t> row);
    void DuplicateLibrarySprite(uint16_t sprite);
    void NewEmptySprite();
    void UseCharacterOnDepth(uint16_t character, uint16_t depth);
    [[nodiscard]] std::optional<uint32_t> AskForLastFrame(uint32_t first);
    void AddCharacterDepth(uint16_t depth, uint16_t character, uint32_t first, uint32_t last);
    [[nodiscard]] QTreeWidget* BuildLibrary();
    [[nodiscard]] QListWidget* BuildHistory();
    void FillHistory();
    void JumpInHistory(int row);
    void FillLibrary(const std::vector<LibraryRow>& characters);
    void ShowLibraryKinds();
    void PlaceLibraryCharacter(uint16_t character);
    [[nodiscard]] QIcon LibraryTile(const std::map<uint16_t, std::string>& images,
                                    uint16_t character);
    [[nodiscard]] QWidget* BuildLibraryPanel();
    void ShowLibrarySprite(uint16_t sprite);
    void ShowLibraryMenu(const QPoint& where);
    bool CopySpanAt(uint16_t depth, uint32_t frame);
    void PasteSpanAt(uint32_t frame);
    void GroupDepthsIntoSprite(uint16_t depth, uint32_t frame);
    void GroupIntoSpriteOver(const Document::GroupRange& range);
    void UngroupSpriteAt(uint16_t depth, uint32_t frame);
    [[nodiscard]] std::vector<uint16_t> HiddenHere() const;
    [[nodiscard]] bool IsHidden(uint16_t depth) const;
    void ToggleHidden(uint16_t depth);
    void ShowEveryDepth();
    void UnlockEveryDepth();
    void UpdateViewRows();
    [[nodiscard]] bool IsLocked(uint16_t depth) const;
    void ToggleLocked(uint16_t depth);
    void SoloDepth(uint16_t depth);
    [[nodiscard]] std::vector<Document::StageOutline>
    VisibleOutlines(const AfpAnimation::Animation& animation) const;
    [[nodiscard]] bool OwnsDepthIn(const Document::GroupRange& range) const;
    void TrimSpanOnTimeline(uint16_t depth, uint32_t frame, uint32_t first, uint32_t last);
    void ResizeViewport();
    void Reload();
    void Save(std::function<void(bool)> then = {});
    void SaveAs(std::function<void(bool)> then = {});
    void OfferToSave(std::function<void(bool)> then);
    void AskForDocument();
    void WriteDocument(const QString& path, std::function<void(bool)> then);
    void RefreshState();
    void ReportProblem(const QString& what);
    void ReportOnce(const QString& what);

    ads::CDockManager* docks_ = nullptr;
    QTreeWidget* package_tree_ = nullptr;
    QTreeWidget* animations_tree_ = nullptr;
    QTreeWidget* images_tree_ = nullptr;
    QTreeWidget* library_ = nullptr;
    QLineEdit* package_filter_ = nullptr;
    QLineEdit* library_filter_ = nullptr;
    std::vector<Document::CharacterKind> library_kinds_;
    std::map<uint16_t, QIcon> library_tiles_;
    std::string library_tiles_path_;
    QAction* onion_action_ = nullptr;
    QAction* path_action_ = nullptr;
    std::vector<uint16_t> selected_depths_;
    QListWidget* history_list_ = nullptr;
    QTableWidget* inspector_ = nullptr;
    Inspector* inspector_panel_ = nullptr;
    SelectionBar* selection_bar_ = nullptr;
    Popover* popover_ = nullptr;
    Notices* notices_ = nullptr;
    StageBar* stage_bar_ = nullptr;
    StartScreen* start_ = nullptr;
    PanelTabs* package_tabs_ = nullptr;
    QLineEdit* animations_filter_ = nullptr;
    QLineEdit* images_filter_ = nullptr;
    QLabel* library_of_ = nullptr;
    QStackedWidget* centre_ = nullptr;
    QThreadPool pool_;
    Busy* busy_ = nullptr;
    bool host_busy_ = false;
    bool closing_ = false;
    std::optional<PendingLoad> pending_load_;
    std::optional<FrameExport> frames_;
    bool view_busy_ = false;
    bool view_again_ = false;
    bool view_retried_ = false;
    uint32_t model_frames_ = 0;
    bool opening_ = false;
    int jobs_ = 0;
    PackageRows rows_;

    ToolStrip* tool_strip_ = nullptr;
    TimelineBar* timeline_bar_ = nullptr;
    ads::CDockWidget* timeline_dock_ = nullptr;
    ads::CDockWidget* graph_dock_ = nullptr;
    std::vector<Document::AnimationLabel> shown_labels_;
    double shown_rate_ = 0;
    QAction* background_action_ = nullptr;
    Viewport* viewport_ = nullptr;
    Timeline* timeline_ = nullptr;
    GraphEditor* graph_ = nullptr;
    QListWidget* graph_properties_ = nullptr;
    std::vector<std::string> graph_hidden_;
    QTimer* resize_timer_ = nullptr;
    QTimer* play_timer_ = nullptr;
    std::vector<Document::ClipSummary> clips_;
    int clip_index_ = 0;
    QAction* play_action_ = nullptr;
    QAction* loop_action_ = nullptr;
    QAction* undo_action_ = nullptr;
    QAction* redo_action_ = nullptr;
    Commands* commands_ = nullptr;
    CommandSearch* search_ = nullptr;
    ads::CDockWidget* history_dock_ = nullptr;
    QLabel* document_state_ = nullptr;
    QLabel* document_icon_ = nullptr;
    QLabel* document_edits_ = nullptr;
    QToolButton* project_button_ = nullptr;
    QToolButton* export_button_ = nullptr;
    QAction* project_button_action_ = nullptr;
    QAction* export_button_action_ = nullptr;
    QAction* trailing_ = nullptr;
    QAction* save_ = nullptr;
    QLabel* host_status_ = nullptr;
    QLabel* host_dot_ = nullptr;
    QLabel* stage_status_ = nullptr;
    QLabel* frame_status_ = nullptr;
    QLabel* chosen_status_ = nullptr;
    QLabel* snap_status_ = nullptr;
    QLabel* zoom_status_ = nullptr;
    qint64 render_ms_ = 0;
    Host host_;
    std::optional<Document::File> file_;
    Document::History history_;
    QString document_path_;
    std::optional<Document::Project> project_;
    QString project_folder_;
    std::vector<Document::AuthoredDepth> authored_;
    std::vector<Document::DepthInClip> hidden_;
    std::vector<Document::DepthInClip> locked_;
    std::optional<Document::CopiedSpan> copied_span_;
    QString key_property_;
    std::optional<uint32_t> key_frame_;
    std::optional<Document::KeyClip> copied_keys_;
    bool keys_copied_last_ = false;
    std::string package_name_;
    std::string animation_path_;
    std::string animation_name_;
    std::optional<uint32_t> depth_;
    uint32_t frame_count_ = 0;
    uint32_t frame_ = 0;
    uint32_t root_frame_ = 0;
    Document::ClipId clip_;
    std::optional<Document::WorkArea> work_area_;
    bool symbol_shown_ = false;
    std::optional<Document::StageOutline> context_;
    QAction* context_action_ = nullptr;
    bool filling_inspector_ = false;
    bool filling_tree_ = false;
    std::optional<SharedTexture::Reader> reader_;
    QSize stage_size_;
    std::map<uint16_t, Document::Box> shape_bounds_;
    std::string shape_bounds_path_;
    std::optional<AnimationChange> pending_preview_;
    bool preview_scheduled_ = false;
    bool previewed_ = false;
    QString last_error_;
    struct Sketch {
        uint16_t depth = 0;
        uint32_t pressed = 0;
        Document::StageOffset latest;
        Document::SketchedOffsets offsets;
    };
    std::optional<Sketch> sketch_;
};

}
