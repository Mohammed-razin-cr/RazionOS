/**
 * @brief Isolated llama.cpp provider adapter for the Razion AI Engine.
 *
 * This service is intentionally provider-specific. The privileged engine
 * communicates with it only through the versioned PEX protocol and never
 * loads this code into its process.
 */
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <toaru/confreader.h>
#include <toaru/json.h>
#include <toaru/pex.h>
#include <toaru/razion_ai.h>
#include <toaru/razion_ai_provider.h>

#define LLAMA_CPP_CONFIG "/etc/razion-ai-llama-cpp.conf"
#define LLAMA_CPP_ENDPOINT "razion-ai-provider-llama-cpp"
#define LLAMA_CPP_MAX_HTTP_RESPONSE 65536
#define LLAMA_CPP_MAX_JSON_REQUEST 8192

typedef struct {
	char host[128];
	int port;
	char model[128];
	int timeout_ms;
	int max_tokens;
	int allow_remote;
	uint32_t last_latency_ms;
} llama_cpp_context_t;

static llama_cpp_context_t llama_cpp = {
	.host = "10.0.2.2",
	.port = 8080,
	.timeout_ms = 4000,
	.max_tokens = 192,
	.allow_remote = 1,
};
static char last_http_error[96] = "not-requested";

static int http_error(const char * stage) {
	snprintf(last_http_error, sizeof(last_http_error),
		"%s;errno=%d", stage, errno);
	return -1;
}

static uint32_t elapsed_ms(struct timeval * start, struct timeval * end) {
	uint64_t start_us = (uint64_t)start->tv_sec * 1000000ULL + start->tv_usec;
	uint64_t end_us = (uint64_t)end->tv_sec * 1000000ULL + end->tv_usec;
	return (uint32_t)((end_us - start_us) / 1000ULL);
}

static int is_local_host(const char * host) {
	return !strcmp(host, "127.0.0.1") || !strcmp(host, "localhost");
}

static int valid_host(const char * host) {
	if (!host || !*host) return 0;
	for (const unsigned char * c = (const unsigned char *)host; *c; ++c) {
		if (!isalnum(*c) && *c != '.' && *c != '-' && *c != '_') return 0;
	}
	return 1;
}

static int llama_cpp_initialize(void * opaque, const char * config_path) {
	llama_cpp_context_t * context = opaque;
	confreader_t * config = confreader_load(config_path);
	if (config) {
		strncpy(context->host,
			confreader_getd(config, "llama_cpp", "host", "10.0.2.2"),
			sizeof(context->host) - 1);
		context->port = confreader_intd(config, "llama_cpp", "port", 8080);
		strncpy(context->model,
			confreader_getd(config, "llama_cpp", "model", ""),
			sizeof(context->model) - 1);
		context->timeout_ms =
			confreader_intd(config, "llama_cpp", "timeout_ms", 4000);
		context->max_tokens =
			confreader_intd(config, "llama_cpp", "max_tokens", 192);
		context->allow_remote =
			confreader_intd(config, "llama_cpp", "allow_remote", 1);
		confreader_free(config);
	}

	context->host[sizeof(context->host) - 1] = '\0';
	context->model[sizeof(context->model) - 1] = '\0';
	if (!valid_host(context->host)) return -1;
	if (!is_local_host(context->host) && !context->allow_remote) return -1;
	if (context->port < 1 || context->port > 65535) return -1;
	if (context->timeout_ms < 250) context->timeout_ms = 250;
	if (context->timeout_ms > 30000) context->timeout_ms = 30000;
	if (context->max_tokens < 16) context->max_tokens = 16;
	if (context->max_tokens > 2048) context->max_tokens = 2048;
	return 0;
}

static void llama_cpp_shutdown(void * opaque) {
	(void)opaque;
}

static int send_all(int fd, const char * data, size_t length) {
	size_t sent = 0;
	while (sent < length) {
		ssize_t written = send(fd, data + sent, length - sent, 0);
		if (written <= 0) return -1;
		sent += written;
	}
	return 0;
}

