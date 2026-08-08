#pragma once

#include "afp_ddr_shape.h"
#include "support/engine_abi.h"

namespace DdrRender {

void SetShapeProvider(const DdrAfp::ShapeProvider* provider);

int AFP_CB Cb_GetShapeId(unsigned int* out_id, const char* name);

int AFP_CB Cb_GetShapeRect(unsigned int id, float* out4);

void AFP_CB Cb_DrawShape(unsigned int id, const float* c0, const float* c1, void* ctx);

}
