#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "judge/afp_pose.h"
#include "judge/clip_catalog.h"
#include "judge/judge_clock.h"
#include "judge/verdict_store.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct TempDir {
    fs::path root;
    explicit TempDir(const char* name)
        : root(fs::temp_directory_path() / (std::string("r573_judge_") + name)) {
        std::error_code ec;
        fs::remove_all(root, ec);
        fs::create_directories(root, ec);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;
};

std::string ReadText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    const std::vector<char> chars(bytes.begin(), bytes.end());
    out.write(chars.data(), static_cast<std::streamsize>(chars.size()));
}

void PushU32Be(std::vector<std::uint8_t>& v, std::uint32_t x) {
    for (const int shift : {24, 16, 8, 0})
        v.push_back(static_cast<std::uint8_t>((x >> shift) & 0xFFU));
}

std::vector<std::uint8_t> PackageWithClips(const std::vector<std::string>& names,
                                           std::size_t name_gap) {
    constexpr std::uint32_t kFlagCoreSize = 0x0400U;
    constexpr std::uint32_t kFlagAfpStreams = 0x0800U;
    constexpr std::uint32_t kHeader = 24 + (3 * 4);
    const auto table = kHeader;
    const auto names_at = static_cast<std::uint32_t>(table + (names.size() * 12) + name_gap);
    std::uint32_t core = names_at;
    for (const std::string& name : names)
        core += static_cast<std::uint32_t>(name.size() + 1);

    std::vector<std::uint8_t> v{'T', 'X', 'P', '2'};
    PushU32Be(v, 0);
    PushU32Be(v, 0);
    PushU32Be(v, core);
    PushU32Be(v, kHeader);
    PushU32Be(v, kFlagCoreSize | kFlagAfpStreams);
    PushU32Be(v, core);
    PushU32Be(v, static_cast<std::uint32_t>(names.size()));
    PushU32Be(v, table);
    std::uint32_t name_at = names_at;
    for (const std::string& name : names) {
        PushU32Be(v, name_at);
        PushU32Be(v, 0x10);
        PushU32Be(v, 0x20);
        name_at += static_cast<std::uint32_t>(name.size() + 1);
    }
    v.resize(names_at, 0);
    for (const std::string& name : names) {
        v.insert(v.end(), name.begin(), name.end());
        v.push_back(0);
    }
    return v;
}

Judge::Catalog TwoPackages() {
    return Judge::Catalog({{.package = "a.bin", .path = "x/a.bin", .clips = {"one", "two"}},
                           {.package = "menu/b.bin", .path = "x/menu/b.bin", .clips = {"three"}}});
}

}

TEST_CASE("A missing verdict file opens empty and the first verdict creates it") {
    const TempDir dir("missing");
    const fs::path path = dir.root / "sub" / "verdicts.json";
    std::string err;
    auto store = Judge::VerdictStore::Open(path.string(), err);
    REQUIRE(store);
    if (!store) return;
    const Judge::ClipKey key{.package = "bg_space.bin", .clip = "bg_space"};
    CHECK_FALSE(store->Judged(key));
    REQUIRE(store->Set(key, {.verdict = Judge::Verdict::Wrong,
                             .note = "jitter on the ring",
                             .matrix_lerp = AfpPose::MatrixLerp::Linear}));
    const std::string text = ReadText(path);
    CHECK(text.find("\"bg_space.bin/bg_space\"") != std::string::npos);
    CHECK(text.find("\"wrong\"") != std::string::npos);
    CHECK(text.find("\"linear\"") != std::string::npos);
    CHECK_FALSE(fs::exists(path.string() + ".tmp"));
}

TEST_CASE("Verdicts read back by package and clip name") {
    const TempDir dir("roundtrip");
    const fs::path path = dir.root / "verdicts.json";
    std::string err;
    {
        auto store = Judge::VerdictStore::Open(path.string(), err);
        REQUIRE(store);
        if (!store) return;
        REQUIRE(store->Set({.package = "a.bin", .clip = "two"},
                           {.verdict = Judge::Verdict::Smooth, .note = "fine"}));
        REQUIRE(store->Set({.package = "a.bin", .clip = "one"},
                           {.verdict = std::nullopt, .note = "look again"}));
    }
    auto reopened = Judge::VerdictStore::Open(path.string(), err);
    REQUIRE(reopened);
    if (!reopened) return;
    const Judge::Entry* two = reopened->Find({.package = "a.bin", .clip = "two"});
    REQUIRE(two != nullptr);
    CHECK(two->verdict == Judge::Verdict::Smooth);
    CHECK(two->note == "fine");
    CHECK(two->matrix_lerp == AfpPose::MatrixLerp::Decompose);
    CHECK_FALSE(reopened->Judged({.package = "a.bin", .clip = "one"}));
    const Judge::Entry* one = reopened->Find({.package = "a.bin", .clip = "one"});
    REQUIRE(one != nullptr);
    CHECK(one->note == "look again");
}

