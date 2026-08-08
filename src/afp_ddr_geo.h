#pragma once

#include "afp_ddr_shape.h"
#include "afp_ddr_txp2.h"

#include <vector>

namespace DdrAfp {

void BuildGeoRegistry(const Txp2Loaded& loaded, const std::vector<int>& texture_slots);

size_t GeoShapeCount();

const ShapeProvider& GeoProvider();

}