static int recv_line(int fd, char * line, size_t capacity) {
	size_t used = 0;
	while (used + 1 < capacity) {
		char c;
		ssize_t n = recv(fd, &c, 1, 0);
		if (n != 1) {
			snprintf(last_http_error, sizeof(last_http_error),
				"recv-line:bytes=%d;used=%u;errno=%d", (int)n, (unsigned)used, errno);
			return -1;
		}
		if (c == '\n') {
			if (used && line[used - 1] == '\r') used--;
			line[used] = '\0';
			return 0;
		}
		line[used++] = c;
	}
	snprintf(last_http_error, sizeof(last_http_error), "recv-line:too-long");
	return -1;
}

static int http_request(
	llama_cpp_context_t * context,
	const char * method,
	const char * path,
	const char * body,
	char ** response_body,
	int * status_code) {

	*response_body = NULL;
	*status_code = 0;

	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) return http_error("socket");

	struct hostent * remote = gethostbyname(context->host);
	if (!remote) {
		close(socket_fd);
		return http_error("resolve");
	}

	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	memcpy(&address.sin_addr.s_addr, remote->h_addr, remote->h_length);
	address.sin_port = htons(context->port);
	if (connect(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		close(socket_fd);
		return http_error("connect");
	}

	struct timeval timeout = {
		.tv_sec = context->timeout_ms / 1000,
		.tv_usec = (context->timeout_ms % 1000) * 1000,
	};
	setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	size_t body_length = body ? strlen(body) : 0;
	char header[512];
	int header_length = snprintf(header, sizeof(header),
		"%s %s HTTP/1.0\r\n"
		"Host: %s:%d\r\n"
		"User-Agent: RazionOS-llama.cpp-Adapter/0.1\r\n"
		"Accept: application/json\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %u\r\n"
		"Connection: close\r\n\r\n",
		method, path, context->host, context->port, (unsigned)body_length);
	if (header_length < 0 || (size_t)header_length >= sizeof(header) ||
		send_all(socket_fd, header, header_length) ||
		(body_length && send_all(socket_fd, body, body_length))) {
		close(socket_fd);
		return http_error("write");
	}

	char line[1024];
	if (recv_line(socket_fd, line, sizeof(line))) {
		close(socket_fd);
		return -1;
	}
	char * code = strchr(line, ' ');
	if (strncmp(line, "HTTP/", 5) || !code ||
		(*status_code = atoi(code + 1)) < 100 || *status_code > 599) {
		close(socket_fd);
		return http_error("status-line");
	}

	size_t content_length = 0;
	int have_content_length = 0;
	while (1) {
		if (recv_line(socket_fd, line, sizeof(line))) {
			close(socket_fd);
			return http_error("headers");
		}
		if (!line[0]) break;
		if (!strncasecmp(line, "Content-Length:", 15)) {
			char * value = line + 15;
			while (*value == ' ' || *value == '\t') value++;
			content_length = strtoul(value, NULL, 10);
			have_content_length = 1;
		}
	}

	if (!have_content_length || content_length > LLAMA_CPP_MAX_HTTP_RESPONSE) {
		close(socket_fd);
		return http_error(have_content_length ?
			"response-too-large" : "missing-content-length");
	}

	*response_body = malloc(content_length + 1);
	if (!*response_body) {
		close(socket_fd);
		return http_error("allocate-response");
	}
	size_t used = 0;
	while (used < content_length) {
		ssize_t received =
			recv(socket_fd, *response_body + used, content_length - used, 0);
		if (received <= 0) {
			free(*response_body);
			*response_body = NULL;
			close(socket_fd);
			return http_error("response-body");
		}
		used += received;
	}
	(*response_body)[content_length] = '\0';
	close(socket_fd);
	strncpy(last_http_error, "none", sizeof(last_http_error) - 1);
	return 0;
}

static int json_escape(const char * input, char * output, size_t output_size) {
	size_t used = 0;
	for (const unsigned char * c = (const unsigned char *)input; *c; ++c) {
		const char * replacement = NULL;
		char unicode[7];
		switch (*c) {
			case '"': replacement = "\\\""; break;
			case '\\': replacement = "\\\\"; break;
			case '\b': replacement = "\\b"; break;
			case '\f': replacement = "\\f"; break;
			case '\n': replacement = "\\n"; break;
			case '\r': replacement = "\\r"; break;
			case '\t': replacement = "\\t"; break;
			default:
				if (*c < 0x20) {
					snprintf(unicode, sizeof(unicode), "\\u%04x", *c);
					replacement = unicode;
				}
		}
		if (replacement) {
			size_t length = strlen(replacement);
			if (used + length >= output_size) return -1;
			memcpy(output + used, replacement, length);
			used += length;
		} else {
			if (used + 1 >= output_size) return -1;
			output[used++] = *c;
		}
	}
	output[used] = '\0';
	return 0;
}

