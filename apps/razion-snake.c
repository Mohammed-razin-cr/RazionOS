/** @brief Razion Snake - a lightweight native arcade game. */
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

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font, * bold;
static int sx[MAX_SEGMENTS], sy[MAX_SEGMENTS], snake_length;
static int food_x, food_y, direction, next_direction, score, over, running = 1;

static int occupied(int x, int y) {
	for (int i = 0; i < snake_length; ++i) if (sx[i] == x && sy[i] == y) return 1;
	return 0;
}

static void place_food(void) {
	if (snake_length >= MAX_SEGMENTS) return;
	do { food_x = rand() % GRID_W; food_y = rand() % GRID_H; } while (occupied(food_x, food_y));
}

static void reset_game(void) {
	snake_length = 4; score = 0; over = 0; direction = next_direction = 1;
	for (int i = 0; i < snake_length; ++i) { sx[i] = GRID_W / 2 - i; sy[i] = GRID_H / 2; }
	place_food();
}

static void tick(void) {
	if (over) return;
	direction = next_direction;
	int nx = sx[0], ny = sy[0];
	if (direction == 0) ny--; else if (direction == 1) nx++; else if (direction == 2) ny++; else nx--;
	if (nx < 0 || ny < 0 || nx >= GRID_W || ny >= GRID_H || occupied(nx, ny)) { over = 1; return; }
	int grow = nx == food_x && ny == food_y;
	if (grow && snake_length < MAX_SEGMENTS) snake_length++;
	for (int i = snake_length - 1; i > 0; --i) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
	sx[0] = nx; sy[0] = ny;
	if (grow) { score += 10; place_food(); }
}

static void redraw(void) {
	struct decor_bounds b; decor_get_bounds(window, &b);
	draw_fill(ctx, RAZION_BACKGROUND);
	int left = (window->width - GRID_W * CELL) / 2;
	int top = b.top_height + 74;
	tt_set_size(bold, 22); tt_draw_string(ctx, bold, b.left_width + 26, b.top_height + 38, "Razion Snake", RAZION_TEXT_PRIMARY);
	tt_set_size(font, 11); tt_draw_string(ctx, font, b.left_width + 26, b.top_height + 59, "Arrow keys move  •  R restarts  •  Esc closes", RAZION_TEXT_SECONDARY);
	char label[64]; snprintf(label, sizeof(label), "Score %d", score); tt_set_size(bold, 13);
	tt_draw_string(ctx, bold, window->width - b.right_width - 100, b.top_height + 39, label, RAZION_ACCENT);
	draw_rounded_rectangle(ctx, left - 10, top - 10, GRID_W * CELL + 20, GRID_H * CELL + 20, 10, RAZION_SURFACE);
	for (int y = 0; y < GRID_H; ++y) for (int x = 0; x < GRID_W; ++x) {
		uint32_t c = ((x + y) & 1) ? rgb(25,37,48) : rgb(21,32,42);
		draw_rectangle_solid(ctx, left + x * CELL, top + y * CELL, CELL - 1, CELL - 1, c);
	}
	draw_rounded_rectangle(ctx, left + food_x * CELL + 3, top + food_y * CELL + 3, CELL - 6, CELL - 6, 6, RAZION_ERROR);
	for (int i = snake_length - 1; i >= 0; --i) {
		uint32_t c = i ? rgb(45,176,164) : RAZION_ACCENT;
		draw_rounded_rectangle(ctx, left + sx[i] * CELL + 2, top + sy[i] * CELL + 2, CELL - 4, CELL - 4, i ? 4 : 6, c);
	}
	if (over) {
		draw_rounded_rectangle(ctx, left + 82, top + 126, 276, 72, 8, premultiply(rgba(10,16,24,232)));
		tt_set_size(bold, 18); tt_draw_string(ctx, bold, left + 154, top + 156, "Game over", RAZION_TEXT_PRIMARY);
		tt_set_size(font, 12); tt_draw_string(ctx, font, left + 128, top + 180, "Press R to play again", RAZION_TEXT_SECONDARY);
	}
	render_decorations(window, ctx, "Razion Snake"); flip(ctx); yutani_flip(yctx, window);
}

static void handle_key(struct yutani_msg_key_event * key) {
	if (key->event.action != KEY_ACTION_DOWN) return;
	if (key->event.keycode == KEY_ESCAPE) running = 0;
	else if (key->event.key == 'r' || key->event.key == 'R') reset_game();
	else if (key->event.keycode == KEY_ARROW_UP && direction != 2) next_direction = 0;
	else if (key->event.keycode == KEY_ARROW_RIGHT && direction != 3) next_direction = 1;
	else if (key->event.keycode == KEY_ARROW_DOWN && direction != 0) next_direction = 2;
	else if (key->event.keycode == KEY_ARROW_LEFT && direction != 1) next_direction = 3;
}

int main(void) {
	srand(time(NULL)); yctx = yutani_init(); if (!yctx) return 1; init_decorations();
	struct decor_bounds b; decor_get_bounds(NULL, &b);
	window = yutani_window_create(yctx, GRID_W * CELL + 60 + b.width, GRID_H * CELL + 120 + b.height);
	window->decorator_flags |= DECOR_FLAG_NO_MAXIMIZE;
	yutani_window_move(yctx, window, (yctx->display_width - window->width) / 2, (yctx->display_height - window->height) / 2);
	yutani_window_advertise_icon(yctx, window, "Razion Snake", "razion-snake");
	ctx = init_graphics_yutani_double_buffer(window); font = tt_font_from_shm("sans-serif"); bold = tt_font_from_shm("sans-serif.bold"); reset_game(); redraw();
	while (running) {
		int fd = fileno(yctx->sock);
		if (fswait2(1, &fd, 115) == 0) {
			yutani_msg_t * message = yutani_poll(yctx);
			while (message) {
				switch (message->type) {
					case YUTANI_MSG_KEY_EVENT: { struct yutani_msg_key_event * key = (void *)message->data; if (key->wid == window->wid) handle_key(key); break; }
					case YUTANI_MSG_WINDOW_MOUSE_EVENT: { struct yutani_msg_window_mouse_event * mouse = (void *)message->data; if (mouse->wid == window->wid && decor_handle_event(yctx, message) == DECOR_CLOSE) running = 0; break; }
					case YUTANI_MSG_WINDOW_FOCUS_CHANGE: { struct yutani_msg_window_focus_change * focus = (void *)message->data; if (focus->wid == window->wid) window->focused = focus->focused; break; }
					case YUTANI_MSG_WINDOW_CLOSE: case YUTANI_MSG_SESSION_END: running = 0; break;
				}
				free(message); message = yutani_poll(yctx);
			}
		} else tick();
		redraw();
	}
	yutani_close(yctx, window); return 0;
}
