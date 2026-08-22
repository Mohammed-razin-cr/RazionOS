# RazionOS Feature QA

This report distinguishes implemented code from operations verified in the
booted release image. A button or source path alone is never counted as a
pass. The tested image and date are recorded below after each release test.

- Image: `image.iso` (`SHA-256 14EC12898A3C9D6A6F80F0D5106502ADA181A424F6E3249EA75398E9B67C18E4`)
- Test date: 2026-08-22
- Runtime: VirtualBox, BIOS boot, VBoxVGA

| Feature | Status | Real/Fake | Notes |
| --- | --- | --- | --- |
| Universal Launcher | PARTIAL | REAL | Native launcher, Ctrl+Space and legacy Super+Space bindings, keyboard navigation, recents, applications, settings, safe system tools, user locations, workspace switching, screenshot, and recording actions are implemented. Ctrl+Space, keyboard entry, result activation, and close behavior were verified in VirtualBox. |
| Global Search | PARTIAL | REAL | Bounded local metadata search ranks real applications, settings, files, folders, workspaces, and compositor tools and opens actual targets. Natural `Open`, `Search`, `Find`, and `Launch` prefixes are normalized. `Open Downloads` returned one deduplicated real folder result and opened `/home/local/Downloads` in Files during runtime testing. No file-content index or dynamic package database exists. |
| Files | PARTIAL | REAL | Existing native Files implementation provides Home, search, grid/list, sorting, create, rename, multi-select, copy, cut, paste, Trash, restore, and keyboard shortcuts using real filesystem operations. Tabs, general file previews, and a mounted-device broker are not implemented. Standard user folders appear only when they exist. |
| Clipboard | PARTIAL | REAL | Yutani now transports MIME-typed, binary-safe clipboard data while retaining text compatibility and large-payload backing storage. Terminal Ctrl+Shift+C/V and Files path-list copy/cut/paste remain real. A captured 100x80 screenshot reported `image/png`, and its clipboard payload was read back at runtime. General rich-text negotiation and a complete cross-application paste matrix remain pending. |
| Workspaces | PASS | REAL | Four compositor-owned workspaces isolate rendering, mouse targeting, focus, panel window lists, and new-window ownership. Panel buttons 1-4, Ctrl+Alt+Left/Right switching, Ctrl+Alt+Shift+Left/Right move-and-follow, client query/switch APIs, and Universal Search actions are implemented. VirtualBox verified a terminal moving from workspace 2 to 3, disappearing on workspace 2, and returning on workspace 3. The desktop, panel, dock, and companion overlay remain sticky. |
| Screenshot | PASS | REAL | Full-display, focused-window, interactive selection, and exact bounded-region capture write real timestamped PNG files to `~/Pictures/Screenshots`. Cropping, `image/png` clipboard publication, notification action, and opening the real Screenshots folder were verified in VirtualBox. |
| Screen Recording | PASS | REAL | The compositor writes real standard 24-bit AVI video at half display resolution and 5 FPS, with start/status/stop protocol, 30-second/150-frame safety limit, notifications, Videos/Recordings output, clipboard save path, Files association, launcher actions, and a native bounded-memory player. VirtualBox produced a 150-frame 88,477,432-byte AVI and played it in the first-party Recording Player. Audio capture is not included. |
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
- Unsupported permission-broker and durable-persistence features are labeled
  unavailable and have no inert UI controls.

## Runtime checklist

Verified against the clean generated ISO: BIOS boot, cursor, keyboard input,
desktop startup, panel and dock, companion startup, Ctrl+Space launcher,
natural-query folder opening, result deduplication, four-workspace switching,
move-window-to-workspace, focus/render isolation, sticky companion behavior,
full-display screenshot, focused-window screenshot, interactive and direct
region capture, cropped PNG output, `image/png` clipboard transport, actionable
screenshot notification, real AVI recording, 30-second automatic stop, native
recording playback, player close behavior, Privacy Center launch, and its Quick
Settings action.

Not repeated in this focused feature pass: the full Files mutation matrix,
Store catalogue transactions, provider discovery/error matrix, Pulse local
actions, Terminal tabs/clipboard, all window-management gestures, reboot, and
cold-reboot persistence. Their statuses above remain conservative and are not
promoted from source inspection alone.
