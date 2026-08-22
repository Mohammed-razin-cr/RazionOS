# RazionOS Feature QA

This report distinguishes implemented code from operations verified in the
booted release image. A button or source path alone is never counted as a
pass. The tested image and date are recorded below after each release test.

- Image: `image.iso` (`SHA-256 8862C7A9B1465DABC9EA4587B47E50BF8C6C535749D13B5CBDFB2B4624D0CABA`)
- Test date: 2026-08-22
- Runtime: VirtualBox, BIOS boot, VBoxVGA

| Feature | Status | Real/Fake | Notes |
| --- | --- | --- | --- |
| Universal Launcher | PARTIAL | REAL | Native launcher, Ctrl+Space and legacy Super+Space bindings, keyboard navigation, recents, applications, settings, safe system tools and user locations are implemented. Ctrl+Space, keyboard entry, result activation, and close behavior were verified in VirtualBox. Workspace results are unavailable because the compositor has no workspace model. |
| Global Search | PARTIAL | REAL | Bounded local metadata search ranks real applications, settings, files and folders and opens actual targets. Natural `Open`, `Search`, `Find`, and `Launch` prefixes are normalized. `Open Downloads` returned one deduplicated real folder result and opened `/home/local/Downloads` in Files during runtime testing. No content index, recent-file database, workspace index, or dynamic package database exists. |
| Files | PARTIAL | REAL | Existing native Files implementation provides Home, search, grid/list, sorting, create, rename, multi-select, copy, cut, paste, Trash, restore, and keyboard shortcuts using real filesystem operations. Tabs, general file previews, and a mounted-device broker are not implemented. Standard user folders appear only when they exist. |
| Clipboard | PARTIAL | REAL | Yutani text clipboard and terminal Ctrl+Shift+C/V are implemented; Files uses a real path-list clipboard for copy/cut/paste. Rich/image clipboard MIME negotiation is not implemented. Cross-application runtime matrix is pending. |
| Workspaces | NOT IMPLEMENTED | REAL | The current compositor has one global window stack and no workspace ownership, overview, move-window protocol, or persisted layout model. No placeholder workspace controls were added. |
| Screenshot | PARTIAL | REAL | Full-display and focused-window capture write actual timestamped PNG files to `~/Pictures/Screenshots`. Both paths created searchable files in the booted ISO, and full-display and focused-window PNGs decoded and rendered correctly in Image Viewer. Region selection, notification actions, and image clipboard data are not supported by current protocols. |
| Screen Recording | BLOCKED | REAL | The compositor exposes no frame-stream/capture protocol, recorder indicator protocol, video encoder, audio capture synchronization, or WebM writer. No fake recording control was added. Native implementation requires a bounded compositor frame subscription and a real encoder integration. |
| Store | PARTIAL | REAL | Native catalogue, categories, search, installed-state checks, application metadata, versions and declared permissions are real. Install/update is correctly disabled because `rzpkg` has no signed repository index, signature verifier, dependency resolver, permission broker, or privileged transaction service. |
| Privacy Center | PARTIAL | REAL | Native center reads actual AI policy and audit status and explicitly reports missing network/device/filesystem permission brokers. Launch from the universal launcher and the `Open Quick Settings` action were verified in VirtualBox. Provider health launches a real engine health request. Revocation cannot be offered until a kernel permission broker exists. |
| AI Provider Hub | PARTIAL | REAL | Provider engine, health checks, timeout/retry/cache handling, local Ollama PEX adapter, provider state output and fail-closed cloud policy exist. Only the Ollama adapter ships; cloud providers are catalogue entries, not adapters. A model is not bundled and secrets are not embedded. |
| Razion Pulse | PARTIAL | REAL | Local system intents and confirmation-gated file actions are native; provider prompts route through the real AI engine and fail clearly when unavailable. The transport currently delivers a completed payload through the streaming callback rather than incremental provider tokens. GUI stop/retry/history/model controls are not implemented. |
| Terminal | PASS | REAL | Existing first-party terminal includes Razion theming, monospace rendering, scrollback, selection, clipboard, history, tabs/sessions, focus handling and persisted preferences. Previously verified in the booted image; regression test remains in final runtime pass. |
| Persistence | BLOCKED | REAL | User configuration writers exist, but the default ISO boots a RAM filesystem. Durable user persistence across a cold reboot needs a stable writable disk/filesystem mount; the current ext2 write path is not safe enough to enable. No fake persistence claim is made. |
| Reboot | PASS | REAL | Authenticated reboot completed successfully in the previously booted milestone image. Final regression test remains in the clean-image pass. |

## Security and failure behavior

- No API keys or provider credentials are committed by this pass.
- Cloud AI is disabled by default; the engine requires both system policy and
  explicit request opt-in.
- AI actions are allow-listed. Destructive actions are not executed without
  explicit confirmation.
- Store installation fails closed without a signed transaction pipeline.
- File and screenshot writes return errors rather than displaying fake success.
- Unsupported permission, workspace, recording, and image-clipboard features
  are labeled unavailable and have no inert UI controls.

## Runtime checklist

Verified against the clean generated ISO: BIOS boot, cursor, keyboard input,
desktop startup, panel and dock, companion startup, Ctrl+Space launcher,
natural-query folder opening, result deduplication, full-display screenshot,
focused-window screenshot, PNG decode/render, Privacy Center launch, and its
Quick Settings action.

Not repeated in this focused feature pass: the full Files mutation matrix,
Store catalogue transactions, provider discovery/error matrix, Pulse local
actions, Terminal tabs/clipboard, all window-management gestures, reboot, and
cold-reboot persistence. Their statuses above remain conservative and are not
promoted from source inspection alone.
