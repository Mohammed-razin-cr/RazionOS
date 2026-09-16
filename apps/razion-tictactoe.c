/** @brief Razion Tic-Tac-Toe - a native keyboard and pointer strategy game. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define BOARD_CELLS 9
#define CELL_SIZE 108
#define CELL_GAP 12
#define BOARD_SIZE (CELL_SIZE * 3 + CELL_GAP * 2)

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font, * bold;

static char board[BOARD_CELLS];
static int selected = 4;
static int hover = -1;
static int pressed = -1;
static int running = 1;
static int game_over;
static char winner;
static int player_score;
static int computer_score;
static int draw_score;
static int8_t minimax_cache[2][19683];

static const int winning_lines[8][3] = {
	{0, 1, 2}, {3, 4, 5}, {6, 7, 8},
	{0, 3, 6}, {1, 4, 7}, {2, 5, 8},
	{0, 4, 8}, {2, 4, 6},
};

static char find_winner(void) {
	for (int i = 0; i < 8; ++i) {
		int a = winning_lines[i][0];
		int b = winning_lines[i][1];
		int c = winning_lines[i][2];
		if (board[a] && board[a] == board[b] && board[a] == board[c]) return board[a];
	}
	return 0;
}

static int board_full(void) {
	for (int i = 0; i < BOARD_CELLS; ++i) if (!board[i]) return 0;
	return 1;
}

static int board_key(void) {
	int key = 0;
	int place = 1;
	for (int i = 0; i < BOARD_CELLS; ++i) {
		int value = board[i] == 'X' ? 1 : board[i] == 'O' ? 2 : 0;
		key += value * place;
		place *= 3;
	}
	return key;
}

static int minimax(int computer_turn) {
	char result = find_winner();
	if (result == 'O') return 1;
	if (result == 'X') return -1;
	if (board_full()) return 0;

	int key = board_key();
	if (minimax_cache[computer_turn][key] != 2) return minimax_cache[computer_turn][key];
	int best = computer_turn ? -100 : 100;
	static const int move_order[BOARD_CELLS] = {4, 0, 2, 6, 8, 1, 3, 5, 7};
	for (int position = 0; position < BOARD_CELLS; ++position) {
		int i = move_order[position];
		if (board[i]) continue;
		board[i] = computer_turn ? 'O' : 'X';
		int score = minimax(!computer_turn);
		board[i] = 0;
		if (computer_turn && score > best) best = score;
		if (!computer_turn && score < best) best = score;
	}
	minimax_cache[computer_turn][key] = best;
	return minimax_cache[computer_turn][key];
}

static void finish_if_needed(void) {
	winner = find_winner();
	if (winner) {
		game_over = 1;
		if (winner == 'X') player_score++;
		else computer_score++;
	} else if (board_full()) {
		game_over = 1;
		draw_score++;
	}
}

static void computer_move(void) {
	int best_score = -100;
	int best_cell = -1;
	static const int move_order[BOARD_CELLS] = {4, 0, 2, 6, 8, 1, 3, 5, 7};
	for (int position = 0; position < BOARD_CELLS; ++position) {
		int i = move_order[position];
		if (board[i]) continue;
		board[i] = 'O';
		int score = minimax(0);
		board[i] = 0;
		if (score > best_score) {
			best_score = score;
			best_cell = i;
		}
	}
	if (best_cell >= 0) board[best_cell] = 'O';
	finish_if_needed();
}

static void reset_game(void) {
	memset(board, 0, sizeof(board));
	selected = 4;
	hover = -1;
	pressed = -1;
	game_over = 0;
	winner = 0;
}

static int play_cell(int cell) {
	if (game_over || cell < 0 || cell >= BOARD_CELLS || board[cell]) return 0;
	board[cell] = 'X';
	selected = cell;
	finish_if_needed();
	if (!game_over) computer_move();
	return 1;
}

static int winning_cell(int cell) {
	if (!winner) return 0;
	for (int i = 0; i < 8; ++i) {
		int a = winning_lines[i][0];
		int b = winning_lines[i][1];
		int c = winning_lines[i][2];
		if (board[a] == winner && board[b] == winner && board[c] == winner &&
			(cell == a || cell == b || cell == c)) return 1;
	}
	return 0;
}

static void draw_mark(int cell, int x, int y, uint32_t cell_color) {
	if (board[cell] == 'X') {
		draw_line_aa(ctx, x + 29, x + CELL_SIZE - 29, y + 29, y + CELL_SIZE - 29, RAZION_ACCENT, 7.0f);
		draw_line_aa(ctx, x + CELL_SIZE - 29, x + 29, y + 29, y + CELL_SIZE - 29, RAZION_ACCENT, 7.0f);
	} else if (board[cell] == 'O') {
		draw_rounded_rectangle(ctx, x + 27, y + 27, CELL_SIZE - 54, CELL_SIZE - 54,
			(CELL_SIZE - 54) / 2, RAZION_WARNING);
		draw_rounded_rectangle(ctx, x + 38, y + 38, CELL_SIZE - 76, CELL_SIZE - 76,
			(CELL_SIZE - 76) / 2, cell_color);
	}
}

static void centered_text(struct TT_Font * face, int size, int y, const char * text, uint32_t color) {
	tt_set_size(face, size);
	int width = tt_string_width(face, text);
	tt_draw_string(ctx, face, (window->width - width) / 2, y, text, color);
}

static void redraw(void) {
	struct decor_bounds b;
	decor_get_bounds(window, &b);
	draw_fill(ctx, RAZION_BACKGROUND);

	int content_left = b.left_width;
	int content_width = window->width - b.width;
	int board_left = content_left + (content_width - BOARD_SIZE) / 2;
	int board_top = b.top_height + 116;

	tt_set_size(bold, 23);
	tt_draw_string(ctx, bold, content_left + 32, b.top_height + 43,
		"Razion Tic-Tac-Toe", RAZION_TEXT_PRIMARY);
	tt_set_size(font, 11);
	tt_draw_string(ctx, font, content_left + 32, b.top_height + 65,
		"Player X  •  Computer O  •  Arrow keys + Enter", RAZION_TEXT_SECONDARY);

	char score[96];
	snprintf(score, sizeof(score), "You %d     Draws %d     Computer %d",
		player_score, draw_score, computer_score);
	centered_text(font, 11, b.top_height + 94, score, RAZION_TEXT_SECONDARY);

	for (int cell = 0; cell < BOARD_CELLS; ++cell) {
		int col = cell % 3;
		int row = cell / 3;
		int x = board_left + col * (CELL_SIZE + CELL_GAP);
		int y = board_top + row * (CELL_SIZE + CELL_GAP);
		uint32_t fill = RAZION_SURFACE_SECONDARY;
		if (winning_cell(cell)) fill = RAZION_SELECTION;
		else if (!game_over && cell == pressed) fill = RAZION_SELECTION;
		else if (!game_over && cell == hover && !board[cell]) fill = RAZION_SURFACE_HOVER;
		draw_rounded_rectangle(ctx, x, y, CELL_SIZE, CELL_SIZE, 10, fill);
		if (window->focused && cell == selected) {
			draw_rounded_rectangle(ctx, x + 3, y + 3, CELL_SIZE - 6, CELL_SIZE - 6, 8, RAZION_BORDER);
			draw_rounded_rectangle(ctx, x + 5, y + 5, CELL_SIZE - 10, CELL_SIZE - 10, 7, fill);
		}
		draw_mark(cell, x, y, fill);
	}

	const char * status = "Your turn — choose an empty square";
	uint32_t status_color = RAZION_TEXT_PRIMARY;
	if (winner == 'X') { status = "You win! Great line."; status_color = RAZION_SUCCESS; }
	else if (winner == 'O') { status = "Computer wins — try another round."; status_color = RAZION_WARNING; }
	else if (game_over) { status = "Draw — evenly matched."; status_color = RAZION_TEXT_PRIMARY; }
	centered_text(bold, 14, board_top + BOARD_SIZE + 40, status, status_color);

	int button_x = content_left + (content_width - 170) / 2;
	int button_y = board_top + BOARD_SIZE + 63;
	uint32_t button_fill = pressed == BOARD_CELLS ? RAZION_SELECTION :
		(hover == BOARD_CELLS ? RAZION_SURFACE_HOVER : RAZION_SURFACE);
	draw_rounded_rectangle(ctx, button_x, button_y, 170, 42, 8, button_fill);
	centered_text(font, 12, button_y + 26, "New round  (R)", RAZION_TEXT_PRIMARY);
	centered_text(font, 10, button_y + 66, "Esc closes the game", RAZION_TEXT_SECONDARY);

	render_decorations(window, ctx, "Razion Tic-Tac-Toe");
	flip(ctx);
	yutani_flip(yctx, window);
}

static int hit_test(int x, int y) {
	struct decor_bounds b;
	decor_get_bounds(window, &b);
	int content_left = b.left_width;
	int content_width = window->width - b.width;
	int board_left = content_left + (content_width - BOARD_SIZE) / 2;
	int board_top = b.top_height + 116;
	for (int cell = 0; cell < BOARD_CELLS; ++cell) {
		int col = cell % 3;
		int row = cell / 3;
		int left = board_left + col * (CELL_SIZE + CELL_GAP);
		int top = board_top + row * (CELL_SIZE + CELL_GAP);
		if (x >= left && x < left + CELL_SIZE && y >= top && y < top + CELL_SIZE) return cell;
	}
	int button_x = content_left + (content_width - 170) / 2;
	int button_y = board_top + BOARD_SIZE + 63;
	if (x >= button_x && x < button_x + 170 && y >= button_y && y < button_y + 42) return BOARD_CELLS;
	return -1;
}

static void move_selection(int dx, int dy) {
	int col = selected % 3;
	int row = selected / 3;
	col = (col + dx + 3) % 3;
	row = (row + dy + 3) % 3;
	selected = row * 3 + col;
}

int main(void) {
	yctx = yutani_init();
	if (!yctx) return 1;
	init_decorations();
	struct decor_bounds b;
	decor_get_bounds(NULL, &b);
	window = yutani_window_create(yctx, 520 + b.width, 590 + b.height);
	window->decorator_flags |= DECOR_FLAG_NO_MAXIMIZE;
	yutani_window_move(yctx, window,
		(yctx->display_width - window->width) / 2,
		(yctx->display_height - window->height) / 2);
	yutani_window_advertise_icon(yctx, window, "Razion Tic-Tac-Toe", "razion-tictactoe");
	ctx = init_graphics_yutani_double_buffer(window);
	font = tt_font_from_shm("sans-serif");
	bold = tt_font_from_shm("sans-serif.bold");
	memset(minimax_cache, 2, sizeof(minimax_cache));
	reset_game();
	redraw();

	while (running) {
		yutani_msg_t * message = yutani_poll(yctx);
		if (!message) continue;
		switch (message->type) {
			case YUTANI_MSG_KEY_EVENT: {
				struct yutani_msg_key_event * key = (void *)message->data;
				if (key->wid != window->wid || key->event.action != KEY_ACTION_DOWN) break;
				int dirty = 0;
				if (key->event.keycode == KEY_ESCAPE) running = 0;
				else if (key->event.key == 'r' || key->event.key == 'R') { reset_game(); dirty = 1; }
				else if (key->event.keycode == KEY_ARROW_LEFT) { move_selection(-1, 0); dirty = 1; }
				else if (key->event.keycode == KEY_ARROW_RIGHT) { move_selection(1, 0); dirty = 1; }
				else if (key->event.keycode == KEY_ARROW_UP) { move_selection(0, -1); dirty = 1; }
				else if (key->event.keycode == KEY_ARROW_DOWN) { move_selection(0, 1); dirty = 1; }
				else if (key->event.key == '\n' || key->event.key == ' ') dirty = play_cell(selected);
				if (dirty) redraw();
				break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)message->data;
				if (mouse->wid != window->wid) break;
				if (decor_handle_event(yctx, message) == DECOR_CLOSE) { running = 0; break; }
				int over = hit_test(mouse->new_x, mouse->new_y);
				int dirty = 0;
				if (mouse->command == YUTANI_MOUSE_EVENT_DOWN) {
					if (pressed != over) dirty = 1;
					pressed = over;
					if (over >= 0 && over < BOARD_CELLS && selected != over) {
						selected = over;
						dirty = 1;
					}
				} else if (mouse->command == YUTANI_MOUSE_EVENT_LEAVE) {
					if (hover != -1 || pressed != -1) dirty = 1;
					hover = -1;
					pressed = -1;
				} else if (mouse->command == YUTANI_MOUSE_EVENT_RAISE ||
					mouse->command == YUTANI_MOUSE_EVENT_CLICK) {
					if (over == pressed) {
						if (over == BOARD_CELLS) { reset_game(); dirty = 1; }
						else dirty |= play_cell(over);
					}
					if (pressed != -1) dirty = 1;
					pressed = -1;
				}
				if (hover != over) { hover = over; dirty = 1; }
				if (dirty) redraw();
				break;
			}
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE: {
				struct yutani_msg_window_focus_change * focus = (void *)message->data;
				if (focus->wid == window->wid) {
					window->focused = focus->focused;
					redraw();
				}
				break;
			}
			case YUTANI_MSG_WINDOW_CLOSE:
			case YUTANI_MSG_SESSION_END:
				running = 0;
				break;
		}
		free(message);
	}

	yutani_close(yctx, window);
	return 0;
}
