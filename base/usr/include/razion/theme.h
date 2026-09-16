/**
 * @brief RazionOS semantic design tokens.
 *
 * Theme selection is read once per process from ~/.razion/theme.conf. This
 * keeps drawing paths fast while allowing new applications to honor the
 * user's Dark or Light preference.
 */
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <toaru/graphics.h>
#include <toaru/text.h>

/* Razion Aurora Dark — soft desktop surfaces with an original blue identity. */
#define RAZION_DARK_BACKGROUND        rgb(11,13,17)
#define RAZION_DARK_SURFACE           rgb(28,31,37)
#define RAZION_DARK_SURFACE_SECONDARY rgb(38,42,50)
#define RAZION_DARK_SURFACE_HOVER     rgb(51,57,68)
#define RAZION_DARK_BORDER            rgb(72,80,95)
#define RAZION_DARK_TEXT_PRIMARY      rgb(246,248,251)
#define RAZION_DARK_TEXT_SECONDARY    rgb(174,182,194)
#define RAZION_DARK_ACCENT            rgb(74,163,255)
#define RAZION_DARK_ACCENT_HOVER      rgb(110,219,255)
#define RAZION_DARK_SUCCESS           rgb(72,199,142)
#define RAZION_DARK_WARNING           rgb(246,190,87)
#define RAZION_DARK_ERROR             rgb(255,95,109)
#define RAZION_DARK_SELECTION         rgb(32,92,158)
#define RAZION_DARK_FOCUS             rgb(110,219,255)

/* Razion Aurora Light — bright, airy, and readable. */
#define RAZION_LIGHT_BACKGROUND        rgb(242,244,248)
#define RAZION_LIGHT_SURFACE           rgb(252,253,255)
#define RAZION_LIGHT_SURFACE_SECONDARY rgb(233,238,246)
#define RAZION_LIGHT_SURFACE_HOVER     rgb(222,232,245)
#define RAZION_LIGHT_BORDER            rgb(190,202,218)
#define RAZION_LIGHT_TEXT_PRIMARY      rgb(23,28,36)
#define RAZION_LIGHT_TEXT_SECONDARY    rgb(80,91,106)
#define RAZION_LIGHT_ACCENT            rgb(0,113,227)
#define RAZION_LIGHT_ACCENT_HOVER      rgb(0,88,179)
#define RAZION_LIGHT_SUCCESS           rgb(31,143,85)
#define RAZION_LIGHT_WARNING           rgb(170,104,0)
#define RAZION_LIGHT_ERROR             rgb(201,45,59)
#define RAZION_LIGHT_SELECTION         rgb(205,226,255)
#define RAZION_LIGHT_FOCUS             rgb(0,113,227)

static inline int razion_theme_is_light(void) {
	static int cached = -1;
	if (cached >= 0) return cached;
	cached = 0;
	const char * home = getenv("HOME");
	if (!home) return cached;
	char path[512];
	int written = snprintf(path, sizeof(path), "%s/.razion/theme.conf", home);
	if (written < 0 || (size_t)written >= sizeof(path)) return cached;
	FILE * config = fopen(path, "r");
	if (!config) return cached;
	char line[64];
	while (fgets(line, sizeof(line), config)) {
		if (!strncmp(line, "theme=light", 11)) {
			cached = 1;
			break;
		} else if (!strncmp(line, "theme=auto", 10)) {
			time_t now = time(NULL);
			struct tm * local = localtime(&now);
			cached = local && local->tm_hour >= 7 && local->tm_hour < 19;
			break;
		}
	}
	fclose(config);
	return cached;
}

