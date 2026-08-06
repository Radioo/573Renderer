#include "backend/afp_family_backend.h"

#include "afp_boot.h"
#include "app_globals.h"
#include "backend/backend.h"
#include "game_runtime.h"
#include "state/app_state.h"

#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

namespace Backend {

namespace {

class AfpModernBackend final : public AfpFamilyBackend {
public:
    AfpModernBackend() { Runtime::SelectRuntime(false); }

    [[nodiscard]] const char* Id() const override { return "afp_modern"; }

protected:
    bool BootEngine(const BootEnv& env) override {
        App::Global().UpdateLoadStage("Booting AFP");
        AfpManager::Boot(g_engine, g_d3d);
        LoadPersistentIfses(env);
        return true;
    }

private:
    static void LoadPersistentIfses(const BootEnv& env) {
        if (!env.load_boot_content || !AfpManager::IsBooted()) return;
        App::Global().UpdateLoadStage("Loading persistent IFSes");
        std::filesystem::path data_root = std::filesystem::path(env.game_dir) / "data";
        std::error_code ec;
        if (!std::filesystem::exists(data_root, ec))
            data_root = std::filesystem::path(env.game_dir);
        AfpManager::LoadBootIfses(g_engine, data_root.string());
    }
};

}

std::unique_ptr<IBackend> MakeAfpModernBackend() {
    return std::make_unique<AfpModernBackend>();
}

}