static struct JSON_Value * object_value(
	struct JSON_Value * object,
	const char * key,
	enum JSON_Type type) {

	if (!object || object->type != JSON_TYPE_OBJECT) return NULL;
	struct JSON_Value * value = JSON_KEY(object, (char *)key);
	if (!value || value->type != type) return NULL;
	return value;
}

static int discover_model(
	llama_cpp_context_t * context,
	char * selected,
	size_t selected_size,
	char * detail,
	size_t detail_size) {

	struct timeval start, end;
	gettimeofday(&start, NULL);
	char * response = NULL;
	int status = 0;
	if (http_request(context, "GET", "/v1/models", NULL, &response, &status)) {
		free(response);
		snprintf(detail, detail_size,
			"llama.cpp is not reachable at %.64s:%d (%.48s)",
			context->host, context->port, last_http_error);
		return -1;
	}
	if (status != 200) {
		free(response);
		snprintf(detail, detail_size,
			"llama.cpp health returned HTTP %d", status);
		return -1;
	}
	gettimeofday(&end, NULL);
	context->last_latency_ms = elapsed_ms(&start, &end);

	struct JSON_Value * root = json_parse(response);
	free(response);
	struct JSON_Value * models = object_value(root, "data", JSON_TYPE_ARRAY);
	if (!models || !models->array->length) {
		if (root) json_free(root);
		snprintf(detail, detail_size, "llama.cpp is running but no local model is installed");
		return -1;
	}

	const char * first = NULL;
	int configured_found = context->model[0] ? 0 : 1;
	foreach (node, models->array) {
		struct JSON_Value * entry = node->value;
		struct JSON_Value * model = object_value(entry, "id", JSON_TYPE_STRING);
		if (!model) model = object_value(entry, "model", JSON_TYPE_STRING);
		if (!model) model = object_value(entry, "name", JSON_TYPE_STRING);
		if (!model) continue;
		if (!first) first = model->string;
		if (context->model[0] && !strcmp(context->model, model->string)) {
			configured_found = 1;
		}
	}

	if (!first || !configured_found) {
		json_free(root);
		snprintf(detail, detail_size,
			context->model[0] ? "Configured llama.cpp model is not installed" :
			"llama.cpp did not return a usable model");
		return -1;
	}
	strncpy(selected, context->model[0] ? context->model : first, selected_size - 1);
	selected[selected_size - 1] = '\0';
	snprintf(detail, detail_size, "ready;model=%s;latency_ms=%u",
		selected, context->last_latency_ms);
	json_free(root);
	return 0;
}

static int llama_cpp_health(void * opaque, char * detail, size_t detail_size) {
	llama_cpp_context_t * context = opaque;
	char model[sizeof(context->model)];
	return discover_model(context, model, sizeof(model), detail, detail_size);
}

static int llama_cpp_available(void * opaque) {
	char detail[RAZION_AI_PROVIDER_MAX_DETAIL];
	return llama_cpp_health(opaque, detail, sizeof(detail)) == 0;
}

static uint32_t llama_cpp_latency(void * opaque) {
	return ((llama_cpp_context_t *)opaque)->last_latency_ms;
}

