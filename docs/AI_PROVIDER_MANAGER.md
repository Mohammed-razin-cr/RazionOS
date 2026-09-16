# Razion AI provider manager

## Purpose

The provider manager is the routing layer inside `razion-ai-engine`. Native
applications submit typed operations through `libtoaru_razion_ai`; they never
format Ollama requests, choose network endpoints, or load provider code.

The primary operational adapter is located at
`providers/llama_cpp/adapter.c`. It runs as the isolated
`razion-ai-provider-llama-cpp` PEX service. The Ollama adapter remains
available as an optional, disabled alternative.

```text
application -> Razion AI SDK -> Razion AI Engine
                                  |
                         provider policy/health
                                  |
                      isolated llama.cpp adapter
                                  |
                   llama-server OpenAI-compatible API
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
implement. The llama.cpp adapter implements health, capability discovery,
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

## llama.cpp configuration

The primary adapter reads `/etc/razion-ai-llama-cpp.conf`:

```ini
[llama_cpp]
host=10.0.2.2
port=8080
model=
timeout_ms=30000
max_tokens=192
allow_remote=1
```

`10.0.2.2` is the VirtualBox NAT address for the host. An empty model selects
the first ID returned by `GET /v1/models`. Text requests use the bounded,
non-streaming `POST /v1/chat/completions` route. The adapter validates response
structure and never passes model output to a shell.

For the tested VirtualBox NAT setup, enable access to host loopback while the
VM is powered off, then start the server on the Windows host:

```powershell
& 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe' modifyvm 'RazionOS 0.1 Alpha' --nat-localhostreachable1 on
llama-server -hf ggml-org/Qwen3.5-0.8B-GGUF -hff Qwen3.5-0.8B-Q4_0.gguf --host 127.0.0.1 --port 8080 --ctx-size 2048
```

After a Windows reboot, from the repository root you can instead run
`& .\scripts\Start-RazionAI.ps1`. It starts the server in the background,
waits until `/v1/models` advertises a loaded model, and does nothing if a
healthy server is already running. The script never opens the port on the
host's other network interfaces. It does not install a Windows startup task;
the host model must be started again after a host reboot.

Use a model you have permission to download and run; the example is a small
GGUF model, not a RazionOS-bundled component. Keep the terminal open only when
starting `llama-server` directly; the launcher starts it in the background.
The `--nat-localhostreachable1` setting lets the guest reach a server bound
only to host loopback through `10.0.2.2`, without exposing TCP 8080 to the
host's other network interfaces. Verify `http://127.0.0.1:8080/v1/models`
on the host before starting RazionOS. If using a different VM network mode,
configure a reachable host address and restrict access to a trusted network.
Model weights remain on the host and are not bundled in RazionOS.

## Optional Ollama configuration

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

`54_razion_ai_llama_cpp.sh` binds the primary adapter endpoint and
`55_razion_ai_ollama.sh` binds the optional adapter before
`60_razion_ai.sh` starts the engine. Startup is lazy: it does not connect to
Ollama, discover models, or perform inference, so it does not delay the
desktop.

If llama-server is absent, the adapter stays available as a service but its
health operation reports the backend unavailable. The engine then reports zero
usable providers, and Pulse continues to use its deterministic offline
classifier. Normal operating-system functionality never depends on AI.
Pulse gives its optional local-model classification up to 30 seconds, matching
the chat client's bounded wait. Already recognized native actions do not make
an AI request and remain immediate. A timed-out or malformed model response
never executes an action.
The llama.cpp adapter asks for one exact native-broker action token and limits
classification generation to 16 tokens at zero temperature. Pulse reconstructs
the official action definition from that token rather than trusting a model-
provided path, command, or risk label.

## Diagnostics

```text
razion-ai-status health
razion-ai-status providers
razion-chat What is 2 plus 2?
```

With no runtime or model, llama-cpp is shown as `unavailable`. With a reachable
llama-server and loaded model, it is shown as `available`. The chat client
must report `Provider: llama-cpp`. The response depends on the external model;
it can be slow on CPU-only hardware and must not be treated as a deterministic
OS function. In particular, small models may invent claims about RazionOS or
mistake it for Linux. The chat client labels generated answers as unverified;
the source tree and project documentation are authoritative for OS facts.
