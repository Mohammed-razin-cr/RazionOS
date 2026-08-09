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
