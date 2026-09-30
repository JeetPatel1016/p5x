# 03 · Filter (CEM3320 model)

24 dB/oct resonant low-pass, one per voice. The CEM3320 is four identical OTA one-pole stages with a resonance feedback loop. We model it as a zero-delay-feedback (ZDF/TPT) ladder with a soft nonlinearity.

## Interface (sketch)
```cpp
namespace p5x::dsp {
class LadderFilterCEM {
public:
    void prepare (double internalRate);
    void reset();
    float process (float in, float cutoffHz, float res01);   // per sample
};
}
```

## Cutoff computation (per sample, per voice)
Work in octaves relative to the cutoff knob:
```
octaves = 0
        + flt_env_amt * 8 * filterEnv(0..1) * velFactor          // 05-modulation.md
        + kbdTrack * (playedNote - 60) / 12                       // kbdTrack = 0, 0.5, 1; which note: OPEN_QUESTIONS #26
        + polyModFilterOctaves + wheelModFilterOctaves + vintageCutoffOffset
cutoffHz = clamp(flt_cutoff * 2^octaves, 5 Hz, 0.45 * internalRate)
```
- Keyboard tracking pivots at MIDI note 60 (C4): at C4 tracking has no effect. (P5X.)
- The envelope is unipolar; the hardware has no envelope inversion. Don't add one.

## Core algorithm
- Four cascaded TPT one-pole low-pass stages with `g = tan(pi * fc / fs)`, `G = g / (1 + g)`.
- Resonance feedback `k = 4.0 * res^1.1 * 1.02` (P5X: slightly above 4 at max so it self-oscillates reliably).
- Solve the linear feedback loop exactly (Zavalishin, *The Art of VA Filter Design*, ladder chapter): compute the instantaneous response of the cascade, solve for the feedback-summed input `u`.
- **Nonlinearity ("cheap" method):** apply `tanh` to the solved `u` (input stage saturation), then run the stages linearly. Do not iterate Newton-Raphson in milestone 2. A per-stage soft clipper may be added in milestone 4 only if listening tests need it (log it in DECISIONS.md).
- **Input drive:** filter input = mixer sum × 0.5 (P5X). This sets how early the tanh bites: a single saw at full mixer level should saturate only lightly.
- **Passband loss with resonance:** as `k` rises, low-frequency gain drops (≈ 1 / (1 + k)). This is characteristic of the 3320 and is **not compensated**. Don't add bass/gain compensation.

## Self-oscillation
- At `res = 1` the filter self-oscillates as a near-sine at the cutoff frequency.
- The tanh bounds the amplitude. The loop must settle to a stable amplitude (no growth, no blow-up) at any cutoff in [20 Hz, 20 kHz] and any sample rate.
- Self-oscillation must start from silence: inject a tiny deterministic excitation (1e-6 impulse) on `reset()` so a patch with no oscillators still whistles, like the hardware. (P5X.)

## Edge cases
- Cutoff modulated at audio rate (poly-mod from Osc B) must stay stable: recompute `g` every sample; no smoothing inside the filter.
- `tan` near Nyquist: guaranteed safe by the 0.45 × rate clamp.
- Denormals: states flushed by `ScopedNoDenormals`; also add ±1e-18 alternating offset to the state if tests show denormal CPU spikes.
- Sample-rate changes: `prepare` recomputes constants and resets state.

## Tests
- `res = 0`, fc = 1 kHz, 96 kHz: the −3 dB point of four cascaded identical one-poles sits at 0.435 × fc, so assert 435 Hz ± 5 %.
- Slope: attenuation between 2 kHz and 4 kHz is 21.2 dB ± 1 dB (four identical poles at fc = 1 kHz haven't reached the 24 dB/oct asymptote at 2–4 × fc).
- `res = 1`, fc in {100, 1 000, 8 000} Hz, no input after reset: output oscillates at fc ± 3 %, RMS stable to ±0.5 dB between 8 s and 9 s, no NaN. (From the 1e-6 impulse the model grows at ≈ 0.031 × fc per second, so start-up takes up to ~4.4 s at 100 Hz.)
- Passband gain at 100 Hz, fc = 1 kHz, res 0 vs res 0.9: drops by 13.1 dB ± 1 dB (the model's 1/(1+k) loss at k = 3.63; present, not compensated).
- Cutoff swept exponentially 20 Hz → 20 kHz → 20 Hz, one full sweep every 1 ms (1 000 sweeps per second), with res 0.95 and white-noise input: output bounded (|y| < 4), no NaN.
- White noise input, random cutoff/res each sample for 10 s: no NaN/Inf, |y| < 4.
