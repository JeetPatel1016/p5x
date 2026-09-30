# 12 · Conventions and how to work in this repo

## Code
- C++20, MSVC `/W4 /WX` (warnings as errors), `/permissive-`. Also compile tests with clang-cl in CI if cheap.
- Namespace `p5x` (`p5x::dsp`, `p5x::midi`, `p5x::ui`, `p5x::debug`).
- Files: `PascalCase.h/.cpp`, one main class per file. Classes `PascalCase`, functions and variables `camelCase`, members without prefix, constants `kPascalCase`.
- `.clang-format` in the repo root (based on JUCE style: Allman braces, 4-space indent, 120 columns). Format before committing.
- File header on every source file:
  ```
  // P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.
  ```
- No `using namespace` in headers. No raw `new`/`delete`; use `std::unique_ptr` and containers (outside the audio thread).
- `float` for audio samples, `double` for phases and time accumulators.
- DSP classes: `prepare()`, `reset()`, `process…()`. No virtual calls in per-sample loops.
- Comments explain *why* (and cite the doc section: `// see 03-filter.md § Self-oscillation`), not *what*.

## Dependencies
Allowed now: JUCE 8 (AGPLv3), Catch2 v3 (BSL-1.0), Kode Mono font (SIL OFL 1.1), fontTools (MIT, build-time only, to make static font instances), Steinberg ASIO SDK (GPLv3 option of its dual license, fetched at build time and never committed; a paid release needs Steinberg's proprietary ASIO license instead). Anything else: ask first, then add to THIRD_PARTY.md.

## Git
- Branch per milestone: `m1-skeleton`, `m2-core-voice`, … Merge to `main` only when the milestone's definition of done is met.
- Small commits, imperative messages: `Add CEM3320 ladder core with ZDF solve`.
- Never commit build outputs, `.vs/`, or local settings.

## How Claude should work here
1. Read `CLAUDE.md`, `docs/00-architecture.md`, and the doc for the module you're touching before writing code.
2. Before a milestone, write a short plan (files to create, tests to add) and show it to the user.
3. Write the tests from the doc first when practical, then the implementation.
4. If a doc is silent, ambiguous or contradictory: stop, add the question to `docs/OPEN_QUESTIONS.md`, ask. Don't invent behaviour.
5. If you believe a spec value is wrong (e.g. sounds bad, measures wrong against the hardware manual), say so with evidence and propose a change. Don't change it silently.
6. Don't refactor or "improve" code outside the current task.
7. At the end of each milestone, report: what was built, test results, pluginval result, CPU number, anything added to OPEN_QUESTIONS.
