# RazionOS Stage 1 Architecture Survey

This document records the unmodified ToaruOS source snapshot found in this
workspace on 2026-07-28. It is an inspection report, not a description of a
RazionOS redesign.

## Baseline update

After the initial survey, the ZIP contents were verified byte-for-byte against
the GitHub tree for commit `6d75d93c83fe6596ef62af93fb72b6bafe08382a`.
Commit/tree metadata was restored without replacing the local source, and the
exact Kuroko and Bim submodule revisions were initialized. The corrected
unmodified source now builds successfully and boots to an interactive desktop
in VirtualBox. See `BUILDING_RAZIONOS.md` and `VIRTUALBOX.md`.

## Snapshot identity and repository state

- The kernel version constants in `kernel/sys/version.c` identify this source
  as **ToaruOS 2.4.0**, codename **"nameless story"**.
- `util/arch.sh` and the root Makefile select **x86_64** by default.
- The kernel is named **Misaka**.
- `boot/config.c` identifies the implemented bootloader as **v5.0**. The prose
  in `boot/README.md` still says v4.0, so the code is the more current source
  for the bootloader version.
- The downloaded ZIP initially had no `.git` directory. Metadata recovery
  established the exact upstream commit as
  `6d75d93c83fe6596ef62af93fb72b6bafe08382a` and restored `master` with
  `origin` set to `https://github.com/klange/toaruos.git`.
- `.gitmodules` declares four submodules:

  | Path | Declared source | Current workspace state |
  | --- | --- | --- |
  | `kuroko` | `../../kuroko-lang/kuroko` | Initialized at `d5763f0bf3e6348d03902c6b2c0af7403208bcae` |
  | `bim` | `../../toaruos/bim` | Initialized at `e7449daa6586defb40cb2652875d09b45491a346` |
  | `util/binutils-gdb` | `../../toaruos/binutils-gdb` | Directory exists but is empty |
  | `util/gcc` | `../../toaruos/gcc` | Directory exists but is empty |

  Kuroko and Bim are required inputs to the normal OS build. The GCC and
  binutils submodules are required only when constructing the project
  cross-toolchain locally; the tested build instead used the official
  prebuilt-toolchain container.

No generated artifacts were present during the initial survey. The successful
baseline build subsequently produced `image.iso`, `misaka-kernel`,
`misaka-kernel.64`, `ramdisk.tar`, and `ramdisk.igz`.

## Source layout

The inspected tree follows the structure documented by this snapshot:

- `kernel/`: Misaka kernel. Architecture-neutral process, syscall, memory,
  VFS, networking, audio, binary-format, and utility code is combined with
  `kernel/arch/x86_64/`.
- `boot/`: shared C implementation for the project's BIOS and x86_64 EFI
  loaders, plus the MBR and linker script.
- `modules/`: one-source-file loadable kernel modules. The x86_64 selection is
  made from `@package x86_64` annotations by `util/valid-modules.sh`.
- `libc/`: the project's C library, dynamic loader, startup objects, syscall
  wrappers, and x86_64 assembly routines.
- `lib/`: first-party shared libraries for graphics, Yutani IPC, menus, text,
  decorations, terminal emulation, image decoding, panel widgets, and other
  common services.
- `apps/`: first-party command-line tools, system services, desktop
  applications, compositor, panel, terminal, file manager, login/session
  managers, and Kuroko/shell applications.
- `base/`: root-filesystem staging tree. It contains headers, configuration,
  startup scripts, user homes, desktop launchers, themes, icons, wallpapers,
  fonts, cursors, and help content.
- `kuroko/`: required interpreter submodule; empty in this copy.
- `bim/`: required editor submodule; empty in this copy.
- `util/`: Kuroko-based dependency and image tools, cross-toolchain builder,
  Docker helpers, and disk/ramdisk generation scripts.
- `build/`: architecture-specific Makefile fragments for x86_64 and aarch64.
- `tests/`: test programs excluded from the normal ramdisk unless `make tests`
  is requested.
