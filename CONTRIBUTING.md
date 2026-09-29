# Contributing to P5X

P5X is Copyright (c) 2026 Jeet Patel and licensed under GPL-3.0-or-later.

The author intends to keep the option of releasing P5X commercially under a different license
later. That's only possible if the author holds the copyright to every line. So:

- **Outside contributions need a copyright assignment to Jeet Patel (or a signed contributor
  license agreement granting equivalent rights) before they can be merged.** Please open an issue
  to discuss a change before writing it.
- Don't submit code copied, pasted or closely translated from any GPL, AGPL or LGPL project.
  Implementing algorithms from papers and books in your own words is fine.
- Don't add dependencies. Any new dependency needs the author's approval and must have a
  permissive license (MIT, BSD, Apache, ISC, zlib, SIL OFL for fonts).

## How work is done here

- The specs in `docs/` are the source of truth; start with `CLAUDE.md` and `docs/00-architecture.md`.
- Code style, file headers and the git workflow are in `docs/12-conventions.md`.
- Every source file starts with:
  `// P5X — Copyright (c) 2026 Jeet Patel. Licensed under GPL-3.0-or-later.`
