#include "document/history.h"

#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

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

std::vector<std::string> History::Names() const {
    std::vector<std::string> names;
    names.reserve(undo_.size() + redo_.size());
    for (const Step& step : undo_)
        names.push_back(step.name);
    for (const Step& step : std::views::reverse(redo_))
        names.push_back(step.name);
    return names;
}

std::optional<Snapshot> History::Jump(std::size_t position, Snapshot current) {
    if (position > undo_.size() + redo_.size()) return std::nullopt;
    while (undo_.size() > position) {
        std::optional<Snapshot> undone = Undo(std::move(current));
        if (!undone) return std::nullopt;
        current = std::move(*undone);
    }
    while (undo_.size() < position) {
        std::optional<Snapshot> redone = Redo(std::move(current));
        if (!redone) return std::nullopt;
        current = std::move(*redone);
    }
    return current;
}

bool History::Saved() const {
    return clean_depth_.has_value() && *clean_depth_ == undo_.size();
}

std::optional<std::size_t> History::StepsFromSaved() const {
    if (!clean_depth_) return std::nullopt;
    return *clean_depth_ > undo_.size() ? *clean_depth_ - undo_.size()
                                        : undo_.size() - *clean_depth_;
}

}
