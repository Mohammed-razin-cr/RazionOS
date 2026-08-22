/**
 * @brief Razion Dock - lightweight native application dock.
 *
 * The dock launches a fixed catalogue of trusted first-party applications
 * and focuses an existing advertised window when one is already open.
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fswait.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <razion/theme.h>
#include <toaru/graphics.h>
#include <toaru/icon_cache.h>
#include <toaru/yutani.h>

#define APP_COUNT 12
#define DOCK_PAD 7
#define DOCK_GAP 6

typedef struct {
	const char * name;
	const char * icon;
	const char * executable;
	const char * argument;
	const char * title_match;
} dock_app_t;

typedef struct {
	int icon_size;
	int position; /* 0 bottom, 1 left, 2 right */
	int auto_hide;
	int compact;
} dock_config_t;

static const dock_app_t apps[APP_COUNT] = {
	{"Launcher", "razion-launcher", "/bin/universal-search", NULL, "Razion Launcher"},
	{"Files", "razion-files", "/bin/file-browser", NULL, "File Browser"},
	{"Ripper", "razion-ripper", "/bin/ripper", NULL, "Ripper"},
	{"Store", "razion-store", "/bin/razion-store", NULL, "Razion Store"},
	{"Terminal", "razion-terminal", "/bin/terminal", NULL, "Terminal"},
	{"Settings", "razion-settings", "/bin/settings", NULL, "RazionOS Settings"},
	{"Pulse", "razion-pulse", "/bin/terminal", "razion-chat", "Razion AI Chat"},
	{"Monitor", "razion-monitor", "/bin/cpuwidget", NULL, "System Monitor"},
	{"Notes", "razion-notes", "/bin/razion-notes", NULL, "Razion Notes"},
	{"Calendar", "razion-calendar", "/bin/razion-calendar", NULL, "Razion Calendar"},
	{"Media", "razion-media", "/bin/razion-media", NULL, "Razion Media Player"},
	{"Trash", "razion-trash", "/bin/file-browser", "~/.local/share/Trash/files", "Recycle Bin"},
};

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static dock_config_t config;
static int running = 1;
static int reload_requested;
static int hovered = -1;
static int pressed = -1;
static int active[APP_COUNT];

static int advertisement_matches(int index, const char * title, const char * icon) {
	if (apps[index].title_match) return strstr(title, apps[index].title_match) != NULL;
	return apps[index].icon && !strcmp(icon, apps[index].icon);
}

static int clamp(int value, int low, int high) {
	return value < low ? low : value > high ? high : value;
}

static void config_defaults(void) {
	config.icon_size = 40;
	config.position = 0;
	config.auto_hide = 0;
	config.compact = 0;
}

static void read_config(void) {
	config_defaults();
	const char * home = getenv("HOME");
	if (!home) return;
	char path[512];
	if (snprintf(path, sizeof(path), "%s/.razion/desktop.conf", home) >= (int)sizeof(path)) return;
	FILE * file = fopen(path, "r");
	if (!file) return;
	char line[128];
	while (fgets(line, sizeof(line), file)) {
		char * newline = strchr(line, '\n');
		if (newline) *newline = '\0';
		if (!strncmp(line, "dock_size=", 10)) config.icon_size = clamp(atoi(line + 10), 34, 64);
		else if (!strcmp(line, "dock_position=left")) config.position = 1;
		else if (!strcmp(line, "dock_position=right")) config.position = 2;
		else if (!strcmp(line, "dock_position=bottom")) config.position = 0;
		else if (!strncmp(line, "dock_autohide=", 14)) config.auto_hide = !!atoi(line + 14);
		else if (!strncmp(line, "dock_compact=", 13)) config.compact = !!atoi(line + 13);
	}
	fclose(file);
}

static int cell_size(void) {
	return config.icon_size + (config.compact ? 4 : DOCK_GAP);
}

static void desired_size(int * out_width, int * out_height) {
	int cell = cell_size();
	if (config.position == 0) {
		*out_width = APP_COUNT * cell + DOCK_PAD * 2;
		*out_height = config.icon_size + DOCK_PAD * 2 + 5;
	} else {
		*out_width = config.icon_size + DOCK_PAD * 2 + 5;
		*out_height = APP_COUNT * cell + DOCK_PAD * 2;
	}
}