static inline uint32_t razion_theme_accent(void) {
	static uint32_t cached;
	if (cached) return cached;
	cached = razion_theme_is_light() ? RAZION_LIGHT_ACCENT : RAZION_DARK_ACCENT;
	const char * home = getenv("HOME");
	if (!home) return cached;
	char path[512];
	if (snprintf(path, sizeof(path), "%s/.razion/theme.conf", home) >= (int)sizeof(path)) return cached;
	FILE * config = fopen(path, "r");
	if (!config) return cached;
	char line[64];
	while (fgets(line, sizeof(line), config)) {
		if (!strncmp(line, "accent=blue", 11)) cached = rgb(74, 145, 247);
		else if (!strncmp(line, "accent=violet", 13)) cached = rgb(151, 105, 245);
		else if (!strncmp(line, "accent=orange", 13)) cached = rgb(235, 154, 72);
		else if (!strncmp(line, "accent=rose", 11)) cached = rgb(226, 92, 132);
		else if (!strncmp(line, "accent=teal", 11)) cached = razion_theme_is_light() ? RAZION_LIGHT_ACCENT : RAZION_DARK_ACCENT;
	}
	fclose(config);
	return cached;
}

/* Active aliases. Keep consumers semantic rather than palette-specific. */
#define RAZION_BACKGROUND        (razion_theme_is_light() ? RAZION_LIGHT_BACKGROUND : RAZION_DARK_BACKGROUND)
#define RAZION_SURFACE           (razion_theme_is_light() ? RAZION_LIGHT_SURFACE : RAZION_DARK_SURFACE)
#define RAZION_SURFACE_SECONDARY (razion_theme_is_light() ? RAZION_LIGHT_SURFACE_SECONDARY : RAZION_DARK_SURFACE_SECONDARY)
#define RAZION_SURFACE_HOVER     (razion_theme_is_light() ? RAZION_LIGHT_SURFACE_HOVER : RAZION_DARK_SURFACE_HOVER)
#define RAZION_BORDER            (razion_theme_is_light() ? RAZION_LIGHT_BORDER : RAZION_DARK_BORDER)
#define RAZION_TEXT_PRIMARY      (razion_theme_is_light() ? RAZION_LIGHT_TEXT_PRIMARY : RAZION_DARK_TEXT_PRIMARY)
#define RAZION_TEXT_SECONDARY    (razion_theme_is_light() ? RAZION_LIGHT_TEXT_SECONDARY : RAZION_DARK_TEXT_SECONDARY)
#define RAZION_ACCENT            (razion_theme_accent())
#define RAZION_ACCENT_HOVER      (razion_theme_is_light() ? RAZION_LIGHT_ACCENT_HOVER : RAZION_DARK_ACCENT_HOVER)
#define RAZION_SUCCESS           (razion_theme_is_light() ? RAZION_LIGHT_SUCCESS : RAZION_DARK_SUCCESS)
#define RAZION_WARNING           (razion_theme_is_light() ? RAZION_LIGHT_WARNING : RAZION_DARK_WARNING)
#define RAZION_ERROR             (razion_theme_is_light() ? RAZION_LIGHT_ERROR : RAZION_DARK_ERROR)
#define RAZION_SELECTION         (razion_theme_is_light() ? RAZION_LIGHT_SELECTION : RAZION_DARK_SELECTION)
#define RAZION_FOCUS             (razion_theme_is_light() ? RAZION_LIGHT_FOCUS : RAZION_DARK_FOCUS)

/* Shared desktop interaction states keep controls consistent across apps. */
enum {
	RAZION_CONTROL_NORMAL   = 0,
	RAZION_CONTROL_HOVER    = 1 << 0,
	RAZION_CONTROL_PRESSED  = 1 << 1,
	RAZION_CONTROL_FOCUSED  = 1 << 2,
	RAZION_CONTROL_PRIMARY  = 1 << 3,
	RAZION_CONTROL_DISABLED = 1 << 4,
};

static inline uint32_t razion_scrim(void) {
	return premultiply(razion_theme_is_light() ? rgba(255, 255, 255, 230) : rgba(10, 13, 18, 232));
}

static inline uint32_t razion_glass(void) {
	return premultiply(razion_theme_is_light() ? rgba(255, 255, 255, 214) : rgba(25, 29, 36, 226));
}

static inline uint32_t razion_glass_border(void) {
	return premultiply(razion_theme_is_light() ? rgba(255, 255, 255, 160) : rgba(255, 255, 255, 40));
}

static inline uint32_t razion_shadow(void) {
	return premultiply(razion_theme_is_light() ? rgba(46, 63, 89, 42) : rgba(0, 0, 0, 86));
}

