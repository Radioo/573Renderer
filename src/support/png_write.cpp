#include "support/png_write.h"

#include "support/com_ptr.h"

#include <cstdint>
#include <ocidl.h>
#include <string>
#include <vector>
#include <wincodec.h>
#include <windows.h>

namespace Support {

namespace {

std::wstring Widen(const std::string& s) {
    int const n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w;
    if (n > 0) {
        w.resize(n - 1);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    }
    return w;
}

struct WicPngTarget {
    ComInit com;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> enc;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
};

bool OpenPngFrame(WicPngTarget& t, const std::wstring& wpath) {
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&t.factory)))) {
        return false;
    }
    if (FAILED(t.factory->CreateStream(&t.stream))) return false;
    if (FAILED(t.stream->InitializeFromFilename(wpath.c_str(), GENERIC_WRITE))) return false;
    if (FAILED(t.factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &t.enc))) return false;
    if (FAILED(t.enc->Initialize(t.stream, WICBitmapEncoderNoCache))) return false;
    if (FAILED(t.enc->CreateNewFrame(&t.frame, &t.props))) return false;
    return !FAILED(t.frame->Initialize(t.props));
}

bool EncodePngPixels(WicPngTarget& t, const uint8_t* bgra, int w, int h) {
    if (FAILED(t.frame->SetSize(static_cast<UINT>(w), static_cast<UINT>(h)))) return false;
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    if (FAILED(t.frame->SetPixelFormat(&fmt))) return false;
    UINT const stride = static_cast<UINT>(w) * 4;
    UINT const bufsize = stride * static_cast<UINT>(h);
    std::vector<BYTE> pixels(bgra, bgra + bufsize);
    if (FAILED(t.frame->WritePixels(static_cast<UINT>(h), stride, bufsize, pixels.data()))) {
        return false;
    }
    if (FAILED(t.frame->Commit())) return false;
    return !FAILED(t.enc->Commit());
}

}

bool WritePngBGRA(const std::string& path, const uint8_t* bgra, int w, int h) {
    if (w <= 0 || h <= 0) return false;
    std::wstring const wpath = Widen(path);
    WicPngTarget t;
    if (!OpenPngFrame(t, wpath)) return false;
    return EncodePngPixels(t, bgra, w, h);
}

}
