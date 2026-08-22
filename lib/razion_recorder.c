/**
 * @brief Lightweight uncompressed AVI writer for RazionOS screen recordings.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <toaru/razion_recorder.h>

#define MAX_FRAMES 150

typedef struct {
	uint32_t offset;
	uint32_t size;
} frame_index_t;

struct razion_avi_writer {
	FILE * file;
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	uint32_t row_size;
	uint32_t frame_size;
	uint32_t frames;
	long riff_size_pos;
	long total_frames_pos;
	long stream_length_pos;
	long movi_size_pos;
	long movi_data_start;
	frame_index_t index[MAX_FRAMES];
};

static void fourcc(FILE * file, const char * value) { fwrite(value, 1, 4, file); }
static void u16(FILE * file, uint16_t value) { fwrite(&value, 2, 1, file); }
static void u32(FILE * file, uint32_t value) { fwrite(&value, 4, 1, file); }
static void s32(FILE * file, int32_t value) { fwrite(&value, 4, 1, file); }

static void patch_u32(FILE * file, long position, uint32_t value) {
	long current = ftell(file);
	fseek(file, position, SEEK_SET);
	u32(file, value);
	fseek(file, current, SEEK_SET);
}

static long begin_list(FILE * file, const char * type) {
	fourcc(file, "LIST");
	long size_position = ftell(file);
	u32(file, 0);
	fourcc(file, type);
	return size_position;
}

static void end_list(FILE * file, long size_position) {
	long end = ftell(file);
	patch_u32(file, size_position, (uint32_t)(end - size_position - 4));
}

razion_avi_writer_t * razion_avi_open(const char * path, uint32_t width, uint32_t height, uint32_t fps) {
	if (!path || width < 2 || height < 2 || fps < 1 || fps > 30 || width > 1920 || height > 1080) return NULL;
	razion_avi_writer_t * writer = calloc(1, sizeof(*writer));
	if (!writer) return NULL;
	writer->file = fopen(path, "wb");
	if (!writer->file) { free(writer); return NULL; }
	writer->width = width;
	writer->height = height;
	writer->fps = fps;
	writer->row_size = (width * 3 + 3) & ~3U;
	writer->frame_size = writer->row_size * height;

	fourcc(writer->file, "RIFF");
	writer->riff_size_pos = ftell(writer->file);
	u32(writer->file, 0);
	fourcc(writer->file, "AVI ");

	long hdrl = begin_list(writer->file, "hdrl");
	fourcc(writer->file, "avih"); u32(writer->file, 56);
	u32(writer->file, 1000000 / fps);
	u32(writer->file, writer->frame_size * fps);
	u32(writer->file, 0); u32(writer->file, 0x10);
	writer->total_frames_pos = ftell(writer->file); u32(writer->file, 0);
	u32(writer->file, 0); u32(writer->file, 1); u32(writer->file, writer->frame_size);
	u32(writer->file, width); u32(writer->file, height);
	for (int i = 0; i < 4; ++i) u32(writer->file, 0);

	long strl = begin_list(writer->file, "strl");
	fourcc(writer->file, "strh"); u32(writer->file, 56);
	fourcc(writer->file, "vids"); fourcc(writer->file, "DIB ");
	u32(writer->file, 0); u16(writer->file, 0); u16(writer->file, 0);
	u32(writer->file, 0); u32(writer->file, 1); u32(writer->file, fps); u32(writer->file, 0);
	writer->stream_length_pos = ftell(writer->file); u32(writer->file, 0);
	u32(writer->file, writer->frame_size); u32(writer->file, 0xFFFFFFFF); u32(writer->file, 0);
	u16(writer->file, 0); u16(writer->file, 0); u16(writer->file, width); u16(writer->file, height);

	fourcc(writer->file, "strf"); u32(writer->file, 40);
	u32(writer->file, 40); s32(writer->file, width); s32(writer->file, height);
	u16(writer->file, 1); u16(writer->file, 24); u32(writer->file, 0);
	u32(writer->file, writer->frame_size); s32(writer->file, 0); s32(writer->file, 0);
	u32(writer->file, 0); u32(writer->file, 0);
	end_list(writer->file, strl);
	end_list(writer->file, hdrl);

	writer->movi_size_pos = begin_list(writer->file, "movi");
	writer->movi_data_start = ftell(writer->file);
	return writer;
}

int razion_avi_add_frame(razion_avi_writer_t * writer, gfx_context_t * source) {
	if (!writer || !writer->file || !source || writer->frames >= MAX_FRAMES) return 1;
	long chunk_position = ftell(writer->file);
	fourcc(writer->file, "00db");
	u32(writer->file, writer->frame_size);
	uint8_t padding[3] = {0};
	uint32_t source_stride = source->stride ? source->stride : source->width * 4;
	uint32_t * pixels = (uint32_t *)(source->backbuffer ? source->backbuffer : source->buffer);
	for (int32_t output_y = writer->height - 1; output_y >= 0; --output_y) {
		uint32_t source_y = (uint64_t)output_y * source->height / writer->height;
		uint32_t * row = (uint32_t *)((uint8_t *)pixels + source_y * source_stride);
		for (uint32_t x = 0; x < writer->width; ++x) {
			uint32_t source_x = (uint64_t)x * source->width / writer->width;
			uint32_t pixel = row[source_x];
			uint8_t bgr[3] = {_BLU(pixel), _GRE(pixel), _RED(pixel)};
			if (fwrite(bgr, 1, 3, writer->file) != 3) return 1;
		}
		uint32_t row_padding = writer->row_size - writer->width * 3;
		if (row_padding && fwrite(padding, 1, row_padding, writer->file) != row_padding) return 1;
	}
	writer->index[writer->frames].offset = (uint32_t)(chunk_position - writer->movi_data_start);
	writer->index[writer->frames].size = writer->frame_size;
	writer->frames++;
	return ferror(writer->file) ? 1 : 0;
}

uint32_t razion_avi_frame_count(razion_avi_writer_t * writer) {
	return writer ? writer->frames : 0;
}

int razion_avi_close(razion_avi_writer_t * writer) {
	if (!writer) return 1;
	int result = 0;
	if (writer->file) {
		long movi_end = ftell(writer->file);
		patch_u32(writer->file, writer->movi_size_pos, (uint32_t)(movi_end - writer->movi_size_pos - 4));
		fourcc(writer->file, "idx1"); u32(writer->file, writer->frames * 16);
		for (uint32_t i = 0; i < writer->frames; ++i) {
			fourcc(writer->file, "00db"); u32(writer->file, 0x10);
			u32(writer->file, writer->index[i].offset); u32(writer->file, writer->index[i].size);
		}
		long file_end = ftell(writer->file);
		patch_u32(writer->file, writer->total_frames_pos, writer->frames);
		patch_u32(writer->file, writer->stream_length_pos, writer->frames);
		patch_u32(writer->file, writer->riff_size_pos, (uint32_t)(file_end - 8));
		result = fclose(writer->file);
	}
	free(writer);
	return result;
}
