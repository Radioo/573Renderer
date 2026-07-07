#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct AfpFuncs;
namespace App {
struct Request;
}

namespace RenderLive {

namespace Inspect {

struct Label {
    std::string name;
    int frame = 0;
};

bool HaveActiveClip(uint32_t modern_stream_id);

bool ReadPlayhead(uint32_t modern_stream_id, uint32_t* cur, uint32_t* total,
                  uint32_t* raw_loop_count);

bool ReadSize(uint32_t modern_stream_id, uint32_t* w, uint32_t* h);

bool ReadRawLayerInfo(uint32_t modern_stream_id, uint32_t* raw_cur, uint32_t* raw_total,
                      uint32_t* flags0);

void ReadComplete(const AfpFuncs& afp, uint32_t modern_stream_id, bool* complete);

std::vector<Label> EnumerateLabels(const AfpFuncs& afp, uint32_t modern_stream_id);

void SetPaused(const AfpFuncs& afp, bool paused);

bool SeekFrame(const AfpFuncs& afp, int frame);

bool GotoLabel(const AfpFuncs& afp, const std::string& name);

}

bool HandleSeekRequest(const App::Request& req, AfpFuncs& afp);
bool HandlePauseRequest(const App::Request& req, AfpFuncs& afp);

void NotifySeek();

void ResetPauseDefend();

void PublishLiveState(AfpFuncs& afp, uint32_t stream_id, int frames_since_switch, bool exporting);

}
