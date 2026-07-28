#!/bin/bash
set -e

VERSION=$(git describe --exact-match --tags)
LAST=$(git describe --abbrev=0 --tags "${VERSION}^" 2>/dev/null || true)

if [ -n "${LAST}" ]; then
	CHANGELOG=$(git log --pretty=format:%s "${LAST}..HEAD" | grep ':' | sed -re 's/([^:]*)\:/- \`\1\`\:/' | sort)
else
	CHANGELOG=$(git log --pretty=format:'- %s' --no-merges)
fi

cat <<NOTES
# RazionOS ${VERSION}

RazionOS is an experimental desktop operating system for virtual machines.
This release packages the Razion Desktop environment, core system tools, and
the x86_64 kernel into a bootable live ISO.

> RazionOS is alpha software. It is not security-hardened and should not be
> used for production workloads or sensitive data.

## Release files

- \`image.iso\` - x86_64 live image with BIOS and UEFI boot entries.

## Origins and licensing

RazionOS is derived from ToaruOS and is not presented as a from-scratch
operating system. The upstream system was created by K. Lange and other
ToaruOS contributors. Copyright, attribution, and license details are
preserved in the repository's \`LICENSE\`, \`NOTICE.md\`, \`AUTHORS\`, source
headers, and component license files.

## Changelog

${CHANGELOG}

## Known limitations

- Hardware support, POSIX coverage, and security hardening are incomplete.
- x86_64 is the verified RazionOS 0.1 Alpha release architecture.
- Running in an isolated virtual machine is strongly recommended.
NOTES
