/**
 * @brief Razion Companion desktop surface and native control center.
 *
 * The overlay is transparent, shaped, event-driven, and entirely optional.
 * Artwork is rendered from original geometric silhouettes; no pet assets or
 * third-party runtime services are required.
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fswait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/pex.h>
#include <toaru/razion_companion.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define CONTROL_WIDTH 900
#define CONTROL_HEIGHT 620
#define OVERLAY_SIZE 230
#define TILE_COLUMNS 6
#define TILE_WIDTH 86
#define TILE_HEIGHT 58

#define C_BACKGROUND rgb(8,12,18)
#define C_PANEL rgb(15,22,31)
#define C_PANEL_HOVER rgb(23,34,48)
#define C_BORDER rgb(45,61,78)
#define C_BLUE rgb(77,162,255)
#define C_TEXT rgb(235,242,248)
#define C_MUTED rgb(145,162,179)
#define C_GREEN rgb(45,207,141)
#define C_ORANGE rgb(245,157,54)
#define C_PURPLE rgb(173,106,255)
#define C_RED rgb(239,91,91)

static const uint32_t color_choices[] = {
	0, 0x75A7FF, 0x5AD6B0, 0xF3A65A, 0xC58AFF, 0xF0809D, 0xE4D16A, 0xA5B0BD
};

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font;
static razion_companion_config_t config;
static razion_companion_state_t pet_state;
static char config_path[RAZION_COMPANION_PATH_MAX];
static int running = 1;
static int control_mode = 0;
static int render_pose = 0;

static uint64_t milliseconds(void) {
	struct timeval value;
	gettimeofday(&value, NULL);
	return (uint64_t)value.tv_sec * 1000ULL + value.tv_usec / 1000;
}

static int prepare_path(void) {
	const char * home = getenv("HOME");
	if (!home) return -1;
	char directory[RAZION_COMPANION_PATH_MAX];
	int length = snprintf(directory, sizeof(directory), "%s/.razion", home);
	if (length < 0 || (size_t)length >= sizeof(directory)) return -1;
	if (mkdir(directory, 0700) && errno != EEXIST) return -1;
	length = snprintf(config_path, sizeof(config_path), "%s/companion.conf", directory);
	return length < 0 || (size_t)length >= sizeof(config_path) ? -1 : 0;
}

static void load_state(void) {
	if (prepare_path() || razion_companion_load(config_path, &config, &pet_state, time(NULL))) {
		razion_companion_defaults(&config, &pet_state, time(NULL));
	}
}

static void save_state(void) {
	if (!config_path[0] && prepare_path()) return;
	razion_companion_sanitize(&config, &pet_state);
	razion_companion_save(config_path, &config, &pet_state);
}

static void save_configuration(void) {
	if (!control_mode) { save_state(); return; }
	razion_companion_config_t disk_config;
	razion_companion_state_t disk_state;
	if (!config_path[0] && prepare_path()) return;
	if (!razion_companion_load(config_path, &disk_config, &disk_state, time(NULL))) {
		pet_state = disk_state;
	}
	save_state();
}

static int send_control(const char * command) {
	FILE * endpoint = pex_connect(RAZION_COMPANION_CONTROL_ENDPOINT);
	if (!endpoint) return 0;
	int sent = pex_reply(endpoint, strlen(command) + 1, (char *)command) == strlen(command) + 1;
	fclose(endpoint);
	return sent;
}

static void launch_overlay(void) {
	if (!fork()) {
		char * args[] = {"/bin/razion-companion", "--overlay", NULL};
		_Exit(execvp(args[0], args));
	}
}

static void notify_overlay(void) {
	save_configuration();
	if (config.enabled) {
		if (!send_control("reload")) launch_overlay();
	} else {
		send_control("quit");
	}
}

static void play_reaction_tone(int action) {
	if (!config.sound) return;
	if (!fork()) {
		char frequency[16];
		char length[16];
		int hz = 340 + config.species * 13 + action * 42;
		snprintf(frequency, sizeof(frequency), "%d", hz);
		snprintf(length, sizeof(length), "%d", action == RAZION_COMPANION_ACTION_SLEEP ? 110 : 70);
		char * args[] = {"/bin/beep", "--volume=20", "-f", frequency, "-l", length, NULL};
		_Exit(execvp(args[0], args));
	}
}

static int system_is_busy(void) {
	FILE * file = fopen("/proc/idle", "r");
	if (!file) return 0;
	char line[256];
	long total_idle = 0;
	int processors = 0;
	while (fgets(line, sizeof(line), file)) {
		char * cursor = strchr(line, ':');
		if (!cursor) continue;
		cursor++;
		long idle = 0;
		for (int i = 0; i < 4; ++i) idle += strtoul(cursor, &cursor, 10);
		total_idle += idle / 4;
		processors++;
	}
	fclose(file);
	return processors && total_idle / processors < 300;
}

static uint32_t pet_body_color(void) {
	if (config.color > 0 && config.color < (int)(sizeof(color_choices) / sizeof(color_choices[0]))) {
		return rgb((color_choices[config.color] >> 16) & 0xFF,
			(color_choices[config.color] >> 8) & 0xFF,
			color_choices[config.color] & 0xFF);
	}
	uint32_t color = razion_companion_species[config.species].body_color;
	return rgb((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
}

static uint32_t pet_accent_color(void) {
	uint32_t color = razion_companion_species[config.species].accent_color;
	return rgb((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
}

static void circle(gfx_context_t * target, int cx, int cy, int radius, uint32_t color) {
	for (int y = -radius; y <= radius; ++y) {
		int span = radius;
		while (span > 0 && span * span + y * y > radius * radius) span--;
		int x0 = cx - span;
		int x1 = cx + span;
		if (cy + y < 0 || cy + y >= target->height) continue;
		if (x0 < 0) x0 = 0;
		if (x1 >= target->width) x1 = target->width - 1;
		for (int x = x0; x <= x1; ++x) GFX(target, x, cy + y) = color;
	}
}

static void ellipse(gfx_context_t * target, int cx, int cy, int rx, int ry, uint32_t color) {
	if (rx < 1 || ry < 1) return;
	for (int y = -ry; y <= ry; ++y) {
		long yy = (long)y * y * rx * rx;
		long limit = (long)rx * rx * ry * ry;
		int span = rx;
		while (span > 0 && (long)span * span * ry * ry + yy > limit) span--;
		int x0 = cx - span;
		int x1 = cx + span;
		if (cy + y < 0 || cy + y >= target->height) continue;
		if (x0 < 0) x0 = 0;
		if (x1 >= target->width) x1 = target->width - 1;
		for (int x = x0; x <= x1; ++x) GFX(target, x, cy + y) = color;
	}
}

static void triangle(gfx_context_t * target, int cx, int top, int half_width, int height, uint32_t color) {
	for (int y = 0; y < height; ++y) {
		int span = half_width * y / height;
		int py = top + y;
		if (py < 0 || py >= target->height) continue;
		for (int x = cx - span; x <= cx + span; ++x) {
			if (x >= 0 && x < target->width) GFX(target, x, py) = color;
		}
	}
}

static void pet_eyes(gfx_context_t * target, int cx, int cy, int scale) {
	int spacing = 13 * scale / 100;
	int radius = 5 * scale / 100 + 1;
	if (render_pose == 4) {
		draw_line_thick(target, cx - spacing - radius, cx - spacing + radius,
			cy, cy + 1, rgb(35,47,57), 2);
		draw_line_thick(target, cx + spacing - radius, cx + spacing + radius,
			cy + 1, cy, rgb(35,47,57), 2);
		return;
	}
	int look = render_pose == 2 ? 2 : 0;
	circle(target, cx - spacing, cy, radius + 2, rgb(238,247,250));
	circle(target, cx + spacing, cy, radius + 2, rgb(238,247,250));
	circle(target, cx - spacing + look, cy, radius, rgb(24,47,62));
	circle(target, cx + spacing + look, cy, radius, rgb(24,47,62));
	circle(target, cx - spacing + look + 1, cy - 1, 1, rgb(255,255,255));
	circle(target, cx + spacing + look + 1, cy - 1, 1, rgb(255,255,255));
}

static void draw_accessory(gfx_context_t * target, int cx, int cy, int scale) {
	int w = 35 * scale / 100;
	switch (config.accessory) {
		case 1: /* collar */
			draw_rectangle_solid(target, cx - w, cy, w * 2, 5, C_BLUE);
			circle(target, cx, cy + 7, 4, C_ORANGE);
			break;
		case 2: /* scarf */
			draw_rectangle_solid(target, cx - w, cy - 2, w * 2, 7, C_RED);
			draw_rectangle_solid(target, cx + w - 8, cy + 4, 7, 20, C_RED);
			break;
		case 3: /* star */
			circle(target, cx, cy + 5, 6, rgb(244,210,67));
			break;
		case 4: /* bow */
			triangle(target, cx - 2, cy - 11, 13, 12, C_PURPLE);
			triangle(target, cx + 2, cy - 11, 13, 12, C_PURPLE);
			circle(target, cx, cy - 3, 4, C_PURPLE);
			break;
		case 5: /* glasses */
			draw_rectangle(target, cx - 27, cy - 27, 21, 14, 0xFF9DE8FF);
			draw_rectangle(target, cx + 6, cy - 27, 21, 14, 0xFF9DE8FF);
			draw_line(target, cx - 6, cx + 6, cy - 20, cy - 20, 0xFF9DE8FF);
			break;
	}
}