- `.make/`: generated application/library dependency rules; not present until
  a build runs.

The staged desktop resources include the `fancy` decoration spritesheet, 108
icon files, one wallpaper under `usr/share/wallpapers` plus the default
`usr/share/wallpaper.jpg`, eight DejaVu Sans/Mono font files, and nine cursor
sprites. User/session defaults are primarily in `base/home/local` and
`base/etc`.

## How the OS boots

### Boot medium

The x86_64 output is a hybrid ISO 9660 image named `image.iso`.

- BIOS boot uses the El Torito `cdrom/boot.sys` image. The custom BIOS loader
  reads the ISO, loads the kernel and ramdisk, and enters the shared boot menu.
- UEFI boot uses an El Torito EFI FAT image, `cdrom/fat.img`, containing
  `efi/boot/bootx64.efi`, `kernel`, and `ramdisk.igz`.
- `xorriso` creates an ISO with both boot entries and a GPT-isohybrid layout.
  The Makefile then replaces the first 512 bytes with the project's generated
  MBR and updates embedded file extents.

The menu in `boot/config.c` defaults to:

```text
root=/dev/ram0 migrate start=live-session vid=auto
```

It also supplies toggles for SMP, framebuffer behavior, and VirtualBox/VMware
guest integration. The loader loads the Misaka kernel and compressed ramdisk
and enters the kernel through its Multiboot-compatible entry point.

### Kernel startup

`kernel/arch/x86_64/bootstrap.S` contains both Multiboot 1 and Multiboot 2
headers. It establishes initial paging and long mode, then calls the x86_64 C
entry point in `kernel/arch/x86_64/main.c`.

The architecture entry point parses Multiboot information, initializes x86_64
CPU and memory facilities, decompresses gzip ramdisk modules, mounts them as
ramdisk devices, and hands control to the architecture-neutral startup.
`kernel/generic.c` initializes processes, VFS, tarfs/tmpfs, devices, IPC,
networking, audio, tasking, and module support. With `migrate`, it unpacks the
tar root into a writable tmpfs. It then executes `/bin/init`.

`apps/init.c` runs executable files in `/etc/startup.d` in lexical order. The
staged scripts configure the splash log, hostname, writable `/tmp` and `/var`,
load detected hardware modules, mount the CD, request DHCP, refresh package
metadata, and finally launch the requested startup application.

## How the kernel is built

The root `Makefile` includes `build/x86_64.mk` and uses the custom
`x86_64-pc-toaru` GCC/binutils cross-toolchain from `util/local/bin`.

- C objects are discovered from `kernel/*.c`, `kernel/*/*.c`, and
  `kernel/arch/x86_64/*.c`.
- Assembly objects come from `kernel/arch/x86_64/*.S`.
- Kernel C flags are freestanding GNU C11, static, PIE, no red zone, no
  standard library, 4 KiB maximum page size, general registers only, and
  `-O2 -g` with warnings enabled.
- `kernel/arch/x86_64/link.ld` controls the kernel layout.
- The linker produces the symbol-bearing `misaka-kernel.64`; the build copies
  and strips it to produce the boot kernel `misaka-kernel`.
- Each applicable source in `modules/` is independently compiled to
  `base/mod/<name>.ko`.

`util/make-version` normally embeds the short Git commit and a dirty suffix.
With the `.git` directory missing, that command fails and reliable revision
metadata cannot be embedded.

## How userspace is built

The build first compiles a host Kuroko executable at
`util/local/bin/kuroko`. That host tool runs `util/auto-dep.krk`, which scans C
includes and emits generated Makefile fragments under `.make/`.

The target cross-toolchain then builds:

- CRT objects and the project's `libc.a` / `libc.so`;
- the target Kuroko executable, library, and modules;
- first-party shared libraries from `lib/` as `base/lib/libtoaru_*.so`;
- C and C++ applications from `apps/` into `base/bin`;
- Kuroko and shell applications copied into `base/bin`;
- Bim and its syntax/theme resources;
- the required target `libgcc_s` files copied from the cross-toolchain;
- generated `/etc/issue` and `/etc/os-release`;
- architecture-eligible loadable kernel modules.

