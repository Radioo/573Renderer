#include "state/app_state.h"

#include "settings/settings.h"
#include "state/boot_lifecycle.h"
#include "state/ifs_catalog.h"
#include "state/live_controls.h"
#include "state/telemetry.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace App {

State& Global() {
    static State s;
    return s;
}

bool SaveCurrentSettings() {
    const State& app = Global();
    Settings::Config c;
    c.game_dir = app.GameDir();
    c.loop_master = app.GetLoopMaster();
    c.root_loop_force = (app.GetRootLoopMode() == State::RootLoopMode::Force);
    int rw = 0;
    int rh = 0;
    app.GetRenderSize(rw, rh);
    if (rw > 0) c.render_width = rw;
    if (rh > 0) c.render_height = rh;
    c.render_fps = app.GetRenderFps();
    c.game_profile = app.GetGameProfileSlug();
    c.master_scale = app.GetMasterScale();
    return Settings::SaveAtomic(c);
}

std::optional<Request> State::TakeRequest() {
    return pending_.Take();
}

void State::PostRequest(Request r) {
    pending_.Post(std::move(r));
}

IfsConfig& State::MutConfig(const std::string& filename) {
    return catalog_.MutConfig(filename);
}

const IfsConfig* State::FindConfig(const std::string& filename) const {
    return catalog_.FindConfig(filename);
}

std::vector<std::pair<std::string, bool>>
State::GetSublayerOverrides(const std::string& filename) const {
    return catalog_.GetSublayerOverrides(filename);
}

void State::SetSublayerOverride(const std::string& filename, const std::string& clip_name,
                                bool visible) {
    catalog_.SetSublayerOverride(filename, clip_name, visible);
}

std::vector<std::string> State::GetSublayerExpanded() const {
    return catalog_.GetSublayerExpanded();
}

void State::SetSublayerExpanded(const std::string& path, bool expanded) {
    catalog_.SetSublayerExpanded(path, expanded);
}

std::vector<State::IfsEntry> State::ListAvailableIfs() const {
    return catalog_.ListAvailableIfs();
}

void State::SetAvailableIfs(std::vector<State::IfsEntry> v) {
    catalog_.SetAvailableIfs(std::move(v));
}

void State::SetIfsScanning(bool scanning) {
    catalog_.SetScanning(scanning);
}

bool State::IsIfsScanning() const {
    return catalog_.IsScanning();
}

void State::WaitForIfsScan() const {
    catalog_.WaitForScan();
}

void State::SetIfsScanStatus(std::string s) {
    catalog_.SetScanStatus(std::move(s));
}

std::string State::GetIfsScanStatus() const {
    return catalog_.GetScanStatus();
}

Status State::GetStatus() const {
    return telemetry_.GetStatus();
}

void State::SetStatus(Status s) {
    telemetry_.SetStatus(std::move(s));
}

std::string State::ActiveIfs() const {
    return catalog_.ActiveIfs();
}

void State::SetActiveIfs(std::string name) {
    catalog_.SetActiveIfs(std::move(name));
}

LoadProgress State::GetLoadProgress() const {
    return boot_.GetLoadProgress();
}

void State::SetLoadProgress(LoadProgress p) {
    boot_.SetLoadProgress(std::move(p));
}

void State::BeginLoad(std::string target) {
    boot_.BeginLoad(std::move(target));
}

void State::UpdateLoadStage(std::string stage, float fraction) {
    boot_.UpdateLoadStage(std::move(stage), fraction);
}

void State::EndLoad() {
    boot_.EndLoad();
}

void State::SetLoadDetail(std::string detail) {
    boot_.SetLoadDetail(std::move(detail));
}

void State::SetTexturesExpected(int n) {
    boot_.SetTexturesExpected(n);
}

void State::BumpTexturesLoaded() {
    boot_.BumpTexturesLoaded();
}

std::string State::GameDir() const {
    return boot_.GameDir();
}

void State::SetGameDir(std::string dir) {
    boot_.SetGameDir(std::move(dir));
}

void State::GetRenderSize(int& w, int& h) const {
    live_.GetRenderSize(w, h);
}

void State::SetRenderSize(int w, int h) {
    live_.SetRenderSize(w, h);
}

int State::GetRenderFps() const {
    return live_.GetRenderFps();
}

void State::SetRenderFps(int fps) {
    live_.SetRenderFps(fps);
}

std::string State::GetGameProfileSlug() const {
    return boot_.GetGameProfileSlug();
}

void State::SetGameProfileSlug(std::string slug) {
    boot_.SetGameProfileSlug(std::move(slug));
}

bool State::IsDdrMode() const {
    return boot_.IsDdrMode();
}

void State::SetIsDdrMode(bool on) {
    boot_.SetIsDdrMode(on);
}

BootState State::GetBootState() const {
    return boot_.GetBootState();
}

void State::SetBootState(BootState s) {
    boot_.SetBootState(s);
}

std::string State::GetBootError() const {
    return boot_.GetBootError();
}

void State::SetBootError(std::string msg) {
    boot_.SetBootError(std::move(msg));
}

bool State::GetLoopMaster() const {
    return live_.GetLoopMaster();
}

void State::SetLoopMaster(bool on) {
    live_.SetLoopMaster(on);
}

State::RootLoopMode State::GetRootLoopMode() const {
    return live_.GetRootLoopMode();
}

void State::SetRootLoopMode(RootLoopMode m) {
    live_.SetRootLoopMode(m);
}

float State::GetMasterScale() const {
    return live_.GetMasterScale();
}

void State::SetMasterScale(float s) {
    live_.SetMasterScale(s);
}

State::LiveOverrides State::GetLiveOverrides() const {
    return live_.GetLiveOverrides();
}

void State::SetLiveOverrides(LiveOverrides o) {
    live_.SetLiveOverrides(o);
}

State::LiveState State::GetLiveState() const {
    return live_.GetLiveState();
}

void State::SetLiveState(const LiveState& s) {
    live_.SetLiveState(s);
}

ExportState State::GetExport() const {
    return telemetry_.GetExport();
}

void State::SetExport(ExportState e) {
    telemetry_.SetExport(std::move(e));
}

CropRect State::GetCropRect() const {
    return live_.GetCropRect();
}

void State::SetCropRect(CropRect r) {
    live_.SetCropRect(r);
}

bool State::GetCropPickMode() const {
    return live_.GetCropPickMode();
}

void State::SetCropPickMode(bool on) {
    live_.SetCropPickMode(on);
}

}