static const char * operation_instruction(razion_ai_operation_t operation) {
	switch (operation) {
		case RAZION_AI_OP_SUMMARIZE:
			return "Summarize the following content accurately and concisely:";
		case RAZION_AI_OP_GENERATE_CODE:
			return "Generate concise, safe code for this request. Return code and essential explanation only:";
		case RAZION_AI_OP_EXPLAIN_ERROR:
			return "Explain this error and suggest safe diagnostic steps. Do not claim to have executed anything:";
		case RAZION_AI_OP_CLASSIFY_INTENT:
			return "Return ONLY one exact action token: open-terminal, open-calculator, open-file-browser, open-system-monitor, open-settings, open-wallpaper-settings, open-home, open-downloads, check-memory, create-python-project, find-recent-pdf, or unknown. No explanation. Command:";
		case RAZION_AI_OP_ANALYZE:
		case RAZION_AI_OP_ANALYZE_LOGS:
		case RAZION_AI_OP_PROCESS_INFO:
		case RAZION_AI_OP_DISK_INFO:
		case RAZION_AI_OP_NETWORK_DIAGNOSTICS:
		case RAZION_AI_OP_SYSTEM_HEALTH:
			return "Analyze only the supplied data. State uncertainty and do not claim direct system access:";
		case RAZION_AI_OP_SEARCH:
		case RAZION_AI_OP_SEARCH_FILES:
		case RAZION_AI_OP_APPLICATION_SEARCH:
		case RAZION_AI_OP_DOCUMENT_SEARCH:
			return "Use only the supplied search context. Do not invent files or results:";
		case RAZION_AI_OP_RUN_TASK:
		case RAZION_AI_OP_SETTINGS_REQUEST:
		case RAZION_AI_OP_VOICE_COMMAND:
			return "Propose a safe action, but do not execute commands or claim the action was performed:";
		default:
			return "Answer clearly and concisely:";
	}
}

static int llama_cpp_generate(
	void * opaque,
	const razion_ai_provider_request_t * request,
	razion_ai_provider_result_t * result) {

	llama_cpp_context_t * context = opaque;
	char selected_model[sizeof(context->model)];
	char detail[RAZION_AI_PROVIDER_MAX_DETAIL];
	if (discover_model(context, selected_model, sizeof(selected_model),
		detail, sizeof(detail))) {
		result->status = RAZION_AI_STATUS_PROVIDER_UNAVAILABLE;
		strncpy(result->output, detail, sizeof(result->output) - 1);
		return result->status;
	}

	char prompt[RAZION_AI_MAX_PAYLOAD + 320];
	snprintf(prompt, sizeof(prompt), "%s\n\n%.*s",
		operation_instruction(request->operation),
		(int)request->input_length, request->input ? request->input : "");

	char escaped_prompt[LLAMA_CPP_MAX_JSON_REQUEST / 2];
	char escaped_model[512];
	if (json_escape(prompt, escaped_prompt, sizeof(escaped_prompt)) ||
		json_escape(selected_model, escaped_model, sizeof(escaped_model))) {
		result->status = RAZION_AI_STATUS_INVALID_REQUEST;
		strncpy(result->output, "Request is too large after encoding.",
			sizeof(result->output) - 1);
		return result->status;
	}

	char body[LLAMA_CPP_MAX_JSON_REQUEST];
	int classification = request->operation == RAZION_AI_OP_CLASSIFY_INTENT;
	int body_length = snprintf(body, sizeof(body),
		"{\"model\":\"%s\",\"messages\":[{\"role\":\"user\","
		"\"content\":\"%s\"}],\"stream\":false,\"max_tokens\":%d,"
		"\"temperature\":%s,\"chat_template_kwargs\":{"
		"\"enable_thinking\":false}}",
		escaped_model, escaped_prompt,
		classification ? 16 : context->max_tokens,
		classification ? "0" : "0.2");
	if (body_length < 0 || (size_t)body_length >= sizeof(body)) {
		result->status = RAZION_AI_STATUS_INVALID_REQUEST;
		return result->status;
	}

	struct timeval start, end;
	gettimeofday(&start, NULL);
	char * response = NULL;
	int status = 0;
	if (http_request(context, "POST", "/v1/chat/completions", body,
		&response, &status)) {
		result->status = RAZION_AI_STATUS_PROVIDER_UNAVAILABLE;
		strncpy(result->output, "llama.cpp request failed or timed out.",
			sizeof(result->output) - 1);
		return result->status;
	}
	gettimeofday(&end, NULL);
	context->last_latency_ms = elapsed_ms(&start, &end);
	result->latency_ms = context->last_latency_ms;

	struct JSON_Value * root = json_parse(response);
	free(response);
	if (status != 200 || !root) {
		if (root) json_free(root);
		result->status = RAZION_AI_STATUS_PROVIDER_UNAVAILABLE;
		strncpy(result->output, "llama.cpp returned an unsuccessful response.",
			sizeof(result->output) - 1);
		return result->status;
	}

	struct JSON_Value * choices = object_value(root, "choices", JSON_TYPE_ARRAY);
	struct JSON_Value * choice = choices ? JSON_IND(choices, 0) : NULL;
	struct JSON_Value * message = object_value(choice, "message", JSON_TYPE_OBJECT);
	struct JSON_Value * output = object_value(message, "content", JSON_TYPE_STRING);
	if (!output || !output->string[0]) {
		struct JSON_Value * error = object_value(root, "error", JSON_TYPE_OBJECT);
		struct JSON_Value * error_message =
			object_value(error, "message", JSON_TYPE_STRING);
		result->status = RAZION_AI_STATUS_PROVIDER_UNAVAILABLE;
		strncpy(result->output,
			error_message ? error_message->string :
			"llama.cpp returned no generated text.",
			sizeof(result->output) - 1);
		json_free(root);
		return result->status;
	}

	result->status = RAZION_AI_STATUS_OK;
	result->granted_capabilities =
		request->requested_capabilities & (
			RAZION_AI_CAP_TEXT | RAZION_AI_CAP_TASK_PROPOSE |
			RAZION_AI_CAP_CODE);
	strncpy(result->output, output->string, sizeof(result->output) - 1);
	json_free(root);
	return result->status;
}

