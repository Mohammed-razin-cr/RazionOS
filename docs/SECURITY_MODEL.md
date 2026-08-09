# RazionOS security model

## Scope and status

RazionOS is alpha software derived from ToaruOS. It is suitable for isolated
development and virtual-machine testing, not for protecting sensitive data or
running hostile applications. This document separates protections enforced by
the current code from intended future architecture.

The current trust model assumes:

- the boot image and RazionOS source used to build it are trusted;
- the local user is trusted to run native applications available in the image;
- AI providers, model output, network services, and searched file metadata are
  untrusted inputs;
- applications are constrained primarily by Unix user permissions, not by a
  complete kernel sandbox.

## Implemented boundaries

### AI Engine and providers

- Applications call the provider-independent Razion AI SDK through bounded,
  versioned PEX messages.
- The root-owned AI Engine applies provider and request policy.
- Provider networking runs in isolated adapter processes rather than inside
  the Engine.
- Local providers are preferred. Cloud providers are disabled by default and
  require both system opt-in and a request flag.
- Malformed responses, unavailable providers, timeouts, unsupported
  capabilities, and policy failures fail closed.
- AI request and response contents are not written to the Engine audit log.
- API keys are not stored in the repository or provider configuration.

### Pulse actions

- Pulse maps input to a bounded action identifier and rebuilds the canonical
  proposal before execution.
- Provider output cannot select an executable path or supply shell text.
- Implemented actions use fixed native APIs or fixed `execv` targets.
- Mutating actions require explicit confirmation.
- Privileged actions remain unavailable without a dedicated authenticated
  broker.
- Pulse audits metadata such as application, action, risk, capability mask,
  and result; it does not log the user's request text.

Pulse capability bits are enforced by the Pulse library for Pulse calls. They
are policy labels, not kernel-enforced application sandbox permissions.

### Universal Search

- Search operates offline and reads filename metadata only.
- The native UI restricts traversal to `$HOME`, 2,048 entries, and eight
  directory levels.
- Hidden entries and symbolic links are excluded; links are never followed.
- Search results are held in memory and are not persisted or uploaded.
- Selected results open through fixed native executables and direct argument
  arrays. Query text and paths are never passed to a shell.

### File operations and Recycle Bin

- File Manager operations run with the current user's filesystem permissions.
- Recycle Bin storage is per-user and uses collision-safe names and metadata.
- Restore avoids overwriting an existing destination.
- Permanent deletion and emptying the Recycle Bin require confirmation in the
  graphical flows.

These controls reduce accidental damage; they do not constitute a sandbox
against malicious native code running as the same user.

### Failure isolation and availability

- AI services daemonize and do not block graphical desktop startup.
- AI can be disabled with the boot option documented by the AI architecture.
- Desktop, File Manager, Terminal, Settings, system tools, and local search do
  not require an AI provider or Internet access.
- Provider and search errors are surfaced as explicit unavailable or failed
  results rather than fabricated output.

## Credentials and network data

The repository contains no API credentials. The current configuration files
contain provider policy and local endpoints only. A protected credential store
has not been implemented, so cloud provider adapters must remain disabled.

The optional Ollama adapter defaults to localhost. Its HTTP transport does not
protect prompts on an untrusted network; remote Ollama endpoints are a
development option only. RazionOS does not silently download models.

## Security properties not yet implemented

The following must not be claimed as current protections:

- kernel-enforced per-application capabilities;
- Razion Capsule process, filesystem, network, or device isolation;
- secure boot or signed system updates;
- a protected API-key/keychain service;
- a hardened package format with signature and integrity enforcement;
- a complete privilege-separated settings or recovery broker;
- comprehensive exploit mitigations and production-grade driver hardening;
- encrypted user storage;
- content or semantic indexing with per-folder consent controls.

The inherited package UI and package tooling are not the planned `rzpkg`
security architecture. Recovery modes and Capsules remain design work.

## Security invariants for future work

1. AI-generated text must never be executed as a shell command.
2. Provider output must be treated as untrusted data.
3. Destructive actions must show the target and effects, request confirmation,
   and execute through a narrow native broker.
4. Credentials must never be hardcoded, logged, committed, or placed in
   world-readable configuration.
5. File brokers must canonicalize and scope paths, reject traversal outside
   approved roots, and define symbolic-link behavior.
6. Telemetry and diagnostics must use real system measurements and label
   inferences explicitly.
7. AI, network, indexing, and provider failures must not prevent the core
   desktop from working.
8. New isolation claims require an enforced security boundary and documented
   limitations, not only a user-interface label.

## Review priorities

Security review should continue to focus on PEX packet validation, provider
HTTP parsing, configuration bounds, path canonicalization, symbolic-link and
rename races, privilege boundaries, process launching, audit-log permissions,
and all future credential storage. Kernel and libc memory-safety review remains
necessary because the system is primarily implemented in C.
