/**
 * @brief Razion Universal Search.
 *
 * Searches a fixed catalogue of installed capabilities plus bounded visible
 * file metadata below the current user's home directory. No AI provider or
 * network service is required.
 */
#include <ctype.h>
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
#include <toaru/razion_search.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define WINDOW_WIDTH 760
#define WINDOW_HEIGHT 530
#define ITEM_CAPACITY 2048
#define RESULT_CAPACITY 7
#define QUERY_CAPACITY 128
#define SEARCH_DEPTH 8

typedef struct {
	const char * executable;
	const char * argument1;
	const char * argument2;
	const char * argument3;
} search_action_t;

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font;
static struct TT_Font * font_bold;
static razion_search_item_t items[ITEM_CAPACITY];
static search_action_t actions[ITEM_CAPACITY];
static size_t item_count;
static size_t fixed_count;
static size_t results[RESULT_CAPACITY];
static size_t result_count;
static size_t selected;
static char query[QUERY_CAPACITY];
static size_t query_length;
static razion_search_stats_t search_stats;
static int running = 1;

static void copy_string(char * destination, size_t size, const char * source) {
	if (!size) return;
	if (!source) source = "";
	size_t length = strlen(source);
	if (length >= size) length = size - 1;
	memcpy(destination, source, length);
	destination[length] = '\0';
}

static void add_fixed(
	uint16_t kind,
	const char * name,
	const char * detail,
	const char * executable,
	const char * argument1,
	const char * argument2,
	const char * argument3) {
	if (item_count >= ITEM_CAPACITY) return;
	razion_search_item_t * item = &items[item_count];
	memset(item, 0, sizeof(*item));
	item->kind = kind;
	copy_string(item->name, sizeof(item->name), name);
	copy_string(item->detail, sizeof(item->detail), detail);
	copy_string(item->target, sizeof(item->target), executable);
	actions[item_count].executable = executable;
	actions[item_count].argument1 = argument1;
	actions[item_count].argument2 = argument2;
	actions[item_count].argument3 = argument3;
	item_count++;
}

static void load_catalogue(void) {
	add_fixed(RAZION_SEARCH_KIND_APPLICATION, "File Manager",
		"Browse files and folders", "/bin/file-browser", NULL, NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_APPLICATION, "Terminal",
		"Command-line environment", "/bin/terminal", NULL, NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_APPLICATION, "Calculator",
		"Native calculator", "/bin/calculator", NULL, NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_APPLICATION, "Razion Pulse",
		"Natural-language system interface", "/bin/terminal", "pulse", NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_APPLICATION, "Razion AI Chat",
		"Optional provider-independent AI client", "/bin/terminal", "razion-chat", NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_APPLICATION, "Help Browser",
		"Local system documentation", "/bin/help-browser", NULL, NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_SETTING, "Settings",
		"Implemented appearance, desktop, system, and AI controls", "/bin/settings", NULL, NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_SETTING, "Wallpaper",
		"Choose wallpaper and placement", "/bin/wallpaper-picker", NULL, NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_SETTING, "Desktop Files",
		"Open the Desktop folder", "/bin/file-browser", NULL, NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_SYSTEM_TOOL, "System Monitor",
		"Live CPU, memory, and network activity", "/bin/cpuwidget", NULL, NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_SYSTEM_TOOL, "System Information",
		"Kernel and platform information", "/bin/terminal", "sysinfo", NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_SYSTEM_TOOL, "AI Provider Status",
		"Real provider health and latency", "/bin/terminal", "razion-ai-status", "providers", NULL);
	add_fixed(RAZION_SEARCH_KIND_SYSTEM_TOOL, "About RazionOS",
		"Version, origin, and license information", "/bin/about", NULL, NULL, NULL);
	add_fixed(RAZION_SEARCH_KIND_SYSTEM_TOOL, "Package Manager",
		"Existing package management interface", "/bin/gsudo", "package-manager", NULL, NULL);
	fixed_count = item_count;

	const char * home = getenv("HOME");
	if (home && *home) {
		char * canonical = realpath(home, NULL);
		if (canonical && strlen(canonical) < RAZION_SEARCH_TARGET_MAX) {
			int collected = razion_search_collect(canonical, &items[item_count],
				ITEM_CAPACITY - item_count, SEARCH_DEPTH, &search_stats);
			if (collected > 0) item_count += collected;
		}
		free(canonical);
	}

	/* Fill in the only catalogue action whose target depends on HOME. */
	if (fixed_count > 8) {
		const char * home = getenv("HOME");
		static char desktop[RAZION_SEARCH_TARGET_MAX];
		if (home && snprintf(desktop, sizeof(desktop), "%s/Desktop", home) > 0) {
			actions[8].argument1 = desktop;
		}
	}
}

static void rebuild_results(void) {
	if (!query[0]) {
		result_count = fixed_count < RESULT_CAPACITY ? fixed_count : RESULT_CAPACITY;
		for (size_t i = 0; i < result_count; ++i) results[i] = i;
	} else {
		result_count = razion_search_rank(items, item_count, query,
			results, RESULT_CAPACITY);
	}
	if (!result_count) selected = 0;
	else if (selected >= result_count) selected = result_count - 1;
}