static int llama_cpp_chat(
	void * opaque,
	const razion_ai_provider_request_t * request,
	razion_ai_provider_result_t * result) {
	return llama_cpp_generate(opaque, request, result);
}

static int llama_cpp_summarize(
	void * opaque,
	const razion_ai_provider_request_t * request,
	razion_ai_provider_result_t * result) {
	return llama_cpp_generate(opaque, request, result);
}

static int unsupported_result(
	void * opaque,
	const razion_ai_provider_request_t * request,
	razion_ai_provider_result_t * result) {
	(void)opaque;
	(void)request;
	memset(result, 0, sizeof(*result));
	result->status = RAZION_AI_STATUS_UNSUPPORTED;
	strncpy(result->output, "This llama.cpp adapter capability is not implemented.",
		sizeof(result->output) - 1);
	return result->status;
}

static int unsupported_stream(
	void * opaque,
	const razion_ai_provider_request_t * request,
	razion_ai_provider_stream_callback_t callback,
	void * user_data) {
	(void)opaque;
	(void)request;
	(void)callback;
	(void)user_data;
	return RAZION_AI_STATUS_UNSUPPORTED;
}

static int unsupported_cancel(void * opaque, uint32_t request_id) {
	(void)opaque;
	(void)request_id;
	return RAZION_AI_STATUS_UNSUPPORTED;
}

static uint32_t llama_cpp_capabilities(void * opaque) {
	(void)opaque;
	return RAZION_AI_CAP_TEXT | RAZION_AI_CAP_TASK_PROPOSE |
		RAZION_AI_CAP_CODE;
}

static const razion_ai_provider_interface_t provider_interface = {
	.initialize = llama_cpp_initialize,
	.shutdown = llama_cpp_shutdown,
	.available = llama_cpp_available,
	.health = llama_cpp_health,
	.latency = llama_cpp_latency,
	.chat = llama_cpp_chat,
	.stream = unsupported_stream,
	.summarize = llama_cpp_summarize,
	.embeddings = unsupported_result,
	.vision = unsupported_result,
	.speech = unsupported_result,
	.cancel = unsupported_cancel,
	.capabilities = llama_cpp_capabilities,
};

static void set_response(
	razion_ai_response_t * response,
	razion_ai_request_t * request,
	razion_ai_status_t status,
	const char * message) {

	memset(response, 0, sizeof(*response));
	response->magic = RAZION_AI_PROTOCOL_MAGIC;
	response->version = RAZION_AI_PROTOCOL_VERSION;
	response->status = status;
	response->request_id = request ? request->request_id : 0;
	strncpy(response->provider, "llama.cpp", sizeof(response->provider) - 1);
	if (message) {
		response->payload_length = strlen(message);
		if (response->payload_length >= sizeof(response->payload)) {
			response->payload_length = sizeof(response->payload) - 1;
		}
		memcpy(response->payload, message, response->payload_length);
	}
}

