/**
 * @brief Razion AI Engine - provider-independent system AI service.
 *
 * Phase 1 establishes a versioned SDK protocol, local-first provider routing,
 * cloud opt-in, action safety policy, and metadata-only auditing. Provider
 * adapters are separate PEX services so third-party code is never loaded into
 * this privileged process.
 */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fswait.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <toaru/confreader.h>
#include <toaru/hashmap.h>
#include <toaru/pex.h>
#include <toaru/razion_ai.h>
#include <toaru/razion_ai_provider.h>

#define ENGINE_CONFIG "/etc/razion-ai.conf"
#define DEFAULT_AUDIT_LOG "/var/log/razion-ai.log"
#define MAX_PROVIDERS 24

typedef struct {
	int local_first;
	int allow_cloud;
	int cloud_fallback;
	int audit_enabled;
	char audit_log[160];
	int provider_timeout_ms;
	razion_ai_provider_descriptor_t providers[MAX_PROVIDERS];
	size_t provider_count;
} engine_config_t;

static engine_config_t engine_config = {
	.local_first = 1,
	.allow_cloud = 0,
	.cloud_fallback = 1,
	.audit_enabled = 1,
	.audit_log = DEFAULT_AUDIT_LOG,
	.provider_timeout_ms = 5000,
};

_Static_assert(sizeof(razion_ai_request_t) <= MAX_PACKET_SIZE,
	"Razion AI requests must fit in one PEX packet");
_Static_assert(sizeof(razion_ai_response_t) <= MAX_PACKET_SIZE,
	"Razion AI responses must fit in one PEX packet");

static uint32_t parse_capabilities(const char * value) {
	uint32_t out = 0;
	if (!value) return out;
	if (strstr(value, "text")) out |= RAZION_AI_CAP_TEXT;
	if (strstr(value, "search")) out |= RAZION_AI_CAP_FILE_SEARCH;
	if (strstr(value, "system-read")) out |= RAZION_AI_CAP_SYSTEM_READ;
	if (strstr(value, "task-propose")) out |= RAZION_AI_CAP_TASK_PROPOSE;
	if (strstr(value, "task-execute")) out |= RAZION_AI_CAP_TASK_EXECUTE;
	if (strstr(value, "voice")) out |= RAZION_AI_CAP_VOICE;
	if (strstr(value, "code")) out |= RAZION_AI_CAP_CODE;
	if (strstr(value, "settings")) out |= RAZION_AI_CAP_SETTINGS;
	return out;
}

static void load_provider(confreader_t * config, const char * section) {
	if (engine_config.provider_count >= MAX_PROVIDERS) return;

	char * enabled = confreader_getd(config, (char *)section, "enabled", "0");
	if (strcmp(enabled, "1") && strcmp(enabled, "yes") && strcmp(enabled, "true")) return;

	razion_ai_provider_descriptor_t * provider =
		&engine_config.providers[engine_config.provider_count];
	memset(provider, 0, sizeof(*provider));

	const char * name = section + strlen("provider.");
	strncpy(provider->name, name, sizeof(provider->name) - 1);
	strncpy(provider->endpoint,
		confreader_getd(config, (char *)section, "endpoint", ""),
		sizeof(provider->endpoint) - 1);

	char * provider_class =
		confreader_getd(config, (char *)section, "class", "cloud");
	provider->provider_class = !strcmp(provider_class, "local") ?
		RAZION_AI_PROVIDER_LOCAL : RAZION_AI_PROVIDER_CLOUD;
	provider->priority =
		confreader_intd(config, (char *)section, "priority", 100);
	provider->capabilities = parse_capabilities(
		confreader_getd(config, (char *)section, "capabilities", "text"));
	provider->enabled = provider->endpoint[0] != '\0';

	if (provider->enabled) engine_config.provider_count++;
}

