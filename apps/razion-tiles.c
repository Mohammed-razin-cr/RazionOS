/** @brief Razion Tiles - a lightweight native 2048 puzzle game. */
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

static yutani_t * yctx; static yutani_window_t * window; static gfx_context_t * ctx; static struct TT_Font * font, * bold;
static int board[4][4], score, running = 1, finished;

static void add_tile(void) {
	int empty[16][2], count = 0;
	for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) if (!board[y][x]) { empty[count][0] = x; empty[count++][1] = y; }
	if (!count) return;
	int pick = rand() % count;
	board[empty[pick][1]][empty[pick][0]] = rand() % 10 ? 2 : 4;
}

static void reset_game(void) { memset(board, 0, sizeof(board)); score = 0; finished = 0; add_tile(); add_tile(); }

static int compact_line(int line[4]) {
	int old[4]; memcpy(old, line, sizeof(old)); int out[4] = {0}, pos = 0;
	for (int i = 0; i < 4; ++i) if (line[i]) out[pos++] = line[i];
	for (int i = 0; i < 3; ++i) if (out[i] && out[i] == out[i+1]) { out[i] *= 2; score += out[i]; out[i+1] = 0; }
	int final[4] = {0}; pos = 0; for (int i = 0; i < 4; ++i) if (out[i]) final[pos++] = out[i];
	memcpy(line, final, sizeof(final)); return memcmp(old, line, sizeof(old)) != 0;
}

static int move_board(int direction) {
	int changed = 0, line[4];
	for (int a = 0; a < 4; ++a) {
		for (int i = 0; i < 4; ++i) {
			if (direction == 0) line[i] = board[a][i];
			else if (direction == 1) line[i] = board[a][3-i];
			else if (direction == 2) line[i] = board[i][a];
			else line[i] = board[3-i][a];
		}
		changed |= compact_line(line);
		for (int i = 0; i < 4; ++i) {
			if (direction == 0) board[a][i] = line[i];
			else if (direction == 1) board[a][3-i] = line[i];
			else if (direction == 2) board[i][a] = line[i];
			else board[3-i][a] = line[i];
		}
	}
	if (changed) add_tile();
	return changed;
}

static int can_move(void) {
	for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) if (!board[y][x] || (x < 3 && board[y][x] == board[y][x+1]) || (y < 3 && board[y][x] == board[y+1][x])) return 1;
	return 0;
}

static uint32_t tile_color(int value) {
	switch (value) { case 2:return rgb(48,64,80); case 4:return rgb(54,91,113); case 8:return rgb(38,139,139); case 16:return rgb(45,170,150); case 32:return rgb(69,125,222); case 64:return rgb(116,83,220); case 128:return rgb(220,132,62); case 256:return rgb(225,91,102); default:return value ? rgb(232,185,69) : RAZION_SURFACE_SECONDARY; }
}

static void redraw(void) {
	struct decor_bounds b; decor_get_bounds(window, &b); draw_fill(ctx, RAZION_BACKGROUND);
	int left = (window->width - 420) / 2, top = b.top_height + 82;
	tt_set_size(bold, 22); tt_draw_string(ctx, bold, b.left_width + 30, b.top_height + 39, "Razion Tiles", RAZION_TEXT_PRIMARY);
	tt_set_size(font, 11); tt_draw_string(ctx, font, b.left_width + 30, b.top_height + 60, "Combine matching tiles  •  Arrow keys move  •  R restarts", RAZION_TEXT_SECONDARY);
	char label[64]; snprintf(label, sizeof(label), "Score %d", score); tt_set_size(bold, 13); tt_draw_string(ctx, bold, window->width - b.right_width - 108, b.top_height + 39, label, RAZION_ACCENT);
	draw_rounded_rectangle(ctx, left - 10, top - 10, 440, 440, 10, RAZION_SURFACE);
	for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
		int px = left + x * 105, py = top + y * 105, value = board[y][x];
		draw_rounded_rectangle(ctx, px, py, 96, 96, 8, tile_color(value));
		if (value) { char number[16]; snprintf(number, sizeof(number), "%d", value); tt_set_size(bold, value > 999 ? 18 : 25); int width = tt_string_width(bold, number); tt_draw_string(ctx, bold, px + (96 - width) / 2, py + 57, number, RAZION_TEXT_PRIMARY); }
	}
	if (finished) { draw_rounded_rectangle(ctx, left + 74, top + 168, 272, 74, 8, premultiply(rgba(10,16,24,235))); tt_set_size(bold, 18); tt_draw_string(ctx, bold, left + 130, top + 198, "No moves left", RAZION_TEXT_PRIMARY); tt_set_size(font, 12); tt_draw_string(ctx, font, left + 111, top + 221, "Press R to restart", RAZION_TEXT_SECONDARY); }
	render_decorations(window, ctx, "Razion Tiles"); flip(ctx); yutani_flip(yctx, window);
}

int main(void) {
	srand(time(NULL)); yctx = yutani_init(); if (!yctx) return 1; init_decorations(); struct decor_bounds b; decor_get_bounds(NULL, &b);
	window = yutani_window_create(yctx, 500 + b.width, 570 + b.height); window->decorator_flags |= DECOR_FLAG_NO_MAXIMIZE;
	yutani_window_move(yctx, window, (yctx->display_width - window->width) / 2, (yctx->display_height - window->height) / 2);
	yutani_window_advertise_icon(yctx, window, "Razion Tiles", "razion-tiles"); ctx = init_graphics_yutani_double_buffer(window); font = tt_font_from_shm("sans-serif"); bold = tt_font_from_shm("sans-serif.bold"); reset_game(); redraw();
	while (running) { yutani_msg_t * message = yutani_poll(yctx); if (!message) continue;
		switch (message->type) {
			case YUTANI_MSG_KEY_EVENT: { struct yutani_msg_key_event * key = (void *)message->data; if (key->wid == window->wid && key->event.action == KEY_ACTION_DOWN) { if (key->event.keycode == KEY_ESCAPE) running = 0; else if (key->event.key == 'r' || key->event.key == 'R') reset_game(); else if (!finished) { int moved = 0; if (key->event.keycode == KEY_ARROW_LEFT) moved = move_board(0); else if (key->event.keycode == KEY_ARROW_RIGHT) moved = move_board(1); else if (key->event.keycode == KEY_ARROW_UP) moved = move_board(2); else if (key->event.keycode == KEY_ARROW_DOWN) moved = move_board(3); if (moved && !can_move()) finished = 1; } redraw(); } break; }
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: { struct yutani_msg_window_mouse_event * mouse = (void *)message->data; if (mouse->wid == window->wid && decor_handle_event(yctx, message) == DECOR_CLOSE) running = 0; break; }
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE: { struct yutani_msg_window_focus_change * focus = (void *)message->data; if (focus->wid == window->wid) { window->focused = focus->focused; redraw(); } break; }
			case YUTANI_MSG_WINDOW_CLOSE: case YUTANI_MSG_SESSION_END: running = 0; break;
		}
		free(message);
	}
	yutani_close(yctx, window); return 0;
}
