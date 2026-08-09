# Razion AI architecture

## Current status

Phases 1 and 2 establish Razion AI as an operating-system service, add Razion
Pulse as its safe command interface, and provide an operational isolated
Ollama adapter. RazionOS does not ship a chatbot, bundle a model, contact a
cloud provider by default, or grant AI unrestricted system access.

Implemented components:

- `razion-ai-engine` — root-owned policy and routing service
- `libtoaru_razion_ai` — native application SDK
- versioned request/response protocol
- configuration-driven provider registry
- local-first selection and explicit cloud opt-in
- provider isolation through PEX service endpoints
- metadata-only audit logging
- fail-closed action execution policy
- `razion-ai-status` diagnostic utility
- typed operation identifiers for files, documents, applications, code,
  errors, logs, processes, disks, networking, health, settings, and voice
- Razion Pulse Phase 2 command interface and native action broker
- offline intent classification with validated local-provider fallback
- canonical action proposals with risk, capability, and confirmation policy
- a common provider interface covering lifecycle, health, latency, chat,
  streaming, summarization, embeddings, vision, speech, cancellation, and
  capabilities
- an isolated Ollama adapter with bounded HTTP/JSON handling, automatic local
  model discovery, real health probes, and explicit unsupported capabilities
- bounded offline Universal Search, reused by Pulse for the implemented local
  PDF-search action

## System position

```text
User applications
        |
Razion AI SDK (libtoaru_razion_ai)
        |
Razion AI Engine (/dev/pex/razion-ai)
        |
Provider manager and policy enforcement
        |
Isolated provider adapter services
        |
Local model runtimes or cloud APIs
```

Applications communicate only with the engine. Provider adapters expose a
common versioned protocol through their own PEX endpoints and are never loaded
into the privileged engine process. This keeps third-party provider code
outside the trust boundary of the central policy service.

## Startup and failure isolation

`/etc/startup.d/60_razion_ai.sh` starts the service before the graphical
session. The daemonizing parent exits immediately so it cannot delay desktop
startup. `no-razion-ai` disables the service.

RazionOS remains fully operational if the engine or every provider is
unavailable. SDK calls return an explicit error, while the desktop, terminal,
file manager, networking, and normal operating-system APIs continue without
AI.

## IPC protocol

The protocol is declared in `<toaru/razion_ai.h>`. Each packet contains:

- magic and protocol version
- operation and request identifier
- request policy flags
- requested capability bits
- stable application identifier
- bounded UTF-8 payload

Packets fit within the existing 1024-byte PEX limit. The SDK validates the
magic, version, request identifier, length, and response bounds before
returning data to an application.

## Provider manager

Providers are declared in `/etc/razion-ai.conf`. Each descriptor has:

- provider name
- isolated PEX endpoint
- `local` or `cloud` class
- capability mask
- priority
- enabled state

The manager filters providers by capability and policy, then chooses the
lowest-priority-number provider in the preferred class. Local providers are
evaluated first by default. Cloud providers require both system-level
`allow_cloud=1` and an application request carrying `ALLOW_CLOUD`.

The initial configuration defines entries for Ollama, Groq, Gemini,
OpenRouter, Hugging Face, Anthropic, and OpenAI. Ollama is the only enabled
entry and is the preferred local adapter. It uses a separate service and
localhost-only configuration by default. Ollama itself and model weights are
not bundled. Cloud entries are disabled.

The engine checks adapter health through the provider protocol, caches the
result briefly, and treats connection failures, timeouts, malformed responses,
and unavailable/internal results as failover conditions.

## Security model

The engine is a policy broker, not a shell.

- Provider code runs out of process.
- Cloud use is disabled by default.
- Prompt contents are never written to the audit log.
- Audit records contain time, PEX source identifier, application identifier,
  operation, selected provider, status, flags, and requested capabilities.
- Requests carrying `EXECUTE_ACTION` require a confirmation flag and remain
  denied because provider output cannot execute actions directly.
- No provider receives a task-execution capability.
- No API key belongs in the repository or `/etc/razion-ai.conf`.

Pulse Phase 2 executes only fixed, non-privileged native actions. Future
privileged action execution must call capability-scoped operating-system
brokers; it must never pass generated text directly to a shell.

## Roadmap

1. **Complete:** engine, provider abstraction, SDK protocol, policy baseline.
2. **Complete:** Pulse command interface, canonical native action proposals,
   offline vocabulary, confirmation, and audit policy.
3. **Partial:** bounded in-memory file metadata search and Pulse PDF-search
   integration are complete; persistent user-controlled indexing is not.
4. Insight diagnostics.
5. Memory activity index.
6. Natural-language settings.
7. **Complete baseline:** operational Ollama text adapter; future work adds
   optional model management and richer inference capabilities.
8. Stable third-party SDK and plugin release.