static void load_config(void) {
	confreader_t * config = confreader_load(ENGINE_CONFIG);
	if (!config) return;

	engine_config.local_first =
		confreader_intd(config, "engine", "local_first", 1);
	engine_config.allow_cloud =
		confreader_intd(config, "engine", "allow_cloud", 0);
	engine_config.cloud_fallback =
		confreader_intd(config, "engine", "cloud_fallback", 1);
	engine_config.audit_enabled =
		confreader_intd(config, "engine", "audit_enabled", 1);
	engine_config.provider_timeout_ms =
		confreader_intd(config, "engine", "provider_timeout_ms", 5000);
	if (engine_config.provider_timeout_ms < 100) {
		engine_config.provider_timeout_ms = 100;
	} else if (engine_config.provider_timeout_ms > 30000) {
		engine_config.provider_timeout_ms = 30000;
	}
	strncpy(engine_config.audit_log,
		confreader_getd(config, "engine", "audit_log", DEFAULT_AUDIT_LOG),
		sizeof(engine_config.audit_log) - 1);

	list_t * sections = hashmap_keys(config->sections);
	foreach (node, sections) {
		char * section = node->value;
		if (!strncmp(section, "provider.", strlen("provider."))) {
			load_provider(config, section);
		}
	}
	list_free(sections);
	free(sections);
	confreader_free(config);
}

static int provider_is_available(razion_ai_provider_descriptor_t * provider) {
	FILE * endpoint = pex_connect(provider->endpoint);
	if (!endpoint) return 0;
	fclose(endpoint);
	return 1;
}

static int provider_matches(
	razion_ai_provider_descriptor_t * provider,
	razion_ai_request_t * request,
	razion_ai_provider_class_t provider_class) {

	if (!provider->enabled || provider->provider_class != provider_class) return 0;
	if ((provider->capabilities & request->requested_capabilities) !=
		request->requested_capabilities) return 0;

	if (provider_class == RAZION_AI_PROVIDER_CLOUD) {
		if (!engine_config.allow_cloud) return 0;
		if (!(request->flags & RAZION_AI_FLAG_ALLOW_CLOUD)) return 0;
		if (request->flags & RAZION_AI_FLAG_LOCAL_ONLY) return 0;
	}

	return provider_is_available(provider);
}

static int select_provider(
	razion_ai_request_t * request,
	razion_ai_provider_class_t provider_class,
	int attempted[MAX_PROVIDERS]) {

	int selected = -1;
	for (size_t i = 0; i < engine_config.provider_count; ++i) {
		if (attempted[i]) continue;
		razion_ai_provider_descriptor_t * candidate = &engine_config.providers[i];
		if (!provider_matches(candidate, request, provider_class)) continue;
		if (selected < 0 ||
			candidate->priority < engine_config.providers[selected].priority) {
			selected = i;
		}
	}
	return selected;
}

static void set_response(
	razion_ai_response_t * response,
	razion_ai_request_t * request,
	razion_ai_status_t status,
	const char * provider,
	const char * message) {

	memset(response, 0, sizeof(*response));
	response->magic = RAZION_AI_PROTOCOL_MAGIC;
	response->version = RAZION_AI_PROTOCOL_VERSION;
	response->status = status;
	response->request_id = request ? request->request_id : 0;
	if (provider) strncpy(response->provider, provider, sizeof(response->provider) - 1);
	if (message) {
		response->payload_length = strlen(message);
		if (response->payload_length >= sizeof(response->payload)) {
			response->payload_length = sizeof(response->payload) - 1;
		}
		memcpy(response->payload, message, response->payload_length);
	}
}

