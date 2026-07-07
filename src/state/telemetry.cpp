#include "state/telemetry.h"

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

}
