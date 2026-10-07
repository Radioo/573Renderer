#include "media_sink.h"
#include "support/com_ptr.h"
#include "support/log.h"
#include "media/media_format.h"
#include "video_encoder.h"
#include "video_encoder_codecs.h"

extern "C" {
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}
#include <system_error>
#include <intsafe.h>
#include <cstddef>
#include <cstdint>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>

#include <filesystem>
#include <string>
#include <vector>

namespace MediaSink {

namespace {

struct PngSeq {
    std::wstring dir_w;
    bool we_inited_com = false;
    ComPtr<IWICImagingFactory> factory;

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
        factory.Reset();
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
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapEncoder> encoder;
        ComPtr<IWICBitmapFrameEncode> frame;
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
    SwsContext* png_sws = nullptr;
    std::vector<uint8_t> png_buf;
    int png_w = 0;
    int png_h = 0;

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    ~Impl() { DropScaler(); }

    void DropScaler() {
        if (png_sws == nullptr) return;
        sws_freeContext(png_sws);
        png_sws = nullptr;
    }

    bool MakeScaler(const Params& p) {
        DropScaler();
        png_w = p.out_width > 0 ? p.out_width : p.src_width;
        png_h = p.out_height > 0 ? p.out_height : p.src_height;
        if (png_w == p.src_width && png_h == p.src_height) return true;
        png_sws =
            VideoEncoder::MakeSws(p.src_width, p.src_height, png_w, png_h, AV_PIX_FMT_BGRA, err);
        if (png_sws == nullptr) return false;
        png_buf.assign(static_cast<std::size_t>(png_w) * png_h * 4, 0);
        return true;
    }

    const uint8_t* Scaled(const uint8_t* bgra) {
        if (png_sws == nullptr) return bgra;
        const uint8_t* slices[1] = {bgra};
        const int strides[1] = {params.src_width * 4};
        uint8_t* out_slices[1] = {png_buf.data()};
        const int out_strides[1] = {png_w * 4};
        sws_scale(png_sws, slices, strides, 0, params.src_height, out_slices, out_strides);
        return png_buf.data();
    }
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
        if (!impl_->MakeScaler(p)) return false;
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
    return impl_->png.WriteFrame(frame_index, impl_->Scaled(bgra), impl_->png_w, impl_->png_h,
                                 impl_->err);
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
    impl_->DropScaler();
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
    impl_->DropScaler();
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
