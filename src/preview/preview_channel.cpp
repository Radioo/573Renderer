#include "preview/preview_channel.h"

#include "support/expected.h"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace PreviewChannel {

namespace {

constexpr std::size_t kMaxMessageBytes = std::size_t{256} * 1024 * 1024;
constexpr std::size_t kLengthPrefixBytes = 4;
constexpr DWORD kPipeBufferBytes = 1U << 20U;
constexpr DWORD kMaxChunkBytes = 1U << 24U;
constexpr DWORD kConnectRetryMs = 10;

std::wstring Wide(const std::string& text) {
    const int length =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(),
                        length);
    return wide;
}

bool IsClosedError(DWORD error) {
    return error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA ||
           error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_OPERATION_ABORTED;
}

class Overlapped {
public:
    Overlapped() { io_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
    Overlapped(const Overlapped&) = delete;
    Overlapped& operator=(const Overlapped&) = delete;
    Overlapped(Overlapped&&) = delete;
    Overlapped& operator=(Overlapped&&) = delete;
    ~Overlapped() {
        if (io_.hEvent != nullptr) CloseHandle(io_.hEvent);
    }

    [[nodiscard]] OVERLAPPED* Io() { return &io_; }
    [[nodiscard]] HANDLE Event() const { return io_.hEvent; }

private:
    OVERLAPPED io_ = {};
};

enum class Direction : uint8_t { Read, Write };

Support::Expected<DWORD, std::string> TransferChunk(HANDLE pipe, Direction direction, uint8_t* data,
                                                    DWORD size, unsigned timeout_ms) {
    Overlapped overlapped;
    const BOOL started = direction == Direction::Write
                             ? WriteFile(pipe, data, size, nullptr, overlapped.Io())
                             : ReadFile(pipe, data, size, nullptr, overlapped.Io());
    if (started == FALSE) {
        const DWORD error = GetLastError();
        if (IsClosedError(error)) return Support::Unexpected(std::string("the pipe was closed"));
        if (error != ERROR_IO_PENDING)
            return Support::Unexpected(std::format("pipe I/O failed (error {})", error));
        if (WaitForSingleObject(overlapped.Event(), timeout_ms) == WAIT_TIMEOUT) {
            CancelIoEx(pipe, overlapped.Io());
            DWORD ignored = 0;
            GetOverlappedResult(pipe, overlapped.Io(), &ignored, TRUE);
            return Support::Unexpected(std::format("pipe I/O timed out after {} ms", timeout_ms));
        }
    }
    DWORD transferred = 0;
    if (GetOverlappedResult(pipe, overlapped.Io(), &transferred, FALSE) == FALSE) {
        const DWORD error = GetLastError();
        if (IsClosedError(error)) return Support::Unexpected(std::string("the pipe was closed"));
        return Support::Unexpected(std::format("pipe I/O failed (error {})", error));
    }
    if (transferred == 0) return Support::Unexpected(std::string("the pipe was closed"));
    return transferred;
}

Support::Expected<void, std::string> Transfer(HANDLE pipe, Direction direction,
                                              std::span<uint8_t> bytes, unsigned timeout_ms) {
    std::size_t done = 0;
    while (done < bytes.size()) {
        const auto chunk =
            static_cast<DWORD>(std::min<std::size_t>(bytes.size() - done, kMaxChunkBytes));
        auto moved = TransferChunk(pipe, direction, bytes.data() + done, chunk, timeout_ms);
        if (!moved) return Support::Unexpected(moved.error());
        done += *moved;
    }
    return {};
}

}

Pipe& Pipe::operator=(Pipe&& other) noexcept {
    if (this != &other) {
        Close();
        handle_ = std::exchange(other.handle_, INVALID_HANDLE_VALUE);
    }
    return *this;
}

void Pipe::Close() {
    if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
    handle_ = INVALID_HANDLE_VALUE;
}

Support::Expected<Pipe, std::string> Listen(const std::string& name) {
    HANDLE handle = CreateNamedPipeW(
        Wide(name).c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
        kPipeBufferBytes, kPipeBufferBytes, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return Support::Unexpected(
            std::format("cannot create pipe {} (error {})", name, GetLastError()));
    }
    return Pipe(handle);
}

