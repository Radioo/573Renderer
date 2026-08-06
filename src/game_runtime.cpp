#include "game_runtime.h"

#include "game_runtime_internal.h"

namespace Runtime {

namespace {

ModernRuntime g_modern;
DdrRuntime g_ddr;
IGameRuntime* g_active = &g_modern;

}

void SelectRuntime(bool legacy_ddr) {
    g_active =
        legacy_ddr ? static_cast<IGameRuntime*>(&g_ddr) : static_cast<IGameRuntime*>(&g_modern);
}

IGameRuntime& Active() {
    return *g_active;
}

}
