# RazionOS Rebranding Audit

## Scope and baseline

Stage 2 starts from branch `razion-development` at the verified ToaruOS 2.4.0
baseline. The product identity is **RazionOS 0.1 Alpha**. The kernel's visible
name is **Razion Kernel**, the desktop is **Razion Desktop**, the default host
name is `razion`, and the shell-facing command is `rzsh`.

The audit searched case-insensitively for `ToaruOS`, `Toaru OS`, `Toaru`,
`Misaka`, `livecd`, and `esh`, as well as the requested Razion identity terms.
The repository still contains hundreds of intentional Toaru-related
occurrences because it is a derived system and its ABI, source namespace,
history, and license provenance must remain accurate.

## Classification

### A. User-facing branding

Safe to rename when it describes the running product:

- bootloader, startup, login, tutorial, toast, MOTD, issue, and `os-release`
  product strings;
- About-dialog titles and current-product descriptions;
- desktop menus, terminal title, system-information labels, wallpaper, and
  product marks;
- `uname -o`, emulator window names, and the default hostname.

These now use RazionOS naming. Upstream attribution inside About, MOTD, and the
tutorial remains explicit.

### B. Internal API or technical identifier

Do not rename during Stage 2:

- headers under `<toaru/...>`;
- `libtoaru_*.so` libraries and C symbols beginning with `toaru_`;
- Yutani, PEX, TTK, Kuroko, and `esh` implementation symbols;
- protocol/environment identifiers such as `opt/org.toaruos.*`,
  Toaru-specific terminal escape handling, MIME strings, and package metadata;
- executable paths consumed by scripts unless a compatibility alias is added.

`rzsh` is therefore a ramdisk symlink to `esh`; `/bin/sh` remains linked to
`esh`. This adds identity without changing shell behavior or POSIX-facing
compatibility.

### C. Build-system identifier

Safe: emulator display names and generated release metadata.

Retained: architecture names, target triples, Docker image names, package
remotes, make targets, toolchain prefixes, and QEMU firmware configuration
keys. These are part of the working build contract.

### D. File or directory name

Most existing names remain unchanged. In particular, `toaru` include and
library paths are compatibility namespaces, not visible product branding.
New product-owned interfaces live under `base/usr/include/razion/`, and the
new wallpaper is `base/usr/share/wallpapers/razion-dark.jpg`.

### E. Copyright, license, and attribution

All original copyright headers, `LICENSE.md`, author names, license notices,
and upstream links remain intact. RazionOS is presented as an open-source
desktop operating system based on ToaruOS. Replacing provenance with a new
copyright claim would be inaccurate and conflicts with the stage stop
condition requiring ToaruOS licensing and attribution to remain.

### F. Documentation

Historical ToaruOS documentation and man pages remain unless they directly
describe the current RazionOS product experience. New RazionOS documentation
records the derived architecture and does not rewrite upstream history.

## Identity registry

| Role | Stage 2 identity | Implementation status |
| --- | --- | --- |
| Product | RazionOS | Implemented |
| Version | 0.1 Alpha | Implemented |
| Kernel display | Razion Kernel | Implemented |
| Desktop | Razion Desktop | Implemented as presentation identity |
| Hostname | `razion` | Implemented |
| Shell identity | `rzsh` | Implemented as compatible alias |
| Package manager | `rzpkg` | Reserved; not implemented |
| Command interface | Razion Pulse | Implemented in AI Phase 2 |
| Application sandbox | Razion Capsules | Reserved; not implemented |
| System monitor | Razion Insight | Reserved; not implemented |

The current package manager remains `msk`, the current runner remains the
panel/Alt-F2 infrastructure, and the current system tools remain unchanged.

## Files modified from the original ToaruOS tree

Application and UI sources:

`apps/about-dialog.c`, `apps/about.c`, `apps/esh.c`,
`apps/file-browser.c`, `apps/glogin-provider.c`,
`apps/help-browser.c`, `apps/package-manager.c`, `apps/panel.c`,
`apps/path_demo.krk`, `apps/show-toasts.krk`, `apps/splash-log.c`,
`apps/sysinfo.c`, `apps/terminal-palette.h`, `apps/terminal.c`,
`apps/tutorial.c`, and `apps/uname.c`.

System configuration and user defaults:

`base/etc/hostname`, `base/etc/motd`, `base/etc/panel.menu`,
`base/etc/passwd`, `base/home/local/.eshrc`,
`base/home/local/.terminal.json`, `base/home/local/README.md`, and
`base/usr/share/wallpaper.jpg`.

Boot, build, kernel, and generators:

`boot/boot.S`, `boot/config.c`, `boot/text.c`, `build/aarch64.mk`,
`build/x86_64.mk`,
`kernel/sys/version.c`, `kernel/misc/fbterm.c`,
`kernel/arch/aarch64/rpi400/fbterm.c`, `util/createramdisk.krk`,
`util/generate-etc-issue.krk`, and `util/generate-etc-os-release.krk`.

Toolkit, panel, and assets:

`lib/decor-fancy.c`, `lib/menu.c`, `lib/panel_appmenu.c`,
`lib/panel_windowlist.c`, `base/usr/share/logo_login.png`,
`base/usr/share/icons/sysinfo-logo.png`,
`base/usr/share/ttk/fancy/borders-active.png`, and
`base/usr/share/ttk/fancy/borders-inactive.png`.

New RazionOS-owned files:

`base/usr/include/razion/identity.h`,
`base/usr/include/razion/theme.h`,
`base/usr/share/wallpapers/razion-dark.jpg`, and the three
`docs/RAZIONOS_*.md` documents.
