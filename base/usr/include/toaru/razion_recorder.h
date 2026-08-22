#pragma once

#include <stdint.h>
#include <toaru/graphics.h>

typedef struct razion_avi_writer razion_avi_writer_t;

razion_avi_writer_t * razion_avi_open(const char * path, uint32_t width, uint32_t height, uint32_t fps);
int razion_avi_add_frame(razion_avi_writer_t * writer, gfx_context_t * source);
int razion_avi_close(razion_avi_writer_t * writer);
uint32_t razion_avi_frame_count(razion_avi_writer_t * writer);
