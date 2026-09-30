// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

namespace p5x::dsp
{
struct OscShapes
{
    bool saw = false, tri = false, pulse = false; // tri ignored for Osc A
};

// CEM3340 VCO model (02-oscillators.md): saw, pulse and triangle from one double-precision phase,
// band-limited with PolyBLEP (steps) and PolyBLAMP (corners). Shapes are independent and summed,
// not normalised.
//
// Every discontinuity between this sample and the next (wrap, pulse edge, triangle corner, sync
// reset) is found with its sub-sample position; its residual is split between this sample and the
// next one (carried in `pending`). That's what lets a hard-sync reset at any point in the sample be
// corrected like a natural wrap.
class Oscillator
{
public:
    void prepare (double internalRate) noexcept;
    void reset (double initialPhase) noexcept; // phase in [0, 1); clears any carried residual

    // One sample. freqHz already includes all modulation; it's clamped to [0.01 Hz, 0.45 × rate].
    // pw is clamped to [0.02, 0.98]. syncFraction in [0, 1) hard-syncs this oscillator: the master
    // wrapped that far into this sample (02-oscillators.md § Hard sync); negative = no sync.
    float process (double freqHz, float pw, OscShapes shapes, double syncFraction = -1.0) noexcept;

    // Sync master side: did the phase wrap during the last process() call, and where (0..1)?
    bool wrappedThisSample() const noexcept { return wrapped; }
    double wrapFraction() const noexcept { return wrapAt; }

    double getPhase() const noexcept { return phase; }

    // 2-sample polynomial residuals (Välimäki & Huovilainen), in sample units:
    // x in (-1, 0) is the sample before the discontinuity, x in [0, 1) the one after.
    static double blep (double x) noexcept;  // unit step
    static double blamp (double x) noexcept; // unit slope change (integral of blep)

private:
    struct Step
    {
        double jump = 0.0;  // value change
        double slope = 0.0; // slope change, per sample
    };

    // Adds the residual of a discontinuity `f` (0..1) into the current step to `out` and `pending`.
    void correct (double& out, double f, Step step) noexcept;

    // Corrections for the natural wraps and edges met while the phase runs from `start` for the part
    // of this step between fractions `from` and `to`.
    void correctEdges (double& out, double start, double dt, double from, double to, double width,
                       OscShapes shapes) noexcept;

    double rate = 96000.0;
    double phase = 0.0;
    double pending = 0.0; // residual owed to the next sample
    double wrapAt = 0.0;
    bool wrapped = false;
};
} // namespace p5x::dsp
