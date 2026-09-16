/** @brief Razion Snake - a responsive native arcade game. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/fswait.h>
#include <time.h>

#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define GRID_W 22
#define GRID_H 16
#define CELL 20
#define MAX_SEGMENTS (GRID_W * GRID_H)

enum game_state { GAME_READY, GAME_PLAYING, GAME_PAUSED, GAME_OVER };

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font, * bold;
static int sx[MAX_SEGMENTS], sy[MAX_SEGMENTS], snake_length;
static int food_x, food_y, direction, next_direction;
static int score, best_score, running = 1;
static int hover_button, pressed_button;
static enum game_state state;

static long long monotonic_ms(void) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int occupied(int x, int y, int count) {
	for (int i = 0; i < count; ++i) if (sx[i] == x && sy[i] == y) return 1;
	return 0;
}

static void place_food(void) {
	if (snake_length >= MAX_SEGMENTS) return;
	do { food_x = rand() % GRID_W; food_y = rand() % GRID_H; }
	while (occupied(food_x, food_y, snake_length));
}

static void reset_game(void) {
	snake_length = 4; score = 0; direction = next_direction = 1; state = GAME_READY;
	for (int i = 0; i < snake_length; ++i) { sx[i] = GRID_W / 2 - i; sy[i] = GRID_H / 2; }
	place_food();
}

static int tick_interval(void) {
	int interval = 125 - (score / 50) * 8;
	return interval < 61 ? 61 : interval;
}

static void tick(void) {
	if (state != GAME_PLAYING) return;
	direction = next_direction;
	int nx = sx[0], ny = sy[0];
	if (direction == 0) ny--; else if (direction == 1) nx++;
	else if (direction == 2) ny++; else nx--;
	int grow = nx == food_x && ny == food_y;
	int collision_count = snake_length - (grow ? 0 : 1);
	if (nx < 0 || ny < 0 || nx >= GRID_W || ny >= GRID_H || occupied(nx, ny, collision_count)) {
		state = GAME_OVER;
		if (score > best_score) best_score = score;
		return;
	}
	if (grow && snake_length < MAX_SEGMENTS) snake_length++;
	for (int i = snake_length - 1; i > 0; --i) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
	sx[0] = nx; sy[0] = ny;
	if (grow) { score += 10; if (score > best_score) best_score = score; place_food(); }
}

static const char * action_label(void) {
	if (state == GAME_OVER) return "New game  (R)";
	if (state == GAME_PLAYING) return "Pause  (Space)";
	if (state == GAME_PAUSED) return "Resume  (Space)";
	return "Start  (Space)";
}

static void draw_overlay(int left, int top) {
	if (state == GAME_PLAYING) return;
	const char * title = state == GAME_OVER ? "Game over" : state == GAME_PAUSED ? "Paused" : "Ready?";
	const char * detail = state == GAME_OVER ? "Press R to play again" :
		state == GAME_PAUSED ? "Space resumes the game" : "Use arrows or WASD to start";
	draw_rounded_rectangle(ctx, left + 80, top + 119, 280, 82, 10, premultiply(rgba(10,16,24,235)));
	tt_set_size(bold, 18); int width = tt_string_width(bold, title);
	tt_draw_string(ctx, bold, left + (GRID_W * CELL - width) / 2, top + 151, title, RAZION_TEXT_PRIMARY);
	tt_set_size(font, 12); width = tt_string_width(font, detail);
	tt_draw_string(ctx, font, left + (GRID_W * CELL - width) / 2, top + 177, detail, RAZION_TEXT_SECONDARY);
}

static void redraw(void) {
	struct decor_bounds b; decor_get_bounds(window, &b); draw_fill(ctx, RAZION_BACKGROUND);
	int left = (window->width - GRID_W * CELL) / 2, top = b.top_height + 78;
	tt_set_size(bold, 22); tt_draw_string(ctx, bold, b.left_width + 26, b.top_height + 39, "Razion Snake", RAZION_TEXT_PRIMARY);
	tt_set_size(font, 11); tt_draw_string(ctx, font, b.left_width + 26, b.top_height + 61, "Arrows / WASD move  •  Space pauses  •  R restarts", RAZION_TEXT_SECONDARY);
	char label[80]; snprintf(label, sizeof(label), "Score %d   Best %d", score, best_score);
	tt_set_size(bold, 12); int label_width = tt_string_width(bold, label);
	tt_draw_string(ctx, bold, window->width - b.right_width - label_width - 26, b.top_height + 39, label, RAZION_ACCENT);
	draw_rounded_rectangle(ctx, left - 10, top - 10, GRID_W * CELL + 20, GRID_H * CELL + 20, 10, RAZION_SURFACE);
	for (int y = 0; y < GRID_H; ++y) for (int x = 0; x < GRID_W; ++x) {
		uint32_t color = ((x + y) & 1) ? RAZION_SURFACE_SECONDARY : RAZION_SURFACE;
		draw_rectangle_solid(ctx, left + x * CELL, top + y * CELL, CELL - 1, CELL - 1, color);
	}
	draw_rounded_rectangle(ctx, left + food_x * CELL + 3, top + food_y * CELL + 3, CELL - 6, CELL - 6, 6, RAZION_ERROR);
	for (int i = snake_length - 1; i >= 0; --i) {
		uint32_t color = i ? RAZION_SUCCESS : RAZION_ACCENT;
		draw_rounded_rectangle(ctx, left + sx[i] * CELL + 2, top + sy[i] * CELL + 2, CELL - 4, CELL - 4, i ? 4 : 6, color);
	}
	draw_overlay(left, top);
	int button_x = (window->width - 176) / 2, button_y = top + GRID_H * CELL + 22;
	uint32_t fill = pressed_button ? RAZION_SELECTION : hover_button ? RAZION_SURFACE_HOVER : RAZION_SURFACE_SECONDARY;
	draw_rounded_rectangle(ctx, button_x, button_y, 176, 40, 8, fill);
	tt_set_size(font, 12); const char * action = action_label(); int action_width = tt_string_width(font, action);
	tt_draw_string(ctx, font, button_x + (176 - action_width) / 2, button_y + 25, action, RAZION_TEXT_PRIMARY);
	render_decorations(window, ctx, "Razion Snake"); flip(ctx); yutani_flip(yctx, window);
}

static int button_hit(int x, int y) {
	struct decor_bounds b; decor_get_bounds(window, &b); int top = b.top_height + 78;
	int button_x = (window->width - 176) / 2, button_y = top + GRID_H * CELL + 22;
	return x >= button_x && x < button_x + 176 && y >= button_y && y < button_y + 40;
}

static void activate_button(void) {
	if (state == GAME_OVER) reset_game();
	else if (state == GAME_PLAYING) state = GAME_PAUSED;
	else state = GAME_PLAYING;
}

static int handle_key(struct yutani_msg_key_event * key) {
	if (key->event.action != KEY_ACTION_DOWN) return 0;
	if (key->event.keycode == KEY_ESCAPE) { running = 0; return 0; }
	if (key->event.key == 'r' || key->event.key == 'R') { reset_game(); return 1; }
	if (key->event.key == ' ' || key->event.key == 'p' || key->event.key == 'P') { activate_button(); return 1; }
	int requested = -1;
	if (key->event.keycode == KEY_ARROW_UP || key->event.key == 'w' || key->event.key == 'W') requested = 0;
	else if (key->event.keycode == KEY_ARROW_RIGHT || key->event.key == 'd' || key->event.key == 'D') requested = 1;
	else if (key->event.keycode == KEY_ARROW_DOWN || key->event.key == 's' || key->event.key == 'S') requested = 2;
	else if (key->event.keycode == KEY_ARROW_LEFT || key->event.key == 'a' || key->event.key == 'A') requested = 3;
	if (requested < 0 || state == GAME_OVER || state == GAME_PAUSED) return 0;
	if ((requested + 2) % 4 != direction) next_direction = requested;
	if (state == GAME_READY) state = GAME_PLAYING;
	return 1;
}

int main(void) {
	srand(time(NULL)); yctx = yutani_init(); if (!yctx) return 1; init_decorations();
	struct decor_bounds b; decor_get_bounds(NULL, &b);
	window = yutani_window_create(yctx, GRID_W * CELL + 60 + b.width, GRID_H * CELL + 154 + b.height);
	window->decorator_flags |= DECOR_FLAG_NO_MAXIMIZE;
	yutani_window_move(yctx, window, (yctx->display_width - window->width) / 2, (yctx->display_height - window->height) / 2);
	yutani_window_advertise_icon(yctx, window, "Razion Snake", "razion-snake");
	ctx = init_graphics_yutani_double_buffer(window); font = tt_font_from_shm("sans-serif"); bold = tt_font_from_shm("sans-serif.bold"); reset_game(); redraw();
	long long next_tick = monotonic_ms() + tick_interval();
	while (running) {
		enum game_state previous_state = state;
		long long remaining = next_tick - monotonic_ms();
		int fd = fileno(yctx->sock), timeout = state == GAME_PLAYING ? (remaining > 0 ? (int)remaining : 0) : 1000;
		int event_ready = yutani_query(yctx) > 0 || fswait2(1, &fd, timeout) == 0, dirty = 0;
		if (event_ready) {
			yutani_msg_t * message = yutani_poll(yctx);
			while (message) {
				switch (message->type) {
					case YUTANI_MSG_KEY_EVENT: { struct yutani_msg_key_event * key = (void *)message->data; if (key->wid == window->wid) dirty |= handle_key(key); break; }
					case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
						struct yutani_msg_window_mouse_event * mouse = (void *)message->data; if (mouse->wid != window->wid) break;
						if (decor_handle_event(yctx, message) == DECOR_CLOSE) { running = 0; break; }
						int over_button = button_hit(mouse->new_x, mouse->new_y);
						if (mouse->command == YUTANI_MOUSE_EVENT_DOWN) { if (pressed_button != over_button) dirty = 1; pressed_button = over_button; }
						else if (mouse->command == YUTANI_MOUSE_EVENT_LEAVE) { if (hover_button || pressed_button) dirty = 1; hover_button = pressed_button = 0; }
						else if (mouse->command == YUTANI_MOUSE_EVENT_RAISE || mouse->command == YUTANI_MOUSE_EVENT_CLICK) { if (over_button && pressed_button) { activate_button(); dirty = 1; } if (pressed_button) dirty = 1; pressed_button = 0; }
						if (hover_button != over_button) { hover_button = over_button; dirty = 1; }
						break;
					}
					case YUTANI_MSG_WINDOW_FOCUS_CHANGE: { struct yutani_msg_window_focus_change * focus = (void *)message->data; if (focus->wid == window->wid) { window->focused = focus->focused; if (!focus->focused && state == GAME_PLAYING) state = GAME_PAUSED; dirty = 1; } break; }
					case YUTANI_MSG_WINDOW_CLOSE: case YUTANI_MSG_SESSION_END: running = 0; break;
				}
				free(message); message = yutani_poll_async(yctx);
			}
		}
		long long now = monotonic_ms();
		if (state != GAME_PLAYING || previous_state != GAME_PLAYING) next_tick = now + tick_interval();
		else if (now >= next_tick) { tick(); dirty = 1; next_tick = now + tick_interval(); }
		if (dirty && running) redraw();
	}
	yutani_close(yctx, window); return 0;
}
