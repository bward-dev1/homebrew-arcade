# GBA

Built with devkitPro's `gba-dev` pacman group (devkitARM + libgba + libtonc +
libmm). `gba-dev` pulls in libtonc as part of the group — no separate
`pacman -S libtonc` is needed.

Toolchain env vars set by the workflow:

- `DEVKITPRO=/opt/devkitpro`
- `DEVKITARM=/opt/devkitpro/devkitARM`

`gba_rules` (included from every game's Makefile via
`include $(DEVKITARM)/gba_rules`) defines `LIBGBA` for you; `LIBTONC` is not
auto-defined and each Makefile sets it explicitly as
`$(DEVKITPRO)/libtonc`, matching devkitPro's own libtonc-template.

Output: `<game-name>.gba`, plus an intermediate `.elf` from the link step.

## Games

- `games/pixel-jumper/` — single-screen endless jumper, mode 3 bitmap
  rendering, libtonc for input/video/vblank helpers, fixed-point (8.8)
  physics so it doesn't pull in libgcc's soft-float routines.
