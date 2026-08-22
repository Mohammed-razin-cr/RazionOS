#!/bin/bash
set -euo pipefail

output="${1:-razion-home.img}"
size_mib="${2:-512}"

if ! [[ "$size_mib" =~ ^[0-9]+$ ]] || [ "$size_mib" -lt 64 ]; then
	echo "usage: $0 [OUTPUT.img] [SIZE_MiB>=64]" >&2
	exit 2
fi

if [ -e "$output" ]; then
	echo "refusing to overwrite existing persistent disk: $output" >&2
	exit 1
fi

if ! command -v genext2fs >/dev/null 2>&1; then
	echo "genext2fs is required (it is included in the RazionOS build container)" >&2
	exit 1
fi

blocks=$((size_mib * 1024))
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT

# The guest seeds account defaults on first mount. Keeping this image empty
# avoids baking passwords, settings, or host files into durable storage.
genext2fs -B 1024 -b "$blocks" -d "$work" "$output"
echo "created $output (${size_mib} MiB raw ext2 persistent-home disk)"
