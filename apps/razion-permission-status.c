/**
 * @brief Query the real RazionOS permission broker for the current process.
 */
#include <stdio.h>
#include <string.h>

#include <toaru/razion_permissions.h>

typedef struct {
	const char * name;
	razion_capability_t capability;
} named_capability_t;

static const named_capability_t capabilities[] = {
	{"files-user-read", RAZION_CAP_FILES_USER_READ},
	{"files-user-write", RAZION_CAP_FILES_USER_WRITE},
	{"network", RAZION_CAP_NETWORK},
	{"audio-capture", RAZION_CAP_AUDIO_CAPTURE},
	{"camera", RAZION_CAP_CAMERA},
	{"settings-read", RAZION_CAP_SETTINGS_READ},
	{"settings-write", RAZION_CAP_SETTINGS_WRITE},
	{"package-query", RAZION_CAP_PACKAGE_QUERY},
	{"package-install", RAZION_CAP_PACKAGE_INSTALL},
	{"power", RAZION_CAP_POWER},
	{"system-read", RAZION_CAP_SYSTEM_READ},
};

int main(int argc, char * argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s CAPABILITY\n", argv[0]);
		return 2;
	}
	for (size_t i = 0; i < sizeof(capabilities) / sizeof(*capabilities); ++i) {
		if (strcmp(argv[1], capabilities[i].name)) continue;
		razion_permission_response_t response;
		int result = razion_permission_check(capabilities[i].capability, NULL, &response);
		if (result < 0) {
			perror("razion-permission-status");
			return 2;
		}
		printf("%s: %s (%s)\n", argv[1],
			razion_permission_status_string(response.status), response.detail);
		return result ? 0 : 1;
	}
	fprintf(stderr, "%s: unknown capability: %s\n", argv[0], argv[1]);
	return 2;
}
