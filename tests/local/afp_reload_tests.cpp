#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "afp_boot.h"
#include "app_globals.h"
#include "avs_boot.h"
#include "backend/afp_profiles.h"
#include "document/preview_packages.h"
#include "engine_dlls.h"
#include "render_seh.h"
#include "render_backend.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"
#include "support/env.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr const char* kPackageFile = "/data/graphic/1/title.ifs";
constexpr const char* kPackage = "title";
constexpr const char* kAnimation = "title";
constexpr const char* kProbeLabel = "editor_probe";
constexpr int kLoopFrame = 240;
constexpr int kProbeFrame = 100;
constexpr uint32_t kSeekFrame = 300;

std::vector<uint8_t> ReadHostFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

Ifs::Entry& Child(std::vector<Ifs::Entry>& entries, const std::string& name) {
    INFO("entry " << name);
    const auto found = std::ranges::find_if(entries, [&](const Ifs::Entry& e) {
        return e.name == name || e.name == Ifs::HashedName(name);
    });
    REQUIRE(found != entries.end());
    return *found;
}

std::vector<uint8_t> WithMovedLabel(const std::vector<uint8_t>& ifs) {
    auto archive = Ifs::Read(ifs);
    REQUIRE(archive.has_value());
    Ifs::Entry& afp = Child(archive->entries, "afp");
    Ifs::Entry& data = Child(afp.children, kAnimation);
    Ifs::Entry& script = Child(Child(afp.children, "bsi").children, kAnimation);
    auto animation = AfpAnimation::ReadStored(data.bytes, script.bytes);
    REQUIRE(animation.has_value());
    REQUIRE(!animation->root.labels.empty());
    animation->strings.emplace_back(kProbeLabel);
    animation->root.labels[0].name =
        static_cast<AfpAnimation::StringId>(animation->strings.size() - 1);
    animation->root.labels[0].frame = kProbeFrame;
    auto stored = AfpAnimation::WriteStored(*animation);
    REQUIRE(stored.has_value());
    data.bytes = stored->data;
    script.bytes = stored->script;
    auto written = Ifs::Write(*archive);
    REQUIRE(written.has_value());
    return *written;
}

bool HasLabel(const std::string& name, int frame) {
    const auto labels = AfpManager::EnumerateLabels(g_afp);
    return std::ranges::any_of(
        labels, [&](const AfpManager::Label& l) { return l.name == name && l.frame == frame; });
}

HWND HiddenWindow() {
    const char* kClass = "r573_afp_reload_test";
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = kClass;
    RegisterClassExA(&wc);
    return CreateWindowExA(0, kClass, "afp reload", WS_OVERLAPPED, 0, 0, 1920, 1080, nullptr,
                           nullptr, GetModuleHandleA(nullptr), nullptr);
}

HWND BootEngine(const std::string& dir) {
    const AfpProfiles::AfpConfig* config = AfpProfiles::For("iidx33");
    REQUIRE(config != nullptr);
    AfpManager::SetActiveConfig(config);
    const std::string dll_dir = EngineDlls::DiscoverDllDir(dir, *config);
    REQUIRE(!dll_dir.empty());
    REQUIRE(EngineDlls::Load(g_engine, dll_dir, *config, false));
    REQUIRE(AvsManager::Boot(g_avs));
    HWND hwnd = HiddenWindow();
    REQUIRE(hwnd != nullptr);
    g_d3d.width = 1920;
    g_d3d.height = 1080;
    REQUIRE(g_d3d.Init(hwnd));
    REQUIRE(AfpManager::Boot(g_engine, g_d3d));
    return hwnd;
}

AvsManager::MemoryIfs Memory(const std::vector<uint8_t>& bytes) {
    return AvsManager::MemoryIfs{
        .bytes = bytes, .ramfs_mountpoint = "/memory_ifs/title", .mountpoint = "/afp/packages"};
}

std::vector<uint8_t> FrameAt(uint32_t frame) {
    REQUIRE(AfpManager::SeekFrame(g_afp, static_cast<int>(frame)));
    g_d3d.BeginFrame();
    RenderSeh::SafeCallSortRender(g_afp.afp_do_sort_render);
    g_d3d.EndFrame();
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    REQUIRE(g_d3d.ReadOffscreenBGRA(pixels, width, height));
    REQUIRE(width == 1920);
    REQUIRE(height == 1080);
    return pixels;
}

}

