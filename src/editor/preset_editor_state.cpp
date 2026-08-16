#include "editor/preset_editor_state.h"

#include "editor/timeline_edits.h"
#include "preset/doc/preset_document.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Editor {

namespace Doc = Preset::Doc;

void State::LoadDocument(Doc::Document document) {
    document_ = std::make_shared<const Doc::Document>(std::move(document));
    saved_ = document_;
    past_.clear();
    future_.clear();
    selection_.clear();
    clipboard_.clear();
    request_ = Request{};
    gesture_ = 0;
    gesture_pushed_ = false;
    dirty_ = false;
    revision_++;
    load_id_++;
    view_ = View{};
}

void State::Close() {
    document_.reset();
    saved_.reset();
    past_.clear();
    future_.clear();
    selection_.clear();
    clipboard_.clear();
    request_ = Request{};
    gesture_ = 0;
    gesture_pushed_ = false;
    dirty_ = false;
    revision_++;
    load_id_++;
}

void State::Push() {
    past_.push_back(Entry{.document = document_, .selection = selection_});
    if ((int)past_.size() > kUndoDepth) past_.erase(past_.begin());
    future_.clear();
}

bool State::Apply(const Edit& edit) {
    if (document_ == nullptr || !edit) return false;
    Doc::Document working = *document_;
    if (!edit(working)) return false;

    const bool record = gesture_ == 0 || !gesture_pushed_;
    if (record) Push();
    if (gesture_ > 0) gesture_pushed_ = true;

    document_ = std::make_shared<const Doc::Document>(std::move(working));
    dirty_ = saved_ == nullptr || !(*document_ == *saved_);
    revision_++;
    DropMissingSelection();
    return true;
}

void State::BeginGesture() {
    gesture_++;
}

void State::EndGesture() {
    if (gesture_ == 0) return;
    gesture_--;
    if (gesture_ == 0) gesture_pushed_ = false;
}

bool State::Undo() {
    if (past_.empty() || document_ == nullptr) return false;
    future_.push_back(Entry{.document = document_, .selection = selection_});
    const Entry entry = past_.back();
    past_.pop_back();
    document_ = entry.document;
    selection_ = entry.selection;
    dirty_ = saved_ == nullptr || !(*document_ == *saved_);
    revision_++;
    DropMissingSelection();
    return true;
}

bool State::Redo() {
    if (future_.empty() || document_ == nullptr) return false;
    past_.push_back(Entry{.document = document_, .selection = selection_});
    const Entry entry = future_.back();
    future_.pop_back();
    document_ = entry.document;
    selection_ = entry.selection;
    dirty_ = saved_ == nullptr || !(*document_ == *saved_);
    revision_++;
    DropMissingSelection();
    return true;
}

void State::CollapseUndo(int depth) {
    if (depth < 0 || (int)past_.size() <= depth + 1) return;
    past_.resize((std::size_t)depth + 1);
}

void State::ClearRedo() {
    future_.clear();
}

bool State::IsSelected(const std::string& clip_id) const {
    return std::ranges::find(selection_, clip_id) != selection_.end();
}

void State::Select(std::string clip_id) {
    selection_.clear();
    selection_.push_back(std::move(clip_id));
}

void State::ExtendSelection(std::string clip_id) {
    if (IsSelected(clip_id)) return;
    selection_.push_back(std::move(clip_id));
}

void State::ToggleSelection(const std::string& clip_id) {
    const auto it = std::ranges::find(selection_, clip_id);
    if (it == selection_.end()) {
        selection_.push_back(clip_id);
        return;
    }
    selection_.erase(it);
}

void State::SetSelection(std::vector<std::string> clip_ids) {
    selection_ = std::move(clip_ids);
}

void State::ClearSelection() {
    selection_.clear();
}

void State::DropMissingSelection() {
    if (document_ == nullptr) return;
    std::erase_if(selection_, [this](const std::string& clip_id) {
        return !FindClip(*document_, clip_id).Valid();
    });
}

void State::PostRequest(Request request) {
    request_ = std::move(request);
}

Request State::TakeRequest() {
    return std::exchange(request_, Request{});
}

void State::SetClipboard(std::vector<ClipboardClip> clips) {
    clipboard_ = std::move(clips);
}

State& Global() {
    static State state;
    return state;
}

}
