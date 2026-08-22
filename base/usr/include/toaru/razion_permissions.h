/**
 * @brief Authenticated RazionOS permission-broker protocol and client API.
 */
#pragma once

#include <stdint.h>

#include <_cheader.h>

_Begin_C_Header

#define RAZION_PERMISSION_MAGIC 0x52504D31U
#define RAZION_PERMISSION_VERSION 1U
#define RAZION_PERMISSION_ENDPOINT "razion-permission-broker"
#define RAZION_PERMISSION_RESOURCE_MAX 256
#define RAZION_PERMISSION_DETAIL_MAX 192

typedef uint64_t razion_capability_t;

enum {
	RAZION_CAP_FILES_USER_READ   = 1ULL << 0,
	RAZION_CAP_FILES_USER_WRITE  = 1ULL << 1,
	RAZION_CAP_NETWORK           = 1ULL << 2,
	RAZION_CAP_AUDIO_CAPTURE     = 1ULL << 3,
	RAZION_CAP_CAMERA            = 1ULL << 4,
	RAZION_CAP_SETTINGS_READ     = 1ULL << 5,
	RAZION_CAP_SETTINGS_WRITE    = 1ULL << 6,
	RAZION_CAP_PACKAGE_QUERY     = 1ULL << 7,
	RAZION_CAP_PACKAGE_INSTALL   = 1ULL << 8,
	RAZION_CAP_POWER             = 1ULL << 9,
	RAZION_CAP_SYSTEM_READ       = 1ULL << 10,
	RAZION_CAP_AI                = 1ULL << 11,
};

typedef enum {
	RAZION_PERMISSION_CHECK = 1,
} razion_permission_operation_t;

typedef enum {
	RAZION_PERMISSION_GRANTED = 0,
	RAZION_PERMISSION_DENIED,
	RAZION_PERMISSION_INVALID,
	RAZION_PERMISSION_UNAVAILABLE,
} razion_permission_status_t;

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t operation;
	uint32_t request_id;
	razion_capability_t capability;
	char resource[RAZION_PERMISSION_RESOURCE_MAX];
} razion_permission_request_t;

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t status;
	uint32_t request_id;
	razion_capability_t capability;
	char detail[RAZION_PERMISSION_DETAIL_MAX];
} razion_permission_response_t;

extern int razion_permission_check(
	razion_capability_t capability,
	const char * resource,
	razion_permission_response_t * response);
extern const char * razion_permission_status_string(
	razion_permission_status_t status);
extern int razion_permission_policy_allows(
	const char * policy_path,
	const char * executable,
	razion_capability_t capability);

_End_C_Header