static int has_extension(const char * path, const char * extension) {
	size_t path_length = strlen(path);
	size_t extension_length = strlen(extension);
	if (path_length < extension_length) return 0;
	path += path_length - extension_length;
	while (*path && *extension) {
		if (tolower((unsigned char)*path) != tolower((unsigned char)*extension)) return 0;
		path++;
		extension++;
	}
	return !*path && !*extension;
}

static void spawn_program(
	const char * executable,
	const char * argument1,
	const char * argument2,
	const char * argument3) {
	pid_t child = fork();
	if (child != 0) return;
	char * arguments[] = {
		(char *)executable,
		(char *)argument1,
		(char *)argument2,
		(char *)argument3,
		NULL
	};
	execv(executable, arguments);
	_Exit(127);
}

static void open_selected(void) {
	if (!result_count || selected >= result_count) return;
	size_t index = results[selected];
	razion_search_item_t * item = &items[index];
	if (index < fixed_count) {
		search_action_t * action = &actions[index];
		spawn_program(action->executable, action->argument1,
			action->argument2, action->argument3);
		return;
	}
	if (item->kind == RAZION_SEARCH_KIND_DIRECTORY) {
		spawn_program("/bin/file-browser", item->target, NULL, NULL);
	} else if (has_extension(item->target, ".png") ||
		has_extension(item->target, ".jpg") ||
		has_extension(item->target, ".jpeg") ||
		has_extension(item->target, ".bmp") ||
		has_extension(item->target, ".tga")) {
		spawn_program("/bin/imgviewer", item->target, NULL, NULL);
	} else if (has_extension(item->target, ".pdf")) {
		spawn_program("/bin/maybe-pdfviewer.krk", item->target, NULL, NULL);
	} else {
		spawn_program("/bin/terminal", "bim", item->target, NULL);
	}
}

static void draw_text_ellipsized(
	struct TT_Font * chosen_font,
	int size,
	int x,
	int y,
	const char * text,
	int maximum_width,
	uint32_t color) {
	char * cropped = tt_ellipsify(text, size, chosen_font, maximum_width, NULL);
	tt_draw_string(ctx, chosen_font, x, y, cropped, color);
	free(cropped);
}

static void redraw(void) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	draw_fill(ctx, RAZION_BACKGROUND);

	int left = bounds.left_width + 34;
	int usable = window->width - bounds.width - 68;
	tt_set_size(font_bold, 24);
	tt_draw_string(ctx, font_bold, left, bounds.top_height + 42,
		"Universal Search", RAZION_TEXT_PRIMARY);
	tt_set_size(font, 12);
	tt_draw_string(ctx, font, left, bounds.top_height + 64,
		"Offline applications, settings, tools, files, and folders", RAZION_TEXT_SECONDARY);

	int input_y = bounds.top_height + 84;
	draw_rounded_rectangle(ctx, left, input_y, usable, 50, 7, RAZION_SURFACE);
	draw_rectangle_solid(ctx, left, input_y + 49, usable, 1, RAZION_FOCUS);
	tt_set_size(font, 18);
	const char * shown = query[0] ? query : "Type to search...";
	draw_text_ellipsized(font, 18, left + 18, input_y + 32, shown,
		usable - 36, query[0] ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY);
	if (window->focused && query[0]) {
		int cursor_x = tt_string_width(font, query);
		if (cursor_x < usable - 42) {
			draw_rectangle_solid(ctx, left + 18 + cursor_x, input_y + 11,
				2, 25, RAZION_FOCUS);
		}
	}

	int result_y = input_y + 66;
	for (size_t row = 0; row < result_count; ++row) {
		razion_search_item_t * item = &items[results[row]];
		int y = result_y + row * 48;
		if (row == selected) {
			draw_rounded_rectangle(ctx, left, y, usable, 43, 6, RAZION_SELECTION);
		}
		tt_set_size(font_bold, 14);
		draw_text_ellipsized(font_bold, 14, left + 14, y + 18,
			item->name, usable - 180, RAZION_TEXT_PRIMARY);
		tt_set_size(font, 11);
		char detail[RAZION_SEARCH_DETAIL_MAX + RAZION_SEARCH_TARGET_MAX + 8];
		if (item->kind == RAZION_SEARCH_KIND_FILE ||
			item->kind == RAZION_SEARCH_KIND_DIRECTORY) {
			snprintf(detail, sizeof(detail), "%s - %s", item->detail, item->target);
		} else {
			copy_string(detail, sizeof(detail), item->detail);
		}
		draw_text_ellipsized(font, 11, left + 14, y + 36, detail,
			usable - 28, RAZION_TEXT_SECONDARY);

		const char * kind = razion_search_kind_string(item->kind);
		tt_set_size(font, 10);
		int kind_width = tt_string_width(font, kind);
		tt_draw_string(ctx, font, left + usable - kind_width - 14, y + 17,
			kind, RAZION_TEXT_SECONDARY);
	}
	if (!result_count) {
		tt_set_size(font, 14);
		tt_draw_string(ctx, font, left + 14, result_y + 26,
			"No local results. Try a shorter name or path.", RAZION_TEXT_SECONDARY);
	}

	char status[192];
	snprintf(status, sizeof(status),
		"%zu searchable items%s  |  Enter opens  |  Esc closes",
		item_count, search_stats.truncated ? " (limit reached)" : "");
	tt_set_size(font, 11);
	tt_draw_string(ctx, font, left,
		window->height - bounds.bottom_height - 18, status, RAZION_TEXT_SECONDARY);

	render_decorations(window, ctx, "Razion Universal Search");
	flip(ctx);
	yutani_flip(yctx, window);
}