static void draw_pet(gfx_context_t * target, int cx, int base_y, int scale, int phase) {
	const razion_companion_species_t * species = &razion_companion_species[config.species];
	uint32_t body = pet_body_color();
	uint32_t accent = pet_accent_color();
	int bob = phase && render_pose != 4 && config.animation_style != 1 ?
		(config.animation_style == 2 ? -4 : -2) : 0;
	if (phase && config.animation_style == 1) cx += 3;
	if (render_pose == 1) base_y += 4;
	if (render_pose == 3 && phase) base_y -= 4;
	if (render_pose == 4) { base_y += 8; scale = scale * 92 / 100; }
	int head_y = base_y - 75 * scale / 100 + bob;
	int body_y = base_y - 29 * scale / 100 + bob;
	int shape = species->shape;

	if (shape == 7) { /* snake */
		for (int i = 0; i < 4; ++i) ellipse(target, cx + (i & 1 ? 10 : -10), base_y - 12 - i * 12, 28 - i * 3, 12, body);
		circle(target, cx, head_y + 20, 27 * scale / 100, body);
		pet_eyes(target, cx, head_y + 15, scale);
		draw_line(target, cx, cx + 10, head_y + 31, head_y + 35, C_RED);
		return;
	}
	if (shape == 8) { /* turtle */
		ellipse(target, cx - 3, body_y, 48 * scale / 100, 28 * scale / 100, accent);
		circle(target, cx + 43 * scale / 100, body_y - 5, 17 * scale / 100, body);
		pet_eyes(target, cx + 48 * scale / 100, body_y - 9, scale * 65 / 100);
		for (int side = -1; side <= 1; side += 2) {
			circle(target, cx + side * 25 * scale / 100, base_y - 4, 9 * scale / 100, body);
		}
		return;
	}
	if (shape == 9) { /* frog */
		ellipse(target, cx, body_y, 42 * scale / 100, 31 * scale / 100, body);
		circle(target, cx - 23 * scale / 100, head_y + 3, 16 * scale / 100, body);
		circle(target, cx + 23 * scale / 100, head_y + 3, 16 * scale / 100, body);
		pet_eyes(target, cx, head_y + 3, scale * 130 / 100);
		return;
	}

	/* Body, feet, and tail/wing establish a shared friendly visual language. */
	ellipse(target, cx, body_y, 37 * scale / 100, 43 * scale / 100, body);
	circle(target, cx - 23 * scale / 100, base_y - 4, 12 * scale / 100, body);
	circle(target, cx + 23 * scale / 100, base_y - 4, 12 * scale / 100, body);
	if (shape == 6) {
		ellipse(target, cx - 39 * scale / 100, body_y - 5, 22 * scale / 100, 11 * scale / 100, accent);
		ellipse(target, cx + 39 * scale / 100, body_y - 5, 22 * scale / 100, 11 * scale / 100, accent);
	}
	if (shape == 2) circle(target, cx, head_y, 48 * scale / 100, accent);

	/* Ears and silhouette-specific features. */
	if (shape == 0 || shape == 1 || shape == 10) {
		triangle(target, cx - 24 * scale / 100, head_y - 43 * scale / 100, 17 * scale / 100, 31 * scale / 100, body);
		triangle(target, cx + 24 * scale / 100, head_y - 43 * scale / 100, 17 * scale / 100, 31 * scale / 100, body);
	} else if (shape == 3) {
		ellipse(target, cx - 22 * scale / 100, head_y - 35 * scale / 100, 11 * scale / 100, 34 * scale / 100, body);
		ellipse(target, cx + 22 * scale / 100, head_y - 35 * scale / 100, 11 * scale / 100, 34 * scale / 100, body);
	} else if (shape == 4 || shape == 12) {
		circle(target, cx - 27 * scale / 100, head_y - 24 * scale / 100, 15 * scale / 100, accent);
		circle(target, cx + 27 * scale / 100, head_y - 24 * scale / 100, 15 * scale / 100, accent);
	} else if (shape == 11) {
		triangle(target, cx, head_y - 62 * scale / 100, 9 * scale / 100, 35 * scale / 100, accent);
		triangle(target, cx - 24 * scale / 100, head_y - 38 * scale / 100, 12 * scale / 100, 25 * scale / 100, body);
		triangle(target, cx + 24 * scale / 100, head_y - 38 * scale / 100, 12 * scale / 100, 25 * scale / 100, body);
	}

	circle(target, cx, head_y, 36 * scale / 100, body);
	if (shape == 5) ellipse(target, cx, body_y - 8, 25 * scale / 100, 36 * scale / 100, accent);
	if (shape == 6) triangle(target, cx, head_y + 5, 11 * scale / 100, 11 * scale / 100, C_ORANGE);
	pet_eyes(target, cx, head_y - 5, scale);
	circle(target, cx, head_y + 12 * scale / 100, 4 * scale / 100 + 1, shape == 6 ? C_ORANGE : rgb(44,39,45));
	if (shape == 10) {
		for (int i = 0; i < 3; ++i) triangle(target, cx + 35 * scale / 100, body_y - 32 + i * 15, 7, 13, accent);
	}
	draw_accessory(target, cx, head_y + 37 * scale / 100, scale);
}

