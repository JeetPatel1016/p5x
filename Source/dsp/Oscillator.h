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
// not normalised. Hard sync arrives with milestone 3.
class Oscillator
{
public:
    void prepare (double internalRate) noexcept;
    void reset (double initialPhase) noexcept; // phase in [0, 1)

    // One sample. freqHz already includes all modulation; it's clamped to [0.01 Hz, 0.45 × rate].
    // pw is clamped to [0.02, 0.98].
    float process (double freqHz, float pw, OscShapes shapes) noexcept;

    double getPhase() const noexcept { return phase; }

    // 2-sample polynomial residuals (Välimäki & Huovilainen), in sample units:
    // x in (-1, 0) is the sample before the discontinuity, x in [0, 1) the one after.
    static double blep (double x) noexcept;  // unit step
    static double blamp (double x) noexcept; // unit slope change (integral of blep)

private:
    double rate = 96000.0;
    double phase = 0.0;
};
} // namespace p5x::dsp
