#include "media_sink.h"
#include "support/log.h"
#include "media/media_format.h"
#include "video_encoder.h"
#include <system_error>
#include <intsafe.h>
#include <cstdint>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>

#include <filesystem>
#include <string>

namespace MediaSink {

namespace {

struct PngSeq {
    std::wstring dir_w;
    bool we_inited_com = false;
    IWICImagingFactory* factory = nullptr;

    bool Open(const std::string& dir) {
        dir_w.assign(dir.begin(), dir.end());
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) return false;

        HRESULT const init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        we_inited_com = SUCCEEDED(init);

        HRESULT const hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                            IID_PPV_ARGS(&factory));
        return SUCCEEDED(hr) && (factory != nullptr);
    }

    void Close() {
        if (factory != nullptr) {
            factory->Release();
            factory = nullptr;
        }
        if (we_inited_com) {
            CoUninitialize();
            we_inited_com = false;
        }
    }

    [[nodiscard]] std::wstring FramePath(int idx) const {
        wchar_t buf[32];
        swprintf_s(buf, L"\\frame_%06d.png", idx);
        return dir_w + buf;
    }

    struct WicPngTarget {
        IWICStream* stream = nullptr;
        IWICBitmapEncoder* encoder = nullptr;
        IWICBitmapFrameEncode* frame = nullptr;
        WicPngTarget() = default;
        WicPngTarget(const WicPngTarget&) = delete;
        WicPngTarget& operator=(const WicPngTarget&) = delete;
        WicPngTarget(WicPngTarget&&) = delete;
        WicPngTarget& operator=(WicPngTarget&&) = delete;
        ~WicPngTarget() {
            if (frame != nullptr) frame->Release();
            if (encoder != nullptr) encoder->Release();
            if (stream != nullptr) stream->Release();
        }
    };

    bool OpenPngTarget(const std::wstring& path, WicPngTarget& t, std::string& err) const {
        HRESULT hr = factory->CreateStream(&t.stream);
        if (FAILED(hr) || (t.stream == nullptr)) {
            err = "WIC: CreateStream failed";
            return false;
        }
        hr = t.stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
        if (FAILED(hr)) {
            err = "WIC: InitializeFromFilename failed (path open?)";
            return false;
        }
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &t.encoder);
        if (FAILED(hr) || (t.encoder == nullptr)) {
            err = "WIC: CreateEncoder(PNG) failed";
            return false;
        }
        hr = t.encoder->Initialize(t.stream, WICBitmapEncoderNoCache);
        if (FAILED(hr)) {
            err = "WIC: Encoder::Initialize failed";
            return false;
        }
        hr = t.encoder->CreateNewFrame(&t.frame, nullptr);
        if (FAILED(hr) || (t.frame == nullptr)) {
            err = "WIC: CreateNewFrame failed";
            return false;
        }
        hr = t.frame->Initialize(nullptr);
        if (FAILED(hr)) {
            err = "WIC: Frame::Initialize failed";
            return false;
        }
        return true;
    }

    bool WriteFrame(int idx, const uint8_t* bgra, int w, int h, std::string& err) const {
        if (factory == nullptr) {
            err = "PngSeq: factory not initialised";
            return false;
        }
        if ((bgra == nullptr) || w <= 0 || h <= 0) {
            err = "PngSeq: bad frame args";
            return false;
        }

        WicPngTarget t;
        if (!OpenPngTarget(FramePath(idx), t, err)) return false;

        HRESULT hr = t.frame->SetSize((UINT)w, (UINT)h);
        if (FAILED(hr)) {
            err = "WIC: Frame::SetSize failed";
            return false;
        }
        WICPixelFormatGUID pf = GUID_WICPixelFormat32bppBGRA;
        hr = t.frame->SetPixelFormat(&pf);
        if (FAILED(hr)) {
            err = "WIC: Frame::SetPixelFormat failed";
            return false;
        }

        const UINT stride = (UINT)w * 4;
        const UINT bytes = stride * (UINT)h;
        hr = t.frame->WritePixels((UINT)h, stride, bytes, const_cast<BYTE*>(bgra));
        if (FAILED(hr)) {
            err = "WIC: WritePixels failed";
            return false;
        }
        hr = t.frame->Commit();
        if (FAILED(hr)) {
            err = "WIC: Frame::Commit failed";
            return false;
        }
        hr = t.encoder->Commit();
        if (FAILED(hr)) {
            err = "WIC: Encoder::Commit failed";
            return false;
        }
        return true;
    }

    void DeleteFrames() const {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dir_w, ec)) {
            auto name = entry.path().filename().wstring();
            if (name.starts_with(L"frame_") && name.size() > 4 && name.ends_with(L".png")) {
                std::filesystem::remove(entry.path(), ec);
            }
        }
    }
};

}

