// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/WheelModSource.h"

#include "dsp/Random.h"

#include <algorithm>

namespace p5x::dsp
{
namespace
{
// Random sub-streams for the global sources; voices use streams 0–9 (their index).
constexpr uint64_t kLfoStream = 1000, kPinkStream = 1001;
} // namespace

void WheelModSource::prepare (double hostRate, int oversamplingFactor, uint64_t seed) noexcept
{
    factor = std::max (1, oversamplingFactor);

    Random phaseSource (Random::derive (seed, kLfoStream));
    lfo.prepare (hostRate, phaseSource.nextDouble()); // initial phase from the seed (05 § LFO)
    pink.prepare (Random::derive (seed, kPinkStream));

    rateSmoother.prepare (hostRate, 20.0);
    mixSmoother.prepare (hostRate, 20.0);
    previous = 0.0f;
}

void WheelModSource::setParameters (float lfoRateHz, Lfo::Shape lfoShape, float noiseMix) noexcept
{
    rateSmoother.setTarget (lfoRateHz);
    mixSmoother.setTarget (noiseMix);
    shape = lfoShape;
}

void WheelModSource::resetSmoothing (float lfoRateHz, float noiseMix) noexcept
{
    rateSmoother.reset (lfoRateHz);
    mixSmoother.reset (noiseMix);
}

void WheelModSource::render (float* out, int hostSamples) noexcept
{
    const float step = 1.0f / (float) factor;

    for (int j = 0; j < hostSamples; ++j)
    {
        const float lfoValue = lfo.process (rateSmoother.next(), shape);
        const float noiseValue = pink.process();
        const float mix = mixSmoother.next();
        const float value = (1.0f - mix) * lfoValue + mix * noiseValue;

        // Linear interpolation from the previous host sample (05-modulation.md § LFO).
        for (int k = 0; k < factor; ++k)
            out[j * factor + k] = previous + (value - previous) * step * (float) (k + 1);

        previous = value;
    }
}
} // namespace p5x::dsp
