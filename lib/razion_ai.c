/**
 * @brief Native client SDK for the Razion AI Engine.
 *
 * Applications use this library instead of communicating with AI providers.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fswait.h>
#include <unistd.h>

#include <toaru/pex.h>
#include <toaru/razion_ai.h>

_Static_assert(sizeof(razion_ai_request_t) <= MAX_PACKET_SIZE,
	"Razion AI requests must fit in one PEX packet");
_Static_assert(sizeof(razion_ai_response_t) <= MAX_PACKET_SIZE,
	"Razion AI responses must fit in one PEX packet");

static int operation_capabilities(razion_ai_operation_t operation) {
	switch (operation) {
		case RAZION_AI_OP_SEARCH:
			return RAZION_AI_CAP_TEXT | RAZION_AI_CAP_FILE_SEARCH;
		case RAZION_AI_OP_ANALYZE:
			return RAZION_AI_CAP_TEXT | RAZION_AI_CAP_SYSTEM_READ;
		case RAZION_AI_OP_RUN_TASK:
			return RAZION_AI_CAP_TEXT | RAZION_AI_CAP_TASK_PROPOSE;
		case RAZION_AI_OP_GENERATE_CODE:
			return RAZION_AI_CAP_TEXT | RAZION_AI_CAP_CODE;
		case RAZION_AI_OP_SEARCH_FILES:
		case RAZION_AI_OP_APPLICATION_SEARCH:
		case RAZION_AI_OP_DOCUMENT_SEARCH:
			return RAZION_AI_CAP_TEXT | RAZION_AI_CAP_FILE_SEARCH;
		case RAZION_AI_OP_ANALYZE_LOGS:
		case RAZION_AI_OP_PROCESS_INFO:
		case RAZION_AI_OP_DISK_INFO:
		case RAZION_AI_OP_NETWORK_DIAGNOSTICS:
		case RAZION_AI_OP_SYSTEM_HEALTH:
			return RAZION_AI_CAP_TEXT | RAZION_AI_CAP_SYSTEM_READ;
		case RAZION_AI_OP_VOICE_COMMAND:
			return RAZION_AI_CAP_TEXT | RAZION_AI_CAP_VOICE |
				RAZION_AI_CAP_TASK_PROPOSE;
		case RAZION_AI_OP_SETTINGS_REQUEST:
			return RAZION_AI_CAP_TEXT | RAZION_AI_CAP_SETTINGS |
				RAZION_AI_CAP_TASK_PROPOSE;
		case RAZION_AI_OP_ASK:
		case RAZION_AI_OP_EXPLAIN_ERROR:
		case RAZION_AI_OP_SUMMARIZE:
			return RAZION_AI_CAP_TEXT;
		default:
			return 0;
	}
}

int razion_ai_init(razion_ai_context_t * context, const char * app_id) {
	if (!context || !app_id || !*app_id) {
		errno = EINVAL;
		return -1;
	}

	memset(context, 0, sizeof(*context));
	strncpy(context->app_id, app_id, sizeof(context->app_id) - 1);
	context->default_flags = RAZION_AI_FLAG_PREFER_LOCAL;
	context->requested_capabilities = RAZION_AI_CAP_TEXT;
	context->next_request_id = ((uint32_t)getpid() << 16) | 1;
	context->timeout_ms = 5000;
	return 0;
}

int razion_ai_request(
	razion_ai_context_t * context,
	razion_ai_operation_t operation,
	const char * input,
	uint32_t flags,
	razion_ai_response_t * response) {

	if (!context || !response) {
		errno = EINVAL;
		return -1;
	}

	size_t input_length = input ? strlen(input) : 0;
	if (input_length >= RAZION_AI_MAX_PAYLOAD) {
		errno = E2BIG;
		return -1;
	}

	razion_ai_request_t request;
	memset(&request, 0, sizeof(request));
	request.magic = RAZION_AI_PROTOCOL_MAGIC;
	request.version = RAZION_AI_PROTOCOL_VERSION;
	request.operation = operation;
	request.request_id = context->next_request_id++;
	request.flags = context->default_flags | flags;
	request.requested_capabilities =
		context->requested_capabilities | operation_capabilities(operation);
	request.payload_length = input_length;
	memcpy(request.app_id, context->app_id, sizeof(request.app_id) - 1);
	request.app_id[sizeof(request.app_id) - 1] = '\0';
	if (input_length) memcpy(request.payload, input, input_length);

	FILE * engine = pex_connect(RAZION_AI_ENDPOINT);
	if (!engine) return -1;

	size_t request_size = offsetof(razion_ai_request_t, payload) + input_length;
	if (pex_reply(engine, request_size, (char *)&request) != request_size) {
		fclose(engine);
		errno = EIO;
		return -1;
	}

	int engine_fd = fileno(engine);
	if (fswait2(1, &engine_fd, context->timeout_ms) != 0) {
		fclose(engine);
		errno = ETIMEDOUT;
		return -1;
	}

	char reply_buffer[MAX_PACKET_SIZE];
	size_t received = pex_recv(engine, reply_buffer);
	fclose(engine);

	if (received < offsetof(razion_ai_response_t, payload)) {
		errno = EPROTO;
		return -1;
	}

	razion_ai_response_t * reply = (razion_ai_response_t *)reply_buffer;
	if (reply->magic != RAZION_AI_PROTOCOL_MAGIC ||
		reply->version != RAZION_AI_PROTOCOL_VERSION ||
		reply->request_id != request.request_id ||
		reply->payload_length >= RAZION_AI_MAX_PAYLOAD ||
		offsetof(razion_ai_response_t, payload) + reply->payload_length > received) {
		errno = EPROTO;
		return -1;
	}

	memset(response, 0, sizeof(*response));
	memcpy(response, reply,
		offsetof(razion_ai_response_t, payload) + reply->payload_length);
	response->provider[sizeof(response->provider) - 1] = '\0';
	response->payload[response->payload_length] = '\0';
	return 0;
}

int razion_ai_ask(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_ASK, input, 0, response);
}

int razion_ai_search(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_SEARCH, input, 0, response);
}

int razion_ai_analyze(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_ANALYZE, input, 0, response);
}

int razion_ai_run_task(
	razion_ai_context_t * context,
	const char * input,
	uint32_t flags,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_RUN_TASK, input, flags, response);
}

int razion_ai_summarize(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_SUMMARIZE, input, 0, response);
}

int razion_ai_generate_code(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_GENERATE_CODE, input, 0, response);
}

int razion_ai_explain_error(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_EXPLAIN_ERROR, input, 0, response);
}

int razion_ai_search_files(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_SEARCH_FILES, query, 0, response);
}

int razion_ai_analyze_logs(
	razion_ai_context_t * context,
	const char * scope,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_ANALYZE_LOGS, scope, 0, response);
}

int razion_ai_process_info(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_PROCESS_INFO, query, 0, response);
}

int razion_ai_disk_info(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_DISK_INFO, query, 0, response);
}

int razion_ai_network_diagnostics(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_NETWORK_DIAGNOSTICS, query, 0, response);
}

int razion_ai_system_health(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_SYSTEM_HEALTH, query, 0, response);
}

int razion_ai_application_search(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_APPLICATION_SEARCH, query, 0, response);
}

int razion_ai_voice_command(
	razion_ai_context_t * context,
	const char * transcript,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_VOICE_COMMAND, transcript, 0, response);
}

int razion_ai_document_search(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_DOCUMENT_SEARCH, query, 0, response);
}

int razion_ai_settings_request(
	razion_ai_context_t * context,
	const char * request,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_SETTINGS_REQUEST, request, 0, response);
}

int razion_ai_health(
	razion_ai_context_t * context,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_HEALTH, NULL, 0, response);
}

int razion_ai_list_providers(
	razion_ai_context_t * context,
	razion_ai_response_t * response) {
	return razion_ai_request(context, RAZION_AI_OP_LIST_PROVIDERS, NULL, 0, response);
}

const char * razion_ai_status_string(razion_ai_status_t status) {
	switch (status) {
		case RAZION_AI_STATUS_OK: return "ok";
		case RAZION_AI_STATUS_INVALID_REQUEST: return "invalid request";
		case RAZION_AI_STATUS_PROTOCOL_ERROR: return "protocol error";
		case RAZION_AI_STATUS_NO_PROVIDER: return "no compatible provider";
		case RAZION_AI_STATUS_PROVIDER_UNAVAILABLE: return "provider unavailable";
		case RAZION_AI_STATUS_PERMISSION_DENIED: return "permission denied";
		case RAZION_AI_STATUS_CONFIRMATION_REQUIRED: return "confirmation required";
		case RAZION_AI_STATUS_UNSUPPORTED: return "unsupported";
		case RAZION_AI_STATUS_INTERNAL_ERROR: return "internal error";
		default: return "unknown status";
	}
}
