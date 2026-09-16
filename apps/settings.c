/** @brief RazionOS native settings control center. */
#include <errno.h>
#include <signal.h>
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

#define WINDOW_WIDTH 760
#define WINDOW_HEIGHT 500
#define SECTION_COUNT 6

typedef struct {
	int theme, accent;
	int dock_position, dock_size, dock_autohide, dock_compact;
} settings_state_t;

static const char * section_names[] = {"Personalization", "Desktop", "System", "Razion AI", "Privacy", "About"};
static const char * accent_names[] = {"blue", "teal", "violet", "orange", "rose"};
static const uint32_t accent_colors[] = {
	0xFF4EA3FF, 0xFF29C4B4, 0xFF9769F5, 0xFFEB9A48, 0xFFE25C84
};
static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font, * bold;
static settings_state_t state;
static int section, hover_id = -1, pressed_id = -1, focus_id = 100, running = 1;
static char status[128] = "Tab or Arrow keys navigate. Enter activates a control.";

static int clamp(int value, int low, int high) { return value < low ? low : value > high ? high : value; }

static int config_directory(char * output, size_t size) {
	const char * home = getenv("HOME");
	if (!home || snprintf(output, size, "%s/.razion", home) >= (int)size) return -1;
	return mkdir(output, 0700) && errno != EEXIST ? -1 : 0;
}

static void load_settings(void) {
	state.dock_size = 40;
	char directory[512], path[1024], line[128];
	if (config_directory(directory, sizeof(directory))) return;
	snprintf(path, sizeof(path), "%s/theme.conf", directory);
	FILE * file = fopen(path, "r");
	if (file) {
		while (fgets(line, sizeof(line), file)) {
			if (!strncmp(line, "theme=light", 11)) state.theme = 1;
			else if (!strncmp(line, "theme=auto", 10)) state.theme = 2;
			else if (!strncmp(line, "accent=", 7)) for (int i = 0; i < 5; ++i)
				if (!strncmp(line + 7, accent_names[i], strlen(accent_names[i]))) state.accent = i;
		}
		fclose(file);
	}
	snprintf(path, sizeof(path), "%s/desktop.conf", directory);
	file = fopen(path, "r");
	if (!file) return;
	while (fgets(line, sizeof(line), file)) {
		if (!strcmp(line, "dock_position=left\n")) state.dock_position = 1;
		else if (!strcmp(line, "dock_position=right\n")) state.dock_position = 2;
		else if (!strncmp(line, "dock_size=", 10)) state.dock_size = clamp(atoi(line + 10), 34, 64);
		else if (!strncmp(line, "dock_autohide=", 14)) state.dock_autohide = !!atoi(line + 14);
		else if (!strncmp(line, "dock_compact=", 13)) state.dock_compact = !!atoi(line + 13);
	}
	fclose(file);
}

static int write_theme(void) {
	char directory[512], path[1024];
	if (config_directory(directory, sizeof(directory))) return -1;
	snprintf(path, sizeof(path), "%s/theme.conf", directory);
	FILE * file = fopen(path, "w");
	if (!file) return -1;
	const char * theme = state.theme == 1 ? "light" : state.theme == 2 ? "auto" : "dark";
	fprintf(file, "theme=%s\naccent=%s\n", theme, accent_names[state.accent]);
	return fclose(file);
}

static void reload_dock(void) {
	char directory[512], path[1024];
	if (config_directory(directory, sizeof(directory))) return;
	snprintf(path, sizeof(path), "%s/dock.pid", directory);
	FILE * file = fopen(path, "r");
	if (!file) return;
	int pid;
	if (fscanf(file, "%d", &pid) == 1 && pid > 1) kill(pid, SIGUSR1);
	fclose(file);
}

static int write_desktop(void) {
	char directory[512], path[1024];
	if (config_directory(directory, sizeof(directory))) return -1;
	snprintf(path, sizeof(path), "%s/desktop.conf", directory);
	FILE * file = fopen(path, "w");
	if (!file) return -1;
	const char * position = state.dock_position == 1 ? "left" : state.dock_position == 2 ? "right" : "bottom";
	fprintf(file, "dock_position=%s\ndock_size=%d\ndock_autohide=%d\ndock_compact=%d\n",
		position, state.dock_size, state.dock_autohide, state.dock_compact);
	int result = fclose(file);
	if (!result) reload_dock();
	return result;
}

