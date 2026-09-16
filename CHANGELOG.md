# Changelog

All notable RazionOS milestone changes are recorded here.

## Unreleased — Modern desktop milestone

### Added

- Native isolated llama.cpp provider adapter using `/v1/models` health/model
  discovery and `/v1/chat/completions` generation, with VirtualBox-host defaults,
  bounded responses, timeouts, and AI Engine routing
- Idempotent Windows-host llama.cpp launcher with loopback-only binding, loaded-
  model readiness check, and a bounded 30-second Pulse AI-classification wait
- Reliable executable permissions for every ramdisk startup hook, including
  persistent home, the permission broker, and Razion AI services
- Improved the native game suite: Snake now provides ready, pause, focus-pause,
  adaptive-speed, best-score, WASD, and pointer states; Tiles now provides undo,
  best score, WASD, pointer actions, and 2048/no-moves feedback
- Razion Tic-Tac-Toe, a native pointer- and keyboard-accessible strategy game
  with session scoring, cached minimax strategy, and state-change-only redraws
- Faster game UX pass: Snake launches movement on the first direction key,
  Tiles explains blocked actions through status feedback, and Tic-Tac-Toe uses
  solved opening moves, immediate win/block checks, and alpha-beta pruning
- Native Razion Assistant panel with chat mode, AI file-search mode, Pulse
  handoff, provider-status handoff, and local Launcher fallback
- Razion System Center for persistent-storage status, milestone update status,
  notification controls, screenshot actions, lock screen, About, boot status,
  and system information entry points
- Discoverable lock-screen overlay, System Center, Assistant, AI file search,
  update status, notification status, and screenshot actions in Settings,
  Launcher, Quick Settings, and the panel menu
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
- State-change-only redraws and complete keyboard focus/navigation for Settings
  and Quick Settings, with non-interactive hardware status cards excluded from
  pointer and keyboard activation
- Original Ripper browser chrome with compact tabs, vector navigation controls,
  a location-aware address field, responsive new-tab surface, full keyboard
  shortcuts/focus navigation, a focus-managed overflow menu, direct bookmark
  control, cached network status, and state-change-only pointer repainting
- Offline Universal Search across a reviewed application/settings/tool
  catalogue and bounded visible filename metadata under the user's home
- Native `libtoaru_razion_search` ranking and collection API, Super+Space and
  Applications-menu entry points, and Pulse PDF-search integration
- Optional native Razion Companion with 22 data-driven species, original
  procedural artwork, persistent needs and customization, subtle local
  notifications, a reflex activity, and a low-idle-cost desktop overlay
- A repository-wide security model that distinguishes current protections from
  planned kernel capabilities, Capsules, credential storage, and hardening

### Verified

- Clean x86_64 build and bootable BIOS/UEFI ISO generation
- VirtualBox desktop boot, graphical folder creation, Recycle Bin move and
  restore, Settings launch, and graceful AI-provider-unavailable behavior
- End-to-end llama.cpp provider health and chat in VirtualBox, AI-assisted Pulse
  Downloads proposal and native folder launch, plus offline Pulse fallback
- Universal Search unit regression, offline CLI matching, Pulse file-search
  capability enforcement, native UI launch, and a 23 ms query over the live
  image's 24 searchable items in VirtualBox
- Razion Companion engine regression, clean hybrid ISO packaging, native
  control-center launch, transparent desktop overlay, and disabled-state
  reboot behavior in VirtualBox

### Known limitations

- Protocol version 1 returns one completed response to the stream callback;
  true multi-chunk streaming requires a protocol update
- Ollama and models are optional external components and are not bundled
- Cloud providers stay disabled pending verified TLS and protected key storage
- Cross-window file drag payloads are not available in the current compositor
- Universal Search is an in-memory filename metadata scan limited to 2,048
  visible items and eight directory levels; persistent and semantic indexing
  are not implemented

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
