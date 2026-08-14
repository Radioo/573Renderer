#pragma once

#include "preset/scene_preset.h"

#include <array>

namespace Preset {

namespace {

constexpr std::array<ParamOverride, 7> kEnd00Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.0F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.0F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, -0.3F}},
    {.id = "model[core].alpha", .f = {0.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {0.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible"},
    {.id = "model[flame].visible"},
}};

constexpr std::array<ParamOverride, 7> kEnd01Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
    {.id = "jitter.from_frame", .i = 430},
    {.id = "jitter.span", .i = 200},
    {.id = "jitter.scale", .f = {0.000099999997F, 0.0F, 0.0F}},
}};

constexpr std::array<Ramp, 3> kEnd01Ramps = {{
    {.id = "camera.eye",
     .from = {0.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.0F, 0.249F},
     .frames = 126,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "model[core].alpha",
     .from = {0.0F, 0.0F, 0.0F},
     .to = {1.0F, 0.0F, 0.0F},
     .frames = 60,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "model[flame].alpha",
     .from = {0.0F, 0.0F, 0.0F},
     .to = {1.0F, 0.0F, 0.0F},
     .frames = 60,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
}};

constexpr std::array<ParamOverride, 10> kEnd02Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, 0.249F}},
    {.id = "model[core].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
    {.id = "jitter.from_frame", .i = 723},
    {.id = "jitter.span", .i = 100},
    {.id = "jitter.scale", .f = {0.000099999997F, 0.0F, 0.0F}},
}};

constexpr std::array<Emitter, 1> kEnd02Burst = {{
    {.package_dir = "data/graph/sys/system",
     .cell = "PTC_ORAN",
     .count = 480,
     .priority = 24,
     .scale = 200,
     .scatter = true,
     .span_x = 1280,
     .span_y = 960,
     .offset_x = -320,
     .offset_y = -240,
     .life = 45},
}};

constexpr std::array<ParamOverride, 9> kEnd03Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, 0.249F}},
    {.id = "model[core].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
    {.id = "pulse.scale_odd", .f = {1.1F, 0.0F, 0.0F}},
    {.id = "pulse.scale_even", .f = {1.1F, 0.0F, 0.0F}},
}};

constexpr std::array<ParamOverride, 9> kEnd04Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.034906584F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, -0.017453292F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, 0.249F}},
    {.id = "model[core].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
    {.id = "pulse.scale_odd", .f = {1.1F, 0.0F, 0.0F}},
    {.id = "pulse.scale_even", .f = {1.1F, 0.0F, 0.0F}},
}};

constexpr std::array<ParamOverride, 10> kEnd05Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, 0.249F}},
    {.id = "model[core].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
    {.id = "pulse.grid", .i = 1},
    {.id = "pulse.scale_odd", .f = {1.1F, 0.0F, 0.0F}},
    {.id = "pulse.scale_even", .f = {1.1F, 0.0F, 0.0F}},
}};

constexpr std::array<ParamOverride, 10> kEnd05bParams = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, 0.249F}},
    {.id = "model[core].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
    {.id = "pulse.grid", .i = 1},
    {.id = "pulse.scale_odd", .f = {1.1F, 0.0F, 0.0F}},
    {.id = "pulse.scale_even", .f = {1.1F, 0.0F, 0.0F}},
}};

constexpr std::array<Ramp, 1> kEnd05bRamps = {{
    {.id = "model[flame].rotation",
     .from = {0.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.0F, 6.4402647F},
     .frames = 369,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
}};

constexpr std::array<ParamOverride, 6> kEnd06Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "model[core].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
}};

constexpr std::array<Ramp, 2> kEnd06Ramps = {{
    {.id = "camera.eye",
     .from = {0.0F, 0.0F, 0.249F},
     .to = {0.0F, 0.0F, -0.85F},
     .frames = 175,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "sprite[END_BG1].scale",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {9.75F, 0.0F, 0.0F},
     .frames = 175,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
}};

constexpr std::array<ParamOverride, 8> kEnd07Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, -0.052359876F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, -0.052359876F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, -0.3F}},
    {.id = "model[core].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
    {.id = "sprite[END_BG1].scale", .f = {2.0F, 0.0F, 0.0F}},
}};

constexpr std::array<ParamOverride, 5> kEnd08Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.017453292F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, -0.2F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
}};

constexpr std::array<Ramp, 2> kEnd08Ramps = {{
    {.id = "model[core].alpha",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.0F, 0.0F},
     .frames = 120,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "model[flame].alpha",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.0F, 0.0F},
     .frames = 120,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
}};

constexpr std::array<ParamOverride, 5> kEnd09Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.0F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.0F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, -0.2F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
}};

