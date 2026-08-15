#include "preset/preset_host.h"

#include "preset/asset_index.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_validate.h"
#include "preset/eval/eval_push.h"
#include "preset/eval/eval_resolve.h"
#include "preset/eval/eval_state.h"
#include "preset/eval/frame_state.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/preset_asset_lengths.h"
#include "preset/preset_host_params.h"
#include "preset/preset_host_push.h"

#include "gc2d/gc_host.h"
#include "scene3d/scene3d_host.h"
#include "scene3d/scene3d_render.h"
#include "support/log.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace PresetHost {

namespace {

namespace Doc = Preset::Doc;
using Preset::Eval::FrameState;
using Preset::Eval::Push;

constexpr float kPresetTicksPerSecond = 60.0F;

enum class CommandKind : unsigned char { Replace, Seek, Paused, Option };

struct Command {
    CommandKind kind = CommandKind::Seek;
    std::shared_ptr<const Doc::Document> document;
    ProgressFn progress;
    int a = 0;
    int b = 0;
    bool flag = false;
};

Command ReplaceCommand(const std::shared_ptr<const Doc::Document>& document,
                       const ProgressFn& progress) {
    Command command;
    command.kind = CommandKind::Replace;
    command.document = document;
    command.progress = progress;
    return command;
}

Command SeekCommand(int frame) {
    Command command;
    command.kind = CommandKind::Seek;
    command.a = frame;
    return command;
}

Command PausedCommand(bool paused) {
    Command command;
    command.kind = CommandKind::Paused;
    command.flag = paused;
    return command;
}

Command OptionCommand(int option, int choice) {
    Command command;
    command.kind = CommandKind::Option;
    command.a = option;
    command.b = choice;
    return command;
}

struct Shared {
    std::shared_ptr<const Doc::Document> base;
    std::shared_ptr<const Doc::Document> effective;
    std::vector<Override> overrides;
    Preset::AssetIndex assets;
    Status status;
    FrameState frame;
    std::vector<Command> queue;
};

std::mutex g_lock;
Shared g_shared;
std::atomic<bool> g_loaded{false};

Preset::Eval::Evaluator g_eval;
std::shared_ptr<const Doc::Document> g_active;
Preset::AssetLengths g_lengths;
std::string g_game_dir;
std::vector<Push> g_state_pushes;
float g_accum = 0.0F;
bool g_playing = true;
int g_problems = 0;

std::string Resolve(const std::string& game_dir, const std::string& relative) {
    if (game_dir.empty()) return relative;
    return (std::filesystem::path(game_dir) / std::filesystem::path(relative)).string();
}

int LengthOf(const Doc::Document& document) {
    return document.length.value_or(0);
}

Scene3dHost::Setup BuildSetup(const FrameState& state) {
    Scene3dHost::Setup setup;
    setup.ticks_per_second = kPresetTicksPerSecond;
    setup.style = (state.shading == Doc::Shading::LitMaterial) ? Scene3d::RenderStyle::LitMaterial
                                                               : Scene3d::RenderStyle::TextureOnly;
    setup.projection.fov_y = state.camera.fov_y;
    setup.projection.near_z = state.camera.near_z;
    setup.projection.far_z = state.camera.far_z;
    setup.projection.aspect = state.camera.aspect_auto ? 0.0F : state.camera.aspect_value;
    setup.eye = state.camera.eye;
    setup.at = state.camera.at;
    setup.up = state.camera.up;
    for (const Preset::Eval::LightState& light : state.lights) {
        setup.lights.push_back(Scene3d::Light{.direction = light.direction,
                                              .diffuse = light.diffuse,
                                              .specular = light.specular,
                                              .enabled = light.enabled});
    }
    for (const Preset::Eval::ModelSlot& slot : state.models) {
        setup.models.push_back(Scene3dHost::ModelSetup{.model = slot.name,
                                                       .blend_mode = slot.blend_mode,
                                                       .alpha = slot.alpha,
                                                       .anim_speed = slot.anim_speed,
                                                       .position = slot.position,
                                                       .rotation = slot.rotation,
                                                       .scale = slot.scale});
    }
    return setup;
}

void Report(const ProgressFn& progress, const std::string& stage, int done, int total) {
    if (!progress) return;
    const float fraction = (total > 0) ? ((float)done / (float)total) : -1.0F;
    progress(stage, fraction);
}

bool LoadAssets(const Doc::Document& document, const FrameState& first,
                const ProgressFn& progress) {
    std::vector<std::string> scene_dirs;
    std::vector<const Doc::Asset*> packages;
    for (const Doc::Asset& asset : document.assets) {
        if (asset.kind == Doc::AssetKind::Scene3d) {
            scene_dirs.push_back(Resolve(g_game_dir, asset.dir));
        } else {
            packages.push_back(&asset);
        }
    }
    const int total = (int)scene_dirs.size() + (int)packages.size();
    int done = 0;
    if (!scene_dirs.empty()) {
        Report(progress, "3D scenes (" + std::to_string(scene_dirs.size()) + ")", done, total);
        if (!Scene3dHost::LoadUnion(scene_dirs, BuildSetup(first))) {
            LOG("Preset", "3D scene dirs failed to load");
            return false;
        }
        done += (int)scene_dirs.size();
    }
    for (const Doc::Asset* asset : packages) {
        Report(progress, "2D package " + asset->id, done, total);
        if (!Gc2dHost::LoadAsset(asset->id, Resolve(g_game_dir, asset->dir))) {
            LOG("Preset", "2D package '%s' failed to load", asset->dir.c_str());
            return false;
        }
        done++;
    }
    Report(progress, "ready", done, total);
    Gc2dHost::SetCanvas(document.render.width, document.render.height);
    return true;
}

Preset::AssetIndex BuildIndex(const Doc::Document& document) {
    Preset::AssetIndex index;
    for (const Doc::Asset& asset : document.assets) {
        Preset::AssetEntry entry;
        entry.id = asset.id;
        entry.kind = asset.kind;
        entry.dir = asset.dir;
        if (asset.kind == Doc::AssetKind::Scene3d) {
            const Scene3dHost::SceneInfo info =
                Scene3dHost::DescribeScene(Resolve(g_game_dir, asset.dir));
            entry.loaded = info.loaded;
            entry.models = info.models;
        } else {
            const Gc2dHost::PackageInfo info = Gc2dHost::DescribePackage(asset.id);
            entry.loaded = info.loaded;
            entry.cells = info.cells;
            for (const Gc2dHost::AnimationInfo& animation : info.animations) {
                entry.animations.push_back(
                    Preset::AssetAnimation{.name = animation.name, .frames = animation.frames});
            }
        }
        index.assets.push_back(std::move(entry));
    }
    return index;
}

Preset::AssetLengths BuildLengths(const Preset::AssetIndex& index) {
    Preset::AssetLengths lengths;
    for (const Preset::AssetEntry& entry : index.assets) {
        if (entry.kind == Doc::AssetKind::Scene3d) {
            lengths.scene_ticks[entry.dir] =
                Scene3dHost::DescribeScene(Resolve(g_game_dir, entry.dir)).max_time;
            continue;
        }
        for (const Preset::AssetAnimation& animation : entry.animations)
            lengths.animation_frames[entry.dir][animation.name] = animation.frames;
    }
    return lengths;
}

int PulseGrid(const FrameState& state) {
    for (const Preset::Eval::ModelSlot& slot : state.models) {
        if (slot.has_pulse) return (slot.pulse.grid == Doc::Grid::B) ? 1 : 0;
    }
    return 0;
}

Status BuildStatus(const Doc::Document& document, const FrameState& frame, int problems) {
    const Preset::Eval::EvalState& state = g_eval.State();
    Status status;
    status.id = document.id;
    status.name = document.name;
    status.frame = state.frame;
    status.length = LengthOf(document);
    status.fps = document.fps;
    status.playing = g_playing;
    status.problems = problems;
    status.countdown_start = status.length;
    status.countdown = std::max(0, status.length - state.frame);
    if (!frame.models.empty()) {
        const Preset::Eval::ModelSlot& lead = frame.models.front();
        status.model_speed = lead.anim_speed;
        status.model_alpha = lead.alpha;
        status.blend_mode = lead.blend_mode;
    }
    const auto grid = (std::size_t)PulseGrid(frame);
    status.beat = state.beat[grid];
    status.beat_since = state.beat_since[grid];
    status.pulse_scale = state.pulse;
    status.jitter = state.jitter;
    status.live_particles = (int)state.particles.size();
    status.option_choices = state.choices;
    return status;
}

int ErrorCount(const Doc::Document& document) {
    int errors = 0;
    for (const Doc::Problem& problem : Doc::Validate(document)) {
        if (problem.severity == Doc::Severity::Error) errors++;
    }
    return errors;
}

void Publish() {
    if (g_active == nullptr) return;
    Status status = BuildStatus(*g_active, g_eval.Current(), g_problems);
    FrameState frame = g_eval.Current();
    const std::scoped_lock guard(g_lock);
    g_shared.status = std::move(status);
    g_shared.frame = std::move(frame);
}

void Post(const Command& command) {
    const std::scoped_lock guard(g_lock);
    g_shared.queue.push_back(command);
}

bool SameAssets(const Doc::Document& a, const Doc::Document& b) {
    return a.assets == b.assets && a.render.width == b.render.width &&
           a.render.height == b.render.height;
}

void ApplyReplace(const std::shared_ptr<const Doc::Document>& document,
                  const ProgressFn& progress) {
    if (document == nullptr || g_active == nullptr) return;
    const int frame = g_eval.State().frame;
    const std::vector<int> choices = g_eval.State().choices;
    if (!SameAssets(*g_active, *document)) {
        g_eval.Load(document, {});
        if (!LoadAssets(*document, g_eval.Current(), progress)) return;
        Preset::AssetIndex index = BuildIndex(*document);
        g_lengths = BuildLengths(index);
        const std::scoped_lock guard(g_lock);
        g_shared.assets = std::move(index);
    }
    g_eval.Load(document, g_lengths);
    for (std::size_t i = 0; i < choices.size(); i++)
        g_eval.SetOption((int)i, choices[i]);
    g_active = document;
    g_state_pushes = g_eval.Seek(frame);
    ApplyPushes(g_state_pushes);
}

void Drain() {
    std::vector<Command> queue;
    {
        const std::scoped_lock guard(g_lock);
        queue.swap(g_shared.queue);
    }
    for (const Command& command : queue) {
        switch (command.kind) {
        case CommandKind::Replace:
            ApplyReplace(command.document, command.progress);
            break;
        case CommandKind::Seek:
            g_state_pushes = g_eval.Seek(command.a);
            ApplyPushes(g_state_pushes);
            g_accum = 0.0F;
            break;
        case CommandKind::Paused:
            g_playing = !command.flag;
            break;
        case CommandKind::Option:
            g_eval.SetOption(command.a, command.b);
            break;
        }
    }
}

std::shared_ptr<const Doc::Document> EffectiveDocument() {
    const std::scoped_lock guard(g_lock);
    return g_shared.effective;
}

std::shared_ptr<const Doc::Document> BaseDocument() {
    const std::scoped_lock guard(g_lock);
    return g_shared.base;
}

void RebuildEffective() {
    std::shared_ptr<const Doc::Document> next;
    {
        const std::scoped_lock guard(g_lock);
        if (g_shared.base == nullptr) return;
        next = std::make_shared<const Doc::Document>(
            WithOverrides(*g_shared.base, g_shared.overrides));
        g_shared.effective = next;
    }
    ReplaceDocument(next);
}

bool Prepare(const std::string& game_dir, const std::shared_ptr<const Doc::Document>& probe,
             const ProgressFn& progress) {
    Unload();
    if (probe == nullptr) return false;
    g_game_dir = game_dir;
    g_eval.Load(probe, {});
    if (!LoadAssets(*probe, g_eval.Current(), progress)) {
        Scene3dHost::Unload();
        Gc2dHost::Unload();
        return false;
    }
    Preset::AssetIndex index = BuildIndex(*probe);
    g_lengths = BuildLengths(index);
    const std::scoped_lock guard(g_lock);
    g_shared.assets = std::move(index);
    return true;
}

void Finish(const std::shared_ptr<const Doc::Document>& document) {
    g_eval.Load(document, g_lengths);
    g_active = document;
    g_accum = 0.0F;
    g_playing = true;
    g_problems = ErrorCount(*document);
    g_state_pushes = g_eval.Rebind();
    ApplyPushes(g_state_pushes);
    {
        const std::scoped_lock guard(g_lock);
        g_shared.base = document;
        g_shared.effective = document;
        g_shared.overrides.clear();
        g_shared.queue.clear();
    }
    g_loaded = true;
    Publish();
    LOG("Preset", "'%s' ready: %zu asset(s), %d frame(s) at %d fps", document->name.c_str(),
        document->assets.size(), LengthOf(*document), document->fps);
}

}

