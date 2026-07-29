/**
 * @brief Diagnostic client for the Razion AI Engine.
 *
 * This utility reports service and provider state. It is intentionally not a
 * conversational interface.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <toaru/razion_ai.h>

static int usage(const char * argv0) {
	fprintf(stderr,
		"Razion AI Engine status utility\n"
		"usage: %s [health|providers]\n",
		argv0);
	return 1;
}

int main(int argc, char * argv[]) {
	if (argc > 2) return usage(argv[0]);

	razion_ai_context_t context;
	razion_ai_response_t response;
	if (razion_ai_init(&context, "razion-ai-status")) {
		fprintf(stderr, "%s: unable to initialize SDK: %s\n", argv[0], strerror(errno));
		return 1;
	}

	const char * command = argc == 2 ? argv[1] : "health";
	int result;
	if (!strcmp(command, "health")) {
		result = razion_ai_health(&context, &response);
	} else if (!strcmp(command, "providers")) {
		result = razion_ai_list_providers(&context, &response);
	} else {
		return usage(argv[0]);
	}

	if (result) {
		fprintf(stderr, "%s: Razion AI Engine is unavailable: %s\n",
			argv[0], strerror(errno));
		return 1;
	}

	printf("%s\n", response.payload);
	if (response.status != RAZION_AI_STATUS_OK) {
		fprintf(stderr, "status: %s\n",
			razion_ai_status_string(response.status));
		return 2;
	}
	return 0;
}
