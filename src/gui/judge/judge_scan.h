#pragma once

#include "gui/judge/judge_view.h"
#include "judge/clip_catalog.h"

#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace JudgeScan {

class Scanner {
public:
    explicit Scanner(std::string graphics_dir);
    ~Scanner();
    Scanner(const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;
    Scanner(Scanner&&) = delete;
    Scanner& operator=(Scanner&&) = delete;

    [[nodiscard]] JudgeView::ScanState Status() const;
    [[nodiscard]] std::optional<Judge::Catalog> TakeCatalog();

private:
    void Run() noexcept;
    void Body();

    std::string graphics_dir_;
    mutable std::mutex mutex_;
    JudgeView::ScanState status_;
    std::vector<Judge::PackageClips> packages_;
    bool taken_ = false;
    std::thread worker_;
};

}
