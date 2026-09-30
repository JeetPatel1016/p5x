// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/Oscillator.h"

#include <algorithm>
#include <cmath>

namespace p5x::dsp
{
namespace
{
// Where the sample sits relative to a discontinuity at phase 0, in samples: (-1, 0) just before
// the wrap, [0, 1) just after it; 2 (no correction) otherwise.
double positionFromWrap (double phase, double dt) noexcept
{
    if (phase < dt)
        return phase / dt;

    if (phase > 1.0 - dt)
        return (phase - 1.0) / dt;

    return 2.0;
}

double wrap01 (double x) noexcept
{
    return x - std::floor (x);
}
} // namespace

double Oscillator::blep (double x) noexcept
{
    if (x >= 1.0 || x <= -1.0)
        return 0.0;

    if (x < 0.0)
        return 0.5 * (x + 1.0) * (x + 1.0);

    return -0.5 * (1.0 - x) * (1.0 - x);
}

double Oscillator::blamp (double x) noexcept
{
    if (x >= 1.0 || x <= -1.0)
        return 0.0;

    if (x < 0.0)
        return (x + 1.0) * (x + 1.0) * (x + 1.0) / 6.0;

    return (1.0 - x) * (1.0 - x) * (1.0 - x) / 6.0;
}

void Oscillator::prepare (double internalRate) noexcept
{
    rate = std::max (1.0, internalRate);
}

void Oscillator::reset (double initialPhase) noexcept
{
    phase = wrap01 (std::isfinite (initialPhase) ? initialPhase : 0.0);
}

float Oscillator::process (double freqHz, float pw, OscShapes shapes) noexcept
{
    // 02-oscillators.md § Pitch / § Edge cases: clamp, never reflect negative frequencies.
    const double freq = std::clamp (std::isfinite (freqHz) ? freqHz : 0.01, 0.01, 0.45 * rate);
    const double dt = freq / rate;
    const double width = std::clamp (std::isfinite (pw) ? (double) pw : 0.5, 0.02, 0.98);

    const double atWrap = positionFromWrap (phase, dt);
    double out = 0.0;

    if (shapes.saw)
    {
        // Rising ramp with a -2 step at the wrap.
        out += 2.0 * phase - 1.0 - 2.0 * blep (atWrap);
    }

    if (shapes.pulse)
    {
        // +2 step at the wrap, -2 step at phase = pw.
        out += (phase < width ? 1.0 : -1.0) + 2.0 * blep (atWrap)
             - 2.0 * blep (positionFromWrap (wrap01 (phase - width), dt));
    }

    if (shapes.tri)
    {
        // 1 - 4|phase - 0.5|: slope +4 then -4 per cycle, i.e. slope changes of +8·dt at phase 0
        // and -8·dt at phase 0.5 (per sample).
        out += 1.0 - 4.0 * std::abs (phase - 0.5) + 8.0 * dt * blamp (atWrap)
             - 8.0 * dt * blamp (positionFromWrap (wrap01 (phase - 0.5), dt));
    }

    phase += dt;
    phase -= std::floor (phase);
    return (float) out;
}
} // namespace p5x::dsp