TEST_CASE("A package split into its textures and the rest draws like the whole package") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    HWND hwnd = BootEngine(dir);
    const int boundary = AfpD3D9::PersistentBoundary();

    const std::vector<uint8_t> original = ReadHostFile(dir + kPackageFile);
    REQUIRE(AfpManager::LoadPackageFromMemory(g_engine, Memory(original), kPackage, kAnimation));
    const std::vector<uint8_t> whole = FrameAt(kSeekFrame);
    CHECK(std::ranges::any_of(whole, [](uint8_t b) { return b != 0; }));
    AfpManager::UnloadPackages(g_engine);

    const auto archive = Ifs::Read(original);
    REQUIRE(archive.has_value());
    const Document::PreviewPackages split = Document::SplitPreviewPackages(*archive);
    const auto textures = Ifs::Write(split.textures);
    const auto content = Ifs::Write(split.content);
    REQUIRE(textures.has_value());
    REQUIRE(content.has_value());
    REQUIRE(
        AfpManager::LoadTexturePackageFromMemory(g_engine, Memory(*textures), "title_textures"));
    CHECK(AfpD3D9::PersistentBoundary() > boundary);
    REQUIRE(AfpManager::LoadPackageFromMemory(g_engine, Memory(*content), kPackage, kAnimation));
    CHECK(FrameAt(kSeekFrame) == whole);

    const int slots = AfpD3D9::NextSlot();
    REQUIRE(AfpManager::ReloadPackageFromMemory(g_engine, Memory(*content), kPackage, kAnimation));
    CHECK(AfpD3D9::NextSlot() == slots);
    CHECK(FrameAt(kSeekFrame) == whole);

    AfpManager::UnloadPackages(g_engine);
    AfpManager::UnloadTexturePackage(g_engine);
    CHECK(AfpD3D9::PersistentBoundary() == boundary);
    CHECK(AfpD3D9::NextSlot() == boundary);
    DestroyWindow(hwnd);
}

TEST_CASE("An edited package reloads from memory under the same name at the same frame") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");
    HWND hwnd = BootEngine(dir);

    const std::vector<uint8_t> original = ReadHostFile(dir + kPackageFile);
    REQUIRE(!original.empty());
    const std::vector<uint8_t> edited = WithMovedLabel(original);
    const auto memory = [](const std::vector<uint8_t>& bytes) { return Memory(bytes); };

    REQUIRE(AfpManager::LoadPackageFromMemory(g_engine, memory(original), kPackage, kAnimation));
    CHECK(AfpManager::AnimName() == kAnimation);
    CHECK(HasLabel("loop", kLoopFrame));
    REQUIRE(AfpManager::SeekFrame(g_afp, static_cast<int>(kSeekFrame)));
    const int loaded_packages = g_afpu.afpu_get_loaded_package_count();
    const int texture_slots = AfpD3D9::NextSlot();

    for (int round = 0; round < 3; round++) {
        INFO("round " << round);
        REQUIRE(
            AfpManager::ReloadPackageFromMemory(g_engine, memory(edited), kPackage, kAnimation));
        CHECK(HasLabel(kProbeLabel, kProbeFrame));
        CHECK_FALSE(HasLabel("loop", kLoopFrame));
        uint32_t cur = 0;
        uint32_t total = 0;
        REQUIRE(AfpManager::ReadMcPlayhead(g_afp, &cur, &total, nullptr));
        CHECK(cur == kSeekFrame);
        CHECK(g_afpu.afpu_get_loaded_package_count() == loaded_packages);
        CHECK(AfpD3D9::NextSlot() == texture_slots);

        REQUIRE(
            AfpManager::ReloadPackageFromMemory(g_engine, memory(original), kPackage, kAnimation));
        CHECK(HasLabel("loop", kLoopFrame));
        CHECK_FALSE(HasLabel(kProbeLabel, kProbeFrame));
    }

    AfpManager::UnloadPackages(g_engine);
    DestroyWindow(hwnd);
}
