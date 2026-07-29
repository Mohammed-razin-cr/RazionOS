# Razion Pulse

## Purpose

Razion Pulse will be the operating-system command interface built on the
Razion AI Engine. It is not intended to be a general chat application.

Pulse will translate natural-language intent into structured requests such as
opening an application, locating a document, creating a project, inspecting
memory, or proposing a package operation.

## Planned request flow

```text
User intent
   -> intent classification
   -> native action lookup
   -> capability and permission check
   -> preview and confirmation when required
   -> operating-system API
   -> audit result
```

Generated shell commands are display-only suggestions. Pulse must prefer
typed RazionOS APIs and must never send model output directly to a shell.

## Action risk levels

| Level | Examples | Policy |
| --- | --- | --- |
| read | open a folder, check memory | execute through scoped read APIs |
| change | adjust a setting, create a project | show the target and request consent |
| privileged | install software, restart networking | authenticate and confirm |
| destructive | delete or overwrite data | explicit preview and confirmation every time |

## Phase status

Pulse is Phase 2 and is not implemented in RazionOS 0.1 Alpha. Phase 1
provides `RazionAI.runTask()` as an advisory request, but execution flags are
denied by the engine.
