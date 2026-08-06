#pragma once

#include "export_capture.h"

namespace Backend {

class AfpModernCaptureDriver final : public Export::ICaptureDriver {
public:
    void BeginCapture(Export::Session& sess) override;
    void TickCapture(Export::Session& sess, D3D9State& d3d) override;
    void EndCapture(Export::Session& sess) override;
};

class AfpDdrCaptureDriver final : public Export::ICaptureDriver {
public:
    void BeginCapture(Export::Session& sess) override;
    void TickCapture(Export::Session& sess, D3D9State& d3d) override;
    void EndCapture(Export::Session& sess) override;
};

}
