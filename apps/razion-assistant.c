/** @brief Razion Assistant - native AI and file-search workspace. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fswait.h>
#include <sys/time.h>
#include <unistd.h>

#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/razion_ai.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define WIDTH 760
#define HEIGHT 560
#define INPUT_MAX 512
#define ANSWER_MAX 768

enum response_state {
	RESPONSE_EMPTY,
	RESPONSE_LOADING,
	RESPONSE_SUCCESS,
	RESPONSE_ERROR,
	RESPONSE_CANCELLED,
};

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font, * bold;
static int running = 1, mode_files = 0, hover = -1, pressed = -1, focus_id = 0;
static int ai_pending = 0, copied = 0;
static enum response_state response_state = RESPONSE_EMPTY;
static unsigned loading_phase = 0;
static uint64_t request_started = 0;
static razion_ai_context_t ai_context;
static razion_ai_async_request_t ai_request;
static char input[INPUT_MAX];
static char last_prompt[INPUT_MAX];
static size_t input_length;
static char answer[ANSWER_MAX];
static char provider[64] = "local-first";

static uint64_t milliseconds(void) {
	struct timeval value;
	gettimeofday(&value, NULL);
	return (uint64_t)value.tv_sec * 1000ULL + value.tv_usec / 1000;
}

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

static void draw_brand_mark(int x, int y) {
	draw_rounded_rectangle(ctx, x, y, 32, 32, 9, RAZION_ACCENT);
	draw_rounded_rectangle(ctx, x + 7,  y + 14, 3, 5,  1, rgb(255,255,255));
	draw_rounded_rectangle(ctx, x + 12, y + 10, 3, 13, 1, rgb(255,255,255));
	draw_rounded_rectangle(ctx, x + 17, y + 7,  3, 19, 1, rgb(255,255,255));
	draw_rounded_rectangle(ctx, x + 22, y + 11, 3, 11, 1, rgb(255,255,255));
}

static void ellipsized(int x, int y, int size, const char * text,
	int width, uint32_t color, int strong) {
	struct TT_Font * face = strong ? bold : font;
	char * shown = tt_ellipsify(text, size, face, width, NULL);
	label(face, x, y, size, shown, color);
	free(shown);
}

static void wrapped(int x, int y, int width, const char * text,
	uint32_t color, int max_rows) {
	char line[120];
	size_t start = 0, length = strlen(text);
	for (int row = 0; start < length && row < max_rows; ++row) {
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
		ellipsized(x, y + row * 18, 12, line, width, color, 0);
		start = end;
		while (text[start] == ' ') start++;
	}
}

static void button(int id, int x, int y, int w, int h, const char * text,
	int primary, int disabled) {
	unsigned state = primary ? RAZION_CONTROL_PRIMARY : RAZION_CONTROL_NORMAL;
	if (!disabled && id == hover) state |= RAZION_CONTROL_HOVER;
	if (!disabled && id == pressed) state |= RAZION_CONTROL_PRESSED;
	if (id == focus_id && window->focused) state |= RAZION_CONTROL_FOCUSED;
	if (disabled) state |= RAZION_CONTROL_DISABLED;
	razion_draw_button(ctx, font, x, y, w, h, text, state);
}

static uint32_t response_color(void) {
	if (response_state == RESPONSE_SUCCESS) return RAZION_SUCCESS;
	if (response_state == RESPONSE_ERROR) return RAZION_ERROR;
	if (response_state == RESPONSE_CANCELLED) return RAZION_WARNING;
	return RAZION_ACCENT;
}

static const char * response_title(void) {
	if (response_state == RESPONSE_LOADING) return mode_files ? "Searching safely" : "Razion AI is thinking";
	if (response_state == RESPONSE_ERROR) return "Request needs attention";
	if (response_state == RESPONSE_CANCELLED) return "Request cancelled";
	return mode_files ? "File search result" : "Razion AI";
}

static const char * suggestion(int id) {
	if (mode_files) {
		if (id == 9) return "Find my recent PDFs";
		if (id == 10) return "Find downloaded files";
		return "Find my project report";
	}
	if (id == 9) return "What can RazionOS do?";
	if (id == 10) return "Explain memory usage";
	return "How do I manage files?";
}

static void draw_empty_state(int left, int top, int usable) {
	int card_y = top + 148;
	razion_draw_card(ctx, left, card_y, usable, 264, RAZION_SURFACE);
	label(bold, left + 24, card_y + 48, 21,
		mode_files ? "Find something on this computer" : "How can I help?",
		RAZION_TEXT_PRIMARY);
	label(font, left + 24, card_y + 74, 12,
		mode_files ? "Search stays local and follows RazionOS file permissions." :
		"Ask a question or start with one of these suggestions.",
		RAZION_TEXT_SECONDARY);

	int gap = 12;
	int width = (usable - 48 - gap * 2) / 3;
	for (int id = 9; id <= 11; ++id) {
		int x = left + 24 + (id - 9) * (width + gap);
		button(id, x, card_y + 112, width, 60, suggestion(id), 0, 0);
	}
	label(font, left + 24, card_y + 230, 10,
		"Local-first  •  No direct command execution  •  Enter sends",
		RAZION_TEXT_SECONDARY);
}

static void draw_conversation(int left, int top, int usable) {
	int card_y = top + 148;
	razion_draw_card(ctx, left, card_y, usable, 264, RAZION_SURFACE);

	label(bold, left + 24, card_y + 25, 11, "YOU", RAZION_TEXT_SECONDARY);
	draw_rounded_rectangle(ctx, left + 22, card_y + 36, usable - 44, 48, 9,
		RAZION_SURFACE_SECONDARY);
	ellipsized(left + 38, card_y + 66, 13, last_prompt,
		usable - 76, RAZION_TEXT_PRIMARY, 0);

	if (response_state == RESPONSE_LOADING)
		razion_draw_loading(ctx, left + 31, card_y + 116, loading_phase, RAZION_ACCENT);
	else
		razion_draw_status_dot(ctx, left + 27, card_y + 112, response_color());
	label(bold, left + 46, card_y + 120, 13, response_title(), RAZION_TEXT_PRIMARY);

	int action_y = card_y + 94;
	button(7, left + usable - 182, action_y, 76, 40, "Retry", 0,
		ai_pending || !last_prompt[0]);
	button(8, left + usable - 98, action_y, 76, 40, copied ? "Copied" : "Copy", 0,
		ai_pending || response_state != RESPONSE_SUCCESS);

	if (response_state == RESPONSE_LOADING) {
		label(font, left + 24, card_y + 156, 12,
			mode_files ? "Checking approved locations through the AI Engine..." :
			"Generating a local response without blocking the desktop...",
			RAZION_TEXT_SECONDARY);
		for (int i = 0; i < 3; ++i) {
			int width = usable - 54 - i * 74;
			draw_rounded_rectangle(ctx, left + 24, card_y + 180 + i * 18,
				width, 7, 4, RAZION_BORDER);
		}
	} else {
		wrapped(left + 24, card_y + 156, usable - 48, answer,
			response_state == RESPONSE_ERROR ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY, 5);
	}

	label(font, left + 24, card_y + 248, 10,
		response_state == RESPONSE_ERROR ?
		"Retry the request or open AI Status for recovery details." :
		"AI-generated response  •  Verify important facts",
		response_state == RESPONSE_ERROR ? RAZION_ERROR : RAZION_TEXT_SECONDARY);
}

static void redraw(void) {
	struct decor_bounds b;
	decor_get_bounds(window, &b);
	draw_fill(ctx, RAZION_BACKGROUND);
	int left = b.left_width + 32;
	int top = b.top_height;
	int usable = window->width - b.width - 64;

	draw_brand_mark(left, top + 24);
	label(bold, left + 44, top + 44, 24, "Razion Assistant", RAZION_TEXT_PRIMARY);
	label(font, left + 44, top + 67, 11,
		"Private by design. Requests pass only through Razion AI Engine.",
		RAZION_TEXT_SECONDARY);
	tt_set_size(font, 10);
	razion_draw_chip(ctx, font, left + usable - 162, top + 31, provider,
		!strcmp(provider, "unavailable") ? RAZION_ERROR :
		!strcmp(provider, "local-first") ? RAZION_ACCENT : RAZION_SUCCESS);

	button(1, left, top + 84, 124, 44, "Chat", !mode_files, 0);
	button(2, left + 132, top + 84, 144, 44, "Search Files", mode_files, 0);
	button(3, left + usable - 300, top + 84, 84, 44, "Pulse", 0, 0);
	button(4, left + usable - 208, top + 84, 92, 44, "Status", 0, 0);
	button(6, left + usable - 108, top + 84, 108, 44, "New Chat", 0, 0);

	if (response_state == RESPONSE_EMPTY) draw_empty_state(left, top, usable);
	else draw_conversation(left, top, usable);

	label(bold, left, top + 433, 11,
		mode_files ? "SEARCH QUERY" : "MESSAGE", RAZION_TEXT_SECONDARY);
	if (focus_id == 0 && window->focused)
		razion_draw_focus(ctx, left, top + 444, usable - 114, 60, 10);
	razion_draw_card(ctx, left, top + 444, usable - 114, 60, RAZION_SURFACE);
	ellipsized(left + 18, top + 480, 15,
		input[0] ? input : (mode_files ?
			(response_state == RESPONSE_EMPTY ? "Search files and documents..." : "Refine your search...") :
			(response_state == RESPONSE_EMPTY ? "Message Razion AI..." : "Ask a follow-up...")),
		usable - 142, input[0] ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY, 0);
	if (window->focused && focus_id == 0 && !ai_pending) {
		tt_set_size(font, 15);
		int cursor = tt_string_width(font, input);
		if (cursor < usable - 148)
			draw_rectangle_solid(ctx, left + 18 + cursor, top + 458, 2, 28, RAZION_FOCUS);
	}
	button(5, left + usable - 104, top + 452, 104, 44,
		ai_pending ? "Cancel" : (mode_files ? "Search" : "Send"), 1, 0);

	char status[180];
	if (ai_pending) {
		copy_text(status, sizeof(status),
			"Working  •  Esc cancels  •  The rest of RazionOS remains responsive");
	} else {
		snprintf(status, sizeof(status),
			"%s  •  Tab moves focus  •  Enter activates  •  Esc closes",
			mode_files ? "Local file mode" : "Local-first chat");
	}
	label(font, left, window->height - b.bottom_height - 18, 10,
		status, RAZION_TEXT_SECONDARY);

	render_decorations(window, ctx, "Razion Assistant");
	flip(ctx);
	yutani_flip(yctx, window);
}

static int hit(int x, int y) {
	struct decor_bounds b;
	decor_get_bounds(window, &b);
	int left = b.left_width + 32;
	int top = b.top_height;
	int usable = window->width - b.width - 64;
	if (y >= top + 84 && y < top + 128) {
		if (x >= left && x < left + 124) return 1;
		if (x >= left + 132 && x < left + 276) return 2;
		if (x >= left + usable - 300 && x < left + usable - 216) return 3;
		if (x >= left + usable - 208 && x < left + usable - 116) return 4;
		if (x >= left + usable - 108 && x < left + usable) return 6;
	}
	if (response_state == RESPONSE_EMPTY && y >= top + 260 && y < top + 320) {
		int gap = 12;
		int width = (usable - 48 - gap * 2) / 3;
		for (int id = 9; id <= 11; ++id) {
			int bx = left + 24 + (id - 9) * (width + gap);
			if (x >= bx && x < bx + width) return id;
		}
	}
	if (response_state != RESPONSE_EMPTY && y >= top + 242 && y < top + 282) {
		if (x >= left + usable - 182 && x < left + usable - 106) return 7;
		if (x >= left + usable - 98 && x < left + usable - 22) return 8;
	}
	if (y >= top + 444 && y < top + 504) {
		if (x >= left && x < left + usable - 114) return 0;
		if (x >= left + usable - 104 && x < left + usable) return 5;
	}
	return -1;
}

static void finish_request(void) {
	if (!ai_pending) return;
	razion_ai_response_t response;
	int result = razion_ai_request_poll(&ai_request, 0, &response);
	if (!result) return;
	ai_pending = 0;
	if (result < 0) {
		copy_text(answer, sizeof(answer),
			"Razion AI Engine did not return a response. Check AI Status, then retry.");
		copy_text(provider, sizeof(provider), "unavailable");
		response_state = RESPONSE_ERROR;
	} else if (response.status != RAZION_AI_STATUS_OK) {
		copy_text(answer, sizeof(answer), response.payload_length ? response.payload :
			razion_ai_status_string(response.status));
		copy_text(provider, sizeof(provider), response.provider[0] ? response.provider : "unavailable");
		response_state = RESPONSE_ERROR;
		if (mode_files) {
			copy_text(answer, sizeof(answer),
				"AI file search is unavailable. Universal Search opened with the same local query.");
			spawn2("/bin/universal-search", "--initial", last_prompt);
		}
	} else {
		copy_text(answer, sizeof(answer), response.payload);
		copy_text(provider, sizeof(provider), response.provider[0] ? response.provider : "engine");
		response_state = RESPONSE_SUCCESS;
	}
	razion_ai_request_cancel(&ai_request);
}

static void cancel_request(void) {
	if (!ai_pending) return;
	razion_ai_request_cancel(&ai_request);
	ai_pending = 0;
	response_state = RESPONSE_CANCELLED;
	copy_text(answer, sizeof(answer),
		"The request was cancelled. Your prompt is still available to edit or retry.");
}

static void submit(void) {
	if (!input[0]) {
		copy_text(answer, sizeof(answer), "Type a message before sending.");
		response_state = RESPONSE_ERROR;
		return;
	}
	if (ai_pending) return;
	copy_text(last_prompt, sizeof(last_prompt), input);
	copied = 0;
	razion_ai_operation_t operation = mode_files ? RAZION_AI_OP_SEARCH_FILES : RAZION_AI_OP_ASK;
	if (razion_ai_request_begin(&ai_context, operation, input,
		RAZION_AI_FLAG_LOCAL_ONLY, &ai_request)) {
		copy_text(answer, sizeof(answer),
			"Razion AI Engine is not reachable. Open AI Status for recovery details, then retry.");
		copy_text(provider, sizeof(provider), "unavailable");
		response_state = RESPONSE_ERROR;
		return;
	}
	ai_pending = 1;
	response_state = RESPONSE_LOADING;
	request_started = milliseconds();
	input[0] = '\0';
	input_length = 0;
	answer[0] = '\0';
}

static void set_prompt(const char * prompt) {
	copy_text(input, sizeof(input), prompt);
	input_length = strlen(input);
	focus_id = 0;
}

static void new_chat(void) {
	if (ai_pending) cancel_request();
	input[0] = '\0';
	last_prompt[0] = '\0';
	answer[0] = '\0';
	input_length = 0;
	response_state = RESPONSE_EMPTY;
	copied = 0;
	focus_id = 0;
}

static void activate(int id) {
	if (id == 1) { mode_files = 0; focus_id = 0; copied = 0; }
	else if (id == 2) { mode_files = 1; focus_id = 0; copied = 0; }
	else if (id == 3) spawn2("/bin/terminal", "pulse", NULL);
	else if (id == 4) spawn2("/bin/terminal", "razion-ai-status", "providers");
	else if (id == 5) { if (ai_pending) cancel_request(); else submit(); }
	else if (id == 6) new_chat();
	else if (id == 7 && !ai_pending && last_prompt[0]) {
		set_prompt(last_prompt);
		submit();
	} else if (id == 8 && !ai_pending && response_state == RESPONSE_SUCCESS) {
		yutani_set_clipboard(yctx, answer);
		copied = 1;
	} else if (id >= 9 && id <= 11 && response_state == RESPONSE_EMPTY) {
		set_prompt(suggestion(id));
		submit();
	}
}

static void move_focus(int direction) {
	static const int full_order[] = {0, 5, 1, 2, 3, 4, 6, 7, 8, 9, 10, 11};
	int current = 0;
	int count = sizeof(full_order) / sizeof(full_order[0]);
	for (int i = 0; i < count; ++i) if (full_order[i] == focus_id) current = i;
	for (int step = 1; step <= count; ++step) {
		int next = (current + direction * step + count * 2) % count;
		int candidate = full_order[next];
		if ((candidate >= 9 && candidate <= 11 && response_state != RESPONSE_EMPTY) ||
			(candidate == 7 && (ai_pending || !last_prompt[0])) ||
			(candidate == 8 && (ai_pending || response_state != RESPONSE_SUCCESS))) continue;
		focus_id = candidate;
		break;
	}
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
	razion_ai_init(&ai_context, "razion-assistant");
	ai_context.timeout_ms = 30000;
	ai_context.default_flags |= RAZION_AI_FLAG_LOCAL_ONLY;
	redraw();
	while (running) {
		int fd = fileno(yctx->sock);
		if (ai_pending && fswait2(1, &fd, 80) != 0) {
			finish_request();
			loading_phase = (milliseconds() - request_started) / 120;
			redraw();
			continue;
		}
		yutani_msg_t * msg = yutani_poll(yctx);
		if (!msg) continue;
		switch (msg->type) {
			case YUTANI_MSG_KEY_EVENT: {
				struct yutani_msg_key_event * key = (void *)msg->data;
				if (key->wid != window->wid || key->event.action != KEY_ACTION_DOWN) break;
				int shift = key->event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT);
				int ctrl = key->event.modifiers & (KEY_MOD_LEFT_CTRL | KEY_MOD_RIGHT_CTRL);
				if (key->event.keycode == KEY_ESCAPE) {
					if (ai_pending) cancel_request(); else running = 0;
				} else if (ctrl && key->event.keycode == 'n') new_chat();
				else if (ctrl && key->event.keycode == 'l') focus_id = 0;
				else if (key->event.keycode == '\t') move_focus(shift ? -1 : 1);
				else if (key->event.key == '\n') {
					if (focus_id > 0) activate(focus_id); else submit();
				} else if (!ai_pending && focus_id == 0 &&
					(key->event.key == '\b' || key->event.keycode == KEY_BACKSPACE)) {
					if (input_length) input[--input_length] = '\0';
					copied = 0;
				} else if (!ai_pending && focus_id == 0 && !ctrl &&
					key->event.key >= 0x20 && key->event.key < 0x7f &&
					input_length + 1 < sizeof(input)) {
					input[input_length++] = key->event.key;
					input[input_length] = '\0';
					copied = 0;
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
				if (mouse->command == YUTANI_MOUSE_EVENT_DOWN) {
					pressed = over;
					if (over >= 0) focus_id = over;
				} else if (mouse->command == YUTANI_MOUSE_EVENT_LEAVE) {
					hover = pressed = -1;
				} else if (mouse->command == YUTANI_MOUSE_EVENT_RAISE ||
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
	if (ai_pending) razion_ai_request_cancel(&ai_request);
	yutani_close(yctx, window);
	return 0;
}