static void launch(const char * executable, const char * first, const char * second) {
	if (fork()) return;
	char * arguments[] = {(char *)executable, (char *)first, (char *)second, NULL};
	execv(executable, arguments);
	_Exit(127);
}

static void label(int x, int y, int size, const char * text, uint32_t color, int strong) {
	tt_set_size(strong ? bold : font, size);
	tt_draw_string(ctx, strong ? bold : font, x, y, text, color);
}

static void button(int id, int x, int y, int width, int height, const char * text, int selected) {
	uint32_t fill = selected || id == pressed_id ? RAZION_SELECTION : id == hover_id ? RAZION_SURFACE_HOVER : RAZION_SURFACE_SECONDARY;
	if (id == focus_id && window->focused)
		razion_draw_focus(ctx, x, y, width, height, 7);
	draw_rounded_rectangle(ctx, x, y, width, height, 7, fill);
	draw_rectangle_solid(ctx, x + 8, y + height - 1, width - 16, 1, selected ? RAZION_ACCENT : RAZION_BORDER);
	tt_set_size(font, 13);
	int tw = tt_string_width(font, text);
	tt_draw_string(ctx, font, x + (width - tw) / 2, y + height / 2 + 5, text, RAZION_TEXT_PRIMARY);
}

static void toggle(int id, int x, int y, const char * title, const char * detail, int enabled) {
	if (id == focus_id && window->focused)
		razion_draw_focus(ctx, x, y, 388, 46, 6);
	draw_rounded_rectangle(ctx, x, y, 388, 46, 6,
		id == pressed_id ? RAZION_SELECTION : id == hover_id ? RAZION_SURFACE_HOVER : RAZION_BACKGROUND);
	label(x, y + 18, 14, title, RAZION_TEXT_PRIMARY, 1);
	label(x, y + 38, 11, detail, RAZION_TEXT_SECONDARY, 0);
	draw_rounded_rectangle(ctx, x + 340, y + 7, 46, 24, 12, enabled ? RAZION_ACCENT : RAZION_SURFACE_HOVER);
	draw_rounded_rectangle(ctx, x + 343 + (enabled ? 22 : 0), y + 10, 18, 18, 9, RAZION_TEXT_PRIMARY);
	if (id == hover_id) draw_rounded_rectangle(ctx, x + 338, y + 5, 50, 28, 14, premultiply(rgba(255,255,255,28)));
}

static void header(int x, int top, const char * title, const char * detail) {
	razion_draw_accent_bar(ctx, x, top + 18, 96);
	label(x, top + 38, 25, title, RAZION_TEXT_PRIMARY, 1);
	label(x, top + 62, 12, detail, RAZION_TEXT_SECONDARY, 0);
	draw_rectangle_solid(ctx, x, top + 78, window->width - x - 28, 1, RAZION_BORDER);
}

static void personalization(int x, int top) {
	header(x, top, "Personalization", "Theme, accent, and wallpaper");
	label(x, top + 111, 13, "Theme", RAZION_TEXT_SECONDARY, 1);
	button(1, x, top + 128, 112, 38, "Dark", state.theme == 0);
	button(2, x + 120, top + 128, 112, 38, "Light", state.theme == 1);
	button(3, x + 240, top + 128, 112, 38, "Auto", state.theme == 2);
	label(x, top + 202, 13, "Accent", RAZION_TEXT_SECONDARY, 1);
	for (int i = 0; i < 5; ++i) {
		int sx = x + i * 50;
		if (focus_id == 10 + i && window->focused)
			draw_rounded_rectangle(ctx, sx - 6, top + 214, 40, 40, 20, RAZION_FOCUS);
		if (state.accent == i) draw_rounded_rectangle(ctx, sx - 4, top + 216, 36, 36, 18, RAZION_TEXT_PRIMARY);
		draw_rounded_rectangle(ctx, sx, top + 220, 28, 28, 14, accent_colors[i]);
	}
	label(x, top + 292, 13, "Wallpaper", RAZION_TEXT_SECONDARY, 1);
	button(20, x, top + 310, 230, 42, "Choose wallpaper…", 0);
	label(x, top + 378, 11, "Theme and accent apply to newly opened native applications.", RAZION_TEXT_SECONDARY, 0);
}

