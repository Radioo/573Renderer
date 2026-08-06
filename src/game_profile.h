#pragma once

#include <string>
#include <vector>

namespace GameProfile {

struct Profile {
    const char* name;
    const char* slug;
    const char* dir_substring;
    const char* backend_id;
    const char* game_dll = nullptr;

    int default_render_w;
    int default_render_h;
};

const std::vector<Profile>& All();

const Profile* AutoDetect(const std::string& dir);

const Profile* BySlug(const std::string& slug);

}
