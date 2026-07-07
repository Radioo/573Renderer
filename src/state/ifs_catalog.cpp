#include "state/ifs_catalog.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace App {

IfsConfig& IfsCatalog::MutConfig(const std::string& filename) {
    const std::scoped_lock lk(mu_);
    for (IfsConfig& c : configs_) {
        if (c.filename == filename) return c;
    }
    configs_.emplace_back();
    configs_.back().filename = filename;
    return configs_.back();
}

const IfsConfig* IfsCatalog::FindConfig(const std::string& filename) const {
    const std::scoped_lock lk(mu_);
    for (const IfsConfig& c : configs_) {
        if (c.filename == filename) return &c;
    }
    return nullptr;
}

std::vector<std::pair<std::string, bool>>
IfsCatalog::GetSublayerOverrides(const std::string& filename) const {
    const std::scoped_lock lk(mu_);
    for (const IfsConfig& c : configs_) {
        if (c.filename == filename) return c.sublayer_overrides;
    }
    return {};
}

void IfsCatalog::SetSublayerOverride(const std::string& filename, const std::string& clip_name,
                                     bool visible) {
    const std::scoped_lock lk(mu_);
    IfsConfig* cfg = nullptr;
    for (IfsConfig& c : configs_) {
        if (c.filename == filename) {
            cfg = &c;
            break;
        }
    }
    if (cfg == nullptr) {
        configs_.emplace_back();
        configs_.back().filename = filename;
        cfg = &configs_.back();
    }
    for (auto& ov : cfg->sublayer_overrides) {
        if (ov.first == clip_name) {
            ov.second = visible;
            return;
        }
    }
    cfg->sublayer_overrides.emplace_back(clip_name, visible);
}

std::vector<std::string> IfsCatalog::GetSublayerExpanded() const {
    const std::scoped_lock lk(mu_);
    return sublayer_expanded_;
}

void IfsCatalog::SetSublayerExpanded(const std::string& path, bool expanded) {
    const std::scoped_lock lk(mu_);
    if (expanded) {
        if (std::ranges::find(sublayer_expanded_, path) == sublayer_expanded_.end()) {
            sublayer_expanded_.push_back(path);
        }
    } else {
        std::erase(sublayer_expanded_, path);
    }
}

std::vector<IfsCatalog::IfsEntry> IfsCatalog::ListAvailableIfs() const {
    const std::scoped_lock lk(mu_);
    return available_ifs_;
}

void IfsCatalog::SetAvailableIfs(std::vector<IfsEntry> v) {
    const std::scoped_lock lk(mu_);
    available_ifs_ = std::move(v);
}

void IfsCatalog::SetScanning(bool scanning) {
    scanning_.store(scanning, std::memory_order_release);
}

bool IfsCatalog::IsScanning() const {
    return scanning_.load(std::memory_order_acquire);
}

void IfsCatalog::WaitForScan() const {
    while (scanning_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void IfsCatalog::SetScanStatus(std::string s) {
    const std::scoped_lock lk(mu_);
    scan_status_ = std::move(s);
}

std::string IfsCatalog::GetScanStatus() const {
    const std::scoped_lock lk(mu_);
    return scan_status_;
}

std::string IfsCatalog::ActiveIfs() const {
    const std::scoped_lock lk(mu_);
    return active_ifs_;
}

void IfsCatalog::SetActiveIfs(std::string name) {
    const std::scoped_lock lk(mu_);
    active_ifs_ = std::move(name);
}

}
