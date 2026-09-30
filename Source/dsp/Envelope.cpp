// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/Envelope.h"

#include <algorithm>
#include <cmath>

namespace p5x::dsp
{
namespace
{
constexpr double kAttackTarget = 1.3;
const double kLnAttack = std::log (1.3 / 0.3); // attack from 0 reaches 1.0 in attackS
const double kLn60dB = std::log (1000.0);        // decay/release: within 60 dB of target in timeS
constexpr double kSustainReached = 1.0e-4;
constexpr double kIdleLevel = 1.0e-5;
} // namespace

void Envelope::prepare (double internalRate) noexcept
{
    rate = std::max (1.0, internalRate);
    attackCoeff = {};
    decayCoeff = {};
    releaseCoeff = {};
    reset();
}

void Envelope::reset() noexcept
{
    current = Stage::Idle;
    level = 0.0;
}

void Envelope::gateOn() noexcept
{
    current = Stage::Attack; // from the current level: no reset, no click (04-envelopes.md § Gate behaviour)
}

void Envelope::gateOff() noexcept
{
    if (current != Stage::Idle)
        current = Stage::Release;
}

double Envelope::coefficient (Coefficient& cache, float seconds, double ln) noexcept
{
    if (seconds != cache.forSeconds)
    {
        const double tau = std::max ((double) seconds, 1.0e-6) / ln;
        cache.value = 1.0 - std::exp (-1.0 / (tau * rate));
        cache.forSeconds = seconds;
    }

    return cache.value;
}

float Envelope::process (const EnvParams& p) noexcept
{
    const double sustain = std::clamp ((double) p.sustain, 0.0, 1.0);

    switch (current)
    {
        case Stage::Idle:
            return 0.0f;

        case Stage::Attack:
            level += (kAttackTarget - level) * coefficient (attackCoeff, p.attackS, kLnAttack);

            if (level >= 1.0)
            {
                level = 1.0;
                current = Stage::Decay;
            }
            break;

        case Stage::Decay:
            level += (sustain - level) * coefficient (decayCoeff, p.decayS, kLn60dB);

            if (std::abs (level - sustain) < kSustainReached)
                current = Stage::Sustain;
            break;

        case Stage::Sustain:
            level = sustain; // follows the (upstream-smoothed) sustain parameter
            break;

        case Stage::Release:
            level += (0.0 - level) * coefficient (releaseCoeff, p.releaseS, kLn60dB);

            if (level < kIdleLevel)
            {
                level = 0.0;
                current = Stage::Idle;
            }
            break;
    }

    return (float) level;
}
} // namespace p5x::dsp
