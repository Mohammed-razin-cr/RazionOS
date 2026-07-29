# RazionOS boot animation

## Overview

RazionOS 0.1 Alpha uses a native, event-driven loader designed for the
project. Its visual language combines matte black, electric blue, soft cyan,
and restrained white type around an original monoline Razion mark. It is not
based on another operating system's logo or animation.

The loader runs inside the desktop compositor. This is the same tested
graphics path that renders the RazionOS desktop, so no second process takes
ownership of the framebuffer during the handoff.

## Rendering pipeline

1. The bootloader starts the kernel with `vid=auto` and the selected boot
   options.
2. The kernel and `splash-log` retain the existing text startup path while
   hardware and core services initialize.
3. `99_runstart.sh` closes the startup logger and launches the compositor with
   `--razion-boot-fade` for a normal graphical boot.
4. The compositor draws the loader directly into its existing double-buffered
   backend. The logo is rendered procedurally as seven anti-aliased vector
   segments; no boot-time bitmap or extra rendering library is used.
5. The progress line remains indeterminate while the session creates its
   desktop surfaces. It does not display a fabricated percentage.
6. The compositor treats the real wallpaper (`bottom_z`) and panel (`top_z`)
   surfaces as the desktop-ready milestone.
7. The loader shows its ready state, scales down slightly, and fades over the
   already-rendered desktop in 420 milliseconds.

## Animation timeline

| Phase | Trigger | Visual |
| --- | --- | --- |
| compositor start | graphics backend ready | matte-black loader frame |
| logo reveal | first 560 ms | vector strokes draw in sequence |
| session launch | desktop processes starting | blue activity pulse |
| desktop ready | wallpaper and panel surfaces registered | full blue line and `Ready` |
| handoff | ready milestone | 420 ms scale-and-opacity fade |

The desktop-ready transition is tied to real compositor state. An eight-second
safety timeout releases the loader for custom sessions that do not create the
standard wallpaper or panel surfaces.

## Configuration

The boot menu provides:

- **Razion boot animation** — enabled by default.
- **Verbose boot output** — uses the existing text path.
- **Fast boot animation** — retained as a boot configuration choice for
  future timing profiles; the current compositor loader prioritizes the real
  desktop-ready event.

The command line accepts:

```text
boot-animation=off
boot-animation-speed=slow
boot-animation-speed=normal
boot-animation-speed=fast
boot-verbose
```

`boot-animation=off`, `boot-verbose`, `debug`, `start=--vga`, and
`start=--headless` bypass the graphical loader.

## Fallback behavior

The loader begins only after the compositor has successfully initialized its
graphics backend. If graphics initialization fails, the existing kernel and
startup console remain the fallback. If the desktop does not publish its
standard surfaces, the eight-second safety timeout exposes whatever session
is available instead of leaving a blank screen.

The older standalone `razion-splash` renderer is not started by the boot
scripts. Keeping framebuffer ownership in the compositor avoids unsupported
pixel-format handoffs on legacy VBE and VboxVGA configurations.

## Performance

The loader uses the compositor's existing redraw loop, clipping, anti-aliased
line renderer, text renderer, and framebuffer flip implementation. Its assets
are procedural, so it performs no image decoding or additional disk I/O. It
does not delay desktop startup: the session launches in parallel, and the
loader exits as soon as the real desktop surfaces are ready.

## Modified files

- `apps/compositor.c` — procedural loader, desktop-ready milestone, fade, and
  safety timeout.
- `base/etc/startup.d/99_runstart.sh` — enables the loader for normal graphical
  boots and preserves text-mode fallbacks.
- `boot/config.c` — boot menu options.
- `assets/boot/` — original logo, palette, animation, and theme references.
