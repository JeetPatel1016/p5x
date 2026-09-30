# 02 · Oscillators and noise

Models the CEM3340 VCO as used on the Rev 3 voice card: two per voice (A, B), plus one white noise source per voice.

## Interface (sketch)
```cpp
namespace p5x::dsp {
struct OscShapes { bool saw, tri, pulse; };            // tri ignored for OSC A
class Oscillator {
public:
    void prepare (double internalRate);
    void reset (double initialPhase);                  // phase in [0,1)
    // Returns one sample. freqHz already includes all modulation. pw in [0.02, 0.98].
    // Hard sync: run the master (B) first; if it wrapped this sample, pass its wrapFraction()
    // (0..1 position of the wrap within the sample) as syncFraction, else a negative value.
    float process (double freqHz, float pw, OscShapes shapes, double syncFraction = -1.0);
    bool wrappedThisSample() const;                    // for use as sync master
    double wrapFraction() const;
};
}
```

## Pitch
Per sample, for each voice:
```
noteA = playedNote (after glide) + osc_a_freq + bend + masterTune/100
      + unisonDetune/100 + (vintageDetuneA + vintageDriftA)/100 + wheelMod/polyMod pitch offsets
noteB = playedNote (after glide) + osc_b_freq + osc_b_fine/100 + bend + masterTune/100
      + unisonDetune/100 + (vintageDetuneB + vintageDriftB)/100 + wheelMod pitch offset
freqX = 440 * 2^((noteX - 69) / 12)          // then ÷128 for B when Lo Freq is on
// detunes and drift are in cents (06-voices.md); Poly-Mod pitch goes to A only (05-modulation.md)
```
- `osc_a_freq` / `osc_b_freq` are whole semitones (quantized knob, like the hardware DAC steps).
- `osc_b_fine` adds cents to B only.
- **Osc B Kbd off:** B's `playedNote` is replaced with a fixed reference note 60 (C4), and B also ignores bend and glide. Master tune still applies (it feeds every VCO on the hardware), as do the other offsets (`osc_b_freq`, fine, unison, Vintage, Wheel-Mod).
- **Osc B Lo Freq on:** B's final frequency is divided by 128 (7 octaves down). Anti-aliasing is unnecessary at that rate but the waveshape code path stays the same. With Kbd off and all offsets 0, B runs at 261.63 / 128 = 2.04 Hz.
- Clamp final frequency to [0.01 Hz, 0.45 × internalRate].

## Waveforms
All waveforms are generated from a `double` phase in [0, 1) advancing by `freq / internalRate` per sample.

| Shape | Formula (naive) | Anti-aliasing | Level |
|---|---|---|---|
| Saw | `2*phase - 1` (rising ramp) | PolyBLEP at wrap | ±1 |
| Pulse | `phase < pw ? +1 : -1` | PolyBLEP at both edges | ±1 |
| Triangle (B only) | integrate-free: `1 - 4*abs(phase - 0.5)` | PolyBLAMP at both corners | ±1 |

- **Shape switches are independent and summed**, like the hardware: A with saw + pulse on = saw + pulse. All switches off = silence (not an error). Sum is not normalised; the mixer and filter drive handle level.
- PW changes take effect immediately per sample (PWM must be smooth; smoothing is upstream).
- PolyBLEP: standard 2-sample polynomial residual (Välimäki & Huovilainen). Implement from the paper, not from any GPL source.

## Hard sync (A to B)
- When `osc_a_sync` is on and B wraps during a sample, A's phase resets to `(1 - bWrapFraction) * incA` and a BLEP of A's pre-reset discontinuity is applied at that fractional position.
- Sync is evaluated at the internal (oversampled) rate every sample.
- B is always the master; B's triangle/pulse are unaffected.

## Free-running phase
- Oscillators **never reset phase on note-on** (analog VCOs free-run). This matters for the sound of stacked voices.
- On `prepare`, each oscillator gets a random initial phase from the voice's seeded `Random`.

## Noise
- White noise, uniform in [−1, 1] from the seeded `Random`, one generator per voice. Level into mixer: `mix_noise * 0.7` (P5X, keeps noise from dominating).
- Wheel-Mod uses a separate global noise source (see 05-modulation.md).

## Edge cases
- Frequency jumps (large poly-mod FM) must not produce NaN or phase outside [0,1): wrap with `phase -= floor(phase)`.
- Negative frequency from extreme modulation is clamped to 0.01 Hz, not reflected (P5X; the hardware exp converter can't go negative).
- An idle voice does **not** advance its oscillators (CPU saving); on reactivation it continues from the stored phase. (Drift of free-running is inaudible here.)

## Tests
- Saw at fundamentals from 1 kHz up to C8 (4186 Hz), 96 kHz internal, through the 2× downsampler: measure aliasing products relative to the fundamental via FFT after downsampling. The threshold is what the specified model (2-sample PolyBLEP at 2×) achieves: measure it in milestone 2, set the test just below it and record the value in DECISIONS.md (−80 dB was an estimate the model may not reach). The test guards against regressions; it doesn't change the model.
- Pulse at PW 0.5: no DC beyond ±0.01 after DC blocker; PW 0.05 and 0.95: audible, non-silent, peak ≤ 1.1.
- Triangle: spectrum odd harmonics only, 12 dB/oct rolloff ±2 dB for first 5 harmonics.
- Sync: A at 3× B frequency with sync on: A's output period equals B's period ±1 sample.
- Lo Freq: B frequency = expected / 128 within 0.1%.
- Phase continuity: two consecutive note-ons don't reset phase (compare phase before/after).
- No NaN/Inf with random frequencies in [−1e6, 1e6] and PW in [−1, 2] (inputs get clamped).
