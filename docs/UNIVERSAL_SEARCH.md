# Razion Universal Search

## Implemented milestone

Razion Universal Search is a native, offline search interface for capabilities
that actually exist in the current image and for visible file metadata below
the current user's home directory.

It searches:

- installed applications in a fixed, reviewed catalogue;
- implemented Settings destinations;
- native system tools;
- visible file and folder names and paths below `$HOME`.

The interface is available from **Applications > Accessories > Universal
Search** or with **Super+Space**. Arrow keys change the selected result, Enter
opens it, and Escape closes the window. `universal-search --query text`
provides a non-graphical diagnostic interface.

Razion Pulse's existing "find recent PDF" action now opens Universal Search
with a PDF query. Matching files with equal relevance are ordered by
modification time. This remains a user-visible search flow; Pulse does not
open or transmit a file automatically.

## Architecture

```text
User query
   -> native Universal Search window
   -> libtoaru_razion_search
   -> fixed capability catalogue + bounded $HOME metadata walk
   -> relevance and modification-time ranking
   -> fixed native opener selected by result type
```

`libtoaru_razion_search` contains the metadata collector and deterministic
ranking logic. The UI owns the reviewed application catalogue and launch
policy. The implementation has no dependency on the Razion AI Engine or any
provider.

## Privacy and security boundary

- Search reads names, paths, file type, and modification time only.
- File contents are not read, copied, embedded, or uploaded.
- Results exist only in the process memory and are not persisted.
- Dot-prefixed entries are excluded.
- Symbolic links are excluded and never followed.
- Traversal is restricted to the canonical `$HOME` root supplied by the UI.
- At most 2,048 items and eight directory levels are collected.
- Result opening uses fixed `execv` argument lists; query text and file paths
  are never interpreted by a shell.
- Search runs with the calling user's normal filesystem permissions.

The `FILE_SEARCH` Pulse capability is currently an application-level policy
check, not a kernel-enforced sandbox capability. The user can still use normal
filesystem tools outside Pulse within the permissions of their account.

## Verification

`tests/test-razion-search.c` covers recursive collection, token matching,
ranking, hidden-entry exclusion, symbolic-link exclusion, and capacity
truncation. The command-line query mode allows deterministic validation in a
booted RazionOS guest without depending on GUI automation.

The milestone was validated in VirtualBox with the documented BIOS, VBoxVGA,
1 GiB RAM, and two-vCPU profile:

- the clean x86_64 build produced a BIOS/UEFI hybrid ISO;
- `test-razion-search` reported `all tests passed` inside the guest;
- `universal-search --query Read` returned the expected visible README and
  launcher metadata;
- Pulse classified and executed `Find recent PDF` through the `FILE_SEARCH`
  capability, then opened the native Search window;
- the guest shell measured 23 ms real time for a complete command-line query
  over the live image's 24 catalogue and home-directory items.

The release ISO grew by 16,384 bytes and the compressed ramdisk by 18,968
bytes relative to the prior checked artifact. The Search executable is 22,728
bytes and its shared library is 10,928 bytes in the x86_64 build. Search has no
idle CPU or desktop-startup cost because it launches and scans only on demand.

## Known limitations

- This milestone performs a bounded in-memory scan when Search starts; it is
  not yet a persistent or continuously updated index.
- Search covers one user's home directory and the reviewed built-in catalogue,
  not arbitrary mounted filesystems.
- Hidden files and folders cannot be included in this version.
- Filename search is ASCII case-insensitive. Unicode normalization and
  locale-aware matching are not implemented.
- File contents and semantic meaning are not searched.
- The catalogue is compiled into the application. A signed application
  metadata registry does not exist yet.
- There is no kernel-enforced application capability model or Capsule
  isolation in this milestone.

These limits keep the first implementation offline, deterministic, and small.
A future index service can add incremental updates and user-controlled roots
without changing the provider-independent search API.
