// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/Voice.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace p5x::dsp
{
namespace
{
double noteToHz (double note) noexcept
{
    return 440.0 * std::exp2 ((note - 69.0) / 12.0);
}
} // namespace

void Voice::prepare (double internalRate, uint64_t seed, int voiceIndex) noexcept
{
    rate = std::max (1.0, internalRate);
    random.setSeed (Random::derive (seed, (uint64_t) voiceIndex));

    oscA.prepare (rate);
    oscB.prepare (rate);
    oscA.reset (random.nextDouble()); // analog VCOs free-run: random start phase (02-oscillators.md)
    oscB.reset (random.nextDouble());
    filter.prepare (rate);
    filterEnvelope.prepare (rate);
    ampEnvelope.prepare (rate);

    dcCoeff = std::exp (-2.0 * std::numbers::pi * kDcBlockerHz / rate);
    reset();
}

void Voice::reset() noexcept
{
    filterEnvelope.reset();
    ampEnvelope.reset();
    filter.reset();
    dcIn = dcOut = 0.0;
    lastOscAHz = lastCutoffHz = 0.0f;
}

void Voice::start (int midiNote, float velocity01) noexcept
{
    // Oscillators never reset phase on note-on (02-oscillators.md § Free-running phase).
    note = midiNote;
    velocity = velocity01;
    filterEnvelope.gateOn();
    ampEnvelope.gateOn();
}

void Voice::release() noexcept
{
    filterEnvelope.gateOff();
    ampEnvelope.gateOff();
}

void Voice::kill() noexcept
{
    reset();
}

void Voice::render (float* out, const VoiceInputs& in, int numSamples) noexcept
{
    if (ampEnvelope.isIdle())
        return; // idle voices don't advance their oscillators (02-oscillators.md § Edge cases)

    const double playedNote = (double) note; // after glide (milestone 4)
    const double keyboardOctaves = in.keyboardTrack * (playedNote - 60.0) / 12.0; // 03-filter.md
    const float velocityFactor = 1.0f; // velocity arrives in milestone 4 (05-modulation.md)

    EnvParams filterParams = in.filterEnv;
    EnvParams ampParams = in.ampEnv;

    for (int i = 0; i < numSamples; ++i)
    {
        // 2. Osc B. With Kbd off it plays a fixed C4 and ignores bend and master tune (02-oscillators.md).
        const double noteB = in.oscBKeyboard ? playedNote + in.oscBFreq + in.fineB[i] * 0.01 + in.pitchOffset[i]
                                             : 60.0 + in.oscBFreq + in.fineB[i] * 0.01;
        const float b = oscB.process (noteToHz (noteB), in.pwB[i], in.shapesB);

        // 4. Osc A (Poly-Mod, Wheel-Mod and sync arrive in milestone 3).
        const double freqA = noteToHz (playedNote + in.oscAFreq + in.pitchOffset[i]);
        const float a = oscA.process (freqA, in.pwA[i], in.shapesA);

        // 5. Mixer
        float mix = a * in.mixA[i] + b * in.mixB[i] + random.nextBipolar() * in.mixNoise[i] * kNoiseLevel;

        if (in.externalInput != nullptr)
            mix += in.externalInput[i];

        // 6. Filter; cutoff in octaves relative to the knob (03-filter.md § Cutoff computation).
        filterParams.sustain = in.filterSustain[i];
        const float filterEnv = filterEnvelope.process (filterParams) * velocityFactor;
        const double octaves = in.envAmount[i] * 8.0 * filterEnv + keyboardOctaves;
        const auto cutoff = (float) (in.cutoffHz[i] * std::exp2 (octaves));
        const float filtered = filter.process (mix * kFilterDrive, cutoff, in.resonance[i]);

        // 7. VCA
        ampParams.sustain = in.ampSustain[i];
        const float amplified = filtered * ampEnvelope.process (ampParams);

        // 8. DC blocker: one-pole high-pass at 5 Hz.
        dcOut = amplified - dcIn + dcCoeff * dcOut;
        dcIn = amplified;

        // 9. Into the shared oversampled buffer.
        out[i] += (float) dcOut * kVoiceGain;

        if (i == numSamples - 1 || ampEnvelope.isIdle())
        {
            lastOscAHz = (float) std::min (freqA, 0.45 * rate);
            lastCutoffHz = (float) std::clamp ((double) cutoff, 5.0, 0.45 * rate);
        }

        if (ampEnvelope.isIdle())
            break;
    }
}
} // namespace p5x::dsp
