/** @brief Razion lock overlay. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <razion/theme.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font, * bold;
static int running = 1;

static void centered(struct TT_Font * face, int size, int y,
	const char * text, uint32_t color) {
	tt_set_size(face, size);
	int width = tt_string_width(face, text);
	tt_draw_string(ctx, face, (ctx->width - width) / 2, y, text, color);
}

static void redraw(void) {
	draw_fill(ctx, rgb(9, 10, 12));
	for (int i = 0; i < 5; ++i) {
		draw_rounded_rectangle(ctx, ctx->width / 2 - 42 + i * 18,
			ctx->height / 2 - 116 + i * 2, 12, 12, 6,
			i == 2 ? RAZION_ACCENT : RAZION_SURFACE_HOVER);
	}
	centered(bold, 34, ctx->height / 2 - 42, "RazionOS", RAZION_TEXT_PRIMARY);
	centered(font, 13, ctx->height / 2 - 12,
		"Locked session - press Enter to return", RAZION_TEXT_SECONDARY);
	centered(font, 10, ctx->height - 44,
		"Current alpha lock screen is a presentation lock, not a password security boundary.",
		RAZION_TEXT_SECONDARY);
	flip(ctx);
	yutani_flip(yctx, window);
}

int main(void) {
	yctx = yutani_init();
	if (!yctx) return 1;
	window = yutani_window_create_flags(yctx, yctx->display_width, yctx->display_height,
		YUTANI_WINDOW_FLAG_DISALLOW_DRAG |
		YUTANI_WINDOW_FLAG_DISALLOW_RESIZE |
		YUTANI_WINDOW_FLAG_NO_ANIMATION);
	if (!window) return 1;
	yutani_window_move(yctx, window, 0, 0);
	yutani_set_stack(yctx, window, YUTANI_ZORDER_OVERLAY);
	yutani_window_advertise(yctx, window, "RazionOS Locked");
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
				if (key->wid == window->wid && key->event.action == KEY_ACTION_DOWN &&
					(key->event.key == '\n' || key->event.keycode == KEY_ESCAPE)) running = 0;
				break;
			}
			case YUTANI_MSG_WINDOW_CLOSE:
			case YUTANI_MSG_SESSION_END:
				running = 0;
				break;
		}
		free(msg);
	}
	yutani_close(yctx, window);
	return 0;
}
