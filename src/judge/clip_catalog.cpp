#include "judge/clip_catalog.h"

#include "formats/txp2.h"
#include "judge/verdict_store.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace Judge {

namespace {

namespace fs = std::filesystem;

constexpr std::size_t kHeaderProbeBytes = 4096;

bool ReadPrefix(const std::string& path, std::size_t limit, std::vector<std::uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(limit, 0);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(limit));
    out.resize(static_cast<std::size_t>(in.gcount()));
    return true;
}

bool LooksLikeTxp2(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < 4) return false;
    const std::string head(bytes.begin(), bytes.begin() + 4);
    return head == "TXP2" || head == "2PXT";
}

std::string Lower(std::string text) {
    std::ranges::transform(text, text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

}

std::optional<std::vector<std::string>> ReadClipNames(const std::string& path, std::string& err) {
    std::vector<std::uint8_t> bytes;
    if (!ReadPrefix(path, kHeaderProbeBytes, bytes)) {
        err = "cannot read " + path;
        return std::nullopt;
    }
    if (!LooksLikeTxp2(bytes)) {
        err = "not a TXP2 package";
        return std::nullopt;
    }
    Txp2::Package probe;
    std::string ignored;
    (void)Txp2::Parse(bytes, probe, ignored);
    std::size_t want = probe.core_size;
    if (want == 0 && bytes.size() == kHeaderProbeBytes) {
        std::error_code ec;
        want = static_cast<std::size_t>(fs::file_size(path, ec));
    }
    if (want > bytes.size() && !ReadPrefix(path, want, bytes)) {
        err = "cannot read " + path;
        return std::nullopt;
    }
    Txp2::Package package;
    if (!Txp2::Parse(bytes, package, err)) return std::nullopt;
    std::vector<std::string> names;
    names.reserve(package.afp_entries.size());
    for (const Txp2::AfpEntry& entry : package.afp_entries)
        names.push_back(entry.name);
    return names;
}

std::vector<std::string> ListPackageFiles(const std::string& graphics_dir) {
    std::vector<std::string> out;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(graphics_dir, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        if (Lower(it->path().extension().string()) != ".bin") continue;
        out.push_back(it->path().string());
    }
    std::ranges::sort(out, [&](const std::string& a, const std::string& b) {
        return Lower(PackageName(graphics_dir, a)) < Lower(PackageName(graphics_dir, b));
    });
    return out;
}

std::string PackageName(const std::string& graphics_dir, const std::string& path) {
    std::error_code ec;
    fs::path rel = fs::relative(fs::path(path), fs::path(graphics_dir), ec);
    if (ec || rel.empty()) rel = fs::path(path).filename();
    return rel.generic_string();
}

Catalog::Catalog(std::vector<PackageClips> packages) : packages_(std::move(packages)) {
    for (std::size_t p = 0; p < packages_.size(); p++) {
        for (const std::string& clip : packages_[p].clips) {
            keys_.push_back(ClipKey{.package = packages_[p].package, .clip = clip});
            package_of_.push_back(p);
        }
    }
}

const std::string& Catalog::PathOf(std::size_t index) const {
    return packages_.at(package_of_.at(index)).path;
}

std::optional<std::size_t> Catalog::IndexOf(const ClipKey& key) const {
    const auto found = std::ranges::find(keys_, key);
    if (found == keys_.end()) return std::nullopt;
    return static_cast<std::size_t>(std::distance(keys_.begin(), found));
}

std::size_t Catalog::JudgedCount(const VerdictStore& store) const {
    return static_cast<std::size_t>(
        std::ranges::count_if(keys_, [&](const ClipKey& key) { return store.Judged(key); }));
}

std::optional<std::size_t> Catalog::NextUnjudged(std::size_t from,
                                                 const VerdictStore& store) const {
    for (std::size_t step = 1; step <= keys_.size(); step++) {
        const std::size_t at = (from + step) % keys_.size();
        if (!store.Judged(keys_[at])) return at;
    }
    return std::nullopt;
}

}