static void position_window(int hidden) {
	int x, y;
	if (config.position == 0) {
		x = (yctx->display_width - (int)window->width) / 2;
		y = hidden ? yctx->display_height - 5 : yctx->display_height - window->height - 10;
	} else if (config.position == 1) {
		x = hidden ? 5 - (int)window->width : 8;
		y = (yctx->display_height - (int)window->height) / 2;
	} else {
		x = hidden ? yctx->display_width - 5 : yctx->display_width - window->width - 8;
		y = (yctx->display_height - (int)window->height) / 2;
	}
	yutani_window_move(yctx, window, x, y);
}

static void write_pid(void) {
	const char * home = getenv("HOME");
	if (!home) return;
	char directory[512], path[512];
	if (snprintf(directory, sizeof(directory), "%s/.razion", home) >= (int)sizeof(directory)) return;
	if (mkdir(directory, 0700) && errno != EEXIST) return;
	if (snprintf(path, sizeof(path), "%s/dock.pid", directory) >= (int)sizeof(path)) return;
	FILE * file = fopen(path, "w");
	if (!file) return;
	fprintf(file, "%d\n", getpid());
	fclose(file);
}

static void signal_reload(int signal_number) {
	(void)signal_number;
	reload_requested = 1;
	signal(SIGUSR1, signal_reload);
}

static int app_at(int x, int y) {
	int coordinate = config.position == 0 ? x : y;
	int index = (coordinate - DOCK_PAD) / cell_size();
	if (coordinate < DOCK_PAD || index < 0 || index >= APP_COUNT) return -1;
	return index;
}

static void redraw(void) {
	draw_fill(ctx, 0);
	draw_rounded_rectangle(ctx, 1, 1, ctx->width - 2, ctx->height - 2,
		12, premultiply(rgba(14, 20, 28, 232)));
	draw_rounded_rectangle(ctx, 1, 1, ctx->width - 2, ctx->height - 2,
		12, premultiply(rgba(65, 82, 98, 80)));
	draw_rounded_rectangle(ctx, 2, 2, ctx->width - 4, ctx->height - 4,
		11, premultiply(rgba(17, 23, 31, 238)));

	int cell = cell_size();
	for (int i = 0; i < APP_COUNT; ++i) {
		int x = config.position == 0 ? DOCK_PAD + i * cell : DOCK_PAD;
		int y = config.position == 0 ? DOCK_PAD : DOCK_PAD + i * cell;
		if (i == hovered || i == pressed) {
			draw_rounded_rectangle(ctx, x - 2, y - 2, config.icon_size + 4,
				config.icon_size + 4, 8, i == pressed ? RAZION_SELECTION : RAZION_SURFACE_HOVER);
		}
		sprite_t * icon = icon_get_48(apps[i].icon);
		int grow = i == hovered && !config.compact ? 4 : 0;
		draw_sprite_scaled_alpha(ctx, icon, x - grow / 2, y - grow / 2,
			config.icon_size + grow, config.icon_size + grow, active[i] ? 1.0 : 0.94);
		if (active[i]) {
			if (config.position == 0) draw_rounded_rectangle(ctx, x + config.icon_size / 2 - 4,
				ctx->height - 6, 8, 3, 2, RAZION_ACCENT);
			else if (config.position == 1) draw_rounded_rectangle(ctx, ctx->width - 6,
				y + config.icon_size / 2 - 4, 3, 8, 2, RAZION_ACCENT);
			else draw_rounded_rectangle(ctx, 3, y + config.icon_size / 2 - 4,
				3, 8, 2, RAZION_ACCENT);
		}
	}
	flip(ctx);
	yutani_flip(yctx, window);
}

static void update_active_windows(void) {
	memset(active, 0, sizeof(active));
	yutani_query_windows(yctx);
	while (1) {
		yutani_msg_t * message = yutani_wait_for(yctx, YUTANI_MSG_WINDOW_ADVERTISE);
		struct yutani_msg_window_advertise * ad = (void *)message->data;
		if (!ad->size) {
			free(message);
			break;
		}
		const char * title = ad->strings;
		const char * icon = &ad->strings[ad->icon];
		for (int i = 0; i < APP_COUNT; ++i) {
			if (advertisement_matches(i, title, icon)) active[i] = 1;
		}
		free(message);
	}
}

