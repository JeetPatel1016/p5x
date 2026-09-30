// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/Lfo.h"

#include <algorithm>
#include <cmath>

namespace p5x::dsp
{
void Lfo::prepare (double hostRate, double initialPhase) noexcept
{
    rate = std::max (1.0, hostRate);
    phase = std::isfinite (initialPhase) ? initialPhase - std::floor (initialPhase) : 0.0;
    squareStep = (float) (2.0 / (kSquareSlewSeconds * rate));
    square = phase < 0.5 ? 1.0f : -1.0f;
}

float Lfo::process (double rateHz, Shape shape) noexcept
{
    const double hz = std::clamp (std::isfinite (rateHz) ? rateHz : kMinRateHz, kMinRateHz, kMaxRateHz);

    // Square: linear 1 ms slew on transitions, so it doesn't click on amplitude-sensitive
    // destinations (05-modulation.md § LFO).
    const float squareTarget = phase < 0.5 ? 1.0f : -1.0f;
    square += std::clamp (squareTarget - square, -squareStep, squareStep);

    float out = square;

    if (shape == Shape::Saw)
        out = (float) (2.0 * phase - 1.0);
    else if (shape == Shape::Triangle)
        out = (float) (1.0 - 4.0 * std::abs (phase - 0.5));

    phase += hz / rate;
    phase -= std::floor (phase);
    return out;
}
} // namespace p5x::dsp
