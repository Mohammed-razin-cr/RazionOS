/** @brief Razion System Center - persistence, updates, notifications, screenshots. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define WIDTH 760
#define HEIGHT 560

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font, * bold;
static int running = 1, hover = -1, pressed = -1, focus = 1, dnd;
static char persistence[80] = "unknown";
static char status[160] = "System Center ready.";

static int ensure_config(char * directory, size_t size) {
	const char * home = getenv("HOME");
	if (!home || snprintf(directory, size, "%s/.razion", home) >= (int)size) return -1;
	return mkdir(directory, 0700) && errno != EEXIST ? -1 : 0;
}

static void load_state(void) {
	FILE * file = fopen("/tmp/razion-persistence-status", "r");
	if (file) {
		if (!fgets(persistence, sizeof(persistence), file)) strcpy(persistence, "unknown");
		fclose(file);
		char * newline = strchr(persistence, '\n');
		if (newline) *newline = '\0';
	}
	char directory[512], path[1024], line[64];
	if (ensure_config(directory, sizeof(directory))) return;
	snprintf(path, sizeof(path), "%s/notifications.conf", directory);
	file = fopen(path, "r");
	if (!file) return;
	while (fgets(line, sizeof(line), file)) if (!strncmp(line, "dnd=1", 5)) dnd = 1;
	fclose(file);
}

static void write_dnd(void) {
	char directory[512], path[1024];
	if (ensure_config(directory, sizeof(directory))) return;
	snprintf(path, sizeof(path), "%s/notifications.conf", directory);
	FILE * file = fopen(path, "w");
	if (!file) return;
	fprintf(file, "dnd=%d\n", dnd);
	fclose(file);
}

static void spawn4(const char * executable, const char * a, const char * b, const char * c) {
	if (fork()) return;
	char * args[] = {(char *)executable, (char *)a, (char *)b, (char *)c, NULL};
	execv(executable, args);
	_Exit(127);
}

static void label(struct TT_Font * face, int x, int y, int size,
	const char * text, uint32_t color) {
	tt_set_size(face, size);
	tt_draw_string(ctx, face, x, y, text, color);
}

static void small_text(int x, int y, int width, const char * text) {
	char * shown = tt_ellipsify(text, 11, font, width, NULL);
	label(font, x, y, 11, shown, RAZION_TEXT_SECONDARY);
	free(shown);
}

static void button(int id, int x, int y, int w, const char * text) {
	uint32_t fill = id == pressed ? RAZION_SELECTION :
		id == hover ? RAZION_SURFACE_HOVER : RAZION_SURFACE_SECONDARY;
	if (id == focus && window->focused) razion_draw_focus(ctx, x, y, w, 42, 8);
	draw_rounded_rectangle(ctx, x, y, w, 42, 8, fill);
	tt_set_size(font, 12);
	int tw = tt_string_width(font, text);
	tt_draw_string(ctx, font, x + (w - tw) / 2, y + 26, text, RAZION_TEXT_PRIMARY);
}

static void card(int x, int y, int w, int h, const char * title,
	const char * detail, uint32_t accent) {
	razion_draw_card(ctx, x, y, w, h, RAZION_SURFACE);
	draw_rectangle_solid(ctx, x + 12, y + h - 1, w - 24, 1, accent);
	label(bold, x + 16, y + 28, 15, title, RAZION_TEXT_PRIMARY);
	small_text(x + 16, y + 51, w - 32, detail);
}

static void redraw(void) {
	struct decor_bounds b;
	decor_get_bounds(window, &b);
	draw_fill(ctx, RAZION_BACKGROUND);
	int left = b.left_width + 30;
	int top = b.top_height;

	razion_draw_accent_bar(ctx, left, top + 24, 112);
	label(bold, left, top + 42, 25, "Razion System Center", RAZION_TEXT_PRIMARY);
	label(font, left, top + 66, 11,
		"Storage, updates, notifications, screenshots, lock screen, and system identity.",
		RAZION_TEXT_SECONDARY);
	tt_set_size(font, 10);
	razion_draw_chip(ctx, font, left + 540, top + 33,
		!strcmp(persistence, "ready") ? "Persistent" : "Live session",
		!strcmp(persistence, "ready") ? RAZION_SUCCESS : RAZION_WARNING);

	char persistence_detail[160];
	snprintf(persistence_detail, sizeof(persistence_detail),
		"Persistent home status: %s. Create a disk with make persistent-home.", persistence);
	card(left, top + 94, 332, 92, "Persistent Storage", persistence_detail,
		!strcmp(persistence, "ready") ? RAZION_SUCCESS : RAZION_WARNING);
	button(1, left + 16, top + 204, 142, "Open Home");
	button(2, left + 170, top + 204, 142, "Show Guide");

	card(left + 362, top + 94, 332, 92, "Updates", "RazionOS 0.1 Alpha uses local milestone builds and changelog review.", RAZION_ACCENT);
	button(3, left + 378, top + 204, 142, "Changelog");
	button(4, left + 532, top + 204, 142, "Store");

	card(left, top + 266, 332, 92, "Notifications", dnd ? "Do Not Disturb is enabled." : "Notifications are enabled through toastd.", dnd ? RAZION_WARNING : RAZION_SUCCESS);
	button(5, left + 16, top + 376, 142, dnd ? "Enable Toasts" : "Pause Toasts");
	button(6, left + 170, top + 376, 142, "Test Toast");

	card(left + 362, top + 266, 332, 92, "Screenshot Tool", "Capture the full screen, focused window, or selected region.", RAZION_ACCENT);
	button(7, left + 378, top + 376, 92, "Full");
	button(8, left + 480, top + 376, 92, "Window");
	button(9, left + 582, top + 376, 92, "Region");

	button(10, left, top + 452, 160, "Lock Screen");
	button(11, left + 174, top + 452, 160, "About RazionOS");
	button(12, left + 348, top + 452, 160, "Boot Status");
	button(13, left + 522, top + 452, 172, "System Info");

	label(font, left, window->height - b.bottom_height - 22, 10, status, RAZION_TEXT_SECONDARY);
	render_decorations(window, ctx, "Razion System Center");
	flip(ctx);
	yutani_flip(yctx, window);
}

static int hit(int x, int y) {
	struct decor_bounds b;
	decor_get_bounds(window, &b);
	int left = b.left_width + 30;
	int top = b.top_height;
	struct { int id, x, y, w; } buttons[] = {
		{1, left + 16, top + 204, 142}, {2, left + 170, top + 204, 142},
		{3, left + 378, top + 204, 142}, {4, left + 532, top + 204, 142},
		{5, left + 16, top + 376, 142}, {6, left + 170, top + 376, 142},
		{7, left + 378, top + 376, 92}, {8, left + 480, top + 376, 92},
		{9, left + 582, top + 376, 92}, {10, left, top + 452, 160},
		{11, left + 174, top + 452, 160}, {12, left + 348, top + 452, 160},
		{13, left + 522, top + 452, 172},
	};
	for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
		if (x >= buttons[i].x && x < buttons[i].x + buttons[i].w &&
			y >= buttons[i].y && y < buttons[i].y + 42) return buttons[i].id;
	}
	return -1;
}

static void move_focus(int direction) {
	focus += direction;
	if (focus < 1) focus = 13;
	if (focus > 13) focus = 1;
}

static void activate(int id) {
	if (id == 1) spawn4("/bin/file-browser", getenv("HOME"), NULL, NULL);
	else if (id == 2) snprintf(status, sizeof(status),
		"Persistent storage: attach a raw ext2 disk as /dev/hda; the startup hook mounts it on /home.");
	else if (id == 3) snprintf(status, sizeof(status),
		"Updates: this alpha uses rebuildable milestone images; review CHANGELOG.md in the source repo.");
	else if (id == 4) spawn4("/bin/razion-store", NULL, NULL, NULL);
	else if (id == 5) { dnd = !dnd; write_dnd(); snprintf(status, sizeof(status), "Notification preference saved."); }
	else if (id == 6) spawn4("/bin/toast.krk", "--icon", "/usr/share/icons/48/help.png", "RazionOS notification test.");
	else if (id == 7) spawn4("/bin/yutani-screenshot", NULL, NULL, NULL);
	else if (id == 8) spawn4("/bin/yutani-screenshot", "--window", NULL, NULL);
	else if (id == 9) spawn4("/bin/yutani-screenshot", "--select", NULL, NULL);
	else if (id == 10) spawn4("/bin/razion-lock", NULL, NULL, NULL);
	else if (id == 11) spawn4("/bin/about", NULL, NULL, NULL);
	else if (id == 12) snprintf(status, sizeof(status),
		"Boot transition: razion-splash receives real startup milestones through /dev/pex/splash.");
	else if (id == 13) spawn4("/bin/terminal", "sysinfo", NULL, NULL);
	load_state();
}

int main(void) {
	yctx = yutani_init();
	if (!yctx) return 1;
	init_decorations();
	struct decor_bounds b;
	decor_get_bounds(NULL, &b);
	window = yutani_window_create(yctx, WIDTH + b.width, HEIGHT + b.height);
	if (!window) return 1;
	yutani_window_move(yctx, window,
		(yctx->display_width - window->width) / 2,
		(yctx->display_height - window->height) / 2);
	yutani_window_advertise_icon(yctx, window, "Razion System Center", "razion-settings");
	ctx = init_graphics_yutani_double_buffer(window);
	font = tt_font_from_shm("sans-serif");
	bold = tt_font_from_shm("sans-serif.bold");
	load_state();
	redraw();
	while (running) {
		yutani_msg_t * msg = yutani_poll(yctx);
		if (!msg) continue;
		switch (msg->type) {
			case YUTANI_MSG_KEY_EVENT: {
				struct yutani_msg_key_event * key = (void *)msg->data;
				if (key->wid != window->wid || key->event.action != KEY_ACTION_DOWN) break;
				if (key->event.keycode == KEY_ESCAPE) running = 0;
				else if (key->event.keycode == '\t') { move_focus(key->event.modifiers & (KEY_MOD_LEFT_SHIFT|KEY_MOD_RIGHT_SHIFT) ? -1 : 1); redraw(); }
				else if (key->event.keycode == KEY_ARROW_LEFT || key->event.keycode == KEY_ARROW_UP) { move_focus(-1); redraw(); }
				else if (key->event.keycode == KEY_ARROW_RIGHT || key->event.keycode == KEY_ARROW_DOWN) { move_focus(1); redraw(); }
				else if (key->event.key == '\n' || key->event.key == ' ') { activate(focus); redraw(); }
				break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)msg->data;
				if (mouse->wid != window->wid) break;
				int decor = decor_handle_event(yctx, msg);
				if (decor == DECOR_CLOSE) running = 0;
				int old_hover = hover, old_pressed = pressed;
				int over = hit(mouse->new_x, mouse->new_y);
				if (mouse->command == YUTANI_MOUSE_EVENT_DOWN) { pressed = over; if (over > 0) focus = over; }
				else if (mouse->command == YUTANI_MOUSE_EVENT_LEAVE) hover = pressed = -1;
				else if (mouse->command == YUTANI_MOUSE_EVENT_RAISE ||
					mouse->command == YUTANI_MOUSE_EVENT_CLICK) {
					if (over >= 0 && over == pressed) activate(over);
					pressed = -1;
				}
				hover = over;
				if (decor == DECOR_REDRAW || old_hover != hover || old_pressed != pressed) redraw();
				break;
			}
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE: {
				struct yutani_msg_window_focus_change * focus = (void *)msg->data;
				if (focus->wid == window->wid) { window->focused = focus->focused; redraw(); }
				break;
			}
			case YUTANI_MSG_WINDOW_CLOSE:
			case YUTANI_MSG_SESSION_END:
				running = 0;
		}
		free(msg);
	}
	yutani_close(yctx, window);
	return 0;
}
