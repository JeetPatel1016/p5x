// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
#include "dsp/Voice.h"

#include <algorithm>
#include <cmath>

namespace p5x::dsp
{
namespace
{
constexpr double kTwoPi = 6.283185307179586;
constexpr uint64_t kVintageStream = 2000; // + voice index; voices use 0–9, global sources 1000+

double noteToHz (double note) noexcept
{
    return 440.0 * std::exp2 ((note - 69.0) / 12.0);
}
} // namespace

void Voice::Drift::step (Random& r, double size, double lowPass) noexcept
{
    walk += r.nextBipolar() * size;

    // Reflect at the bounds so the walk stays within ±kDriftCents.
    if (walk > kDriftCents)
        walk = 2.0 * kDriftCents - walk;
    else if (walk < -kDriftCents)
        walk = -2.0 * kDriftCents - walk;

    value += (walk - value) * lowPass;
}

void Voice::prepare (double internalRate, uint64_t seed, int voiceIndex, int oversampling) noexcept
{
    rate = std::max (1.0, internalRate);
    random.setSeed (Random::derive (seed, (uint64_t) voiceIndex));
    vintageRandom.setSeed (Random::derive (seed, kVintageStream + (uint64_t) voiceIndex));

    oscA.prepare (rate);
    oscB.prepare (rate);
    oscA.reset (random.nextDouble()); // analog VCOs free-run: random start phase (02-oscillators.md)
    oscB.reset (random.nextDouble());
    filter.prepare (rate);
    filterEnvelope.prepare (rate);
    ampEnvelope.prepare (rate);
    pressure.prepare (rate, 20.0); // aftertouch: Lin 20 (05-modulation.md § Aftertouch)

    // Vintage offsets at Vintage = 1, drawn once (06-voices.md § Vintage).
    vintageDetuneA = vintageRandom.nextBipolar() * kVintageDetuneCents;
    vintageDetuneB = vintageRandom.nextBipolar() * kVintageDetuneCents;
    vintageCutoff = vintageRandom.nextBipolar() * kVintageCutoffOctaves;
    vintageEnv = vintageRandom.nextBipolar() * kVintageEnvScale;

    // Drift: steps whose spread after kDriftCrossSeconds is about the full range (2 × kDriftCents);
    // a uniform step in [−a, a] has standard deviation a / √3.
    const double hostRate = rate / std::max (1, oversampling);
    const double sigma = 2.0 * kDriftCents / std::sqrt (kDriftCrossSeconds * hostRate);
    driftStepSize = sigma * std::sqrt (3.0);
    driftLowPass = 1.0 - std::exp (-kTwoPi * kDriftLowPassHz / hostRate);
    driftEvery = std::max (1, oversampling);
    driftCountdown = 0;
    driftA = {};
    driftB = {};

    hasPlayed = false;
    pitch = 60.0;
    glideRemaining = 0;
    unisonCents = 0.0;
    unisonGain = 1.0f;
    reset();
}

void Voice::reset() noexcept
{
    filterEnvelope.reset();
    ampEnvelope.reset();
    filter.reset();
    pressure.reset (0.0f);
    lastOscAHz = lastOscBHz = lastCutoffHz = 0.0f;
}

void Voice::beginGlide (double from, int target, double glideSeconds) noexcept
{
    note = target;
    const int samples = (int) std::lround (glideSeconds * rate);

    // Constant time, linear in semitones (06-voices.md § Glide).
    if (! hasPlayed || samples <= 0 || from == (double) target)
    {
        pitch = (double) target;
        glideRemaining = 0;
    }
    else
    {
        pitch = from;
        glideStep = ((double) target - from) / (double) samples;
        glideRemaining = samples;
    }

    hasPlayed = true;
}

void Voice::start (int midiNote, float velocity01, double glideSeconds, double fromNote) noexcept
{
    // Oscillators never reset phase on note-on (02-oscillators.md § Free-running phase).
    beginGlide (fromNote >= 0.0 ? fromNote : pitch, midiNote, glideSeconds);
    velocity = velocity01;
    pressure.setTarget (0.0f); // a new note starts with no poly pressure
    filterEnvelope.gateOn();
    ampEnvelope.gateOn();
}

void Voice::glideTo (int midiNote, double glideSeconds) noexcept
{
    beginGlide (pitch, midiNote, glideSeconds);
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

    // Velocity (05-modulation.md § Velocity): v = 1 when off.
    const float ampVelocity = in.velocityOn ? 0.25f + 0.75f * velocity : 1.0f;
    const float filterVelocity = in.velocityOn ? 0.5f + 0.5f * velocity : 1.0f;

    EnvParams filterParams = in.filterEnv;
    EnvParams ampParams = in.ampEnv;

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Glide → current note.
        if (glideRemaining > 0)
            pitch = --glideRemaining == 0 ? (double) note : pitch + glideStep;

        // Vintage drift moves at the host rate (06-voices.md § Vintage).
        if (--driftCountdown < 0)
        {
            driftCountdown = driftEvery - 1;
            driftA.step (vintageRandom, driftStepSize, driftLowPass);
            driftB.step (vintageRandom, driftStepSize, driftLowPass);
        }

        // Modulation sources for this sample. Destinations that are off add nothing, and a source at
        // zero adds exactly zero, so both render bit-identically to no modulation (05 § Tests). The
        // same holds for Vintage 0, unison off and velocity off (06 § Tests).
        const float vintage = in.vintage[i];
        const double offsetA = (unisonCents + vintage * (vintageDetuneA + driftA.value)) * 0.01;
        const double offsetB = (unisonCents + vintage * (vintageDetuneB + driftB.value)) * 0.01;
        const float envScale = 1.0f + vintage * (float) vintageEnv;

        // Aftertouch raises the Wheel-Mod amount: max(wheel, pressure) (05 § Aftertouch).
        const float voicePressure = pressure.next();
        float amount = in.wheelModAmount[i];

        if (in.aftertouchOn)
            amount = std::max (amount, std::max (in.channelPressure[i], voicePressure));

        const float wm = in.wheelModSource[i] * amount;

        // The filter envelope feeds both the filter and Poly-Mod (after velocity scaling).
        filterParams.attackS = in.filterEnv.attackS * envScale;
        filterParams.decayS = in.filterEnv.decayS * envScale;
        filterParams.releaseS = in.filterEnv.releaseS * envScale;
        filterParams.sustain = in.filterSustain[i];
        const float filterEnv = filterEnvelope.process (filterParams) * filterVelocity;

        // 2. Osc B. With Kbd off it plays a fixed C4 and ignores bend and glide (02-oscillators.md § Pitch).
        double noteB = in.oscBKeyboard ? pitch + in.oscBFreq + in.fineB[i] * 0.01 + in.pitchOffset[i]
                                       : 60.0 + in.oscBFreq + in.fineB[i] * 0.01 + in.masterTune[i];
        noteB += offsetB;
        float pwB = in.pwB[i];

        if (in.wmFreqB)
            noteB += kWheelModSemitones * wm;

        if (in.wmPwB)
            pwB += kModPulseWidth * wm;

        double freqB = noteToHz (noteB);

        if (in.oscBLoFreq)
            freqB /= kLoFreqDivisor;

        const float b = oscB.process (freqB, pwB, in.shapesB);

        // 3. Poly-Mod: audio rate, from B's raw summed output (05-modulation.md § Poly-Mod).
        const float pm = in.pmFilterEnv[i] * filterEnv + in.pmOscB[i] * b;

        // 4. Osc A, hard-synced to B's wrap when Sync is on.
        double noteA = pitch + in.oscAFreq + in.pitchOffset[i];
        noteA += offsetA;
        float pwA = in.pwA[i];

        if (in.wmFreqA)
            noteA += kWheelModSemitones * wm;

        if (in.pmFreqA)
            noteA += kPolyModSemitones * pm;

        if (in.wmPwA)
            pwA += kModPulseWidth * wm;

        if (in.pmPwA)
            pwA += kModPulseWidth * pm;

        const double freqA = noteToHz (noteA);
        const double syncAt = in.syncA && oscB.wrappedThisSample() ? oscB.wrapFraction() : -1.0;
        const float a = oscA.process (freqA, pwA, in.shapesA, syncAt);

        // 5. Mixer
        float mix = a * in.mixA[i] + b * in.mixB[i] + random.nextBipolar() * in.mixNoise[i] * kNoiseLevel;

        if (in.externalInput != nullptr)
            mix += in.externalInput[i];

        // 6. Filter; cutoff in octaves relative to the knob (03-filter.md § Cutoff computation).
        // Keyboard tracking follows the glided note, not bend (DECISIONS.md, OPEN_QUESTIONS #26).
        double octaves = in.envAmount[i] * 8.0 * filterEnv + in.keyboardTrack * (pitch - 60.0) / 12.0;
        octaves += vintage * vintageCutoff;

        if (in.wmFilter)
            octaves += kWheelModOctaves * wm;

        if (in.pmFilter)
            octaves += kPolyModOctaves * pm;

        const auto cutoff = (float) (in.cutoffHz[i] * std::exp2 (octaves));
        const float filtered = filter.process (mix * kFilterDrive, cutoff, in.resonance[i]);

        // 7. VCA: × amp envelope × velocity gain.
        ampParams.attackS = in.ampEnv.attackS * envScale;
        ampParams.decayS = in.ampEnv.decayS * envScale;
        ampParams.releaseS = in.ampEnv.releaseS * envScale;
        ampParams.sustain = in.ampSustain[i];
        const float amplified = filtered * ampEnvelope.process (ampParams) * ampVelocity;

        // 8. Into the shared oversampled buffer (the DC blocker runs once on the sum: 06 § Output stage).
        out[i] += amplified * kVoiceGain * unisonGain;

        if (i == numSamples - 1 || ampEnvelope.isIdle())
        {
            lastOscAHz = (float) std::clamp (freqA, 0.01, 0.45 * rate);
            lastOscBHz = (float) std::clamp (freqB, 0.01, 0.45 * rate);
            lastCutoffHz = (float) std::clamp ((double) cutoff, 5.0, 0.45 * rate);
        }

        if (ampEnvelope.isIdle())
            break;
    }
}
} // namespace p5x::dsp
