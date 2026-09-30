// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

namespace p5x::dsp
{
// CEM3320 24 dB/oct low-pass (03-filter.md): four cascaded TPT one-pole stages with the resonance
// feedback loop solved exactly (zero-delay feedback, after Zavalishin), then tanh on the solved
// input ("cheap" nonlinearity) before running the stages. Passband loss with resonance is part of
// the character and is not compensated.
class LadderFilterCEM
{
public:
    void prepare (double internalRate) noexcept;
    void reset() noexcept; // clears state and queues the 1e-6 start-up excitation

    // cutoffHz is clamped to [5 Hz, 0.45 × rate]; res01 to [0, 1]. g is recomputed every sample.
    float process (float in, float cutoffHz, float res01) noexcept;

    static float feedbackFor (float res01) noexcept; // k = 4 · res^1.1 · 1.02

private:
    double rate = 96000.0;
    double s1 = 0.0, s2 = 0.0, s3 = 0.0, s4 = 0.0;
    double excitation = 0.0;
};
} // namespace p5x::dsp
