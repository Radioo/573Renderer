#pragma once

#include "document/document.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace Document {

constexpr std::size_t kDefaultHistorySteps = 32;

class History {
public:
    explicit History(std::size_t limit = kDefaultHistorySteps) : limit_(limit) {}

    void Clear();

    void Record(std::string name, File before);

    [[nodiscard]] bool CanUndo() const { return !undo_.empty(); }

    [[nodiscard]] bool CanRedo() const { return !redo_.empty(); }

    [[nodiscard]] const std::string& UndoName() const;

    [[nodiscard]] const std::string& RedoName() const;

    [[nodiscard]] std::optional<File> Undo(File current);

    [[nodiscard]] std::optional<File> Redo(File current);

    void MarkSaved() { clean_depth_ = undo_.size(); }

    [[nodiscard]] bool Saved() const;

private:
    struct Step {
        std::string name;
        File file;
    };

    std::size_t limit_;
    std::vector<Step> undo_;
    std::vector<Step> redo_;
    std::optional<std::size_t> clean_depth_ = 0;
};

}