static void draw_bar(gfx_context_t * target, int x, int y, int width,
	const char * label, int value, uint32_t color) {
	tt_set_size(font, 12);
	char value_text[16];
	snprintf(value_text, sizeof(value_text), "%d", value);
	tt_draw_string(target, font, x, y + 12, label, C_MUTED);
	tt_draw_string(target, font, x + width - 22, y + 12, value_text, C_TEXT);
	draw_rounded_rectangle(target, x, y + 19, width, 7, 3, rgb(31,42,54));
	draw_rounded_rectangle(target, x, y + 19, width * value / 100, 7, 3, color);
}

static void post_notice(razion_companion_notice_t notice) {
	if (notice == RAZION_COMPANION_NOTICE_NONE) return;
	FILE * toast = fopen("/dev/pex/toast", "w");
	if (!toast) return; /* Visual speech remains the fallback. */
	fprintf(toast, "{\"duration\":5,\"body\":\"%s %s\"}",
		config.name, razion_companion_notice_text(notice));
	fclose(toast);
}

static void overlay_redraw(const char * reaction, int phase) {
	draw_fill(ctx, rgba(0,0,0,0));
	if (reaction && *reaction) {
		int bubble_width = tt_string_width(font, reaction) + 24;
		if (bubble_width > OVERLAY_SIZE - 12) bubble_width = OVERLAY_SIZE - 12;
		draw_rounded_rectangle(ctx, (OVERLAY_SIZE - bubble_width) / 2, 6,
			bubble_width, 34, 12, rgba(10,17,25,235));
		tt_set_size(font, 12);
		tt_draw_string(ctx, font, (OVERLAY_SIZE - bubble_width) / 2 + 12, 28,
			reaction, rgb(239,246,252));
	}
	int scale = config.size == 1 ? 72 : config.size == 2 ? 92 : 112;
	draw_pet(ctx, OVERLAY_SIZE / 2, OVERLAY_SIZE - 18, scale, phase);
	flip(ctx);
	yutani_flip(yctx, window);
}

