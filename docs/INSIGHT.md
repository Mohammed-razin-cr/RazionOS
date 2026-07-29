# Razion Insight

## Purpose

Razion Insight will convert system telemetry into plain-language,
evidence-backed diagnostics. It will explain observed behavior rather than
merely restating CPU, memory, disk, or network percentages.

## Planned data sources

- process and thread information from `/proc`
- CPU and memory counters
- block-device activity
- network interface and DNS diagnostics
- service health and recent system logs

Collection remains deterministic operating-system code. AI receives a
bounded, structured summary instead of unrestricted access to `/proc`, logs,
or user files.

## Diagnostic contract

Every explanation should include:

- observed condition
- supporting measurements
- likely cause, labeled as an inference
- safe suggested action
- confidence or insufficient-data state

Insight may propose actions through Pulse, but it may not execute them.

## Phase status

Insight is planned for Phase 4. The Phase 1 SDK reserves
`RazionAI.analyze()` and the `SYSTEM_READ` capability for this work; no
background telemetry collection has been enabled yet.
