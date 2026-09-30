// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/Voice.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace p5x::dsp
{
void Voice::prepare (double newSampleRate) noexcept
{
    sampleRate = std::max (1.0, newSampleRate);
    attackStep = static_cast<float> (1.0 / (kAttackSeconds * sampleRate));
    releaseStep = static_cast<float> (1.0 / (kReleaseSeconds * sampleRate));
    reset();
}

void Voice::reset() noexcept
{
    stage = Stage::Idle;
    level = 0.0f;
}

void Voice::start (int midiNote, float /*velocity01: ignored until milestone 4*/) noexcept
{
    note = midiNote;
    stage = level >= 1.0f ? Stage::Sustain : Stage::Attack;
}

void Voice::release() noexcept
{
    if (stage != Stage::Idle)
        stage = Stage::Release;
}

void Voice::kill() noexcept
{
    reset();
}

void Voice::render (float* out, const float* pitchOffsetSemis, int numSamples) noexcept
{
    if (stage == Stage::Idle)
        return;

    constexpr double twoPi = 2.0 * std::numbers::pi;
    const double maxFreq = 0.45 * sampleRate; // see 00-architecture.md § Real-time rules
    const double invRate = 1.0 / sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        if (stage == Stage::Attack)
        {
            level += attackStep;

            if (level >= 1.0f)
            {
                level = 1.0f;
                stage = Stage::Sustain;
            }
        }
        else if (stage == Stage::Release)
        {
            level -= releaseStep;

            if (level <= 0.0f)
            {
                reset();
                return;
            }
        }

        const double semis = static_cast<double> (note) + static_cast<double> (pitchOffsetSemis[i]) - 69.0;
        const double freq = std::clamp (440.0 * std::exp2 (semis / 12.0), 0.01, maxFreq);

        out[i] += static_cast<float> (std::sin (twoPi * phase)) * level * kVoiceGain;

        phase += freq * invRate;
        phase -= std::floor (phase);
    }
}
} // namespace p5x::dsp
