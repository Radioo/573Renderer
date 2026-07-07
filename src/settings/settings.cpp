#include "settings/settings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <cstddef>
#include <charconv>
#include <filesystem>
#include <memory>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>

namespace Settings {

namespace {

std::string Trim(const std::string& s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t'))
        b++;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) {
        e--;
    }
    return s.substr(b, e - b);
}

std::string NormalizeGameDir(const std::string& p) {
    std::string out;
    out.reserve(p.size());
    for (const char c : p) {
        char n = c;
        if (n == '\\') n = '/';
        if (n >= 'A' && n <= 'Z') n = static_cast<char>(n - 'A' + 'a');
        out.push_back(n);
    }
    while (!out.empty() && out.back() == '/')
        out.pop_back();
    return out;
}

int ParseIntOrZero(std::string_view v) {
    int out = 0;
    std::from_chars(v.data(), std::to_address(v.end()), out);
    return out;
}

float ParseFloatOrZero(std::string_view v) {
    float out = 0.0F;
    std::from_chars(v.data(), std::to_address(v.end()), out);
    return out;
}

bool IsFalseToken(const std::string& v) {
    return v == "0" || v == "false" || v == "False" || v == "no" || v == "No";
}

bool IsTrueToken(const std::string& v) {
    return v == "force" || v == "Force" || v == "1" || v == "true" || v == "True" || v == "yes" ||
           v == "Yes";
}

void ApplyPositiveInt(int& into, const std::string& v) {
    const int n = ParseIntOrZero(v);
    if (n > 0) into = n;
}

void ApplyKey(Config& c, const std::string& key, const std::string& val) {
    if (key == "game_dir") {
        c.game_dir = val;
    } else if (key == "loop_master") {
        c.loop_master = !IsFalseToken(val);
    } else if (key == "root_loop") {
        c.root_loop_force = IsTrueToken(val);
    } else if (key == "render_width") {
        ApplyPositiveInt(c.render_width, val);
    } else if (key == "render_height") {
        ApplyPositiveInt(c.render_height, val);
    } else if (key == "render_fps") {
        ApplyPositiveInt(c.render_fps, val);
    } else if (key == "game_profile") {
        c.game_profile = val;
    } else if (key == "master_scale") {
        const float v = ParseFloatOrZero(val);
        if (v > 0.0F) c.master_scale = v;
    }
}

}

bool SameGameDir(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return false;
    return NormalizeGameDir(a) == NormalizeGameDir(b);
}

std::string DefaultIniPath() {
    std::array<char, MAX_PATH> exe{};
    GetModuleFileNameA(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
    std::string p(exe.data());
    const std::size_t slash = p.find_last_of("\\/");
    if (slash != std::string::npos) p.resize(slash + 1);
    return p + "settings.ini";
}

Config LoadFrom(const std::string& ini_path) {
    Config c;
    std::ifstream f(ini_path);
    if (!f.is_open()) return c;

    std::string line;
    while (std::getline(f, line)) {
        const std::string t = Trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') continue;

        const std::size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        ApplyKey(c, Trim(t.substr(0, eq)), Trim(t.substr(eq + 1)));
    }
    return c;
}

bool SaveAtomicTo(const Config& c, const std::string& ini_path) {
    const std::string tmp_path = ini_path + ".tmp";

    {
        std::ofstream f(tmp_path, std::ios::trunc);
        if (!f.is_open()) return false;
        f << "# 573Renderer settings (from 573_vibe_reversing)\n";
        f << "# Auto-generated; hand-edit is fine.\n";
        f << "game_dir=" << c.game_dir << "\n";
        f << "loop_master=" << (c.loop_master ? "1" : "0") << "\n";
        f << "root_loop=" << (c.root_loop_force ? "force" : "hold") << "\n";
        f << "render_width=" << c.render_width << "\n";
        f << "render_height=" << c.render_height << "\n";
        f << "render_fps=" << c.render_fps << "\n";
        f << "game_profile=" << c.game_profile << "\n";
        f << "master_scale=" << c.master_scale << "\n";
        f.flush();
        if (!f.good()) return false;
    }

    std::error_code ec;
    std::filesystem::rename(tmp_path, ini_path, ec);
    if (ec) {
        std::filesystem::remove(tmp_path, ec);
        return false;
    }
    return true;
}

Config Load() {
    return LoadFrom(DefaultIniPath());
}

bool SaveAtomic(const Config& c) {
    return SaveAtomicTo(c, DefaultIniPath());
}

}
