#include "preview/host/preview_host_session.h"

#include "afp_boot.h"
#include "app_globals.h"
#include "avs_boot.h"
#include "backend/afp_profiles.h"
#include "engine_dlls.h"
#include "game_profile.h"
#include "preview_host_generated.h"
#include "render_seh.h"
#include "shared_frame.h"

#include <flatbuffers/buffer.h>
#include <flatbuffers/flatbuffer_builder.h>
#include <flatbuffers/verifier.h>

#include <windows.h>

#include <bit>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace PreviewHost {

namespace {

constexpr const char* kWindowClass = "r573_preview_host";
constexpr const char* kRamfsPoint = "/preview_ifs/package";
constexpr const char* kPackagePoint = "/afp/packages";

std::vector<uint8_t> Finished(const flatbuffers::FlatBufferBuilder& builder) {
    const std::span<const uint8_t> bytes(builder.GetBufferPointer(), builder.GetSize());
    return {bytes.begin(), bytes.end()};
}

std::vector<uint8_t> Failure(const std::string& message) {
    flatbuffers::FlatBufferBuilder builder;
    const auto failure = PreviewProtocol::CreateFailure(builder, builder.CreateString(message));
    builder.Finish(PreviewProtocol::CreateReplyMessage(builder, PreviewProtocol::Reply::Failure,
                                                       failure.Union()));
    return Finished(builder);
}

std::vector<uint8_t> Done() {
    flatbuffers::FlatBufferBuilder builder;
    const auto done = PreviewProtocol::CreateDone(builder);
    builder.Finish(
        PreviewProtocol::CreateReplyMessage(builder, PreviewProtocol::Reply::Done, done.Union()));
    return Finished(builder);
}

std::vector<uint8_t> LoadedReply() {
    uint32_t total = 0;
    AfpManager::ReadMcPlayhead(g_afp, nullptr, &total, nullptr);
    flatbuffers::FlatBufferBuilder builder;
    std::vector<flatbuffers::Offset<PreviewProtocol::Label>> labels;
    for (const AfpManager::Label& label : AfpManager::EnumerateLabels(g_afp)) {
        labels.push_back(PreviewProtocol::CreateLabel(builder, builder.CreateString(label.name),
                                                      static_cast<uint32_t>(label.frame)));
    }
    const auto loaded = PreviewProtocol::CreateLoaded(builder, total, builder.CreateVector(labels));
    builder.Finish(PreviewProtocol::CreateReplyMessage(builder, PreviewProtocol::Reply::Loaded,
                                                       loaded.Union()));
    return Finished(builder);
}

std::vector<uint8_t> SeekReply(const PreviewProtocol::Seek& seek) {
    if (!AfpManager::SeekFrame(g_afp, static_cast<int>(seek.frame())))
        return Failure("seeking failed");
    return Done();
}

bool IsFailure(std::span<const uint8_t> reply) {
    return flatbuffers::GetRoot<PreviewProtocol::ReplyMessage>(reply.data())->reply_type() ==
           PreviewProtocol::Reply::Failure;
}

std::vector<uint8_t> SelectAnimationReply(const PreviewProtocol::SelectAnimation& select) {
    if (select.name() == nullptr) return Failure("SelectAnimation needs a name");
    if (!AfpManager::SwitchAnimation(g_engine, select.name()->str(), true))
        return Failure("animation " + select.name()->str() + " did not start");
    return LoadedReply();
}

std::vector<uint8_t> ShowSymbolReply(const PreviewProtocol::ShowSymbol& show) {
    if (show.name() == nullptr) return Failure("ShowSymbol needs a name");
    if (!AfpManager::AttachSymbol(g_afp, show.name()->str()))
        return Failure("the animation has no symbol called " + show.name()->str());
    return LoadedReply();
}

HWND CreateHiddenWindow(int width, int height) {
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = kWindowClass;
    RegisterClassExA(&wc);
    return CreateWindowExA(0, kWindowClass, "preview host", WS_OVERLAPPED, 0, 0, width, height,
                           nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
}

}

Session::~Session() {
    if (package_loaded_) AfpManager::UnloadPackages(g_engine);
    if (window_ != nullptr) DestroyWindow(window_);
}

std::vector<uint8_t> Session::Handle(std::span<const uint8_t> request) {
    flatbuffers::Verifier verifier(request.data(), request.size());
    if (!PreviewProtocol::VerifyRequestMessageBuffer(verifier))
        return Failure("the request is not a valid RequestMessage");
    const auto* message = PreviewProtocol::GetRequestMessage(request.data());
    if (const auto* boot = message->request_as_Boot()) return Boot(*boot);
    if (!booted_) return Failure("the host has not booted a game yet");
    if (const auto* load = message->request_as_LoadPackage()) return LoadPackage(*load);
    if (const auto* select = message->request_as_SelectAnimation())
        return Loaded(SelectAnimationReply(*select));
    if (const auto* seek = message->request_as_Seek()) return SeekReply(*seek);
    if (const auto* show = message->request_as_ShowSymbol()) return Loaded(ShowSymbolReply(*show));
    if (const auto* fill = message->request_as_BackgroundFill()) {
        background_drawn_ = fill->drawn();
        AfpManager::SetBackgroundDrawn(g_afp, background_drawn_);
        return Done();
    }
    if (const auto* resize = message->request_as_Resize()) return Resize(*resize);
    if (message->request_as_Render() != nullptr) return Render();
    return Failure("unknown request");
}

std::vector<uint8_t> Session::Boot(const PreviewProtocol::Boot& boot) {
    if (booted_) return Failure("the host has already booted a game");
    if (boot.game_dir() == nullptr || boot.build() == nullptr)
        return Failure("Boot needs a game directory and a build");
    const std::string game_dir = boot.game_dir()->str();
    const std::string build = boot.build()->str();
    const AfpProfiles::AfpConfig* config = AfpProfiles::For(build);
    const GameProfile::Profile* profile = GameProfile::BySlug(build);
    if (config == nullptr || profile == nullptr) return Failure("unknown build " + build);
    AfpManager::SetActiveConfig(config);
    const std::string dll_dir = EngineDlls::DiscoverDllDir(game_dir, *config);
    if (dll_dir.empty()) return Failure("no game DLLs under " + game_dir);
    if (!EngineDlls::Load(g_engine, dll_dir, *config, false))
        return Failure("the game DLLs under " + dll_dir + " did not load");
    if (!AvsManager::Boot(g_avs)) return Failure("avs2-core did not boot");
    g_d3d.width = profile->default_render_w;
    g_d3d.height = profile->default_render_h;
    window_ = CreateHiddenWindow(g_d3d.width, g_d3d.height);
    if (window_ == nullptr || !g_d3d.Init(window_)) return Failure("the D3D9 device did not start");
    if (!AfpManager::Boot(g_engine, g_d3d)) return Failure("afp-core did not boot");
    booted_ = true;
    view_width_ = static_cast<uint32_t>(g_d3d.width);
    view_height_ = static_cast<uint32_t>(g_d3d.height);
    return Done();
}

std::vector<uint8_t> Session::LoadPackage(const PreviewProtocol::LoadPackage& load) {
    if (load.package() == nullptr || load.animation() == nullptr || load.ifs() == nullptr)
        return Failure("LoadPackage needs a package, an animation and IFS bytes");
    const AvsManager::MemoryIfs ifs{.bytes = {load.ifs()->begin(), load.ifs()->end()},
                                    .ramfs_mountpoint = kRamfsPoint,
                                    .mountpoint = kPackagePoint};
    const std::string package = load.package()->str();
    const std::string animation = load.animation()->str();
    const bool ok = package_loaded_
                        ? AfpManager::ReloadPackageFromMemory(g_engine, ifs, package, animation)
                        : AfpManager::LoadPackageFromMemory(g_engine, ifs, package, animation);
    if (!ok)
        return Failure("package " + package + " with animation " + animation + " did not load");
    package_loaded_ = true;
    return Loaded(LoadedReply());
}

std::vector<uint8_t> Session::Loaded(std::vector<uint8_t> reply) const {
    if (!IsFailure(reply)) AfpManager::SetBackgroundDrawn(g_afp, background_drawn_);
    return reply;
}

std::vector<uint8_t> Session::Resize(const PreviewProtocol::Resize& resize) {
    if (resize.width() == 0 || resize.height() == 0) return Failure("the viewport size is empty");
    view_width_ = resize.width();
    view_height_ = resize.height();
    frame_.reset();
    return Done();
}

std::vector<uint8_t> Session::Render() {
    if (!frame_) {
        auto target = SharedFrame::Create(g_d3d.device, static_cast<int>(view_width_),
                                          static_cast<int>(view_height_));
        if (!target) return Failure(target.error());
        frame_ = std::move(*target);
    }
    g_d3d.BeginFrame();
    if (g_afp.afp_do_sort_render != nullptr)
        RenderSeh::SafeCallSortRender(g_afp.afp_do_sort_render);
    g_d3d.EndFrame();
    auto copied = SharedFrame::Copy(g_d3d.device, g_d3d.offscreen_rt, *frame_);
    if (!copied) return Failure(copied.error());
    uint32_t current = 0;
    AfpManager::ReadMcPlayhead(g_afp, &current, nullptr, nullptr);
    flatbuffers::FlatBufferBuilder builder;
    const auto frame = PreviewProtocol::CreateFrame(
        builder, static_cast<uint64_t>(std::bit_cast<uintptr_t>(frame_->handle)), view_width_,
        view_height_, current, static_cast<uint32_t>(g_d3d.width),
        static_cast<uint32_t>(g_d3d.height));
    builder.Finish(
        PreviewProtocol::CreateReplyMessage(builder, PreviewProtocol::Reply::Frame, frame.Union()));
    return Finished(builder);
}

}