static void apply_position(void) {
	int x = config.position_x;
	int y = config.position_y;
	if (config.desktop_mode == 1) {
		x = yctx->display_width - OVERLAY_SIZE - 18;
		y = yctx->display_height - OVERLAY_SIZE - 18;
	}
	if (x < 0) x = 0;
	if (y < 28) y = 28;
	if (x + OVERLAY_SIZE > (int)yctx->display_width) x = yctx->display_width - OVERLAY_SIZE;
	if (y + OVERLAY_SIZE > (int)yctx->display_height) y = yctx->display_height - OVERLAY_SIZE;
	yutani_window_move(yctx, window, x, y);
}

static void overlay_reload(void) {
	razion_companion_config_t previous = config;
	load_state();
	if (!config.enabled) { running = 0; return; }
	yutani_window_update_shape(yctx, window, config.click_through ?
		YUTANI_SHAPE_THRESHOLD_PASSTHROUGH : YUTANI_SHAPE_THRESHOLD_CLEAR);
	if (config.always_on_top != previous.always_on_top) {
		yutani_set_stack(yctx, window, config.always_on_top ? YUTANI_ZORDER_OVERLAY : 1);
	}
	apply_position();
	overlay_redraw(config.hidden ? NULL : "", 0);
}

static void handle_overlay_command(const char * command, char * reaction, size_t size, uint64_t * reaction_until) {
	if (!strcmp(command, "quit")) { running = 0; return; }
	if (!strcmp(command, "reload")) { overlay_reload(); return; }
	if (!strcmp(command, "show")) { config.hidden = 0; save_state(); overlay_redraw("Welcome back!", 0); return; }
	if (!strcmp(command, "hide")) { config.hidden = 1; save_state(); overlay_redraw(NULL, 0); return; }
	if (!strncmp(command, "action:", 7)) {
		int action = atoi(command + 7);
		if (action >= 0 && action < RAZION_COMPANION_ACTION_COUNT) {
			render_pose = action == RAZION_COMPANION_ACTION_SLEEP ? 4 :
				action == RAZION_COMPANION_ACTION_PLAY ? 3 :
				action == RAZION_COMPANION_ACTION_TALK ? 2 : 1;
			const char * text = razion_companion_act(&config, &pet_state, action, time(NULL));
			snprintf(reaction, size, "%s", text);
			*reaction_until = milliseconds() + 3500;
			play_reaction_tone(action);
			save_state();
		}
	} else if (!strncmp(command, "react:", 6)) {
		int action = atoi(command + 6);
		overlay_reload();
		static const char * reactions[] = {
			"That was delicious!", "That feels nice!", "Let's play!", "Good night...", NULL
		};
		if (action >= 0 && action < RAZION_COMPANION_ACTION_COUNT) {
			render_pose = action == RAZION_COMPANION_ACTION_SLEEP ? 4 :
				action == RAZION_COMPANION_ACTION_PLAY ? 3 :
				action == RAZION_COMPANION_ACTION_TALK ? 2 : 1;
			const char * text = action == RAZION_COMPANION_ACTION_TALK ?
				razion_companion_species[config.species].voice : reactions[action];
			snprintf(reaction, size, "%s", text);
			*reaction_until = milliseconds() + 3500;
			play_reaction_tone(action);
		}
	}
}

