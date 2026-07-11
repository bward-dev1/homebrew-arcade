# 3DS

Built with devkitPro's `3ds-dev` pacman group (devkitARM + libctru +
citro2d/citro3d as needed, plus `makerom`/`bannertool` for `.cia` packaging
if a game wants an installable CIA rather than a `.3dsx` homebrew-launcher
binary). 3DS uses devkitARM, not a separate devkitPPC toolchain. Env vars
set by the workflow:

- `DEVKITPRO=/opt/devkitpro`
- `DEVKITARM=/opt/devkitpro/devkitARM`

A standard 3DS Makefile includes `$(DEVKITARM)/3ds_rules` and links against
`libctru`, following the same TARGET/BUILD/SOURCES/INCLUDES/LIBS/LIBDIRS
shape as the other platforms.

Output: `<game-name>.3dsx` (homebrew-launcher format) and/or
`<game-name>.cia` depending on the Makefile's target list.

## Games

- **simon-sez** — button-pattern memory/reaction game. Watch a growing
  sequence of A/B/X/Y flashes, then repeat it back; each correct round
  adds a step and speeds up. Console-text-API only (no citro2d needed).
