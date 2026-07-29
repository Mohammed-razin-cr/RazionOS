# Razion AI architecture

## Phase 1 status

Phase 1 establishes Razion AI as an operating-system service. It does not ship
a chatbot, bundle a model, contact a cloud provider, or grant AI unrestricted
system access.

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

Phase 1 defines entries for Ollama, OpenAI, Gemini, Groq, OpenRouter, and
Hugging Face. Ollama is registered as the preferred local adapter endpoint,
but no model runtime or adapter is bundled yet. Cloud entries are disabled.

## Security model

The engine is a policy broker, not a shell.

- Provider code runs out of process.
- Cloud use is disabled by default.
- Prompt contents are never written to the audit log.
- Audit records contain time, PEX source identifier, application identifier,
  operation, selected provider, status, flags, and requested capabilities.
- Requests carrying `EXECUTE_ACTION` require a confirmation flag and are
  still denied in Phase 1.
- No provider receives a task-execution capability in Phase 1.
- No API key belongs in the repository or `/etc/razion-ai.conf`.

Future action execution must call capability-scoped operating-system brokers;
it must never pass generated text directly to a shell.

## Roadmap

1. **Complete:** engine, provider abstraction, SDK protocol, policy baseline.
2. Pulse command interface and native action proposals.
3. Indexed file search.
4. Insight diagnostics.
5. Memory activity index.
6. Natural-language settings.
7. Optional local inference adapter and model management.
8. Stable third-party SDK release.
