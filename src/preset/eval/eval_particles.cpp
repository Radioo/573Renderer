#include "preset/eval/eval_particles.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_push.h"
#include "preset/eval/frame_state.h"
#include "preset/preset_rng.h"
#include "support/math/float_trig.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Preset::Eval {

namespace {

constexpr float kDegrees = 0.017453292F;

bool Spawns(const Doc::EmitterCmd& emitter, const Doc::Clip& clip, int frame, const BeatState& beat,
            const BeatSnapshot& snapshot) {
    switch (emitter.spawn) {
    case Doc::Spawn::ClipStart:
        return frame == clip.start;
    case Doc::Spawn::EveryFrame:
        return true;
    case Doc::Spawn::Beat: {
        if (beat.rate <= 0) return false;
        const auto grid = (std::size_t)(emitter.beat_grid == Doc::Grid::B ? 1 : 0);
        if (snapshot.since[grid] != 0) return false;
        return (snapshot.index[grid] & 1) == emitter.beat_odd;
    }
    }
    return false;
}

void SpawnOne(const Doc::EmitterCmd& emitter, const std::string& source,
              const Doc::Scatter* scatter, int reach, float phase, int index, const Vec2f& center,
              Preset::Ran3& rng, std::vector<Particle>& particles) {
    Particle particle;
    particle.emitter = source;
    particle.asset = emitter.asset;
    particle.cell = emitter.cell;
    particle.from_x = (int)center[0];
    particle.from_y = (int)center[1];
    if (scatter != nullptr) {
        particle.to_x = (rng.Next() % (int)scatter->span[0]) + (int)scatter->offset[0];
        particle.to_y = (rng.Next() % (int)scatter->span[1]) + (int)scatter->offset[1];
    } else {
        const float angle = (((float)index * (float)emitter.angle_step_deg) + phase) * kDegrees;
        particle.to_x = (int)((Support::Sinf(angle) * (float)reach) + center[0]);
        particle.to_y = (int)((Support::Cosf(angle) * (float)reach) + center[1]);
    }
    particle.life = (emitter.life_span > 0) ? ((rng.Next() % emitter.life_span) + emitter.life_base)
                                            : emitter.life;
    particle.life = std::max(1, particle.life);
    particle.priority = emitter.priority;
    particle.blend = (int)emitter.blend;
    particle.scale = (float)emitter.scale_percent * 0.01F;
    particles.push_back(std::move(particle));
}

}

int RingReach(const Doc::EmitterCmd& emitter, int elapsed) {
    const int span = std::max(1, emitter.reach_frames);
    return emitter.radius_from +
           (((emitter.radius_to - emitter.radius_from) * std::min(elapsed, span)) / span);
}

float RingPhase(const Doc::EmitterCmd& emitter, int frame) {
    const float wobble = Support::Sinf((float)frame * (float)emitter.phase_rate_deg * kDegrees) *
                         (float)emitter.phase_amplitude_deg;
    return (float)(int)wobble;
}

void AgeParticles(std::vector<Particle>& particles) {
    for (Particle& particle : particles)
        particle.age++;
    std::erase_if(particles,
                  [](const Particle& particle) { return particle.age >= particle.life; });
}

void SpawnParticles(const FrameState& state, int frame, const BeatSnapshot& beat, int canvas_w,
                    int canvas_h, Preset::Ran3& rng, std::vector<Particle>& particles) {
    for (const Doc::Clip* clip : state.emitters) {
        const auto* emitter = std::get_if<Doc::EmitterCmd>(&clip->command);
        if (emitter == nullptr) continue;
        const Doc::Scatter* scatter =
            emitter->scatter.has_value() ? &emitter->scatter.value() : nullptr;
        if (scatter != nullptr && (scatter->span[0] <= 0.0 || scatter->span[1] <= 0.0)) continue;
        if (!Spawns(*emitter, *clip, frame, state.beat, beat)) continue;

        const int elapsed = std::max(0, frame - clip->start);
        const int reach = RingReach(*emitter, elapsed);
        const float phase = RingPhase(*emitter, frame);
        const Vec2f center = {
            emitter->center.has_value() ? (float)(*emitter->center)[0] : (float)canvas_w * 0.5F,
            emitter->center.has_value() ? (float)(*emitter->center)[1] : (float)canvas_h * 0.5F};
        for (int i = 0; i < emitter->count; i++)
            SpawnOne(*emitter, clip->id, scatter, reach, phase, i, center, rng, particles);
    }
}

float DrawJitter(const JitterState& jitter, Preset::Ran3& rng) {
    if (!jitter.active || jitter.span <= 0) return 0.0F;
    const int centre = jitter.span / 2;
    const int draw = (rng.Next() % jitter.span) - centre;
    return (float)draw * jitter.scale;
}

void CollectParticles(const std::vector<Particle>& particles, int split, bool behind_models,
                      std::vector<CellDraw>& cells) {
    for (const Particle& particle : particles) {
        if ((particle.priority >= split) != behind_models) continue;
        if (particle.age < 0) continue;
        const int age = particle.age;
        const int life = particle.life;
        const int x = particle.from_x + ((age * (particle.to_x - particle.from_x)) / life);
        const int y = particle.from_y + ((age * (particle.to_y - particle.from_y)) / life);
        const int alpha = 100 - ((100 * age) / life);
        cells.push_back(CellDraw{.asset = particle.asset,
                                 .name = particle.cell,
                                 .x = (float)x,
                                 .y = (float)y,
                                 .alpha = (float)alpha * 0.01F,
                                 .scale = particle.scale,
                                 .blend = particle.blend});
    }
}

}
