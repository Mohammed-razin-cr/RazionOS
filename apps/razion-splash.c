/**
 * @file apps/razion-splash.c
 * @brief Milestone-driven RazionOS boot splash.
 *
 * The renderer owns the framebuffer only during the init-script phase. It
 * receives actual startup milestones through the existing `splash` PEX
 * endpoint, then releases the framebuffer before the compositor starts.
 *
 * Copyright (C) 2026 Mohammed Razin
 * SPDX-License-Identifier: MIT
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fswait.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <toaru/graphics.h>
#include <toaru/pex.h>
#include <toaru/text.h>

#define MATTE_BLACK rgb(9,10,12)
#define ELECTRIC_BLUE rgb(78,163,255)
#define SOFT_CYAN rgb(110,219,255)
#define SOFT_WHITE rgb(245,248,252)
#define MUTED_TEXT rgb(163,177,192)

#define FRAME_MS 33
#define INTRO_MS 520
#define EXIT_MS 360

struct splash_stage {
	const char * event;
	const char * label;
	int target;
};

static struct splash_stage stages[] = {
	{"kernel",   "Initializing Razion Kernel", 12},
	{"core",     "Loading Core Services",       34},
	{"services", "Starting Device Services",    58},
	{"graphics", "Starting Graphics",           78},
	{"desktop",  "Launching Razion Desktop",    92},
	{"ready",    "Ready",                      100},
	{NULL, NULL, 0},
};

struct logo_segment {
	int x1, y1, x2, y2;
};

/* Original monoline Razion mark: a composed R with a split diagonal. */
static struct logo_segment logo_segments[] = {
	{-28,-42, -28, 42},
	{-28,-42,  16,-42},
	{ 16,-42,  31,-27},
	{ 31,-27,  31, -8},
	{ 31, -8,  16,  4},
	{ 16,  4, -28,  4},
	{ -6,  4,  34, 42},
};

static uint64_t monotonic_ms(void) {
	struct timeval now;
	gettimeofday(&now, NULL);
	return (uint64_t)now.tv_sec * 1000 + now.tv_usec / 1000;
}

static int clamp_int(int value, int low, int high) {
	if (value < low) return low;
	if (value > high) return high;
	return value;
}

static uint32_t faded(uint32_t color, int opacity) {
	opacity = clamp_int(opacity, 0, 255);
	return premultiply(rgba(_RED(color), _GRE(color), _BLU(color), opacity));
}

static int has_option(const char * option) {
	FILE * cmdline = fopen("/proc/cmdline", "r");
	if (!cmdline) return 0;

	char buffer[2048] = {0};
	fread(buffer, 1, sizeof(buffer) - 1, cmdline);
	fclose(cmdline);

	char * token = strtok(buffer, " \n");
	while (token) {
		if (!strcmp(token, option)) return 1;
		token = strtok(NULL, " \n");
	}
	return 0;
}

static int animation_enabled(void) {
	return !has_option("boot-animation=off") && !has_option("boot-verbose") && !has_option("debug");
}

static int framebuffer_supported(gfx_context_t * ctx) {
	if (!ctx || ctx->size == 0) return 0;

	/* VboxVGA's legacy early framebuffer is not reliable for direct rendering. */
	FILE * pci = fopen("/proc/pci", "r");
	if (!pci) return 1;
	char buffer[2048] = {0};
	size_t length = fread(buffer, 1, sizeof(buffer) - 1, pci);
	fclose(pci);
	return !length || !strstr(buffer, "80ee:beef");
}

static int animation_step(void) {
	if (has_option("boot-animation-speed=fast")) return 18;
	if (has_option("boot-animation-speed=slow")) return 7;
	return 11;
}

static void say_hello(void) {
	struct utsname u;
	uname(&u);
	char * separator = strstr(u.release, "-");
	if (separator) *separator = '\0';
	printf("RazionOS 0.1 Alpha is starting up (kernel %s)...\n", u.release);
}

static void draw_partial_segment(gfx_context_t * ctx, int center_x, int center_y,
	int scale, struct logo_segment * segment, int amount, uint32_t color, float width) {
	if (amount <= 0) return;
	if (amount > 100) amount = 100;

	int x1 = center_x + segment->x1 * scale / 100;
	int y1 = center_y + segment->y1 * scale / 100;
	int x2 = center_x + segment->x2 * scale / 100;
	int y2 = center_y + segment->y2 * scale / 100;
	int end_x = x1 + (x2 - x1) * amount / 100;
	int end_y = y1 + (y2 - y1) * amount / 100;
	draw_line_aa(ctx, x1, end_x, y1, end_y, color, width);
}

static void draw_logo(gfx_context_t * ctx, int center_x, int center_y, int scale,
	int reveal, int opacity) {
	int segment_count = sizeof(logo_segments) / sizeof(logo_segments[0]);
	for (int i = 0; i < segment_count; ++i) {
		int local = clamp_int(reveal - i * 100 / segment_count, 0, 100 / segment_count);
		local = local * segment_count;
		if (reveal > 70) {
			draw_partial_segment(ctx, center_x, center_y, scale, &logo_segments[i], local,
				faded(ELECTRIC_BLUE, opacity / 5), 5.2f * scale / 100.0f);
		}
		draw_partial_segment(ctx, center_x, center_y, scale, &logo_segments[i], local,
			faded(SOFT_CYAN, opacity), 1.65f * scale / 100.0f);
	}
}

