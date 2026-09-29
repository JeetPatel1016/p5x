# 11 · Testing and milestone sign-off

## Tooling
- **Catch2 v3** (BSL-1.0) for unit tests, fetched via CMake `FetchContent`. Test target `p5x_tests` links `p5x_dsp`, `p5x_midi` and `p5x_debug_core` only (no plugin wrapper), plus a separate `p5x_plugin_tests` that instantiates the processor headlessly.
- **pluginval** v1.0.4 (GPL, used as an external tool only, never linked). Locally it's at `..\tools\pluginval\pluginval.exe` (outside the repo); CI downloads its own copy. Both run `pluginval --strictness-level 5 --validate-in-process --skip-gui-tests P5X.vst3`.
- **CI:** (no GitHub remote yet; run everything locally until the user creates one) GitHub Actions, `windows-latest`, Release and Debug builds, runs both test targets and pluginval. Artifacts: VST3 + Standalone zip.

## Test types
1. **Unit tests** per module: listed at the end of each doc (02–10 and 13). Every item there must exist as a test.
2. **Golden renders** (from milestone 2): a fixed MIDI sequence (`tests/data/golden.mid`: chords, legato line, sustain use, bends) rendered offline with seed 1 at 48 kHz for a set of patches. Compared against stored reference WAVs by **RMS per 50 ms window (±0.5 dB)** and **spectral centroid (±3 %)**, not bit-exact. Updating a golden reference requires the user's approval and a DECISIONS.md line.
3. **Robustness:** random parameter values + random MIDI for 60 s at 44.1, 48, 96, 192 kHz and block sizes 1, 17, 64, 512, 4096: no NaN/Inf, peak < 4.0 before the clipper, no assertion.
4. **Real-time safety:** in Debug builds, a global allocation counter asserts zero allocations inside `processBlock` (after the first block). Runs under the robustness test.
5. **Performance:** 5 voices, all held, full-saw patch, res 0.8, 2× OS at 48 kHz, block 128: processBlock ≤ 10 % of one core on the CI machine (report the number; fail above 15 %).
6. **Manual checklist** (below): done by the user with the Standalone app and KeyLab.

## Milestone definitions of done
Every milestone: builds clean (warnings as errors, `/W4`), all tests pass, pluginval passes, the plugin shows vendor `Jeet Patel` and the right version, docs updated, DECISIONS.md updated if anything changed.

| # | Additional acceptance |
|---|---|
| 1 | Sine per note, 5-voice poly with stealing; MIDI table in 07 fully handled; MIDI Learn works on placeholder editor incl. reserved CCs, move, pickup, persistence; Standalone settings, 61-key on-screen keyboard + computer keyboard, notes outside 36–96 ignored; debug console log + MIDI monitor + map; param count test = 50 (all params exist even if DSP ignores them yet) |
| 2 | Oscillator, filter, envelope tests from 02–04; output stage; 2× OS; voices table + CPU in console; Standalone `Route input into filter`; first golden renders |
| 3 | Modulation tests from 05; sync; Lo Freq; scope |
| 4 | Velocity, aftertouch, unison, glide, Vintage tests from 05–06; HQ mode; performance test passing |
| 5 | Art pipeline acceptance in 13 (deterministic renders, frame counts, layout test); UI acceptance in 08; placeholder editor deleted |
| 6 | Preset/state tests in 09; 40 factory presets; Dump state |
| 7 | SSM filter mode with its own tests (spec to be written before starting) |

## Manual checklist (user, per milestone)
- Standalone opens, picks ASIO device, plays from the KeyLab and from the computer keyboard.
- MIDI Learn a knob and a fader to two controls; move them; reload app; mappings still there.
- Hold a chord with sustain pedal, release keys, lift pedal: everything releases.
- Load in the DAW, save project, reopen: sound and mappings restored.
- Debug console shows notes, CCs, and no unexpected WARN/ERROR during normal playing.