static void desktop(int x, int top) {
	header(x, top, "Desktop", "Dock behavior and launcher access");
	label(x, top + 111, 13, "Dock position", RAZION_TEXT_SECONDARY, 1);
	button(30, x, top + 128, 112, 38, "Bottom", state.dock_position == 0);
	button(31, x + 120, top + 128, 112, 38, "Left", state.dock_position == 1);
	button(32, x + 240, top + 128, 112, 38, "Right", state.dock_position == 2);
	label(x, top + 207, 14, "Icon size", RAZION_TEXT_PRIMARY, 1);
	char size[32]; snprintf(size, sizeof(size), "%d px", state.dock_size);
	button(33, x + 238, top + 183, 42, 38, "−", 0);
	button(34, x + 330, top + 183, 42, 38, "+", 0);
	label(x + 286, top + 207, 13, size, RAZION_TEXT_SECONDARY, 0);
	toggle(35, x, top + 244, "Auto-hide", "Reveal the dock at the display edge", state.dock_autohide);
	toggle(36, x, top + 304, "Compact mode", "Reduce spacing between dock icons", state.dock_compact);
	button(37, x, top + 382, 230, 42, "Open Razion Launcher", 0);
}

static void action_grid(int x, int top, const char * title, const char * detail, int first, const char * items[], int count) {
	header(x, top, title, detail);
	for (int i = 0; i < count; ++i) button(first + i, x + (i % 2) * 220,
		top + 112 + (i / 2) * 62, 204, 46, items[i], 0);
}

static void redraw(void) {
	struct decor_bounds bounds; decor_get_bounds(window, &bounds);
	draw_fill(ctx, RAZION_BACKGROUND);
	int top = bounds.top_height, sidebar = window->width < 740 ? 174 : 205;
	draw_rectangle_solid(ctx, bounds.left_width, top, sidebar, window->height - bounds.height, RAZION_SURFACE);
	razion_draw_accent_bar(ctx, bounds.left_width + 24, top + 22, 76);
	label(bounds.left_width + 24, top + 42, 18, "RAZIONOS", RAZION_ACCENT, 1);
	label(bounds.left_width + 24, top + 62, 10, "SYSTEM SETTINGS", RAZION_TEXT_SECONDARY, 1);
	for (int i = 0; i < SECTION_COUNT; ++i) {
		int y = top + 91 + i * 48;
		if (focus_id == 100 + i && window->focused)
			razion_draw_focus(ctx, bounds.left_width + 12, y, sidebar - 24, 38, 6);
		if (section == i || hover_id == 100 + i) draw_rounded_rectangle(ctx, bounds.left_width + 12, y,
			sidebar - 24, 38, 6, section == i ? RAZION_SELECTION : RAZION_SURFACE_HOVER);
		if (section == i) draw_rounded_rectangle(ctx, bounds.left_width + 12, y + 8, 3, 22, 2, RAZION_ACCENT);
		label(bounds.left_width + 28, y + 24, 13, section_names[i], section == i ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY, section == i);
	}
	label(bounds.left_width + 24, window->height - bounds.bottom_height - 28, 10, "RazionOS 0.1 Alpha", RAZION_TEXT_SECONDARY, 0);
	int x = bounds.left_width + sidebar + 28;
	if (section == 0) personalization(x, top);
	else if (section == 1) desktop(x, top);
	else if (section == 2) {
		const char * items[] = {"Quick Settings", "System Center", "File Manager", "Terminal",
			"Updates", "Screenshots", "Lock Screen", "System Monitor"};
		action_grid(x, top, "System", "Storage, updates, capture, lock, and native tools", 40, items, 8);
		label(x, top + 398, 11, "Only controls backed by current hardware and services are exposed.", RAZION_TEXT_SECONDARY, 0);
	} else if (section == 3) {
		const char * items[] = {"Assistant Panel", "AI File Search", "Open Razion Pulse", "AI Chat", "Provider Status"};
		action_grid(x, top, "Razion AI", "Optional local-first AI through the official engine", 50, items, 5);
		label(x, top + 322, 11, "No browsing data or files are sent without an explicit user action.", RAZION_TEXT_SECONDARY, 0);
	} else if (section == 4) {
		header(x, top, "Privacy", "Real system capability and AI policy status");
		button(70, x, top + 112, 250, 46, "Open Privacy Center", 0);
		label(x, top + 198, 11, "Unavailable permission brokers are reported explicitly, never simulated.", RAZION_TEXT_SECONDARY, 0);
	} else {
		header(x, top, "About RazionOS", "AI-native operating system");
		label(x, top + 126, 32, "R", RAZION_ACCENT, 1);
		label(x + 42, top + 116, 17, "RazionOS 0.1 Alpha", RAZION_TEXT_PRIMARY, 1);
		label(x + 42, top + 140, 11, "Native, lightweight, secure by design", RAZION_TEXT_SECONDARY, 0);
		button(60, x, top + 182, 230, 42, "Open system information", 0);
		label(x, top + 272, 11, "Built on ToaruOS components with preserved NCSA attribution.", RAZION_TEXT_SECONDARY, 0);
	}
	label(x, window->height - bounds.bottom_height - 22, 10, status, RAZION_TEXT_SECONDARY, 0);
	render_decorations(window, ctx, "RazionOS Settings"); flip(ctx); yutani_flip(yctx, window);
}

