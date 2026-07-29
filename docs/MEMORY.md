# Razion Memory

## Purpose

Razion Memory will provide a privacy-controlled activity index for semantic
search across user-approved files, applications, projects, commands,
downloads, clipboard entries, and recent documents.

It is an index, not an unrestricted recording service.

## Planned architecture

```text
Approved data source
   -> metadata extractor
   -> content policy and redaction
   -> local index
   -> optional local embeddings
   -> scoped search API
```

## Privacy requirements

- disabled until the user completes setup
- per-source opt-in and removal controls
- visible indexed-location list
- local storage by default
- no cloud upload without a separate, explicit permission
- clipboard indexing off by default
- bounded retention and complete index deletion
- applications receive only results allowed by their capabilities

## Phase status

Memory is planned for Phase 5. Phase 1 does not scan, copy, embed, or index
user data.
