# NDS

Built with devkitPro's `nds-dev` pacman group (devkitARM + libnds +
libfilesystem/libfat as needed). Toolchain env vars set by the workflow:

- `DEVKITPRO=/opt/devkitpro`
- `DEVKITARM=/opt/devkitpro/devkitARM`

A standard NDS Makefile includes `$(DEVKITARM)/ds_rules` and follows the
same TARGET/BUILD/SOURCES/INCLUDES/LIBS/LIBDIRS shape as the GBA template,
linking against `libnds` (and `libfat`/`libmm`/`libnds9`/`libnds7` as the
game needs — dual-CPU ARM9/ARM7 builds use separate source trees per the
devkitPro NDS template).

Output: `<game-name>.nds`.

## Games (5/5)

- `maze-muncher` - chase/collector maze game
- `grid-slither` - snake
- `card-flip` - card-matching
- `paddle-bounce` - breakout/paddle
- `star-defender` - top-down projectile shooter