static int run_overlay(void) {
	load_state();
	if (!config.enabled) return 0;
	FILE * control = pex_bind(RAZION_COMPANION_CONTROL_ENDPOINT);
	if (!control) return 0; /* One overlay per user session. */
	yctx = yutani_init();
	if (!yctx) { fclose(control); return 1; }
	window = yutani_window_create_flags(yctx, OVERLAY_SIZE, OVERLAY_SIZE,
		YUTANI_WINDOW_FLAG_NO_STEAL_FOCUS | YUTANI_WINDOW_FLAG_DISALLOW_RESIZE |
		YUTANI_WINDOW_FLAG_NO_ANIMATION);
	ctx = init_graphics_yutani_double_buffer(window);
	font = tt_font_from_shm("sans-serif");
	yutani_window_advertise_icon(yctx, window, "Razion Companion", "star");
	yutani_window_update_shape(yctx, window, config.click_through ?
		YUTANI_SHAPE_THRESHOLD_PASSTHROUGH : YUTANI_SHAPE_THRESHOLD_CLEAR);
	if (config.always_on_top) yutani_set_stack(yctx, window, YUTANI_ZORDER_OVERLAY);
	apply_position();
	overlay_redraw(config.hidden ? NULL : "Hello!", 0);

	char reaction[96] = "";
	uint64_t reaction_until = 0;
	uint64_t last_frame = 0;
	uint64_t last_save = milliseconds();
	uint64_t last_load_check = 0;
	uint64_t last_wander = 0;
	uint64_t last_pose_change = milliseconds();
	int busy = 0;
	int wander_direction = 1;
	int phase = 0;
	while (running) {
		int fds[2] = {fileno(yctx->sock), fileno(control)};
		uint64_t before_wait = milliseconds();
		if (!last_load_check || before_wait - last_load_check >= 15000) {
			busy = system_is_busy();
			last_load_check = before_wait;
		}
		int timeout = config.hidden ? 60000 : (config.animation_speed == 1 ? 900 : config.animation_speed == 2 ? 550 : 350);
		if (busy && !config.hidden) timeout *= 3;
		int index = fswait2(2, fds, timeout);
		if (index == 0) {
			yutani_msg_t * event = yutani_poll(yctx);
			while (event) {
				if (event->type == YUTANI_MSG_WINDOW_MOUSE_EVENT && !config.hidden) {
					struct yutani_msg_window_mouse_event * mouse = (void *)event->data;
					if (mouse->command == YUTANI_MOUSE_EVENT_CLICK) {
						if (mouse->buttons & YUTANI_MOUSE_BUTTON_RIGHT) {
							if (!fork()) { char * args[] = {"/bin/razion-companion", NULL}; _Exit(execvp(args[0], args)); }
						} else {
							render_pose = 1;
							const char * text = razion_companion_act(&config, &pet_state,
								RAZION_COMPANION_ACTION_PET, time(NULL));
							snprintf(reaction, sizeof(reaction), "%s", text);
							reaction_until = milliseconds() + 3000;
							play_reaction_tone(RAZION_COMPANION_ACTION_PET);
							save_state();
						}
					} else if (mouse->command == YUTANI_MOUSE_EVENT_DOWN &&
						config.desktop_mode != 1 && mouse->buttons & YUTANI_MOUSE_BUTTON_LEFT) {
						yutani_window_drag_start(yctx, window);
					}
				} else if (event->type == YUTANI_MSG_WINDOW_CLOSE || event->type == YUTANI_MSG_SESSION_END) {
					running = 0;
				}
				free(event);
				event = yutani_poll_async(yctx);
			}
		} else if (index == 1) {
			pex_packet_t * packet = calloc(1, PACKET_SIZE);
			pex_listen(control, packet);
			packet->data[MAX_PACKET_SIZE - 1] = '\0';
			handle_overlay_command((char *)packet->data, reaction, sizeof(reaction), &reaction_until);
			free(packet);
		}
		uint64_t now_ms = milliseconds();
		if (!config.hidden && !busy && config.desktop_mode == 0 && now_ms - last_wander >= 4000) {
			render_pose = 3;
			int next_x = window->x + wander_direction * (3 + config.animation_speed);
			if (next_x < 0 || next_x + OVERLAY_SIZE > (int)yctx->display_width) {
				wander_direction = -wander_direction;
				next_x = window->x + wander_direction * (3 + config.animation_speed);
			}
			yutani_window_move(yctx, window, next_x, window->y);
			config.position_x = next_x;
			last_wander = now_ms;
		}
		if (!config.hidden && now_ms - last_frame >= (uint64_t)timeout) {
			phase = !phase;
			if (reaction_until && now_ms >= reaction_until) {
				reaction[0] = '\0'; reaction_until = 0; render_pose = 0;
			}
			if (!reaction_until && now_ms - last_pose_change >= 9000) {
				render_pose = (render_pose + 1) % 3; /* idle, sit, look around */
				last_pose_change = now_ms;
			}
			overlay_redraw(reaction, phase);
			last_frame = now_ms;
		}
		if (razion_companion_tick(&pet_state, time(NULL))) {
			post_notice(razion_companion_notice(&config, &pet_state, time(NULL)));
		}
		if (now_ms - last_save >= 60000) { save_state(); last_save = now_ms; }
	}
	config.position_x = window->x;
	config.position_y = window->y;
	save_state();
	yutani_close(yctx, window);
	fclose(control);
	return 0;
}

/* ---------------------------- Control center ---------------------------- */

static int hover_x = -1;
static int hover_y = -1;
static int editing_name = 0;
static uint64_t status_until = 0;
static char status_text[96] = "Changes are saved locally.";
static int reflex_active = 0;
static int reflex_x = 0;
static int reflex_y = 0;
static uint64_t reflex_until = 0;

static int hit(int x, int y, int width, int height, struct yutani_msg_window_mouse_event * mouse) {
	return mouse->new_x >= x && mouse->new_x < x + width && mouse->new_y >= y && mouse->new_y < y + height;
}

static void label_button(int x, int y, int width, const char * text, int selected) {
	int hover = hover_x >= x && hover_x < x + width && hover_y >= y && hover_y < y + 34;
	draw_rounded_rectangle(ctx, x, y, width, 34, 6, selected ? rgb(31,90,148) : hover ? C_PANEL_HOVER : C_PANEL);
	draw_rectangle(ctx, x, y, width, 1, selected ? C_BLUE : C_BORDER);
	tt_set_size(font, 12);
	int text_width = tt_string_width(font, text);
	tt_draw_string(ctx, font, x + (width - text_width) / 2, y + 22, text, C_TEXT);
}

