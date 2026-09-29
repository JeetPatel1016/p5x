# 09 · Presets and plugin state

## What lives where
| Data | Preset | Plugin state (DAW project) | Global file |
|---|---|---|---|
| All 50 host parameters | ✓ | ✓ | |
| Preset name, index, edited flag | | ✓ | |
| MIDI map | | ✓ | default map (`midi-map.xml`) |
| `midi_channel`, `takeover_mode`, `hq_mode`, `program_change_enabled` | | ✓ | defaults for new instances (`settings.xml`) |
| `seed` | | ✓ | |
| Audio device settings | | | Standalone only (`standalone.xml`) |
| Window scale | | ✓ | |

Global files live in `%APPDATA%/P5X/`. `settings.xml` is rewritten whenever a setting changes in the Settings dialog.

## Plugin state format
`getStateInformation` writes XML (via `copyXmlToBinary`):
```xml
<P5XState version="1">
  <PARAMETERS .../>                <!-- APVTS state -->
  <Preset name="Brass Stab" index="12" edited="0"/>
  <MidiMap><Map cc="74" param="flt_cutoff"/></MidiMap>
  <Settings midiChannel="0" takeover="pickup" hq="0" programChange="0" scale="1.0"/>
  <Seed value="1234567890123"/>
</P5XState>
```
- `setStateInformation` must be tolerant: unknown elements/attributes ignored, missing ones take defaults, invalid values clamped. Never crash on bad data (fuzz test).
- Parameters missing from old state take their default (future-proofing when params are added after approval).

## Preset file format
- Extension `.p5xpreset`, XML:
```xml
<P5XPreset version="1" name="Brass Stab" author="" category="Brass">
  <Param id="flt_cutoff" value="1850.0"/>  <!-- real units, not normalized -->
  ...
</P5XPreset>
```
- Values in **real units** (Hz, seconds, semitones), so presets stay valid if a range changes later.
- Loading: unknown IDs ignored; missing IDs set to default; values clamped to range.

## Preset list
- Factory presets: embedded in BinaryData, read-only, indices 000–039.
- User presets: `%APPDATA%/P5X/Presets/*.p5xpreset`, sorted by name, indices from 040 upward.
- Prev/next wraps around the combined list.
- Save: prompts for a name, writes to the user folder. Overwriting an existing user preset asks for confirmation. Factory presets can't be overwritten (save as a new user preset instead).
- Loading a preset: on the message thread; sets all params via `setValueNotifyingHost` inside a `beginChangeGesture`/`endChangeGesture` pair per param, then resets Pickup state for all mapped CCs. Voices keep playing (no audio interruption, no voice reset).
- Edited flag: set when any param changes after load; shown as `*`.

## Factory presets (milestone 6)
- 40 original patches, original names only. Categories: Bass, Brass, Keys, Lead, Pad, Poly, FX, Sync, Poly-Mod.
- An init patch at index 000 named `Init` with all defaults.
- Stored as `.p5xpreset` files in `Resources/Presets/` and compiled into BinaryData.

## Tests
- State round trip: set random values for every param, map 3 CCs, save, create new instance, load → identical params, map, settings, seed.
- Garbage state (random bytes, truncated XML, wrong version) → loads defaults, no crash.
- Preset round trip in real units: save then load → each param within 1e-4 of original.
- Preset load doesn't change the MIDI map or settings.
