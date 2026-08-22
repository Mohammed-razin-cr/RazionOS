/**
 * @brief Control the compositor-owned RazionOS screen recorder.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <toaru/yutani.h>

int main(int argc, char ** argv) {
	yutani_t * yctx = yutani_init();
	if (!yctx) return 1;
	if (argc != 2) {
		fprintf(stderr, "usage: razion-recorder start|stop|status\n");
		return 2;
	}
	uint32_t action;
	if (!strcmp(argv[1], "start")) action = YUTANI_RECORDING_START;
	else if (!strcmp(argv[1], "stop")) action = YUTANI_RECORDING_STOP;
	else if (!strcmp(argv[1], "status")) action = YUTANI_RECORDING_QUERY;
	else {
		fprintf(stderr, "usage: razion-recorder start|stop|status\n");
		return 2;
	}
	yutani_recording_request(yctx, action);
	if (action != YUTANI_RECORDING_QUERY) return 0;
	yutani_msg_t * message = yutani_wait_for(yctx, YUTANI_MSG_RECORDING_STATUS);
	if (!message) return 1;
	struct yutani_msg_recording * status = (void *)message->data;
	if (status->active) printf("Recording: %u frames (%s)\n", status->frames, status->path);
	else if (status->path[0]) printf("Stopped: %s\n", status->path);
	else puts("Stopped");
	free(message);
	return 0;
}
