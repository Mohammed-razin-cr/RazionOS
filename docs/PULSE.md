# Razion Pulse

## Phase 2 status

Razion Pulse is the operating-system command interface built on the Razion AI
Engine. It is not a chatbot and it never treats generated text as a shell
command.

Phase 2 provides:

- the `pulse` one-shot and interactive command
- a native `libtoaru_razion_pulse` application API
- deterministic offline classification for common operating-system requests
- local-only AI Engine classification fallback for unknown wording
- operational local Ollama classification when a runtime and model are
  configured, with deterministic offline behavior when they are not
- a bounded action vocabulary and canonical proposal validation
- risk labels, capability checks, explicit confirmation, and metadata-only
  auditing
- fixed native implementations for application launch, directory open,
  memory status, and Python project creation
- fail-closed proposals for actions whose native brokers belong to later
  phases

## Request flow

```text
User request
   -> offline intent classifier
   -> optional local AI Engine classifier
   -> known action identifier validation
   -> canonical action proposal
   -> capability and risk policy
   -> confirmation when required
   -> fixed native operating-system API
   -> metadata-only audit result
```

The local classifier covers the standard Pulse vocabulary without a model or
network connection. If it does not recognize the wording, Pulse may submit a
`CLASSIFY_INTENT` request to the AI Engine with `LOCAL_ONLY`. The Ollama
adapter uses a constrained classification prompt when a local runtime and
model are available. A provider may return only a known action identifier such
as `open-downloads`. Any other response is rejected.

## Command interface

```text
pulse "Open Downloads"
pulse "Check memory usage"
pulse --plan "Create a Python project"
pulse --confirm "Create a Python project"
pulse --json "Restart networking"
```

Launching Razion Pulse from the Applications menu opens a terminal and asks
for one operating-system request. `--plan` never executes. `--json` produces a
machine-readable plan and implies `--plan`.

## Implemented action vocabulary

| Action identifier | Risk | Capability | Phase 2 behavior |
| --- | --- | --- | --- |
| `open-terminal` | read | app launch | launch fixed native binary |
| `open-calculator` | read | app launch | launch fixed native binary |
| `open-file-browser` | read | app launch | launch fixed native binary |
| `open-system-monitor` | read | app launch | launch fixed native binary |
| `open-wallpaper-settings` | read | app launch | launch fixed native binary |
| `open-home` | read | directory open | open fixed user directory |
| `open-downloads` | read | directory open | open fixed user directory |
| `check-memory` | read | system read | read `/proc/meminfo` directly |
| `create-python-project` | change | file create | require confirmation; never overwrite |
| `find-recent-pdf` | read | file search | open bounded offline Universal Search with a PDF query |
| `restart-networking` | privileged | network admin | denied; no broker yet |
| `install-nodejs` | privileged | package management | denied; no broker yet |
| `summarize-today` | read | activity read | proposal only until Phase 5 |

## Confirmation and execution

Read-only actions with an available native implementation may execute
immediately. A change action returns `confirmation-required` unless the user
explicitly supplies `--confirm`.

Python project creation uses a fixed target,
`~/Projects/python-project`, and creates only `main.py`. It refuses an existing
target and removes incomplete output if creation fails.

Privileged and destructive actions are never enabled by `--confirm` alone.
They remain unavailable until RazionOS has a dedicated authenticated broker
for the required capability.

## Security boundary

The proposal received by the executor is not trusted. Before execution, the
library rebuilds the canonical proposal from its action identifier and
verifies that the numeric action matches. Descriptive targets and capability
fields supplied by callers or providers cannot redirect execution.

Native actions use fixed `execv`, filesystem, or procfs calls. Pulse does not
invoke a command shell. The Phase 2 library runs with the calling user's
ordinary OS permissions; its capability labels enforce Pulse policy but are
not a replacement for future kernel-enforced capabilities.

Audit records contain timestamp, application identifier, action identifier,
risk, outcome, and capability mask. Request text is not logged. Per-user
records are stored in `~/.razion/pulse-audit.log`.

## Native SDK

Applications include:

```c
#include <toaru/razion_pulse.h>
```

and use:

```c
razion_pulse_propose(request, &proposal);
razion_pulse_execute(&context, &proposal, confirmed, result, result_size);
```

Link with `-ltoaru_razion_pulse`. The dependency generator recognizes the
header automatically.

## Remaining roadmap

- Universal Search now supplies the first bounded, permission-scoped metadata
  search broker. Persistent indexing and arbitrary natural-language query
  parameters remain future work.
- Phase 5 supplies the activity index used by `summarize-today`.
- Privileged network and package actions require separate authenticated
  brokers before they can become executable.
- Later provider work may add streaming, embeddings, vision, speech, and model
  management; the initial Ollama text adapter is operational.
