#include "render_live.h"
#include "afp_funcs.h"
#include "afpu_funcs.h"
#include "afp_boot.h"
#include "game_runtime.h"
#include "state/app_state.h"
#include "app_globals.h"
#include "export.h"
#include "render_seh.h"
#include "state/telemetry.h"
#include "support/log.h"

#include <cstdint>
#include <corecrt.h>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace RenderLive {

namespace Inspect {

bool HaveActiveClip(uint32_t modern_stream_id) {
    return Runtime::Active().HaveActiveClip(modern_stream_id);
}

bool ReadPlayhead(uint32_t modern_stream_id, uint32_t* cur, uint32_t* total,
                  uint32_t* raw_loop_count) {
    return Runtime::Active().ReadPlayhead(modern_stream_id, cur, total, raw_loop_count);
}

bool ReadSize(uint32_t modern_stream_id, uint32_t* w, uint32_t* h) {
    return Runtime::Active().ReadSize(modern_stream_id, w, h);
}

bool ReadRawLayerInfo(uint32_t modern_stream_id, uint32_t* raw_cur, uint32_t* raw_total,
                      uint32_t* flags0) {
    return Runtime::Active().ReadRawLayerInfo(modern_stream_id, raw_cur, raw_total, flags0);
}

void ReadComplete(const AfpFuncs& afp, uint32_t modern_stream_id, bool* complete) {
    if (complete == nullptr) return;
    *complete = Runtime::Active().ReadComplete(afp, modern_stream_id);
}

std::vector<Label> EnumerateLabels(const AfpFuncs& afp, uint32_t modern_stream_id) {
    std::vector<Label> out;
    for (auto& l : Runtime::Active().EnumerateLabels(afp, modern_stream_id))
        out.push_back({.name = l.name, .frame = l.frame});
    return out;
}

void SetPaused(const AfpFuncs& afp, bool paused) {
    Runtime::Active().SetPaused(afp, paused);
}

bool SeekFrame(const AfpFuncs& afp, int frame) {
    return Runtime::Active().SeekFrame(afp, frame);
}

bool GotoLabel(const AfpFuncs& afp, const std::string& name) {
    return Runtime::Active().GotoLabel(afp, name);
}

}

namespace {
bool s_seek_happened = false;
}

void NotifySeek() {
    s_seek_happened = true;
}

namespace {
bool s_pause_applied = false;
}
namespace {
uint32_t s_pause_applied_sid = 0xFFFFFFFC;
}

void ResetPauseDefend() {
    s_pause_applied_sid = 0xFFFFFFFC;
}

bool HandleSeekRequest(const App::Request& req, AfpFuncs& afp) {
    if (!req.seek_frame) return false;
    if (Export::IsCapturing()) {
        LOG("Live", "ignoring seek to %d - export is capturing", req.seek_to_frame);
        return true;
    }
    RenderLive::Inspect::SeekFrame(afp, req.seek_to_frame);
    auto ov = App::Global().GetLiveOverrides();
    ov.paused = true;
    App::Global().SetLiveOverrides(ov);
    RenderLive::Inspect::SetPaused(afp, true);
    NotifySeek();
    return true;
}

bool HandlePauseRequest(const App::Request& req, AfpFuncs& afp) {
    if (!req.set_paused) return false;
    if (Export::IsCapturing()) {
        LOG("Live", "ignoring pause toggle - export is capturing");
        return true;
    }
    auto ov = App::Global().GetLiveOverrides();
    ov.paused = req.paused_value;
    App::Global().SetLiveOverrides(ov);
    RenderLive::Inspect::SetPaused(afp, ov.paused);
    return true;
}

namespace {
void ApplyFilter(uint32_t stream_id, bool enabled) {
    static afp_set_filter_t s_fn = nullptr;
    static bool s_resolved = false;
    static bool s_applied = false;
    static uint32_t s_applied_sid = 0xFFFFFFFC;

    if (stream_id == 0xFFFFFFFC || (int)stream_id < 0) return;
    if (enabled == s_applied && stream_id == s_applied_sid) return;

    if (!s_resolved) {
        s_fn = g_afp.afp_set_filter;
        s_resolved = true;
    }
    if (s_fn != nullptr) {
        uint8_t blob[48] = {0};
        *reinterpret_cast<uint32_t*>(blob) = 0x80000000U | (enabled ? 1U : 0U);
        s_fn((int)stream_id, blob, sizeof(blob));
        LOG("Live", "filter %s on stream 0x%08x (id 0x%08x)", enabled ? "ON" : "off", stream_id,
            0x80000000U | (enabled ? 1U : 0U));
    }
    s_applied = enabled;
    s_applied_sid = stream_id;
}
}

namespace {
void ApplyPauseDefend(AfpFuncs& afp, uint32_t active_id, bool paused) {
    if (active_id == 0xFFFFFFFC || (int)active_id < 0) return;
    if (paused == s_pause_applied && active_id == s_pause_applied_sid) return;
    Inspect::SetPaused(afp, paused);
    s_pause_applied = paused;
    s_pause_applied_sid = active_id;
}
}

