# RazionOS boot animation

## Overview

RazionOS 0.1 Alpha uses a native, milestone-driven boot splash designed for
the project. It is not based on another operating system's visual identity or
animation. The visual language is matte black, electric blue, soft cyan, and
restrained white type around the original monoline Razion mark.

## Rendering pipeline

1. The bootloader starts the kernel with `vid=auto` and the configured boot
   animation options.
2. The kernel retains its existing framebuffer terminal. This remains the
   first graphics fallback and is used whenever a linear framebuffer is not
   available.
3. `00_startuplog.sh` starts `razion-splash`, which forks so init can continue.
   It opens the existing `splash` PEX endpoint and, when graphics are
   available, maps `/dev/fb0` through `init_graphics_fullscreen_double_buffer`.
4. The splash draws its original logo as seven vector stroke segments using
   the built-in graphics renderer. It does not load a boot-time bitmap or add
   a rendering dependency.
5. Startup scripts send real milestones through `/dev/pex/splash`.
6. At `!ready`, the splash fades out and releases the framebuffer.
   `compositor --razion-boot-fade` then applies a short desktop fade-in.

## Real milestones

| Event | Source | Displayed stage |
| --- | --- | --- |
| initial state | `razion-splash` | Initializing Razion Kernel |
| `@razion:core` | `02_hostname.sh` | Loading Core Services |
| `@razion:services` | `04_modprobe.sh` | Starting Device Services |
| `@razion:graphics` | `99_runstart.sh` | Starting Graphics |
| `@razion:desktop` | `99_runstart.sh` | Launching Razion Desktop |
| `!ready` | `99_runstart.sh` | Ready |

The progress line follows these events only. It contains no fabricated
percentages or timer-driven fake loading phases.

## Configuration

The boot menu now provides:

- **Razion boot animation** — enabled by default; disable it for the existing
  framebuffer text boot path.
- **Verbose boot output** — uses the existing console log behavior instead of
  the graphical splash.
- **Fast boot animation** — selects the faster visual easing profile. Normal
  is the default.

The bootloader command-line editor also accepts:

```text
boot-animation=off
boot-animation-speed=slow
boot-animation-speed=normal
boot-animation-speed=fast
boot-verbose
```

`debug` also selects the text boot path so diagnostics are never hidden.

## Fallback behavior

If `/dev/fb0` cannot be opened, the font cannot be loaded, a legacy 24-bit VBE
framebuffer or VboxVGA adapter is detected, animation is disabled, or
verbose/debug output is requested, `razion-splash` keeps the same
PEX endpoint and writes the real startup messages to `/dev/console`. The
kernel framebuffer terminal remains available underneath it, so a graphics
failure cannot leave the display blank.

## Performance

The splash renders at a maximum of 30 frames per second and uses the existing
double-buffered framebuffer API. It performs no asset decoding and no extra
I/O after startup. Milestone updates are delivered through the pre-existing
PEX channel. The only bounded handoff time is the 0.5 second logo exit window,
which allows the framebuffer owner to change without visible tearing; the
desktop compositor fades in during that interval's completion.

## Modified files

- `apps/razion-splash.c` — native splash renderer and console fallback.
- `apps/compositor.c` — optional desktop fade-in at boot.
- `boot/config.c` — boot menu configuration.
- `base/etc/startup.d/00_startuplog.sh` — starts the renderer.
- `base/etc/startup.d/02_hostname.sh`, `04_modprobe.sh`, and
  `99_runstart.sh` — real milestone events and handoff.
- `assets/boot/` — original logo, palette, and animation source references.