static int hit_test(int x, int y) {
	struct decor_bounds b; decor_get_bounds(window, &b);
	int top = b.top_height, sidebar = window->width < 740 ? 174 : 205;
	for (int i = 0; i < SECTION_COUNT; ++i) { int row = top + 91 + i * 48;
		if (x >= b.left_width + 12 && x < b.left_width + sidebar - 12 && y >= row && y < row + 38) return 100 + i; }
	int mx = b.left_width + sidebar + 28;
	if (section == 0) {
		for (int i = 0; i < 3; ++i) if (x >= mx + i*120 && x < mx + i*120 + 112 && y >= top+128 && y < top+166) return 1+i;
		for (int i = 0; i < 5; ++i) if (x >= mx+i*50-4 && x < mx+i*50+36 && y >= top+216 && y < top+252) return 10+i;
		if (x >= mx && x < mx+230 && y >= top+310 && y < top+352) return 20;
	} else if (section == 1) {
		for (int i = 0; i < 3; ++i) if (x >= mx+i*120 && x < mx+i*120+112 && y >= top+128 && y < top+166) return 30+i;
		if (x >= mx+238 && x < mx+280 && y >= top+183 && y < top+221) return 33;
		if (x >= mx+330 && x < mx+372 && y >= top+183 && y < top+221) return 34;
		if (x >= mx && x < mx+390 && y >= top+244 && y < top+292) return 35;
		if (x >= mx && x < mx+390 && y >= top+304 && y < top+352) return 36;
		if (x >= mx && x < mx+230 && y >= top+382 && y < top+424) return 37;
	} else if (section == 2 || section == 3) {
		int count = section == 2 ? 8 : 5, first = section == 2 ? 40 : 50;
		for (int i = 0; i < count; ++i) { int bx=mx+(i%2)*220, by=top+112+(i/2)*62;
			if (x>=bx && x<bx+204 && y>=by && y<by+46) return first+i; }
	} else if (section == 4) {
		if (x>=mx && x<mx+250 && y>=top+112 && y<top+158) return 70;
	} else if (x>=mx && x<mx+230 && y>=top+182 && y<top+224) return 60;
	return -1;
}