static const char * operation_name(uint16_t operation) {
	switch (operation) {
		case RAZION_AI_OP_HEALTH: return "health";
		case RAZION_AI_OP_LIST_PROVIDERS: return "providers";
		case RAZION_AI_OP_ASK: return "ask";
		case RAZION_AI_OP_SEARCH: return "search";
		case RAZION_AI_OP_ANALYZE: return "analyze";
		case RAZION_AI_OP_RUN_TASK: return "run-task";
		case RAZION_AI_OP_SUMMARIZE: return "summarize";
		case RAZION_AI_OP_GENERATE_CODE: return "generate-code";
		case RAZION_AI_OP_EXPLAIN_ERROR: return "explain-error";
		case RAZION_AI_OP_SEARCH_FILES: return "search-files";
		case RAZION_AI_OP_ANALYZE_LOGS: return "analyze-logs";
		case RAZION_AI_OP_PROCESS_INFO: return "process-info";
		case RAZION_AI_OP_DISK_INFO: return "disk-info";
		case RAZION_AI_OP_NETWORK_DIAGNOSTICS: return "network-diagnostics";
		case RAZION_AI_OP_SYSTEM_HEALTH: return "system-health";
		case RAZION_AI_OP_APPLICATION_SEARCH: return "application-search";
		case RAZION_AI_OP_VOICE_COMMAND: return "voice-command";
		case RAZION_AI_OP_DOCUMENT_SEARCH: return "document-search";
		case RAZION_AI_OP_SETTINGS_REQUEST: return "settings-request";
		default: return "unknown";
	}
}

static void audit_request(
	uintptr_t source,
	razion_ai_request_t * request,
	razion_ai_response_t * response) {

	if (!engine_config.audit_enabled) return;
	mkdir("/var/log", 0755);
	FILE * log = fopen(engine_config.audit_log, "a");
	if (!log) return;

	time_t now = time(NULL);
	fprintf(log,
		"time=%ld source=%lu app=%s operation=%s provider=%s status=%u flags=0x%x capabilities=0x%x\n",
		now,
		(unsigned long)source,
		request->app_id,
		operation_name(request->operation),
		response->provider[0] ? response->provider : "none",
		response->status,
		request->flags,
		request->requested_capabilities);
	fclose(log);
}

static void health_response(
	razion_ai_request_t * request,
	razion_ai_response_t * response) {

	size_t available = 0;
	for (size_t i = 0; i < engine_config.provider_count; ++i) {
		if (provider_is_available(&engine_config.providers[i])) available++;
	}

	char message[RAZION_AI_MAX_PAYLOAD];
	snprintf(message, sizeof(message),
		"engine=ready;protocol=%d;providers=%d;available=%d;local_first=%s;cloud=%s",
		RAZION_AI_PROTOCOL_VERSION,
		(int)engine_config.provider_count,
		(int)available,
		engine_config.local_first ? "on" : "off",
		engine_config.allow_cloud ? "opt-in" : "disabled");
	set_response(response, request, RAZION_AI_STATUS_OK, "engine", message);
}

static void provider_list_response(
	razion_ai_request_t * request,
	razion_ai_response_t * response) {

	char message[RAZION_AI_MAX_PAYLOAD] = {0};
	size_t used = 0;
	for (size_t i = 0; i < engine_config.provider_count; ++i) {
		razion_ai_provider_descriptor_t * provider = &engine_config.providers[i];
		int written = snprintf(message + used, sizeof(message) - used,
			"%s%s:%s:%s:priority=%d",
			used ? ";" : "",
			provider->name,
			provider->provider_class == RAZION_AI_PROVIDER_LOCAL ? "local" : "cloud",
			provider_is_available(provider) ? "available" : "unavailable",
			provider->priority);
		if (written < 0 || (size_t)written >= sizeof(message) - used) break;
		used += written;
	}
	set_response(response, request, RAZION_AI_STATUS_OK, "engine",
		used ? message : "no providers configured");
}

