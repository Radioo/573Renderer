#pragma once

#include "document/authored.h"
#include "document/document.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace Document {

constexpr std::size_t kDefaultHistorySteps = 32;

struct Snapshot {
    File file;
    std::vector<AuthoredDepth> authored;
};

class History {
public:
    explicit History(std::size_t limit = kDefaultHistorySteps) : limit_(limit) {}

    void Clear();

    void Record(std::string name, Snapshot before);

    [[nodiscard]] bool CanUndo() const { return !undo_.empty(); }

    [[nodiscard]] bool CanRedo() const { return !redo_.empty(); }

    [[nodiscard]] const std::string& UndoName() const;

    [[nodiscard]] const std::string& RedoName() const;

    [[nodiscard]] std::optional<Snapshot> Undo(Snapshot current);

    [[nodiscard]] std::optional<Snapshot> Redo(Snapshot current);

    [[nodiscard]] std::vector<std::string> Names() const;

    [[nodiscard]] std::size_t Position() const { return undo_.size(); }

    [[nodiscard]] std::optional<Snapshot> Jump(std::size_t position, Snapshot current);

    void MarkSaved() { clean_depth_ = undo_.size(); }

    [[nodiscard]] bool Saved() const;

private:
    struct Step {
        std::string name;
        Snapshot state;
    };

    std::size_t limit_;
    std::vector<Step> undo_;
    std::vector<Step> redo_;
    std::optional<std::size_t> clean_depth_ = 0;
};

}
