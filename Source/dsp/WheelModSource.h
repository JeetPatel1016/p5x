// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

#include "dsp/Lfo.h"
#include "dsp/PinkNoise.h"
#include "dsp/Smoother.h"

#include <cstdint>

namespace p5x::dsp
{
// Wheel-Mod's global source (05-modulation.md § Wheel-Mod): (1 − wm_mix)·LFO + wm_mix·pink noise,
// bipolar. Runs at the host rate and is linearly interpolated up to the internal rate. The amount
// (mod wheel) and the destinations are applied per voice.
class WheelModSource
{
public:
    void prepare (double hostRate, int oversamplingFactor, uint64_t seed) noexcept;

    // Block-rate targets: lfo_rate is smoothed Log 20, wm_mix Lin 20 (01-parameters.md).
    void setParameters (float lfoRateHz, Lfo::Shape lfoShape, float noiseMix) noexcept;
    void resetSmoothing (float lfoRateHz, float noiseMix) noexcept;

    // Fills hostSamples × oversamplingFactor internal-rate values.
    void render (float* out, int hostSamples) noexcept;

    const Lfo& getLfo() const noexcept { return lfo; }

private:
    Lfo lfo;
    PinkNoise pink;
    LogSmoother rateSmoother;
    LinearSmoother mixSmoother;
    Lfo::Shape shape = Lfo::Shape::Triangle;
    int factor = 2;
    float previous = 0.0f;
};
} // namespace p5x::dsp
