#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Editor {

enum class LibrarySource : uint8_t {
    BuiltIn,
    User,
    OtherBuild,
};

struct LibraryEntry {
    std::string id = {};
    std::string name = {};
    std::string build = {};
    std::string path = {};
    std::string summary = {};
    LibrarySource source = LibrarySource::BuiltIn;
    int errors = 0;
    int warnings = 0;
    bool modified = false;
    bool loaded = false;
};

struct LibraryGroup {
    std::string label = {};
    LibrarySource source = LibrarySource::BuiltIn;
    std::vector<LibraryEntry> entries = {};
};

std::vector<LibraryGroup> GroupLibrary(const std::vector<LibraryEntry>& entries,
                                       std::string_view filter);

std::string Slug(std::string_view name);

std::string UniqueId(std::string_view wanted, const std::vector<std::string>& taken);

std::string CopyId(std::string_view id, const std::vector<std::string>& taken);

std::string UserRelativePath(std::string_view build, std::string_view id);

}
