/**
 * @brief Modal file-name dialog for create and rename operations.
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <toaru/graphics.h>
#include <toaru/button.h>
#include <toaru/decorations.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define DIALOG_WIDTH 460
#define DIALOG_HEIGHT 190
#define NAME_MAXIMUM 240

enum operation_mode {
	MODE_CREATE_FILE,
	MODE_CREATE_FOLDER,
	MODE_RENAME,
};

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font;
static struct TTKButton okay = {0};
static struct TTKButton cancel = {0};
static enum operation_mode mode;
static char target[4096];
static char name[NAME_MAXIMUM + 1];
static size_t name_length;
static char message[256];
static pid_t callback_pid;
static int running = 1;

static int in_button(struct TTKButton * button,
	struct yutani_msg_window_mouse_event * event) {
	return event->new_x >= button->x &&
		event->new_x < button->x + button->width &&
		event->new_y >= button->y &&
		event->new_y < button->y + button->height;
}

static char * title(void) {
	if (mode == MODE_CREATE_FILE) return "Create New File";
	if (mode == MODE_CREATE_FOLDER) return "Create New Folder";
	return "Rename Item";
}

static void draw_dialog(void) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	draw_fill(ctx, rgb(9,10,12));

	tt_set_size(font, 14);
	tt_draw_string(ctx, font, bounds.left_width + 24,
		bounds.top_height + 36, "Name", rgb(230,236,244));

	int box_x = bounds.left_width + 24;
	int box_y = bounds.top_height + 50;
	int box_width = window->width - bounds.width - 48;
	draw_rounded_rectangle(ctx, box_x, box_y, box_width, 34, 5,
		rgb(24,28,35));
	draw_rectangle(ctx, box_x, box_y, box_width, 1, rgb(78,163,255));
	draw_rectangle(ctx, box_x, box_y + 33, box_width, 1, rgb(78,163,255));
	draw_rectangle(ctx, box_x, box_y, 1, 34, rgb(78,163,255));
	draw_rectangle(ctx, box_x + box_width - 1, box_y, 1, 34, rgb(78,163,255));
	tt_draw_string(ctx, font, box_x + 10, box_y + 23, name,
		rgb(255,255,255));
	int cursor = tt_string_width(font, name);
	draw_rectangle(ctx, box_x + 10 + cursor, box_y + 8, 1, 19,
		rgb(110,219,255));

	if (message[0]) {
		tt_set_size(font, 12);
		tt_draw_string(ctx, font, box_x, box_y + 54, message,
			rgb(255,120,120));
	}

	ttk_button_draw(ctx, &cancel);
	ttk_button_draw(ctx, &okay);
	render_decorations(window, ctx, title());
	flip(ctx);
	yutani_flip(yctx, window);
}

static int valid_name(void) {
	if (!name[0] || !strcmp(name, ".") || !strcmp(name, "..") ||
		strchr(name, '/')) {
		strcpy(message, "Enter a valid name without '/'.");
		return 0;
	}
	return 1;
}

static int destination_path(char * output, size_t output_size) {
	const char * directory = target;
	char parent[4096];
	if (mode == MODE_RENAME) {
		strncpy(parent, target, sizeof(parent) - 1);
		parent[sizeof(parent) - 1] = '\0';
		directory = dirname(parent);
	}
	int written = snprintf(output, output_size, "%s%s%s", directory,
		!strcmp(directory, "/") ? "" : "/", name);
	if (written < 0 || (size_t)written >= output_size) {
		errno = ENAMETOOLONG;
		return -1;
	}
	return 0;
}

static void apply_operation(void) {
	message[0] = '\0';
	if (!valid_name()) {
		draw_dialog();
		return;
	}
	char destination[4096];
	if (destination_path(destination, sizeof(destination))) goto failure;

	int result;
	if (mode == MODE_CREATE_FOLDER) {
		result = mkdir(destination, 0755);
	} else if (mode == MODE_CREATE_FILE) {
		int descriptor = open(destination, O_WRONLY | O_CREAT | O_EXCL, 0644);
		result = descriptor < 0 ? -1 : close(descriptor);
	} else {
		if (!strcmp(target, destination)) {
			result = 0;
		} else {
			struct stat existing;
			if (!lstat(destination, &existing)) {
				errno = EEXIST;
				result = -1;
			} else {
				result = rename(target, destination);
			}
		}
	}
	if (!result) {
		if (callback_pid > 0) kill(callback_pid, SIGURG);
		running = 0;
		return;
	}

failure:
	snprintf(message, sizeof(message), "Unable to save: %s", strerror(errno));
	draw_dialog();
}

static int parse_arguments(int argc, char * argv[]) {
	if (argc != 4) return -1;
	if (!strcmp(argv[1], "--create-file")) mode = MODE_CREATE_FILE;
	else if (!strcmp(argv[1], "--create-folder")) mode = MODE_CREATE_FOLDER;
	else if (!strcmp(argv[1], "--rename")) mode = MODE_RENAME;
	else return -1;
	if (strlen(argv[2]) >= sizeof(target)) return -1;
	strcpy(target, argv[2]);
	callback_pid = atoi(argv[3]);
	if (callback_pid < 1) return -1;
	if (mode == MODE_RENAME) {
		const char * base = strrchr(target, '/');
		base = base ? base + 1 : target;
		name_length = strlen(base);
		if (name_length >= sizeof(name)) name_length = sizeof(name) - 1;
		memcpy(name, base, name_length);
		name[name_length] = '\0';
	}
	return 0;
}

int main(int argc, char * argv[]) {
	if (parse_arguments(argc, argv)) {
		fprintf(stderr,
			"usage: %s --create-file|--create-folder DIRECTORY PID\n"
			"       %s --rename PATH PID\n", argv[0], argv[0]);
		return 1;
	}
	yctx = yutani_init();
	if (!yctx) return 1;
	init_decorations();
	struct decor_bounds bounds;
	decor_get_bounds(NULL, &bounds);
	window = yutani_window_create_flags(yctx,
		DIALOG_WIDTH + bounds.width, DIALOG_HEIGHT + bounds.height,
		YUTANI_WINDOW_FLAG_DIALOG_ANIMATION);
	window->decorator_flags |= DECOR_FLAG_NO_MAXIMIZE;
	yutani_window_move(yctx, window,
		yctx->display_width / 2 - window->width / 2,
		yctx->display_height / 2 - window->height / 2);
	yutani_window_advertise_icon(yctx, window, title(), "folder");
	ctx = init_graphics_yutani_double_buffer(window);
	font = tt_font_from_shm("sans-serif");

	okay.title = mode == MODE_RENAME ? "Rename" : "Create";
	okay.width = 90;
	okay.height = 28;
	okay.x = window->width - bounds.right_width - 24 - okay.width;
	okay.y = window->height - bounds.bottom_height - 20 - okay.height;
	cancel.title = "Cancel";
	cancel.width = 90;
	cancel.height = 28;
	cancel.x = okay.x - 12 - cancel.width;
	cancel.y = okay.y;
	draw_dialog();

	struct TTKButton * pressed = NULL;
	while (running) {
		yutani_msg_t * event = yutani_poll(yctx);
		if (!event) continue;
		switch (event->type) {
			case YUTANI_MSG_KEY_EVENT: {
				struct yutani_msg_key_event * key = (void *)event->data;
				if (key->event.action != KEY_ACTION_DOWN || key->wid != window->wid) break;
				if (key->event.key == KEY_ESCAPE) running = 0;
				else if (key->event.key == '\n') apply_operation();
				else if (key->event.key == '\b' || key->event.keycode == KEY_BACKSPACE) {
					if (name_length) name[--name_length] = '\0';
					draw_dialog();
				} else if (key->event.key >= 0x20 && key->event.key < 0x7F &&
					name_length < NAME_MAXIMUM) {
					name[name_length++] = key->event.key;
					name[name_length] = '\0';
					draw_dialog();
				}
				break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)event->data;
				if (mouse->wid != window->wid) break;
				int decor = decor_handle_event(yctx, event);
				if (decor == DECOR_CLOSE) running = 0;
				if (mouse->command == YUTANI_MOUSE_EVENT_DOWN) {
					if (in_button(&okay, mouse)) pressed = &okay;
					else if (in_button(&cancel, mouse)) pressed = &cancel;
					if (pressed) pressed->hilight = 2;
					draw_dialog();
				} else if ((mouse->command == YUTANI_MOUSE_EVENT_RAISE ||
					mouse->command == YUTANI_MOUSE_EVENT_CLICK) && pressed) {
					if (in_button(pressed, mouse)) {
						if (pressed == &okay) apply_operation();
						else running = 0;
					}
					pressed->hilight = 0;
					pressed = NULL;
					draw_dialog();
				}
				break;
			}
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE: {
				struct yutani_msg_window_focus_change * focus = (void *)event->data;
				if (focus->wid == window->wid) {
					window->focused = focus->focused;
					draw_dialog();
				}
				break;
			}
			case YUTANI_MSG_WINDOW_CLOSE:
			case YUTANI_MSG_SESSION_END:
				running = 0;
				break;
		}
		free(event);
	}
	yutani_close(yctx, window);
	return 0;
}
