#include "state/telemetry.h"

#include "preset/preset_preview.h"

#include <mutex>
#include <utility>

namespace App {

Status Telemetry::GetStatus() const {
    const std::scoped_lock lk(mu_);
    return status_;
}

void Telemetry::SetStatus(Status s) {
    const std::scoped_lock lk(mu_);
    status_ = std::move(s);
}

ExportState Telemetry::GetExport() const {
    const std::scoped_lock lk(mu_);
    return export_;
}

void Telemetry::SetExport(ExportState e) {
    const std::scoped_lock lk(mu_);
    export_ = std::move(e);
}

PresetStatus Telemetry::GetPreset() const {
    const std::scoped_lock lk(mu_);
    return preset_;
}

void Telemetry::SetPreset(PresetStatus p) {
    const std::scoped_lock lk(mu_);
    preset_ = std::move(p);
}

Preset::Preview::SnapshotPtr Telemetry::GetPresetPreview() const {
    const std::scoped_lock lk(mu_);
    return preview_;
}

void Telemetry::SetPresetPreview(Preset::Preview::SnapshotPtr p) {
    const std::scoped_lock lk(mu_);
    preview_ = std::move(p);
}

void Telemetry::RequestPresetReport() {
    const std::scoped_lock lk(mu_);
    report_ttl_ = kPresetReportTtlFrames;
}

bool Telemetry::TakePresetReportRequest() {
    const std::scoped_lock lk(mu_);
    if (report_ttl_ <= 0) return false;
    report_ttl_--;
    return true;
}

}
