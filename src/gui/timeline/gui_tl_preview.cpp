#include "gui_tl_preview.h"

#include "gui/gui_window.h"

#include <d3d9.h>

#include "imgui.h"
#include "preset/preset_preview.h"
#include "state/app_state.h"
#include "state/preset_commands.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Panels::Timeline {

namespace {

constexpr float kCellW = 96.0F;
constexpr float kCellH = 72.0F;

struct Uploaded {
    IDirect3DTexture9* texture = nullptr;
};

std::string g_requested;
std::string g_uploaded_key;
unsigned g_uploaded_version = 0;
std::vector<Uploaded> g_uploaded;

void ReleaseUploads() {
    for (Uploaded& item : g_uploaded) {
        if (item.texture != nullptr) item.texture->Release();
    }
    g_uploaded.clear();
}

IDirect3DTexture9* Upload(IDirect3DDevice9* device, const Preset::Preview::Sample& sample) {
    IDirect3DTexture9* texture = nullptr;
    if (FAILED(device->CreateTexture((UINT)sample.width, (UINT)sample.height, 1, 0, D3DFMT_A8R8G8B8,
                                     D3DPOOL_MANAGED, &texture, nullptr))) {
        return nullptr;
    }
    D3DLOCKED_RECT locked = {};
    if (FAILED(texture->LockRect(0, &locked, nullptr, 0))) {
        texture->Release();
        return nullptr;
    }
    for (int y = 0; y < sample.height; y++) {
        auto* row = (uint8_t*)locked.pBits + ((std::size_t)y * (std::size_t)locked.Pitch);
        const uint8_t* from = sample.bgra.data() + ((std::size_t)y * (std::size_t)sample.width * 4);
        std::copy_n(from, (std::size_t)sample.width * 4, row);
    }
    texture->UnlockRect(0);
    return texture;
}

void Sync(const Preset::Preview::Snapshot& snapshot) {
    if (snapshot.key == g_uploaded_key && snapshot.version == g_uploaded_version) return;
    IDirect3DDevice9* device = Gui::GetDevice();
    ReleaseUploads();
    g_uploaded_key = snapshot.key;
    g_uploaded_version = snapshot.version;
    if (device == nullptr) return;
    for (const Preset::Preview::Sample& sample : snapshot.samples) {
        if (sample.width <= 0 || sample.height <= 0) continue;
        g_uploaded.push_back(Uploaded{.texture = Upload(device, sample)});
    }
}

}

void RequestPreview(const std::string& asset, const std::string& animation,
                    const std::vector<std::string>& hidden_parts) {
    const std::string key = Preset::Preview::KeyFor(asset, animation, hidden_parts);
    if (key == g_requested) return;
    g_requested = key;
    App::Global().PostCommand(PresetCmd::Wrap(PresetCmd::PreviewLayer{
        .asset = asset, .animation = animation, .hidden_parts = hidden_parts}));
}

void ForgetPreview() {
    g_requested.clear();
    g_uploaded_key.clear();
    g_uploaded_version = 0;
    ReleaseUploads();
}

void DrawPreviewStrip(const std::string& key) {
    const Preset::Preview::SnapshotPtr snapshot = App::Global().GetPresetPreview();
    const bool mine = snapshot != nullptr && snapshot->key == key;
    if (mine) Sync(*snapshot);
    const int total = mine ? snapshot->total : 6;
    const int ready = mine ? (int)g_uploaded.size() : 0;

    ImGui::TextDisabled("Preview: %d / %d sample(s) rendered on the render thread", ready, total);
    for (int i = 0; i < total; i++) {
        if (i > 0) ImGui::SameLine();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(("###tl_preview_" + std::to_string(i)).c_str(),
                               ImVec2(kCellW, kCellH));
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 corner(origin.x + kCellW, origin.y + kCellH);
        draw->AddRect(origin, corner, ImGui::GetColorU32(ImGuiCol_Border));
        if (i < ready && g_uploaded[(std::size_t)i].texture != nullptr) {
            draw->AddImage((ImTextureID)(uintptr_t)g_uploaded[(std::size_t)i].texture,
                           ImVec2(origin.x + 1.0F, origin.y + 1.0F),
                           ImVec2(corner.x - 1.0F, corner.y - 1.0F));
        } else {
            const std::string label =
                "sample " + std::to_string(i + 1) + " / " + std::to_string(total);
            draw->AddText(ImVec2(origin.x + 6.0F, origin.y + (kCellH * 0.5F) - 6.0F),
                          ImGui::GetColorU32(ImGuiCol_TextDisabled), label.c_str());
        }
    }
}

}
