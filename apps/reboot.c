/**
 * @brief (Try to) reboot the system.
 *
 * Note that only root can reboot, and this doesn't
 * do any fancy setuid stuff.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2013-2014 K. Lange
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>

int main(int argc, char ** argv) {
	/* Flush the optional persistent-home disk before asking the kernel to
	 * restart. ATA maintains a write-back cache, so a raw reboot without this
	 * loses otherwise successful filesystem writes. */
	int persistent_disk = open("/dev/hda", O_RDONLY);
	if (persistent_disk >= 0) {
		if (ioctl(persistent_disk, IOCTLSYNC, NULL) < 0) {
			fprintf(stderr, "%s: unable to flush /dev/hda: %s\n",
				argv[0], strerror(errno));
			close(persistent_disk);
			return 1;
		}
		close(persistent_disk);
	}
	if (reboot(0) < 0) {
		fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
	}
	return 1;
}
