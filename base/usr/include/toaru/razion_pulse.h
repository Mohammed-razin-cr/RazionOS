#pragma once

#include <_cheader.h>
#include <stddef.h>
#include <stdint.h>

_Begin_C_Header

#define RAZION_PULSE_PROTOCOL_VERSION 1
#define RAZION_PULSE_MAX_APP_ID 48
#define RAZION_PULSE_MAX_ACTION_ID 40
#define RAZION_PULSE_MAX_TARGET 256
#define RAZION_PULSE_MAX_SUMMARY 192

typedef enum {
	RAZION_PULSE_ACTION_UNKNOWN = 0,
	RAZION_PULSE_ACTION_OPEN_TERMINAL,
	RAZION_PULSE_ACTION_OPEN_CALCULATOR,
	RAZION_PULSE_ACTION_OPEN_FILE_BROWSER,
	RAZION_PULSE_ACTION_OPEN_SYSTEM_MONITOR,
	RAZION_PULSE_ACTION_OPEN_WALLPAPER_SETTINGS,
	RAZION_PULSE_ACTION_OPEN_HOME,
	RAZION_PULSE_ACTION_OPEN_DOWNLOADS,
	RAZION_PULSE_ACTION_CHECK_MEMORY,
	RAZION_PULSE_ACTION_CREATE_PYTHON_PROJECT,
	RAZION_PULSE_ACTION_FIND_RECENT_PDF,
	RAZION_PULSE_ACTION_RESTART_NETWORKING,
	RAZION_PULSE_ACTION_INSTALL_NODEJS,
	RAZION_PULSE_ACTION_SUMMARIZE_TODAY,
	RAZION_PULSE_ACTION_OPEN_SETTINGS,
	RAZION_PULSE_ACTION_COUNT,
} razion_pulse_action_t;

typedef enum {
	RAZION_PULSE_RISK_READ = 0,
	RAZION_PULSE_RISK_CHANGE,
	RAZION_PULSE_RISK_PRIVILEGED,
	RAZION_PULSE_RISK_DESTRUCTIVE,
} razion_pulse_risk_t;

typedef enum {
	RAZION_PULSE_STATUS_OK = 0,
	RAZION_PULSE_STATUS_UNKNOWN,
	RAZION_PULSE_STATUS_INVALID,
	RAZION_PULSE_STATUS_CAPABILITY_DENIED,
	RAZION_PULSE_STATUS_CONFIRMATION_REQUIRED,
	RAZION_PULSE_STATUS_UNAVAILABLE,
	RAZION_PULSE_STATUS_FAILED,
} razion_pulse_status_t;

enum {
	RAZION_PULSE_CAP_APP_LAUNCH     = 1 << 0,
	RAZION_PULSE_CAP_DIRECTORY_OPEN = 1 << 1,
	RAZION_PULSE_CAP_SYSTEM_READ    = 1 << 2,
	RAZION_PULSE_CAP_FILE_CREATE    = 1 << 3,
	RAZION_PULSE_CAP_FILE_SEARCH    = 1 << 4,
	RAZION_PULSE_CAP_PACKAGE_MANAGE = 1 << 5,
	RAZION_PULSE_CAP_NETWORK_ADMIN  = 1 << 6,
	RAZION_PULSE_CAP_ACTIVITY_READ  = 1 << 7,
};

typedef struct {
	char app_id[RAZION_PULSE_MAX_APP_ID];
	uint32_t granted_capabilities;
} razion_pulse_context_t;

typedef struct {
	uint16_t version;
	uint16_t action;
	uint16_t risk;
	uint16_t requires_confirmation;
	uint32_t required_capabilities;
	uint32_t executable;
	char action_id[RAZION_PULSE_MAX_ACTION_ID];
	char target[RAZION_PULSE_MAX_TARGET];
	char summary[RAZION_PULSE_MAX_SUMMARY];
} razion_pulse_proposal_t;

extern int razion_pulse_init(
	razion_pulse_context_t * context,
	const char * app_id,
	uint32_t granted_capabilities);

/**
 * Convert a request into a proposal using the built-in offline vocabulary.
 * The function never executes an action.
 */
extern razion_pulse_status_t razion_pulse_propose(
	const char * request,
	razion_pulse_proposal_t * proposal);

/**
 * Convert a provider-returned action identifier into a validated proposal.
 * Only identifiers in the built-in action vocabulary are accepted.
 */
extern razion_pulse_status_t razion_pulse_propose_action(
	const char * action_id,
	razion_pulse_proposal_t * proposal);

/**
 * Execute a validated native action. Proposal targets are descriptive only;
 * execution selects fixed native implementations from proposal->action.
 */
extern razion_pulse_status_t razion_pulse_execute(
	razion_pulse_context_t * context,
	const razion_pulse_proposal_t * proposal,
	int confirmed,
	char * result,
	size_t result_size);

extern const char * razion_pulse_risk_string(razion_pulse_risk_t risk);
extern const char * razion_pulse_status_string(razion_pulse_status_t status);

_End_C_Header
