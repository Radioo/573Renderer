#pragma once
#include <string>
#include <cstdint>
#include <windows.h>
#include "cli/cli.h"

bool BootFromGameDir(HINSTANCE hInstance, const std::string& game_dir, bool want_render_window,
                     bool load_boot_ifses, int render_w, int render_h,
                     const std::string& profile_slug);

bool MountAndLoadIfs(const std::string& ifs_path, bool from_arc = false);

void ApplyCliOverrides(const Cli::Options& opts);

void ApplyVariants(uint32_t stream_id);

void ApplySubLayerVisibility(uint32_t stream_id);
