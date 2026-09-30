# P5X

A polyphonic virtual-analog synthesizer for Windows (VST3 + Standalone), by Jeet Patel.

Status: **milestone 3** (modulation), version 0.3.0: two oscillators with hard sync and Osc B Lo Freq, noise, mixer,
the resonant 4-pole filter, filter and amp envelopes, Poly-Mod, Wheel-Mod (LFO / pink noise on the mod wheel),
2× oversampling. Velocity, aftertouch, unison, glide and Vintage arrive in milestone 4. See `CLAUDE.md` and `docs/` for
the full plan and specs.

## Building

Requirements: Windows 10/11, Visual Studio 2022 (C++ desktop workload), CMake 3.25+, Git.
JUCE, Catch2 and the ASIO SDK are downloaded by CMake on the first configure.

```bash
cmake --preset vs2022
```

```bash
cmake --build --preset release
```

```bash
ctest --preset release
```

Outputs land in `build/P5X_artefacts/<Config>/` (`VST3/P5X.vst3` and `Standalone/P5X.exe`).

> If Windows **Smart App Control** is on, it blocks the unsigned programs this build creates
> (JUCE's build helper, the tests, the Standalone app). Development needs it turned off.

## Playing

- **Notes:** P5X plays 61 keys, MIDI notes 36–96 (C2–C7). Notes outside that range are ignored.
- **Standalone:** open Settings (button or the P5X menu) to pick the audio device (ASIO, Windows
  Audio or DirectSound), MIDI inputs and MIDI channel. The computer keyboard plays notes:
  `A S D F G H J K` = C D E F G A B C, `W E T Y U` = the sharps, `Z` / `X` = octave down / up.
- **MIDI Learn:** right-click any control → *Learn MIDI CC*, then move a knob or fader on your
  controller. CC 1 (mod wheel), CC 64 (sustain) and CC 120–127 are reserved. Mappings are saved
  with the instance; *Save as default map* in Settings makes new instances start with them.
- **Debug console:** the *Debug* button shows the log, a MIDI monitor and the MIDI map.

### Arturia KeyLab Essential mk3

- Use a **User** program, not DAW mode.
- In Arturia MIDI Control Center, set the encoders to **Absolute** (relative encoder modes aren't supported).
- In the Standalone app, select the keyboard's **main** MIDI port, not its DAW port.
- If the pads trigger notes, set P5X's MIDI channel to the keyboard's channel instead of Omni.
- Leave the keyboard's octave/transpose at its default: P5X ignores notes outside 36–96.

## License

GPL-3.0-or-later. Third-party components are listed in `THIRD_PARTY.md`. Contributions need a
copyright assignment; see `CONTRIBUTING.md`.
