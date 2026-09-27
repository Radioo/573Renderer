#include "document/transform_parts.h"

#include "document/stage_bounds.h"

#include <cmath>
#include <numbers>

namespace Document {

namespace {

constexpr double kHalfTurn = 180.0;

double Degrees(double radians) {
    return radians * kHalfTurn / std::numbers::pi;
}

double Radians(double degrees) {
    return degrees * std::numbers::pi / kHalfTurn;
}

double Wrapped(double degrees) {
    double wrapped = std::remainder(degrees, 2 * kHalfTurn);
    if (wrapped <= -kHalfTurn) wrapped += 2 * kHalfTurn;
    return wrapped;
}

}

TransformParts PartsOf(const Linear& linear) {
    const bool mirrored = (linear.a * linear.d) - (linear.b * linear.c) < 0;
    const double width = std::hypot(linear.a, linear.b);
    const double turn =
        mirrored ? std::atan2(-linear.b, -linear.a) : std::atan2(linear.b, linear.a);
    const double upright = std::atan2(-linear.c, linear.d);
    return TransformParts{.scale_x = mirrored ? -width : width,
                          .scale_y = std::hypot(linear.c, linear.d),
                          .rotation = Wrapped(Degrees(turn)),
                          .skew = Wrapped(Degrees(upright - turn))};
}

Linear LinearOf(const TransformParts& parts) {
    const double turn = Radians(parts.rotation);
    const double upright = Radians(parts.rotation + parts.skew);
    return Linear{.a = parts.scale_x * std::cos(turn),
                  .b = parts.scale_x * std::sin(turn),
                  .c = -parts.scale_y * std::sin(upright),
                  .d = parts.scale_y * std::cos(upright)};
}

}