`util/createramdisk.krk` walks `base/`, assigns expected ownership and modes,
adds conventional symlinks, and writes `ramdisk.tar`. `gzip` produces
`ramdisk.igz`.

## How the graphical desktop starts

The final startup script, `base/etc/startup.d/99_runstart.sh`, reads the
kernel's `start=` parameter. For the default `start=live-session`, it executes:

```text
/bin/compositor -- live-session
```

`apps/compositor.c` is Yutani, a combined compositing display server and
window manager. It owns the framebuffer, window stacking/layout, input
routing, and the Yutani IPC service, then starts the requested client.

`apps/live-session.c` drops from root to the `local` user and runs
`/bin/session`. `apps/session.c` first honors an executable
`~/.yutanirc`; otherwise it launches:

- `file-browser --wallpaper` from the user's Desktop directory, which provides
  the wallpaper/desktop icons; and
- `panel --really`, which supplies the top panel, application menu, task
  list, and panel widgets.

When the live session exits, `live-session` launches the graphical login
manager. Decoration rendering is client-side through `lib/decorations.c` and
the selected decoration library/resources.

## How the bootable image is generated

For x86_64, `all` aliases `system`, and `system` depends on `image.iso`.
The dependency graph builds the kernel and userspace ramdisk, then:

1. Copies `misaka-kernel` and `ramdisk.igz` into `fatbase/`.
2. Builds the 32-bit BIOS loader with the cross compiler and host GNU linker.
3. Builds an x86_64 GNU-EFI application and converts it to
   `fatbase/efi/boot/bootx64.efi`.
4. Uses `util/mkdisk.sh`, `mkfs.fat`, and mtools to construct
   `cdrom/fat.img`.
5. Uses `xorriso -as mkisofs` to create a BIOS+UEFI hybrid `image.iso`.
6. Generates and installs the project's MBR and patches ISO extents with
   `util/update-extents.krk`.

The complete native build command, once all sources and the cross-toolchain
are available, is:

```bash
make -j"$(nproc)"
```

The repository's official container workflow uses:

```bash
docker run -v "$(pwd)":/root/misaka -w /root/misaka \
  -e LANG=C.UTF-8 -t toaruos/build-tools:1.99.x \
  util/build-in-docker.sh
```

That helper invokes the equivalent staged sequence
`make util/local/bin/kuroko`, `make base/lib/libc.so`, and `make -j4`.

The expected final bootable artifact is:

```text
<repository-root>/image.iso
```

## VirtualBox compatibility

Yes. The generated `image.iso` is intended to boot directly in VirtualBox and
contains both BIOS and x86_64 UEFI loaders. ToaruOS includes a VirtualBox
driver (`modules/vbox.c`) and enables automatic display sizing and absolute
pointer integration by default. The ISO should be attached directly as an
optical disk; no conversion is needed.

The snapshot README recommends an **Other 64-bit** guest, at least **1 GiB
RAM**, at least **2 CPUs**, no required hard disk, and an **Intel Gigabit**
network adapter. A dedicated, tested VirtualBox configuration belongs to the
later baseline-boot stage; Stage 1 did not build or boot this incomplete copy.

## Development dependencies

There are two supported approaches.

### A. Official prebuilt-toolchain container

The README recommends Docker with `toaruos/build-tools:1.99.x`. On the
development host this requires Docker plus Git/submodules; the image carries
the Linux build packages and cross-toolchain. Docker Desktop is installed on
the Windows host, but its Linux engine was not running during this survey.

### B. Native Ubuntu build, including the cross-toolchain

The current x86_64 scripts require these Ubuntu packages:

