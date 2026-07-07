#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace App {

struct VariantSlot {
    std::string path;
    std::string bitmap;
    std::string default_bitmap;
    bool visible = true;
    bool bitmap_override = false;
    bool is_valid = false;
};

struct CompanionIfs {
    std::string path;
    std::string suffix;
    std::string display_name;
    uint32_t pkg_id = 0;
    bool loaded = false;
};

struct IfsConfig {
    std::string filename;
    std::vector<std::string> bitmap_names;
    std::vector<std::string> anim_names;
    std::vector<VariantSlot> slots;
    std::vector<CompanionIfs> companions;
    std::vector<std::pair<std::string, bool>> sublayer_overrides;
};

class IfsCatalog {
public:
    struct IfsEntry {
        std::string name;
        std::string full_path;
        bool from_arc = false;
    };

    IfsConfig& MutConfig(const std::string& filename);
    [[nodiscard]] const IfsConfig* FindConfig(const std::string& filename) const;

    [[nodiscard]] std::vector<std::pair<std::string, bool>>
    GetSublayerOverrides(const std::string& filename) const;
    void SetSublayerOverride(const std::string& filename, const std::string& clip_name,
                             bool visible);

    [[nodiscard]] std::vector<std::string> GetSublayerExpanded() const;
    void SetSublayerExpanded(const std::string& path, bool expanded);

    [[nodiscard]] std::vector<IfsEntry> ListAvailableIfs() const;
    void SetAvailableIfs(std::vector<IfsEntry> v);

    void SetScanning(bool scanning);
    [[nodiscard]] bool IsScanning() const;
    void WaitForScan() const;
    void SetScanStatus(std::string s);
    [[nodiscard]] std::string GetScanStatus() const;

    [[nodiscard]] std::string ActiveIfs() const;
    void SetActiveIfs(std::string name);

private:
    mutable std::mutex mu_;
    std::vector<IfsConfig> configs_;
    std::vector<std::string> sublayer_expanded_;
    std::vector<IfsEntry> available_ifs_;
    std::atomic<bool> scanning_{false};
    std::string scan_status_;
    std::string active_ifs_;
};

}
