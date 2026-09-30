# Third-party components

P5X is Copyright (c) 2026 Jeet Patel and licensed under GPL-3.0-or-later (see `LICENSE`).
It uses the components below. Nothing else may be added without the author's approval
(`docs/12-conventions.md` § Dependencies).

| Component | Version | License | How it's used | Source |
|---|---|---|---|---|
| JUCE | 8.0.15 | AGPLv3 (or commercial JUCE license) | Framework: plugin wrappers, audio devices, UI. Fetched by CMake, linked into the plugin. | https://github.com/juce-framework/JUCE |
| Steinberg ASIO SDK | 2.3.4 | GPLv3 option of Steinberg's dual license (proprietary Steinberg ASIO License or GPLv3) | ASIO device support in the Standalone app. Fetched by CMake at build time; never committed to this repo. | https://download.steinberg.net/sdk_downloads/ASIO-SDK_2.3.4_2025-10-15.zip |
| Catch2 | 3.16.0 | BSL-1.0 | Unit tests only; not shipped. | https://github.com/catchorg/Catch2 |
| Kode Mono (Isa Ozler) | static instances from the variable font | SIL Open Font License 1.1 | The only typeface; embedded in the plugin. License text: `Resources/fonts/KodeMono/OFL.txt`, shipped with the plugin. | https://github.com/isaozler/kode-mono |
| fontTools | — | MIT | Build-time only, used once to make the static Kode Mono instances. Not shipped. | https://github.com/fonttools/fonttools |

Tools used outside the repo and never linked or shipped:

| Tool | License | Use |
|---|---|---|
| pluginval 1.0.4 | GPLv3 | Plugin validation, run from `..\tools\pluginval\` locally and downloaded by CI |

## Before any paid (closed-source) release

- **JUCE:** the AGPLv3 only covers open-source distribution. A paid release needs a commercial JUCE license.
- **ASIO:** the GPLv3 option only covers GPL releases. A closed-source release needs Steinberg's proprietary ASIO License Agreement, signed by Steinberg (see the SDK's `LICENSE.txt`). The "ASIO" name and logo follow Steinberg's usage guidelines under either license.
- Every line of P5X's own code must be owned by Jeet Patel (see `CONTRIBUTING.md`).
