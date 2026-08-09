# RazionOS documentation

## Release and user documentation

- [Build RazionOS](BUILDING_RAZIONOS.md) — reproducible x86_64 build process
  and artifact validation
- [VirtualBox](VIRTUALBOX.md) — tested virtual-machine configuration and
  historical baseline evidence
- [Changelog](../CHANGELOG.md) — public milestone summary
- [Modern desktop milestone](DESKTOP_MILESTONE.md) — File Manager, Recycle Bin,
  wallpaper, Settings, AI responsiveness, limitations, and verification
- [Universal Search](UNIVERSAL_SEARCH.md) — offline capability and bounded
  home-directory metadata search
- [Razion Companion](features/RAZION_COMPANION.md) — optional offline desktop
  companion architecture, customization, persistence, and performance model

## Architecture and design records

- [Razion AI architecture](AI_ARCHITECTURE.md) — engine, IPC, Pulse,
  provider routing, security boundary, and roadmap
- [AI provider manager](AI_PROVIDER_MANAGER.md) — common provider interface,
  Ollama adapter, health-aware routing, and configuration
- [AI security and privacy](AI_SECURITY.md) — trust boundaries, local-first
  policy, data handling, and fail-closed behavior
- [Security model](SECURITY_MODEL.md) — enforced protections, trust boundaries,
  limitations, and invariants for future work
- [Razion AI SDK](AI_SDK.md) — native application API and result handling
- [Razion Pulse](PULSE.md) — implemented safe operating-system command
  interface and native action policy
- [Razion Insight](INSIGHT.md) — explainable system diagnostics design
- [Razion Memory](MEMORY.md) — privacy-controlled semantic activity index
- [Architecture survey](RAZIONOS_ARCHITECTURE.md) — Stage 1 upstream baseline
  and subsystem map
- [Rebranding audit](RAZIONOS_REBRANDING.md) — identity decisions and
  deliberately preserved technical/upstream names
- [UI architecture](RAZIONOS_UI_ARCHITECTURE.md) — Stage 2 desktop component
  map and verification
- [Design system](RAZIONOS_DESIGN_SYSTEM.md) — Razion Dark tokens, typography,
  and asset-generation record

## Screenshots

Current RazionOS 0.1 Alpha screenshots are stored in `screenshots/`.
Historical, clearly labeled ToaruOS baseline evidence is stored in `assets/`
and is referenced only by the baseline build and VirtualBox records.
