// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
// The voice (06-voices.md § Voice structure), with the milestone 3 modulation (05-modulation.md).
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
        fSus {}, aSus {}, tune {}, wmSource {}, wmAmount {}, pmFilterEnv {}, pmOscB {};
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
        in.masterTune = tune.data();
        in.wheelModSource = wmSource.data();
        in.wheelModAmount = wmAmount.data();
        in.pmFilterEnv = pmFilterEnv.data();
        in.pmOscB = pmOscB.data();
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

TEST_CASE ("Voice: Osc B with Kbd off plays C4, ignores bend, follows master tune", "[dsp][voice]")
{
    Voice v;
    v.prepare (kRate, 1, 0);
    Params p;
    p.mixA.fill (0.0f);
    p.mixB.fill (1.0f);
    p.in.shapesB = { false, true, false }; // triangle: a clear fundamental
    p.cutoff.fill (20000.0f);
    p.envAmt.fill (0.0f);
    p.tune.fill (1.0f);  // master tune +1 semitone: applies with Kbd off too (02-oscillators.md § Pitch)
    p.pitch.fill (3.0f); // bend +2 (which Kbd off must ignore) + tune +1
    p.in.oscBKeyboard = false;
    v.start (72, 1.0f);
    render (v, p, 9600);

    REQUIRE (dominantFrequency (render (v, p, 65536)) == Approx (261.63 * std::pow (2.0, 1.0 / 12.0)).margin (2.0));

    // Kbd on: follows the played note plus bend and tune.
    p.in.oscBKeyboard = true;
    render (v, p, 9600);
    REQUIRE (dominantFrequency (render (v, p, 65536)) == Approx (523.25 * std::pow (2.0, 3.0 / 12.0)).margin (2.0));
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

//==================================================================================================
// Milestone 3: modulation inside the voice (05-modulation.md, 02-oscillators.md).

TEST_CASE ("Voice: Lo Freq divides Osc B's frequency by 128", "[dsp][voice][mod]")
{
    // Measured from B's phase advance: the DC blocker would hide a 2 Hz waveform (02 § Tests).
    auto measuredHz = [] (bool keyboard, int note)
    {
        Voice v;
        v.prepare (kRate, 1, 0);
        Params p;
        p.in.oscBLoFreq = true;
        p.in.oscBKeyboard = keyboard;
        v.start (note, 1.0f);
        render (v, p, 1000);

        const double before = v.getOscBPhase();
        constexpr size_t n = 24000; // 0.25 s: less than one cycle at these rates
        render (v, p, n);
        double advance = v.getOscBPhase() - before;
        advance -= std::floor (advance);
        return advance * kRate / (double) n;
    };

    REQUIRE (measuredHz (false, 72) == Approx (261.6256 / 128.0).epsilon (0.001)); // Kbd off: C4 / 128 = 2.04 Hz
    REQUIRE (measuredHz (true, 69) == Approx (440.0 / 128.0).epsilon (0.001));
}

TEST_CASE ("Voice: Poly-Mod Osc B -> Freq A makes FM sidebands at A +- n x 200 Hz", "[dsp][voice][mod]")
{
    // B at 200 Hz (triangle), A a fifth above; Poly-Mod amount 0.3 from Osc B into Freq A.
    const float toB200 = (float) (12.0 * std::log2 (200.0 / 261.6256)); // bend so note 60's B sits at 200 Hz

    auto spectrum = [&] (bool polyMod)
    {
        Voice v;
        v.prepare (kRate, 3, 0);
        Params p;
        p.pitch.fill (toB200);
        p.in.oscAFreq = 7;
        p.in.shapesB = { false, true, false };
        p.mixB.fill (0.0f);
        p.cutoff.fill (20000.0f);
        p.envAmt.fill (0.0f);
        p.pmOscB.fill (0.3f);
        p.in.pmFreqA = polyMod;
        v.start (60, 1.0f);
        render (v, p, 9600);
        return p5x::test::magnitudeSpectrum (render (v, p, 65536));
    };

    const double binHz = kRate / 65536.0;
    const auto withPm = spectrum (true);
    const auto without = spectrum (false);

    // Carrier: the strongest line with Poly-Mod on (its centre moves: exponential FM raises the mean pitch).
    size_t carrier = 10;

    for (size_t k = 11; k < (size_t) (3000.0 / binHz); ++k)
        if (withPm[k] > withPm[carrier])
            carrier = k;

    const double top = withPm[carrier];

    for (int n : { -1, 1, 2, 3 })
    {
        const double bin = (double) carrier + n * 200.0 / binHz;
        INFO ("carrier " << (double) carrier * binHz << " Hz, sideband n = " << n);
        REQUIRE (p5x::test::toDb (p5x::test::peakAround (withPm, bin, 3) / top) > -40.0);
    }

    // Without Poly-Mod the spectrum near A has no lines 200 Hz from its own peak.
    size_t plainPeak = 10;

    for (size_t k = 11; k < (size_t) (3000.0 / binHz); ++k)
        if (without[k] > without[plainPeak])
            plainPeak = k;

    REQUIRE (p5x::test::toDb (p5x::test::peakAround (without, (double) plainPeak + 200.0 / binHz, 3) / without[plainPeak]) < -40.0);
}

TEST_CASE ("Voice: Poly-Mod amount 0 with destinations on is bit-identical to them off", "[dsp][voice][mod]")
{
    auto renderWith = [] (bool destinations)
    {
        Voice v;
        v.prepare (kRate, 9, 2);
        Params p;
        p.in.shapesB = { true, true, true };
        p.in.shapesA = { true, false, true };
        p.in.pmFreqA = p.in.pmPwA = p.in.pmFilter = destinations; // amounts stay 0
        v.start (55, 1.0f);
        return render (v, p, 20000);
    };

    REQUIRE (renderWith (true) == renderWith (false));
}

TEST_CASE ("Voice: Wheel-Mod amount 0 with destinations on is bit-identical to them off", "[dsp][voice][mod]")
{
    auto renderWith = [] (bool destinations)
    {
        Voice v;
        v.prepare (kRate, 9, 2);
        Params p;
        p.in.shapesA = { true, false, true };
        p.in.shapesB = { false, false, true };

        for (size_t i = 0; i < p.wmSource.size(); ++i) // a moving source, wheel at 0
            p.wmSource[i] = (float) std::sin (0.01 * (double) i);

        p.in.wmFreqA = p.in.wmFreqB = p.in.wmPwA = p.in.wmPwB = p.in.wmFilter = destinations;
        v.start (55, 1.0f);
        return render (v, p, 20000);
    };

    REQUIRE (renderWith (true) == renderWith (false));
}

TEST_CASE ("Voice: Wheel-Mod depths at full source", "[dsp][voice][mod]")
{
    Voice v;
    v.prepare (kRate, 1, 0);
    Params p;
    p.envAmt.fill (0.0f);
    p.cutoff.fill (1000.0f);
    p.wmSource.fill (-1.0f);
    p.wmAmount.fill (1.0f);
    p.in.wmFreqA = true;
    p.in.wmFreqB = true;
    p.in.wmFilter = true;
    v.start (72, 1.0f);
    render (v, p, 256);

    REQUIRE (v.getOscAHz() == Approx (261.6256).epsilon (1e-4)); // C5 − 12 st
    REQUIRE (v.getOscBHz() == Approx (261.6256).epsilon (1e-4));
    REQUIRE (v.getCutoffHz() == Approx (1000.0 / 16.0).epsilon (1e-4)); // −4 octaves
}

TEST_CASE ("Voice: Poly-Mod filter envelope depths", "[dsp][voice][mod]")
{
    // Filter env held at sustain 1 → pm = pm_filt_env; Freq A +48 st × pm, cutoff +5 oct × pm.
    Voice v;
    v.prepare (kRate, 1, 0);
    Params p;
    p.envAmt.fill (0.0f);
    p.fSus.fill (1.0f);
    p.cutoff.fill (100.0f);
    p.pmFilterEnv.fill (0.25f);
    p.in.pmFreqA = true;
    p.in.pmFilter = true;
    v.start (48, 1.0f);
    render (v, p, 9600);

    REQUIRE (v.getOscAHz() == Approx (130.8128 * 2.0).epsilon (1e-3));             // C3 + 12 st
    REQUIRE (v.getCutoffHz() == Approx (100.0 * std::exp2 (1.25)).epsilon (1e-3)); // + 1.25 octaves
}
