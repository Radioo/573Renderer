#pragma once

#include "judge/afp_pose.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace Judge {

enum class Verdict : std::uint8_t { Smooth, Wrong, Unsure };

struct ClipKey {
    std::string package;
    std::string clip;

    [[nodiscard]] std::string Text() const;
    auto operator<=>(const ClipKey&) const = default;
};

struct Entry {
    std::optional<Verdict> verdict;
    std::string note;
    AfpPose::MatrixLerp matrix_lerp = AfpPose::MatrixLerp::Decompose;

    bool operator==(const Entry&) const = default;
};

[[nodiscard]] const char* VerdictName(Verdict verdict);
[[nodiscard]] std::optional<Verdict> VerdictFromName(const std::string& name);

class VerdictStore {
public:
    [[nodiscard]] static std::optional<VerdictStore> Open(const std::string& path,
                                                          std::string& err);

    [[nodiscard]] const Entry* Find(const ClipKey& key) const;
    [[nodiscard]] bool Judged(const ClipKey& key) const;
    [[nodiscard]] const std::string& LastError() const { return last_error_; }

    bool Set(const ClipKey& key, const Entry& entry);

private:
    bool Save();

    std::string path_;
    std::map<std::string, Entry> entries_;
    std::string last_error_;
};

}
