# 10 · Debug console and Standalone test harness

## Logging
- API (callable from any thread): `P5X_LOG(level, fmt, args...)`, levels ERROR, WARN, INFO, DEBUG.
- Entry: POD struct `{ uint64 timeMs; uint16 instanceId; uint8 level; uint8 source; char text[120]; }`. `source` is the subsystem: `engine` (processor audio path: voices, steals, clipper), `midi`, `learn`, `state`, `ui`, `standalone`, `log` (the logger itself, e.g. drop counts). `instanceId` is the plugin instance's number in the process (1, 2, …); 0 = global, not tied to an instance. Formatting uses a fixed-size `snprintf` into `text`; no allocation. Longer text is truncated.
- Any thread other than the message thread (audio threads of every instance, host worker threads): pushes into a lock-free bounded multi-producer ring of 2 048 entries (per-slot sequence numbers, written from scratch; `AbstractFifo` is single-producer and can't be used here). If full, the entry is dropped and a `dropped` counter increments (reported as WARN by the drainer).
- Message thread: logs go into a second FIFO of the same type (so ordering and formatting are identical).
- The logger is process-wide: shared by all plugin instances, created with the first and destroyed with the last. One drainer per process (30 Hz timer) moves entries to a shared ring of the last 5 000 and to the file sink (each line includes the instance id). Each instance's console shows its own entries plus global ones.
- **Rate limiting:** call sites that can repeat per block (voice steals, clipper, pickup) use `P5X_LOG_RATE(level, perSecond, ...)`.
- **DEBUG level** is compiled out in Release builds (macro expands to nothing) except when `P5X_VERBOSE=1` is set at build time.
- File sink: `%APPDATA%/P5X/logs/p5x-YYYY-MM-DD.log`, append, rotating, keep 5 files. Written by the message thread only.

## Telemetry
- Per voice: state (idle/on/release/sustained), note, velocity, Osc A Hz, cutoff Hz, amp env stage/level.
- Global: CPU % (processBlock time / block duration, smoothed 300 ms), sample rate, block size, oversampling factor, xrun count (blocks where processing time > block duration), last 512 output samples for the scope.
- Written by the audio thread once per block into a triple buffer; read by the UI at 30 Hz.

## Debug console window
Toggled by the Debug button (LED lit while open). Available in all builds. Matches the Debug console artboard (not in the repo yet: OPEN_QUESTIONS #15). Parts arrive by milestone: log, MIDI monitor, MIDI map (1); Voices table, CPU card (2); Scope (3); Dump state (6). Parts not built yet are hidden, not shown disabled.
- **Header buttons:** Pause (freezes views, logging continues), Clear, Save log…, Dump state, Panic.
- **Log pane:** timestamp, level (colored: INFO blue `#8FB7E0`, WARN accent, ERROR `#E06C5A`, DEBUG dim), text. Level filter chips.
- **MIDI monitor:** last 200 messages: time, channel, type, data. Shows all incoming messages *before* the channel filter, with channel-filtered messages and out-of-range notes (07-midi.md) dimmed.
- **Voices table:** from telemetry.
- **MIDI map:** CC → parameter list (tab next to MIDI monitor).
- **CPU card:** CPU %, rate, block size, OS factor, xruns.
- **Scope:** last 512 output samples, triggered on rising zero crossing.
- **Dump state:** writes `%APPDATA%/P5X/dumps/p5x-dump-<timestamp>.txt` with every parameter (ID, real value), MIDI map, settings, seed, voice snapshot, build info (version, git hash, build type).

## Standalone test harness
The Standalone app is the main testing tool. Use a custom `StandaloneFilterWindow` replacement (JUCE's `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP`).
- **Settings dialog** (gear icon + app menu), matching the Audio/MIDI settings artboard:
  - Audio: device type (every type JUCE offers on Windows: ASIO, Windows Audio, Windows Audio (Exclusive Mode), Windows Audio (Low Latency Mode), DirectSound), output, input, sample rate, buffer size. Built on `AudioDeviceSelectorComponent`, restyled.
  - `Route input into filter` checkbox (Standalone only, **milestone 2**: it needs the filter and a Standalone-only input bus; the VST3 stays output-only): the selected input's first channel is added to each voice's mixer at unity, so the filter can be tested with external audio. Off by default and not saved (always off on launch, to avoid feedback surprises).
  - MIDI: active input checklist, MIDI channel, knob takeover, CC mappings count + `Save as default map` + `Clear all`, program change toggle, pitch bend range.
  - `Computer keyboard plays notes` toggle.
  - Buttons: Test tone (440 Hz sine, −12 dBFS, 2 s), Reset audio (close/reopen device), Close.
  - Plugin build: the dialog shows only the MIDI and HQ items, minus the MIDI input checklist (the host owns MIDI inputs).
- **Computer keyboard:** A S D F G H J K = C D E F G A B C; W E T Y U = C# D# F# G# A#; Z / X = octave down/up, clamped so every key stays inside the 61-key range (the A key from C2 to C6, MIDI 36–84); velocity 100. Default: the A key plays C3 (MIDI 48). Active only when the Standalone window has focus and no text field is focused. Key repeat is ignored.
- **Persistence:** device settings, window size/scale, last preset, and settings survive relaunch (`standalone.xml`).
- **Crash safety:** if the saved audio device fails to open, fall back to the default device and log WARN, never exit.

## Tests
- Logging from the audio thread at 10 000 entries/s for 10 s: no allocation (checked with a custom allocator hook in tests), no blocking, drops counted.
- Dump state file parses back (contains every param ID).
- Computer keyboard: key down/up generates matching note on/off; octave shift clamps.
