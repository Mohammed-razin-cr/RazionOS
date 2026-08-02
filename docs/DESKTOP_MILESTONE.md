# Modern desktop milestone

This document describes the desktop and AI reliability work added after the
RazionOS 0.1 Alpha baseline. The implementation uses the existing compositor,
Yutani toolkit, C library, PEX IPC, and system utilities; it does not add a
second desktop framework.

## File Manager

The File Manager now supports creating files and folders, renaming, copying,
cutting, pasting, deletion, permanent deletion, properties, current-directory
search, sorting, multiple selection, context menus, and same-window dragging
of selected entries into a folder.

The graphical `file-name-dialog` helper validates names and refuses to replace
an existing entry. Clipboard operations pass paths as argument vectors to
native programs instead of constructing shell command strings.

Keyboard shortcuts:

| Shortcut | Action |
| --- | --- |
| `Ctrl+N` | Create file |
| `Ctrl+Shift+N` | Create folder |
| `F2` | Rename selected entry |
| `Ctrl+C`, `Ctrl+X`, `Ctrl+V` | Copy, cut, paste |
| `Ctrl+A` | Select all |
| `Ctrl+F` | Search the current directory |
| `Delete` | Move selected entries to Recycle Bin |
| `Shift+Delete` | Permanently delete selected entries |

Cross-window file dragging is not advertised because the current compositor
does not provide a file-transfer drag payload. Cut and Paste provide the safe
cross-window path.

## Recycle Bin

`razion-trash` implements a per-user FreeDesktop-style layout beneath
`~/.local/share/Trash`. Deleted entries are moved into `files/`, while matching
`.trashinfo` records retain the original absolute path and deletion time.
Stored names are made collision-safe. Restore refuses to overwrite an existing
destination. Moves across filesystem boundaries use the native `mv`
copy-and-remove fallback after an atomic `rename` is rejected. Emptying the bin
recursively removes only its managed contents and requires confirmation in the
graphical workflow.

The desktop Recycle Bin launcher opens the managed directory. File Manager
context actions provide restore, empty, and permanent-delete operations.

## Wallpaper and appearance

The Wallpaper Picker discovers PNG, JPG, and JPEG images in
`/usr/share/wallpapers`, previews a selection, and applies Fill, Center, or
Stretch placement. The chosen file and mode are persisted in the user's
RazionOS configuration and restored by the desktop after reboot.

The native Settings application provides entry points for Wallpaper, Desktop,
AI Status, About, Terminal, and Dark or Light theme selection. Theme choice is
read from `~/.razion/theme.conf` by RazionOS-aware applications.

## AI responsiveness and reliability

Razion Pulse and Razion Chat communicate only with Razion AI Engine. Provider
connections remain isolated behind provider adapters. The engine now adds:

- health-aware local-first provider selection and automatic failover;
- bounded retry and timeout configuration;
- a bounded, short-lived response cache for safe operations;
- non-blocking SDK begin, poll, and cancel primitives; and
- background provider discovery with health and cache statistics.

The SDK's stream callback is currently compatibility infrastructure: protocol
version 1 delivers the completed bounded response in one callback. True
multi-chunk provider streaming requires a future IPC protocol revision.

Ollama remains an optional external runtime and model; neither is bundled.
Groq, Gemini, and OpenRouter configuration placeholders remain disabled until
RazionOS has a verified TLS client and protected credential storage. API keys
must not be sent through an insecure transport or embedded in an ISO.

## Verification

The milestone was verified with a clean x86_64 container build, hybrid ISO
artifact inspection, and an interactive VirtualBox boot. Live checks covered
desktop startup, graphical folder creation, Recycle Bin move/size/restore,
Settings launch, and AI Engine health reporting. AI Engine failure remains
non-fatal when no external provider is reachable.
