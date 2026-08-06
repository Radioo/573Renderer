#include "backend/afp_family_backend.h"

#include "afp_ddr.h"
#include "app_globals.h"
#include "backend/afp_profiles.h"
#include "backend/backend.h"
#include "game_runtime.h"
#include "state/app_state.h"
#include "state/boot_lifecycle.h"

#include <memory>

namespace Backend {

namespace {

class AfpDdrBackend final : public AfpFamilyBackend {
public:
    AfpDdrBackend() { Runtime::SelectRuntime(true); }

    [[nodiscard]] const char* Id() const override { return "afp_ddr"; }

protected:
    bool BootEngine([[maybe_unused]] const BootEnv& env) override {
        App::Global().UpdateLoadStage("Booting AFP");
        if (!DdrAfp::Boot(g_afp_dll, g_afpu_dll, g_d3d)) {
            auto& state = App::Global();
            state.EndLoad();
            state.SetBootError("DDR AFP 2.13.7 boot failed "
                               "(libafp-win64 / libafputils-win64).");
            state.SetBootState(App::BootState::Failed);
            return false;
        }
        DdrAfp::SetTimeScale(cfg_->time_scale);
        return true;
    }
};

}

std::unique_ptr<IBackend> MakeAfpDdrBackend() {
    return std::make_unique<AfpDdrBackend>();
}

}
