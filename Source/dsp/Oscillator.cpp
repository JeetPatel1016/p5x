// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/Oscillator.h"

#include <algorithm>
#include <cmath>

namespace p5x::dsp
{
namespace
{
double wrap01 (double x) noexcept
{
    return x - std::floor (x);
}

// The summed naive waveform at `phase` (02-oscillators.md § Waveforms).
double naive (double phase, double width, OscShapes shapes) noexcept
{
    double out = 0.0;

    if (shapes.saw)
        out += 2.0 * phase - 1.0;

    if (shapes.pulse)
        out += phase < width ? 1.0 : -1.0;

    if (shapes.tri)
        out += 1.0 - 4.0 * std::abs (phase - 0.5);

    return out;
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
    pending = 0.0;
    wrapped = false;
    wrapAt = 0.0;
}

void Oscillator::correct (double& out, double f, Step step) noexcept
{
    // The discontinuity sits f samples after this sample and 1 − f before the next one: this sample
    // takes blep(−f), the next blep(1 − f). Written out so f = 0 lands on the "before" side.
    const double before = 1.0 - f;
    out += step.jump * 0.5 * before * before + step.slope * before * before * before / 6.0;
    pending += -step.jump * 0.5 * f * f + step.slope * f * f * f / 6.0;
}

void Oscillator::correctEdges (double& out, double start, double dt, double from, double to, double width,
                               OscShapes shapes) noexcept
{
    const double end = start + (to - from) * dt; // phase reached (unwrapped)

    // Fraction of the step at which the phase crosses `threshold` (or threshold + 1), or −1.
    auto crossing = [&] (double threshold)
    {
        const double t = threshold > start ? threshold : threshold + 1.0;
        return t <= end ? from + (t - start) / dt : -1.0;
    };

    if (const double f = crossing (1.0); f >= 0.0)
    {
        wrapped = true;
        wrapAt = f;

        // Saw falls by 2, pulse rises by 2, triangle's slope turns from −4 to +4 per cycle.
        Step step;
        step.jump = (shapes.saw ? -2.0 : 0.0) + (shapes.pulse ? 2.0 : 0.0);
        step.slope = shapes.tri ? 8.0 * dt : 0.0;
        correct (out, f, step);
    }

    if (shapes.pulse)
        if (const double f = crossing (width); f >= 0.0)
            correct (out, f, { -2.0, 0.0 });

    if (shapes.tri)
        if (const double f = crossing (0.5); f >= 0.0)
            correct (out, f, { 0.0, -8.0 * dt });
}

float Oscillator::process (double freqHz, float pw, OscShapes shapes, double syncFraction) noexcept
{
    // 02-oscillators.md § Pitch / § Edge cases: clamp, never reflect negative frequencies.
    const double freq = std::clamp (std::isfinite (freqHz) ? freqHz : 0.01, 0.01, 0.45 * rate);
    const double dt = freq / rate;
    const double width = std::clamp (std::isfinite (pw) ? (double) pw : 0.5, 0.02, 0.98);
    const bool syncing = syncFraction >= 0.0 && syncFraction < 1.0;

    // All shapes off is silence (02 § Waveforms), including a residual left over from a moment ago.
    const bool anyShape = shapes.saw || shapes.tri || shapes.pulse;
    double out = naive (phase, width, shapes) + (anyShape ? pending : 0.0);
    pending = 0.0;
    wrapped = false;

    if (! syncing)
    {
        correctEdges (out, phase, dt, 0.0, 1.0, width, shapes);
        phase = wrap01 (phase + dt);
        return (float) out;
    }

    // Hard sync (02-oscillators.md § Hard sync): run to the master's wrap, jump to phase 0, then
    // run the rest of the sample. The reset is corrected like any other discontinuity.
    correctEdges (out, phase, dt, 0.0, syncFraction, width, shapes);
    const double atReset = wrap01 (phase + syncFraction * dt);

    Step step;
    step.jump = naive (0.0, width, shapes) - naive (atReset, width, shapes);

    if (shapes.tri) // the triangle rises (+4 per cycle) from phase 0
        step.slope = (4.0 - (atReset < 0.5 ? 4.0 : -4.0)) * dt;

    correct (out, syncFraction, step);
    correctEdges (out, 0.0, dt, syncFraction, 1.0, width, shapes);
    phase = wrap01 ((1.0 - syncFraction) * dt);
    return (float) out;
}
} // namespace p5x::dsp
