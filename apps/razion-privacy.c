/**
 * @brief Razion Privacy Center.
 *
 * Reports only capabilities observable from the running system. RazionOS does
 * not currently have a per-application permission broker, so device and file
 * access controls are explicitly reported as unavailable instead of simulated.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <razion/theme.h>
#include <toaru/confreader.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define WIDTH 820
#define HEIGHT 570

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font, * bold;
static int running = 1, hover = -1, pressed = -1, focus_id = 1;
static int cloud_allowed, ollama_enabled, audit_enabled;
static char recent_activity[180];

static void launch(const char * executable, const char * first, const char * second) {
	if (fork()) return;
	char * arguments[] = {(char *)executable, (char *)first, (char *)second, NULL};
	execv(executable, arguments);
	_Exit(127);
}

static void read_last_activity(const char * path) {
	recent_activity[0] = '\0';
	FILE * file = fopen(path, "r");
	if (!file) {
		snprintf(recent_activity, sizeof(recent_activity), "No readable AI activity log is present.");
		return;
	}
	char line[512];
	while (fgets(line, sizeof(line), file)) {
		size_t length = strlen(line);
		while (length && isspace((unsigned char)line[length - 1])) line[--length] = '\0';
		if (!length) continue;
		for (size_t i = 0; line[i]; ++i) if ((unsigned char)line[i] < 32) line[i] = ' ';
		snprintf(recent_activity, sizeof(recent_activity), "%s", line);
	}
	fclose(file);
	if (!recent_activity[0]) snprintf(recent_activity, sizeof(recent_activity), "AI activity log is empty.");
}

static void refresh_status(void) {
	cloud_allowed = 0;
	ollama_enabled = 0;
	audit_enabled = 1;
	const char * audit_path = "/var/log/razion-ai.log";
	confreader_t * config = confreader_load("/etc/razion-ai.conf");
	if (config) {
		cloud_allowed = confreader_intd(config, "engine", "allow_cloud", 0);
		audit_enabled = confreader_intd(config, "engine", "audit_enabled", 1);
		ollama_enabled = confreader_intd(config, "provider.ollama", "enabled", 0);
		audit_path = confreader_getd(config, "engine", "audit_log", "/var/log/razion-ai.log");
		read_last_activity(audit_path);
		confreader_free(config);
	} else {
		snprintf(recent_activity, sizeof(recent_activity), "AI policy file is unavailable.");
	}
}

static void label(int x, int y, int size, const char * value, uint32_t color, int strong) {
	tt_set_size(strong ? bold : font, size);
	tt_draw_string(ctx, strong ? bold : font, x, y, value, color);
}

static void row(int x, int y, int width, const char * title, const char * detail,
	const char * status, uint32_t status_color) {
	draw_rectangle_solid(ctx, x, y + 54, width, 1, RAZION_BORDER);
	label(x, y + 20, 12, title, RAZION_TEXT_PRIMARY, 1);
	char * shown = tt_ellipsify(detail, 9, font, width - 180, NULL);
	label(x, y + 40, 9, shown, RAZION_TEXT_SECONDARY, 0);
	free(shown);
	tt_set_size(bold, 9);
	int sw = tt_string_width(bold, status);
	label(x + width - sw, y + 27, 9, status, status_color, 1);
}

static void button(int id, int x, int y, int width, const char * text) {
	unsigned state = RAZION_CONTROL_NORMAL;
	if (id == hover) state |= RAZION_CONTROL_HOVER;
	if (id == pressed) state |= RAZION_CONTROL_PRESSED;
	if (id == focus_id && window->focused) state |= RAZION_CONTROL_FOCUSED;
	razion_draw_button(ctx, font, x, y, width, 36, text, state);
}

static void redraw(void) {
	struct decor_bounds bounds; decor_get_bounds(window, &bounds);
	draw_fill(ctx, RAZION_BACKGROUND);
	int x = bounds.left_width + 28, top = bounds.top_height;
	int width = window->width - bounds.width - 56;
	razion_draw_accent_bar(ctx, x, top + 20, 112);
	label(x, top + 39, 23, "Privacy Center", RAZION_TEXT_PRIMARY, 1);
	label(x, top + 61, 10, "Live policy and capability status — unsupported controls are never simulated", RAZION_TEXT_SECONDARY, 0);

	razion_draw_card(ctx, x, top + 82, width, 285, RAZION_SURFACE);
	int rx = x + 18, rw = width - 36, y = top + 92;
	row(rx, y, rw, "Network access", "Network is system-wide; per-application mediation is not implemented.",
		"CONTROL UNAVAILABLE", RAZION_WARNING); y += 55;
	row(rx, y, rw, "Microphone and camera", "Device nodes may exist, but RazionOS has no per-application permission broker.",
		"CONTROL UNAVAILABLE", RAZION_WARNING); y += 55;
	row(rx, y, rw, "Filesystem access", "Applications run with the signed-in user's filesystem permissions.",
		"USER PERMISSIONS", RAZION_ACCENT); y += 55;
	row(rx, y, rw, "Cloud AI routing", cloud_allowed ? "Cloud routing is allowed by system policy." : "Cloud routing is disabled in /etc/razion-ai.conf.",
		cloud_allowed ? "OPT-IN ENABLED" : "DISABLED", cloud_allowed ? RAZION_WARNING : RAZION_SUCCESS); y += 55;
	row(rx, y, rw, "Local Ollama provider", ollama_enabled ? "Adapter is configured; connection status requires a live health request." : "Adapter is not enabled in the provider policy.",
		ollama_enabled ? "CONFIGURED" : "NOT CONFIGURED", ollama_enabled ? RAZION_ACCENT : RAZION_TEXT_SECONDARY);

	label(x, top + 401, 12, "Recent AI activity", RAZION_TEXT_PRIMARY, 1);
	razion_draw_card(ctx, x, top + 416, width, 54, RAZION_SURFACE);
	char * activity = tt_ellipsify(audit_enabled ? recent_activity : "AI activity auditing is disabled by policy.", 9, font, width - 28, NULL);
	label(x + 14, top + 448, 9, activity, RAZION_TEXT_SECONDARY, 0);
	free(activity);

	button(1, x, top + 488, 170, "Refresh real status");
	button(2, x + 182, top + 488, 190, "Test AI providers");
	button(3, x + 384, top + 488, 180, "Open Quick Settings");
	label(x, window->height - bounds.bottom_height - 14, 8,
		"Revocation controls require a kernel permission broker and are not available in this release.", RAZION_TEXT_SECONDARY, 0);
	render_decorations(window, ctx, "Razion Privacy Center");
	flip(ctx); yutani_flip(yctx, window);
}

static int hit_test(int mx, int my) {
	struct decor_bounds b; decor_get_bounds(window, &b);
	int x = b.left_width + 28, y = b.top_height + 488;
	if (my < y || my >= y + 36) return -1;
	if (mx >= x && mx < x + 170) return 1;
	if (mx >= x + 182 && mx < x + 372) return 2;
	if (mx >= x + 384 && mx < x + 564) return 3;
	return -1;
}

static void activate(int id) {
	if (id == 1) refresh_status();
	else if (id == 2) launch("/bin/terminal", "razion-ai-status", "providers");
	else if (id == 3) launch("/bin/quick-settings", NULL, NULL);
}

int main(void) {
	yctx = yutani_init(); if (!yctx) return 1; init_decorations();
	struct decor_bounds bounds; decor_get_bounds(NULL, &bounds);
	window = yutani_window_create(yctx, WIDTH + bounds.width, HEIGHT + bounds.height); if (!window) return 1;
	yutani_window_move(yctx, window, (yctx->display_width-window->width)/2, (yctx->display_height-window->height)/2);
	yutani_window_advertise_icon(yctx, window, "Privacy Center", "razion-settings");
	ctx = init_graphics_yutani_double_buffer(window);
	font = tt_font_from_shm("sans-serif"); bold = tt_font_from_shm("sans-serif.bold");
	refresh_status(); redraw();
	while (running) {
		yutani_msg_t * message = yutani_poll(yctx); if (!message) continue;
		if (message->type == YUTANI_MSG_KEY_EVENT) {
			struct yutani_msg_key_event * key = (void *)message->data;
			if (key->wid == window->wid && key->event.action == KEY_ACTION_DOWN) {
				if (key->event.keycode == KEY_ESCAPE) running = 0;
				else if (key->event.keycode == '\t' || key->event.keycode == KEY_ARROW_RIGHT ||
					key->event.keycode == KEY_ARROW_DOWN) {
					focus_id = focus_id % 3 + 1; redraw();
				} else if (key->event.keycode == KEY_ARROW_LEFT || key->event.keycode == KEY_ARROW_UP) {
					focus_id = focus_id == 1 ? 3 : focus_id - 1; redraw();
				} else if (key->event.key == '\n') { activate(focus_id); redraw(); }
			}
		} else if (message->type == YUTANI_MSG_WINDOW_MOUSE_EVENT) {
			struct yutani_msg_window_mouse_event * mouse = (void *)message->data;
			if (mouse->wid == window->wid) {
				if (decor_handle_event(yctx, message) == DECOR_CLOSE) running = 0;
				int over = hit_test(mouse->new_x, mouse->new_y);
				if (mouse->command == YUTANI_MOUSE_EVENT_DOWN) { pressed = over; if (over > 0) focus_id = over; }
				else if (mouse->command == YUTANI_MOUSE_EVENT_LEAVE) { hover = -1; pressed = -1; }
				else if (mouse->command == YUTANI_MOUSE_EVENT_RAISE || mouse->command == YUTANI_MOUSE_EVENT_CLICK) {
					if (over >= 0 && over == pressed) activate(over);
					pressed = -1;
				}
				hover = over; redraw();
			}
		} else if (message->type == YUTANI_MSG_WINDOW_FOCUS_CHANGE) {
			struct yutani_msg_window_focus_change * focus = (void *)message->data;
			if (focus->wid == window->wid) { window->focused = focus->focused; redraw(); }
		} else if (message->type == YUTANI_MSG_WINDOW_CLOSE || message->type == YUTANI_MSG_SESSION_END) running = 0;
		free(message);
	}
	yutani_close(yctx, window); return 0;
}
