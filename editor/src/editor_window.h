#pragma once

#include "editor_host.h"

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
#include "document/stage_move.h"
#include "document/timeline.h"
#include "preview/preview_client.h"
#include "preview/shared_texture.h"

#include <QImage>
#include <QMainWindow>
#include <QSize>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ads {
class CDockManager;
}

class QAction;
class QComboBox;
class QMenu;
class QScrollArea;
class QWidget;
class QPoint;
class QTableWidget;
class QTableWidgetItem;
class QTimer;
class QLineEdit;
class QListWidget;
class QTreeWidget;

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

class GraphEditor;
class Timeline;
class Viewport;

class Window : public QMainWindow {
    Q_OBJECT

public:
    Window();
    ~Window() override;

    void OpenDocument(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void BuildPanels();
    void BuildMenus();
    void ChooseGameDirectory();
    void StartHost(const QString& game_dir);
    void ChooseDocument();
    void CreateProject();
    void ChooseProject();
    void CloseProject();
    void SaveProject();
    void ExportToPackage();
    void AddProjectImage();
    void EditOwnedScript();
    void ReportDrift();
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
    void AddKeyActions();
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
    void StretchSelectedKeysBy(const Document::KeyStretch& stretch);
    void SelectMovedKeys(const std::vector<Document::KeyRef>& chosen,
                         std::vector<Document::KeyRef> moved);
    void EasyEaseSelectedKeys(Document::EasySide side);
    void ToggleHoldSelectedKeys();
    void ShowKeyMenu(const QPoint& where, const QString& property, uint32_t frame, bool on_key);
    bool ApplyKeyEdit(const QString& value);
    bool ApplyKeyFilterEdit(const QString& field, const QString& value);
    void ShowInspectorMenu(const QPoint& where);
    void PickColour(QTableWidgetItem* cell);
    void StartAnimating();
    void OpenProject(const QString& folder);
    [[nodiscard]] std::string TargetBuild() const;
    void FillTree();
    void ShowSelectedEntry();
    void ShowAnimation(const std::string& name);
    void ShowFrame();
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
    [[nodiscard]] std::vector<uint16_t> SelectedDepths() const;
    void ChooseDepths(std::vector<uint16_t> depths);
    void RemoveChosenDepths(uint32_t frame);
    void MoveDepthsOnStage(const std::vector<Document::DepthOffset>& moves, const QString& name,
                           bool finished);
    void ArrangeChosen(const QString& name,
                       const std::function<std::vector<Document::DepthOffset>(
                           const std::vector<Document::StageOutline>&)>& offsets,
                       std::size_t fewest);
    void AddAlignMenu(QMenu* edit);
    void ShowGhostsAround(uint32_t frame);
    void SaveImageAs(const QString& name);
    void AddImageFromFile();
    void ReplaceImageWithPicture(const QString& name);
    void SeekTo(uint32_t frame);
    [[nodiscard]] uint32_t ClipFrameCount() const;
    void JumpToFrame(int64_t frame);
    void GoToFrame();
    void StepToMark(Document::Direction direction);
    void AddStepActions(QMenu* menu);
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
    void ShowClipTimeline();
    [[nodiscard]] std::optional<Placeable>
    ChoosePlaceable(const AfpAnimation::Animation& animation);
    void PlaceImage(const std::string& image, const Document::DepthSpan& span);
    bool LoadViewportClip(const Document::File& file);
    void TogglePlay();
    void StepPlayback();
    void StopPlayback();
    [[nodiscard]] bool Playing() const;
    void ChooseDepth(uint32_t depth);
    [[nodiscard]] bool OutlinesMatchView() const;
    void UpdateOutlines(const AfpAnimation::Animation& animation);
    [[nodiscard]] std::vector<Document::PathPoint>
    PathOfDepth(const AfpAnimation::Animation& animation) const;
    void PickOnStage(double x, double y);
    void EditOnStage(uint16_t depth, const QString& name, const OwnedChange& owned,
                     const AnimationChange& baked, bool finished);
    void PreviewOnStage(AnimationChange change);
    void RunStagePreview();
    void MoveOnStage(uint16_t depth, double dx, double dy, bool finished);
    void ReshapeOnStage(uint16_t depth, double scale_x, double scale_y, double turn, bool finished);
    void MoveSpanInTime(uint16_t depth, uint32_t frame, int64_t by);
    void MoveSpanToDepth(uint16_t depth, uint32_t frame);
    void ArrangeDepth(Document::Arrange how, const QString& name);
    void SplitDepthAt(uint16_t depth, uint32_t frame);
    void SequenceChosenDepths(uint32_t frame);
    [[nodiscard]] std::optional<Document::Span> SpanNearPlayhead();
    void MoveEdgeToPlayhead(SpanEnd end);
    void TrimEdgeToPlayhead(SpanEnd end);
    void AddPlayheadMenu(QMenu* edit);
    void AddArrangeMenu(QMenu* edit);
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
    void FillLibrary(const AfpAnimation::Animation& animation,
                     const std::vector<Document::CharacterSummary>& characters);
    void ShowLibrarySprite(uint16_t sprite);
    void ShowLibraryMenu(const QPoint& where);
    bool CopySpanAt(uint16_t depth, uint32_t frame);
    void PasteSpanAt(uint32_t frame);
    void GroupDepthsIntoSprite(uint16_t depth, uint32_t frame);
    void UngroupSpriteAt(uint16_t depth, uint32_t frame);
    [[nodiscard]] std::vector<uint16_t> HiddenHere() const;
    [[nodiscard]] bool IsHidden(uint16_t depth) const;
    void ToggleHidden(uint16_t depth);
    void ShowEveryDepth();
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
    bool Save();
    bool SaveAs();
    bool OfferToSave();
    void RefreshState();
    void ReportProblem(const QString& what);
    void ReportOnce(const QString& what);

    ads::CDockManager* docks_ = nullptr;
    QTreeWidget* package_tree_ = nullptr;
    QTreeWidget* library_ = nullptr;
    QLineEdit* package_filter_ = nullptr;
    QLineEdit* library_filter_ = nullptr;
    QAction* onion_action_ = nullptr;
    QAction* path_action_ = nullptr;
    std::vector<uint16_t> selected_depths_;
    QListWidget* history_list_ = nullptr;
    QTableWidget* inspector_ = nullptr;
    QAction* background_action_ = nullptr;
    Viewport* viewport_ = nullptr;
    Timeline* timeline_ = nullptr;
    GraphEditor* graph_ = nullptr;
    QTimer* resize_timer_ = nullptr;
    QTimer* play_timer_ = nullptr;
    QComboBox* clip_box_ = nullptr;
    QAction* play_action_ = nullptr;
    QAction* loop_action_ = nullptr;
    QAction* undo_action_ = nullptr;
    QAction* redo_action_ = nullptr;
    QAction* create_project_action_ = nullptr;
    QAction* close_project_action_ = nullptr;
    QAction* export_action_ = nullptr;
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
    bool filling_inspector_ = false;
    std::optional<SharedTexture::Reader> reader_;
    QSize stage_size_;
    std::map<uint16_t, Document::Box> shape_bounds_;
    std::string shape_bounds_path_;
    std::optional<AnimationChange> pending_preview_;
    bool preview_scheduled_ = false;
    bool previewed_ = false;
    QString last_error_;
};

}
