# Razion AI SDK

## Overview

The native Phase 1 SDK is provided by:

```c
#include <toaru/razion_ai.h>
```

Link applications with `-ltoaru_razion_ai`. The repository dependency
generator recognizes the header and adds the library automatically.

Applications talk to `/dev/pex/razion-ai`; they do not select endpoints,
format provider-specific HTTP requests, or handle provider credentials.

Provider implementers use `<toaru/razion_ai_provider.h>`. Its common interface
keeps provider-specific behavior behind isolated adapter services and requires
unsupported capabilities to be reported explicitly.

## Initialize

```c
razion_ai_context_t ai;
razion_ai_response_t response;

if (razion_ai_init(&ai, "example-app")) {
    /* SDK initialization failed */
}
```

The application identifier is bounded and included in audit metadata.
Contexts default to local-first routing, text capability, and a five-second
engine response timeout.

## APIs

```c
razion_ai_ask(&ai, input, &response);
razion_ai_search(&ai, query, &response);
razion_ai_analyze(&ai, data, &response);
razion_ai_run_task(&ai, intent, flags, &response);
razion_ai_summarize(&ai, text, &response);
razion_ai_generate_code(&ai, request, &response);
razion_ai_explain_error(&ai, error, &response);
razion_ai_search_files(&ai, query, &response);
razion_ai_analyze_logs(&ai, scope, &response);
razion_ai_process_info(&ai, query, &response);
razion_ai_disk_info(&ai, query, &response);
razion_ai_network_diagnostics(&ai, query, &response);
razion_ai_system_health(&ai, query, &response);
razion_ai_application_search(&ai, query, &response);
razion_ai_voice_command(&ai, transcript, &response);
razion_ai_document_search(&ai, query, &response);
razion_ai_settings_request(&ai, request, &response);
razion_ai_classify_intent(&ai, request, &response);
razion_ai_health(&ai, &response);
razion_ai_list_providers(&ai, &response);
```

The C names correspond to the public concepts:

| Public API | Native symbol |
| --- | --- |
| `RazionAI.ask()` | `razion_ai_ask()` |
| `RazionAI.search()` | `razion_ai_search()` |
| `RazionAI.analyze()` | `razion_ai_analyze()` |
| `RazionAI.runTask()` | `razion_ai_run_task()` |
| `RazionAI.summarize()` | `razion_ai_summarize()` |

Typed APIs are also reserved for code generation, error explanation, file and
document search, log analysis, process and disk information, network
diagnostics, system health, application search, optional voice commands, and
settings proposals. They use the same policy and provider-routing path.

`razion_ai_classify_intent()` is used by Razion Pulse only after its offline
classifier does not recognize a request. Pulse sets `LOCAL_ONLY`, and accepts
the response only when it exactly identifies an action in its built-in,
canonical vocabulary.

## Razion Pulse SDK

Phase 2 also provides:

```c
#include <toaru/razion_pulse.h>
```

Link with `-ltoaru_razion_pulse`. The proposal API never executes:

```c
razion_pulse_proposal_t proposal;
razion_pulse_status_t status =
    razion_pulse_propose(request, &proposal);
```

Execution is a separate call with an explicit application context and
confirmation value:

```c
razion_pulse_context_t pulse;
char result[512];

razion_pulse_init(&pulse, "example-app",
    RAZION_PULSE_CAP_APP_LAUNCH |
    RAZION_PULSE_CAP_SYSTEM_READ);
status = razion_pulse_execute(
    &pulse, &proposal, confirmed, result, sizeof(result));
```

The executor reconstructs the canonical action definition before applying
policy. It ignores caller-supplied descriptive targets when selecting the
native implementation.

## Result handling

A function result of zero means the engine returned a structurally valid
response. The operation result is then available in `response.status`.
A negative function result indicates an IPC timeout, unavailable engine, or
invalid protocol reply and sets `errno`.

```c
if (!razion_ai_summarize(&ai, text, &response)) {
    if (response.status == RAZION_AI_STATUS_OK) {
        printf("%s\n", response.payload);
    } else {
        fprintf(stderr, "%s\n",
            razion_ai_status_string(response.status));
    }
}
```

Payloads are bounded to 767 bytes in protocol version 1. Larger document
operations will use handles to permission-scoped data sources in a future
protocol version rather than placing whole files in PEX packets.

## Cloud policy

Cloud routing requires both:

1. `allow_cloud=1` in the system configuration.
2. `RAZION_AI_FLAG_ALLOW_CLOUD` on the individual request.

`RAZION_AI_FLAG_LOCAL_ONLY` prevents fallback. Applications should expose a
clear user control before setting the cloud flag.

## Task safety

`razion_ai_run_task()` requests an action proposal. Setting
`RAZION_AI_FLAG_EXECUTE_ACTION` without
`RAZION_AI_FLAG_USER_CONFIRMED` returns `CONFIRMATION_REQUIRED`. Direct engine
execution remains denied even with confirmation. Applications must validate a
proposal through a capability-scoped native broker such as Razion Pulse.

## Diagnostics

From a RazionOS terminal:

```text
razion-ai-status health
razion-ai-status providers
```

The utility reports service and provider state only; it is not a chatbot.
Provider availability reflects a real adapter health probe, including whether
Ollama is reachable and has a usable installed model.
