# Razion AI provider manager

## Purpose

The provider manager is the routing layer inside `razion-ai-engine`. Native
applications submit typed operations through `libtoaru_razion_ai`; they never
format Ollama requests, choose network endpoints, or load provider code.

The first operational adapter is located at
`providers/ollama/adapter.c`. It runs as the isolated
`razion-ai-provider-ollama` PEX service.

```text
application -> Razion AI SDK -> Razion AI Engine
                                  |
                         provider policy/health
                                  |
                       isolated Ollama adapter
                                  |
                           Ollama HTTP API
```

## Common provider interface

`<toaru/razion_ai_provider.h>` declares the interface implemented by
provider adapters:

- initialization and shutdown
- availability, health, and observed latency
- chat and bounded text generation
- streaming
- summarization
- embeddings
- vision
- speech
- cancellation
- capability discovery

An adapter must explicitly return `UNSUPPORTED` for methods it does not
implement. The initial Ollama adapter implements health, capability discovery,
chat-style text operations, summarization, code assistance, and action
proposal classification. Streaming, embeddings, vision, speech, and
cancellation remain explicitly unsupported in protocol version 1.

## Selection and failover

Providers are declared in `/etc/razion-ai.conf`. The engine filters them by:

1. enabled state;
2. requested capability mask;
3. local or cloud policy;
4. a live health response from the isolated adapter; and
5. numeric priority, where the lowest value wins.

Health results are cached for five seconds to avoid repeated model-runtime
probes. Connection failure, timeout, malformed protocol data, or a provider
unavailable/internal error marks an adapter unhealthy and allows the manager
to try the next compatible provider. Cloud routing remains disabled by
default and requires system and per-request opt-in.

## Ollama configuration

The adapter reads `/etc/razion-ai-ollama.conf`:

```ini
[ollama]
host=127.0.0.1
port=11434
model=
timeout_ms=4000
max_tokens=192
allow_remote=0
```

An empty model selects the first installed model returned by `/api/tags`.
Setting `model` requires that exact model to be installed. RazionOS does not
bundle Ollama or a model.

The default permits only `127.0.0.1` and `localhost`. For development with
Ollama on a virtual-machine host, set the host address supported by the VM
network and set `allow_remote=1` deliberately. The initial adapter uses
Ollama's plain local HTTP API; do not expose it to an untrusted network.

## Startup and failure behavior

`55_razion_ai_ollama.sh` binds the adapter endpoint before
`60_razion_ai.sh` starts the engine. Startup is lazy: it does not connect to
Ollama, discover models, or perform inference, so it does not delay the
desktop.

If Ollama is absent, the adapter stays available as a service but its health
operation reports the backend unavailable. The engine then reports zero
usable providers, and Pulse continues to use its deterministic offline
classifier. Normal operating-system functionality never depends on AI.

## Diagnostics

```text
razion-ai-status health
razion-ai-status providers
```

With no runtime or model, Ollama is shown as `unavailable`. With a reachable
runtime and installed model, it is shown as `available`.
