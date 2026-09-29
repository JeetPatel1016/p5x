# 00 · Architecture

## Module map and dependency rules
```
Source/
  PluginProcessor.*       host glue: params, state, processBlock orchestration
  PluginEditor.*          top-level UI component
  params/                 ParameterIDs.h, ParameterLayout.cpp (see 01-parameters.md)
  dsp/                    Random, Smoother, Oscillator, Noise, LadderFilterCEM, (LadderFilterSSM), Envelope,
                          LFO, PolyMod, Voice, VoiceAllocator, Vintage, OutputStage
  midi/                   MidiHandler, MidiLearn, MidiMonitor
  presets/                PresetManager, FactoryPresets
  debug/                  DebugLog, LogFileSink, Telemetry, DebugConsole
  ui/                     LookAndFeelP5X, ImageKnob, ImageButton4, ImageWheel, RealKeyboard, PresetDisplay
                          (milestones 1–4 only: PlaceholderPanel, OnScreenKeyboard; deleted in milestone 5)
  standalone/             StandaloneApp, SettingsDialog, ComputerKeyboardInput
tests/                    Catch2 tests, one file per dsp/midi module + golden renders
art/                      layout.json, style.json, Blender scripts; see 13-art-pipeline.md (concept images live outside the repo in ../artwork/concepts/)
Resources/ui/1x, 2x/      rendered PNGs (committed), bundled via BinaryData
```
Dependency rules (enforced by CMake targets):
- `dsp/` is a static library (`p5x_dsp`) that depends **only on the C++ standard library** (and `<juce_core>` for math helpers at most). No `juce_audio_processors`, no GUI, no APVTS. It takes plain structs of already-smoothed values. This keeps it unit-testable.
- `midi/` depends on `juce_audio_basics` only. It holds the MIDI Learn logic and tables; the part that needs the plugin wrapper (the 60 Hz timer that calls `setValueNotifyingHost`) lives in `PluginProcessor`.
- `dsp/` never logs. A component that must report an event (e.g. a voice steal) returns it to the caller, and the processor logs it.
- JUCE modules compile into every binary that links them, so `p5x_midi` and `p5x_debug_core` (logger, telemetry, FIFOs; `juce_core` only) are CMake INTERFACE libraries that carry their sources. The boundary is enforced by `p5x_tests`, which links only those modules.
- `ui/` never touches `dsp/` directly. It talks to parameters (APVTS attachments) and reads telemetry snapshots.
- `debug/Telemetry` is the only way data flows from the audio thread to the UI, besides parameters.

## Threads
| Thread | Owns | Must never |
|---|---|---|
| Audio (`processBlock`) | voices, DSP state, smoothers, MIDI Learn CC table reads | allocate, lock, log to file, call the message thread, throw |
| Message (UI) | editor, dialogs, preset I/O, file log sink, MIDI Learn table writes | touch DSP objects directly |
| Timer callbacks (message thread, 30 Hz) | drain log FIFO, read telemetry, repaint meters | block more than ~2 ms |

Cross-thread channels (all lock-free, fixed size, allocated in the constructor):
- **Parameters:** APVTS `std::atomic<float>*` raw pointers, cached in the processor constructor.
- **Log FIFO** (any thread → UI): lock-free bounded multi-producer ring of fixed-size POD entries (see 10-debug-and-harness.md). Not `juce::AbstractFifo`, which is single-producer: several plugin instances or host threads can log at the same time.
- **Telemetry** (audio → UI): per-voice snapshot structs written into a triple buffer; UI reads the latest.
- **MIDI Learn table** (UI ↔ audio): `std::array<std::atomic<int16_t>, 128>` mapping CC number → parameter index (−1 = unmapped). Learn events (audio saw CC while learning) go audio → UI through a small SPSC FIFO. See 07-midi.md.
- **MIDI monitor** (audio → UI): SPSC FIFO of raw 3-byte messages + timestamp.

## Signal flow (per voice)
```
          ┌──────────── Poly-Mod (Filt Env, Osc B) ─────────────┐
          │                                                     ▼
 note ─► pitch ─► OSC A (saw+pulse, sync to B) ─┐        freq A / PW A / cutoff
          │      OSC B (saw+tri+pulse) ─────────┤─► Mixer ─► CEM3320 LPF ─► VCA ─► DC block ─► voice out
          │      Noise (white) ─────────────────┘            ▲                ▲
          └── Wheel-Mod (LFO/noise × wheel) ──► freq A/B, PW A/B, cutoff      │
                                     Filter Env ──► cutoff    Amp Env ────────┘
```
Global (shared by all voices): LFO, wheel-mod noise source, master tune, pitch bend, mod wheel, aftertouch.
Output: sum of voices (at oversampled rate) → downsampler → master volume → safety clipper → out. See 06-voices.md.

## Oversampling
- All voice DSP runs at **2× the host rate** (4× in HQ mode). Voices render into one shared oversampled mono buffer, which is downsampled **once** after summing (not per voice).
- Use `juce::dsp::Oversampling` (polyphase IIR, max quality) on the summed signal only. Upsampling isn't needed because voices generate at the high rate directly. The one exception is the Standalone-only `Route input into filter` path (10-debug-and-harness.md, milestone 2), whose input is upsampled before it reaches the voices.
- Report the downsampler latency via `setLatencySamples` and update it when HQ changes (HQ change takes effect at the next `prepareToPlay`; see 06-voices.md).
- Output is **mono internally, duplicated to stereo** (the original is mono). Stereo bus only.
- Milestone 1 renders its placeholder sine voices at the host rate with no oversampling (latency 0). Oversampling arrives in milestone 2.

## Block processing order (`processBlock`)
1. `juce::ScopedNoDenormals`. Start CPU timer.
2. Read all parameter atomics once; set smoother targets.
3. Walk the `MidiBuffer`. For each event at sample offset `n`: render voices from the last offset to `n`, then handle the event (07-midi.md). Render the remainder at the end. Sample-accurate.
4. Downsample the summed buffer to host rate.
5. Output stage: master volume, safety clipper (06-voices.md).
6. Copy mono to both channels. Push telemetry. Stop CPU timer, update CPU%.

Must handle: any block size 0–8192, variable block sizes between calls, sample rates 44.1–192 kHz, repeated `prepareToPlay` calls, `setStateInformation` before or after `prepareToPlay`, `releaseResources` then re-prepare.

## Real-time rules
- No `new`/`delete`, `std::vector` growth, `std::string` building, `std::function` allocation, locks, `DBG`, file I/O, or exceptions on the audio thread. Allocate everything in the constructor or `prepareToPlay` for the maximum block size (with a size guard that splits oversized blocks).
- `std::atomic` loads/stores with `memory_order_relaxed` for parameters; acquire/release for FIFOs.
- Phase accumulators use `double`. Audio samples use `float`.
- Clamp every value entering DSP (frequencies to the range in each module's doc and never above 0.45 × internal rate: oscillators from 0.01 Hz, filter from 5 Hz; gains ≥ 0) so a bad parameter can never produce NaN/Inf.
- Every DSP object has `prepare(sampleRate, maxBlock)` and `reset()`; `reset()` must not allocate.

## Randomness
All randomness (initial oscillator phases, Vintage offsets, drift, noise) comes from `p5x::Random` (xorshift128+), seeded from a per-instance `seed` stored in plugin state. Same seed + same input = same output. Never use `rand()` or `std::random_device` in DSP.
