# RazionOS UI Architecture

## Existing graphics stack

Razion Desktop is a presentation layer over the existing ToaruOS graphics
architecture:

1. Yutani provides compositing, focus, window movement, resize, input events,
   stacking, blur flags, and window advertisement.
2. `libtoaru_graphics` provides software raster drawing and sprite operations.
3. The decoration loader selects a dynamic theme library; Stage 2 updates the
   existing `decor-fancy` implementation and its small sprite sheet.
4. `libtoaru_menu` centrally renders popup menus and in-application menu bars.
5. `panel` loads independently compiled `libtoaru_panel_*.so` widgets.
6. The file browser provides the desktop surface, wallpaper, icons, labels,
   selection, and context-menu behavior.
7. The text service exposes shared `sans-serif`, bold, and monospace faces.

These names remain unchanged because they are technical interfaces.

## Stage 2 component map

```text
Razion identity constants
├── generated os-release / issue
├── boot, login, tutorial, About, uname
└── rzsh compatibility entry point

Razion semantic theme tokens
├── window decorations
├── popup menus and application menu bars
├── top system bar and active-window list
├── About dialog and login controls
└── terminal palette and shell prompt

Razion Desktop assets
├── razion-dark.jpg
├── login/About R mark
└── terminal-safe system-information mark
```

## System bar and launcher

The current system bar order is:

`[Razion Applications] [active windows] [volume] [network] [weather] [date] [clock] [session]`

Only backed controls are shown. The existing application menu retains icon
rendering, categories, mouse interaction, and keyboard access with Alt+F1.
Alt+F2 remains the working command runner. System Information, About, package
management, wallpaper selection, system monitor, restart, and logout are
exposed through existing commands.

Search-backed launch and a grid view are not added because the menu parser has
no application index or query model. That architecture can later become a
provider for Razion Pulse.

## Performance decisions

- No new compositor protocol or kernel subsystem was introduced.
- The panel continues to use a baked wallpaper-derived background rather than
  requiring real-time blur.
- Decorations retain small cached sprites and software-drawn interaction
  states.
- Fonts and icons continue through shared caches.
- Corner rounding is limited to small controls and menu shells already
  supported efficiently by the graphics library.

## Current limitations

- Theme tokens are compile-time C macros; there is no runtime theme service,
  persisted theme schema, or automatic dark/light switch.
- Most older applications still own their client-area colors, padding, and
  control metrics. Stage 2 centralizes the highest-leverage shared components
  but cannot restyle every application without a broader widget toolkit.
- The terminal is cell-grid based. True content padding would require
  coordinated changes to cell geometry, mouse hit testing, damage regions,
  resize snapping, and selection; Stage 2 therefore changes palette and shell
  presentation without risking terminal correctness.
- Decoration rounding is constrained by rectangular client buffers and
  sprite-based borders; full rounded-window clipping would require compositor
  support.
- Desktop icon layout is fixed by the file-browser implementation and lacks a
  density/spacing preference API.
- The launcher description format supports hierarchy and actions but no
  metadata index, fuzzy search, favorites, or freedesktop-style discovery.
- The current system-information tool queries CPU and memory through helper
  commands rather than a structured settings API.

## Recommended next work

1. Add a runtime theme descriptor and token lookup API while keeping current
   macros as fallbacks.
2. Build a shared Razion widget layer for buttons, inputs, dialogs, lists, and
   spacing metrics.
3. Add launcher metadata/index providers and then implement Razion Pulse on
   top of them.
4. Add compositor-supported rounded clipping and lightweight shadows only
   after profiling.
5. Add desktop layout preferences and an accessible keyboard focus model.
6. Design `rzpkg` as a compatible front end before changing any `msk` package
   contracts.
7. Treat Razion Capsules and Razion Insight as separate security and
   observability stages, respectively.

## Stage 2 verification

The final x86_64 source state was rebuilt after `make clean` with the official
ToaruOS cross-toolchain container. It produced `image.iso` and `ramdisk.igz`
without compilation errors.

The ISO was tested in the existing VirtualBox baseline with VBoxVGA, 1024 MiB
RAM, and two CPUs. Verified results:

- Razion Kernel starts and the graphical desktop reaches 1024×768;
- the Razion wallpaper, system bar, desktop icons, and network/audio/date/
  clock/session widgets render;
- Ctrl+Alt+T opens the terminal and keyboard command entry works;
- hostname reports `razion`, `rzsh -v` reports the compatible shell identity,
  and `uname` reports Razion Kernel and RazionOS;
- Alt+F1 opens the category launcher with keyboard navigation, and Alt+F2
  launches Calculator;
- About RazionOS and the terminal system-information view render with kernel,
  architecture, CPU, and memory information;
- File Browser/desktop, Terminal, Calculator, About, and system information
  all run;
- `sudo reboot` restarts the guest and returns to Razion Desktop.

VirtualBox reports the USB HID mouse and guest graphics capability as attached.
The automated headless interface used for this pass does not provide pointer
motion/click injection, so visual pointer interaction remains a short manual
smoke test rather than an automated assertion.

The first generated JPEG used 4:2:0 chroma subsampling, which the current
ToaruOS JPEG loader did not decode and caused both wallpaper consumers to
exit. The final asset is baseline JPEG with 4:4:4 sampling; the corrected ISO
boots reliably before and after a guest-initiated restart.