static int validate_request(pex_packet_t * packet, razion_ai_request_t ** out) {
	if (packet->size < offsetof(razion_ai_request_t, payload)) return 0;
	razion_ai_request_t * request = (razion_ai_request_t *)packet->data;
	if (request->magic != RAZION_AI_PROTOCOL_MAGIC ||
		request->version != RAZION_AI_PROTOCOL_VERSION ||
		request->payload_length >= RAZION_AI_MAX_PAYLOAD ||
		offsetof(razion_ai_request_t, payload) + request->payload_length >
			packet->size) {
		return 0;
	}
	request->payload[request->payload_length] = '\0';
	*out = request;
	return 1;
}

static void handle_request(
	razion_ai_request_t * request,
	razion_ai_response_t * response) {

	if (request->operation == RAZION_AI_OP_PROVIDER_HEALTH) {
		char detail[RAZION_AI_PROVIDER_MAX_DETAIL];
		if (provider_interface.health(&llama_cpp, detail, sizeof(detail))) {
			set_response(response, request,
				RAZION_AI_STATUS_PROVIDER_UNAVAILABLE, detail);
		} else {
			set_response(response, request, RAZION_AI_STATUS_OK, detail);
			response->granted_capabilities =
				provider_interface.capabilities(&llama_cpp);
		}
		return;
	}

	if (request->operation == RAZION_AI_OP_PROVIDER_CAPABILITIES) {
		set_response(response, request, RAZION_AI_STATUS_OK,
			"text,chat,summarize,code,task-propose;"
			"unsupported=stream,embeddings,vision,speech,cancel");
		response->granted_capabilities =
			provider_interface.capabilities(&llama_cpp);
		return;
	}

	razion_ai_provider_request_t provider_request = {
		.operation = request->operation,
		.request_id = request->request_id,
		.flags = request->flags,
		.requested_capabilities = request->requested_capabilities,
		.input = request->payload,
		.input_length = request->payload_length,
	};
	razion_ai_provider_result_t result;
	memset(&result, 0, sizeof(result));

	int status;
	if (request->operation == RAZION_AI_OP_SUMMARIZE) {
		status = provider_interface.summarize(
			&llama_cpp, &provider_request, &result);
	} else {
		status = provider_interface.chat(&llama_cpp, &provider_request, &result);
	}
	set_response(response, request, status, result.output);
	response->granted_capabilities = result.granted_capabilities;
}

static int run_service(void) {
	if (provider_interface.initialize(&llama_cpp, LLAMA_CPP_CONFIG)) {
		fprintf(stderr, "razion-ai-provider-llama-cpp: unsafe or invalid configuration\n");
		return 1;
	}

	FILE * endpoint = pex_bind(LLAMA_CPP_ENDPOINT);
	if (!endpoint) {
		fprintf(stderr, "razion-ai-provider-llama-cpp: unable to bind %s: %s\n",
			LLAMA_CPP_ENDPOINT, strerror(errno));
		provider_interface.shutdown(&llama_cpp);
		return 1;
	}

	while (1) {
		pex_packet_t * packet = calloc(1, PACKET_SIZE + 1);
		if (!packet) break;
		pex_listen(endpoint, packet);

		razion_ai_request_t * request = NULL;
		razion_ai_response_t response;
		if (!validate_request(packet, &request)) {
			set_response(&response, NULL, RAZION_AI_STATUS_PROTOCOL_ERROR,
				"Invalid Razion AI provider request.");
		} else {
			handle_request(request, &response);
		}

		size_t response_size =
			offsetof(razion_ai_response_t, payload) + response.payload_length;
		pex_send(endpoint, packet->source, response_size, (char *)&response);
		free(packet);
	}

	provider_interface.shutdown(&llama_cpp);
	return 1;
}

static int usage(const char * argv0) {
	fprintf(stderr,
		"Razion AI llama.cpp provider\n"
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
		fprintf(stderr,
			"razion-ai-provider-llama-cpp: only root may run the system service\n");
		return 1;
	}
	if (daemonize && fork()) return 0;
	return run_service();
}
