# RazionOS Modern Desktop Milestone

This milestone collects the user-facing desktop features added around RazionOS
0.1 Alpha. The goal is to make common desktop work discoverable without hiding
the experimental status of the OS.

## Feature Surfaces

- **Persistent storage:** `06_persistent_home.sh` mounts an attached ext2 disk
  at `/home` and records runtime status in `/tmp/razion-persistence-status`.
  `razion-system-center` displays that status and explains the setup path.
- **AI Assistant:** `razion-assistant` is a native Yutani panel that talks only
  through Razion AI Engine. It supports chat mode, AI file-search mode, Pulse
  handoff, and provider-status handoff.
- **AI file search:** Assistant file-search mode requests
  `RAZION_AI_OP_SEARCH_FILES`. If the AI provider is unavailable, it opens
  Razion Launcher with the user's query for bounded local search.
- **File Manager:** `file-browser` remains the main file manager and already
  provides create, rename, copy, cut, paste, delete, Recycle Bin integration,
  search, sort, context menus, keyboard shortcuts, and drag support.
- **Settings:** Settings now links to System Center, screenshots, lock screen,
  AI Assistant, AI file search, and provider status.
- **Launcher/Search:** `universal-search` can now open with an initial query and
  indexes Assistant, System Center, persistent storage, notifications, updates,
  screenshots, and lock screen actions.
- **Notifications:** `toastd` remains the notification daemon. System Center and
  Quick Settings expose Do Not Disturb and a test-notification path.
- **Updates:** RazionOS 0.1 Alpha has no online updater. System Center exposes a
  milestone update surface that reports this honestly and points users to the
  rebuildable source workflow and local app catalogue.
- **Boot transition:** `razion-splash` receives real startup milestones through
  `/dev/pex/splash`. The implementation remains milestone-driven rather than a
  fake timer.
- **Lock screen:** `razion-lock` provides a full-screen presentation lock
  overlay. It is not a password security boundary in this alpha release.
- **About:** `about` now identifies RazionOS 0.1 Alpha, the x86_64 desktop
  focus, the local-first AI direction, and preserved ToaruOS origin notice.
- **Screenshot tool:** `yutani-screenshot` already supports full-screen,
  focused-window, and region captures. System Center exposes all three modes.

## Design Notes

The new apps use the existing Yutani window server, Razion semantic theme
tokens, and standard process launch patterns. They do not add external UI
libraries or background network services.

## Known Limitations

- The lock screen is a session overlay, not authentication.
- Updates are informational; online update download/install is future work.
- AI file search depends on the provider stack. Local Launcher search remains
  the deterministic fallback.
- Persistent storage requires a correctly attached ext2 disk.