| Package | Why it is required |
| --- | --- |
| `build-essential` | Host C/C++ compiler, GNU Make, linker/assembler, and basic build tools |
| `git` | Repository revision metadata and submodule checkout |
| `gzip` | Produces `ramdisk.igz` |
| `xorriso` | Produces the hybrid bootable ISO |
| `mtools` | `mcopy`/`mmd` populate the EFI FAT image without mounting it |
| `dosfstools` | `mkfs.fat` creates the EFI FAT image |
| `gnu-efi` | EFI headers, CRT object, libraries, and linker script |
| `autoconf` | Toolchain configure/build support |
| `automake` | Toolchain configure/build support |
| `libgmp-dev` | GCC build prerequisite |
| `libmpfr-dev` | GCC build prerequisite |
| `libmpc-dev` | GCC build prerequisite |
| `flex` | GCC/binutils source build prerequisite |
| `bison` | GCC/binutils source build prerequisite |
| `texinfo` | GCC/binutils documentation/build prerequisite |

The checked-in Dockerfile also installs `python3`, `wget`, and `genext2fs`,
but none is invoked by the current x86_64 Makefile or image path. Python was
already reported available by the project owner. `genext2fs` is not used for
the ISO/ramdisk flow in this snapshot, and `wget` is not used by
`util/build-toolchain.sh`.

Clang, NASM, CMake, GRUB utilities, GDB, QEMU, `pkg-config`, and the
autotools frontends are not part of the normal OS compilation command.
QEMU is useful for the Makefile's `run`, `shell`, and `test` targets; GDB and
GRUB are optional debugging/fallback tools; VirtualBox is the intended
external VM.

### Observed installation state

The current Codex environment exposes Windows and only the
`docker-desktop` WSL distribution. There is no accessible WSL distribution
named `Ubuntu`, so the package state of the owner's Ubuntu development
environment cannot be truthfully labeled installed or missing from here.

Observed on the Windows host:

- **INSTALLED:** Git, Python, Docker CLI.
- **MISSING or not on PATH:** native GCC/G++, Make, xorriso, and the ToaruOS
  cross-toolchain. These Windows results do not substitute for an Ubuntu
  dependency audit.
- **UNAVAILABLE:** Docker engine (Docker Desktop Linux engine was stopped).

Optional tools for the target workflow:

- **OPTIONAL:** `qemu-system-x86`, `gdb`, `grub-pc-bin`,
  `grub-efi-amd64-bin`, `clang`, `cmake`, `nasm`, and `pkg-config`.

Because Ubuntu itself is not accessible, the following single command is
designed to pass **only actually missing required packages** to `apt`. It must
be run by the owner inside the intended Ubuntu environment; it was not
executed during this survey:

```bash
sudo apt install $(for p in build-essential git gzip xorriso mtools dosfstools gnu-efi autoconf automake libgmp-dev libmpfr-dev libmpc-dev flex bison texinfo; do dpkg-query -W -f='${Status}' "$p" 2>/dev/null | grep -q 'ok installed' || printf '%s ' "$p"; done)
```

If the official prebuilt Docker builder is used, installing this complete
native toolchain package set on the host is unnecessary.

## Initial blockers and resolution

The initial survey found four blockers:

1. It has no `.git` metadata, so its exact commit and submodule pins are
   unknown and `git submodule update --init` cannot run.
2. Required Kuroko and Bim directories are empty.
3. Neither `util/local/` nor the GCC/binutils submodule sources are present, so
   the required `x86_64-pc-toaru-*` cross-toolchain is unavailable.
4. The actual Ubuntu environment is not connected to this workspace, so its
   installed/missing package state cannot be verified.

The first three were resolved by metadata-only verification/recovery,
initializing Kuroko and Bim at their exact pins, and using the official builder
container. A fifth archive-specific issue was then identified: the GitHub ZIP
stored 18 symbolic links as regular files containing their targets. Restoring
those link semantics in the Linux build tree was required for a working
desktop, most visibly for `base/usr/share/wallpaper.jpg`.

The baseline ISO now builds and boots. No RazionOS rebranding or UI source
modification has been performed.
