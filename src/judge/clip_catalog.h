#pragma once

#include "judge/verdict_store.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace Judge {

struct PackageClips {
    std::string package;
    std::string path;
    std::vector<std::string> clips;
};

[[nodiscard]] std::optional<std::vector<std::string>> ReadClipNames(const std::string& path,
                                                                    std::string& err);

[[nodiscard]] std::vector<std::string> ListPackageFiles(const std::string& graphics_dir);

[[nodiscard]] std::string PackageName(const std::string& graphics_dir, const std::string& path);

class Catalog {
public:
    Catalog() = default;
    explicit Catalog(std::vector<PackageClips> packages);

    [[nodiscard]] std::size_t Size() const { return keys_.size(); }
    [[nodiscard]] const ClipKey& Key(std::size_t index) const { return keys_.at(index); }
    [[nodiscard]] const std::string& PathOf(std::size_t index) const;
    [[nodiscard]] std::optional<std::size_t> IndexOf(const ClipKey& key) const;
    [[nodiscard]] std::size_t JudgedCount(const VerdictStore& store) const;
    [[nodiscard]] std::optional<std::size_t> NextUnjudged(std::size_t from,
                                                          const VerdictStore& store) const;

private:
    std::vector<PackageClips> packages_;
    std::vector<ClipKey> keys_;
    std::vector<std::size_t> package_of_;
};

}