static const char * cycle_name(int kind, int value) {
	static const char * colors[] = {"Original", "Sky", "Mint", "Amber", "Violet", "Rose", "Gold", "Slate"};
	static const char * accessories[] = {"None", "Collar", "Scarf", "Charm", "Bow", "Glasses"};
	static const char * personalities[] = {"Species", "Playful", "Calm", "Curious", "Cozy"};
	static const char * sizes[] = {"", "Small", "Medium", "Large"};
	static const char * speeds[] = {"", "Gentle", "Normal", "Lively"};
	static const char * modes[] = {"Free movement", "Corner mode", "Stay nearby"};
	static const char * notices[] = {"Off", "Rare", "Balanced", "Often"};
	static const char * styles[] = {"Soft bounce", "Gentle sway", "Bright bounce"};
	switch (kind) {
		case 0: return colors[value];
		case 1: return accessories[value];
		case 2: return personalities[value];
		case 3: return sizes[value];
		case 4: return speeds[value];
		case 5: return modes[value];
		case 6: return notices[value];
		default: return styles[value];
	}
}

static void control_redraw(void) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	draw_fill(ctx, C_BACKGROUND);
	int ox = bounds.left_width;
	int oy = bounds.top_height;

	tt_set_size(font, 24);
	tt_draw_string(ctx, font, ox + 28, oy + 40, "Desktop Companion", C_TEXT);
	tt_set_size(font, 12);
	tt_draw_string(ctx, font, ox + 28, oy + 64,
		"A quiet, offline personality layer for your RazionOS desktop.", C_MUTED);
	label_button(ox + 700, oy + 24, 160, config.enabled ? "Companion enabled" : "Companion disabled", config.enabled);

	/* Data-driven species grid. */
	tt_set_size(font, 13);
	tt_draw_string(ctx, font, ox + 28, oy + 94, "Choose a companion", C_TEXT);
	for (int i = 0; i < RAZION_COMPANION_SPECIES_COUNT; ++i) {
		int column = i % TILE_COLUMNS;
		int row = i / TILE_COLUMNS;
		int x = ox + 28 + column * (TILE_WIDTH + 7);
		int y = oy + 108 + row * (TILE_HEIGHT + 7);
		int hovered = hover_x >= x && hover_x < x + TILE_WIDTH && hover_y >= y && hover_y < y + TILE_HEIGHT;
		draw_rounded_rectangle(ctx, x, y, TILE_WIDTH, TILE_HEIGHT, 7,
			i == config.species ? rgb(25,66,105) : hovered ? C_PANEL_HOVER : C_PANEL);
		draw_rectangle(ctx, x, y, TILE_WIDTH, 1, i == config.species ? C_BLUE : C_BORDER);
		uint32_t color = razion_companion_species[i].body_color;
		circle(ctx, x + 15, y + 19, 8, rgb((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
		tt_set_size(font, 11);
		tt_draw_string(ctx, font, x + 28, y + 22, razion_companion_species[i].name, C_TEXT);
		tt_set_size(font, 9);
		tt_draw_string(ctx, font, x + 10, y + 44, razion_companion_species[i].voice, C_MUTED);
	}

	/* Profile and live preview. */
	int px = ox + 602;
	int py = oy + 88;
	draw_rounded_rectangle(ctx, px, py, 270, 290, 10, C_PANEL);
	draw_pet(ctx, px + 70, py + 158, 76, 0);
	tt_set_size(font, 19);
	tt_draw_string(ctx, font, px + 138, py + 38, config.name, C_TEXT);
	tt_set_size(font, 11);
	tt_draw_string(ctx, font, px + 138, py + 59, razion_companion_species[config.species].name, C_BLUE);
	tt_draw_string(ctx, font, px + 138, py + 78, razion_companion_species[config.species].temperament, C_MUTED);
	draw_bar(ctx, px + 138, py + 96, 112, "Happy", pet_state.happiness, C_GREEN);
	draw_bar(ctx, px + 138, py + 128, 112, "Fed", pet_state.hunger, C_ORANGE);
	draw_bar(ctx, px + 138, py + 160, 112, "Energy", pet_state.energy, C_BLUE);
	draw_bar(ctx, px + 138, py + 192, 112, "Health", pet_state.health, C_RED);
	char level[64];
	snprintf(level, sizeof(level), "Level %d   %d/%d XP", pet_state.level,
		pet_state.experience, pet_state.level * 100);
	tt_set_size(font, 11);
	tt_draw_string(ctx, font, px + 22, py + 262, level, C_PURPLE);

	/* Name and customization rows. */
	int cy = oy + 384;
	tt_set_size(font, 11);
	tt_draw_string(ctx, font, ox + 28, cy + 13, "Pet name", C_MUTED);
	draw_rounded_rectangle(ctx, ox + 28, cy + 20, 200, 34, 6, rgb(20,28,38));
	draw_rectangle(ctx, ox + 28, cy + 20, 200, 1, editing_name ? C_BLUE : C_BORDER);
	tt_set_size(font, 13);
	tt_draw_string(ctx, font, ox + 40, cy + 42, config.name, C_TEXT);
	if (editing_name) {
		int cursor = tt_string_width(font, config.name);
		draw_rectangle(ctx, ox + 40 + cursor, cy + 27, 1, 18, C_BLUE);
	}
	label_button(ox + 240, cy + 20, 110, cycle_name(0, config.color), 0);
	label_button(ox + 360, cy + 20, 110, cycle_name(1, config.accessory), 0);
	label_button(ox + 480, cy + 20, 110, cycle_name(2, config.personality), 0);
	label_button(ox + 600, cy + 20, 95, cycle_name(3, config.size), 0);
	label_button(ox + 705, cy + 20, 155, cycle_name(5, config.desktop_mode), 0);

	cy += 66;
	label_button(ox + 28, cy, 130, config.hidden ? "Show on desktop" : "Hide from desktop", !config.hidden);
	label_button(ox + 168, cy, 112, config.always_on_top ? "Always on top" : "Normal layer", config.always_on_top);
	label_button(ox + 290, cy, 112, config.click_through ? "Click-through" : "Interactive", config.click_through);
	label_button(ox + 412, cy, 100, config.sound ? "Sound on" : "Sound off", config.sound);
	label_button(ox + 522, cy, 132, cycle_name(6, config.notification_frequency), config.notifications);
	label_button(ox + 664, cy, 95, cycle_name(4, config.animation_speed), 0);
	label_button(ox + 769, cy, 91, cycle_name(7, config.animation_style), 0);

	cy += 48;
	const char * actions[] = {"Feed", "Pet", "Play", "Sleep", "Talk"};
	for (int i = 0; i < RAZION_COMPANION_ACTION_COUNT; ++i) {
		label_button(ox + 28 + i * 102, cy, 92, actions[i], 0);
	}
	if (reflex_active) {
		draw_rounded_rectangle(ctx, ox + 552, cy - 5, 308, 44, 8, rgb(20,31,43));
		circle(ctx, ox + 552 + reflex_x, cy - 5 + reflex_y, 11, C_GREEN);
		tt_set_size(font, 10);
		tt_draw_string(ctx, font, ox + 570, cy + 23, "Catch the mint dot!", C_MUTED);
	} else {
		label_button(ox + 552, cy, 128, "Reflex activity", 0);
	}

	tt_set_size(font, 11);
	if (status_until && milliseconds() > status_until) snprintf(status_text, sizeof(status_text), "Changes are saved locally.");
	tt_draw_string(ctx, font, ox + 28, window->height - bounds.bottom_height - 14, status_text, C_MUTED);
	render_decorations(window, ctx, "RazionOS Personalization - Desktop Companion");
	flip(ctx);
	yutani_flip(yctx, window);
}

static void status(const char * text) {
	snprintf(status_text, sizeof(status_text), "%s", text);
	status_until = milliseconds() + 3500;
}

static void control_action(int action) {
	const char * reaction = razion_companion_act(&config, &pet_state, action, time(NULL));
	char message[96];
	snprintf(message, sizeof(message), "%s: %s", config.name, reaction);
	status(message);
	save_state();
	char command[32];
	snprintf(command, sizeof(command), "react:%d", action);
	if (!send_control(command)) play_reaction_tone(action);
}

static void start_reflex(void) {
	reflex_active = 1;
	reflex_x = 45 + rand() % 240;
	reflex_y = 12 + rand() % 20;
	reflex_until = milliseconds() + 5000;
	status("Catch the mint dot before it moves.");
}

static void control_click(struct yutani_msg_window_mouse_event * mouse, int ox, int oy) {
	if (hit(ox + 700, oy + 24, 160, 34, mouse)) {
		config.enabled = !config.enabled;
		if (!config.enabled) config.hidden = 0;
		notify_overlay();
		status(config.enabled ? "Companion enabled." : "Companion disabled; no background process remains.");
		return;
	}
	for (int i = 0; i < RAZION_COMPANION_SPECIES_COUNT; ++i) {
		int x = ox + 28 + (i % TILE_COLUMNS) * (TILE_WIDTH + 7);
		int y = oy + 108 + (i / TILE_COLUMNS) * (TILE_HEIGHT + 7);
		if (hit(x, y, TILE_WIDTH, TILE_HEIGHT, mouse)) {
			config.species = i;
			notify_overlay();
			status("Companion species updated.");
			return;
		}
	}
	int cy = oy + 384;
	if (hit(ox + 28, cy + 20, 200, 34, mouse)) { editing_name = 1; return; }
	if (hit(ox + 240, cy + 20, 110, 34, mouse)) {
		int maximum = pet_state.level + 1;
		if (maximum > 7) maximum = 7;
		config.color = (config.color + 1) % (maximum + 1);
	} else if (hit(ox + 360, cy + 20, 110, 34, mouse)) {
		int maximum = pet_state.level;
		if (maximum > 5) maximum = 5;
		config.accessory = (config.accessory + 1) % (maximum + 1);
	}
	else if (hit(ox + 480, cy + 20, 110, 34, mouse)) config.personality = (config.personality + 1) % 5;
	else if (hit(ox + 600, cy + 20, 95, 34, mouse)) config.size = config.size % 3 + 1;
	else if (hit(ox + 705, cy + 20, 155, 34, mouse)) config.desktop_mode = (config.desktop_mode + 1) % 3;
	else {
		cy += 66;
		if (hit(ox + 28, cy, 130, 34, mouse)) {
			config.hidden = !config.hidden;
			send_control(config.hidden ? "hide" : "show");
		} else if (hit(ox + 168, cy, 112, 34, mouse)) config.always_on_top = !config.always_on_top;
		else if (hit(ox + 290, cy, 112, 34, mouse)) config.click_through = !config.click_through;
		else if (hit(ox + 412, cy, 100, 34, mouse)) config.sound = !config.sound;
		else if (hit(ox + 522, cy, 132, 34, mouse)) {
			config.notification_frequency = (config.notification_frequency + 1) % 4;
			config.notifications = config.notification_frequency != 0;
		} else if (hit(ox + 664, cy, 95, 34, mouse)) config.animation_speed = config.animation_speed % 3 + 1;
		else if (hit(ox + 769, cy, 91, 34, mouse)) {
			int maximum = pet_state.level >= 3 ? 2 : pet_state.level >= 2 ? 1 : 0;
			config.animation_style = (config.animation_style + 1) % (maximum + 1);
		}
		else {
			cy += 48;
			for (int i = 0; i < RAZION_COMPANION_ACTION_COUNT; ++i) {
				if (hit(ox + 28 + i * 102, cy, 92, 34, mouse)) { control_action(i); return; }
			}
			if (!reflex_active && hit(ox + 552, cy, 128, 34, mouse)) { start_reflex(); return; }
			if (reflex_active && hit(ox + 552, cy - 5, 308, 44, mouse)) {
				int dx = mouse->new_x - (ox + 552 + reflex_x);
				int dy = mouse->new_y - (cy - 5 + reflex_y);
				if (dx * dx + dy * dy < 18 * 18 && milliseconds() <= reflex_until) {
					control_action(RAZION_COMPANION_ACTION_PLAY);
					status("Great reflexes! Bonus play experience earned.");
					reflex_active = 0;
				} else status("Almost - try the mint dot again.");
				return;
			}
			return;
		}
	}
	editing_name = 0;
	notify_overlay();
	status("Customization saved.");
}

static int run_control(void) {
	load_state();
	srand(time(NULL));
	yctx = yutani_init();
	if (!yctx) return 1;
	init_decorations();
	struct decor_bounds bounds;
	decor_get_bounds(NULL, &bounds);
	window = yutani_window_create(yctx, CONTROL_WIDTH + bounds.width, CONTROL_HEIGHT + bounds.height);
	window->decorator_flags |= DECOR_FLAG_NO_MAXIMIZE;
	yutani_window_move(yctx, window,
		yctx->display_width / 2 - window->width / 2,
		yctx->display_height / 2 - window->height / 2);
	yutani_window_advertise_icon(yctx, window, "Desktop Companion", "star");
	ctx = init_graphics_yutani_double_buffer(window);
	font = tt_font_from_shm("sans-serif");
	control_redraw();
	while (running) {
		yutani_msg_t * event = yutani_poll(yctx);
		if (!event) continue;
		switch (event->type) {
			case YUTANI_MSG_KEY_EVENT: {
				struct yutani_msg_key_event * key = (void *)event->data;
				if (key->event.action != KEY_ACTION_DOWN) break;
				if (key->event.key == KEY_ESCAPE) { if (editing_name) editing_name = 0; else running = 0; }
				else if (editing_name) {
					size_t length = strlen(config.name);
					if (key->event.key == '\n') { editing_name = 0; notify_overlay(); status("Name saved."); }
					else if (key->event.key == '\b' || key->event.keycode == KEY_BACKSPACE) {
						if (length) config.name[length - 1] = '\0';
					} else if (key->event.key >= 0x20 && key->event.key < 0x7F &&
						key->event.key != '=' && key->event.key != '"' && key->event.key != '\\' &&
						length < RAZION_COMPANION_NAME_MAX - 1) {
						config.name[length] = key->event.key;
						config.name[length + 1] = '\0';
					}
				}
				control_redraw();
				break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)event->data;
				if (mouse->wid != window->wid) break;
				int decor = decor_handle_event(yctx, event);
				if (decor == DECOR_CLOSE) running = 0;
				hover_x = mouse->new_x;
				hover_y = mouse->new_y;
				if (mouse->command == YUTANI_MOUSE_EVENT_CLICK) {
					control_click(mouse, bounds.left_width, bounds.top_height);
				}
				control_redraw();
				break;
			}
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE: {
				struct yutani_msg_window_focus_change * focus = (void *)event->data;
				if (focus->wid == window->wid) { window->focused = focus->focused; control_redraw(); }
				break;
			}
			case YUTANI_MSG_WINDOW_CLOSE:
			case YUTANI_MSG_SESSION_END:
				running = 0;
		}
		free(event);
	}
	if (editing_name) notify_overlay(); else save_configuration();
	yutani_close(yctx, window);
	return 0;
}

int main(int argc, char * argv[]) {
	signal(SIGCHLD, SIG_IGN);
	if (argc > 1 && !strcmp(argv[1], "--overlay")) return run_overlay();
	if (argc > 1 && !strcmp(argv[1], "--hide")) { load_state(); config.hidden = 1; save_state(); send_control("hide"); return 0; }
	if (argc > 1 && !strcmp(argv[1], "--show")) { load_state(); config.hidden = 0; save_state(); send_control("show"); return 0; }
	if (argc > 1 && !strncmp(argv[1], "--", 2)) {
		fprintf(stderr, "usage: %s [--overlay|--hide|--show]\n", argv[0]);
		return 1;
	}
	control_mode = 1;
	return run_control();
}
