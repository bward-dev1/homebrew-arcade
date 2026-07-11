# homebrew-arcade

GBA/NDS/3DS homebrew, built entirely on GitHub Actions. There is no local
toolchain here on purpose — devkitPro's ARM toolchains, per-platform
libraries, and any emulators live only inside CI runners. The dev machine
never installs devkitPro, gcc-arm, or an emulator; every build and every
smoke test happens on GitHub's infrastructure and is inspected via `gh run`.

## Layout

```
homebrew-arcade/
  gba/
    games/<game-name>/     one dir per GBA game, each with its own Makefile
    README.md               GBA toolchain notes
  nds/
    games/<game-name>/     one dir per NDS game
    README.md               NDS toolchain notes
  n3ds/
    games/<game-name>/     one dir per 3DS game
    README.md               3DS toolchain notes
  .github/workflows/
    gba.yml
    nds.yml
    n3ds.yml
```

Each platform directory is independent: its own games/ tree, its own
workflow, its own toolchain group. A push that only touches `gba/**` never
triggers the nds/3ds workflows and vice versa.

## Games (15/15 — GBA 5/5, NDS 5/5, 3DS 5/5, all CI-green)

### GBA (`gba/games/`)
- **pixel-jumper** — side-scrolling run/jump platformer against gravity and gaps.
- **brick-blaster** — Breakout-style paddle-and-ball brick clearing.
- **key-quest** — timed maze navigation to collect keys before time runs out.
- **mole-mash** — whack-a-mole reaction/timing game.
- **star-siege** — space shooter blasting waves of enemies.

### NDS (`nds/games/`)
- **maze-muncher** — Pac-Man-style maze chase, dodging/eluding pursuing ghosts.
- **grid-slither** — classic snake, growing and avoiding self-collision.
- **card-flip** — memory card-matching pairs game.
- **paddle-bounce** — Breakout-style paddle-and-ball brick clearing.
- **star-defender** — top-down shooter defending against descending enemies.

### 3DS (`n3ds/games/`)
- **simon-sez** — memory pattern game, repeat an escalating sequence.
- **block-drop** — Tetris-style falling-block stacking puzzle.
- **meteor-dash** — top-down dodge/survival against incoming hazards.
- **vault-run** — maze navigation collecting numbered tumblers in strict order against a clock.
- **sky-hopper** — vertical climbing platform jumper: hop between platforms up a static tower while an accelerating camera scroll threatens to leave you behind.

## Adding a new game

Drop a new directory under the right platform's `games/` folder with a
standard devkitPro-template Makefile (see the existing `pixel-jumper`
example under `gba/games/` for the canonical structure) and source files.
Push it. That's the entire setup step — CI discovers new game directories by
globbing `<platform>/games/*/` at build time, so nothing else needs to be
registered anywhere.

Requirements for a new game directory to build cleanly in CI:
- A `Makefile` that includes the platform's devkitPro rules file
  (`$(DEVKITARM)/gba_rules` for GBA, the devkitARM/devkitPPC NDS/3DS
  equivalents for the others) and follows the standard
  TARGET/BUILD/SOURCES/INCLUDES/LIBS/LIBDIRS variable layout.
- Source under `source/`, headers under `include/`, matching the Makefile's
  `SOURCES`/`INCLUDES` variables.
- No dependency on anything outside the pacman package group the workflow
  installs (`gba-dev` / `nds-dev` / `3ds-dev`) unless the workflow is updated
  to install the extra package too.

## How CI works

Each workflow (`gba.yml`, `nds.yml`, `n3ds.yml`):

1. Triggers on `push` to that platform's directory (or the workflow file
   itself) via a `paths:` filter, plus `workflow_dispatch` for manual runs.
2. Installs devkitPro on the `ubuntu-latest` runner via the official apt
   installer (`install-devkitpro-pacman`), then pulls the platform's dev
   package group (`gba-dev`, `nds-dev`, or `3ds-dev`) with `dkp-pacman -S
   --noconfirm` — nothing here ever runs on the developer's machine.
3. Globs `<platform>/games/*/` and runs `make` in each game directory. If
   there are zero game directories, the build step logs that and exits 0
   — an empty platform is a valid, green state, not a failure.
4. Runs a smoke test against whatever ROMs got built. All three workflows
   default to a ROM-header sanity check (magic bytes / fixed header fields
   at the documented offsets, plus a plausible file-size range) rather than
   a real emulator boot — headless emulator CLIs (mgba, melonDS/desmume,
   Lime3DS/Citra) aren't reliably available as apt packages on
   `ubuntu-latest` and are a much larger source of CI flakiness than they're
   worth for a homebrew-scale pipeline. Each workflow has a comment at the
   smoke-test step explaining this trade-off inline.
5. Uploads any built ROMs as workflow artifacts via
   `actions/upload-artifact@v4` (skipped cleanly if nothing was built).

## Verifying a run

```
gh run list
gh run watch <run-id>
gh run view <run-id> --log-failed
```