static int focusable_controls(int * ids, int capacity) {
	int count = 0;
#define ADD_FOCUS(value) do { if (count < capacity) ids[count++] = (value); } while (0)
	for (int i = 0; i < SECTION_COUNT; ++i) ADD_FOCUS(100 + i);
	if (section == 0) {
		for (int id = 1; id <= 3; ++id) ADD_FOCUS(id);
		for (int id = 10; id <= 14; ++id) ADD_FOCUS(id);
		ADD_FOCUS(20);
	} else if (section == 1) {
		for (int id = 30; id <= 37; ++id) ADD_FOCUS(id);
	} else if (section == 2) {
		for (int id = 40; id <= 47; ++id) ADD_FOCUS(id);
	} else if (section == 3) {
		for (int id = 50; id <= 54; ++id) ADD_FOCUS(id);
	} else if (section == 4) {
		ADD_FOCUS(70);
	} else {
		ADD_FOCUS(60);
	}
#undef ADD_FOCUS
	return count;
}

static void move_focus(int direction) {
	int ids[24];
	int count = focusable_controls(ids, 24);
	if (!count) return;
	int current = 0;
	for (int i = 0; i < count; ++i) {
		if (ids[i] == focus_id) { current = i; break; }
	}
	current = (current + direction + count) % count;
	focus_id = ids[current];
}

static void move_sidebar_focus(int direction) {
	int current = focus_id >= 100 && focus_id < 100 + SECTION_COUNT ? focus_id - 100 : section;
	current = (current + direction + SECTION_COUNT) % SECTION_COUNT;
	focus_id = 100 + current;
	section = current;
}

static void focus_first_section_control(void) {
	int ids[24];
	int count = focusable_controls(ids, 24);
	if (count > SECTION_COUNT) focus_id = ids[SECTION_COUNT];
}

static void activate(int id) {
	if (id >= 100 && id < 106) { section = id - 100; return; }
	if (id >= 1 && id <= 3) { state.theme=id-1; if (!write_theme()) snprintf(status,sizeof(status),"Theme saved. Reopen applications to apply it."); }
	else if (id >= 10 && id < 15) { state.accent=id-10; if (!write_theme()) snprintf(status,sizeof(status),"Accent saved. Reopen applications to apply it."); }
	else if (id == 20) launch("/bin/wallpaper-picker",NULL,NULL);
	else if (id >= 30 && id <= 32) { state.dock_position=id-30; write_desktop(); snprintf(status,sizeof(status),"Dock position updated."); }
	else if (id == 33 || id == 34) { state.dock_size=clamp(state.dock_size+(id==33?-4:4),34,64); write_desktop(); }
	else if (id == 35) { state.dock_autohide=!state.dock_autohide; write_desktop(); }
	else if (id == 36) { state.dock_compact=!state.dock_compact; write_desktop(); }
	else if (id == 37) launch("/bin/universal-search",NULL,NULL);
	else if (id == 40) launch("/bin/quick-settings",NULL,NULL);
	else if (id == 41) launch("/bin/razion-system-center",NULL,NULL);
	else if (id == 42) launch("/bin/file-browser",NULL,NULL);
	else if (id == 43) launch("/bin/terminal",NULL,NULL);
	else if (id == 44) launch("/bin/razion-system-center",NULL,NULL);
	else if (id == 45) launch("/bin/razion-system-center",NULL,NULL);
	else if (id == 46) launch("/bin/razion-lock",NULL,NULL);
	else if (id == 47) launch("/bin/cpuwidget",NULL,NULL);
	else if (id == 50) launch("/bin/razion-assistant",NULL,NULL);
	else if (id == 51) launch("/bin/razion-assistant","--files",NULL);
	else if (id == 52) launch("/bin/terminal","pulse",NULL);
	else if (id == 53) launch("/bin/terminal","razion-chat",NULL);
	else if (id == 54) launch("/bin/terminal","razion-ai-status","providers");
	else if (id == 70) launch("/bin/razion-privacy",NULL,NULL);
	else if (id == 60) launch("/bin/terminal","sysinfo",NULL);
}

static void resize_finish(unsigned int width, unsigned int height) {
	if (width < 720) width = 720;
	if (height < 500) height = 500;
	yutani_window_resize_accept(yctx,window,width,height); reinit_graphics_yutani(ctx,window);
	yutani_window_resize_done(yctx,window); redraw();
}

