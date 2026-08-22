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

/* Razion Dark */
#define RAZION_DARK_BACKGROUND        rgb(17,22,29)
#define RAZION_DARK_SURFACE           rgb(25,32,41)
#define RAZION_DARK_SURFACE_SECONDARY rgb(32,41,52)
#define RAZION_DARK_SURFACE_HOVER     rgb(43,55,68)
#define RAZION_DARK_BORDER            rgb(59,73,87)
#define RAZION_DARK_TEXT_PRIMARY      rgb(235,241,245)
#define RAZION_DARK_TEXT_SECONDARY    rgb(157,171,183)
#define RAZION_DARK_ACCENT            rgb(41,196,180)
#define RAZION_DARK_ACCENT_HOVER      rgb(76,216,200)
#define RAZION_DARK_SUCCESS           rgb(79,195,134)
#define RAZION_DARK_WARNING           rgb(235,180,74)
#define RAZION_DARK_ERROR             rgb(235,94,103)
#define RAZION_DARK_SELECTION         rgb(35,117,126)
#define RAZION_DARK_FOCUS             rgb(88,224,208)

/* Razion Light — reserved for the future runtime theme switcher. */
#define RAZION_LIGHT_BACKGROUND        rgb(238,242,244)
#define RAZION_LIGHT_SURFACE           rgb(250,252,253)
#define RAZION_LIGHT_SURFACE_SECONDARY rgb(226,233,236)
#define RAZION_LIGHT_SURFACE_HOVER     rgb(216,226,230)
#define RAZION_LIGHT_BORDER            rgb(185,198,204)
#define RAZION_LIGHT_TEXT_PRIMARY      rgb(25,34,40)
#define RAZION_LIGHT_TEXT_SECONDARY    rgb(82,99,108)
#define RAZION_LIGHT_ACCENT            rgb(0,137,126)
#define RAZION_LIGHT_ACCENT_HOVER      rgb(0,112,104)
#define RAZION_LIGHT_SUCCESS           rgb(30,135,79)
#define RAZION_LIGHT_WARNING           rgb(171,112,0)
#define RAZION_LIGHT_ERROR             rgb(190,52,62)
#define RAZION_LIGHT_SELECTION         rgb(181,231,225)
#define RAZION_LIGHT_FOCUS             rgb(0,137,126)

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
