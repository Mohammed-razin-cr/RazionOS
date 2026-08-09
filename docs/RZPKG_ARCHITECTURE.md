# rzpkg architecture

`rzpkg` is the future native package transaction interface for RazionOS. The
current milestone implements its metadata model, strict parser, validation
diagnostics, repository readiness check, command-line inspection surface, and
the Razion Store consumer. It does not install, update, or remove software.

## Transaction boundary

The intended transaction path is:

```text
Razion Store
  -> signed repository index
  -> rzpkg metadata validation
  -> dependency resolver
  -> archive checksum and signature verification
  -> requested-permission review
  -> privileged transaction service
  -> atomic installation and audit record
```

Every stage must succeed before an install control can become available. The
current `rzpkg install`, `update`, and `remove` commands fail closed with exit
status 69 because the signed index, signature verifier, dependency resolver,
and privileged transaction service are not configured.

## Metadata

The public `toaru/rzpkg.h` interface models name, version, architecture,
description, installed size, dependencies, permissions, maintainer, license,
SHA-256 checksum, and a future signature envelope. Metadata is parsed as
bounded `key=value` records. Unknown fields, invalid characters, malformed
digests, missing required values, and unsupported architectures are rejected.

Signature text may be carried but is never treated as verified by the parser.
Verification belongs to a dedicated trust service with an updateable keyring.
Package scripts are not supported by this milestone and are never executed.

## Offline behavior

Razion Store uses a built-in catalogue of actual applications shipped in the
system image. It derives installed state and binary sizes from the filesystem.
Updates and downloads explicitly report that no signed repository service is
configured. Optional AI application search goes through the official Razion
AI IPC API, is local-only by default, and cannot start a package transaction.
