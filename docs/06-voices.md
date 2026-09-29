# 06 · Voices, allocation, unison, glide, Vintage, output stage

## Voice structure
Each voice owns: Osc A, Osc B, noise generator, LadderFilterCEM, filter Envelope, amp Envelope, DC blocker, per-voice `Random`, Vintage offsets, glide state, current note, velocity, gate flag, age counter.

Per internal sample:
1. Glide → current note.
2. Osc B (with Kbd / Lo Freq / fine / wheel-mod).
3. Poly-Mod value.
4. Osc A (with poly-mod, wheel-mod, sync from B).
5. Mixer: `a * mix_osc_a + b * mix_osc_b + noise * mix_noise * 0.7`.
6. Filter (cutoff per 03-filter.md).
7. VCA: `× ampEnv × velocityGain`.
8. DC blocker: one-pole high-pass at 5 Hz.
9. Add to the shared oversampled buffer × **voice gain 0.3** (P5X headroom: 5 voices of full saw stay mostly below 0 dBFS).

## Voice count
- `perf_voices` = 5 / 8 / 10. Allocate 10 voices always; the parameter sets how many are active.
- Changing the count while notes sound: new count applies when all voices are idle (or at next `prepareToPlay`). Log INFO when applied.

## Allocation (poly mode)
In order:
1. **Same note already sounding** (gate on or releasing) → reuse that voice, retrigger.
2. **Next idle voice, round-robin** starting after the last voice assigned. (Round-robin matches the hardware's rotating assigner and spreads Vintage character.)
3. **Steal**, in this order: the oldest voice in Release; else the oldest voice held only by the sustain pedal; else the oldest key-held voice.
- Stolen voices retrigger from their current envelope level (no fade, no click compensation; this is the analog behaviour).
- Log WARN on every steal (rate-limited, max 10/s).

## Sustain pedal
- Pedal down: note-offs mark voices as `sustained` instead of releasing.
- Pedal up: all `sustained` voices get `gateOff()`.
- Re-pressing a sustained note reuses its voice (rule 1).

## Unison (`perf_unison`)
- All active voices play one note: **last-note priority**. Releasing the current note returns to the most recent still-held note (note stack of 16, drop oldest when full).
- Each new note retriggers envelopes on all voices (from current level). Interaction with the sustain pedal is open (OPEN_QUESTIONS #30).
- Detune spread: voice `i` of `N` gets `(i/(N-1) - 0.5) × 14 cents` (±7 cents total spread), plus its Vintage offsets.
- Output gain in unison: voice gain × `1 / sqrt(N)` × 1.5 so unison is louder than one voice but doesn't clip hard. (P5X.)
- Switching unison on/off: all voices get `gateOff()` first, then the new mode takes effect.

## Glide (`perf_glide`)
- Constant-time glide: each voice moves from its previous note to the new note in `perf_glide` seconds, linearly in semitones.
- In poly mode each voice glides from **its own** last note. In unison all voices glide together from the previous unison note.
- `perf_glide = 0` → instant (no computation).
- A voice's very first note (no previous note): open, OPEN_QUESTIONS #29.
- First note after a voice was idle glides from that voice's last note (hardware behaviour). (Verify; see OPEN_QUESTIONS.)

## Vintage (`perf_vintage`, 0–1)
Per voice, drawn once from the seeded `Random` at `prepare` and scaled by the knob. Every `prepare` re-seeds each per-voice `Random` from the instance `seed` and the voice index, so repeated `prepare` calls and reloads give the same values:
| Offset | At Vintage = 1 |
|---|---|
| Static detune Osc A, Osc B (independent) | uniform ±8 cents |
| Static cutoff offset | uniform ±0.2 octaves |
| Static envelope time scale | uniform ×0.9–1.1 (all four times, both envelopes) |
| Slow drift, Osc A and B (independent) | random walk, ±4 cents max, low-passed at 0.3 Hz, updated at host rate |
At Vintage = 0 every offset is exactly 0 and output is identical to a "perfect" instrument.

## Output stage (after downsampling, host rate)
1. `× master_volume` (dB → gain, −60 dB = 0).
2. **Safety clipper:** `y = tanh(x)` for |x| > 0.8, identity below, with a continuous transition (P5X; keeps a runaway patch from reaching the host at full scale). Log WARN when engaged (rate-limited 1/s). (Formula open: OPEN_QUESTIONS #27.)
3. Mono → both output channels.

## Oversampling modes
- 2× default, 4× when `hq_mode` is on. Internal rate = host rate × factor.
- `hq_mode` changes apply at the next `prepareToPlay`. In Standalone, toggling it restarts the audio device. In the plugin, show "applies on next playback restart" in settings.

## Tests
- 5 voices, 6 held notes: 6th note steals the oldest held; log shows a WARN.
- Same note twice: only one voice used.
- Sustain: note on, note off, pedal down before off → voice keeps gate until pedal up.
- Unison: 3 held notes, release top → pitch returns to the previous note.
- Glide 0.5 s from C3 to C4: pitch at 0.25 s = F#3 ± 10 cents.
- Vintage 0 renders bit-identical to a build with Vintage code removed (compare against analytic reference).
- Same seed → identical render; different seed → different render (at Vintage > 0).
- Master volume −60 → output all zeros.
