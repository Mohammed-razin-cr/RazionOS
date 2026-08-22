# Premium Razion Desktop

This milestone extends the existing native Yutani desktop. It does not add a
web shell, Electron, Chromium, or a second compositor.

## Desktop surfaces

- The top panel retains real window state, audio, network, date, clock, and
  session controls and adds a native quick-settings entry point.
- `razion-dock` is a lightweight shaped Yutani surface with trusted first-party
  launch targets, existing-window focus, active indicators, hover feedback,
  bottom/left/right placement, icon sizing, compact mode, auto-hide, and
  per-user persistence in `~/.razion/desktop.conf`.
- `universal-search` is presented as Razion Launcher. It searches installed
  applications, settings, system tools, and bounded home-directory metadata,
  supports keyboard navigation, and persists a bounded recent-app list.
- The default premium wallpaper is a decoder-compatible 1920×1080 JPEG and is
  still replaceable through the existing Wallpaper Picker.

## Settings and controls

The redesigned native Settings application exposes only implemented controls:

- Dark, Light, and time-based Auto themes;
- five persisted accent choices;
- built-in/user wallpaper selection;
- dock position, size, auto-hide, and compact mode;
- working system, AI, launcher, and information entry points.

Quick Settings reports actual network and mixer availability. Its audio
control writes the existing mixer knob, appearance persists the selected
theme, Do Not Disturb is enforced by `toastd`, and the restart action remains
an explicit privileged operation. Bluetooth, brightness, night light, and
airplane-mode controls are intentionally hidden because this system does not
currently provide working backends for them.

## First-party applications

- Ripper is the stable first-party entry point for the honest native Razion
  Browser shell. No remote HTML/CSS/JavaScript rendering is claimed without a
  sandboxed standards-compliant engine.
- Razion Notes edits and persists local plain text.
- Razion Calendar renders the current system month and supports month
  navigation.
- Razion Media Player controls the existing low-overhead PCM player and states
  its supported input format in the interface.
- System Monitor retains real `/proc` and network-device samples while using
  the shared Razion palette.

## Existing window behavior preserved

The compositor already provides focus and z-order, dragging, resizing,
minimize/unminimize effects, maximize, keyboard tiling and snapping, dialogs,
and shaped overlays. The cursor damage calculation now uses the correct
vertical hotspot offset, preventing stale or missing cursor regions while
moving, resizing, and switching surfaces.

RazionOS does not yet expose a virtual-workspace object model. No fake
workspace switcher is shown; that feature requires compositor protocol and
window-ownership work in a future milestone.
