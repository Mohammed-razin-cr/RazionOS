/**
 * @brief Razion Store - native, fail-closed software catalogue.
 *
 * The first milestone intentionally exposes only software already present in
 * the RazionOS image. Repository transactions remain disabled until rzpkg has
 * a signed index, signature verifier, dependency resolver, permission broker,
 * and privileged transaction service.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fswait.h>
#include <sys/stat.h>
#include <unistd.h>

#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/icon_cache.h>
#include <toaru/kbd.h>
#include <toaru/razion_ai.h>
#include <toaru/rzpkg.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define STORE_WIDTH 1120
#define STORE_HEIGHT 680
#define SEARCH_MAX 96

typedef struct {
	const char * name;
	const char * package;
	const char * description;
	const char * version;
	const char * size;
	const char * category;
	const char * icon;
	const char * executable;
	const char * license;
	const char * dependencies;
	const char * permissions;
	const char * changelog;
	int featured;
} store_app_t;

static const store_app_t catalogue[] = {
	{"Razion Store", "razion-store", "Browse verified system applications and inspect future package security requirements.", "0.1.0", "System image", "System", "razion-store", "/bin/razion-store", "NCSA", "rzpkg metadata, Razion graphics", "installed applications: read; optional local AI", "Added fail-closed repository and permission surfaces.", 1},
	{"Ripper", "ripper", "Native first-party browser shell with explicit web-engine and offline status.", "0.1.0", "System image", "Internet", "razion-ripper", "/bin/ripper", "NCSA", "Razion graphics, network status", "network status: read; optional local AI", "Added tabs, history, bookmarks, downloads, and privacy settings.", 1},
	{"Desktop Companion", "razion-companion", "An offline companion with care, activities, customization, and desktop mode.", "0.1.0", "974 KiB", "Utilities", "star", "/bin/razion-companion", "NCSA", "Razion graphics, Yutani", "desktop overlay; local settings", "Native dashboard and 22 companion species.", 1},
	{"Razion Launcher", "universal-search", "Fast local search for applications, settings, files, and folders.", "0.1.0", "148 KiB", "Utilities", "razion-launcher", "/bin/universal-search", "NCSA", "Razion Search", "user files: read metadata", "Added bounded offline file search.", 1},
	{"Privacy Center", "razion-privacy", "Inspect real privacy capabilities, AI policy, and recent AI activity without simulated permissions.", "0.1.0", "System image", "System", "razion-settings", "/bin/razion-privacy", "NCSA", "Razion graphics, AI policy", "system policy: read; AI audit metadata: read", "Added explicit status for available and unavailable permission controls.", 1},
	{"Bim Editor", "bim", "Lightweight terminal text editor with syntax highlighting.", "3.x", "682 KiB", "Development", "accessories-text-editor", "/bin/bim", "NCSA", "Kuroko runtime components", "user files: read/write", "Bundled editor from the base system.", 1},
	{"Terminal", "terminal", "Native terminal emulator for the Razion shell environment.", "2.x", "416 KiB", "System", "razion-terminal", "/bin/terminal", "NCSA", "Razion text and graphics", "process launch; pseudo-terminal", "Uses the current Razion theme palette.", 1},
	{"File Manager", "file-browser", "Browse local folders, launch files, and manage the desktop.", "2.x", "396 KiB", "System", "razion-files", "/bin/file-browser", "NCSA", "Razion graphics", "user files: read/write; process launch", "Desktop and file views share one native implementation.", 1},
	{"Image Viewer", "imgviewer", "Open common local image formats with a native lightweight viewer.", "2.x", "121 KiB", "Graphics", "image-x-generic", "/bin/imgviewer", "NCSA", "PNG, JPEG, graphics", "selected files: read", "Supports the image codecs included in RazionOS.", 0},
	{"Calculator", "calculator", "A focused native calculator for everyday arithmetic.", "2.x", "94 KiB", "Education", "accessories-calculator", "/bin/calculator", "NCSA", "Razion graphics", "none", "Bundled scientific and standard calculations.", 0},
	{"Help", "help-browser", "Read the local RazionOS documentation without a network connection.", "2.x", "102 KiB", "Education", "help-browser", "/bin/help-browser", "NCSA", "Markup renderer", "system documentation: read", "Local rich-text help topics.", 0},
	{"System Monitor", "cpuwidget", "Inspect live CPU, memory, and network activity.", "2.x", "137 KiB", "System", "razion-monitor", "/bin/cpuwidget", "NCSA", "procfs", "system metrics: read", "Native live performance charts.", 0},
	{"Razion Snake", "razion-snake", "A lightweight native arcade game with keyboard controls and scoring.", "1.0.0", "System image", "Games", "razion-snake", "/bin/razion-snake", "NCSA", "Razion graphics, Yutani", "none", "Added responsive arcade gameplay and restart controls.", 0},
	{"Razion Tiles", "razion-tiles", "A native 2048-style number puzzle with local score tracking.", "1.0.0", "System image", "Games", "razion-tiles", "/bin/razion-tiles", "NCSA", "Razion graphics, Yutani", "none", "Added keyboard-driven tile merging and restart controls.", 0},
};

static const char * categories[] = {"Development", "Internet", "Graphics", "Education",
	"Utilities", "Multimedia", "Games", "System"};
static const char * navigation[] = {"Discover", "Categories", "Installed", "Updates", "Downloads", "Settings"};

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font;
static struct TT_Font * bold;
static int running = 1;
static int page = 0;
static int selected = 0;
static int active_category = -1;
static int editing_search = 0;
static char search[SEARCH_MAX];
static size_t search_length;
static char status_text[256] = "Offline system catalogue loaded.";
static razion_ai_context_t ai_context;
static razion_ai_async_request_t ai_request;
static int ai_pending;

static void copy_text(char * out, size_t size, const char * in) {
	if (!size) return;
	if (!in) in = "";
	size_t length = strlen(in);
	if (length >= size) length = size - 1;
	memcpy(out, in, length);
	out[length] = '\0';
}

static int contains_folded(const char * haystack, const char * needle) {
	if (!needle[0]) return 1;
	for (; *haystack; ++haystack) {
		const char * h = haystack, * n = needle;
		while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { h++; n++; }
		if (!*n) return 1;
	}
	return 0;
}

static int app_installed(const store_app_t * app) {
	return !access(app->executable, X_OK);
}

static void app_size(const store_app_t * app, char * output, size_t size) {
	struct stat stat_buffer;
	if (!stat(app->executable, &stat_buffer)) {
		if (stat_buffer.st_size >= 1024 * 1024) snprintf(output, size, "%llu.%llu MiB",
			(unsigned long long)stat_buffer.st_size / (1024 * 1024),
			((unsigned long long)stat_buffer.st_size % (1024 * 1024)) * 10 / (1024 * 1024));
		else snprintf(output, size, "%llu KiB", (unsigned long long)(stat_buffer.st_size + 1023) / 1024);
	} else copy_text(output, size, app->size);
}

static int app_visible(size_t index) {
	const store_app_t * app = &catalogue[index];
	if (active_category >= 0 && strcmp(app->category, categories[active_category])) return 0;
	if (page == 0 && !app->featured && !search[0]) return 0;
	if (page == 2 && !app_installed(app)) return 0;
	return contains_folded(app->name, search) || contains_folded(app->description, search) ||
		contains_folded(app->category, search);
}

static void draw_ellipsized(struct TT_Font * face, int size, int x, int y,
	const char * text, int maximum, uint32_t color) {
	char * shown = tt_ellipsify(text, size, face, maximum, NULL);
	tt_draw_string(ctx, face, x, y, shown, color);
	free(shown);
}

static void button(int x, int y, int width, int height, const char * label,
	int selected_button, int disabled) {
	draw_rounded_rectangle(ctx, x, y, width, height, 5,
		selected_button ? RAZION_SELECTION : RAZION_SURFACE_SECONDARY);
	draw_rectangle_solid(ctx, x, y, width, 1,
		selected_button ? RAZION_FOCUS : RAZION_BORDER);
	tt_set_size(font, 10);
	int tw = tt_string_width(font, label);
	tt_draw_string(ctx, font, x + (width - tw) / 2, y + height / 2 + 4,
		label, disabled ? RAZION_TEXT_SECONDARY : RAZION_TEXT_PRIMARY);
}

static void icon_or_fallback(const store_app_t * app, int x, int y) {
	sprite_t * icon = icon_get_48(app->icon);
	if (icon) draw_sprite(ctx, icon, x, y);
	else {
		draw_rounded_rectangle(ctx, x, y, 48, 48, 9, RAZION_SELECTION);
		tt_set_size(bold, 20);
		char initial[2] = {app->name[0], 0};
		tt_draw_string(ctx, bold, x + 16, y + 31, initial, RAZION_TEXT_PRIMARY);
	}
}

static void draw_security_summary(int x, int y, int width, const store_app_t * app) {
	draw_rounded_rectangle(ctx, x, y, width, 118, 7, RAZION_SURFACE_SECONDARY);
	tt_set_size(bold, 11); tt_draw_string(ctx, bold, x + 14, y + 20, "Before any transaction", RAZION_TEXT_PRIMARY);
	tt_set_size(font, 9);
	char line[320], size_text[32];
	app_size(app, size_text, sizeof(size_text));
	snprintf(line, sizeof(line), "Application  %s    Version  %s    Size  %s", app->name, app->version, size_text);
	draw_ellipsized(font, 9, x + 14, y + 39, line, width - 28, RAZION_TEXT_SECONDARY);
	snprintf(line, sizeof(line), "License  %s    Dependencies  %s", app->license, app->dependencies);
	draw_ellipsized(font, 9, x + 14, y + 58, line, width - 28, RAZION_TEXT_SECONDARY);
	snprintf(line, sizeof(line), "Requested permissions  %s", app->permissions);
	draw_ellipsized(font, 9, x + 14, y + 77, line, width - 28, RAZION_TEXT_SECONDARY);
	tt_draw_string(ctx, font, x + 14, y + 99,
		"Integrity and signature verification are required before install is enabled.", RAZION_WARNING);
}

static void draw_details(int x, int y, int width, int height) {
	const store_app_t * app = &catalogue[selected];
	draw_rounded_rectangle(ctx, x, y, width, height, 7, RAZION_SURFACE);
	draw_rectangle_solid(ctx, x, y, width, 1, RAZION_BORDER);
	icon_or_fallback(app, x + 18, y + 18);
	tt_set_size(bold, 18); draw_ellipsized(bold, 18, x + 78, y + 38, app->name, width - 96, RAZION_TEXT_PRIMARY);
	tt_set_size(font, 9); tt_draw_string(ctx, font, x + 78, y + 56, app->category, RAZION_ACCENT);
	draw_ellipsized(font, 10, x + 18, y + 89, app->description, width - 36, RAZION_TEXT_SECONDARY);

	int row = y + 119;
	char size_text[32]; app_size(app, size_text, sizeof(size_text));
	const char * labels[] = {"Version", "Size", "Developer", "License", "Dependencies", "Permissions"};
	const char * values[] = {app->version, size_text, "RazionOS Project", app->license, app->dependencies, app->permissions};
	for (int i = 0; i < 6; ++i) {
		tt_set_size(font, 8); tt_draw_string(ctx, font, x + 18, row, labels[i], RAZION_TEXT_SECONDARY);
		draw_ellipsized(font, 9, x + 118, row, values[i], width - 136, RAZION_TEXT_PRIMARY);
		row += 22;
	}
	tt_set_size(bold, 10); tt_draw_string(ctx, bold, x + 18, row + 10, "Changelog", RAZION_TEXT_PRIMARY);
	draw_ellipsized(font, 9, x + 18, row + 30, app->changelog, width - 36, RAZION_TEXT_SECONDARY);
	tt_set_size(bold, 10); tt_draw_string(ctx, bold, x + 18, row + 57, "Screenshots", RAZION_TEXT_PRIMARY);
	tt_set_size(font, 9); tt_draw_string(ctx, font, x + 18, row + 76,
		"No verified screenshots are bundled for this system component.", RAZION_TEXT_SECONDARY);

	int security_y = y + height - 171;
	draw_security_summary(x + 12, security_y, width - 24, app);
	button(x + 12, y + height - 43, width - 24, 31,
		app_installed(app) ? "Managed by the RazionOS image" : "Install unavailable", 0, 1);
}

static void draw_empty(int x, int y, int width, const char * title, const char * explanation) {
	draw_rounded_rectangle(ctx, x, y, width, 126, 7, RAZION_SURFACE);
	tt_set_size(bold, 15); tt_draw_string(ctx, bold, x + 20, y + 34, title, RAZION_TEXT_PRIMARY);
	tt_set_size(font, 10); draw_ellipsized(font, 10, x + 20, y + 58, explanation, width - 40, RAZION_TEXT_SECONDARY);
	tt_draw_string(ctx, font, x + 20, y + 88,
		"No arbitrary files are downloaded or executed.", RAZION_WARNING);
}

static void redraw(void) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	int ox = bounds.left_width, oy = bounds.top_height;
	int width = window->width - bounds.width, height = window->height - bounds.height;
	int nav_w = width < 900 ? 142 : 178;
	draw_fill(ctx, RAZION_BACKGROUND);
	draw_rectangle_solid(ctx, ox, oy, nav_w, height, RAZION_SURFACE);
	draw_rectangle_solid(ctx, ox + nav_w - 1, oy, 1, height, RAZION_BORDER);
	tt_set_size(bold, 18); tt_draw_string(ctx, bold, ox + 16, oy + 31, "RAZION", RAZION_ACCENT);
	tt_set_size(font, 10); tt_draw_string(ctx, font, ox + 16, oy + 49, "STORE", RAZION_TEXT_SECONDARY);
	for (int i = 0; i < 6; ++i) {
		int ny = oy + 76 + i * 42;
		if (page == i) draw_rounded_rectangle(ctx, ox + 8, ny, nav_w - 16, 34, 5, RAZION_SELECTION);
		tt_set_size(font, 10); tt_draw_string(ctx, font, ox + 22, ny + 21, navigation[i],
			page == i ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY);
	}
	tt_set_size(font, 8); tt_draw_string(ctx, font, ox + 16, oy + height - 18,
		rzpkg_repository_available(NULL) ? "Signed repository ready" : "Offline catalogue", RAZION_TEXT_SECONDARY);

	int content_x = ox + nav_w + 16;
	int content_w = width - nav_w - 32;
	tt_set_size(bold, 22); tt_draw_string(ctx, bold, content_x, oy + 35,
		page == 0 ? "Featured applications" : navigation[page], RAZION_TEXT_PRIMARY);
	int ai_w = width < 900 ? 92 : 122;
	int search_w = content_w < 700 ? 190 : 260;
	int search_x = content_x + content_w - search_w - ai_w - 8;
	draw_rounded_rectangle(ctx, search_x, oy + 12, search_w, 34, 5, RAZION_SURFACE_SECONDARY);
	draw_rectangle_solid(ctx, search_x, oy + 45, search_w, 1, editing_search ? RAZION_FOCUS : RAZION_BORDER);
	tt_set_size(font, 10); draw_ellipsized(font, 10, search_x + 12, oy + 33,
		search[0] ? search : "Search applications", search_w - 24,
		search[0] ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY);
	button(search_x + search_w + 8, oy + 12, ai_w, 34, ai_pending ? "AI working" : "Ask Razion AI", ai_pending, ai_pending);

	int body_y = oy + 64;
	int body_h = height - 96;
	if (page == 3) {
		draw_empty(content_x, body_y, content_w, "Updates require a trusted repository",
			"No signed package index or update transaction service is configured yet.");
	} else if (page == 4) {
		draw_empty(content_x, body_y, content_w, "No active downloads",
			"Downloads will appear only after a verified rzpkg transaction is explicitly approved.");
	} else if (page == 5) {
		draw_empty(content_x, body_y, content_w, "Security-first package pipeline",
			"Repository -> dependency resolver -> integrity verification -> permission review -> transaction service");
		tt_set_size(font, 10); tt_draw_string(ctx, font, content_x + 20, body_y + 164,
			"Razion AI is optional, local-only by default, and can recommend but never install.", RAZION_TEXT_SECONDARY);
	} else {
		int detail_w = content_w < 700 ? 268 : 344;
		int grid_w = content_w - detail_w - 12;
		int cols = grid_w < 410 ? 1 : 2;
		int gap = 8;
		int card_w = (grid_w - gap * (cols - 1)) / cols;
		int shown = 0;
		if (page == 1) {
			int chip_w = (grid_w - gap) / 2;
			for (int i = 0; i < 8; ++i) {
				int cx = content_x + (i % 2) * (chip_w + gap);
				int cy = body_y + (i / 2) * 39;
				button(cx, cy, chip_w, 31, categories[i], active_category == i, 0);
			}
			body_y += 164; body_h -= 164;
		}
		for (size_t i = 0; i < sizeof(catalogue) / sizeof(catalogue[0]); ++i) {
			if (!app_visible(i)) continue;
			int x = content_x + (shown % cols) * (card_w + gap);
			int y = body_y + (shown / cols) * 78;
			if (y + 70 > oy + height - 35) break;
			draw_rounded_rectangle(ctx, x, y, card_w, 69, 6,
				selected == (int)i ? RAZION_SELECTION : RAZION_SURFACE);
			draw_rectangle_solid(ctx, x, y, card_w, 1,
				selected == (int)i ? RAZION_FOCUS : RAZION_BORDER);
			icon_or_fallback(&catalogue[i], x + 10, y + 10);
			tt_set_size(bold, 11); draw_ellipsized(bold, 11, x + 68, y + 25,
				catalogue[i].name, card_w - 78, RAZION_TEXT_PRIMARY);
			tt_set_size(font, 8); draw_ellipsized(font, 8, x + 68, y + 43,
				catalogue[i].category, card_w - 78, RAZION_ACCENT);
			draw_ellipsized(font, 8, x + 68, y + 58,
				app_installed(&catalogue[i]) ? "Installed with RazionOS" : "Unavailable",
				card_w - 78, RAZION_TEXT_SECONDARY);
			shown++;
		}
		if (!shown) draw_empty(content_x, body_y, grid_w, "No matching applications",
			"Try another name or clear the category filter.");
		draw_details(content_x + grid_w + 12, oy + 64, detail_w, body_h);
	}

	draw_rectangle_solid(ctx, content_x, oy + height - 27, content_w, 1, RAZION_BORDER);
	tt_set_size(font, 8); draw_ellipsized(font, 8, content_x, oy + height - 10,
		status_text, content_w, RAZION_TEXT_SECONDARY);
	render_decorations(window, ctx, "Razion Store");
	flip(ctx);
	yutani_flip(yctx, window);
}

static int hit(int x, int y, int w, int h, struct yutani_msg_window_mouse_event * mouse) {
	return mouse->new_x >= x && mouse->new_x < x + w && mouse->new_y >= y && mouse->new_y < y + h;
}

static void start_ai(void) {
	if (!search[0]) {
		copy_text(status_text, sizeof(status_text), "Type a request such as 'find a lightweight code editor' first.");
		return;
	}
	if (ai_pending) return;
	if (razion_ai_request_begin(&ai_context, RAZION_AI_OP_APPLICATION_SEARCH, search,
		RAZION_AI_FLAG_LOCAL_ONLY, &ai_request)) {
		copy_text(status_text, sizeof(status_text), "Razion AI Engine is unavailable; local catalogue search still works.");
		return;
	}
	ai_pending = 1;
	copy_text(status_text, sizeof(status_text), "Razion AI is searching the application catalogue locally...");
}

static void poll_ai(void) {
	if (!ai_pending) return;
	razion_ai_response_t response;
	int result = razion_ai_request_poll(&ai_request, 0, &response);
	if (!result) return;
	ai_pending = 0;
	if (result < 0) {
		copy_text(status_text, sizeof(status_text), "Razion AI request failed; no browsing data was sent externally.");
	} else if (response.status != RAZION_AI_STATUS_OK) {
		snprintf(status_text, sizeof(status_text), "Razion AI: %s. Local catalogue results remain available.",
			razion_ai_status_string(response.status));
	} else {
		response.payload[response.payload_length] = '\0';
		snprintf(status_text, sizeof(status_text), "AI recommendation: %.190s", response.payload);
	}
	razion_ai_request_cancel(&ai_request);
}

static void click(struct yutani_msg_window_mouse_event * mouse) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	int ox = bounds.left_width, oy = bounds.top_height;
	int width = window->width - bounds.width;
	int nav_w = width < 900 ? 142 : 178;
	for (int i = 0; i < 6; ++i) if (hit(ox + 8, oy + 76 + i * 42, nav_w - 16, 34, mouse)) {
		page = i; editing_search = 0; if (i != 1) active_category = -1;
		copy_text(status_text, sizeof(status_text), "View changed."); return;
	}
	int content_x = ox + nav_w + 16, content_w = width - nav_w - 32;
	int ai_w = width < 900 ? 92 : 122, search_w = content_w < 700 ? 190 : 260;
	int search_x = content_x + content_w - search_w - ai_w - 8;
	if (hit(search_x, oy + 12, search_w, 34, mouse)) { editing_search = 1; return; }
	if (hit(search_x + search_w + 8, oy + 12, ai_w, 34, mouse)) { start_ai(); return; }
	int body_y = oy + 64;
	if (page == 1) {
		int detail_w = content_w < 700 ? 268 : 344;
		int grid_w = content_w - detail_w - 12, gap = 8, chip_w = (grid_w - gap) / 2;
		for (int i = 0; i < 8; ++i) if (hit(content_x + (i % 2) * (chip_w + gap),
			body_y + (i / 2) * 39, chip_w, 31, mouse)) {
			active_category = active_category == i ? -1 : i;
			copy_text(status_text, sizeof(status_text), active_category < 0 ? "Category filter cleared." : categories[i]); return;
		}
		body_y += 164;
	}
	if (page > 2) return;
	int detail_w = content_w < 700 ? 268 : 344, grid_w = content_w - detail_w - 12;
	int cols = grid_w < 410 ? 1 : 2, gap = 8, card_w = (grid_w - gap * (cols - 1)) / cols;
	int shown = 0;
	for (size_t i = 0; i < sizeof(catalogue) / sizeof(catalogue[0]); ++i) {
		if (!app_visible(i)) continue;
		int x = content_x + (shown % cols) * (card_w + gap), y = body_y + (shown / cols) * 78;
		if (hit(x, y, card_w, 69, mouse)) {
			selected = i;
			copy_text(status_text, sizeof(status_text), "Application details selected. Transactions remain permission-gated.");
			return;
		}
		shown++;
	}
}

int main(void) {
	yctx = yutani_init();
	if (!yctx) return 1;
	init_decorations();
	struct decor_bounds bounds;
	decor_get_bounds(NULL, &bounds);
	int width = STORE_WIDTH + bounds.width, height = STORE_HEIGHT + bounds.height;
	if (width > (int)yctx->display_width - 16) width = yctx->display_width - 16;
	if (height > (int)yctx->display_height - 42) height = yctx->display_height - 42;
	window = yutani_window_create(yctx, width, height);
	yutani_window_move(yctx, window, yctx->display_width / 2 - width / 2,
		28 + (yctx->display_height - 28) / 2 - height / 2);
	yutani_window_advertise_icon(yctx, window, "Razion Store", "razion-store");
	ctx = init_graphics_yutani_double_buffer(window);
	font = tt_font_from_shm("sans-serif");
	bold = tt_font_from_shm("sans-serif.bold");
	razion_ai_init(&ai_context, "razion-store");
	ai_context.default_flags = RAZION_AI_FLAG_LOCAL_ONLY;
	redraw();
	while (running) {
		int fd = fileno(yctx->sock);
		if (ai_pending && fswait2(1, &fd, 60) != 0) { poll_ai(); redraw(); continue; }
		yutani_msg_t * event = yutani_poll(yctx);
		if (!event) continue;
		switch (event->type) {
			case YUTANI_MSG_KEY_EVENT: {
				struct yutani_msg_key_event * key = (void *)event->data;
				if (key->wid != window->wid || key->event.action != KEY_ACTION_DOWN) break;
				if (key->event.keycode == KEY_ESCAPE) {
					if (editing_search) editing_search = 0; else running = 0;
				} else if (editing_search && (key->event.key == '\b' || key->event.keycode == KEY_BACKSPACE)) {
					if (search_length) search[--search_length] = '\0';
				} else if (editing_search && key->event.key == '\n') editing_search = 0;
				else if (editing_search && key->event.key >= 0x20 && key->event.key < 0x7f && search_length + 1 < sizeof(search)) {
					search[search_length++] = key->event.key; search[search_length] = '\0';
				}
				redraw(); break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)event->data;
				if (mouse->wid != window->wid) break;
				int decor = decor_handle_event(yctx, event);
				if (decor == DECOR_CLOSE) running = 0;
				if (mouse->command == YUTANI_MOUSE_EVENT_CLICK) click(mouse);
				redraw(); break;
			}
			case YUTANI_MSG_RESIZE_OFFER: {
				struct yutani_msg_window_resize * resize = (void *)event->data;
				int w = resize->width < 760 ? 760 : resize->width;
				int h = resize->height < 540 ? 540 : resize->height;
				yutani_window_resize_accept(yctx, window, w, h); reinit_graphics_yutani(ctx, window);
				yutani_window_resize_done(yctx, window); redraw(); break;
			}
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE: {
				struct yutani_msg_window_focus_change * focus = (void *)event->data;
				if (focus->wid == window->wid) { window->focused = focus->focused; redraw(); }
				break;
			}
			case YUTANI_MSG_WINDOW_CLOSE:
			case YUTANI_MSG_SESSION_END: running = 0;
		}
		free(event);
	}
	if (ai_pending) razion_ai_request_cancel(&ai_request);
	yutani_close(yctx, window);
	return 0;
}