namespace {
void ReadFileInfo(App::State::LiveState& ls, const App::Status& st) {
    uint32_t const pkg = AfpManager::PackageId();
    if ((g_afpu.afpuloc_get_package_info == nullptr) || (pkg == 0U) || pkg == 0xFFFFFFFE) return;
    ls.have_file_info = true;
    ls.conv_ver = g_afpu.afpuloc_get_package_info(pkg, 1);
    ls.pkg_ver = g_afpu.afpuloc_get_package_info(pkg, 3);
    ls.afp_ver = g_afpu.afpuloc_get_package_info(pkg, 4);
    const char* eng = (g_afpu.afpuloc_get_version_string != nullptr)
                          ? g_afpu.afpuloc_get_version_string(pkg, 2)
                          : nullptr;
    if (eng != nullptr) {
        strncpy_s(ls.conv_engine.data(), ls.conv_engine.size(), eng, _TRUNCATE);
    }
    ls.size_bytes = st.ifs_size_bytes;
    ls.load_time_ms = st.load_time_ms;

    static uint32_t s_logged_pkg = 0;
    if (pkg != s_logged_pkg) {
        s_logged_pkg = pkg;
        auto maj = [](uint32_t v) { return (v >> 16) & 0xFFFF; };
        auto min_ = [](uint32_t v) { return (v >> 8) & 0xFF; };
        auto pat = [](uint32_t v) { return v & 0xFF; };
        LOG("Live",
            "FILE INFO pkg=0x%08x converter %u.%u.%u(%s) package %u.%u.%u "
            "afp %u.%u.%u size=%llu",
            pkg, maj(ls.conv_ver), min_(ls.conv_ver), pat(ls.conv_ver),
            ls.conv_engine[0] ? ls.conv_engine.data() : "???", maj(ls.pkg_ver), min_(ls.pkg_ver),
            pat(ls.pkg_ver), maj(ls.afp_ver), min_(ls.afp_ver), pat(ls.afp_ver),
            (unsigned long long)ls.size_bytes);
    }
}
}

namespace {
std::vector<App::SubLayerNode> BuildSubLayerLevel(const AfpFuncs& afp, uint32_t stream_id,
                                                  int parent_mc, const std::string& parent_path,
                                                  const std::vector<std::string>& expanded,
                                                  int depth) {
    std::vector<App::SubLayerNode> out;
    if (depth > 8 || parent_mc < 0 || (afp.afp_mc_enumerate_children == nullptr)) return out;

    static thread_local char names[64][128];
    DWORD code = 0;
    int const n = RenderSeh::SafeEnumChildNames(afp.afp_mc_enumerate_children, (uint32_t)parent_mc,
                                                0, names, 64, &code);
    if (n <= 0) return out;

    std::vector<std::string> child_names;
    child_names.reserve(n);
    for (int i = 0; i < n; ++i)
        child_names.emplace_back(names[i]);

    out.reserve(child_names.size());
    for (auto& nm : child_names) {
        if (nm.empty()) continue;
        App::SubLayerNode node;
        node.name = nm;
        if (parent_path.empty()) {
            node.path = nm;
        } else {
            node.path = parent_path;
            node.path += "/";
            node.path += nm;
        }
        bool is_expanded = false;
        for (const auto& e : expanded) {
            if (e == node.path) {
                is_expanded = true;
                break;
            }
        }
        if (is_expanded && (afp.afp_mc_get_id_by_path != nullptr)) {
            int const cid = RenderSeh::SafeGetIdByPath(afp.afp_mc_get_id_by_path, stream_id,
                                                       node.path.c_str(), &code);
            if (cid > 0) {
                node.children =
                    BuildSubLayerLevel(afp, stream_id, cid, node.path, expanded, depth + 1);
                node.enumerated = true;
            }
        }
        out.push_back(std::move(node));
    }
    return out;
}
}

namespace {
App::SubLayerNode BuildSubLayerTree(const AfpFuncs& afp, uint32_t stream_id,
                                    const std::vector<std::string>& expanded) {
    App::SubLayerNode root;
    int const root_mc = AfpManager::GetRootMcId(afp);
    if (root_mc < 0) return root;
    root.children = BuildSubLayerLevel(afp, stream_id, root_mc, "", expanded, 0);
    root.enumerated = true;
    return root;
}
}

