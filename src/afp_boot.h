#pragma once

#include "afp_funcs.h"
#include "afpu_funcs.h"
#include "avs_funcs.h"
#include "support/dll_loader.h"
#include "game_profile.h"
#include "render_backend.h"
#include <string>
#include <vector>

struct EngineSession;

namespace AfpManager {
void SetActiveProfile(const GameProfile::Profile* p);

bool Boot(EngineSession& es, D3D9State& d3d);

bool LoadPackages(EngineSession& es, const std::string& pkg_hint = "");

void LoadBootIfses(EngineSession& es, const std::string& data_dir);

void Shutdown(EngineSession& es);

void DestroySceneStreams(AfpFuncs& afp);

void UnloadPackages(EngineSession& es);

bool SwitchAnimation(EngineSession& es, const std::string& anim_name, bool force = false);

bool PlayBitmapAnimation(EngineSession& es, const std::string& bitmap_name);

bool ForceReplay(EngineSession& es);

void DestroyCurrentStream(AfpFuncs& afp);

bool IsMasterComplete(const AfpFuncs& afp);

bool ReadLayerPosition(const AfpFuncs& afp, uint32_t* cur, uint32_t* total);

bool ReadMcPlayhead(const AfpFuncs& afp, uint32_t* cur, uint32_t* total, uint32_t* loop_count);

bool ReadLayerAdvanceCounter(const AfpFuncs& afp, uint32_t* counter);

uint32_t LoadCompanion(EngineSession& es, const std::string& companion_path,
                       const std::string& pkg_name);

void UnloadCompanion(EngineSession& es, uint32_t pkg_id);

void UnloadAllCompanions(EngineSession& es);

int SwapClipBitmapFromCompanion(EngineSession& es, uint32_t pkg_id, uint32_t stream_id,
                                const char* clip_name);

std::string LastCompanionMountPoint();

bool IsBooted();
uint32_t StreamId();
uint32_t PackageId();
const std::string& AnimName();

struct Label {
    std::string name;
    int frame = 0;
};

std::vector<Label> EnumerateLabels(const AfpFuncs& afp);

bool GotoLabel(const AfpFuncs& afp, const std::string& label);

bool SeekFrame(const AfpFuncs& afp, int frame);

void SetStreamPaused(const AfpFuncs& afp, bool paused);

struct ChildClip {
    std::string name;
    float screen_x = 0.0f;
    float screen_y = 0.0f;
    bool have_pos = false;
    int cur = -1;
    int total = -1;
    bool have_playhead = false;
};

int GetRootMcId(const AfpFuncs& afp);

std::vector<ChildClip> EnumerateChildClips(const AfpFuncs& afp, bool want_positions = true,
                                           bool* ok = nullptr);
}