static int run_text_query(const char * text) {
	load_catalogue();
	size_t found[32];
	size_t count = razion_search_rank(items, item_count, text,
		found, sizeof(found) / sizeof(found[0]));
	for (size_t i = 0; i < count; ++i) {
		razion_search_item_t * item = &items[found[i]];
		printf("%s\t%s\t%s\n", razion_search_kind_string(item->kind),
			item->name, item->target);
	}
	return count ? 0 : 1;
}

int main(int argc, char * argv[]) {
	if (argc == 3 && !strcmp(argv[1], "--query")) {
		return run_text_query(argv[2]);
	}
	if (argc == 2 && !strcmp(argv[1], "--help")) {
		puts("usage: universal-search [--query text | --recent-pdf]");
		return 0;
	}
	if (argc > 1 && strcmp(argv[1], "--recent-pdf")) {
		fprintf(stderr, "universal-search: unknown option: %s\n", argv[1]);
		return 2;
	}

	load_catalogue();
	if (argc == 2) {
		copy_string(query, sizeof(query), "pdf");
		query_length = strlen(query);
	}
	rebuild_results();

	yctx = yutani_init();
	if (!yctx) return 1;
	init_decorations();
	struct decor_bounds bounds;
	decor_get_bounds(NULL, &bounds);
	window = yutani_window_create(yctx,
		WINDOW_WIDTH + bounds.width, WINDOW_HEIGHT + bounds.height);
	if (!window) return 1;
	yutani_window_move(yctx, window,
		yctx->display_width / 2 - window->width / 2,
		yctx->display_height / 2 - window->height / 2);
	yutani_window_advertise_icon(yctx, window,
		"Razion Universal Search", "applications-generic");
	ctx = init_graphics_yutani_double_buffer(window);
	font = tt_font_from_shm("sans-serif");
	font_bold = tt_font_from_shm("sans-serif.bold");
	redraw();

	while (running) {
		yutani_msg_t * event = yutani_poll(yctx);
		if (!event) continue;
		switch (event->type) {
			case YUTANI_MSG_KEY_EVENT: {
				struct yutani_msg_key_event * key = (void *)event->data;
				if (key->wid != window->wid || key->event.action != KEY_ACTION_DOWN) break;
				if (key->event.keycode == KEY_ESCAPE) {
					running = 0;
				} else if (key->event.keycode == KEY_ARROW_UP) {
					if (selected) selected--;
					redraw();
				} else if (key->event.keycode == KEY_ARROW_DOWN) {
					if (selected + 1 < result_count) selected++;
					redraw();
				} else if (key->event.key == '\n') {
					open_selected();
				} else if (key->event.key == '\b' || key->event.keycode == KEY_BACKSPACE) {
					if (query_length) query[--query_length] = '\0';
					selected = 0;
					rebuild_results();
					redraw();
				} else if (key->event.key >= 0x20 && key->event.key < 0x7f &&
					query_length + 1 < sizeof(query)) {
					query[query_length++] = key->event.key;
					query[query_length] = '\0';
					selected = 0;
					rebuild_results();
					redraw();
				}
				break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)event->data;
				if (mouse->wid != window->wid) break;
				int decor = decor_handle_event(yctx, event);
				if (decor == DECOR_CLOSE) running = 0;
				if (mouse->command == YUTANI_MOUSE_EVENT_CLICK) {
					struct decor_bounds current;
					decor_get_bounds(window, &current);
					int first = current.top_height + 150;
					if (mouse->new_y >= first) {
						int row = (mouse->new_y - first) / 48;
						if (row >= 0 && (size_t)row < result_count) {
							selected = row;
							redraw();
							open_selected();
						}
					}
				}
				break;
			}
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE: {
				struct yutani_msg_window_focus_change * focus = (void *)event->data;
				if (focus->wid == window->wid) {
					window->focused = focus->focused;
					redraw();
				}
				break;
			}
			case YUTANI_MSG_WINDOW_CLOSE:
			case YUTANI_MSG_SESSION_END:
				running = 0;
		}
		free(event);
	}
	yutani_close(yctx, window);
	return 0;
}
