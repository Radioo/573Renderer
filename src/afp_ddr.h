#pragma once

#include <string>
#include <cstdint>
#include <vector>

class DllLoader;
struct AvsFuncs;
struct D3D9State;

namespace DdrAfp {

bool Boot(DllLoader& afp_dll, DllLoader& afpu_dll, D3D9State& d3d);

bool LoadIfs(AvsFuncs& avs, DllLoader& avs_dll, const std::string& ifs_disk_path,
             const std::string& pkg_name);

void RenderFrame(float dt);

void SetTimeScale(float s);

bool IsBooted();
uint32_t LayerId();

uint32_t LoopFrames();

uint32_t FrameCounter();

int ClipCurrentFrame();
int ClipLabelFrame(const char* name);

struct Label {
    std::string name;
    int frame = 0;
};

bool ReadPlayhead(uint32_t* cur, uint32_t* total, uint32_t* raw_loop_count);

bool ReadSize(uint32_t* w, uint32_t* h);

std::vector<Label> EnumerateLabels();

void SetPaused(bool paused);

bool SeekFrame(int frame);

bool GotoLabel(const std::string& name);

std::vector<std::string> ClipNames();
std::string ActiveClip();
bool SwitchClip(const std::string& name);

void Shutdown();

}