TEST_CASE("A verdict file that does not parse is refused and left untouched") {
    const TempDir dir("corrupt");
    const fs::path path = dir.root / "verdicts.json";
    {
        std::ofstream out(path, std::ios::binary);
        out << R"({"verdicts": {"a.bin/one": {"verdict": "great"}}})";
    }
    const std::string before = ReadText(path);
    std::string err;
    CHECK_FALSE(Judge::VerdictStore::Open(path.string(), err));
    CHECK(err.find("unknown verdict") != std::string::npos);
    CHECK(ReadText(path) == before);
}

TEST_CASE("The catalog lists every clip of every package and counts the judged ones") {
    const TempDir dir("catalog");
    std::string err;
    auto store = Judge::VerdictStore::Open((dir.root / "v.json").string(), err);
    REQUIRE(store);
    if (!store) return;
    const Judge::Catalog catalog = TwoPackages();
    REQUIRE(catalog.Size() == 3);
    CHECK(catalog.Key(2).package == "menu/b.bin");
    CHECK(catalog.PathOf(2) == "x/menu/b.bin");
    CHECK(catalog.IndexOf({.package = "a.bin", .clip = "two"}) == 1U);
    REQUIRE(store->Set(catalog.Key(0), {.verdict = Judge::Verdict::Unsure, .note = {}}));
    REQUIRE(store->Set(catalog.Key(1), {.verdict = Judge::Verdict::Smooth, .note = {}}));
    CHECK(catalog.JudgedCount(*store) == 2);
    CHECK(catalog.NextUnjudged(0, *store) == 2U);
    REQUIRE(store->Set(catalog.Key(2), {.verdict = Judge::Verdict::Wrong, .note = {}}));
    CHECK_FALSE(catalog.NextUnjudged(0, *store));
}

TEST_CASE("Clip names are read from the package even when they sit past the header probe") {
    const TempDir dir("txp2");
    const std::vector<std::string> names{"bg_ginga", "bg_space"};
    WriteBytes(dir.root / "near.bin", PackageWithClips(names, 0));
    WriteBytes(dir.root / "far.bin", PackageWithClips(names, 9000));
    WriteBytes(dir.root / "junk.bin", {'n', 'o', 'p', 'e', 0, 0, 0, 0});
    std::string err;
    CHECK(Judge::ReadClipNames((dir.root / "near.bin").string(), err) == names);
    CHECK(Judge::ReadClipNames((dir.root / "far.bin").string(), err) == names);
    CHECK_FALSE(Judge::ReadClipNames((dir.root / "junk.bin").string(), err));
    fs::create_directories(dir.root / "menu");
    WriteBytes(dir.root / "menu" / "c.bin", PackageWithClips({"x"}, 0));
    const auto files = Judge::ListPackageFiles(dir.root.string());
    REQUIRE(files.size() == 4);
    CHECK(Judge::PackageName(dir.root.string(), files[3]) == "near.bin");
    CHECK(Judge::PackageName(dir.root.string(), files[2]) == "menu/c.bin");
}

TEST_CASE("The clock advances in fixed logic steps while the display runs faster") {
    Judge::Clock clock(1.0F / 60.0F);
    CHECK(clock.Tick(1.0F / 120.0F) == 0);
    CHECK(clock.SinceAdvance() == Catch::Approx(1.0F / 120.0F));
    CHECK(clock.Tick(1.0F / 120.0F) == 1);
    CHECK(clock.SinceAdvance() == Catch::Approx(0.0F).margin(1e-6));
    clock.SetScaleIndex(1);
    CHECK(clock.Tick(1.0F / 60.0F) == 0);
    CHECK(clock.Tick(1.0F / 60.0F) == 1);
    clock.SetPaused(true);
    CHECK(clock.Tick(1.0F) == 0);
    CHECK(clock.StepRender() == 0);
    CHECK(clock.SinceAdvance() == Catch::Approx(1.0F / 120.0F));
    CHECK(clock.StepRender() == 1);
    CHECK(clock.SinceAdvance() == Catch::Approx(0.0F).margin(1e-6));
    CHECK(clock.StepRender() == 0);
    CHECK(clock.SinceAdvance() == Catch::Approx(1.0F / 120.0F));
    CHECK(clock.Tick(1.0F) == 0);
    clock.SetPaused(false);
    clock.SetScaleIndex(0);
    CHECK(clock.Tick(1.0F) == 4);
    CHECK(clock.SinceAdvance() == 0.0F);
    clock.SetScaleIndex(99);
    CHECK(clock.Scale() == Judge::kTimeScales.back());
}
