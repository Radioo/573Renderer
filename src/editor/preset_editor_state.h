#pragma once

#include "editor/timeline_edits.h"
#include "editor/timeline_view.h"
#include "preset/doc/preset_document.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Editor {

enum class RequestKind : uint8_t {
    None,
    ClipProperties,
    AddCommand,
    AddTrack,
    DocumentProperties,
};

struct Request {
    RequestKind kind = RequestKind::None;
    std::string clip_id = {};
    std::string track_id = {};
    int frame = 0;
};

using DocPtr = std::shared_ptr<const Preset::Doc::Document>;

using Edit = std::function<bool(Preset::Doc::Document&)>;

inline constexpr int kUndoDepth = 200;

class State {
public:
    void LoadDocument(Preset::Doc::Document document);
    void Close();

    [[nodiscard]] bool Loaded() const { return document_ != nullptr; }
    [[nodiscard]] const Preset::Doc::Document& Document() const { return *document_; }
    [[nodiscard]] const DocPtr& Snapshot() const { return document_; }

    bool Apply(const Edit& edit);
    void BeginGesture();
    void EndGesture();

    bool Undo();
    bool Redo();
    void CollapseUndo(int depth);
    void ClearRedo();
    [[nodiscard]] int UndoDepth() const { return (int)past_.size(); }
    [[nodiscard]] int RedoDepth() const { return (int)future_.size(); }
    [[nodiscard]] bool Dirty() const { return dirty_; }
    [[nodiscard]] unsigned Revision() const { return revision_; }
    [[nodiscard]] unsigned LoadId() const { return load_id_; }

    [[nodiscard]] const std::vector<std::string>& Selection() const { return selection_; }
    [[nodiscard]] bool IsSelected(const std::string& clip_id) const;
    void Select(std::string clip_id);
    void ExtendSelection(std::string clip_id);
    void ToggleSelection(const std::string& clip_id);
    void SetSelection(std::vector<std::string> clip_ids);
    void ClearSelection();
    void DropMissingSelection();

    void PostRequest(Request request);
    [[nodiscard]] const Request& PendingRequest() const { return request_; }
    Request TakeRequest();

    void SetClipboard(std::vector<ClipboardClip> clips);
    [[nodiscard]] const std::vector<ClipboardClip>& Clipboard() const { return clipboard_; }

    [[nodiscard]] View& MutView() { return view_; }
    [[nodiscard]] const View& GetView() const { return view_; }

private:
    struct Entry {
        DocPtr document;
        std::vector<std::string> selection;
    };

    void Push();

    DocPtr document_;
    DocPtr saved_;
    std::vector<Entry> past_;
    std::vector<Entry> future_;
    std::vector<std::string> selection_;
    std::vector<ClipboardClip> clipboard_;
    Request request_;
    View view_;
    unsigned revision_ = 0;
    unsigned load_id_ = 0;
    int gesture_ = 0;
    bool gesture_pushed_ = false;
    bool dirty_ = false;
};

State& Global();

}
