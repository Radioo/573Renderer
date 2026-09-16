#pragma once

#include "editor_host.h"

#include "document/document.h"
#include "document/history.h"
#include "formats/afp_animation.h"
#include "support/expected.h"
#include "document/outline.h"
#include "preview/shared_texture.h"

#include <QMainWindow>
#include <QString>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ads {
class CDockManager;
}

class QAction;
class QPoint;
class QTableWidget;
class QTableWidgetItem;
class QTimer;
class QTreeWidget;

namespace Editor {

using AnimationChange =
    std::function<Support::Expected<void, std::string>(AfpAnimation::Animation&)>;

using DocumentChange = std::function<Support::Expected<void, std::string>(Document::File&)>;

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
    void FillTree();
    void ShowSelectedEntry();
    void ShowAnimation(const std::string& name);
    void ShowFrame();
    void ApplyFieldEdit(QTableWidgetItem* item);
    void EditAnimation(const QString& name, const AnimationChange& change);
    void ShowTimelineMenu(const QPoint& where, uint32_t frame, const QString& label);
    void ShowPackageMenu(const QPoint& where);
    void EditDocument(const QString& name, const DocumentChange& change);
    void Undo();
    void Redo();
    void ShowRestored();
    void FillInspector(const std::vector<Document::Field>& fields, bool editable);
    void RenderFrame();
    void SeekTo(uint32_t frame);
    void ChooseDepth(uint32_t depth);
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
    Viewport* viewport_ = nullptr;
    Timeline* timeline_ = nullptr;
    QTimer* resize_timer_ = nullptr;
    QAction* undo_action_ = nullptr;
    QAction* redo_action_ = nullptr;
    Host host_;
    std::optional<Document::File> file_;
    Document::History history_;
    QString document_path_;
    std::string package_name_;
    std::string animation_path_;
    std::string animation_name_;
    std::optional<uint32_t> depth_;
    uint32_t frame_count_ = 0;
    uint32_t frame_ = 0;
    bool filling_inspector_ = false;
    std::optional<SharedTexture::Reader> reader_;
    QString last_error_;
};

}
