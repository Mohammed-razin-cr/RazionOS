# Razion Browser engine status

Razion Browser is a native Yutani application with real tab management,
per-tab back and forward history, reload, home, address entry, session
bookmarks, history, downloads, settings, network-status detection, and an
optional user-triggered local-only Razion AI search path.

RazionOS does not currently include a sandboxed HTML/CSS/JavaScript rendering
engine. The inherited Help Browser renders a small local markup language, and
`fetch` can transfer HTTP data, but neither is a standards-compliant web
platform. Razion Browser therefore never displays synthetic remote content or
claims that an address was rendered. Online and offline states are explicit.

## Native shell interface

Ripper uses a compact two-level browser chrome implemented directly with the
existing Yutani and Razion graphics libraries. The first level contains the
tab strip, navigation controls, address field, location-state badge, Go action,
and browser menu. Bookmarks, history, downloads, and privacy are grouped on a
separate secondary row so they do not compete with page navigation.

The shell provides visible hover, press, active, disabled, and keyboard-focus
states. Tab and Shift+Tab traverse enabled controls in visual order. Ctrl+L
focuses and selects the address, Ctrl+D toggles the current bookmark, Ctrl+T
opens a new tab, Ctrl+W closes the active tab, Ctrl+Tab changes tabs, and
Alt+Left or Alt+Right traverses history. The overflow menu traps focus while
open, supports Arrow-key navigation, and closes with Escape or an outside click.
Pointer motion redraws the double buffer only when the hovered or pressed
control changes. Network-device status is cached between explicit navigation
or reload actions instead of being queried during paint, keeping the native
software-rendered interface responsive.

The design follows familiar desktop-browser interaction conventions, but its
visual hierarchy, RazionOS semantic colors, Ripper identity, and local-first
status surfaces are original to RazionOS.

## Engine acceptance requirements

A future engine integration must provide:

- process isolation from the browser shell;
- TLS certificate and hostname verification;
- bounded IPC for navigation, pixels, input, accessibility, and downloads;
- origin-scoped cookies, storage, permissions, and cache controls;
- content-process crash recovery;
- explicit download destinations and integrity reporting;
- no page or selection data sent to AI without a user action and visible
  provider policy.

Until those requirements are met, external navigation remains an honest
engine-status page. The shell stays useful for exercising native browser state
and future engine IPC without pretending that web content exists.