int main(void) {
	yctx=yutani_init(); if (!yctx) return 1; init_decorations();
	struct decor_bounds bounds; decor_get_bounds(NULL,&bounds);
	window=yutani_window_create(yctx,WINDOW_WIDTH+bounds.width,WINDOW_HEIGHT+bounds.height); if (!window) return 1;
	yutani_window_move(yctx,window,(yctx->display_width-window->width)/2,(yctx->display_height-window->height)/2);
	yutani_window_advertise_icon(yctx,window,"RazionOS Settings","razion-settings");
	ctx=init_graphics_yutani_double_buffer(window); font=tt_font_from_shm("sans-serif"); bold=tt_font_from_shm("sans-serif.bold");
	load_settings(); redraw();
	while (running) {
		yutani_msg_t * message=yutani_poll(yctx); if (!message) continue;
		switch (message->type) {
			case YUTANI_MSG_KEY_EVENT: { struct yutani_msg_key_event * key=(void*)message->data;
				if (key->wid != window->wid || key->event.action != KEY_ACTION_DOWN) break;
				int changed = 0;
				if (key->event.keycode == KEY_ESCAPE) running = 0;
				else if (key->event.keycode == '\t') {
					move_focus(key->event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT) ? -1 : 1);
					changed = 1;
				} else if (key->event.keycode == KEY_ARROW_UP && focus_id >= 100) {
					move_sidebar_focus(-1); changed = 1;
				} else if (key->event.keycode == KEY_ARROW_DOWN && focus_id >= 100) {
					move_sidebar_focus(1); changed = 1;
				} else if (key->event.keycode == KEY_ARROW_UP) {
					move_focus(-1); changed = 1;
				} else if (key->event.keycode == KEY_ARROW_RIGHT && focus_id >= 100) {
					focus_first_section_control(); changed = 1;
				} else if (key->event.keycode == KEY_ARROW_LEFT && focus_id < 100) {
					focus_id = 100 + section; changed = 1;
				} else if (key->event.keycode == KEY_ARROW_LEFT) {
					move_focus(-1); changed = 1;
				} else if (key->event.keycode == KEY_ARROW_RIGHT || key->event.keycode == KEY_ARROW_DOWN) {
					move_focus(1); changed = 1;
				} else if ((key->event.key == '\n' || key->event.key == ' ') && focus_id >= 0) {
					activate(focus_id); changed = 1;
				}
				if (changed) redraw();
				break; }
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: { struct yutani_msg_window_mouse_event * mouse=(void*)message->data;
				if (mouse->wid!=window->wid) break;
				int decor = decor_handle_event(yctx,message);
				if (decor==DECOR_CLOSE) running=0;
				int old_hover = hover_id, old_pressed = pressed_id, activated = 0;
				int over=hit_test(mouse->new_x,mouse->new_y); if (mouse->command==YUTANI_MOUSE_EVENT_DOWN) { pressed_id=over; if (over >= 0) focus_id=over; }
				else if (mouse->command==YUTANI_MOUSE_EVENT_LEAVE) { hover_id=-1; pressed_id=-1; }
				else if (mouse->command==YUTANI_MOUSE_EVENT_RAISE || mouse->command==YUTANI_MOUSE_EVENT_CLICK) { if (over>=0 && over==pressed_id) { activate(over); activated=1; } pressed_id=-1; }
				hover_id=over;
				if (decor == DECOR_REDRAW || old_hover != hover_id || old_pressed != pressed_id || activated) redraw();
				break; }
			case YUTANI_MSG_RESIZE_OFFER: { struct yutani_msg_window_resize * resize=(void*)message->data;
				if (resize->wid==window->wid) resize_finish(resize->width,resize->height);
				break; }
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE: { struct yutani_msg_window_focus_change * focus=(void*)message->data;
				if (focus->wid==window->wid) { window->focused=focus->focused; redraw(); } break; }
			case YUTANI_MSG_WINDOW_CLOSE: case YUTANI_MSG_SESSION_END: running=0;
		}
		free(message);
	}
	yutani_close(yctx,window); return 0;
}
