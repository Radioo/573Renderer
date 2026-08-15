#pragma once

#include "preset/doc/preset_document.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_validate.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Preset::Doc {

struct Entry {
    Document document = {};
    std::string path = {};
    bool builtin = false;
    std::vector<Problem> problems = {};
};

struct ScanStatus {
    int done = 0;
    int total = 0;
    std::string current = {};
};

using ScanProgressFn = std::function<void(const ScanStatus&)>;

std::filesystem::path UserRoot();

Loaded LoadFile(const std::filesystem::path& path);

class Registry {
public:
    void Load(const std::filesystem::path& user_root, const ScanProgressFn& progress = {});

    std::vector<const Entry*> ForBuild(std::string_view build) const;

    const Entry* Find(std::string_view build, std::string_view id) const;

    std::vector<const Entry*> Problems() const;

private:
    std::vector<Entry> entries_;
    std::vector<Entry> rejected_;
};

}