constexpr std::array<Ramp, 4> kEnd09Ramps = {{
    {.id = "model[core].alpha",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.0F, 0.0F},
     .frames = 60,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "model[flame].alpha",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.0F, 0.0F},
     .frames = 60,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "model[core].rotation",
     .from = {0.0F, 0.0F, 45.0F},
     .to = {0.0F, 6.283186F, 45.0F},
     .frames = 60,
     .degrees_per_frame = 1.125F,
     .curve = Curve::Sine},
    {.id = "sprite[END_BG1].scale",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {1.92F, 0.0F, 0.0F},
     .frames = 92,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
}};

constexpr std::array<Emitter, 1> kEnd09Burst = {{
    {.package_dir = "data/graph/sys/system",
     .cell = "PTC_CYAN",
     .count = 480,
     .priority = 24,
     .scale = 200,
     .scatter = true,
     .span_x = 1280,
     .span_y = 960,
     .offset_x = -320,
     .offset_y = -240,
     .life = 45},
}};

constexpr std::array<ParamOverride, 5> kEnd10Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.0F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.0F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, -0.2F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
}};

constexpr std::array<Ramp, 4> kEnd10Ramps = {{
    {.id = "model[core].alpha",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.0F, 0.0F},
     .frames = 60,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "model[flame].alpha",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.0F, 0.0F},
     .frames = 60,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "model[core].rotation",
     .from = {0.0F, 0.0F, 45.0F},
     .to = {0.0F, 12.56637F, 45.0F},
     .frames = 60,
     .degrees_per_frame = 1.125F,
     .curve = Curve::Sine},
    {.id = "sprite[END_BG1].scale",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {2.84F, 0.0F, 0.0F},
     .frames = 92,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
}};

constexpr std::array<Emitter, 1> kEnd10Burst = {{
    {.package_dir = "data/graph/sys/system",
     .cell = "PTC_BLUE",
     .count = 480,
     .priority = 24,
     .scale = 200,
     .scatter = true,
     .span_x = 1280,
     .span_y = 960,
     .offset_x = -320,
     .offset_y = -240,
     .life = 45},
}};

constexpr std::array<ParamOverride, 5> kEnd11Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.157079628F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.157079628F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, -0.2F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
}};

constexpr std::array<Ramp, 3> kEnd11Ramps = {{
    {.id = "model[core].alpha",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.0F, 0.0F},
     .frames = 60,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "model[flame].alpha",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {0.0F, 0.0F, 0.0F},
     .frames = 60,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "sprite[END_BG1].scale",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {2.35F, 0.0F, 0.0F},
     .frames = 45,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
}};

constexpr std::array<Emitter, 1> kEnd11Burst = {{
    {.package_dir = "data/graph/sys/system",
     .cell = "PTC_ORAN",
     .count = 480,
     .priority = 24,
     .scale = 200,
     .scatter = true,
     .span_x = 1280,
     .span_y = 960,
     .offset_x = -320,
     .offset_y = -240,
     .life = 45},
}};

constexpr std::array<ParamOverride, 7> kEnd12Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, -0.209439504F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, -0.209439504F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, -0.2F}},
    {.id = "model[core].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
}};

constexpr std::array<Ramp, 1> kEnd12Ramps = {{
    {.id = "sprite[END_BG1].scale",
     .from = {3.0F, 0.0F, 0.0F},
     .to = {2.1F, 0.0F, 0.0F},
     .frames = 45,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
}};

constexpr std::array<Emitter, 1> kEnd12Burst = {{
    {.package_dir = "data/graph/sys/system",
     .cell = "PTC_ORAN",
     .count = 480,
     .priority = 24,
     .scale = 200,
     .scatter = true,
     .span_x = 1280,
     .span_y = 960,
     .offset_x = -320,
     .offset_y = -240,
     .life = 45},
}};

constexpr std::array<ParamOverride, 5> kEnd13Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, 0.279252672F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, 0.279252672F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, -0.2F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
}};

constexpr std::array<Ramp, 3> kEnd13Ramps = {{
    {.id = "model[core].alpha",
     .from = {0.0F, 0.0F, 0.0F},
     .to = {1.0F, 0.0F, 0.0F},
     .frames = 60,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "model[flame].alpha",
     .from = {0.0F, 0.0F, 0.0F},
     .to = {1.0F, 0.0F, 0.0F},
     .frames = 60,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
    {.id = "sprite[END_BG1].scale",
     .from = {1.0F, 0.0F, 0.0F},
     .to = {5.15F, 0.0F, 0.0F},
     .frames = 83,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
}};

constexpr std::array<Emitter, 1> kEnd13Burst = {{
    {.package_dir = "data/graph/sys/system",
     .cell = "PTC_ORAN",
     .count = 32,
     .priority = 24,
     .scale = 200,
     .scatter = true,
     .span_x = 1280,
     .span_y = 960,
     .offset_x = -320,
     .offset_y = -240,
     .spawn = Spawn::EveryFrame,
     .life_base = 48,
     .life_span = 48},
}};

constexpr std::array<ParamOverride, 9> kEnd14Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, -0.052359876F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, -0.052359876F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, -0.23F}},
    {.id = "model[core].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
    {.id = "pulse.scale_odd", .f = {1.5F, 0.0F, 0.0F}},
    {.id = "pulse.scale_even", .f = {1.2F, 0.0F, 0.0F}},
}};

