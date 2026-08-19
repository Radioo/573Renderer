#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Movie {

struct Frame {
    int width = 0;
    int height = 0;
    const std::uint8_t* bgra = nullptr;
};

using StepFn = std::function<void(int decoded, int wanted)>;

class Source {
public:
    Source();
    ~Source();
    Source(const Source&) = delete;
    Source& operator=(const Source&) = delete;
    Source(Source&&) noexcept;
    Source& operator=(Source&&) noexcept;

    bool Open(const std::string& path);

    void Close();

    [[nodiscard]] bool IsOpen() const;

    [[nodiscard]] const std::string& LastError() const { return err_; }

    [[nodiscard]] double FrameRate() const;

    [[nodiscard]] int IndexAt(double seconds) const;

    [[nodiscard]] int Position() const;

    [[nodiscard]] bool Broken() const;

    void SetOutputSize(int width, int height);

    Frame FrameAt(int index, const StepFn& on_step = {});

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string err_;
};

}
