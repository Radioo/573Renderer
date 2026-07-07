#include <catch2/catch_test_macros.hpp>

#include "settings/settings.h"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <string>

namespace {

struct TempIni {
    std::filesystem::path path;
    explicit TempIni(const std::string& name) {
        path = std::filesystem::temp_directory_path() / name;
        std::filesystem::remove(path);
    }
    ~TempIni() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        std::filesystem::remove(path.string() + ".tmp", ec);
    }
    TempIni(const TempIni&) = delete;
    TempIni& operator=(const TempIni&) = delete;
    TempIni(TempIni&&) = delete;
    TempIni& operator=(TempIni&&) = delete;

    void Write(const std::string& content) const {
        std::ofstream f(path);
        f << content;
    }
    [[nodiscard]] std::string Str() const { return path.string(); }
};

}

TEST_CASE("LoadFrom returns defaults when the file is missing") {
    const TempIni ini("r573_settings_missing.ini");
    const Settings::Config c = Settings::LoadFrom(ini.Str());
    CHECK(c.game_dir.empty());
    CHECK(c.render_width == 1080);
    CHECK(c.render_height == 1920);
    CHECK(c.render_fps == 120);
    CHECK(c.master_scale == 1.0F);
    CHECK_FALSE(c.loop_master);
    CHECK_FALSE(c.root_loop_force);
}

TEST_CASE("LoadFrom parses keys, comments, and unknown lines") {
    const TempIni ini("r573_settings_parse.ini");
    ini.Write("# header comment\n"
              "; alt comment\n"
              "\n"
              "game_dir = D:/games/KFC \n"
              "loop_master=1\n"
              "root_loop=force\n"
              "render_width=1920\n"
              "render_height=1080\n"
              "render_fps=60\n"
              "game_profile=sdvx7\n"
              "master_scale=1.5\n"
              "future_key=whatever\n"
              "not a key value line\n");
    const Settings::Config c = Settings::LoadFrom(ini.Str());
    CHECK(c.game_dir == "D:/games/KFC");
    CHECK(c.loop_master);
    CHECK(c.root_loop_force);
    CHECK(c.render_width == 1920);
    CHECK(c.render_height == 1080);
    CHECK(c.render_fps == 60);
    CHECK(c.game_profile == "sdvx7");
    CHECK(c.master_scale == 1.5F);
}

TEST_CASE("LoadFrom accepts the documented boolean spellings") {
    const TempIni ini("r573_settings_bools.ini");
    ini.Write("loop_master=no\nroot_loop=hold\n");
    const Settings::Config off = Settings::LoadFrom(ini.Str());
    CHECK_FALSE(off.loop_master);
    CHECK_FALSE(off.root_loop_force);

    ini.Write("loop_master=\nroot_loop=yes\n");
    const Settings::Config on = Settings::LoadFrom(ini.Str());
    CHECK(on.loop_master);
    CHECK(on.root_loop_force);
}

TEST_CASE("LoadFrom guards malformed numeric values") {
    const TempIni ini("r573_settings_badnum.ini");
    ini.Write("render_width=foo\nrender_height=-5\nrender_fps=0\nmaster_scale=junk\n");
    const Settings::Config c = Settings::LoadFrom(ini.Str());
    CHECK(c.render_width == 1080);
    CHECK(c.render_height == 1920);
    CHECK(c.render_fps == 120);
    CHECK(c.master_scale == 1.0F);
}

TEST_CASE("SaveAtomicTo then LoadFrom round-trips") {
    const TempIni ini("r573_settings_roundtrip.ini");
    Settings::Config c;
    c.game_dir = "E:/KFC";
    c.loop_master = true;
    c.root_loop_force = true;
    c.master_scale = 1.5F;
    c.render_width = 1920;
    c.render_height = 1080;
    c.render_fps = 60;
    c.game_profile = "iidx33";
    REQUIRE(Settings::SaveAtomicTo(c, ini.Str()));

    const Settings::Config back = Settings::LoadFrom(ini.Str());
    CHECK(back.game_dir == c.game_dir);
    CHECK(back.loop_master == c.loop_master);
    CHECK(back.root_loop_force == c.root_loop_force);
    CHECK(back.master_scale == c.master_scale);
    CHECK(back.render_width == c.render_width);
    CHECK(back.render_height == c.render_height);
    CHECK(back.render_fps == c.render_fps);
    CHECK(back.game_profile == c.game_profile);
}

TEST_CASE("SaveAtomicTo replaces an existing file completely") {
    const TempIni ini("r573_settings_replace.ini");
    Settings::Config first;
    first.game_dir = "old";
    REQUIRE(Settings::SaveAtomicTo(first, ini.Str()));

    Settings::Config second;
    second.game_dir = "new";
    second.render_fps = 30;
    REQUIRE(Settings::SaveAtomicTo(second, ini.Str()));

    const Settings::Config back = Settings::LoadFrom(ini.Str());
    CHECK(back.game_dir == "new");
    CHECK(back.render_fps == 30);
    CHECK_FALSE(std::filesystem::exists(ini.Str() + ".tmp"));
}

TEST_CASE("SameGameDir matches paths that differ only in case, separators, trailing slashes") {
    CHECK(Settings::SameGameDir("D:\\Games\\SDVX 7 - NABLA", "d:/games/SDVX 7 - nabla/"));
    CHECK(Settings::SameGameDir("C:\\Games\\IIDX", "C:\\Games\\IIDX\\"));
    CHECK(Settings::SameGameDir("relative/dir", "relative\\dir"));
}

TEST_CASE("SameGameDir rejects different dirs and empty operands") {
    CHECK_FALSE(Settings::SameGameDir("D:\\Games\\IIDX 33", "D:\\Games\\SDVX 7 - NABLA"));
    CHECK_FALSE(Settings::SameGameDir("C:\\Games\\IIDX", "C:\\Games\\IIDX 2"));
    CHECK_FALSE(Settings::SameGameDir("", "D:\\Games"));
    CHECK_FALSE(Settings::SameGameDir("D:\\Games", ""));
    CHECK_FALSE(Settings::SameGameDir("", ""));
}