bool LoadDocument(const std::string& game_dir, const std::shared_ptr<const Doc::Document>& document,
                  const ProgressFn& progress) {
    if (!Prepare(game_dir, document, progress)) return false;
    Finish(document);
    return true;
}

void ReplaceDocument(const std::shared_ptr<const Doc::Document>& document,
                     const ProgressFn& progress) {
    if (!g_loaded) return;
    Post(ReplaceCommand(document, progress));
}

void Unload() {
    if (!g_loaded) return;
    Scene3dHost::Unload();
    Gc2dHost::Unload();
    g_loaded = false;
    g_active.reset();
    g_state_pushes.clear();
    g_lengths = Preset::AssetLengths{};
    g_accum = 0.0F;
    const std::scoped_lock guard(g_lock);
    g_shared = Shared{};
}

bool Active() {
    return g_loaded;
}

void RenderFrame(float dt) {
    if (!g_loaded) return;
    Drain();
    if (!g_loaded || g_active == nullptr) return;

    const int fps = std::max(1, g_active->fps);
    const float step = 1.0F / (float)fps;
    bool advance = false;
    if (g_playing) {
        g_accum += dt;
        if (g_accum + 1.0e-6F >= step) {
            g_accum -= step;
            advance = true;
        }
    }
    ApplyPushes(g_eval.DrawFrame(0.0F));
    if (advance) {
        const int length = LengthOf(*g_active);
        if (length > 0 && g_eval.State().frame + 1 >= length) {
            g_state_pushes = g_eval.Seek(0);
        } else {
            g_state_pushes = g_eval.AdvanceFrame();
        }
    }
    ApplyPushes(g_state_pushes);
    Publish();
}

