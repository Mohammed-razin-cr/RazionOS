#pragma once

#include <_cheader.h>
#include <stdint.h>

#include <toaru/razion_ai.h>

_Begin_C_Header

#define RAZION_AI_PROVIDER_PROTOCOL_VERSION 1
#define RAZION_AI_MAX_PROVIDER_ENDPOINT 80

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

_End_C_Header
