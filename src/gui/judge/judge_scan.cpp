#include "gui/judge/judge_scan.h"

#include "support/log.h"

#include <utility>

namespace JudgeScan {

Scanner::Scanner(std::string graphics_dir)
    : graphics_dir_(std::move(graphics_dir)), worker_([this] { Body(); }) {}

Scanner::~Scanner() {
    if (worker_.joinable()) worker_.join();
}

void Scanner::Body() {
    const std::vector<std::string> files = Judge::ListPackageFiles(graphics_dir_);
    {
        const std::lock_guard lock(mutex_);
        status_.total = files.size();
    }
    std::vector<Judge::PackageClips> found;
    for (const std::string& file : files) {
        const std::string package = Judge::PackageName(graphics_dir_, file);
        {
            const std::lock_guard lock(mutex_);
            status_.current = package;
        }
        std::string err;
        std::optional<std::vector<std::string>> clips = Judge::ReadClipNames(file, err);
        if (!clips) {
            LOG("Judge", "skipping %s: %s", package.c_str(), err.c_str());
        } else if (!clips->empty()) {
            found.push_back({.package = package, .path = file, .clips = std::move(*clips)});
        }
        const std::lock_guard lock(mutex_);
        status_.read++;
    }
    const std::lock_guard lock(mutex_);
    packages_ = std::move(found);
    status_.done = true;
    status_.current.clear();
}

JudgeView::ScanState Scanner::Status() const {
    const std::lock_guard lock(mutex_);
    return status_;
}

std::optional<Judge::Catalog> Scanner::TakeCatalog() {
    const std::lock_guard lock(mutex_);
    if (!status_.done || taken_) return std::nullopt;
    taken_ = true;
    return Judge::Catalog(std::move(packages_));
}

}