namespace {

void FillLiveState(AfpFuncs& afp, uint32_t stream_id, uint32_t active_id, int frames_since_switch,
                   bool filter_on, App::State::LiveState& lstate) {
    uint32_t raw_cur = 0;
    uint32_t raw_total = 0;
    uint32_t flags0 = 0;
    if (Inspect::ReadRawLayerInfo(stream_id, &raw_cur, &raw_total, &flags0)) {
        lstate.have_layer_info = true;
        lstate.flags0 = flags0;
        lstate.total_length = raw_total;
        lstate.cur_pos = raw_cur;
    }
    uint32_t sw = 0;
    uint32_t sh = 0;
    if (Inspect::ReadSize(stream_id, &sw, &sh)) {
        lstate.mc_w = sw;
        lstate.mc_h = sh;
    }
    lstate.stream_id = active_id;
    lstate.frames_since_switch = frames_since_switch;
    Inspect::ReadComplete(afp, stream_id, &lstate.master_complete);
    lstate.filter_on = filter_on;

    static uint32_t s_wrap_prev_cur = 0xFFFFFFFF;
    static uint32_t s_wrap_count = 0;
    static uint32_t s_wrap_sid = 0xFFFFFFFC;
    static std::string s_wrap_label;
    App::Status const wstat = App::Global().GetStatus();
    uint32_t mc_c = 0;
    uint32_t mc_t = 0;
    uint32_t mc_lc = 0;
    if (Inspect::HaveActiveClip(stream_id) &&
        Inspect::ReadPlayhead(stream_id, &mc_c, &mc_t, &mc_lc)) {
        const bool restarted = (active_id != s_wrap_sid) || (wstat.active_label != s_wrap_label) ||
                               (frames_since_switch == 0) || s_seek_happened;
        if (restarted) {
            s_wrap_prev_cur = 0xFFFFFFFF;
            s_wrap_count = 0;
            s_wrap_sid = active_id;
            s_wrap_label = wstat.active_label;
        }
        if (s_wrap_prev_cur != 0xFFFFFFFF && mc_c < s_wrap_prev_cur) ++s_wrap_count;
        s_wrap_prev_cur = mc_c;

        lstate.have_mc_playhead = true;
        lstate.label_active = wstat.label_playback_active;
        lstate.mc_cur = mc_c;
        lstate.mc_total = mc_t;
        lstate.mc_loop_count = mc_lc;
        lstate.mc_wrap_count = s_wrap_count;
    }
    if (Runtime::Active().SupportsLiveExtras()) ReadFileInfo(lstate, wstat);
    s_seek_happened = false;
}

void RefreshSubLayerTree(AfpFuncs& afp, uint32_t stream_id, App::Status& st) {
    static int s_tree_wait = 0;
    if (--s_tree_wait > 0) return;
    s_tree_wait = 15;
    st.mc_tree = BuildSubLayerTree(afp, stream_id, App::Global().GetSublayerExpanded());
    static uint32_t s_tl_sid = 0xFFFFFFFC;
    static auto s_tl_n = (size_t)-1;
    if (stream_id != s_tl_sid || st.mc_tree.children.size() != s_tl_n) {
        std::string dbg;
        for (auto& c : st.mc_tree.children)
            dbg += " '" + c.name + "'";
        LOG("Live", "Sub-layer tree: %zu top-level clip(s) on 0x%08x:%s",
            st.mc_tree.children.size(), stream_id, dbg.c_str());
        s_tl_sid = stream_id;
        s_tl_n = st.mc_tree.children.size();
    }
}

void RefreshMcNameList(AfpFuncs& afp, uint32_t stream_id, App::Status& st) {
    static uint32_t s_enum_sid = 0xFFFFFFFC;
    static int s_enum_wait = 0;
    if (stream_id == s_enum_sid && --s_enum_wait > 0) return;
    s_enum_sid = stream_id;
    s_enum_wait = 120;
    bool enum_ok = false;
    auto kids = AfpManager::EnumerateChildClips(afp, true, &enum_ok);
    if (!enum_ok) return;
    st.mc_children.clear();
    st.mc_children.reserve(kids.size());
    for (auto& k : kids) {
        st.mc_children.push_back(
            {.name = k.name, .x = k.screen_x, .y = k.screen_y, .have_pos = k.have_pos});
    }
}

void PublishStatusExtras(AfpFuncs& afp, uint32_t stream_id) {
    App::Status st = App::Global().GetStatus();
    if (!Runtime::Active().SupportsLiveExtras() || stream_id == 0xFFFFFFFC) {
        if (!st.mc_children.empty()) st.mc_children.clear();
        if (!st.mc_tree.children.empty()) st.mc_tree = App::SubLayerNode{};
    } else {
        RefreshSubLayerTree(afp, stream_id, st);
        if (App::Global().GetLiveOverrides().show_mc_names) {
            RefreshMcNameList(afp, stream_id, st);
        } else if (!st.mc_children.empty()) {
            st.mc_children.clear();
        }
    }
    App::Global().SetStatus(st);
}

}

void PublishLiveState(AfpFuncs& afp, uint32_t stream_id, int frames_since_switch, bool exporting) {
    const auto live_ov = App::Global().GetLiveOverrides();
    const uint32_t active_id = Runtime::Active().ActiveClipId(stream_id);

    if (!exporting) {
        if (Runtime::Active().SupportsLiveExtras()) ApplyFilter(stream_id, live_ov.filter_enabled);
        ApplyPauseDefend(afp, active_id, live_ov.paused);
    }

    App::State::LiveState lstate;
    FillLiveState(afp, stream_id, active_id, frames_since_switch, live_ov.filter_enabled, lstate);
    App::Global().SetLiveState(lstate);

    PublishStatusExtras(afp, stream_id);
}

}
