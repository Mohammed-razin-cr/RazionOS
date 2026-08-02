#pragma once

#include <_cheader.h>
#include <stdint.h>

#include <toaru/razion_ai.h>

_Begin_C_Header

#define RAZION_AI_PROVIDER_PROTOCOL_VERSION 1
#define RAZION_AI_MAX_PROVIDER_ENDPOINT 80
#define RAZION_AI_PROVIDER_MAX_DETAIL 192

/**
 * Provider adapters are isolated services, not libraries loaded into the
 * privileged engine. Each adapter binds its configured PEX endpoint and
 * accepts the same versioned request and response structures used by the SDK.
 */
typedef struct {
	char name[RAZION_AI_MAX_PROVIDER_NAME];
	char endpoint[RAZION_AI_MAX_PROVIDER_ENDPOINT];
	razion_ai_provider_class_t provider_class;
	uint32_t capabilities;
	int priority;
	int enabled;
} razion_ai_provider_descriptor_t;

typedef struct {
	razion_ai_operation_t operation;
	uint32_t request_id;
	uint32_t flags;
	uint32_t requested_capabilities;
	const char * input;
	size_t input_length;
} razion_ai_provider_request_t;

typedef struct {
	razion_ai_status_t status;
	uint32_t granted_capabilities;
	uint32_t latency_ms;
	char output[RAZION_AI_MAX_PAYLOAD];
} razion_ai_provider_result_t;

typedef int (*razion_ai_provider_stream_callback_t)(
	const char * data,
	size_t length,
	void * user_data);

/**
 * Common in-process interface implemented by every isolated provider adapter.
 * Provider-specific code remains behind this table; the engine communicates
 * with adapter services only through the versioned PEX protocol.
 */
typedef struct {
	int (*initialize)(void * context, const char * config_path);
	void (*shutdown)(void * context);
	int (*available)(void * context);
	int (*health)(
		void * context,
		char * detail,
		size_t detail_size);
	uint32_t (*latency)(void * context);
	int (*chat)(
		void * context,
		const razion_ai_provider_request_t * request,
		razion_ai_provider_result_t * result);
	int (*stream)(
		void * context,
		const razion_ai_provider_request_t * request,
		razion_ai_provider_stream_callback_t callback,
		void * user_data);
	int (*summarize)(
		void * context,
		const razion_ai_provider_request_t * request,
		razion_ai_provider_result_t * result);
	int (*embeddings)(
		void * context,
		const razion_ai_provider_request_t * request,
		razion_ai_provider_result_t * result);
	int (*vision)(
		void * context,
		const razion_ai_provider_request_t * request,
		razion_ai_provider_result_t * result);
	int (*speech)(
		void * context,
		const razion_ai_provider_request_t * request,
		razion_ai_provider_result_t * result);
	int (*cancel)(void * context, uint32_t request_id);
	uint32_t (*capabilities)(void * context);
} razion_ai_provider_interface_t;

_End_C_Header
