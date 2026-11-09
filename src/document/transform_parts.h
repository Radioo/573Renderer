#pragma once

#include "document/stage_bounds.h"

namespace Document {

struct TransformParts {
    double scale_x = 1;
    double scale_y = 1;
    double rotation = 0;
    double skew = 0;
};

[[nodiscard]] TransformParts PartsOf(const Linear& linear);

[[nodiscard]] Linear LinearOf(const TransformParts& parts);

}