Support::Expected<void, std::string> Accept(const Pipe& server, unsigned timeout_ms) {
    Overlapped overlapped;
    if (ConnectNamedPipe(server.Get(), overlapped.Io()) != FALSE) return {};
    const DWORD error = GetLastError();
    if (error == ERROR_PIPE_CONNECTED) return {};
    if (error != ERROR_IO_PENDING)
        return Support::Unexpected(std::format("waiting for a client failed (error {})", error));
    if (WaitForSingleObject(overlapped.Event(), timeout_ms) == WAIT_TIMEOUT) {
        CancelIoEx(server.Get(), overlapped.Io());
        return Support::Unexpected(std::format("no client connected within {} ms", timeout_ms));
    }
    DWORD ignored = 0;
    if (GetOverlappedResult(server.Get(), overlapped.Io(), &ignored, FALSE) == FALSE) {
        return Support::Unexpected(
            std::format("waiting for a client failed (error {})", GetLastError()));
    }
    return {};
}

Support::Expected<Pipe, std::string> Connect(const std::string& name, unsigned timeout_ms) {
    const std::wstring wide = Wide(name);
    const ULONGLONG deadline = GetTickCount64() + timeout_ms;
    while (true) {
        HANDLE handle = CreateFileW(wide.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                    OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (handle != INVALID_HANDLE_VALUE) return Pipe(handle);
        const DWORD error = GetLastError();
        if (error != ERROR_PIPE_BUSY && error != ERROR_FILE_NOT_FOUND)
            return Support::Unexpected(std::format("cannot open pipe {} (error {})", name, error));
        if (GetTickCount64() >= deadline) {
            return Support::Unexpected(
                std::format("pipe {} did not accept within {} ms", name, timeout_ms));
        }
        if (error == ERROR_PIPE_BUSY) {
            WaitNamedPipeW(wide.c_str(), kConnectRetryMs);
        } else {
            Sleep(kConnectRetryMs);
        }
    }
}

Support::Expected<void, std::string> Send(const Pipe& pipe, std::span<const uint8_t> message) {
    if (message.size() > kMaxMessageBytes)
        return Support::Unexpected(std::format("message of {} bytes is too large", message.size()));
    std::vector<uint8_t> framed(kLengthPrefixBytes + message.size());
    const auto length = static_cast<uint32_t>(message.size());
    for (std::size_t i = 0; i < kLengthPrefixBytes; i++)
        framed[i] = static_cast<uint8_t>((length >> (8U * i)) & 0xFFU);
    std::ranges::copy(message, framed.begin() + kLengthPrefixBytes);
    return Transfer(pipe.Get(), Direction::Write, framed, INFINITE);
}

Support::Expected<std::vector<uint8_t>, std::string> Receive(const Pipe& pipe,
                                                             unsigned timeout_ms) {
    std::vector<uint8_t> prefix(kLengthPrefixBytes);
    auto header = Transfer(pipe.Get(), Direction::Read, prefix, timeout_ms);
    if (!header) return Support::Unexpected(header.error());
    std::size_t length = 0;
    for (std::size_t i = 0; i < kLengthPrefixBytes; i++)
        length |= static_cast<std::size_t>(prefix[i]) << (8U * i);
    if (length > kMaxMessageBytes)
        return Support::Unexpected(std::format("message of {} bytes is too large", length));
    std::vector<uint8_t> message(length);
    auto body = Transfer(pipe.Get(), Direction::Read, message, timeout_ms);
    if (!body) return Support::Unexpected(body.error());
    return message;
}

Support::Expected<std::vector<uint8_t>, std::string> Client::Call(std::span<const uint8_t> request,
                                                                  const std::string& request_name) {
    last_request_ = request_name;
    auto sent = Send(pipe_, request);
    if (!sent) {
        return Support::Unexpected(
            std::format("sending {} to the host failed: {}", request_name, sent.error()));
    }
    auto reply = Receive(pipe_, timeout_ms_);
    if (!reply) {
        return Support::Unexpected(
            std::format("the host did not answer {}: {}", request_name, reply.error()));
    }
    return reply;
}

}
