// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/LadderFilterCEM.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace p5x::dsp
{
namespace
{
constexpr double kStartupImpulse = 1.0e-6; // see 03-filter.md § Self-oscillation
} // namespace

void LadderFilterCEM::prepare (double internalRate) noexcept
{
    rate = std::max (1.0, internalRate);
    reset();
}

void LadderFilterCEM::reset() noexcept
{
    s1 = s2 = s3 = s4 = 0.0;
    excitation = kStartupImpulse;
}

float LadderFilterCEM::feedbackFor (float res01) noexcept
{
    const float res = std::clamp (res01, 0.0f, 1.0f);
    return 4.0f * std::pow (res, 1.1f) * 1.02f;
}

float LadderFilterCEM::process (float in, float cutoffHz, float res01) noexcept
{
    const double fc = std::clamp (std::isfinite (cutoffHz) ? (double) cutoffHz : 1000.0, 5.0, 0.45 * rate);
    const double g = std::tan (std::numbers::pi * fc / rate);
    const double G = g / (1.0 + g);
    const double k = feedbackFor (std::isfinite (res01) ? res01 : 0.0f);

    const double x = (std::isfinite (in) ? (double) in : 0.0) + excitation;
    excitation = 0.0;

    // Instantaneous response of the cascade: y4 = G^4 · u + S.
    const double oneMinusG = 1.0 - G;
    const double S = oneMinusG * (G * G * G * s1 + G * G * s2 + G * s3 + s4);
    const double G4 = G * G * G * G;

    // Solve u = x - k · y4 exactly, then saturate the input stage.
    const double u = std::tanh ((x - k * S) / (1.0 + k * G4));

    auto stage = [G] (double input, double& state)
    {
        const double v = (input - state) * G;
        const double y = v + state;
        state = y + v;
        return y;
    };

    const double y1 = stage (u, s1);
    const double y2 = stage (y1, s2);
    const double y3 = stage (y2, s3);
    const double y4 = stage (y3, s4);
    return (float) y4;
}
} // namespace p5x::dsp