static void draw_centered(gfx_context_t * ctx, struct TT_Font * font, const char * text,
	int y, uint32_t color) {
	int width = tt_string_width(font, text);
	tt_draw_string(ctx, font, (ctx->width - width) / 2, y, text, color);
}

static void render(gfx_context_t * ctx, struct TT_Font * font, uint64_t started,
	int visible_progress, int target_progress, const char * stage, uint64_t exiting) {
	uint64_t now = monotonic_ms();
	int elapsed = now - started;
	int intro = clamp_int(elapsed * 100 / INTRO_MS, 0, 100);
	int reveal = clamp_int((elapsed - 90) * 100 / INTRO_MS, 0, 100);
	int opacity = intro * 255 / 100;
	int scale = 100;

	if (exiting) {
		int out = clamp_int((now - exiting) * 100 / EXIT_MS, 0, 100);
		opacity = opacity * (100 - out) / 100;
		scale = 100 - out * 12 / 100;
	}

	draw_fill(ctx, MATTE_BLACK);

	int center_y = ctx->height / 2 - 48;
	draw_logo(ctx, ctx->width / 2, center_y, scale, reveal, opacity);

	tt_set_size_px(font, 22 * scale / 100);
	draw_centered(ctx, font, "RazionOS", center_y + 78 * scale / 100, faded(SOFT_WHITE, opacity));

	int line_width = ctx->width < 640 ? 210 : 280;
	int line_x = (ctx->width - line_width) / 2;
	int line_y = center_y + 116 * scale / 100;
	draw_rounded_rectangle(ctx, line_x, line_y, line_width, 2, 1, faded(MUTED_TEXT, opacity / 3));
	draw_rounded_rectangle(ctx, line_x, line_y, line_width * visible_progress / 100, 2, 1, faded(ELECTRIC_BLUE, opacity));

	tt_set_size_px(font, 13);
	draw_centered(ctx, font, stage, line_y + 30, faded(MUTED_TEXT, opacity));

	/* Keep the displayed value event-driven; never advance it without a milestone. */
	(void)target_progress;
	flip(ctx);
}

static int set_stage(const char * message, const char ** label) {
	if (strncmp(message, "@razion:", 8)) return -1;
	const char * event = message + 8;
	for (int i = 0; stages[i].event; ++i) {
		if (!strcmp(event, stages[i].event)) {
			*label = stages[i].label;
			return stages[i].target;
		}
	}
	return -1;
}

int main(int argc, char * argv[]) {
	(void)argc;
	if (getuid() != 0) {
		fprintf(stderr, "%s: only root should run this\n", argv[0]);
		return 1;
	}

	if (fork()) return 0;

	int graphical = animation_enabled();
	gfx_context_t * ctx = graphical ? init_graphics_fullscreen_double_buffer() : NULL;
	struct TT_Font * font = ctx ? tt_font_from_file("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf") : NULL;
	/* Legacy framebuffer paths use the proven text-mode fallback. */
	if (!font || !framebuffer_supported(ctx)) graphical = 0;

	FILE * endpoint = pex_bind("splash");
	if (!endpoint) return fprintf(stderr, "%s: pex: %s\n", argv[0], strerror(errno)), 1;

	int console = open("/dev/console", O_APPEND | O_WRONLY);
	if (console == -1) return fprintf(stderr, "%s: /dev/console: %s\n", argv[0], strerror(errno)), 1;
	dup2(console, STDOUT_FILENO);
	close(console);

	if (!graphical) say_hello();

	const char * stage = stages[0].label;
	int target_progress = stages[0].target;
	int visible_progress = 0;
	int step = animation_step();
	uint64_t started = monotonic_ms();
	uint64_t exiting = 0;
	int pex_fd[] = {fileno(endpoint)};

	while (1) {
		int wait = graphical ? FRAME_MS : 100;
		int index = fswait2(1, pex_fd, wait);

		if (index == 0) {
			pex_packet_t * packet = calloc(1, PACKET_SIZE + 1);
			pex_listen(endpoint, packet);
			packet->data[packet->size] = '\0';
			char * message = (char *)packet->data;

			if (!strncmp(message, "!ready", 6) || !strncmp(message, "!quit", 5)) {
				stage = stages[5].label;
				target_progress = stages[5].target;
				if (graphical) exiting = monotonic_ms();
				else { free(packet); break; }
			} else {
				const char * stage_label = stage;
				int target = set_stage(message, &stage_label);
				if (target >= 0) {
					stage = stage_label;
					target_progress = target;
					if (!graphical) printf("%s\n", stage);
				} else if (!graphical && packet->size >= 4 && packet->size <= 160) {
					printf("%s\n", message[0] == ':' ? message + 1 : message);
				}
			}
			free(packet);
		}

		if (!graphical) continue;
		if (visible_progress < target_progress) {
			visible_progress += step;
			if (visible_progress > target_progress) visible_progress = target_progress;
		}
		render(ctx, font, started, visible_progress, target_progress, stage, exiting);
		if (exiting && monotonic_ms() - exiting >= EXIT_MS) break;
	}

	fclose(endpoint);
	return 0;
}