static int forward_to_provider(
	razion_ai_provider_descriptor_t * provider,
	razion_ai_request_t * request,
	razion_ai_response_t * response) {

	FILE * endpoint = pex_connect(provider->endpoint);
	if (!endpoint) return -1;

	size_t request_size =
		offsetof(razion_ai_request_t, payload) + request->payload_length;
	if (pex_reply(endpoint, request_size, (char *)request) != request_size) {
		fclose(endpoint);
		return -1;
	}

	int provider_fd = fileno(endpoint);
	if (fswait2(1, &provider_fd, engine_config.provider_timeout_ms) != 0) {
		fclose(endpoint);
		return -1;
	}

	char buffer[MAX_PACKET_SIZE];
	size_t received = pex_recv(endpoint, buffer);
	fclose(endpoint);
	if (received < offsetof(razion_ai_response_t, payload)) return -1;

	razion_ai_response_t * provider_response = (razion_ai_response_t *)buffer;
	if (provider_response->magic != RAZION_AI_PROTOCOL_MAGIC ||
		provider_response->version != RAZION_AI_PROTOCOL_VERSION ||
		provider_response->request_id != request->request_id ||
		provider_response->payload_length >= RAZION_AI_MAX_PAYLOAD ||
		offsetof(razion_ai_response_t, payload) + provider_response->payload_length > received) {
		return -1;
	}

	memcpy(response, provider_response,
		offsetof(razion_ai_response_t, payload) + provider_response->payload_length);
	strncpy(response->provider, provider->name, sizeof(response->provider) - 1);
	response->provider[sizeof(response->provider) - 1] = '\0';
	response->granted_capabilities &=
		provider->capabilities & request->requested_capabilities;
	response->payload[response->payload_length] = '\0';
	return 0;
}

static int route_to_provider(
	razion_ai_request_t * request,
	razion_ai_response_t * response,
	const char ** last_provider) {

	int attempted[MAX_PROVIDERS] = {0};
	int prefer_local =
		engine_config.local_first || (request->flags & RAZION_AI_FLAG_PREFER_LOCAL);
	razion_ai_provider_class_t order[2] = {
		prefer_local ? RAZION_AI_PROVIDER_LOCAL : RAZION_AI_PROVIDER_CLOUD,
		prefer_local ? RAZION_AI_PROVIDER_CLOUD : RAZION_AI_PROVIDER_LOCAL,
	};
	int attempted_any = 0;

	for (int class_index = 0; class_index < 2; ++class_index) {
		if (class_index == 1 &&
			order[0] == RAZION_AI_PROVIDER_LOCAL &&
			!engine_config.cloud_fallback) {
			break;
		}

		while (1) {
			int selected = select_provider(request, order[class_index], attempted);
			if (selected < 0) break;
			attempted[selected] = 1;
			attempted_any = 1;
			razion_ai_provider_descriptor_t * provider =
				&engine_config.providers[selected];
			*last_provider = provider->name;
			if (!forward_to_provider(provider, request, response)) return 0;
		}
	}

	return attempted_any ? -2 : -1;
}

static void handle_request(
	razion_ai_request_t * request,
	razion_ai_response_t * response) {

	if (request->flags & RAZION_AI_FLAG_EXECUTE_ACTION) {
		if (!(request->flags & RAZION_AI_FLAG_USER_CONFIRMED)) {
			set_response(response, request, RAZION_AI_STATUS_CONFIRMATION_REQUIRED,
				"engine", "Action execution requires explicit user confirmation.");
		} else {
			set_response(response, request, RAZION_AI_STATUS_PERMISSION_DENIED,
				"engine", "Phase 1 does not grant action-execution capabilities.");
		}
		return;
	}

	switch (request->operation) {
		case RAZION_AI_OP_HEALTH:
			health_response(request, response);
			return;
		case RAZION_AI_OP_LIST_PROVIDERS:
			provider_list_response(request, response);
			return;
		case RAZION_AI_OP_ASK:
		case RAZION_AI_OP_SEARCH:
		case RAZION_AI_OP_ANALYZE:
		case RAZION_AI_OP_RUN_TASK:
		case RAZION_AI_OP_SUMMARIZE:
		case RAZION_AI_OP_GENERATE_CODE:
		case RAZION_AI_OP_EXPLAIN_ERROR:
		case RAZION_AI_OP_SEARCH_FILES:
		case RAZION_AI_OP_ANALYZE_LOGS:
		case RAZION_AI_OP_PROCESS_INFO:
		case RAZION_AI_OP_DISK_INFO:
		case RAZION_AI_OP_NETWORK_DIAGNOSTICS:
		case RAZION_AI_OP_SYSTEM_HEALTH:
		case RAZION_AI_OP_APPLICATION_SEARCH:
		case RAZION_AI_OP_VOICE_COMMAND:
		case RAZION_AI_OP_DOCUMENT_SEARCH:
		case RAZION_AI_OP_SETTINGS_REQUEST:
			break;
		default:
			set_response(response, request, RAZION_AI_STATUS_UNSUPPORTED,
				"engine", "The requested operation is not supported.");
			return;
	}

	const char * last_provider = NULL;
	int routed = route_to_provider(request, response, &last_provider);
	if (routed == -1) {
		set_response(response, request, RAZION_AI_STATUS_NO_PROVIDER,
			"engine", "No compatible provider is available under the current privacy policy.");
		return;
	}
	if (routed == -2) {
		set_response(response, request, RAZION_AI_STATUS_PROVIDER_UNAVAILABLE,
			last_provider, "All compatible providers failed or timed out.");
	}
}

