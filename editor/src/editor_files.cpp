#include "editor_files.h"

#include "game_profile.h"

#include <QString>

#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

namespace Editor {

std::vector<uint8_t> ReadFileBytes(const QString& path) {
    std::ifstream in(path.toStdWString(), std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

bool WriteFileBytes(const QString& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path.toStdWString(), std::ios::binary);
    for (const uint8_t byte : bytes)
        out.put(static_cast<char>(byte));
    return out.good();
}

std::string InstallBuild(const QString& game_dir) {
    if (game_dir.isEmpty()) return {};
    const GameProfile::Profile* profile = GameProfile::AutoDetect(game_dir.toStdString());
    return profile != nullptr ? std::string(profile->slug) : std::string();
}

}
