/**
 * @brief Control and inspect compositor-owned RazionOS workspaces.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <toaru/yutani.h>

static int read_status(yutani_t * yctx) {
	yutani_workspace_query(yctx);
	yutani_msg_t * message = yutani_wait_for(yctx, YUTANI_MSG_WORKSPACE_STATUS);
	if (!message) return 1;
	struct yutani_msg_workspace * state = (void *)message->data;
	printf("Workspace %u of %u\n", state->workspace + 1, state->count);
	free(message);
	return 0;
}

int main(int argc, char ** argv) {
	yutani_t * yctx = yutani_init();
	if (!yctx) {
		fprintf(stderr, "razion-workspace: compositor is unavailable\n");
		return 1;
	}

	if (argc == 1 || !strcmp(argv[1], "status")) return read_status(yctx);
	if (argc == 3 && !strcmp(argv[1], "switch")) {
		errno = 0;
		char * end = NULL;
		unsigned long requested = strtoul(argv[2], &end, 10);
		if (errno || !end || *end || requested < 1 || requested > 4) {
			fprintf(stderr, "usage: razion-workspace [status | switch 1..4]\n");
			return 2;
		}
		yutani_workspace_switch(yctx, requested - 1);
		return 0;
	}

	fprintf(stderr, "usage: razion-workspace [status | switch 1..4]\n");
	return 2;
}