Status GetStatus() {
    const std::scoped_lock guard(g_lock);
    return g_shared.status;
}

Preset::AssetIndex GetAssetIndex() {
    const std::scoped_lock guard(g_lock);
    return g_shared.assets;
}

void Seek(int frame) {
    if (!g_loaded) return;
    const int length = GetStatus().length;
    const int wanted = (length > 0) ? std::min(frame, length - 1) : frame;
    Post(SeekCommand(std::max(0, wanted)));
}

void SetPaused(bool paused) {
    if (!g_loaded) return;
    Post(PausedCommand(paused));
}

void SetCountdown(int frames) {
    const Status status = GetStatus();
    if (status.length <= 0) return;
    Seek(std::clamp(status.length - frames, 0, status.length - 1));
}

void SetOption(int option, int choice) {
    if (!g_loaded) return;
    Post(OptionCommand(option, choice));
}

int NaturalFrames() {
    return GetStatus().length;
}

bool OpaqueScreen() {
    const std::shared_ptr<const Doc::Document> document = EffectiveDocument();
    return document != nullptr && document->render.opaque;
}

void Restart() {
    Seek(0);
}

std::vector<StateView> ListStates() {
    std::vector<StateView> out;
    const std::shared_ptr<const Doc::Document> document = EffectiveDocument();
    if (document == nullptr) return out;
    const std::vector<int> choices = GetStatus().option_choices;
    for (std::size_t i = 0; i < document->options.size(); i++) {
        const Doc::OptionSpec& option = document->options[i];
        StateView view;
        view.id = option.id;
        view.label = option.label;
        view.choice = (i < choices.size()) ? choices[i] : option.default_choice;
        for (const Doc::ChoiceSpec& choice : option.choices) {
            view.choices.push_back(choice.label);
            for (const Doc::ChoiceValue& value : choice.values) {
                if (value.id == "camera.eye") view.moves_camera = true;
            }
        }
        out.push_back(std::move(view));
    }
    return out;
}

