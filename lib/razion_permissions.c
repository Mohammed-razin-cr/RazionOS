/**
 * @brief Client SDK for the authenticated RazionOS permission broker.
 */
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/fswait.h>
#include <unistd.h>

#include <toaru/pex.h>
#include <toaru/razion_permissions.h>

typedef struct {
	const char * name;
	razion_capability_t value;
} capability_name_t;

static const capability_name_t policy_capabilities[] = {
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
	{"ai", RAZION_CAP_AI},
};

static char * policy_trim(char * text) {
	while (isspace((unsigned char)*text)) text++;
	char * end = text + strlen(text);
	while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
	return text;
}

static razion_capability_t policy_parse_capabilities(const char * list) {
	razion_capability_t result = 0;
	const char * item = list;
	while (*item) {
		while (isspace((unsigned char)*item)) item++;
		const char * end = item;
		while (*end && *end != ',') end++;
		const char * trimmed_end = end;
		while (trimmed_end > item && isspace((unsigned char)trimmed_end[-1])) trimmed_end--;
		size_t item_length = trimmed_end - item;
		for (size_t i = 0; i < sizeof(policy_capabilities) / sizeof(*policy_capabilities); ++i) {
			size_t name_length = strlen(policy_capabilities[i].name);
			if (item_length == name_length && !memcmp(item, policy_capabilities[i].name, name_length)) {
				result |= policy_capabilities[i].value;
				break;
			}
		}
		item = *end ? end + 1 : end;
	}
	return result;
}

int razion_permission_policy_allows(
	const char * policy_path,
	const char * executable,
	razion_capability_t capability) {
	if (!policy_path) policy_path = "/etc/razion-permissions.conf";
	if (!executable || executable[0] != '/' || !capability ||
		(capability & (capability - 1))) return 0;
	FILE * file = fopen(policy_path, "r");
	if (!file) return 0;
	char line[768];
	int allowed = 0;
	while (fgets(line, sizeof(line), file)) {
		char * text = policy_trim(line);
		if (!*text || *text == '#') continue;
		char * equals = strchr(text, '=');
		if (!equals) continue;
		*equals = '\0';
		if (strcmp(policy_trim(text), executable)) continue;
		allowed = !!(policy_parse_capabilities(policy_trim(equals + 1)) & capability);
		break;
	}
	fclose(file);
	return allowed;
}

int razion_permission_check(
	razion_capability_t capability,
	const char * resource,
	razion_permission_response_t * response) {

	if (!capability || (capability & (capability - 1)) || !response) {
		errno = EINVAL;
		return -1;
	}

	razion_permission_request_t request;
	memset(&request, 0, sizeof(request));
	request.magic = RAZION_PERMISSION_MAGIC;
	request.version = RAZION_PERMISSION_VERSION;
	request.operation = RAZION_PERMISSION_CHECK;
	request.request_id = ((uint32_t)getpid() << 16) ^ (uint32_t)capability;
	request.capability = capability;
	if (resource) {
		strncpy(request.resource, resource, sizeof(request.resource) - 1);
	}

	FILE * broker = pex_connect(RAZION_PERMISSION_ENDPOINT);
	if (!broker) return -1;
	if (pex_reply(broker, sizeof(request), (char *)&request) != sizeof(request)) {
		fclose(broker);
		errno = EIO;
		return -1;
	}

	int fd = fileno(broker);
	if (fswait2(1, &fd, 1000) != 0) {
		fclose(broker);
		errno = ETIMEDOUT;
		return -1;
	}

	char buffer[MAX_PACKET_SIZE];
	size_t received = pex_recv(broker, buffer);
	fclose(broker);
	if (received != sizeof(*response)) {
		errno = EPROTO;
		return -1;
	}
	memcpy(response, buffer, sizeof(*response));
	response->detail[sizeof(response->detail) - 1] = '\0';
	if (response->magic != RAZION_PERMISSION_MAGIC ||
		response->version != RAZION_PERMISSION_VERSION ||
		response->request_id != request.request_id ||
		response->capability != capability) {
		errno = EPROTO;
		return -1;
	}
	return response->status == RAZION_PERMISSION_GRANTED ? 1 : 0;
}

const char * razion_permission_status_string(
	razion_permission_status_t status) {
	switch (status) {
		case RAZION_PERMISSION_GRANTED: return "granted";
		case RAZION_PERMISSION_DENIED: return "denied";
		case RAZION_PERMISSION_INVALID: return "invalid request";
		case RAZION_PERMISSION_UNAVAILABLE: return "broker unavailable";
		default: return "unknown";
	}
}
