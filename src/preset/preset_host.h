#pragma once

#include "preset/asset_index.h"
#include "preset/doc/preset_document.h"
#include "preset/preset_preview.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace PresetHost {

struct Status {
    std::string id;
    std::string name;
    float model_speed = 0.0F;
    float model_alpha = 1.0F;
    int blend_mode = 0;
    int frame = 0;
    int length = 0;
    int fps = 60;
    bool playing = false;
    bool loop = true;
    int problems = 0;
    int beat = 0;
    int beat_since = 0;
    float pulse_scale = 1.0F;
    float jitter = 0.0F;
    int live_particles = 0;
    std::vector<int> option_choices;
};

using ProgressFn = std::function<void(const std::string& stage, float fraction)>;

bool LoadDocument(const std::string& game_dir,
                  const std::shared_ptr<const Preset::Doc::Document>& document,
                  const ProgressFn& progress = {});

void ReplaceDocument(const std::shared_ptr<const Preset::Doc::Document>& document,
                     const ProgressFn& progress = {});

void Unload();

bool Active();

void RenderFrame(float dt);

Status GetStatus();

std::shared_ptr<const Preset::AssetIndex> GetAssetIndex();

void RequestPreview(const std::string& asset, const std::string& animation,
                    const std::vector<std::string>& hidden_parts, int samples);

void PumpPreview();

Preset::Preview::SnapshotPtr GetPreview();

void Seek(int frame);

void SetPaused(bool paused);

void SetLoop(bool loop);

void SetOption(int option, int choice);

int NaturalFrames();

bool OpaqueScreen();

void Restart();

}
