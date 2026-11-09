#include "engine_dlls.h"

#include "avs_funcs.h"
#include "backend/afp_profiles.h"
#include "engine_session.h"
#include "support/log.h"

#include <windows.h>

#include <filesystem>
#include <string>
#include <system_error>

namespace EngineDlls {

std::string DiscoverDllDir(const std::string& game_dir, const AfpProfiles::AfpConfig& p) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (game_dir.empty()) return {};

    fs::path const root = fs::path(game_dir);
    const fs::path candidates[] = {
        root / "modules",
        root / "contents" / "modules",
        root,
    };
    const char* required[] = {p.avs_dll, p.afp_dll, p.afpu_dll};
    for (const auto& c : candidates) {
        bool all_present = true;
        for (const char* name : required) {
            if (name == nullptr) continue;
            if (!fs::exists(c / name, ec)) {
                all_present = false;
                break;
            }
        }
        if (all_present) {
            return c.string() + "\\";
        }
    }
    return {};
}

bool Load(EngineSession& es, const std::string& dll_dir, const AfpProfiles::AfpConfig& p,
          bool legacy_afp) {
    LOG("Init", "Loading DLLs from: %s (avs=%s afp=%s afpu=%s)", dll_dir.c_str(), p.avs_dll,
        p.afp_dll, (p.afpu_dll != nullptr) ? p.afpu_dll : "(none)");
    {
        std::string d = dll_dir;
        if (!d.empty() && (d.back() == '\\' || d.back() == '/')) d.pop_back();
        SetDllDirectoryA(d.c_str());
    }
    if (!es.avs_dll.Load((dll_dir + p.avs_dll).c_str())) return false;
    if (!es.afp_dll.Load((dll_dir + p.afp_dll).c_str())) return false;
    if (p.afpu_dll == nullptr) {
        LOG("Init", "Profile ships no afp-utils DLL (pre-afputils AFP generation)");
    } else if (!es.afpu_dll.Load((dll_dir + p.afpu_dll).c_str())) {
        return false;
    }
    const AvsOrdinals* avs_ord = &kAvsOrdinals217;
    const char* avs_ord_name = "avs 2.16.3/2.17";
    if (p.avs_generation == AfpProfiles::AvsGeneration::Avs2161) {
        avs_ord = &kAvsOrdinals2161;
        avs_ord_name = "avs 2.16.1";
    } else if (p.avs_generation == AfpProfiles::AvsGeneration::Avs2158) {
        avs_ord = &kAvsOrdinals2158;
        avs_ord_name = "avs 2.15.8";
    } else if (p.avs_generation == AfpProfiles::AvsGeneration::Avs2134) {
        avs_ord = &kAvsOrdinals2134;
        avs_ord_name = "avs 2.13.4";
    }
    LOG("Init", "AVS ordinal map: %s", avs_ord_name);
    if (!es.avs.Load(es.avs_dll, *avs_ord)) {
        LOG("Init", "FAILED to resolve AVS functions");
        return false;
    }
    if (legacy_afp) {
        LOG("Init", "Legacy AFP 2.13.7 (DDR) profile: DLLs loaded; afp/afpu "
                    "func resolve deferred to DdrAfp::Boot");
        return true;
    }
    if (!es.afp.Load(es.afp_dll)) {
        LOG("Init", "FAILED to resolve AFP functions");
        return false;
    }
    if (!es.afpu.Load(es.afpu_dll)) {
        LOG("Init", "FAILED to resolve AFPU functions");
        return false;
    }
    return true;
}

}
