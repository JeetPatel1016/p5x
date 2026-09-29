# P5X: project instructions

P5X is a polyphonic virtual-analog synth plugin modeled on the Sequential Prophet-5 **Rev 3** architecture. It's for personal use first, and may become a paid product later. Target: Windows VST3 + Standalone.

**Naming:** never use "Prophet", "Sequential" or "Prophet-5" in the product name, UI text, code identifiers, presets or repo name. The only exception is citing reference documents in `docs/`.

## Product identity (permanent once milestone 1 ships)
The author and vendor is **Jeet Patel**. It must show up everywhere a host or the OS shows plugin info. Set these in `juce_add_plugin` exactly:

| Setting | Value |
|---|---|
| `PRODUCT_NAME` | `P5X` |
| `COMPANY_NAME` | `Jeet Patel` (the vendor/manufacturer shown in DAW plugin lists and VST3 info) |
| `COMPANY_COPYRIGHT` | `Copyright (c) 2026 Jeet Patel` (goes into the Windows file properties of the .vst3 and .exe) |
| `COMPANY_WEBSITE` | empty for now |
| `BUNDLE_ID` | `com.jeetpatel.p5x` |
| `PLUGIN_MANUFACTURER_CODE` | `JtPl` |
| `PLUGIN_CODE` | `P5xS` |
| `VERSION` | `0.1.0` (milestone N ships as `0.N.0`) |
| `IS_SYNTH` / `NEEDS_MIDI_INPUT` | `TRUE` / `TRUE` |
| `VST3_CATEGORIES` | `Instrument Synth` |
| `FORMATS` | `VST3 Standalone` |

- The Settings dialog footer and the Standalone app's About item show `P5X <version> · © 2026 Jeet Patel`.
- Every source file header: `// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.`
- `LICENSE`, `THIRD_PARTY.md` and `CONTRIBUTING.md` name Jeet Patel as the copyright holder.
- `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE` and `BUNDLE_ID` must never change after milestone 1, or DAWs will treat P5X as a different plugin and saved projects lose it.
- A plugin test asserts `JucePlugin_Manufacturer == "Jeet Patel"` and `JucePlugin_Name == "P5X"`.

