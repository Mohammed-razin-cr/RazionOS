/**
 * @brief yutani-screenshot - make the compositor take a screenshot
 *
 * This injects a screenshot key press into the compositor.
 * This only works because the compositor accepts key events
 * from applications as equivalent to local key presses, due
 * to a holdover from a legacy design. This is subject to
 * change at some point in the future.
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2026 K. Lange
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <toaru/yutani.h>
#include <toaru/yutani-internal.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>

int show_usage(int argc, char * argv[]) {
#define X_S "\033[3m"
#define X_E "\033[0m"
	fprintf(stderr,
			"%s - make the compositor take a screenshot\n"
			"\n"
			"usage: %s [-w | -s | -r X,Y,W,H]\n"
			"\n"
			" -w  --window " X_S "take a screenshot of the focused window" X_E "\n"
			" -s  --select " X_S "interactively select a display region" X_E "\n"
			" -r  --region " X_S "capture an exact X,Y,W,H display region" X_E "\n"
			"     --help   " X_S "show this help text" X_E "\n"
			"\n", argv[0], argv[0]);
	return 1;
}

static void normalize_rectangle(int x0, int y0, int x1, int y1,
	int * x, int * y, unsigned int * width, unsigned int * height) {
	*x = x0 < x1 ? x0 : x1;
	*y = y0 < y1 ? y0 : y1;
	*width = abs(x1 - x0) + 1;
	*height = abs(y1 - y0) + 1;
}

static void draw_selector(gfx_context_t * ctx, int dragging,
	int x0, int y0, int x1, int y1) {
	draw_fill(ctx, rgba(5, 9, 14, 150));
	if (dragging) {
		int x, y;
		unsigned int width, height;
		normalize_rectangle(x0, y0, x1, y1, &x, &y, &width, &height);
		draw_rectangle_solid(ctx, x, y, width, height, rgba(74, 167, 252, 45));
		draw_rectangle(ctx, x, y, width, height, rgba(113, 184, 255, 255));
		if (width > 2 && height > 2) {
			draw_rectangle(ctx, x + 1, y + 1, width - 2, height - 2,
				rgba(8, 18, 28, 230));
		}
	}
	flip(ctx);
}

static int select_region(yutani_t * yctx) {
	yutani_window_t * window = yutani_window_create_flags(yctx,
		yctx->display_width, yctx->display_height,
		YUTANI_WINDOW_FLAG_DISALLOW_DRAG |
		YUTANI_WINDOW_FLAG_DISALLOW_RESIZE |
		YUTANI_WINDOW_FLAG_NO_ANIMATION);
	if (!window) return 1;
	yutani_window_move(yctx, window, 0, 0);
	yutani_set_stack(yctx, window, YUTANI_ZORDER_OVERLAY);
	yutani_window_advertise(yctx, window, "Select screenshot region");

	gfx_context_t * ctx = init_graphics_yutani_double_buffer(window);
	int dragging = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	draw_selector(ctx, dragging, x0, y0, x1, y1);
	yutani_flip(yctx, window);

	int running = 1;
	while (running) {
		yutani_msg_t * message = yutani_poll(yctx);
		if (!message) continue;
		switch (message->type) {
			case YUTANI_MSG_KEY_EVENT: {
				struct yutani_msg_key_event * key = (void *)message->data;
				if (key->wid == window->wid && key->event.action == KEY_ACTION_DOWN &&
					key->event.keycode == KEY_ESCAPE) running = 0;
				break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)message->data;
				if (mouse->wid != window->wid) break;
				if (mouse->command == YUTANI_MOUSE_EVENT_DOWN) {
					if (!dragging) {
						dragging = 1;
						x0 = x1 = mouse->new_x;
						y0 = y1 = mouse->new_y;
					} else {
						x1 = mouse->new_x;
						y1 = mouse->new_y;
					}
					draw_selector(ctx, dragging, x0, y0, x1, y1);
					yutani_flip(yctx, window);
				} else if (dragging && (mouse->command == YUTANI_MOUSE_EVENT_DRAG ||
					mouse->command == YUTANI_MOUSE_EVENT_MOVE)) {
					x1 = mouse->new_x;
					y1 = mouse->new_y;
					draw_selector(ctx, dragging, x0, y0, x1, y1);
					yutani_flip(yctx, window);
				} else if (dragging && (mouse->command == YUTANI_MOUSE_EVENT_RAISE ||
					mouse->command == YUTANI_MOUSE_EVENT_CLICK)) {
					x1 = mouse->new_x;
					y1 = mouse->new_y;
					int x, y;
					unsigned int width, height;
					normalize_rectangle(x0, y0, x1, y1, &x, &y, &width, &height);
					if (width < 8 || height < 8) {
						draw_selector(ctx, dragging, x0, y0, x1, y1);
						yutani_flip(yctx, window);
						break;
					}
					yutani_close(yctx, window);
					window = NULL;
					usleep(50000);
					yutani_screenshot_region(yctx, x, y, width, height);
					running = 0;
				}
				break;
			}
			case YUTANI_MSG_WINDOW_CLOSE:
			case YUTANI_MSG_SESSION_END:
				running = 0;
				break;
		}
		free(message);
	}
	if (window) yutani_close(yctx, window);
	release_graphics_yutani(ctx);
	return 0;
}

int main(int argc, char * argv[]) {
	int of_window = 0;
	int select = 0;
	int region_x = 0, region_y = 0;
	unsigned int region_width = 0, region_height = 0;
	int opt;

	struct option long_opts[] = {
		{"window",no_argument,0,'w'},
		{"select",no_argument,0,'s'},
		{"region",required_argument,0,'r'},
		{"help",no_argument,0,'?'},
		{0,0,0,0},
	};

	while ((opt = getopt_long(argc, argv, "wsr:", long_opts, NULL)) != -1) {
		switch (opt) {
			case 'w':
				of_window = 1;
				break;
			case 's':
				select = 1;
				break;
			case 'r': {
				char trailing;
				if (sscanf(optarg, "%d,%d,%u,%u%c", &region_x, &region_y,
					&region_width, &region_height, &trailing) != 4) {
					fprintf(stderr, "%s: invalid region '%s'\n", argv[0], optarg);
					return 1;
				}
				break;
			}
			case '?':
				return show_usage(argc,argv);
		}
	}

	yutani_t * yctx = yutani_init();
	if (!yctx) {
		fprintf(stderr, "%s: not connected\n", argv[0]);
		return 1;
	}
	if (select) return select_region(yctx);
	if (region_width && region_height) {
		yutani_screenshot_region(yctx, region_x, region_y, region_width, region_height);
		return 0;
	}

	key_event_t event = {KEY_PRINT_SCREEN, of_window ? KEY_MOD_LEFT_SHIFT : 0, KEY_ACTION_DOWN, 0};
	key_event_state_t state = {0};

	yutani_msg_buildx_key_event_alloc(response);
	yutani_msg_buildx_key_event(response, 0, &event, &state);
	yutani_msg_send(yctx, response);

	return 0;
}
