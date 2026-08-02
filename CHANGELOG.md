# Changelog

All notable RazionOS milestone changes are recorded here.

## Unreleased — Modern desktop milestone

### Added

- Modern File Manager operations: create, rename, copy, cut, paste, safe and
  permanent deletion, properties, current-folder search, sorting, multiple
  selection, context menus, keyboard shortcuts, and same-window folder drag
- Per-user Recycle Bin with collision-safe storage, metadata, restore, size,
  empty, cross-filesystem moves, desktop launcher, and confirmation before
  permanent deletion
- Wallpaper placement modes with PNG/JPG/JPEG discovery, preview, persistence,
  desktop context access, and a native Settings application
- Razion AI response cache, configurable retries, health statistics, async SDK
  begin/poll/cancel operations, Razion Chat, and Settings integration

### Verified

- Clean x86_64 build and bootable BIOS/UEFI ISO generation
- VirtualBox desktop boot, graphical folder creation, Recycle Bin move and
  restore, Settings launch, and graceful AI-provider-unavailable behavior

### Known limitations

- Protocol version 1 returns one completed response to the stream callback;
  true multi-chunk streaming requires a protocol update
- Ollama and models are optional external components and are not bundled
- Cloud providers stay disabled pending verified TLS and protected key storage
- Cross-window file drag payloads are not available in the current compositor

## 0.1 Alpha

First public milestone.

### Added

- RazionOS 0.1 Alpha product identity across boot, login, desktop, terminal,
  system information, and user-facing documentation
- Razion Desktop presentation layer and Razion Dark visual system
- RazionOS wallpaper, logos, desktop styling, menus, dialogs, and system bar
- Central identity and semantic theme headers for future development
- Build, architecture, UI, design, rebranding, and VirtualBox documentation
- Current RazionOS and historical baseline screenshots
- Razion AI Engine Phase 1 with a native SDK, versioned IPC protocol,
  local-first provider routing, explicit cloud opt-in, and metadata-only audit
  logging
- Provider-independent APIs for AI-assisted search, analysis, diagnostics,
  code, documents, applications, settings proposals, and optional voice input
- Razion Pulse Phase 2 with an offline command vocabulary, local-only
  provider classification fallback, canonical native actions, risk policy,
  explicit confirmation, and per-user metadata auditing
- Safe native Pulse actions for launching selected applications, opening
  standard directories, reporting memory use, and creating a fixed Python
  project without overwriting existing files
- Isolated Ollama provider service with a common provider interface, automatic
  installed-model discovery, bounded local HTTP/JSON requests, real health
  reporting, latency tracking, local-only defaults, and graceful offline
  behavior
- Health-aware provider selection and failover with short-lived status caching
- Provider manager and AI security/privacy documentation

### Verified

- Clean x86_64 build with the pinned Docker toolchain
- Hybrid live ISO generation with BIOS and x86_64 UEFI boot entries
- Interactive desktop boot in VirtualBox
- Razion AI Engine startup, health reporting, provider discovery, and audit
  metadata in the live system
- Razion Pulse memory reporting, native directory launch, JSON planning,
  confirmation enforcement, overwrite protection, privileged denial, and
  metadata-only auditing in VirtualBox

### Origins

RazionOS 0.1 Alpha is derived from ToaruOS. Original authorship, copyright
notices, license terms, and component provenance are preserved in the
repository.