static int validate_request(pex_packet_t * packet, razion_ai_request_t ** request_out) {
	if (packet->size < offsetof(razion_ai_request_t, payload)) return 0;
	razion_ai_request_t * request = (razion_ai_request_t *)packet->data;
	if (request->magic != RAZION_AI_PROTOCOL_MAGIC ||
		request->version != RAZION_AI_PROTOCOL_VERSION ||
		request->payload_length >= RAZION_AI_MAX_PAYLOAD ||
		offsetof(razion_ai_request_t, payload) + request->payload_length > packet->size) {
		return 0;
	}
	request->app_id[sizeof(request->app_id) - 1] = '\0';
	for (size_t i = 0; request->app_id[i]; ++i) {
		unsigned char c = request->app_id[i];
		if (!isalnum(c) && c != '.' && c != '_' && c != '-') {
			request->app_id[i] = '_';
		}
	}
	request->payload[request->payload_length] = '\0';
	*request_out = request;
	return 1;
}

static int run_engine(void) {
	load_config();
	FILE * endpoint = pex_bind(RAZION_AI_ENDPOINT);
	if (!endpoint) {
		fprintf(stderr, "razion-ai-engine: unable to bind %s: %s\n",
			RAZION_AI_ENDPOINT, strerror(errno));
		return 1;
	}

	while (1) {
		pex_packet_t * packet = calloc(1, PACKET_SIZE + 1);
		if (!packet) return 1;
		pex_listen(endpoint, packet);

		razion_ai_request_t * request = NULL;
		razion_ai_response_t response;
		if (!validate_request(packet, &request)) {
			razion_ai_request_t invalid;
			memset(&invalid, 0, sizeof(invalid));
			if (packet->size >= offsetof(razion_ai_request_t, request_id) +
				sizeof(invalid.request_id)) {
				memcpy(&invalid.request_id,
					packet->data + offsetof(razion_ai_request_t, request_id),
					sizeof(invalid.request_id));
			}
			set_response(&response, &invalid, RAZION_AI_STATUS_PROTOCOL_ERROR,
				"engine", "Invalid Razion AI protocol request.");
		} else {
			handle_request(request, &response);
			audit_request(packet->source, request, &response);
		}

		size_t response_size =
			offsetof(razion_ai_response_t, payload) + response.payload_length;
		pex_send(endpoint, packet->source, response_size, (char *)&response);
		free(packet);
	}
}

static int usage(const char * argv0) {
	fprintf(stderr,
		"Razion AI Engine\n"
		"usage: %s [--daemon]\n",
		argv0);
	return 1;
}

int main(int argc, char * argv[]) {
	int daemonize = 0;
	if (argc == 2 && !strcmp(argv[1], "--daemon")) {
		daemonize = 1;
	} else if (argc != 1) {
		return usage(argv[0]);
	}

	if (getuid() != 0) {
		fprintf(stderr, "razion-ai-engine: only root may run the system service\n");
		return 1;
	}

	if (daemonize && fork()) return 0;
	return run_engine();
}
