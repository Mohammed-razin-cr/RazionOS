/**
 * @brief RazionOS Settings control center.
 */
#include <errno.h>
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

#define SETTINGS_WIDTH 620
#define SETTINGS_HEIGHT 460
#define BUTTON_COUNT 7

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font;
static struct TTKButton buttons[BUTTON_COUNT];
static int theme_light;
static char theme_title[48];
static int running = 1;

static int read_light_theme(void) {
	const char * home = getenv("HOME");
	if (!home) return 0;
	char path[512];
	snprintf(path, sizeof(path), "%s/.razion/theme.conf", home);
	FILE * file = fopen(path, "r");
	if (!file) return 0;
	char line[64];
	int light = 0;
	while (fgets(line, sizeof(line), file)) {
		if (!strncmp(line, "theme=light", 11)) light = 1;
	}
	fclose(file);
	return light;
}

static int write_theme(void) {
	const char * home = getenv("HOME");
	if (!home) return -1;
	char directory[512];
	char path[512];
	int written = snprintf(directory, sizeof(directory), "%s/.razion", home);
	if (written < 0 || (size_t)written >= sizeof(directory)) return -1;
	if (mkdir(directory, 0700) && errno != EEXIST) return -1;
	size_t directory_length = strlen(directory);
	if (directory_length + sizeof("/theme.conf") > sizeof(path)) return -1;
	memcpy(path, directory, directory_length);
	memcpy(path + directory_length, "/theme.conf", sizeof("/theme.conf"));
	FILE * file = fopen(path, "w");
	if (!file) return -1;
	fprintf(file, "theme=%s\n", theme_light ? "light" : "dark");
	return fclose(file);
}

static void launch(char * executable, char * first, char * second) {
	if (!fork()) {
		char * args[] = {executable, first, second, NULL};
		_Exit(execvp(executable, args));
	}
}

static void update_theme_title(void) {
	snprintf(theme_title, sizeof(theme_title), "Theme: %s",
		theme_light ? "Light" : "Dark");
	buttons[1].title = theme_title;
}

static void setup_buttons(void) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	char * titles[BUTTON_COUNT] = {
		"Wallpaper", theme_title, "Desktop Companion", "Desktop",
		"AI Status", "About", "Terminal"
	};
	for (int i = 0; i < BUTTON_COUNT; ++i) {
		buttons[i].title = titles[i];
		buttons[i].width = 160;
		buttons[i].height = 42;
		buttons[i].x = bounds.left_width + 48 + (i % 3) * 180;
		buttons[i].y = bounds.top_height + 120 + (i / 3) * 70;
	}
}

static void redraw(void) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	draw_fill(ctx, theme_light ? rgb(238,242,244) : rgb(9,10,12));
	uint32_t primary = theme_light ? rgb(25,34,40) : rgb(235,241,245);
	uint32_t secondary = theme_light ? rgb(82,99,108) : rgb(157,171,183);
	tt_set_size(font, 24);
	tt_draw_string(ctx, font, bounds.left_width + 48,
		bounds.top_height + 54, "Settings", primary);
	tt_set_size(font, 13);
	tt_draw_string(ctx, font, bounds.left_width + 48,
		bounds.top_height + 82,
		"Personalization, desktop, system information, and AI configuration", secondary);
	for (int i = 0; i < BUTTON_COUNT; ++i) ttk_button_draw(ctx, &buttons[i]);
	tt_draw_string(ctx, font, bounds.left_width + 48,
		window->height - bounds.bottom_height - 36,
		"Theme changes apply to newly opened applications and the next login.",
		secondary);
	render_decorations(window, ctx, "RazionOS Settings");
	flip(ctx);
	yutani_flip(yctx, window);
}

static int in_button(struct TTKButton * button,
	struct yutani_msg_window_mouse_event * event) {
	return event->new_x >= button->x &&
		event->new_x < button->x + button->width &&
		event->new_y >= button->y &&
		event->new_y < button->y + button->height;
}

static void activate(int index) {
	const char * home = getenv("HOME");
	char desktop[512];
	switch (index) {
		case 0:
			launch("wallpaper-picker", NULL, NULL);
			break;
		case 1:
			theme_light = !theme_light;
			if (write_theme()) theme_light = !theme_light;
			update_theme_title();
			redraw();
			break;
		case 2:
			launch("razion-companion", NULL, NULL);
			break;
		case 3:
			if (home) {
				snprintf(desktop, sizeof(desktop), "%s/Desktop", home);
				launch("file-browser", desktop, NULL);
			}
			break;
		case 4:
			launch("terminal", "razion-ai-status", "providers");
			break;
		case 5:
			launch("about", NULL, NULL);
			break;
		case 6:
			launch("terminal", NULL, NULL);
			break;
	}
}

int main(int argc, char * argv[]) {
	yctx = yutani_init();
	if (!yctx) return 1;
	init_decorations();
	struct decor_bounds bounds;
	decor_get_bounds(NULL, &bounds);
	window = yutani_window_create(yctx,
		SETTINGS_WIDTH + bounds.width, SETTINGS_HEIGHT + bounds.height);
	yutani_window_move(yctx, window,
		yctx->display_width / 2 - window->width / 2,
		yctx->display_height / 2 - window->height / 2);
	yutani_window_advertise_icon(yctx, window, "RazionOS Settings", "applications-generic");
	ctx = init_graphics_yutani_double_buffer(window);
	font = tt_font_from_shm("sans-serif");
	theme_light = read_light_theme();
	update_theme_title();
	setup_buttons();
	redraw();

	int pressed = -1;
	while (running) {
		yutani_msg_t * event = yutani_poll(yctx);
		if (!event) continue;
		switch (event->type) {
			case YUTANI_MSG_KEY_EVENT: {
				struct yutani_msg_key_event * key = (void *)event->data;
				if (key->event.action == KEY_ACTION_DOWN &&
					key->event.key == KEY_ESCAPE) running = 0;
				break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)event->data;
				if (mouse->wid != window->wid) break;
				int decor = decor_handle_event(yctx, event);
				if (decor == DECOR_CLOSE) running = 0;
				int over = -1;
				for (int i = 0; i < BUTTON_COUNT; ++i) {
					if (in_button(&buttons[i], mouse)) over = i;
					buttons[i].hilight = i == over ? 1 : 0;
				}
				if (mouse->command == YUTANI_MOUSE_EVENT_DOWN && over >= 0) {
					pressed = over;
					buttons[over].hilight = 2;
				} else if ((mouse->command == YUTANI_MOUSE_EVENT_RAISE ||
					mouse->command == YUTANI_MOUSE_EVENT_CLICK) && pressed >= 0) {
					if (pressed == over) activate(pressed);
					pressed = -1;
				}
				redraw();
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
