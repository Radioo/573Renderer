#include "document/history.h"

#include <optional>
#include <string>
#include <utility>

namespace Document {

namespace {

const std::string kNothing;

}

void History::Clear() {
    undo_.clear();
    redo_.clear();
    clean_depth_ = 0;
}

void History::Record(std::string name, Snapshot before) {
    redo_.clear();
    undo_.push_back(Step{.name = std::move(name), .state = std::move(before)});
    if (undo_.size() > limit_) {
        undo_.erase(undo_.begin());
        if (clean_depth_) {
            if (*clean_depth_ == 0) {
                clean_depth_.reset();
            } else {
                --*clean_depth_;
            }
        }
    }
    if (clean_depth_ && *clean_depth_ > undo_.size()) clean_depth_.reset();
}

const std::string& History::UndoName() const {
    return undo_.empty() ? kNothing : undo_.back().name;
}

const std::string& History::RedoName() const {
    return redo_.empty() ? kNothing : redo_.back().name;
}

std::optional<Snapshot> History::Undo(Snapshot current) {
    if (undo_.empty()) return std::nullopt;
    Step step = std::move(undo_.back());
    undo_.pop_back();
    redo_.push_back(Step{.name = step.name, .state = std::move(current)});
    return std::move(step.state);
}

std::optional<Snapshot> History::Redo(Snapshot current) {
    if (redo_.empty()) return std::nullopt;
    Step step = std::move(redo_.back());
    redo_.pop_back();
    undo_.push_back(Step{.name = step.name, .state = std::move(current)});
    return std::move(step.state);
}

bool History::Saved() const {
    return clean_depth_.has_value() && *clean_depth_ == undo_.size();
}

}