static yutani_wid_t find_existing(int index) {
	yutani_wid_t found = 0;
	yutani_query_windows(yctx);
	while (1) {
		yutani_msg_t * message = yutani_wait_for(yctx, YUTANI_MSG_WINDOW_ADVERTISE);
		struct yutani_msg_window_advertise * ad = (void *)message->data;
		if (!ad->size) {
			free(message);
			break;
		}
		const char * title = ad->strings;
		const char * icon = &ad->strings[ad->icon];
		if (advertisement_matches(index, title, icon)) found = ad->wid;
		free(message);
	}
	return found;
}

static void launch_app(int index) {
	yutani_wid_t existing = find_existing(index);
	if (existing) {
		yutani_focus_window(yctx, existing);
		return;
	}
	if (fork()) return;
	char expanded[512];
	const char * argument = apps[index].argument;
	if (argument && argument[0] == '~') {
		const char * home = getenv("HOME");
		snprintf(expanded, sizeof(expanded), "%s%s", home ? home : "", argument + 1);
		argument = expanded;
	}
	char * arguments[] = {(char *)apps[index].executable, (char *)argument, NULL};
	execv(arguments[0], arguments);
	_Exit(127);
}

static void resize_finish(unsigned int new_width, unsigned int new_height) {
	yutani_window_resize_accept(yctx, window, new_width, new_height);
	reinit_graphics_yutani(ctx, window);
	yutani_window_resize_done(yctx, window);
	yutani_window_update_shape(yctx, window, YUTANI_SHAPE_THRESHOLD_CLEAR);
	position_window(0);
	redraw();
}

static void reload_config(void) {
	read_config();
	int new_width, new_height;
	desired_size(&new_width, &new_height);
	yutani_window_resize(yctx, window, new_width, new_height);
	position_window(0);
	reload_requested = 0;
}

int main(void) {
	yctx = yutani_init();
	if (!yctx) return 1;
	read_config();
	int dock_width, dock_height;
	desired_size(&dock_width, &dock_height);
	window = yutani_window_create_flags(yctx, dock_width, dock_height,
		YUTANI_WINDOW_FLAG_NO_STEAL_FOCUS | YUTANI_WINDOW_FLAG_DISALLOW_DRAG |
		YUTANI_WINDOW_FLAG_DISALLOW_RESIZE | YUTANI_WINDOW_FLAG_ALT_ANIMATION);
	if (!window) return 1;
	yutani_window_update_shape(yctx, window, YUTANI_SHAPE_THRESHOLD_CLEAR);
	yutani_set_stack(yctx, window, YUTANI_ZORDER_OVERLAY);
	position_window(0);
	ctx = init_graphics_yutani_double_buffer(window);
	write_pid();
	signal(SIGUSR1, signal_reload);
	yutani_subscribe_windows(yctx);
	update_active_windows();
	redraw();

	while (running) {
		if (reload_requested) reload_config();
		waitpid(-1, NULL, WNOHANG);
		int fds[1] = {fileno(yctx->sock)};
		if (fswait2(1, fds, 200) < 0) continue;
		yutani_msg_t * message = yutani_poll(yctx);
		if (!message) continue;
		switch (message->type) {
			case YUTANI_MSG_NOTIFY:
				update_active_windows();
				redraw();
				break;
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)message->data;
				if (mouse->wid != window->wid) break;
				int over = app_at(mouse->new_x, mouse->new_y);
				if (mouse->command == YUTANI_MOUSE_EVENT_ENTER) position_window(0);
				if (mouse->command == YUTANI_MOUSE_EVENT_LEAVE) {
					hovered = -1;
					pressed = -1;
					if (config.auto_hide) position_window(1);
				} else if (mouse->command == YUTANI_MOUSE_EVENT_DOWN && over >= 0) {
					pressed = over;
				} else if ((mouse->command == YUTANI_MOUSE_EVENT_CLICK ||
					mouse->command == YUTANI_MOUSE_EVENT_RAISE) && pressed >= 0) {
					if (pressed == over) launch_app(pressed);
					pressed = -1;
				}
				hovered = over;
				redraw();
				break;
			}
			case YUTANI_MSG_RESIZE_OFFER: {
				struct yutani_msg_window_resize * resize = (void *)message->data;
				if (resize->wid == window->wid) resize_finish(resize->width, resize->height);
				break;
			}
			case YUTANI_MSG_WELCOME:
				position_window(0);
				break;
			case YUTANI_MSG_SESSION_END:
				running = 0;
				break;
		}
		free(message);
	}
	yutani_unsubscribe_windows(yctx);
	yutani_close(yctx, window);
	return 0;
}
