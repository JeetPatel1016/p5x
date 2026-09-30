// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#pragma once

namespace p5x::dsp
{
struct EnvParams
{
    float attackS = 0.002f, decayS = 0.5f, sustain = 1.0f, releaseS = 0.2f;
};

// CEM3310 ADSR model (04-envelopes.md): one-pole RC segments toward a target. Attack charges toward
// 1.3 (convex curve) and ends at 1.0; decay and release are "time to within 60 dB". Time parameters
// are read every sample; gates retrigger from the current level.
class Envelope
{
public:
    enum class Stage
    {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    void prepare (double internalRate) noexcept;
    void reset() noexcept;

    void gateOn() noexcept;
    void gateOff() noexcept;

    float process (const EnvParams& params) noexcept;

    Stage stage() const noexcept { return current; }
    bool isIdle() const noexcept { return current == Stage::Idle; }
    float getLevel() const noexcept { return (float) level; }

private:
    // coefficient = 1 - exp(-1 / (tau · rate)), cached per stage until the time changes.
    struct Coefficient
    {
        float forSeconds = -1.0f;
        double value = 0.0;
    };

    double coefficient (Coefficient& cache, float seconds, double ln) noexcept;

    double rate = 96000.0;
    double level = 0.0;
    Stage current = Stage::Idle;
    Coefficient attackCoeff, decayCoeff, releaseCoeff;
};
} // namespace p5x::dsp
