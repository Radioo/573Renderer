#include "backend/backend.h"

#include "backend/afp_family_backend.h"
#include "backend/scene3d_backend.h"
#include "game_profile.h"

#include <memory>
#include <string_view>

namespace Backend {

namespace {

struct Entry {
    const char* id;
    std::unique_ptr<IBackend> (*make)();
};

constexpr Entry kBackends[] = {
    {.id = "afp_modern", .make = &MakeAfpModernBackend},
    {.id = "afp_ddr", .make = &MakeAfpDdrBackend},
    {.id = "scene3d", .make = &MakeScene3dBackend},
};

std::unique_ptr<IBackend> g_active;

}

IBackend* Active() {
    return g_active.get();
}

bool CreateActive(const GameProfile::Profile& profile) {
    g_active.reset();
    for (const auto& e : kBackends) {
        if (std::string_view(profile.backend_id) == e.id) {
            g_active = e.make();
            break;
        }
    }
    return g_active != nullptr;
}

}
