/**
 * @brief about - Compatibility layer for about-dialog
 *
 * This utility provides a wrapper for executing the new 'about-dialog'
 * application using the argument format from the old 'about'.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2026 K. Lange
 */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <razion/identity.h>

int main(int argc, char * argv[]) {
	if (argc < 2) {
		execvp("about-dialog",(char *const[]){
			"about-dialog",
			"--title-about", RAZION_PRODUCT_NAME,
			"--logo", "/usr/share/logo_login.png",
			"--icon", "star",
			"--name", RAZION_PRODUCT_FULL_NAME,
			"--",
			"Open-source desktop operating system.",
			"Razion Desktop on the Razion Kernel.",
			"-",
			"Based on ToaruOS by K. Lange and contributors.",
			"© 2011-2026 K. Lange, et al.",
			"Upstream components: NCSA/UIUC license.",
			"-",
			"%https://github.com/Mohammed-razin-cr/RazionOS",
			NULL
		});
		fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
	} else if (argc < 5) {
		fprintf(stderr, "Invalid arguments.\n");
		return 1;
	} else {
		char * args[100] = {
			"about-dialog",
			"--title",argv[1],
			"--logo",argv[2],
			"--icon","star",
			"--name",argv[3],
			NULL
		};
		int c = 9;

		if (argc > 6) {
			char * tmp;
			asprintf(&tmp, "%s,%s", argv[5], argv[6]);
			args[c++] = "--at";
			args[c++] = tmp;
		}

		args[c++] = "--";

		int i = 0;
		char * me = argv[4], * end;
		do {
			args[c++] = me;
			i++;
			end = strchr(me,'\n');
			if (end) {
				*end = '\0';
				me = end+1;
			}
			if (i >= 20) break;
		} while (end);

		return execvp("about-dialog",args);
	}
}