struct Sink::Impl {
    Params params;
    bool opened = false;
    bool video_path = false;
    VideoEncoder::Encoder enc;
    PngSeq png;
    std::string err;
};

Sink::Sink() : impl_(std::make_unique<Impl>()) {}
Sink::~Sink() {
    try {
        if (impl_) Cancel();
    } catch (...) {
        LOG("MediaSink", "~Sink: Cancel threw, ignoring");
    }
}
Sink::Sink(Sink&&) noexcept = default;
Sink& Sink::operator=(Sink&&) noexcept = default;

bool Sink::Open(const Params& p) {
    if (!impl_) impl_ = std::make_unique<Impl>();
    if (impl_->opened) {
        impl_->err = "MediaSink::Open: already opened";
        return false;
    }
    impl_->params = p;
    impl_->video_path = !WritesDirectory(p.format);

    if (impl_->video_path) {
        VideoEncoder::Params ep;
        ep.output_path = p.output_path;
        ep.format = p.format;
        ep.src_width = p.src_width;
        ep.src_height = p.src_height;
        ep.out_width = p.out_width;
        ep.out_height = p.out_height;
        ep.fps = p.fps;
        ep.quality = p.quality;
        ep.keyframe_interval = p.keyframe_interval;
        ep.prefer_hardware = p.prefer_hardware;
        if (!impl_->enc.Create(ep)) {
            impl_->err = impl_->enc.LastError();
            return false;
        }
    } else {
        if (!impl_->png.Open(p.output_path)) {
            impl_->err = "PngSeq: failed to create output directory '" + p.output_path + "'";
            return false;
        }
    }
    impl_->opened = true;
    return true;
}

bool Sink::SubmitFrame(const uint8_t* bgra, int frame_index) {
    if (!impl_ || !impl_->opened) {
        if (impl_) impl_->err = "MediaSink::SubmitFrame: not opened";
        return false;
    }
    if (impl_->video_path) {
        if (!impl_->enc.SubmitFrame(bgra, frame_index)) {
            impl_->err = impl_->enc.LastError();
            return false;
        }
        return true;
    }
    return impl_->png.WriteFrame(frame_index, bgra, impl_->params.src_width,
                                 impl_->params.src_height, impl_->err);
}

bool Sink::Finish() {
    if (!impl_ || !impl_->opened) return false;
    if (impl_->video_path) {
        if (!impl_->enc.Finish()) {
            impl_->err = impl_->enc.LastError();
            return false;
        }
    } else {
        impl_->png.Close();
    }
    impl_->opened = false;
    return true;
}

void Sink::Cancel() {
    if (!impl_ || !impl_->opened) return;
    if (impl_->video_path) {
        impl_->enc.Cancel();
    } else {
        impl_->png.DeleteFrames();
        impl_->png.Close();
        std::error_code ec;
        std::filesystem::remove(impl_->params.output_path, ec);
    }
    impl_->opened = false;
}

bool Sink::UsingHardware() const {
    return impl_ && impl_->opened && impl_->video_path && impl_->enc.UsingHardware();
}

Format Sink::ActiveFormat() const {
    return impl_ ? impl_->params.format : Format::AVIF;
}
const std::string& Sink::OutputPath() const {
    static const std::string kEmpty;
    return impl_ ? impl_->params.output_path : kEmpty;
}
const std::string& Sink::LastError() const {
    static const std::string kEmpty;
    return impl_ ? impl_->err : kEmpty;
}

}
