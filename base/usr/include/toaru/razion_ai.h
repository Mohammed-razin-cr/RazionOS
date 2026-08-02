#pragma once

#include <_cheader.h>
#include <stddef.h>
#include <stdint.h>

_Begin_C_Header

#define RAZION_AI_ENDPOINT "razion-ai"
#define RAZION_AI_PROTOCOL_MAGIC 0x52414931U
#define RAZION_AI_PROTOCOL_VERSION 1
#define RAZION_AI_MAX_APP_ID 48
#define RAZION_AI_MAX_PROVIDER_NAME 32
#define RAZION_AI_MAX_PAYLOAD 768

typedef enum {
	RAZION_AI_OP_HEALTH = 1,
	RAZION_AI_OP_LIST_PROVIDERS,
	RAZION_AI_OP_ASK,
	RAZION_AI_OP_SEARCH,
	RAZION_AI_OP_ANALYZE,
	RAZION_AI_OP_RUN_TASK,
	RAZION_AI_OP_SUMMARIZE,
	RAZION_AI_OP_GENERATE_CODE,
	RAZION_AI_OP_EXPLAIN_ERROR,
	RAZION_AI_OP_SEARCH_FILES,
	RAZION_AI_OP_ANALYZE_LOGS,
	RAZION_AI_OP_PROCESS_INFO,
	RAZION_AI_OP_DISK_INFO,
	RAZION_AI_OP_NETWORK_DIAGNOSTICS,
	RAZION_AI_OP_SYSTEM_HEALTH,
	RAZION_AI_OP_APPLICATION_SEARCH,
	RAZION_AI_OP_VOICE_COMMAND,
	RAZION_AI_OP_DOCUMENT_SEARCH,
	RAZION_AI_OP_SETTINGS_REQUEST,
	RAZION_AI_OP_CLASSIFY_INTENT,
	RAZION_AI_OP_PROVIDER_HEALTH,
	RAZION_AI_OP_PROVIDER_CAPABILITIES,
} razion_ai_operation_t;

typedef enum {
	RAZION_AI_STATUS_OK = 0,
	RAZION_AI_STATUS_INVALID_REQUEST,
	RAZION_AI_STATUS_PROTOCOL_ERROR,
	RAZION_AI_STATUS_NO_PROVIDER,
	RAZION_AI_STATUS_PROVIDER_UNAVAILABLE,
	RAZION_AI_STATUS_PERMISSION_DENIED,
	RAZION_AI_STATUS_CONFIRMATION_REQUIRED,
	RAZION_AI_STATUS_UNSUPPORTED,
	RAZION_AI_STATUS_INTERNAL_ERROR,
} razion_ai_status_t;

typedef enum {
	RAZION_AI_PROVIDER_LOCAL = 1,
	RAZION_AI_PROVIDER_CLOUD,
} razion_ai_provider_class_t;

enum {
	RAZION_AI_CAP_TEXT          = 1 << 0,
	RAZION_AI_CAP_FILE_SEARCH   = 1 << 1,
	RAZION_AI_CAP_SYSTEM_READ   = 1 << 2,
	RAZION_AI_CAP_TASK_PROPOSE  = 1 << 3,
	RAZION_AI_CAP_TASK_EXECUTE  = 1 << 4,
	RAZION_AI_CAP_VOICE         = 1 << 5,
	RAZION_AI_CAP_CODE          = 1 << 6,
	RAZION_AI_CAP_SETTINGS      = 1 << 7,
};

enum {
	RAZION_AI_FLAG_PREFER_LOCAL   = 1 << 0,
	RAZION_AI_FLAG_LOCAL_ONLY     = 1 << 1,
	RAZION_AI_FLAG_ALLOW_CLOUD    = 1 << 2,
	RAZION_AI_FLAG_EXECUTE_ACTION = 1 << 3,
	RAZION_AI_FLAG_USER_CONFIRMED = 1 << 4,
	RAZION_AI_FLAG_BYPASS_CACHE   = 1 << 5,
};

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t operation;
	uint32_t request_id;
	uint32_t flags;
	uint32_t requested_capabilities;
	uint32_t payload_length;
	char app_id[RAZION_AI_MAX_APP_ID];
	char payload[RAZION_AI_MAX_PAYLOAD];
} razion_ai_request_t;

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t status;
	uint32_t request_id;
	uint32_t granted_capabilities;
	uint32_t payload_length;
	char provider[RAZION_AI_MAX_PROVIDER_NAME];
	char payload[RAZION_AI_MAX_PAYLOAD];
} razion_ai_response_t;

