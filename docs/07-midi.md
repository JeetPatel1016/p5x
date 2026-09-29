# 07 · MIDI and MIDI Learn

## Input handling (milestone 1 onward)
All events are processed at their sample offset within the block (see 00-architecture.md block order).

| Message | Behaviour |
|---|---|
| Note On (vel > 0) | allocate voice (06-voices.md), velocity captured |
| Note On vel 0 / Note Off | note off (release velocity ignored) |
| Pitch Bend | 14-bit, centered at 8192, scaled to ± `bend_range` semitones, smoothed Lin 5 ms. Applies to Osc A and B (B only when Kbd is on) |
| CC 1 (mod wheel) | Wheel-Mod amount (hardwired, not learnable) |
| CC 64 (sustain) | ≥ 64 = down, < 64 = up (06-voices.md) |
| CC 120 All Sound Off | all voices reset to Idle immediately (hard cut) |
| CC 123 All Notes Off | all voices `gateOff()`; sustain state cleared |
| CC 121 Reset All Controllers | mod wheel 0, bend centre, aftertouch 0, sustain up |
| CC 122, 124–127 | ignored (reserved, never learnable) |
| other CCs | routed through MIDI Learn (below); unmapped CCs are ignored (still shown in the monitor) |
| Channel Pressure | aftertouch, all voices (05-modulation.md) |
| Poly Pressure | aftertouch for the voice playing that note |
| Program Change | only if `program_change_enabled`: loads factory/user preset N (0-based index into the preset list). Loading happens on the message thread via async message; the audio thread never loads presets. Until presets exist (milestone 6) it's parsed and logged only. Most VST3 hosts don't pass Program Change to plugins as MIDI; revisit at milestone 6 |
| Everything else (SysEx, clock, etc.) | ignored |

- **Channel filter:** `midi_channel` 0 = Omni, 1–16 = that channel only. Applies to all messages above.
- **Panic** (debug console button): same as All Sound Off + clear sustain + clear note stack.
- Notes 0–127 all accepted; pitch clamps keep extreme notes safe.

## MIDI Learn (Kontakt-style)
No CC is mapped to anything out of the box. The user builds their own map from the panel.

### Flow
1. Right-click any knob or LED button (placeholder editor in milestones 1–4, real panel from milestone 5). Context menu:
   - `Learn MIDI CC`
   - `Remove MIDI CC 74` (only shown when mapped; shows its CC number)
   - `Cancel learn` (only shown while this control is waiting)
2. After `Learn MIDI CC`, the control enters a waiting state: pulsing accent ring; the top bar shows `Waiting for CC… · <Param name> · Esc to cancel`.
3. The next non-reserved CC received on the active channel binds. The ring stops, the top bar shows `<Param name> ← CC 74` for 2 s, logged INFO.
4. Only one control waits at a time; starting another learn cancels the previous. Esc or `Cancel learn` exits with no change. No timeout.

### Rules
- **Reserved, never learnable:** CC 1, CC 64, CC 120–127. While waiting, a reserved CC is ignored and logged WARN (`CC 64 is reserved (sustain)`); learning continues.
- One CC → one parameter. Learning a CC already in use **moves** it (log WARN `CC 74 moved: Cutoff → Resonance`). One parameter can have only one CC; learning a new CC for a mapped parameter replaces the old one.
- **Continuous params:** CC value 0–127 → normalized 0–1 → `setValueNotifyingHost`, through the parameter's own range and skew. Int params round to nearest step.
- **Bool params:** toggle on each rising crossing of 64 (value goes from < 64 to ≥ 64). So a momentary pad/button toggles once per press.
- **Choice params** (`flt_kbd`, `lfo_shape`, `perf_voices`): CC range split into equal zones, one per option.
- **Takeover:** `Pickup` (default): after a preset load or a UI/host change, a mapped CC is ignored until its value crosses (or lands within 1/127 of) the parameter's current normalized value; then it tracks. `Jump`: CC applies immediately. Log pickup acquisition at DEBUG.
- Absolute CCs only. Relative encoder modes are out of scope (README tells users to set encoders to Absolute).
- **Applying a learned CC (two paths, both required):**
  1. *Sound, immediately:* the audio thread writes the CC's normalized value into a per-parameter override slot that the DSP reads in place of the parameter atomic, so the change is heard with zero latency.
  2. *Host, shortly after:* the audio thread also pushes `{paramIndex, value}` into an SPSC FIFO; a 60 Hz message-thread timer calls `setValueNotifyingHost` (wrapped in a change gesture) so the UI and DAW automation see it.
  The override slot clears once the parameter atomic matches it. Never call `setValueNotifyingHost` from the audio thread.

### Data model
- Audio side: `std::array<std::atomic<int16_t>, 128> ccToParam` (index into the parameter list, −1 = none).
- Message side: the authoritative map `std::map<int, juce::String paramID>`; every edit rewrites the atomic table.
- Learn state: `std::atomic<int> learningParamIndex` (−1 = not learning). The audio thread, when it's ≥ 0 and a non-reserved CC arrives, pushes `{cc, paramIndex}` into the learn FIFO and doesn't apply the CC. The message thread commits the mapping and sets `learningParamIndex = -1`.

### Persistence
- Mappings belong to the instance, not the preset. Loading a preset never changes the map.
- Stored in plugin state as `<MidiMap><Map cc="74" param="flt_cutoff"/>…</MidiMap>` (by param ID, not index).
- Settings: `Save as default map` writes `%APPDATA%/P5X/midi-map.xml`; new instances load it if it exists and the host state doesn't include a `<MidiMap>` element. An empty `<MidiMap/>` counts as a map, so a cleared map stays cleared. `Clear all mappings` asks for confirmation.
- Tooltip on mapped controls: `Cutoff · 42% · CC 74`. The debug console lists the full map.

### Controller notes (KeyLab Essential mk3, for README)
Use a User program (not DAW mode); set encoders to Absolute in Arturia MIDI Control Center; in the Standalone app select the keyboard's main MIDI port, not its DAW port; if pads trigger notes, set P5X's MIDI channel to the keyboard's channel instead of Omni.

## Tests
- Rapid note on/off + sustain sequences (10 000 random events): no stuck voices after All Notes Off + pedal up.
- Panic: all output zero within one block.
- Channel filter 3: messages on channel 1 ignored.
- Learn: set waiting on `flt_cutoff`, send CC 64 (ignored), CC 74 (binds). Map shows 74 → flt_cutoff.
- Re-learn CC 74 on `flt_res`: cutoff unmapped, resonance mapped.
- Bool toggle: CC 20 values 0,127,127,0,127 → toggles twice.
- Pickup: param at 0.5, CC sends 10, 20, 30: no change; sends 64: starts tracking.
- Map survives `getStateInformation` → `setStateInformation` round trip and a preset load.