## Environment and getting started
- **Machine:** Windows 11 (Lenovo Legion 5), Arturia KeyLab Essential mk3 (61 keys) as the MIDI controller. P5X plays 61 keys: MIDI notes 36–96.
- **Installed by the user:** Visual Studio 2022 (MSVC, C++ desktop workload), CMake, Git.
- **Folder layout** (`Desktop\p5x\`):
  - `dev\`: **this repo** (git on `main`; specs committed 2026-09-29; milestone work happens on `m*` branches).
  - `tools\pluginval\pluginval.exe`: pluginval v1.0.4. Run it from `dev` as `..\tools\pluginval\pluginval.exe --strictness-level 5 --validate-in-process --skip-gui-tests <path-to>\P5X.vst3`. It's GPL, so it stays outside the repo and is never linked.
  - `artwork\concepts\`: concept images and prompts (reference only, not part of the repo).
- **Fetched by CMake (`FetchContent`):** JUCE 8.0.15, Catch2 v3.16.0 and the Steinberg ASIO SDK (see DECISIONS.md). Change a pinned version only with the user's approval and a DECISIONS.md line.
- **Font:** already in `Resources/fonts/KodeMono/`.
- **Not installed yet:** Blender 4.2 LTS (needed at milestone 5) and a DAW for manual testing (ask the user which one when milestone 1 is ready to test).
- **CI:** there's no GitHub remote yet. Until the user creates one, run the build, tests and pluginval locally and report the results; write the GitHub Actions workflow anyway so it's ready.
- **Commits:** commit on the milestone branch as you go (see 12-conventions.md). Never push unless the user asks.

**First session:** read this file and every doc in `docs/`, then write a plan for **milestone 1 only** (files to create, CMake targets, tests, how you'll verify it) and show it to the user before writing any code.

## The specs are the source of truth
This file is the index. Each module has an exact spec in `docs/`. **Before you touch a module, read its doc and `docs/00-architecture.md`.**

| Doc | Read before working on |
|---|---|
| `docs/00-architecture.md` | anything: threading, signal flow, module boundaries, real-time rules |
| `docs/01-parameters.md` | anything that adds, reads, or changes a parameter |
| `docs/02-oscillators.md` | `dsp/Oscillator*`, sync, noise |
| `docs/03-filter.md` | `dsp/LadderFilter*` |
| `docs/04-envelopes.md` | `dsp/Envelope*` |
| `docs/05-modulation.md` | `dsp/LFO*`, `dsp/PolyMod*`, Wheel-Mod, aftertouch, velocity |
| `docs/06-voices.md` | `dsp/Voice*`, `dsp/VoiceAllocator*`, unison, glide, Vintage, oversampling, output stage |
| `docs/07-midi.md` | `midi/*`, MIDI Learn |
| `docs/08-ui.md` | `ui/*`, `PluginEditor*` |
| `docs/09-presets-and-state.md` | presets, `getStateInformation` / `setStateInformation` |
| `docs/10-debug-and-harness.md` | `debug/*`, `standalone/*` |
| `docs/11-testing.md` | tests, CI, milestone sign-off |
| `docs/12-conventions.md` | every change: code style and how to work in this repo |
| `docs/13-art-pipeline.md` | `art/`, Blender scripts, `layout.json`, anything that loads UI images |
| `docs/DECISIONS.md` | the log of locked decisions and why |
| `docs/OPEN_QUESTIONS.md` | things not decided yet; **don't resolve these yourself** |

## Working rules (non-negotiable)
1. **Implement the spec exactly.** Don't add features, controls, parameters or behaviour that aren't in a doc, even if they seem helpful.
2. **If a spec is missing, ambiguous, or conflicts with another doc or with reality** (for example a JUCE API doesn't allow it), stop. Add an entry to `docs/OPEN_QUESTIONS.md` and ask the user. Don't pick an answer silently.
3. **Behaviour changes update the doc in the same commit.** If the user approves a change, update the relevant doc and add a line to `docs/DECISIONS.md`.
4. **One milestone at a time** (below). Don't start the next until the current one meets its definition of done in `docs/11-testing.md`.
5. **Real-time safety is absolute**: see `docs/00-architecture.md` § Real-time rules.
6. Parameter IDs in `docs/01-parameters.md` are permanent once milestone 1 ships. Never rename or reuse one.

## Locked decisions (details in `docs/DECISIONS.md`)
- **Revision:** Rev 3 (CEM3340 VCO, CEM3320 VCF, CEM3310 envelopes). The Rev 1/2 SSM2040 filter is optional milestone 7.
- **Platform:** Windows only, VST3 + Standalone. C++20, JUCE 8, CMake, MSVC.
- **Scope:** faithful to the original, plus light extras (velocity, aftertouch, unison, glide, Vintage). No effects.
- **MIDI:** full MIDI from milestone 1. No default CC mappings: the user maps controls with right-click MIDI Learn. Test controller: Arturia KeyLab Essential mk3.
- **Testing:** the Standalone app is the test harness (device settings, on-screen and computer keyboard, debug console).
- **UI:** realistic hardware look. JUCE components drawing pre-rendered images (filmstrip knobs, photoreal panel); no WebView. All art rendered from Blender scripts in the repo; AI concept images are inspiration only. Layout from the **P5X Panel** design canvas via `art/layout.json`.

## Licensing rules (critical)
The plan is open source now (GPL-3.0) with the option to sell later. That only works if the author owns every line.
- **Write all code from scratch.** Don't copy, paste or closely translate code from any GPL/AGPL/LGPL project (OB-Xd, Surge, Vital/Helm, Dexed, Autodafe VES, the NI Pro-53 Cmajor port, etc.). Reading them to understand an approach is fine; reproducing them is not.
- Algorithms from papers and books (ZDF filters, PolyBLEP, etc.) are fine to implement in our own words.
- Only add dependencies with permissive licenses (MIT, BSD, Apache, ISC, zlib, SIL OFL for fonts). Ask before adding any dependency at all.
- Approved exception: the Steinberg ASIO SDK under the GPLv3 option of its dual license (fine while P5X is GPL; a paid release needs Steinberg's proprietary ASIO license). Never commit the SDK.
- Keep `THIRD_PARTY.md` listing every dependency and its license.
- `LICENSE` = GPL-3.0. `CONTRIBUTING.md` states outside contributions need copyright assignment (or a CLA).
- **JUCE 8** is used under AGPLv3 while open source. Before any paid release it needs a commercial JUCE license; note this in `THIRD_PARTY.md`.
- Don't use original factory patch names or sampled audio from the hardware.

## Milestones
Details and acceptance criteria: `docs/11-testing.md`.
1. **Skeleton + test harness:** CMake project; sine-per-note polyphony; full MIDI spec; MIDI Learn on a placeholder slider editor; Standalone settings, on-screen and computer keyboard; lock-free logger + debug console (log, MIDI monitor); pluginval passes; LICENSE, THIRD_PARTY.md, CONTRIBUTING.md.
2. **Core voice:** oscillators, mixer, CEM3320 filter, both envelopes, output stage, 2x oversampling. Debug: Voices panel, CPU meter.
3. **Modulation:** Poly-Mod, Wheel-Mod, LFO, sync, Osc B Lo Freq. Debug: scope.
4. **Extras + character:** velocity, aftertouch, unison, glide, Vintage, filter nonlinearity tuning, HQ 4x mode, CPU profiling.
5. **UI:** (a) art pipeline: `layout.json`, Blender scripts, rendered @1x/@2x assets in the locked Direction C style (`art/style.json`); (b) the full panel built from those assets, resizable, MIDI Learn visuals.
6. **Presets:** save/load, prev/next, 40 original factory patches, Dump state.
7. **Optional:** SSM2040-style filter + envelope mode (Rev 1/2).

Until milestone 5, use a plain grid of sliders and buttons (with right-click MIDI Learn) instead of the real panel.
