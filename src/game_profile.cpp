#include "game_profile.h"

#include <filesystem>
#include <system_error>

#include <cctype>
#include <string>
#include <vector>

namespace GameProfile {

namespace {

const std::vector<Profile> kProfiles = {
    Profile{
        .name = "IIDX 26 (Rootage)",
        .slug = "iidx26",
        .dir_substring = "rootage",
        .backend_id = "afp_modern",
        .game_dll = "bm2dx.dll",
        .default_render_w = 1280,
        .default_render_h = 720,
    },
    Profile{
        .name = "IIDX 33 (Sparkle Shower)",
        .slug = "iidx33",
        .dir_substring = "iidx",
        .backend_id = "afp_modern",
        .game_dll = "bm2dx.dll",
        .default_render_w = 1920,
        .default_render_h = 1080,
    },
    Profile{
        .name = "SDVX 7 (NABLA)",
        .slug = "sdvx7",
        .dir_substring = "sdvx",
        .backend_id = "afp_modern",
        .game_dll = "soundvoltex.dll",
        .default_render_w = 1080,
        .default_render_h = 1920,
    },
    Profile{
        .name = "DDR World (MDX)",
        .slug = "ddrworld",
        .dir_substring = "mdx",
        .backend_id = "afp_ddr",
        .game_dll = "gamemdx.dll",
        .default_render_w = 1280,
        .default_render_h = 720,
    },
    Profile{
        .name = "GITADORA DELTA",
        .slug = "gitadora",
        .dir_substring = "delta",
        .backend_id = "afp_modern",
        .game_dll = "gdxg.dll",
        .default_render_w = 3840,
        .default_render_h = 2160,
    },
    Profile{
        .name = "jubeat (T44)",
        .slug = "t44",
        .dir_substring = "t44",
        .backend_id = "afp_modern",
        .game_dll = "jubeat2019.dll",
        .default_render_w = 1080,
        .default_render_h = 1920,
    },
};

std::string ToLower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char const c : s)
        out.push_back((char)std::tolower((unsigned char)c));
    return out;
}

bool GameDllPresent(const std::string& dir, const char* game_dll) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root(dir);
    const fs::path candidates[] = {
        root / "modules",
        root / "contents" / "modules",
        root,
    };
    for (const auto& c : candidates) {
        if (fs::exists(c / game_dll, ec)) return true;
    }
    return false;
}

}

const std::vector<Profile>& All() {
    return kProfiles;
}

const Profile* AutoDetect(const std::string& dir) {
    if (dir.empty()) return nullptr;
    const std::string dir_lc = ToLower(dir);
    for (const auto& p : kProfiles) {
        if ((p.dir_substring == nullptr) || (*p.dir_substring == 0)) continue;
        std::string const needle = ToLower(p.dir_substring);
        if (dir_lc.find(needle) != std::string::npos) return &p;
    }
    for (const auto& p : kProfiles) {
        if (p.game_dll == nullptr) continue;
        if (GameDllPresent(dir, p.game_dll)) return &p;
    }
    return nullptr;
}

const Profile* BySlug(const std::string& slug) {
    if (slug.empty()) return nullptr;
    for (const auto& p : kProfiles) {
        if (slug == p.slug) return &p;
    }
    return nullptr;
}

}