namespace {

FrameState BaseFrame(const FrameState& effective, const std::vector<int>& choices) {
    const std::shared_ptr<const Doc::Document> base = BaseDocument();
    if (base == nullptr) return effective;
    const Preset::Eval::ResolveInput input{
        .document = base.get(), .choices = &choices, .tweens = true};
    return Preset::Eval::ResolveFrame(input, effective.frame);
}

}

std::vector<ParamView> ListParams() {
    FrameState effective;
    std::vector<Override> overrides;
    std::vector<int> choices;
    {
        const std::scoped_lock guard(g_lock);
        if (g_shared.base == nullptr) return {};
        effective = g_shared.frame;
        overrides = g_shared.overrides;
        choices = g_shared.status.option_choices;
    }
    return ListParamViews(effective, BaseFrame(effective, choices), overrides);
}

void SetParam(const std::string& id, const std::array<float, 3>& value, int ivalue) {
    bool changed = false;
    {
        const std::scoped_lock guard(g_lock);
        if (g_shared.base == nullptr) return;
        changed = WriteOverride(g_shared.overrides, id, value, ivalue, g_shared.frame);
    }
    if (changed) RebuildEffective();
}

void ResetParam(const std::string& id) {
    {
        const std::scoped_lock guard(g_lock);
        ClearOverride(g_shared.overrides, id);
    }
    RebuildEffective();
}

void ResetGroup(const std::string& group) {
    {
        const std::scoped_lock guard(g_lock);
        ClearOverrideGroup(g_shared.overrides, group, g_shared.frame);
    }
    RebuildEffective();
}

void ResetAllParams() {
    {
        const std::scoped_lock guard(g_lock);
        g_shared.overrides.clear();
    }
    RebuildEffective();
}

int ChangedParamCount() {
    const std::scoped_lock guard(g_lock);
    return (int)g_shared.overrides.size();
}

}
