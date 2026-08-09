#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace Support {

inline std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

inline std::string StripTrailingSlashes(std::string s) {
    while (!s.empty() && (s.back() == '/' || s.back() == '\\'))
        s.pop_back();
    return s;
}

template <typename StatusT> class FolderJob {
public:
    template <typename RunFn> bool Start(const std::string& folder, RunFn run) {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true)) return false;
        std::thread(std::move(run), folder).detach();
        return true;
    }

    void Finish() { running_ = false; }

    [[nodiscard]] bool IsRunning() const { return running_.load(); }

    void Publish(const StatusT& s) {
        const std::scoped_lock lk(mu_);
        status_ = s;
    }

    [[nodiscard]] StatusT Get() const {
        const std::scoped_lock lk(mu_);
        return status_;
    }

private:
    mutable std::mutex mu_;
    StatusT status_;
    std::atomic<bool> running_{false};
};

inline constexpr long long kScanProgressEvery = 512;

template <typename Progress, typename Entry>
long long ScanFolder(const std::filesystem::path& root, Progress&& progress, Entry&& entry) {
    long long scanned = 0;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator
             it(root, std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if ((++scanned % kScanProgressEvery) == 0) progress(it->path());
        entry(it);
    }
    return scanned;
}

}
