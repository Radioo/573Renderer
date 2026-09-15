#pragma once

#include "support/expected.h"

#include <windows.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace PreviewChannel {

class Pipe {
public:
    Pipe() = default;
    explicit Pipe(HANDLE handle) : handle_(handle) {}
    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;
    Pipe(Pipe&& other) noexcept : handle_(other.handle_) { other.handle_ = INVALID_HANDLE_VALUE; }
    Pipe& operator=(Pipe&& other) noexcept;
    ~Pipe() { Close(); }

    [[nodiscard]] HANDLE Get() const { return handle_; }
    void Close();

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

[[nodiscard]] Support::Expected<Pipe, std::string> Listen(const std::string& name);

[[nodiscard]] Support::Expected<void, std::string> Accept(const Pipe& server, unsigned timeout_ms);

[[nodiscard]] Support::Expected<Pipe, std::string> Connect(const std::string& name,
                                                           unsigned timeout_ms);

[[nodiscard]] Support::Expected<void, std::string> Send(const Pipe& pipe,
                                                        std::span<const uint8_t> message);

[[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string> Receive(const Pipe& pipe,
                                                                           unsigned timeout_ms);

class Client {
public:
    Client(Pipe pipe, unsigned timeout_ms) : pipe_(std::move(pipe)), timeout_ms_(timeout_ms) {}

    [[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string>
    Call(std::span<const uint8_t> request, const std::string& request_name);

    [[nodiscard]] const std::string& LastRequest() const { return last_request_; }

private:
    Pipe pipe_;
    unsigned timeout_ms_;
    std::string last_request_;
};

}
