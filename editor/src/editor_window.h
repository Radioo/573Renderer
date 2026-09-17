#pragma once

#include "editor_host.h"

#include "document/document.h"
#include "document/authored.h"
#include "document/clip.h"
#include "document/group_sprite.h"
#include "document/hidden_depths.h"
#include "document/history.h"
#include "document/inspector.h"
#include "document/key_selection.h"
#include "document/playback.h"
#include "document/project.h"
#include "formats/afp_animation.h"
#include "support/expected.h"
#include "document/outline.h"
#include "document/place_image.h"
#include "document/stage_bounds.h"
#include "preview/shared_texture.h"

#include <QMainWindow>
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
class QScrollArea;
class QWidget;
class QPoint;
class QTableWidget;
class QTableWidgetItem;
class QTimer;
class QTreeWidget;

namespace Editor {

using AnimationChange =
    std::function<Support::Expected<void, std::string>(AfpAnimation::Animation&)>;

struct Placeable {
    std::optional<uint16_t> character;
    std::string image;
};

using DocumentChange = std::function<Support::Expected<void, std::string>(Document::File&)>;

using AuthoredChange =
    std::function<Support::Expected<void, std::string>(Document::AuthoredDepth&)>;

using OwnedChange = std::function<Support::Expected<void, std::string>(
    Document::AuthoredDepth&, const Document::BakedDepth&)>;

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
    bool EditOwned(const QString& name, const OwnedChange& change);
    void AddKeyActions();
    void CopySelectedKeys();
    void PasteCopiedKeys();
    void RemoveSelectedKeys();
    void SelectAllKeys();
    void ShiftSelectedKeys(int64_t by);
    void ShowKeyMenu(const QPoint& where, const QString& property, uint32_t frame, bool on_key);
    bool ApplyKeyEdit(const QString& value);
    bool ApplyKeyFilterEdit(const QString& field, const QString& value);
    void ShowInspectorMenu(const QPoint& where);
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
    void SeekTo(uint32_t frame);
    void SeekViewport(uint32_t frame);
    QWidget* BuildTimelinePanel(QScrollArea* timeline_area);
    void FillClips();
    void RefillClipsKeepingChoice();
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
    void PickOnStage(double x, double y);
    void EditOnStage(uint16_t depth, const QString& name, const OwnedChange& owned,
                     const AnimationChange& baked, bool finished);
    void PreviewOnStage(AnimationChange change);
    void RunStagePreview();
    void MoveOnStage(uint16_t depth, double dx, double dy, bool finished);
    void ReshapeOnStage(uint16_t depth, double scale_x, double scale_y, double turn, bool finished);
    void MoveSpanInTime(uint16_t depth, uint32_t frame, int64_t by);
    void MoveSpanToDepth(uint16_t depth, uint32_t frame);
    void DuplicateSpanToDepth(uint16_t depth, uint32_t frame);
    void GroupDepthsIntoSprite(uint16_t depth, uint32_t frame);
    void UngroupSpriteAt(uint16_t depth, uint32_t frame);
    [[nodiscard]] std::vector<uint16_t> HiddenHere() const;
    [[nodiscard]] bool IsHidden(uint16_t depth) const;
    void ToggleHidden(uint16_t depth);
    void ShowEveryDepth();
    void UpdateHiddenRows();
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
    QTableWidget* inspector_ = nullptr;
    QAction* background_action_ = nullptr;
    Viewport* viewport_ = nullptr;
    Timeline* timeline_ = nullptr;
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
    std::vector<Document::HiddenDepth> hidden_;
    QString key_property_;
    std::optional<uint32_t> key_frame_;
    std::optional<Document::KeyClip> copied_keys_;
    std::string package_name_;
    std::string animation_path_;
    std::string animation_name_;
    std::optional<uint32_t> depth_;
    uint32_t frame_count_ = 0;
    uint32_t frame_ = 0;
    uint32_t root_frame_ = 0;
    Document::ClipId clip_;
    bool symbol_shown_ = false;
    bool filling_inspector_ = false;
    std::optional<SharedTexture::Reader> reader_;
    std::map<uint16_t, Document::Box> shape_bounds_;
    std::string shape_bounds_path_;
    std::optional<AnimationChange> pending_preview_;
    bool preview_scheduled_ = false;
    bool previewed_ = false;
    QString last_error_;
};

}
