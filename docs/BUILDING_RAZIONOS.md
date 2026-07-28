# Building the Unmodified Baseline

This document records the successful x86_64 baseline build performed on
2026-07-28 before any RazionOS rebranding or redesign.

## Verified source

- Upstream: `https://github.com/klange/toaruos.git`
- Branch: `master`
- Commit: `6d75d93c83fe6596ef62af93fb72b6bafe08382a`
- Kernel version: ToaruOS 2.4.0
- Bim: `e7449daa6586defb40cb2652875d09b45491a346`
- Kuroko: `d5763f0bf3e6348d03902c6b2c0af7403208bcae`
- Builder: `toaruos/build-tools:1.99.x`
- Builder digest:
  `sha256:cfc840017cd3db18244bb425cbeb56dbcc833eb3c8d99ee473d2d548a8b28b46`

The local source originated from `toaruos-master.zip`. Its 1,115 regular-file
blobs were verified against the upstream Git tree before Git metadata was
restored.

## Required submodules

For the official container build, only Kuroko and Bim are needed:

```bash
git submodule update --init kuroko bim
```

`util/gcc` and `util/binutils-gdb` are needed only when building the
cross-toolchain locally instead of using the official builder image.

## Standard Ubuntu/Docker build

The complete target remains the repository's default Make target:

```bash
make -j"$(nproc)"
```

With the official builder, use:

```bash
docker pull toaruos/build-tools:1.99.x
docker run --rm \
  -v "$(pwd)":/root/misaka \
  -w /root/misaka \
  -e LANG=C.UTF-8 \
  -e LD_LIBRARY_PATH=/root/gcc_local/x86_64-pc-linux-gnu/x86_64-pc-toaru/lib \
  -t toaruos/build-tools:1.99.x \
  util/build-in-docker.sh
```

The explicit `LD_LIBRARY_PATH` is needed by this builder revision for its
direct `x86_64-pc-toaru-as` invocation during final MBR generation.

## ZIP and Windows bind-mount caveats

Two host-environment problems were found during the baseline build:

1. The downloaded ZIP flattened 18 Git symbolic links into regular text
   files. The most visible was `base/usr/share/wallpaper.jpg`, which must be a
   relative link to `wallpapers/whiteeye.jpg`. Building with the flattened
   file boots, but the wallpaper loader fails and leaves an unusable black
   desktop.
2. Docker Desktop's Windows bind filesystem does not support the script's
   `fallocate` call. `util/mkdisk.sh` then falls back to
   `dd if=/dev/zero bs=1`, which is extremely slow on that filesystem.

The successful Windows-hosted build copied the verified tree into the
container's native Linux filesystem, recreated every `120000` Git-tree entry
as a symbolic link, removed only generated ramdisk/ISO outputs, ran the
official helper, and copied the resulting artifacts back. On a normal Ubuntu
Git checkout, symbolic links and `fallocate` should work directly.

The corrected ramdisk was checked explicitly:

```text
usr/share/wallpaper.jpg -> wallpapers/whiteeye.jpg
usr/share/wallpapers/whiteeye.jpg (471815 bytes)
```

## Outputs

The successful build produced:

| Artifact | Size | SHA-256 |
| --- | ---: | --- |
| `image.iso` | 7,462,912 bytes | `95e07078424bd038e0116adeb3e4a36f0edee88118c9a4dfcea08ed16168a429` |
| `misaka-kernel` | 259,888 bytes | `ccb2bd82d0ac3d3d5890e1db5b8f307c86d83facf3e264d6f59107a23fb2b179` |
| `misaka-kernel.64` | 1,512,312 bytes | `2e11d4f1bdb5e20c87e8a8004abdcb182a0da9bc1d01add355d1fb12342db3ba` |
| `ramdisk.igz` | 6,653,964 bytes | `ad7723f73d9e913e831840b2eeb3e99a2a7e1abd4efba4cec4cde33ec08d271e` |

The final bootable artifact was:

```text
<repository-root>/image.iso
```

## Image validation

Validation found:

- valid MBR signature `55 AA`;
- valid ISO9660 primary-volume signature `CD001`;
- El Torito BIOS boot entry using `/boot.sys`;
- alternate no-emulation EFI boot entry using `/fat.img`;
- hybrid MBR layout;
- successful BIOS boot in Oracle VirtualBox 7.0.18;
- successful kernel initialization, VirtualBox integration, DHCP, graphical
  desktop startup, and interactive terminal launch.

The build and boot evidence is shown in
`docs/assets/toaruos-baseline-desktop.png` and
`docs/assets/toaruos-baseline-terminal-test.png`.

## Baseline source status at the time of this test

The upstream source and initialized submodules remain at their exact baseline
revisions. Generated build products are ignored by the upstream `.gitignore`.
At this baseline checkpoint, the only intentional untracked additions were the
RazionOS documentation and boot-evidence images. Branding and UI work was
performed later and is documented separately.
