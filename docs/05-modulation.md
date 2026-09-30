# 05 · Modulation: LFO, Poly-Mod, Wheel-Mod, velocity, aftertouch

All depths below are **P5X initial values**. Tune by ear in milestone 4 and log any change in DECISIONS.md.

## LFO (global, one for the whole instrument)
- Rate `lfo_rate`, 0.05–30 Hz. Runs at the **host rate** (not oversampled), linearly interpolated to the internal rate.
- Free-running; never resets on note-on. Initial phase from the seed.
- Shapes (single choice, `lfo_shape`), output bipolar ±1:
  - Saw: rising ramp `2*phase - 1`
  - Triangle: `1 - 4*abs(phase - 0.5)`
  - Square: `phase < 0.5 ? +1 : -1`, with a 1 ms linear slew on transitions to avoid clicks when it modulates amplitude-sensitive destinations.
- No anti-aliasing needed (sub-audio).

## Wheel-Mod (global source, per-voice destinations)
```
source   = (1 - wm_mix) * lfo + wm_mix * pinkNoise          // both bipolar ±1
amount   = max(modWheel, aftertouchIfEnabled)                // 0..1, see below
wm       = source * amount
```
- Pink noise: global white noise through a −3 dB/oct filter (Paul Kellet "economy" 3-pole approximation, implemented from the published coefficients), then scaled so RMS ≈ 0.35. Generated at the **host rate** alongside the LFO (the coefficients are designed for 44.1–48 kHz) and linearly interpolated to the internal rate like the LFO.
- `modWheel` = CC1 / 127, smoothed Lin 20 ms. CC1 is hardwired and not learnable (07-midi.md).

Destinations (each toggled independently):
| Toggle | Effect at `wm = ±1` |
|---|---|
| `wm_dest_freq_a` | Osc A pitch ± 12 semitones |
| `wm_dest_freq_b` | Osc B pitch ± 12 semitones |
| `wm_dest_pw_a` | Osc A PW ± 0.45 (then clamped to 0.02–0.98) |
| `wm_dest_pw_b` | Osc B PW ± 0.45 |
| `wm_dest_filter` | cutoff ± 4 octaves |

Note: ±12 semitones at full wheel is intentionally large (full-throw effect). Typical vibrato lives at small wheel amounts.

## Poly-Mod (per voice)
```
pm = pm_filt_env * filterEnvLevel(0..1)  +  pm_osc_b * oscBOutput(±3)   // raw sum of B's active shapes, each ±1
```
- Osc B output used here is B's **raw summed waveform** from the current sample (before the mixer level), not normalised: with saw + tri + pulse on it reaches ±3, so more shapes give deeper modulation. This allows audio-rate FM, which is the point of Poly-Mod.
- Filter env source is the same filter envelope level the filter uses (after velocity scaling, before `flt_env_amt`).

| Toggle | Effect at `pm = 1` |
|---|---|
| `pm_dest_freq_a` | Osc A pitch + 48 semitones (exponential FM; pm is bipolar when Osc B is used) |
| `pm_dest_pw_a` | Osc A PW + 0.45 × pm |
| `pm_dest_filter` | cutoff + 5 octaves × pm |

- Poly-Mod is computed **every internal sample** (it's audio-rate). No smoothing on the Osc B signal path; the amount knobs are smoothed.
- Order within a sample: compute Osc B → compute pm → compute Osc A (with pm and sync) → mixer → filter.

## Velocity (`perf_velocity`)
When on, with `v = velocity / 127`:
- Amp: VCA gain × `(0.25 + 0.75 * v)`.
- Filter envelope: the envelope's output level is scaled by `(0.5 + 0.5 * v)`. That scaled level feeds both the filter (× `flt_env_amt` × 8 octaves, see 03-filter.md) and Poly-Mod's filter-env source. For the filter this is the same as scaling `flt_env_amt`.

When off, `v` is treated as 1 (full level for every note). Velocity is captured at note-on and fixed for the note.

## Aftertouch (`perf_aftertouch`)
- When on, aftertouch pushes up the Wheel-Mod amount: `amount = max(modWheel, at)`, where `at` is the smoothed aftertouch (Lin 20 ms).
- **Channel pressure:** one `at` value for all voices.
- **Poly pressure:** each voice uses its own note's pressure; the per-voice Wheel-Mod amount is `max(modWheel, voiceAt)`.
- When off, aftertouch is ignored completely (still shown in the MIDI monitor).
- A future filter-cutoff destination is an open question; don't implement it.

## Tests
- LFO rate 1 Hz: period 1 s ± 0.5 %. Square has no sample-to-sample jump > 2/(0.001 × rate) (a full −1 → +1 swing spread over 1 ms at the LFO's host rate).
- Wheel-Mod with wheel 0: no pitch/PW/cutoff change on any destination (bit-identical to wheel-mod off).
- Wheel 1, `wm_dest_freq_a`, LFO triangle: Osc A pitch swings ±12 st ± 0.1.
- Poly-Mod Osc B → Freq A with B at 200 Hz, amount 0.3: output spectrum shows FM sidebands at A ± n·200 Hz.
- Poly-Mod amount 0 with dest on: bit-identical to dest off.
- Velocity off: two notes at velocity 10 and 127 render identically.
- Aftertouch off: pressure messages don't change the output.
