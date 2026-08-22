/**
 * @brief Native player for RazionOS uncompressed AVI screen recordings.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include <toaru/yutani.h>
#include <toaru/graphics.h>
#include <toaru/decorations.h>

static uint32_t read_u32(FILE * file) {
	uint32_t value = 0;
	fread(&value, 4, 1, file);
	return value;
}

static long find_chunk(FILE * file, const char * name, long limit) {
	char value[4];
	for (long position = 12; position < limit; ++position) {
		fseek(file, position, SEEK_SET);
		if (fread(value, 1, 4, file) != 4) return -1;
		if (!memcmp(value, name, 4)) return position;
	}
	return -1;
}

int main(int argc, char ** argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: razion-recording-player FILE.avi\n");
		return 2;
	}
	FILE * file = fopen(argv[1], "rb");
	if (!file) return 1;
	char signature[12];
	if (fread(signature, 1, 12, file) != 12 || memcmp(signature, "RIFF", 4) || memcmp(signature + 8, "AVI ", 4)) {
		fprintf(stderr, "%s: not an AVI recording\n", argv[1]);
		fclose(file);
		return 1;
	}

	long avih = find_chunk(file, "avih", 1048576);
	long strf = find_chunk(file, "strf", 1048576);
	long movi = find_chunk(file, "movi", 1048576);
	if (avih < 0 || strf < 0 || movi < 0) { fclose(file); return 1; }
	fseek(file, avih + 8, SEEK_SET);
	uint32_t frame_delay = read_u32(file);
	fseek(file, strf + 12, SEEK_SET);
	uint32_t width = read_u32(file);
	uint32_t height = read_u32(file);
	fseek(file, strf + 22, SEEK_SET);
	uint16_t bits = 0; fread(&bits, 2, 1, file);
	if (width < 2 || height < 2 || width > 1920 || height > 1080 || bits != 24 || frame_delay < 1000) {
		fclose(file);
		return 1;
	}
	uint32_t row_size = (width * 3 + 3) & ~3U;
	uint32_t frame_size = row_size * height;
	uint8_t * row = malloc(row_size);
	uint32_t * pixels = malloc(width * height * 4);
	if (!row || !pixels) { free(row); free(pixels); fclose(file); return 1; }

	yutani_t * yctx = yutani_init();
	if (!yctx) { free(row); free(pixels); fclose(file); return 1; }
	init_decorations();
	struct decor_bounds bounds;
	decor_get_bounds(NULL, &bounds);
	yutani_window_t * window = yutani_window_create_flags(yctx, width + bounds.width, height + bounds.height,
		YUTANI_WINDOW_FLAG_DISALLOW_RESIZE);
	yutani_window_move(yctx, window, (yctx->display_width - window->width) / 2, (yctx->display_height - window->height) / 2);
	char title[320];
	snprintf(title, sizeof(title), "%s - Recording Player", basename(argv[1]));
	yutani_window_advertise_icon(yctx, window, title, "video");
	gfx_context_t * ctx = init_graphics_yutani_double_buffer(window);
	sprite_t frame = {.width = width, .height = height, .bitmap = pixels};

	fseek(file, movi + 4, SEEK_SET);
	int running = 1;
	while (running) {
		char chunk[4];
		if (fread(chunk, 1, 4, file) != 4) break;
		uint32_t size = read_u32(file);
		if (!memcmp(chunk, "idx1", 4)) break;
		if (memcmp(chunk, "00db", 4) || size != frame_size) {
			fseek(file, size + (size & 1), SEEK_CUR);
			continue;
		}
		for (int32_t source_y = height - 1; source_y >= 0; --source_y) {
			if (fread(row, 1, row_size, file) != row_size) { running = 0; break; }
			for (uint32_t x = 0; x < width; ++x) {
				uint8_t * bgr = &row[x * 3];
				pixels[source_y * width + x] = 0xFF000000 | (bgr[2] << 16) | (bgr[1] << 8) | bgr[0];
			}
		}
		if (!running) break;
		draw_fill(ctx, rgb(0,0,0));
		draw_sprite(ctx, &frame, bounds.left_width, bounds.top_height);
		render_decorations(window, ctx, title);
		flip(ctx);
		yutani_flip(yctx, window);

		yutani_msg_t * message;
		while ((message = yutani_poll_async(yctx))) {
			if (message->type == YUTANI_MSG_WINDOW_CLOSE || message->type == YUTANI_MSG_SESSION_END) running = 0;
			if (message->type == YUTANI_MSG_WINDOW_MOUSE_EVENT && decor_handle_event(yctx, message) == DECOR_CLOSE) running = 0;
			free(message);
		}
		if (running) usleep(frame_delay);
	}

	yutani_close(yctx, window);
	free(row); free(pixels); fclose(file);
	return 0;
}
