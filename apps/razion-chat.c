/**
 * @brief Razion AI conversational client.
 *
 * This client talks only to the Razion AI Engine SDK. It never connects to a
 * provider endpoint or reads provider credentials.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <toaru/razion_ai.h>

#define INPUT_SIZE RAZION_AI_MAX_PAYLOAD

static int print_chunk(const char * data, size_t length, void * user_data) {
	(void)user_data;
	fwrite(data, 1, length, stdout);
	fflush(stdout);
	return 0;
}

static int ask(
	razion_ai_context_t * context,
	const char * prompt,
	uint32_t flags) {

	razion_ai_response_t response;
	printf("Razion AI: ");
	if (razion_ai_request_stream(context, RAZION_AI_OP_ASK, prompt, flags,
		print_chunk, NULL, &response)) {
		printf("unavailable\n");
		return 1;
	}
	if (response.status != RAZION_AI_STATUS_OK) {
		printf("%s", response.payload_length ? response.payload :
			razion_ai_status_string(response.status));
		printf("\n");
		return 1;
	}
	printf("\n\nProvider: %s\n", response.provider[0] ? response.provider : "engine");
	printf("AI-generated answer; verify important facts against project documentation.\n");
	return 0;
}

static int usage(const char * argv0) {
	fprintf(stderr,
		"Razion AI Chat\n"
		"usage: %s [--allow-cloud] [PROMPT]\n", argv0);
	return 1;
}

int main(int argc, char * argv[]) {
	uint32_t flags = RAZION_AI_FLAG_PREFER_LOCAL;
	char prompt[INPUT_SIZE] = {0};
	size_t used = 0;
	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			return usage(argv[0]);
		}
		if (!strcmp(argv[i], "--allow-cloud")) {
			flags |= RAZION_AI_FLAG_ALLOW_CLOUD;
			continue;
		}
		size_t length = strlen(argv[i]);
		if (used + length + (used ? 1 : 0) >= sizeof(prompt)) return usage(argv[0]);
		if (used) prompt[used++] = ' ';
		memcpy(prompt + used, argv[i], length);
		used += length;
		prompt[used] = '\0';
	}

	razion_ai_context_t context;
	if (razion_ai_init(&context, "razion-chat")) {
		fprintf(stderr, "razion-chat: AI Engine is unavailable\n");
		return 1;
	}
	context.timeout_ms = 30000;
	if (used) return ask(&context, prompt, flags);

	printf("Razion AI Chat\n"
		"Local providers are preferred. Type /quit to exit.\n\n");
	while (1) {
		printf("You: ");
		fflush(stdout);
		if (!fgets(prompt, sizeof(prompt), stdin)) break;
		char * newline = strchr(prompt, '\n');
		if (newline) *newline = '\0';
		if (!strcmp(prompt, "/quit") || !strcmp(prompt, "/exit")) break;
		if (!prompt[0]) continue;
		ask(&context, prompt, flags);
	}
	return 0;
}
