/**
 * @brief Razion Pulse - safe operating-system command interface.
 *
 * Pulse is a one-shot intent interface, not a chatbot. It only executes
 * allowlisted native actions from the Razion Pulse action broker.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <toaru/razion_ai.h>
#include <toaru/razion_pulse.h>

#define REQUEST_MAX 512
#define RESULT_MAX 512

static int usage(const char * argv0) {
	fprintf(stderr,
		"Razion Pulse - operating-system command interface\n"
		"usage: %s [--plan] [--confirm] [--json] [REQUEST]\n"
		"\n"
		"  --plan     show the validated action without executing it\n"
		"  --confirm  explicitly approve a change action\n"
		"  --json     emit a machine-readable proposal\n"
		"\n"
		"Examples:\n"
		"  %s \"Open Downloads\"\n"
		"  %s \"Check memory usage\"\n"
		"  %s --plan \"Create a Python project\"\n"
		"  %s --confirm \"Create a Python project\"\n",
		argv0, argv0, argv0, argv0, argv0);
	return 1;
}

static void json_string(const char * input) {
	putchar('"');
	for (const unsigned char * c = (const unsigned char *)input; *c; ++c) {
		switch (*c) {
			case '"': fputs("\\\"", stdout); break;
			case '\\': fputs("\\\\", stdout); break;
			case '\n': fputs("\\n", stdout); break;
			case '\r': fputs("\\r", stdout); break;
			case '\t': fputs("\\t", stdout); break;
			default:
				if (*c >= 0x20) putchar(*c);
				break;
		}
	}
	putchar('"');
}

static void print_proposal(const razion_pulse_proposal_t * proposal, int json) {
	if (json) {
		fputs("{\"version\":1,\"action\":", stdout);
		json_string(proposal->action_id);
		fputs(",\"summary\":", stdout);
		json_string(proposal->summary);
		fputs(",\"target\":", stdout);
		json_string(proposal->target);
		printf(",\"risk\":\"%s\",\"capabilities\":%u,"
			"\"confirmation_required\":%s,\"executable\":%s}\n",
			razion_pulse_risk_string(proposal->risk),
			proposal->required_capabilities,
			proposal->requires_confirmation ? "true" : "false",
			proposal->executable ? "true" : "false");
		return;
	}

	printf("Razion Pulse proposal\n"
		"  Action: %s\n"
		"  Target: %s\n"
		"  Risk: %s\n"
		"  Confirmation: %s\n"
		"  Native broker: %s\n",
		proposal->summary,
		proposal->target,
		razion_pulse_risk_string(proposal->risk),
		proposal->requires_confirmation ? "required" : "not required",
		proposal->executable ? "available" : "not available in this phase");
}

static razion_pulse_status_t local_ai_fallback(
	const char * request,
	razion_pulse_proposal_t * proposal) {

	razion_ai_context_t ai;
	razion_ai_response_t response;
	if (razion_ai_init(&ai, "razion-pulse")) return RAZION_PULSE_STATUS_UNKNOWN;
	ai.default_flags |= RAZION_AI_FLAG_LOCAL_ONLY;
	if (razion_ai_classify_intent(&ai, request, &response)) {
		return RAZION_PULSE_STATUS_UNKNOWN;
	}
	if (response.status != RAZION_AI_STATUS_OK) {
		return RAZION_PULSE_STATUS_UNKNOWN;
	}
	return razion_pulse_propose_action(response.payload, proposal);
}

int main(int argc, char * argv[]) {
	int plan_only = 0;
	int confirmed = 0;
	int json = 0;
	char request[REQUEST_MAX] = {0};
	size_t request_length = 0;

	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			return usage(argv[0]);
		} else if (!strcmp(argv[i], "--plan")) {
			plan_only = 1;
		} else if (!strcmp(argv[i], "--confirm")) {
			confirmed = 1;
		} else if (!strcmp(argv[i], "--json")) {
			json = 1;
			plan_only = 1;
		} else {
			size_t argument_length = strlen(argv[i]);
			if (request_length + argument_length + (request_length ? 1 : 0) >=
				sizeof(request)) {
				fprintf(stderr, "%s: request is too long\n", argv[0]);
				return 1;
			}
			if (request_length) request[request_length++] = ' ';
			memcpy(request + request_length, argv[i], argument_length);
			request_length += argument_length;
			request[request_length] = '\0';
		}
	}
	if (!request_length) {
		printf("Razion Pulse\n"
			"Describe one operating-system action: ");
		fflush(stdout);
		if (!fgets(request, sizeof(request), stdin)) return 1;
		char * newline = strchr(request, '\n');
		if (newline) *newline = '\0';
		request_length = strlen(request);
		if (!request_length) return 1;
	}

	razion_pulse_proposal_t proposal;
	razion_pulse_status_t status = razion_pulse_propose(request, &proposal);
	if (status == RAZION_PULSE_STATUS_UNKNOWN) {
		status = local_ai_fallback(request, &proposal);
	}
	if (status != RAZION_PULSE_STATUS_OK) {
		fprintf(stderr,
			"Razion Pulse did not recognize a verified operating-system action.\n"
			"No action was performed.\n");
		return 2;
	}

	print_proposal(&proposal, json);
	if (plan_only) return 0;

	uint32_t capabilities =
		RAZION_PULSE_CAP_APP_LAUNCH |
		RAZION_PULSE_CAP_DIRECTORY_OPEN |
		RAZION_PULSE_CAP_SYSTEM_READ |
		RAZION_PULSE_CAP_FILE_CREATE;
	razion_pulse_context_t context;
	if (razion_pulse_init(&context, "razion-pulse", capabilities)) {
		fprintf(stderr, "%s: unable to initialize action broker\n", argv[0]);
		return 1;
	}

	char result[RESULT_MAX];
	status = razion_pulse_execute(
		&context, &proposal, confirmed, result, sizeof(result));
	printf("Result: %s\n", result);
	if (status == RAZION_PULSE_STATUS_CONFIRMATION_REQUIRED) {
		printf("Review the target, then repeat with --confirm to approve it.\n");
	}
	return status == RAZION_PULSE_STATUS_OK ? 0 : 3;
}