static inline void razion_draw_focus(gfx_context_t * ctx, int x, int y, int w, int h, int radius) {
	draw_rounded_rectangle(ctx, x - 2, y - 2, w + 4, h + 4, radius + 2, RAZION_FOCUS);
}

static inline void razion_draw_card(gfx_context_t * ctx, int x, int y, int w, int h, uint32_t fill) {
	draw_rounded_rectangle(ctx, x, y + 2, w, h, 12, razion_shadow());
	draw_rounded_rectangle(ctx, x, y, w, h, 12, fill);
	draw_rounded_rectangle(ctx, x, y, w, 1, 1, razion_glass_border());
	draw_rectangle_solid(ctx, x + 12, y + h - 1, w - 24, 1, RAZION_BORDER);
}

static inline void razion_draw_glass_panel(gfx_context_t * ctx, int x, int y, int w, int h, int radius) {
	draw_rounded_rectangle(ctx, x, y + 3, w, h, radius, razion_shadow());
	draw_rounded_rectangle(ctx, x, y, w, h, radius, razion_glass());
	draw_rounded_rectangle(ctx, x + 1, y + 1, w - 2, 1, 1, razion_glass_border());
}

static inline void razion_draw_accent_bar(gfx_context_t * ctx, int x, int y, int w) {
	draw_rounded_rectangle(ctx, x, y, w, 3, 2, RAZION_ACCENT);
}

static inline void razion_draw_status_dot(gfx_context_t * ctx, int x, int y, uint32_t color) {
	draw_rounded_rectangle(ctx, x, y, 8, 8, 4, color);
}

static inline void razion_draw_chip(gfx_context_t * ctx, struct TT_Font * font,
	int x, int y, const char * text, uint32_t color) {
	int width = tt_string_width(font, text) + 22;
	draw_rounded_rectangle(ctx, x, y, width, 24, 12, razion_glass());
	razion_draw_status_dot(ctx, x + 8, y + 8, color);
	tt_draw_string(ctx, font, x + 22, y + 16, text, RAZION_TEXT_SECONDARY);
}

static inline void razion_draw_button(gfx_context_t * ctx, struct TT_Font * font,
	int x, int y, int w, int h, const char * text, unsigned state) {
	if (state & RAZION_CONTROL_FOCUSED) {
		razion_draw_focus(ctx, x, y, w, h, 9);
	}
	uint32_t fill = state & RAZION_CONTROL_DISABLED ? RAZION_SURFACE_SECONDARY :
		state & RAZION_CONTROL_PRESSED ? RAZION_SELECTION :
		state & RAZION_CONTROL_PRIMARY ? RAZION_ACCENT :
		state & RAZION_CONTROL_HOVER ? RAZION_SURFACE_HOVER : RAZION_SURFACE_SECONDARY;
	draw_rounded_rectangle(ctx, x, y, w, h, 9, fill);
	draw_rectangle_solid(ctx, x + 12, y + h - 1, w - 24, 1,
		state & RAZION_CONTROL_PRIMARY ? RAZION_ACCENT_HOVER : RAZION_BORDER);
	tt_set_size(font, 12);
	int text_width = tt_string_width(font, text);
	uint32_t text_color = state & RAZION_CONTROL_DISABLED ?
		RAZION_TEXT_SECONDARY : RAZION_TEXT_PRIMARY;
	tt_draw_string(ctx, font, x + (w - text_width) / 2,
		y + (h + 12) / 2, text, text_color);
}

static inline void razion_draw_loading(gfx_context_t * ctx, int x, int y,
	unsigned phase, uint32_t color) {
	static const int points[8][2] = {
		{0,-7}, {5,-5}, {7,0}, {5,5}, {0,7}, {-5,5}, {-7,0}, {-5,-5}
	};
	for (unsigned i = 0; i < 8; ++i) {
		uint32_t dot = i == phase % 8 ? color : RAZION_BORDER;
		int size = i == phase % 8 ? 4 : 3;
		draw_rounded_rectangle(ctx, x + points[i][0] - size / 2,
			y + points[i][1] - size / 2, size, size, size / 2, dot);
	}
}
