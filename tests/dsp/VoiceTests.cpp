// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// The milestone 2 voice (06-voices.md § Voice structure).
#include "dsp/Voice.h"
#include "support/Spectrum.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>

using Catch::Approx;
using p5x::dsp::Voice;
using p5x::dsp::VoiceInputs;

namespace
{
constexpr double kRate = 96000.0;
constexpr int kMax = 4096;

// Constant per-sample parameter arrays with the 01-parameters.md defaults.
struct Params
{
    std::array<float, kMax> pitch {}, pwA {}, pwB {}, fine {}, mixA {}, mixB {}, noise {}, cutoff {}, res {}, envAmt {},
        fSus {}, aSus {};
    VoiceInputs in;

    Params()
    {
        pwA.fill (0.5f);
        pwB.fill (0.5f);
        mixA.fill (1.0f);
        mixB.fill (0.8f);
        cutoff.fill (4000.0f);
        envAmt.fill (0.4f);
        fSus.fill (0.4f);
        aSus.fill (1.0f);
        in.shapesA = { true, false, false };
        in.shapesB = { true, false, false };
        in.filterEnv = { 0.005f, 0.6f, 0.4f, 0.3f };
        in.ampEnv = { 0.002f, 0.5f, 1.0f, 0.2f };
        wire();
    }

    void wire()
    {
        in.pitchOffset = pitch.data();
        in.pwA = pwA.data();
        in.pwB = pwB.data();
        in.fineB = fine.data();
        in.mixA = mixA.data();
        in.mixB = mixB.data();
        in.mixNoise = noise.data();
        in.cutoffHz = cutoff.data();
        in.resonance = res.data();
        in.envAmount = envAmt.data();
        in.filterSustain = fSus.data();
        in.ampSustain = aSus.data();
    }
};

std::vector<float> render (Voice& v, const Params& p, size_t n)
{
    std::vector<float> out (n, 0.0f);

    for (size_t done = 0; done < n; done += kMax)
        v.render (out.data() + done, p.in, (int) std::min<size_t> (kMax, n - done));

    return out;
}

double dominantFrequency (const std::vector<float>& x)
{
    const auto mag = p5x::test::magnitudeSpectrum (x);
    size_t peak = 2;

    for (size_t k = 3; k < mag.size(); ++k)
        if (mag[k] > mag[peak])
            peak = k;

    return (double) peak * kRate / (double) x.size();
}
} // namespace

TEST_CASE ("Voice: idle is silent; note on sounds; release goes idle", "[dsp][voice]")
{
    Voice v;
    v.prepare (kRate, 1, 0);
    Params p;

    for (float s : render (v, p, 1024))
        REQUIRE (s == 0.0f);

    v.start (60, 1.0f);
    REQUIRE (p5x::test::rms (render (v, p, 9600)) > 0.01);

    v.release();
    REQUIRE (v.isReleasing());
    render (v, p, (size_t) (kRate * 0.5)); // 0.2 s release reaches idle at ~0.33 s
    REQUIRE (v.isIdle());
}

TEST_CASE ("Voice: output stays within the voice gain headroom", "[dsp][voice]")
{
    Voice v;
    v.prepare (kRate, 1, 0);
    Params p;
    p.res.fill (0.0f);
    v.start (48, 1.0f);

    float peak = 0.0f;

    for (float s : render (v, p, (size_t) kRate))
        peak = std::max (peak, std::abs (s));

    REQUIRE (peak > 0.02f);
    REQUIRE (peak <= Voice::kVoiceGain * 1.2f);
}

TEST_CASE ("Voice: oscillators free-run; note-on doesn't reset phase", "[dsp][voice]")
{
    Voice v;
    v.prepare (kRate, 7, 0);
    Params p;
    v.start (60, 1.0f);
    render (v, p, 1000);

    const double a = v.getOscAPhase(), b = v.getOscBPhase();
    v.start (64, 1.0f);
    REQUIRE (v.getOscAPhase() == a);
    REQUIRE (v.getOscBPhase() == b);
}

TEST_CASE ("Voice: same seed renders identically; different seed differs", "[dsp][voice]")
{
    auto renderWithSeed = [] (uint64_t seed)
    {
        Voice v;
        v.prepare (kRate, seed, 3);
        Params p;
        p.noise.fill (0.5f);
        v.start (57, 1.0f);
        return render (v, p, 20000);
    };

    REQUIRE (renderWithSeed (1) == renderWithSeed (1));
    REQUIRE (renderWithSeed (1) != renderWithSeed (2));
}

TEST_CASE ("Voice: Osc B with Kbd off plays C4 and ignores bend", "[dsp][voice]")
{
    Voice v;
    v.prepare (kRate, 1, 0);
    Params p;
    p.mixA.fill (0.0f);
    p.mixB.fill (1.0f);
    p.in.shapesB = { false, true, false }; // triangle: a clear fundamental
    p.cutoff.fill (20000.0f);
    p.envAmt.fill (0.0f);
    p.pitch.fill (2.0f); // a bend that Osc B must ignore
    p.in.oscBKeyboard = false;
    v.start (72, 1.0f);
    render (v, p, 9600);

    REQUIRE (dominantFrequency (render (v, p, 65536)) == Approx (261.63).margin (2.0));

    // Kbd on: follows the played note plus the bend.
    p.in.oscBKeyboard = true;
    render (v, p, 9600);
    REQUIRE (dominantFrequency (render (v, p, 65536)) == Approx (523.25 * std::pow (2.0, 2.0 / 12.0)).margin (2.0));
}

TEST_CASE ("Voice: filter keyboard tracking and envelope amount move the cutoff", "[dsp][voice]")
{
    Voice v;
    v.prepare (kRate, 1, 0);
    Params p;
    p.envAmt.fill (0.0f);
    p.cutoff.fill (1000.0f);

    p.in.keyboardTrack = 1.0f;
    v.start (72, 1.0f); // one octave above C4
    render (v, p, 256);
    REQUIRE (v.getCutoffHz() == Approx (2000.0f).epsilon (0.001));

    p.in.keyboardTrack = 0.5f;
    render (v, p, 256);
    REQUIRE (v.getCutoffHz() == Approx (1000.0f * std::sqrt (2.0f)).epsilon (0.001));

    // Envelope amount 1 at full filter envelope = +8 octaves (clamped at 0.45 × rate).
    p.in.keyboardTrack = 0.0f;
    p.envAmt.fill (1.0f);
    p.fSus.fill (1.0f);
    render (v, p, 9600);
    REQUIRE (v.getCutoffHz() == Approx (0.45f * (float) kRate).epsilon (0.001));
}

TEST_CASE ("Voice: kill cuts immediately", "[dsp][voice]")
{
    Voice v;
    v.prepare (kRate, 1, 0);
    Params p;
    v.start (60, 1.0f);
    render (v, p, 2000);
    v.kill();

    REQUIRE (v.isIdle());

    for (float s : render (v, p, 256))
        REQUIRE (s == 0.0f);
}
