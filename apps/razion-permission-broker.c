/**
 * @brief Root-owned permission decision service using kernel-authenticated PEX
 * client credentials and exact executable policy entries.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <toaru/pex.h>
#include <toaru/razion_permissions.h>

#define POLICY_PATH "/etc/razion-permissions.conf"
#define MAX_RULES 64
#define EXECUTABLE_MAX 256
#define CAPABILITY_LIST_MAX 512

typedef struct {
	char executable[EXECUTABLE_MAX];
	char capability_list[CAPABILITY_LIST_MAX];
} permission_rule_t;

static permission_rule_t rules[MAX_RULES];
static size_t rule_count;

typedef struct {
	const char * name;
	razion_capability_t value;
} capability_name_t;

static const capability_name_t capability_names[] = {
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

static char * trim(char * text) {
	while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;
	char * end = text + strlen(text);
	while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = '\0';
	return text;
}

static razion_capability_t parse_capabilities(const char * list) {
	razion_capability_t result = 0;
	const char * item = list;
	while (*item) {
		while (*item == ' ' || *item == '\t') item++;
		const char * end = item;
		while (*end && *end != ',') end++;
		const char * trimmed_end = end;
		while (trimmed_end > item &&
			(trimmed_end[-1] == ' ' || trimmed_end[-1] == '\t' ||
			 trimmed_end[-1] == '\r' || trimmed_end[-1] == '\n')) trimmed_end--;
		size_t item_length = trimmed_end - item;
		for (size_t i = 0; i < sizeof(capability_names) / sizeof(*capability_names); ++i) {
			size_t name_length = strlen(capability_names[i].name);
			if (item_length == name_length && !memcmp(item, capability_names[i].name, name_length)) {
				result |= capability_names[i].value;
				break;
			}
		}
		item = *end ? end + 1 : end;
	}
	return result;
}

static int load_policy(void) {
	FILE * file = fopen(POLICY_PATH, "r");
	if (!file) return -1;
	char line[768];
	while (rule_count < MAX_RULES && fgets(line, sizeof(line), file)) {
		char * text = trim(line);
		if (!*text || *text == '#') continue;
		char * equals = strchr(text, '=');
		if (!equals) continue;
		*equals = '\0';
		char * executable = trim(text);
		char * capability_list = trim(equals + 1);
		if (executable[0] != '/' || strlen(executable) >= EXECUTABLE_MAX) continue;
		if (!parse_capabilities(capability_list)) continue;
		strncpy(rules[rule_count].executable, executable,
			sizeof(rules[rule_count].executable) - 1);
		strncpy(rules[rule_count].capability_list, capability_list,
			sizeof(rules[rule_count].capability_list) - 1);
		rule_count++;
	}
	fclose(file);
	return rule_count ? 0 : -1;
}

static size_t bounded_length(const char * text, size_t limit) {
	size_t length = 0;
	while (length < limit && text[length]) length++;
	return length;
}

static int same_executable(const char * authenticated, const char * policy) {
	size_t authenticated_length = bounded_length(authenticated, PEX_EXECUTABLE_MAX);
	size_t policy_length = bounded_length(policy, EXECUTABLE_MAX);
	return authenticated_length == policy_length &&
		authenticated_length < PEX_EXECUTABLE_MAX &&
		!memcmp(authenticated, policy, authenticated_length);
}

static int capability_list_contains(const char * list, const char * wanted) {
	size_t wanted_length = strlen(wanted);
	const char * item = list;
	while (*item) {
		while (*item == ' ' || *item == '\t') item++;
		const char * end = item;
		while (*end && *end != ',') end++;
		const char * trimmed_end = end;
		while (trimmed_end > item &&
			(trimmed_end[-1] == ' ' || trimmed_end[-1] == '\t' ||
			 trimmed_end[-1] == '\r' || trimmed_end[-1] == '\n')) trimmed_end--;
		if ((size_t)(trimmed_end - item) == wanted_length &&
			!memcmp(item, wanted, wanted_length)) return 1;
		item = *end ? end + 1 : end;
	}
	return 0;
}

static int allowed(const pex_packet_t * packet, razion_capability_t capability) {
	const char * wanted = NULL;
	for (size_t i = 0; i < sizeof(capability_names) / sizeof(*capability_names); ++i) {
		if (capability_names[i].value == capability) {
			wanted = capability_names[i].name;
			break;
		}
	}
	if (!wanted) return 0;
	for (size_t i = 0; i < rule_count; ++i) {
		if (same_executable(packet->executable, rules[i].executable)) {
			return capability_list_contains(rules[i].capability_list, wanted);
		}
	}
	return 0;
}

static void respond(FILE * endpoint, const pex_packet_t * packet,
	const razion_permission_request_t * request, razion_permission_status_t status,
	const char * detail) {
	razion_permission_response_t response;
	memset(&response, 0, sizeof(response));
	response.magic = RAZION_PERMISSION_MAGIC;
	response.version = RAZION_PERMISSION_VERSION;
	response.status = status;
	response.request_id = request ? request->request_id : 0;
	response.capability = request ? request->capability : 0;
	strncpy(response.detail, detail, sizeof(response.detail) - 1);
	pex_send(endpoint, packet->source, sizeof(response), (char *)&response);
}

static int run_broker(void) {
	if (load_policy()) {
		fprintf(stderr, "razion-permission-broker: no valid policy rules\n");
		return 1;
	}
	FILE * endpoint = pex_bind(RAZION_PERMISSION_ENDPOINT);
	if (!endpoint) {
		fprintf(stderr, "razion-permission-broker: bind failed: %s\n", strerror(errno));
		return 1;
	}
	while (1) {
		pex_packet_t * packet = calloc(1, PACKET_SIZE + 1);
		if (!packet) return 1;
		pex_listen(endpoint, packet);
		if (!packet->source) { free(packet); continue; }
		if (packet->size != sizeof(razion_permission_request_t)) {
			respond(endpoint, packet, NULL, RAZION_PERMISSION_INVALID, "Malformed request.");
			free(packet);
			continue;
		}
		razion_permission_request_t * request = (void *)packet->data;
		if (request->magic != RAZION_PERMISSION_MAGIC ||
			request->version != RAZION_PERMISSION_VERSION ||
			request->operation != RAZION_PERMISSION_CHECK ||
			!request->capability || (request->capability & (request->capability - 1))) {
			respond(endpoint, packet, request, RAZION_PERMISSION_INVALID, "Invalid protocol or capability.");
		} else if (allowed(packet, request->capability)) {
			respond(endpoint, packet, request, RAZION_PERMISSION_GRANTED,
				"Granted by exact executable policy.");
		} else {
			char detail[sizeof(((razion_permission_response_t *)0)->detail)];
			snprintf(detail, sizeof(detail), "Denied by policy for %.120s (uid %u).",
				packet->executable[0] ? packet->executable : "<unknown>", packet->uid);
			respond(endpoint, packet, request, RAZION_PERMISSION_DENIED,
				detail);
		}
		free(packet);
	}
}

int main(int argc, char * argv[]) {
	if (argc > 2 || (argc == 2 && strcmp(argv[1], "--daemon"))) {
		fprintf(stderr, "usage: %s [--daemon]\n", argv[0]);
		return 2;
	}
	if (getuid() != 0) {
		fprintf(stderr, "razion-permission-broker: only root may run the service\n");
		return 1;
	}
	if (argc == 2 && fork()) return 0;
	return run_broker();
}
