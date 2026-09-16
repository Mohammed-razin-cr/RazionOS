# Razion AI security and privacy

## Trust boundaries

The root-owned Razion AI Engine is a small policy and routing service. It does
not contain provider networking code and does not execute model output.
Provider adapters run as separate PEX services, and applications communicate
through bounded, versioned request and response structures.

Razion Pulse is a separate native action broker. It accepts only known action
identifiers, reconstructs canonical action definitions, checks capabilities,
requires confirmation for changes, and uses fixed operating-system calls.
Generated text is never passed to a shell.

## Local-first policy

- llama.cpp is the enabled provider in the initial configuration.
- Ollama remains an optional disabled provider.
- The tested VirtualBox setup binds llama-server to Windows host loopback;
  guest access uses the explicitly enabled NAT loopback route.
- Cloud providers are disabled placeholders.
- Cloud use requires `allow_cloud=1` and an `ALLOW_CLOUD` request flag.
- `LOCAL_ONLY` prevents cloud fallback regardless of system policy.
- RazionOS does not bundle a model or silently download one.

## Data handling

Protocol payloads are bounded to 767 bytes in version 1. Audit records contain
metadata only: time, application, operation, provider, result, flags, and
capabilities. Prompts, generated content, file contents, and credentials are
not written to the engine audit log.

The llama.cpp and Ollama adapters send the typed request payload and a bounded
operation instruction, not unrelated user files. They have no direct user-file,
process, settings, package, or shell access. Future
features that need operating-system data must receive it from a
permission-scoped native broker.

## Configuration and credentials

`/etc/razion-ai.conf` contains provider policy, not secrets. The repository
must not contain API tokens. Future cloud adapters must use a protected
credential facility and must never log authorization headers or secret
values.

Remote model-server access is an explicit development option. Because the
local API is plain HTTP in this release, enabling a non-local host can disclose
prompts to the network and is unsuitable for untrusted networks.

## Fail-closed behavior

Malformed packets, unsupported capabilities, unavailable providers, timeouts,
missing confirmation, and privileged actions without a dedicated broker are
rejected. AI failure does not block boot, the desktop, or native applications.