typedef struct {
	char app_id[RAZION_AI_MAX_APP_ID];
	uint32_t default_flags;
	uint32_t requested_capabilities;
	uint32_t next_request_id;
	int timeout_ms;
} razion_ai_context_t;

/**
 * An in-flight request created by razion_ai_request_begin(). The transport is
 * intentionally opaque so applications can keep AI work outside their UI
 * event loop without depending on PEX internals.
 */
typedef struct {
	void * transport;
	uint32_t request_id;
	int timeout_ms;
	int active;
} razion_ai_async_request_t;

typedef int (*razion_ai_stream_callback_t)(
	const char * data,
	size_t length,
	void * user_data);

/**
 * Initialize an application context. Application identifiers are included in
 * audit metadata and should be stable, short names such as "file-browser".
 */
extern int razion_ai_init(razion_ai_context_t * context, const char * app_id);

/**
 * Submit a request to the Razion AI Engine.
 *
 * Returns zero when a protocol response was received. Provider and policy
 * failures are reported through response->status. A negative result indicates
 * that the engine endpoint could not be reached or the reply was malformed.
 */
extern int razion_ai_request(
	razion_ai_context_t * context,
	razion_ai_operation_t operation,
	const char * input,
	uint32_t flags,
	razion_ai_response_t * response);

/**
 * Start an AI request without waiting for its response. Call
 * razion_ai_request_poll() from the application's existing event loop.
 */
extern int razion_ai_request_begin(
	razion_ai_context_t * context,
	razion_ai_operation_t operation,
	const char * input,
	uint32_t flags,
	razion_ai_async_request_t * request);

/**
 * Poll an asynchronous request. Returns 1 when a response is ready, 0 while
 * it is pending, and -1 on a transport or protocol error.
 */
extern int razion_ai_request_poll(
	razion_ai_async_request_t * request,
	int timeout_ms,
	razion_ai_response_t * response);

/** Cancel and release a pending request transport. */
extern void razion_ai_request_cancel(razion_ai_async_request_t * request);

/**
 * Deliver a completed response through a callback. Provider adapters may
 * later emit multiple chunks without changing application call sites.
 */
extern int razion_ai_request_stream(
	razion_ai_context_t * context,
	razion_ai_operation_t operation,
	const char * input,
	uint32_t flags,
	razion_ai_stream_callback_t callback,
	void * user_data,
	razion_ai_response_t * response);

extern int razion_ai_ask(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response);
extern int razion_ai_search(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response);
extern int razion_ai_analyze(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response);
extern int razion_ai_run_task(
	razion_ai_context_t * context,
	const char * input,
	uint32_t flags,
	razion_ai_response_t * response);
extern int razion_ai_summarize(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response);
extern int razion_ai_generate_code(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response);
extern int razion_ai_explain_error(
	razion_ai_context_t * context,
	const char * input,
	razion_ai_response_t * response);
extern int razion_ai_search_files(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response);
extern int razion_ai_analyze_logs(
	razion_ai_context_t * context,
	const char * scope,
	razion_ai_response_t * response);
extern int razion_ai_process_info(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response);
extern int razion_ai_disk_info(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response);
extern int razion_ai_network_diagnostics(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response);
extern int razion_ai_system_health(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response);
extern int razion_ai_application_search(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response);
extern int razion_ai_voice_command(
	razion_ai_context_t * context,
	const char * transcript,
	razion_ai_response_t * response);
extern int razion_ai_document_search(
	razion_ai_context_t * context,
	const char * query,
	razion_ai_response_t * response);
extern int razion_ai_settings_request(
	razion_ai_context_t * context,
	const char * request,
	razion_ai_response_t * response);
extern int razion_ai_classify_intent(
	razion_ai_context_t * context,
	const char * request,
	razion_ai_response_t * response);
extern int razion_ai_health(
	razion_ai_context_t * context,
	razion_ai_response_t * response);
extern int razion_ai_list_providers(
	razion_ai_context_t * context,
	razion_ai_response_t * response);
extern const char * razion_ai_status_string(razion_ai_status_t status);

_End_C_Header
