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

None yet. `games/` exists with a `.gitkeep` so the directory is tracked;
`n3ds.yml` builds cleanly with zero games (logs it and exits 0) until the
first one lands here.
