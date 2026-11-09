#pragma once

#include "backend/afp_profiles.h"
#include "engine_session.h"

#include <string>

namespace EngineDlls {

[[nodiscard]] std::string DiscoverDllDir(const std::string& game_dir,
                                         const AfpProfiles::AfpConfig& p);

[[nodiscard]] bool Load(EngineSession& es, const std::string& dll_dir,
                        const AfpProfiles::AfpConfig& p, bool legacy_afp);

}
