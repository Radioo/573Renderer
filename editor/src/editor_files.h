#pragma once

#include <cstdint>
#include <vector>

class QString;

namespace Editor {

inline constexpr const char* kGameDirKey = "game/directory";
inline constexpr const char* kDocumentDirKey = "document/directory";
inline constexpr const char* kProjectDirKey = "project/directory";
inline constexpr const char* kLoopKey = "playback/loop";
inline constexpr const char* kBackgroundKey = "preview/background";
inline constexpr const char* kSnapKey = "stage/snap";
inline constexpr const char* kBuildSlug = "iidx33";

[[nodiscard]] std::vector<uint8_t> ReadFileBytes(const QString& path);

bool WriteFileBytes(const QString& path, const std::vector<uint8_t>& bytes);

}
