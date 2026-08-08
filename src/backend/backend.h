#pragma once

#include <any>
#include <string>

namespace Cli {
struct Options;
}
namespace Export {
class ICaptureDriver;
}
namespace GameProfile {
struct Profile;
}
namespace Loop {
struct AutopilotInputs;
}

namespace Backend {

struct BootEnv {
    std::string game_dir;
    const GameProfile::Profile* profile = nullptr;
    const Cli::Options* cli = nullptr;
    bool load_boot_content = true;
};

class IBackend {
public:
    IBackend() = default;
    virtual ~IBackend() = default;
    IBackend(const IBackend&) = delete;
    IBackend& operator=(const IBackend&) = delete;
    IBackend(IBackend&&) = delete;
    IBackend& operator=(IBackend&&) = delete;

    [[nodiscard]] virtual const char* Id() const = 0;

    virtual bool Boot(const BootEnv& env) = 0;

    virtual void Shutdown() = 0;

    [[nodiscard]] virtual bool ContentReady() const = 0;

    virtual void StartContentScan() = 0;

    virtual bool LoadContent(const std::string& path, bool from_arc) = 0;

    virtual void UnloadContent() = 0;

    virtual void AdvanceFrame(float dt, int frame_count, bool exporting) = 0;

    virtual void RenderScene(float dt, int frame_count) = 0;

    virtual void FillAutopilotInputs(Loop::AutopilotInputs& in) = 0;

    virtual void BindSubmonitor() = 0;

    virtual bool HandleCommand(const std::any& payload) = 0;

    virtual Export::ICaptureDriver& ExportDriver() = 0;
};

IBackend* Active();

bool CreateActive(const GameProfile::Profile& profile);

}
