/** @brief Razion Assistant - native AI and file-search panel. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/razion_ai.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define WIDTH 720
#define HEIGHT 520
#define INPUT_MAX 512
#define ANSWER_MAX 768

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font, * bold;
static int running = 1, mode_files = 0, hover = -1, pressed = -1;
static char input[INPUT_MAX];
static size_t input_length;
static char answer[ANSWER_MAX] = "Ask a question, search files, or open Pulse for safe system actions.";
static char provider[64] = "local-first";

static void spawn2(const char * executable, const char * a, const char * b) {
	if (fork()) return;
	char * args[] = {(char *)executable, (char *)a, (char *)b, NULL};
	execv(executable, args);
	_Exit(127);
}

static void copy_text(char * out, size_t size, const char * text) {
	if (!size) return;
	if (!text) text = "";
	snprintf(out, size, "%s", text);
}

static void label(struct TT_Font * face, int x, int y, int size,
	const char * text, uint32_t color) {
	tt_set_size(face, size);
	tt_draw_string(ctx, face, x, y, text, color);
}

static void ellipsized(int x, int y, int size, const char * text,
	int width, uint32_t color, int strong) {
	struct TT_Font * face = strong ? bold : font;
	char * shown = tt_ellipsify(text, size, face, width, NULL);
	label(face, x, y, size, shown, color);
	free(shown);
}

static void wrapped(int x, int y, int width, const char * text, uint32_t color) {
	char line[120];
	size_t start = 0, length = strlen(text);
	int row = 0;
	while (start < length && row < 8) {
		size_t take = length - start;
		if (take > 86) take = 86;
		size_t end = start + take;
		if (end < length) {
			size_t space = end;
			while (space > start && text[space] != ' ') space--;
			if (space > start) end = space;
		}
		size_t n = end - start;
		if (n >= sizeof(line)) n = sizeof(line) - 1;
		memcpy(line, text + start, n);
		line[n] = '\0';
		ellipsized(x, y + row * 19, 12, line, width, color, 0);
		start = end;
		while (text[start] == ' ') start++;
		row++;
	}
}

static void button(int id, int x, int y, int w, const char * text, int selected) {
	uint32_t fill = selected || id == pressed ? RAZION_SELECTION :
		id == hover ? RAZION_SURFACE_HOVER : RAZION_SURFACE_SECONDARY;
	draw_rounded_rectangle(ctx, x, y, w, 44, 8, fill);
	draw_rectangle_solid(ctx, x + 10, y + 43, w - 20, 1,
		selected ? RAZION_ACCENT : RAZION_BORDER);
	tt_set_size(font, 12);
	int tw = tt_string_width(font, text);
	tt_draw_string(ctx, font, x + (w - tw) / 2, y + 27, text, RAZION_TEXT_PRIMARY);
}

static void redraw(void) {
	struct decor_bounds b;
	decor_get_bounds(window, &b);
	draw_fill(ctx, RAZION_BACKGROUND);
	int left = b.left_width + 32;
	int top = b.top_height;
	int usable = window->width - b.width - 64;

	label(bold, left, top + 42, 25, "Razion Assistant", RAZION_TEXT_PRIMARY);
	label(font, left, top + 66, 11,
		"AI talks only through Razion AI Engine. File search stays local-first.",
		RAZION_TEXT_SECONDARY);

	button(1, left, top + 88, 126, "Ask AI", !mode_files);
	button(2, left + 136, top + 88, 150, "Search Files", mode_files);
	button(3, left + 296, top + 88, 126, "Open Pulse", 0);
	button(4, left + 432, top + 88, 154, "Provider Status", 0);

	draw_rounded_rectangle(ctx, left, top + 150, usable, 56, 8, RAZION_SURFACE);
	draw_rectangle_solid(ctx, left + 12, top + 205, usable - 24, 1, RAZION_FOCUS);
	ellipsized(left + 16, top + 184, 17,
		input[0] ? input : (mode_files ? "Find files, PDFs, downloads..." : "Ask Razion AI..."),
		usable - 32, input[0] ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY, 0);
	if (window->focused) {
		int cursor = tt_string_width(font, input);
		if (cursor < usable - 42)
			draw_rectangle_solid(ctx, left + 16 + cursor, top + 164, 2, 29, RAZION_FOCUS);
	}

	draw_rounded_rectangle(ctx, left, top + 232, usable, 190, 8, RAZION_SURFACE);
	label(bold, left + 18, top + 258, 14,
		mode_files ? "File Search Result" : "Assistant Reply", RAZION_TEXT_PRIMARY);
	wrapped(left + 18, top + 292, usable - 36, answer, RAZION_TEXT_SECONDARY);

	char status[160];
	snprintf(status, sizeof(status), "Mode: %s  |  Provider: %s  |  Enter submits  |  Esc closes",
		mode_files ? "AI file search" : "chat", provider);
	label(font, left, window->height - b.bottom_height - 22, 10, status, RAZION_TEXT_SECONDARY);

	render_decorations(window, ctx, "Razion Assistant");
	flip(ctx);
	yutani_flip(yctx, window);
}

static int hit(int x, int y) {
	struct decor_bounds b;
	decor_get_bounds(window, &b);
	int left = b.left_width + 32;
	int top = b.top_height + 88;
	if (y < top || y >= top + 44) return -1;
	if (x >= left && x < left + 126) return 1;
	if (x >= left + 136 && x < left + 286) return 2;
	if (x >= left + 296 && x < left + 422) return 3;
	if (x >= left + 432 && x < left + 586) return 4;
	return -1;
}

static void submit(void) {
	if (!input[0]) {
		copy_text(answer, sizeof(answer), "Type a request first.");
		return;
	}
	if (mode_files) {
		razion_ai_context_t ai;
		razion_ai_response_t response;
		if (!razion_ai_init(&ai, "razion-assistant")) {
			ai.timeout_ms = 30000;
			ai.default_flags |= RAZION_AI_FLAG_LOCAL_ONLY;
			if (!razion_ai_search_files(&ai, input, &response) &&
				response.status == RAZION_AI_STATUS_OK) {
				copy_text(answer, sizeof(answer), response.payload);
				copy_text(provider, sizeof(provider), response.provider[0] ? response.provider : "engine");
				return;
			}
		}
		copy_text(answer, sizeof(answer),
			"AI file search is unavailable, so Razion Launcher opened with your local query.");
		spawn2("/bin/universal-search", "--initial", input);
		return;
	}

	razion_ai_context_t ai;
	razion_ai_response_t response;
	memset(&response, 0, sizeof(response));
	response.status = RAZION_AI_STATUS_PROVIDER_UNAVAILABLE;
	if (razion_ai_init(&ai, "razion-assistant")) {
		copy_text(answer, sizeof(answer), "Razion AI Engine is not reachable.");
		copy_text(provider, sizeof(provider), "unavailable");
		return;
	}
	ai.timeout_ms = 30000;
	copy_text(answer, sizeof(answer), "Thinking...");
	redraw();
	if (razion_ai_ask(&ai, input, &response) || response.status != RAZION_AI_STATUS_OK) {
		copy_text(answer, sizeof(answer), response.payload_length ? response.payload :
			razion_ai_status_string(response.status));
		copy_text(provider, sizeof(provider), "unavailable");
		return;
	}
	copy_text(answer, sizeof(answer), response.payload);
	copy_text(provider, sizeof(provider), response.provider[0] ? response.provider : "engine");
}

static void activate(int id) {
	if (id == 1) mode_files = 0;
	else if (id == 2) mode_files = 1;
	else if (id == 3) spawn2("/bin/terminal", "pulse", NULL);
	else if (id == 4) spawn2("/bin/terminal", "razion-ai-status", "providers");
}

int main(int argc, char * argv[]) {
	if (argc > 1 && !strcmp(argv[1], "--files")) mode_files = 1;
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
	yutani_window_advertise_icon(yctx, window, "Razion Assistant", "razion-pulse");
	ctx = init_graphics_yutani_double_buffer(window);
	font = tt_font_from_shm("sans-serif");
	bold = tt_font_from_shm("sans-serif.bold");
	redraw();
	while (running) {
		yutani_msg_t * msg = yutani_poll(yctx);
		if (!msg) continue;
		switch (msg->type) {
			case YUTANI_MSG_KEY_EVENT: {
				struct yutani_msg_key_event * key = (void *)msg->data;
				if (key->wid != window->wid || key->event.action != KEY_ACTION_DOWN) break;
				if (key->event.keycode == KEY_ESCAPE) running = 0;
				else if (key->event.key == '\n') submit();
				else if (key->event.key == '\b' || key->event.keycode == KEY_BACKSPACE) {
					if (input_length) input[--input_length] = '\0';
				} else if (key->event.key >= 0x20 && key->event.key < 0x7f &&
					input_length + 1 < sizeof(input)) {
					input[input_length++] = key->event.key;
					input[input_length] = '\0';
				}
				redraw();
				break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)msg->data;
				if (mouse->wid != window->wid) break;
				int decor = decor_handle_event(yctx, msg);
				if (decor == DECOR_CLOSE) running = 0;
				int old_hover = hover, old_pressed = pressed;
				int over = hit(mouse->new_x, mouse->new_y);
				if (mouse->command == YUTANI_MOUSE_EVENT_DOWN) pressed = over;
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
