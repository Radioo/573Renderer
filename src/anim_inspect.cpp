#include "anim_inspect.h"

#include "afp_boot.h"
#include "app_globals.h"
#include "game_runtime.h"
#include "render_live.h"
#include "render_seh.h"
#include "state/app_state.h"
#include "state/ifs_catalog.h"
#include "support/log.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

namespace AnimInspect {

namespace {

struct AnimInfo {
    std::string name;
    uint32_t total_frames = 0;
    bool ok = false;
    std::vector<RenderLive::Inspect::Label> labels;
};

std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char const ch : s) {
        switch (ch) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x",
                         static_cast<unsigned>(static_cast<unsigned char>(ch)));
                out += buf;
            } else {
                out += ch;
            }
        }
    }
    return out;
}

void TickAfp(int frames) {
    if (g_afp.afp_do_update == nullptr) return;
    for (int i = 0; i < frames; ++i) {
        const RenderSeh::FaultReport r =
            RenderSeh::SafeCallUpdate(g_afp.afp_do_update, 1.0F / 120.0F);
        if (r.faulted) {
            LOG("AnimInspect", "afp_do_update faulted (0x%08lx) during inspect tick",
                static_cast<unsigned long>(r.code));
            return;
        }
    }
}

AnimInfo InspectOne(const std::string& name) {
    AnimInfo info;
    info.name = name;
    Runtime::Active().SwitchAnimation(name, "");
    if (Runtime::Active().ActiveClipName() != name) {
        LOG("AnimInspect", "  '%s': switch failed, skipping", name.c_str());
        return info;
    }
    TickAfp(2);
    const uint32_t sid = Runtime::Active().ActiveClipId(AfpManager::StreamId());
    uint32_t cur = 0;
    uint32_t total = 0;
    if (!Runtime::Active().ReadPlayhead(sid, &cur, &total, nullptr)) {
        LOG("AnimInspect", "  '%s': playhead unreadable, skipping", name.c_str());
        return info;
    }
    info.total_frames = total;
    info.labels = RenderLive::Inspect::EnumerateLabels(g_afp, sid);
    info.ok = true;
    return info;
}

bool WriteJson(const std::string& out_path, const std::string& ifs_name,
               const std::vector<AnimInfo>& infos) {
    std::string j;
    j += "{\n  \"ifs\": \"" + JsonEscape(ifs_name) + "\",\n  \"anims\": [\n";
    for (size_t i = 0; i < infos.size(); ++i) {
        const AnimInfo& a = infos[i];
        j += R"(    {"name": ")" + JsonEscape(a.name) + R"(", "ok": )" + (a.ok ? "true" : "false");
        if (a.ok) {
            j += R"(, "total_frames": )" + std::to_string(a.total_frames) + R"(, "labels": [)";
            for (size_t k = 0; k < a.labels.size(); ++k) {
                j += R"({"name": ")" + JsonEscape(a.labels[k].name) + R"(", "frame": )" +
                     std::to_string(a.labels[k].frame) + "}";
                if (k + 1 < a.labels.size()) j += ", ";
            }
            j += "]";
        }
        j += "}";
        if (i + 1 < infos.size()) j += ",";
        j += "\n";
    }
    j += "  ]\n}\n";

    std::ofstream f(out_path, std::ios::binary | std::ios::trunc);
    if (!f) {
        LOG("AnimInspect", "cannot write '%s'", out_path.c_str());
        return false;
    }
    f.write(j.data(), static_cast<std::streamsize>(j.size()));
    return true;
}

}

int Run(const std::string& out_path) {
    auto& state = App::Global();
    const std::string active = state.ActiveIfs();
    if (active.empty()) {
        LOG("AnimInspect", "no IFS loaded - pass --ifs <path> together with --dump-anim-info");
        return 2;
    }
    const App::IfsConfig* cfg = state.FindConfig(active);
    if (cfg == nullptr || cfg->anim_names.empty()) {
        LOG("AnimInspect", "IFS '%s' lists no animations", active.c_str());
        return 2;
    }
    const std::vector<std::string> names = cfg->anim_names;

    std::vector<AnimInfo> infos;
    infos.reserve(names.size());
    for (size_t i = 0; i < names.size(); ++i) {
        LOG("AnimInspect", "[%zu/%zu] inspecting '%s'", i + 1, names.size(), names[i].c_str());
        infos.push_back(InspectOne(names[i]));
    }

    if (!WriteJson(out_path, active, infos)) return 2;
    size_t ok_count = 0;
    for (const AnimInfo& a : infos)
        if (a.ok) ok_count++;
    LOG("AnimInspect", "wrote %zu/%zu animation(s) -> '%s'", ok_count, infos.size(),
        out_path.c_str());
    return 0;
}

}
