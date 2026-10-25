#include "shared_frame.h"

#include "support/com_ptr.h"
#include "support/expected.h"

#include <windows.h>

#include <d3d9.h>

#include <format>
#include <string>
#include <utility>

namespace SharedFrame {

Support::Expected<Target, std::string> Create(IDirect3DDevice9* device, int width, int height) {
    ComPtr<IDirect3DDevice9Ex> device_ex;
    if (FAILED(device->QueryInterface(__uuidof(IDirect3DDevice9Ex),
                                      reinterpret_cast<void**>(&device_ex))))
        return Support::Unexpected(std::string("shared frames need a D3D9Ex device"));
    Target target{
        .texture = {}, .sync_probe = {}, .handle = nullptr, .width = width, .height = height};
    const HRESULT hr =
        device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                              D3DPOOL_DEFAULT, &target.texture, &target.handle);
    if (FAILED(hr)) {
        return Support::Unexpected(std::format("shared texture {}x{} failed (hr=0x{:08x})", width,
                                               height, static_cast<unsigned long>(hr)));
    }
    if (FAILED(device->CreateRenderTarget(1, 1, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE,
                                          &target.sync_probe, nullptr)))
        return Support::Unexpected(std::string("the frame sync surface cannot be created"));
    return target;
}

Support::Expected<void, std::string> Copy(IDirect3DDevice9* device, IDirect3DSurface9* source,
                                          const Target& target) {
    ComPtr<IDirect3DSurface9> level;
    if (FAILED(target.texture->GetSurfaceLevel(0, &level)))
        return Support::Unexpected(std::string("the shared texture has no surface"));
    D3DSURFACE_DESC source_desc = {};
    source->GetDesc(&source_desc);
    const bool same_size = std::cmp_equal(source_desc.Width, target.width) &&
                           std::cmp_equal(source_desc.Height, target.height);
    const HRESULT hr = device->StretchRect(source, nullptr, level, nullptr,
                                           same_size ? D3DTEXF_NONE : D3DTEXF_LINEAR);
    if (FAILED(hr)) {
        return Support::Unexpected(
            std::format("copying the frame failed (hr=0x{:08x})", static_cast<unsigned long>(hr)));
    }
    if (FAILED(device->StretchRect(level, nullptr, target.sync_probe, nullptr, D3DTEXF_POINT)))
        return Support::Unexpected(std::string("the frame sync copy failed"));
    D3DLOCKED_RECT locked = {};
    if (FAILED(target.sync_probe->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return Support::Unexpected(std::string("waiting for the GPU to finish the frame failed"));
    target.sync_probe->UnlockRect();
    return {};
}

}