constexpr std::array<Emitter, 1> kEnd14Burst = {{
    {.package_dir = "data/graph/sys/system",
     .cell = "PTC_BLUE",
     .count = 128,
     .priority = 24,
     .scale = 200,
     .scatter = true,
     .span_x = 1280,
     .span_y = 960,
     .offset_x = -320,
     .offset_y = -240,
     .spawn = Spawn::Beat,
     .beat_odd = 1,
     .life_base = 48,
     .life_span = 48},
}};

constexpr std::array<ParamOverride, 7> kEnd15Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, -0.052359876F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, -0.052359876F, 0.0F}},
    {.id = "camera.eye", .f = {0.0F, 0.0F, -0.23F}},
    {.id = "model[core].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
}};

constexpr std::array<ParamOverride, 6> kEnd16Params = {{
    {.id = "model[core].motion.spin_per_frame", .f = {0.0F, -0.052359876F, 0.0F}},
    {.id = "model[flame].motion.spin_per_frame", .f = {0.0F, -0.052359876F, 0.0F}},
    {.id = "model[core].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[flame].alpha", .f = {1.0F, 0.0F, 0.0F}},
    {.id = "model[core].visible", .i = 1},
    {.id = "model[flame].visible", .i = 1},
}};

constexpr std::array<Ramp, 1> kEnd16Ramps = {{
    {.id = "camera.eye",
     .from = {0.0F, 0.0F, -0.23F},
     .to = {0.0F, 0.0F, -1.73F},
     .frames = 300,
     .degrees_per_frame = 0.0F,
     .curve = Curve::Linear},
}};

constexpr std::array<Phase, 18> kEndingPhases = {{
    {.label = "Black, models hidden", .start_frame = 0, .params = kEnd00Params},
    {.label = "Fade in, camera pulling back",
     .start_frame = 71,
     .params = kEnd01Params,
     .ramps = kEnd01Ramps},
    {.label = "Burst, the spin-up whip",
     .start_frame = 440,
     .params = kEnd02Params,
     .emitters = kEnd02Burst},
    {.label = "Beat pulse", .start_frame = 814, .params = kEnd03Params},
    {.label = "Core and flame counter-rotate", .start_frame = 1185, .params = kEnd04Params},
    {.label = "Second beat run", .start_frame = 1557, .params = kEnd05Params},
    {.label = "Beat 80, the flame unwinds",
     .start_frame = 1930,
     .params = kEnd05bParams,
     .ramps = kEnd05bRamps},
    {.label = "Camera pushes through",
     .start_frame = 2300,
     .params = kEnd06Params,
     .ramps = kEnd06Ramps},
    {.label = "Snap back, triple speed", .start_frame = 2475, .params = kEnd07Params},
    {.label = "Fade out, close in",
     .start_frame = 2486,
     .params = kEnd08Params,
     .ramps = kEnd08Ramps},
    {.label = "Cyan burst",
     .start_frame = 2672,
     .params = kEnd09Params,
     .ramps = kEnd09Ramps,
     .emitters = kEnd09Burst},
    {.label = "Blue burst",
     .start_frame = 2765,
     .params = kEnd10Params,
     .ramps = kEnd10Ramps,
     .emitters = kEnd10Burst},
    {.label = "Nine degree spin",
     .start_frame = 2858,
     .params = kEnd11Params,
     .ramps = kEnd11Ramps,
     .emitters = kEnd11Burst},
    {.label = "Reverse twelve",
     .start_frame = 2904,
     .params = kEnd12Params,
     .ramps = kEnd12Ramps,
     .emitters = kEnd12Burst},
    {.label = "Sixteen degree spin, fading up",
     .start_frame = 2950,
     .params = kEnd13Params,
     .ramps = kEnd13Ramps,
     .emitters = kEnd13Burst},
    {.label = "Logo hold", .start_frame = 3034, .params = kEnd14Params, .emitters = kEnd14Burst},
    {.label = "Drift out", .start_frame = 3787, .params = kEnd15Params},
    {.label = "Final push in", .start_frame = 4065, .params = kEnd16Params, .ramps = kEnd16Ramps},
}};

}

}
