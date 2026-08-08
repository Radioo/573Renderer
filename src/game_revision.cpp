#include "game_revision.h"

#include "support/log.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>

namespace GameRevision {

namespace {

constexpr size_t kDatecodeDigits = 10;

bool IsDatecode(const std::string& name) {
    if (name.size() != kDatecodeDigits) return false;
    return std::ranges::all_of(name, [](const char c) { return c >= '0' && c <= '9'; });
}

}

std::string LatestRevisionDir(const std::string& game_dir) {
    std::error_code ec;
    const std::filesystem::path root(game_dir);
    if (!std::filesystem::is_directory(root, ec)) return {};

    std::string best;
    for (const auto& e : std::filesystem::directory_iterator(root, ec)) {
        if (ec) break;
        if (!e.is_directory(ec)) continue;
        const std::string name = e.path().filename().string();
        if (!IsDatecode(name)) continue;
        if (name > best) best = name;
    }
    if (best.empty()) return {};

    const std::string full = (root / best).string();
    LOG("Boot", "revision folders present; using the newest non-.orig one: %s", best.c_str());
    return full;
}

}
