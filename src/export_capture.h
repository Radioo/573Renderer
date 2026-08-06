#pragma once

struct D3D9State;

namespace Export {

struct Session;

class ICaptureDriver {
public:
    ICaptureDriver() = default;
    virtual ~ICaptureDriver() = default;
    ICaptureDriver(const ICaptureDriver&) = delete;
    ICaptureDriver& operator=(const ICaptureDriver&) = delete;
    ICaptureDriver(ICaptureDriver&&) = delete;
    ICaptureDriver& operator=(ICaptureDriver&&) = delete;

    virtual void BeginCapture(Session& sess) = 0;

    virtual void TickCapture(Session& sess, D3D9State& d3d) = 0;

    virtual void EndCapture(Session& sess) = 0;
};

}
