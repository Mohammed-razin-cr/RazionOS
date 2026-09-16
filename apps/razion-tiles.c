/** @brief Razion Tiles - a native 2048-style number puzzle. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define SIZE 4
#define CELL 96
#define STEP 105

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font, * bold;
static int board[SIZE][SIZE], undo_board[SIZE][SIZE];
static int score, best_score, undo_score;
static int running = 1, finished, undo_available, reached_goal;
static int hover = -1, pressed = -1;
static char status[96] = "Merge matching tiles to reach 2048.";

static int can_move(void);

static void add_tile(void) {
	int empty[SIZE * SIZE][2], count = 0;
	for (int y = 0; y < SIZE; ++y) for (int x = 0; x < SIZE; ++x) {
		if (!board[y][x]) { empty[count][0] = x; empty[count++][1] = y; }
	}
	if (!count) return;
	int pick = rand() % count;
	board[empty[pick][1]][empty[pick][0]] = rand() % 10 ? 2 : 4;
}

static void set_status(const char * message) {
	snprintf(status, sizeof(status), "%s", message);
}

static void reset_game(void) {
	memset(board, 0, sizeof(board));
	score = 0; finished = 0; reached_goal = 0; undo_available = 0;
	add_tile(); add_tile();
	set_status("New board ready. Use arrows or WASD.");
}

static int compact_line(int line[SIZE]) {
	int old[SIZE]; memcpy(old, line, sizeof(old));
	int packed[SIZE] = {0}, out[SIZE] = {0}, count = 0, pos = 0;
	for (int i = 0; i < SIZE; ++i) if (line[i]) packed[count++] = line[i];
	for (int i = 0; i < count; ++i) {
		if (i + 1 < count && packed[i] == packed[i+1]) {
			out[pos] = packed[i] * 2; score += out[pos];
			if (out[pos] >= 2048) reached_goal = 1;
			pos++; i++;
		} else out[pos++] = packed[i];
	}
	memcpy(line, out, sizeof(out));
	return memcmp(old, line, sizeof(old)) != 0;
}

static int move_board(int direction) {
	int before[SIZE][SIZE], before_score = score;
	int previous_goal = reached_goal;
	memcpy(before, board, sizeof(before));
	reached_goal = 0;
	int changed = 0, line[SIZE];
	for (int a = 0; a < SIZE; ++a) {
		for (int i = 0; i < SIZE; ++i) {
			if (direction == 0) line[i] = board[a][i];
			else if (direction == 1) line[i] = board[a][SIZE-1-i];
			else if (direction == 2) line[i] = board[i][a];
			else line[i] = board[SIZE-1-i][a];
		}
		changed |= compact_line(line);
		for (int i = 0; i < SIZE; ++i) {
			if (direction == 0) board[a][i] = line[i];
			else if (direction == 1) board[a][SIZE-1-i] = line[i];
			else if (direction == 2) board[i][a] = line[i];
			else board[SIZE-1-i][a] = line[i];
		}
	}
	if (!changed) {
		reached_goal = previous_goal;
		set_status(can_move() ? "That direction is blocked." : "No moves left. Undo or start over.");
		return 0;
	}
	memcpy(undo_board, before, sizeof(undo_board));
	undo_score = before_score; undo_available = 1;
	add_tile();
	if (score > best_score) best_score = score;
	set_status(reached_goal ? "2048 reached. You can keep playing." : "Nice move.");
	return 1;
}

static int can_move(void) {
	for (int y = 0; y < SIZE; ++y) for (int x = 0; x < SIZE; ++x) {
		if (!board[y][x] || (x < SIZE-1 && board[y][x] == board[y][x+1]) ||
			(y < SIZE-1 && board[y][x] == board[y+1][x])) return 1;
	}
	return 0;
}

static int undo_move(void) {
	if (!undo_available) return 0;
	memcpy(board, undo_board, sizeof(board));
	score = undo_score; undo_available = 0; finished = 0; reached_goal = 0;
	set_status("Undo restored the previous board.");
	return 1;
}

static uint32_t tile_color(int value) {
	switch (value) {
		case 2: return rgb(48,64,80); case 4: return rgb(54,91,113);
		case 8: return rgb(38,139,139); case 16: return rgb(45,170,150);
		case 32: return rgb(69,125,222); case 64: return rgb(116,83,220);
		case 128: return rgb(190,105,48); case 256: return rgb(193,67,80);
		case 512: return rgb(174,129,30); case 1024: return rgb(154,113,22);
		default: return value ? RAZION_WARNING : RAZION_SURFACE_SECONDARY;
	}
}

static void draw_button(int id, int x, int y, const char * label, int enabled) {
	uint32_t fill = !enabled ? RAZION_SURFACE : id == pressed ? RAZION_SELECTION :
		id == hover ? RAZION_SURFACE_HOVER : RAZION_SURFACE_SECONDARY;
	draw_rounded_rectangle(ctx, x, y, 142, 44, 8, fill);
	tt_set_size(font, 12); int width = tt_string_width(font, label);
	tt_draw_string(ctx, font, x + (142 - width) / 2, y + 27, label,
		enabled ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY);
}

static void redraw(void) {
	struct decor_bounds b; decor_get_bounds(window, &b); draw_fill(ctx, RAZION_BACKGROUND);
	int left = (window->width - 420) / 2, top = b.top_height + 86;
	tt_set_size(bold, 22); tt_draw_string(ctx, bold, b.left_width + 30, b.top_height + 39, "Razion Tiles", RAZION_TEXT_PRIMARY);
	tt_set_size(font, 11); tt_draw_string(ctx, font, b.left_width + 30, b.top_height + 61, "Arrows / WASD move  •  U undoes  •  R starts over", RAZION_TEXT_SECONDARY);
	char label[80]; snprintf(label, sizeof(label), "Score %d   Best %d", score, best_score);
	tt_set_size(bold, 12); int label_width = tt_string_width(bold, label);
	tt_draw_string(ctx, bold, window->width - b.right_width - label_width - 30, b.top_height + 39, label, RAZION_ACCENT);
	tt_set_size(font, 10); tt_draw_string(ctx, font, b.left_width + 30, b.top_height + 78, status, RAZION_TEXT_SECONDARY);
	draw_rounded_rectangle(ctx, left - 10, top - 10, 440, 440, 10, RAZION_SURFACE);
	for (int y = 0; y < SIZE; ++y) for (int x = 0; x < SIZE; ++x) {
		int px = left + x * STEP, py = top + y * STEP, value = board[y][x];
		draw_rounded_rectangle(ctx, px, py, CELL, CELL, 8, tile_color(value));
		if (value) {
			char number[16]; snprintf(number, sizeof(number), "%d", value);
			tt_set_size(bold, value > 999 ? 18 : value > 99 ? 21 : 25);
			int width = tt_string_width(bold, number);
			tt_draw_string(ctx, bold, px + (CELL - width) / 2, py + 57, number, RAZION_TEXT_PRIMARY);
		}
	}
	if (finished || reached_goal) {
		const char * title = finished ? "No moves left" : "2048 reached!";
		const char * detail = finished ? "Undo or start a new game" : "Keep going or start fresh";
		draw_rounded_rectangle(ctx, left + 70, top + 164, 280, 82, 10, premultiply(rgba(10,16,24,235)));
		tt_set_size(bold, 18); int width = tt_string_width(bold, title);
		tt_draw_string(ctx, bold, left + (420 - width) / 2, top + 196, title, RAZION_TEXT_PRIMARY);
		tt_set_size(font, 12); width = tt_string_width(font, detail);
		tt_draw_string(ctx, font, left + (420 - width) / 2, top + 222, detail, RAZION_TEXT_SECONDARY);
	}
	int buttons_y = top + 432;
	draw_button(0, left + 57, buttons_y, "New game  (R)", 1);
	draw_button(1, left + 221, buttons_y, "Undo  (U)", undo_available);
	render_decorations(window, ctx, "Razion Tiles"); flip(ctx); yutani_flip(yctx, window);
}

static int hit_test(int x, int y) {
	struct decor_bounds b; decor_get_bounds(window, &b);
	int left = (window->width - 420) / 2, buttons_y = b.top_height + 86 + 432;
	if (x >= left + 57 && x < left + 199 && y >= buttons_y && y < buttons_y + 44) return 0;
	if (x >= left + 221 && x < left + 363 && y >= buttons_y && y < buttons_y + 44) return 1;
	return -1;
}

static int activate(int id) {
	if (id == 0) { reset_game(); return 1; }
	if (id == 1) {
		if (!undo_available) { set_status("Nothing to undo yet."); return 1; }
		return undo_move();
	}
	return 0;
}

int main(void) {
	srand(time(NULL)); yctx = yutani_init(); if (!yctx) return 1; init_decorations();
	struct decor_bounds b; decor_get_bounds(NULL, &b);
	window = yutani_window_create(yctx, 500 + b.width, 610 + b.height);
	window->decorator_flags |= DECOR_FLAG_NO_MAXIMIZE;
	yutani_window_move(yctx, window, (yctx->display_width - window->width) / 2, (yctx->display_height - window->height) / 2);
	yutani_window_advertise_icon(yctx, window, "Razion Tiles", "razion-tiles");
	ctx = init_graphics_yutani_double_buffer(window); font = tt_font_from_shm("sans-serif"); bold = tt_font_from_shm("sans-serif.bold"); reset_game(); redraw();
	while (running) {
		yutani_msg_t * message = yutani_poll(yctx); if (!message) continue; int dirty = 0;
		switch (message->type) {
			case YUTANI_MSG_KEY_EVENT: {
				struct yutani_msg_key_event * key = (void *)message->data;
				if (key->wid != window->wid || key->event.action != KEY_ACTION_DOWN) break;
				if (key->event.keycode == KEY_ESCAPE) running = 0;
				else if (key->event.key == 'r' || key->event.key == 'R') dirty = activate(0);
				else if (key->event.key == 'u' || key->event.key == 'U') dirty = activate(1);
				else if (!finished) {
					int direction = -1;
					if (key->event.keycode == KEY_ARROW_LEFT || key->event.key == 'a' || key->event.key == 'A') direction = 0;
					else if (key->event.keycode == KEY_ARROW_RIGHT || key->event.key == 'd' || key->event.key == 'D') direction = 1;
					else if (key->event.keycode == KEY_ARROW_UP || key->event.key == 'w' || key->event.key == 'W') direction = 2;
					else if (key->event.keycode == KEY_ARROW_DOWN || key->event.key == 's' || key->event.key == 'S') direction = 3;
					if (direction >= 0) { dirty = 1; move_board(direction); if (!can_move()) finished = 1; }
				}
				break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)message->data; if (mouse->wid != window->wid) break;
				if (decor_handle_event(yctx, message) == DECOR_CLOSE) { running = 0; break; }
				int over = hit_test(mouse->new_x, mouse->new_y); if (over == 1 && !undo_available) over = -1;
				if (mouse->command == YUTANI_MOUSE_EVENT_DOWN) { if (pressed != over) dirty = 1; pressed = over; }
				else if (mouse->command == YUTANI_MOUSE_EVENT_LEAVE) { if (hover != -1 || pressed != -1) dirty = 1; hover = pressed = -1; }
				else if (mouse->command == YUTANI_MOUSE_EVENT_RAISE || mouse->command == YUTANI_MOUSE_EVENT_CLICK) { if (over >= 0 && over == pressed) dirty |= activate(over); if (pressed != -1) dirty = 1; pressed = -1; }
				if (hover != over) { hover = over; dirty = 1; }
				break;
			}
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE: { struct yutani_msg_window_focus_change * focus = (void *)message->data; if (focus->wid == window->wid) { window->focused = focus->focused; dirty = 1; } break; }
			case YUTANI_MSG_WINDOW_CLOSE: case YUTANI_MSG_SESSION_END: running = 0; break;
		}
		free(message); if (dirty && running) redraw();
	}
	yutani_close(yctx, window); return 0;
}
