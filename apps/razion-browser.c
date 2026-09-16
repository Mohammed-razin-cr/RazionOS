/**
 * @brief Razion Browser native shell.
 *
 * RazionOS currently has HTTP transfer primitives and a small local markup
 * renderer, but no sandboxed HTML/CSS/JavaScript engine. This application is
 * therefore an honest browser shell: tabs, navigation state, bookmarks,
 * history, downloads and privacy settings are real; remote pages are never
 * represented as rendered when no web engine exists.
 */
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fswait.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <net/if.h>

#include <razion/theme.h>
#include <toaru/decorations.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/razion_ai.h>
#include <toaru/text.h>
#include <toaru/yutani.h>

#define BROWSER_WIDTH 1140
#define BROWSER_HEIGHT 700
#define TAB_MAX 6
#define HISTORY_MAX 12
#define URL_MAX 256

typedef enum {
	VIEW_HOME = 0,
	VIEW_EXTERNAL,
	VIEW_SEARCH,
	VIEW_BOOKMARKS,
	VIEW_HISTORY,
	VIEW_DOWNLOADS,
	VIEW_SETTINGS,
} browser_view_t;

typedef struct {
	char title[48];
	char url[URL_MAX];
	char history[HISTORY_MAX][URL_MAX];
	int history_count;
	int history_index;
	browser_view_t view;
	int bookmarked;
} browser_tab_t;

static yutani_t * yctx;
static yutani_window_t * window;
static gfx_context_t * ctx;
static struct TT_Font * font;
static struct TT_Font * bold;
static browser_tab_t tabs[TAB_MAX];
static int tab_count;
static int active_tab;
static int running = 1;
static int editing_address;
static int address_selected;
static int hover_id = -1;
static int pressed_id = -1;
static int focus_id = 5;
static int menu_open;
static int network_available;
static char address[URL_MAX];
static size_t address_length;
static char status_text[320] = "Native shell ready. No browsing data leaves the device without an explicit action.";
static razion_ai_context_t ai_context;
static razion_ai_async_request_t ai_request;
static int ai_pending;
static char ai_result[512];

static void copy_text(char * output, size_t size, const char * input) {
	if (!size) return;
	if (!input) input = "";
	size_t length = strlen(input);
	if (length >= size) length = size - 1;
	memcpy(output, input, length);
	output[length] = '\0';
}

static int network_online(void) {
	DIR * directory = opendir("/dev/net");
	if (!directory) return 0;
	struct dirent * entry;
	int online = 0;
	while (!online && (entry = readdir(directory))) {
		if (entry->d_name[0] == '.') continue;
		char path[512];
		snprintf(path, sizeof(path), "/dev/net/%s", entry->d_name);
		int device = open(path, O_RDONLY);
		if (device < 0) continue;
		uint32_t flags = 0, address_value = 0;
		if (!ioctl(device, SIOCGIFFLAGS, &flags) && !(flags & IFF_LOOPBACK) &&
			(flags & IFF_UP) && !ioctl(device, SIOCGIFADDR, &address_value)) online = 1;
		close(device);
	}
	closedir(directory);
	return online;
}

static void tab_init(browser_tab_t * tab) {
	memset(tab, 0, sizeof(*tab));
	copy_text(tab->title, sizeof(tab->title), "New tab");
	copy_text(tab->url, sizeof(tab->url), "about:home");
	copy_text(tab->history[0], sizeof(tab->history[0]), tab->url);
	tab->history_count = 1;
	tab->view = VIEW_HOME;
}

static void sync_address(void) {
	copy_text(address, sizeof(address), tabs[active_tab].url);
	address_length = strlen(address);
}

static void add_history(browser_tab_t * tab, const char * url) {
	if (tab->history_index + 1 < tab->history_count) tab->history_count = tab->history_index + 1;
	if (tab->history_count == HISTORY_MAX) {
		memmove(tab->history, tab->history + 1, sizeof(tab->history[0]) * (HISTORY_MAX - 1));
		tab->history_count--;
	}
	copy_text(tab->history[tab->history_count], sizeof(tab->history[0]), url);
	tab->history_index = tab->history_count++;
}

static int starts_with_folded(const char * text, const char * prefix) {
	while (*prefix) {
		if (tolower((unsigned char)*text++) != tolower((unsigned char)*prefix++)) return 0;
	}
	return 1;
}

static void apply_location(browser_tab_t * tab, const char * location, int record) {
	if (!location[0]) location = "about:home";
	copy_text(tab->url, sizeof(tab->url), location);
	if (record) add_history(tab, tab->url);
	ai_result[0] = '\0';
	if (!strcmp(location, "about:home") || !strcmp(location, "razion:home")) {
		copy_text(tab->title, sizeof(tab->title), "New tab");
		tab->view = VIEW_HOME;
		copy_text(status_text, sizeof(status_text), "Home loaded locally.");
	} else if (starts_with_folded(location, "http://") || starts_with_folded(location, "https://")) {
		copy_text(tab->title, sizeof(tab->title), "Web engine unavailable");
		tab->view = VIEW_EXTERNAL;
		copy_text(status_text, sizeof(status_text), network_available ?
			"Network is available, but no sandboxed web rendering engine is installed." :
			"Network unavailable. The browser shell remains fully responsive offline.");
	} else {
		copy_text(tab->title, sizeof(tab->title), "Search");
		tab->view = VIEW_SEARCH;
		copy_text(status_text, sizeof(status_text),
			"Search query kept local. Use Ask Razion AI explicitly for an optional local-only request.");
	}
	sync_address();
}

static void navigate(const char * location) {
	network_available = network_online();
	apply_location(&tabs[active_tab], location, 1);
}

static void draw_ellipsized(struct TT_Font * face, int size, int x, int y,
	const char * text, int maximum, uint32_t color) {
	char * shown = tt_ellipsify(text, size, face, maximum, NULL);
	tt_draw_string(ctx, face, x, y, shown, color);
	free(shown);
}

static void button(int id, int x, int y, int width, int height, const char * label,
	int selected, int disabled) {
	uint32_t fill = selected ? RAZION_SELECTION :
		id == pressed_id ? RAZION_SELECTION : id == hover_id ? RAZION_SURFACE_HOVER : RAZION_SURFACE;
	if (!disabled && id == focus_id && window->focused)
		draw_rounded_rectangle(ctx, x - 2, y - 2, width + 4, height + 4, 9, RAZION_FOCUS);
	draw_rounded_rectangle(ctx, x, y, width, height, 7, fill);
	if (selected) draw_rectangle_solid(ctx, x + 8, y + height - 2, width - 16, 2, RAZION_ACCENT);
	tt_set_size(font, 10);
	int tw = tt_string_width(font, label);
	tt_draw_string(ctx, font, x + (width - tw) / 2, y + height / 2 + 4,
		label, disabled ? RAZION_TEXT_SECONDARY : RAZION_TEXT_PRIMARY);
}

static void icon_button(int id, int x, int y, int kind, int disabled) {
	uint32_t color = disabled ? RAZION_TEXT_SECONDARY : RAZION_TEXT_PRIMARY;
	button(id, x, y, 36, 34, "", 0, disabled);
	if (kind == 0 || kind == 1) {
		int direction = kind ? 1 : -1;
		int center = x + 18;
		draw_line_aa(ctx, center + direction * 7, center - direction * 6, y + 17, y + 17, color, 1.6f);
		draw_line_aa(ctx, center + direction * 7, center + direction * 1, y + 17, y + 11, color, 1.6f);
		draw_line_aa(ctx, center + direction * 7, center + direction * 1, y + 17, y + 23, color, 1.6f);
	} else if (kind == 2) {
		draw_line_aa(ctx, x + 12, x + 12, y + 13, y + 22, color, 1.5f);
		draw_line_aa(ctx, x + 12, x + 18, y + 13, y + 9, color, 1.5f);
		draw_line_aa(ctx, x + 18, x + 24, y + 9, y + 13, color, 1.5f);
		draw_line_aa(ctx, x + 24, x + 24, y + 13, y + 22, color, 1.5f);
		draw_line_aa(ctx, x + 12, x + 24, y + 22, y + 22, color, 1.5f);
	} else if (kind == 3) {
		draw_line_aa(ctx, x + 12, x + 23, y + 11, y + 11, color, 1.5f);
		draw_line_aa(ctx, x + 23, x + 25, y + 11, y + 17, color, 1.5f);
		draw_line_aa(ctx, x + 25, x + 21, y + 17, y + 23, color, 1.5f);
		draw_line_aa(ctx, x + 21, x + 12, y + 23, y + 23, color, 1.5f);
		draw_line_aa(ctx, x + 12, x + 10, y + 23, y + 18, color, 1.5f);
		draw_line_aa(ctx, x + 10, x + 14, y + 18, y + 18, color, 1.5f);
	} else {
		for (int i = 0; i < 3; ++i) draw_rounded_rectangle(ctx, x + 16, y + 10 + i * 6, 3, 3, 2, color);
	}
}

static void draw_bookmark_star(int center_x, int center_y, uint32_t color) {
	int px[] = {center_x, center_x + 2, center_x + 8, center_x + 3, center_x + 5,
		center_x, center_x - 5, center_x - 3, center_x - 8, center_x - 2};
	int py[] = {center_y - 8, center_y - 2, center_y - 2, center_y + 2, center_y + 8,
		center_y + 4, center_y + 8, center_y + 2, center_y - 2, center_y - 2};
	for (int i = 0; i < 10; ++i)
		draw_line_aa(ctx, px[i], px[(i + 1) % 10], py[i], py[(i + 1) % 10], color, 1.2f);
}

static void info_panel(int x, int y, int width, const char * title,
	const char * body, uint32_t accent) {
	draw_rounded_rectangle(ctx, x, y, width, 112, 7, RAZION_SURFACE);
	draw_rectangle_solid(ctx, x, y, 3, 112, accent);
	tt_set_size(bold, 16); tt_draw_string(ctx, bold, x + 22, y + 32, title, RAZION_TEXT_PRIMARY);
	tt_set_size(font, 10); draw_ellipsized(font, 10, x + 22, y + 58, body, width - 44, RAZION_TEXT_SECONDARY);
}

static void draw_home(int x, int y, int width, int height) {
	int online = network_available;
	int center = x + width / 2;
	int compact = height < 460;
	int logo_y = compact ? 20 : 38;
	int title_y = compact ? 90 : 118;
	int helper_y = compact ? 116 : 145;
	int cards_y = compact ? 142 : 184;
	int privacy_y = compact ? 282 : 330;
	int detail_y = compact ? 305 : 355;
	int actions_y = compact ? 326 : 386;
	draw_rounded_rectangle(ctx, center - 22, y + logo_y, 44, 44, 14, RAZION_SELECTION);
	tt_set_size(bold, 22); tt_draw_string(ctx, bold, center - 8, y + logo_y + 31, "R", RAZION_FOCUS);
	tt_set_size(bold, 28);
	const char * title = "Browse with Ripper";
	tt_draw_string(ctx, bold, center - tt_string_width(bold, title) / 2, y + title_y, title, RAZION_TEXT_PRIMARY);
	tt_set_size(font, 11);
	const char * helper = "Enter an address or a local search in the bar above";
	tt_draw_string(ctx, font, center - tt_string_width(font, helper) / 2, y + helper_y, helper, RAZION_TEXT_SECONDARY);
	int gap = 14, card_w = (width - 82 - gap) / 2;
	info_panel(x + 34, y + cards_y, card_w, "Web engine",
		"Not installed yet. Remote pages are never shown as fabricated content.", RAZION_WARNING);
	info_panel(x + 34 + card_w + gap, y + cards_y, card_w,
		online ? "Network connected" : "Working offline",
		online ? "Connectivity is ready for a future isolated rendering engine." :
		"Tabs, history, bookmarks and settings remain available locally.",
		online ? RAZION_SUCCESS : RAZION_ERROR);
	tt_set_size(bold, 12); tt_draw_string(ctx, bold, x + 36, y + privacy_y, "Private by design", RAZION_TEXT_PRIMARY);
	tt_set_size(font, 10); tt_draw_string(ctx, font, x + 36, y + detail_y,
		"Ripper keeps searches local unless you explicitly request Razion AI.", RAZION_TEXT_SECONDARY);
	button(20, x + 34, y + actions_y, 108, 34, "Bookmarks", 0, 0);
	button(21, x + 150, y + actions_y, 88, 34, "History", 0, 0);
	button(22, x + 246, y + actions_y, 106, 34, "Downloads", 0, 0);
	button(23, x + 360, y + actions_y, 92, 34, "Privacy", 0, 0);
}

static void draw_external(int x, int y, int width, int height) {
	info_panel(x + 34, y + 52, width - 68,
		network_available ? "Page not rendered" : "Network unavailable",
		network_available ? "RazionOS can transfer HTTP data, but it has no sandboxed standards-compliant web renderer." :
		"This address was recorded in session history, but no connection or rendering was attempted.",
		network_available ? RAZION_WARNING : RAZION_ERROR);
	tt_set_size(bold, 14); tt_draw_string(ctx, bold, x + 36, y + 202, "Requested address", RAZION_TEXT_PRIMARY);
	draw_ellipsized(font, 11, x + 36, y + 228, tabs[active_tab].url, width - 72, RAZION_ACCENT);
	tt_set_size(font, 10); tt_draw_string(ctx, font, x + 36, y + 274,
		"No page content, scripts, cookies, downloads, or browsing data were fabricated.", RAZION_TEXT_SECONDARY);
	button(-1, x + 36, y + 312, 210, 34, "Summarize page unavailable", 0, 1);
	button(400, x + 254, y + 312, 180, 34, tabs[active_tab].bookmarked ? "Bookmarked" : "Add bookmark", tabs[active_tab].bookmarked, 0);
}

static void draw_search(int x, int y, int width, int height) {
	info_panel(x + 34, y + 52, width - 68, "Natural search query",
		"No default search provider is contacted. Razion AI runs only after the explicit button below.", RAZION_ACCENT);
	tt_set_size(bold, 14); tt_draw_string(ctx, bold, x + 36, y + 202, "Query", RAZION_TEXT_PRIMARY);
	draw_ellipsized(font, 11, x + 36, y + 228, tabs[active_tab].url, width - 72, RAZION_ACCENT);
	button(401, x + 36, y + 260, 178, 34, ai_pending ? "AI working" : "Ask Razion AI", ai_pending, ai_pending);
	if (ai_result[0]) {
		tt_set_size(bold, 12); tt_draw_string(ctx, bold, x + 36, y + 334, "AI response", RAZION_TEXT_PRIMARY);
		draw_ellipsized(font, 10, x + 36, y + 362, ai_result, width - 72, RAZION_TEXT_SECONDARY);
	}
}

static void draw_list_view(int x, int y, int width, browser_view_t view) {
	const char * title = view == VIEW_BOOKMARKS ? "Bookmarks" : view == VIEW_HISTORY ? "History" :
		view == VIEW_DOWNLOADS ? "Downloads" : "Settings";
	tt_set_size(bold, 24); tt_draw_string(ctx, bold, x + 34, y + 55, title, RAZION_TEXT_PRIMARY);
	if (view == VIEW_BOOKMARKS) {
		info_panel(x + 34, y + 86, width - 68, "RazionOS project",
			"https://github.com/Mohammed-razin-cr/RazionOS", RAZION_ACCENT);
		if (tabs[active_tab].bookmarked) info_panel(x + 34, y + 210, width - 68,
			"Current session bookmark", tabs[active_tab].url, RAZION_SUCCESS);
	} else if (view == VIEW_HISTORY) {
		browser_tab_t * tab = &tabs[active_tab];
		for (int i = tab->history_count - 1, row = 0; i >= 0; --i, ++row) {
			int row_y = y + 88 + row * 44;
			draw_rounded_rectangle(ctx, x + 34, row_y, width - 68, 36, 5,
				i == tab->history_index ? RAZION_SELECTION : RAZION_SURFACE);
			draw_ellipsized(font, 10, x + 48, row_y + 23, tab->history[i], width - 96, RAZION_TEXT_PRIMARY);
		}
	} else if (view == VIEW_DOWNLOADS) {
		info_panel(x + 34, y + 86, width - 68, "No downloads",
			"A download service will be added only with explicit destination, integrity, and permission controls.", RAZION_WARNING);
	} else {
		info_panel(x + 34, y + 86, width - 68, "Privacy by default",
			"AI is local-only by default. No page data is sent to an AI provider without an explicit action.", RAZION_SUCCESS);
		info_panel(x + 34, y + 210, width - 68, "Web engine integration",
			"Future engines must provide process isolation, TLS validation, storage policy, and content permissions.", RAZION_WARNING);
	}
}

static void menu_item(int id, int x, int y, int width, const char * label,
	const char * shortcut, int selected, int disabled) {
	uint32_t fill = selected ? RAZION_SELECTION : id == pressed_id ? RAZION_SELECTION :
		id == hover_id ? RAZION_SURFACE_HOVER : RAZION_SURFACE;
	if (!disabled && id == focus_id && window->focused)
		draw_rounded_rectangle(ctx, x - 2, y - 2, width + 4, 34, 8, RAZION_FOCUS);
	draw_rounded_rectangle(ctx, x, y, width, 30, 6, fill);
	tt_set_size(font, 10);
	tt_draw_string(ctx, font, x + 12, y + 20, label,
		disabled ? RAZION_TEXT_SECONDARY : RAZION_TEXT_PRIMARY);
	if (shortcut && shortcut[0]) {
		int sw = tt_string_width(font, shortcut);
		tt_draw_string(ctx, font, x + width - sw - 12, y + 20, shortcut, RAZION_TEXT_SECONDARY);
	}
}

static void draw_browser_menu(int ox, int oy, int width) {
	int x = ox + width - 220, y = oy + 91, w = 204;
	draw_rounded_rectangle(ctx, x + 4, y + 5, w, 222, 10, RAZION_BACKGROUND);
	draw_rounded_rectangle(ctx, x, y, w, 222, 9, RAZION_SURFACE_SECONDARY);
	draw_rectangle_solid(ctx, x + 12, y, w - 24, 1, RAZION_BORDER);
	menu_item(500, x + 8, y + 8, w - 16, "New tab", "Ctrl+T", 0, tab_count == TAB_MAX);
	menu_item(501, x + 8, y + 42, w - 16, "Bookmarks", "", tabs[active_tab].view == VIEW_BOOKMARKS, 0);
	menu_item(502, x + 8, y + 76, w - 16, "History", "", tabs[active_tab].view == VIEW_HISTORY, 0);
	menu_item(503, x + 8, y + 110, w - 16, "Downloads", "", tabs[active_tab].view == VIEW_DOWNLOADS, 0);
	menu_item(504, x + 8, y + 144, w - 16, "Privacy", "", tabs[active_tab].view == VIEW_SETTINGS, 0);
	draw_rectangle_solid(ctx, x + 14, y + 181, w - 28, 1, RAZION_BORDER);
	menu_item(505, x + 8, y + 186, w - 16, "Close Ripper", "", 0, 0);
}

static void redraw(void) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	int ox = bounds.left_width, oy = bounds.top_height;
	int width = window->width - bounds.width, height = window->height - bounds.height;
	draw_fill(ctx, RAZION_BACKGROUND);

	int tab_y = oy, tab_h = 44, tab_w = (width - 58) / (tab_count < 4 ? 4 : tab_count);
	if (tab_w > 220) tab_w = 220;
	draw_rectangle_solid(ctx, ox, tab_y, width, tab_h, RAZION_SURFACE);
	for (int i = 0; i < tab_count; ++i) {
		int id = 100 + i, x = ox + 6 + i * tab_w;
		if (id == focus_id && window->focused)
			draw_rounded_rectangle(ctx, x, tab_y + 4, tab_w - 4, tab_h - 5, 9, RAZION_FOCUS);
		draw_rounded_rectangle(ctx, x + 2, tab_y + 6, tab_w - 8, tab_h - 7, 7,
			i == active_tab ? RAZION_SURFACE_SECONDARY : id == hover_id ? RAZION_SURFACE_HOVER : RAZION_SURFACE);
		draw_rounded_rectangle(ctx, x + 13, tab_y + 17, 10, 10, 4,
			i == active_tab ? RAZION_ACCENT : RAZION_BORDER);
		draw_ellipsized(font, 9, x + 30, tab_y + 29, tabs[i].title, tab_w - 67,
			i == active_tab ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY);
		int close_id = 200 + i;
		if (close_id == focus_id && window->focused)
			draw_rounded_rectangle(ctx, x + tab_w - 38, tab_y + 9, 26, 26, 8, RAZION_FOCUS);
		if (close_id == hover_id || close_id == pressed_id)
			draw_rounded_rectangle(ctx, x + tab_w - 36, tab_y + 11, 22, 22, 6,
				close_id == pressed_id ? RAZION_SELECTION : RAZION_SURFACE_HOVER);
		tt_set_size(font, 11); tt_draw_string(ctx, font, x + tab_w - 29, tab_y + 27, "x", RAZION_TEXT_SECONDARY);
		if (i == active_tab) draw_rectangle_solid(ctx, x + 13, tab_y + tab_h - 2, tab_w - 30, 2, RAZION_ACCENT);
	}
	button(300, ox + 8 + tab_count * tab_w, tab_y + 7, 36, 32, "+", 0, tab_count == TAB_MAX);
	draw_rectangle_solid(ctx, ox, oy + tab_h, width, 1, RAZION_BORDER);

	int toolbar_y = oy + tab_h + 7;
	icon_button(1, ox + 10, toolbar_y, 0, tabs[active_tab].history_index == 0);
	icon_button(2, ox + 50, toolbar_y, 1, tabs[active_tab].history_index + 1 >= tabs[active_tab].history_count);
	icon_button(3, ox + 90, toolbar_y, 3, 0);
	icon_button(4, ox + 130, toolbar_y, 2, 0);
	int address_x = ox + 176;
	int address_w = width - 276;
	if (editing_address || focus_id == 5)
		draw_rounded_rectangle(ctx, address_x - 2, toolbar_y - 2, address_w + 4, 38, 10, RAZION_FOCUS);
	draw_rounded_rectangle(ctx, address_x, toolbar_y, address_w, 34, 8,
		address_selected && editing_address ? RAZION_SELECTION : RAZION_SURFACE_SECONDARY);
	int is_local = !strncmp(tabs[active_tab].url, "about:", 6) || !strncmp(tabs[active_tab].url, "razion:", 7);
	int is_secure = starts_with_folded(tabs[active_tab].url, "https://");
	draw_rounded_rectangle(ctx, address_x + 9, toolbar_y + 8, 18, 18, 6,
		is_local ? RAZION_SELECTION : is_secure ? RAZION_SUCCESS : RAZION_SURFACE_HOVER);
	tt_set_size(bold, 9); tt_draw_string(ctx, bold, address_x + 15, toolbar_y + 21,
		is_local ? "R" : is_secure ? "S" : "i", RAZION_TEXT_PRIMARY);
	draw_ellipsized(font, 10, address_x + 36, toolbar_y + 22,
		address[0] ? address : "Search or enter an address", address_w - 78,
		address[0] ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY);
	if (focus_id == 7 && window->focused)
		draw_rounded_rectangle(ctx, address_x + address_w - 31, toolbar_y + 4, 27, 27, 8, RAZION_FOCUS);
	if (hover_id == 7 || pressed_id == 7 || tabs[active_tab].bookmarked)
		draw_rounded_rectangle(ctx, address_x + address_w - 29, toolbar_y + 6, 23, 23, 7,
			tabs[active_tab].bookmarked || pressed_id == 7 ? RAZION_SELECTION : RAZION_SURFACE_HOVER);
	draw_bookmark_star(address_x + address_w - 18, toolbar_y + 17,
		tabs[active_tab].bookmarked ? RAZION_FOCUS : RAZION_TEXT_SECONDARY);
	button(6, address_x + address_w + 8, toolbar_y, 44, 34, "Go", 0, 0);
	icon_button(24, address_x + address_w + 56, toolbar_y, 4, 0);

	int nav_y = toolbar_y + 41;
	draw_rectangle_solid(ctx, ox, nav_y, width, 35, RAZION_SURFACE);
	tt_set_size(bold, 10); tt_draw_string(ctx, bold, ox + 16, nav_y + 22, "RIPPER", RAZION_ACCENT);
	button(20, ox + 86, nav_y + 4, 88, 27, "Bookmarks", tabs[active_tab].view == VIEW_BOOKMARKS, 0);
	button(21, ox + 180, nav_y + 4, 70, 27, "History", tabs[active_tab].view == VIEW_HISTORY, 0);
	button(22, ox + 256, nav_y + 4, 90, 27, "Downloads", tabs[active_tab].view == VIEW_DOWNLOADS, 0);
	button(23, ox + 352, nav_y + 4, 76, 27, "Privacy", tabs[active_tab].view == VIEW_SETTINGS, 0);
	if (width > 900) {
		tt_set_size(font, 9); const char * shortcut = "Ctrl+L address   Ctrl+T new tab   Ctrl+W close tab";
		tt_draw_string(ctx, font, ox + width - tt_string_width(font, shortcut) - 16, nav_y + 22,
			shortcut, RAZION_TEXT_SECONDARY);
	}

	int body_y = nav_y + 35;
	int body_h = height - (body_y - oy) - 31;
	draw_rounded_rectangle(ctx, ox + 8, body_y + 8, width - 16, body_h - 8, 8, RAZION_SURFACE_SECONDARY);
	browser_view_t view = tabs[active_tab].view;
	if (view == VIEW_HOME) draw_home(ox + 8, body_y + 8, width - 16, body_h - 8);
	else if (view == VIEW_EXTERNAL) draw_external(ox + 8, body_y + 8, width - 16, body_h - 8);
	else if (view == VIEW_SEARCH) draw_search(ox + 8, body_y + 8, width - 16, body_h - 8);
	else draw_list_view(ox + 8, body_y + 8, width - 16, view);

	draw_rectangle_solid(ctx, ox + 8, oy + height - 27, width - 16, 1, RAZION_BORDER);
	tt_set_size(font, 8); draw_ellipsized(font, 8, ox + 18, oy + height - 10,
		status_text, width - 132, RAZION_TEXT_SECONDARY);
	const char * privacy = "LOCAL-FIRST";
	tt_set_size(bold, 8); tt_draw_string(ctx, bold, ox + width - tt_string_width(bold, privacy) - 18,
		oy + height - 10, privacy, RAZION_ACCENT);
	if (menu_open) draw_browser_menu(ox, oy, width);
	render_decorations(window, ctx, "Ripper — Razion Browser");
	flip(ctx);
	yutani_flip(yctx, window);
}

static int hit(int x, int y, int w, int h, struct yutani_msg_window_mouse_event * mouse) {
	return mouse->new_x >= x && mouse->new_x < x + w && mouse->new_y >= y && mouse->new_y < y + h;
}

static void start_ai(void) {
	if (ai_pending || tabs[active_tab].view != VIEW_SEARCH) return;
	if (razion_ai_request_begin(&ai_context, RAZION_AI_OP_SEARCH, tabs[active_tab].url,
		RAZION_AI_FLAG_LOCAL_ONLY, &ai_request)) {
		copy_text(status_text, sizeof(status_text), "Razion AI Engine is unavailable. No external search provider was contacted.");
		return;
	}
	ai_pending = 1;
	copy_text(status_text, sizeof(status_text), "Razion AI local-only search requested by the user.");
}

static void poll_ai(void) {
	if (!ai_pending) return;
	razion_ai_response_t response;
	int result = razion_ai_request_poll(&ai_request, 0, &response);
	if (!result) return;
	ai_pending = 0;
	if (result < 0) copy_text(ai_result, sizeof(ai_result), "The AI transport failed.");
	else if (response.status != RAZION_AI_STATUS_OK) snprintf(ai_result, sizeof(ai_result),
		"Razion AI: %s", razion_ai_status_string(response.status));
	else {
		response.payload[response.payload_length] = '\0';
		copy_text(ai_result, sizeof(ai_result), response.payload);
	}
	razion_ai_request_cancel(&ai_request);
	copy_text(status_text, sizeof(status_text), "AI request completed. It did not trigger navigation or a download.");
}

static void new_tab(void) {
	if (tab_count == TAB_MAX) return;
	tab_init(&tabs[tab_count]);
	active_tab = tab_count++;
	sync_address();
}

static void close_tab(int index) {
	if (tab_count == 1) { tab_init(&tabs[0]); active_tab = 0; sync_address(); return; }
	memmove(&tabs[index], &tabs[index + 1], sizeof(tabs[0]) * (tab_count - index - 1));
	tab_count--;
	if (active_tab >= tab_count) active_tab = tab_count - 1;
	else if (active_tab > index) active_tab--;
	sync_address();
}

static void history_move(int direction) {
	browser_tab_t * tab = &tabs[active_tab];
	int next = tab->history_index + direction;
	if (next < 0 || next >= tab->history_count) return;
	tab->history_index = next;
	apply_location(tab, tab->history[next], 0);
}

static int control_at(struct yutani_msg_window_mouse_event * mouse) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	int ox = bounds.left_width, oy = bounds.top_height;
	int width = window->width - bounds.width, height = window->height - bounds.height;
	int toolbar_y = oy + 51;
	if (menu_open) {
		int menu_x = ox + width - 220, menu_y = oy + 91;
		for (int i = 0; i < 5; ++i)
			if ((i || tab_count < TAB_MAX) && hit(menu_x + 8, menu_y + 8 + i * 34, 188, 30, mouse)) return 500 + i;
		if (hit(menu_x + 8, menu_y + 186, 188, 30, mouse)) return 505;
		int address_w = width - 276;
		if (hit(ox + 176 + address_w + 56, toolbar_y, 36, 34, mouse)) return 24;
		return 498;
	}
	int tab_w = (width - 58) / (tab_count < 4 ? 4 : tab_count);
	if (tab_w > 220) tab_w = 220;
	for (int i = 0; i < tab_count; ++i) {
		int x = ox + 6 + i * tab_w;
		if (hit(x + tab_w - 39, oy + 8, 28, 28, mouse)) return 200 + i;
		if (hit(x + 2, oy + 6, tab_w - 8, 37, mouse)) return 100 + i;
	}
	if (tab_count < TAB_MAX && hit(ox + 8 + tab_count * tab_w, oy + 7, 36, 32, mouse)) return 300;
	if (tabs[active_tab].history_index > 0 && hit(ox + 10, toolbar_y, 36, 34, mouse)) return 1;
	if (tabs[active_tab].history_index + 1 < tabs[active_tab].history_count && hit(ox + 50, toolbar_y, 36, 34, mouse)) return 2;
	if (hit(ox + 90, toolbar_y, 36, 34, mouse)) return 3;
	if (hit(ox + 130, toolbar_y, 36, 34, mouse)) return 4;
	int address_x = ox + 176, address_w = width - 276;
	if (hit(address_x + address_w - 34, toolbar_y, 32, 34, mouse)) return 7;
	if (hit(address_x, toolbar_y, address_w, 34, mouse)) return 5;
	if (hit(address_x + address_w + 8, toolbar_y, 44, 34, mouse)) return 6;
	if (hit(address_x + address_w + 56, toolbar_y, 36, 34, mouse)) return 24;
	int nav_y = toolbar_y + 41;
	if (hit(ox + 86, nav_y + 4, 88, 27, mouse)) return 20;
	if (hit(ox + 180, nav_y + 4, 70, 27, mouse)) return 21;
	if (hit(ox + 256, nav_y + 4, 90, 27, mouse)) return 22;
	if (hit(ox + 352, nav_y + 4, 76, 27, mouse)) return 23;
	int body_y = nav_y + 43;
	if (tabs[active_tab].view == VIEW_HOME) {
		int body_h = height - (nav_y + 35 - oy) - 31;
		int actions_y = body_h - 8 < 460 ? 326 : 386;
		if (hit(ox + 42, body_y + actions_y, 108, 34, mouse)) return 20;
		if (hit(ox + 158, body_y + actions_y, 88, 34, mouse)) return 21;
		if (hit(ox + 254, body_y + actions_y, 106, 34, mouse)) return 22;
		if (hit(ox + 368, body_y + actions_y, 92, 34, mouse)) return 23;
	} else if (tabs[active_tab].view == VIEW_EXTERNAL &&
		hit(ox + 262, body_y + 312, 180, 34, mouse)) return 400;
	else if (tabs[active_tab].view == VIEW_SEARCH && !ai_pending &&
		hit(ox + 44, body_y + 260, 178, 34, mouse)) return 401;
	return -1;
}

static void activate(int id) {
	if (id == 498) { menu_open = 0; focus_id = 24; }
	else if (id >= 100 && id < 100 + tab_count) {
		active_tab = id - 100; sync_address(); editing_address = 0; address_selected = 0;
	} else if (id >= 200 && id < 200 + tab_count) close_tab(id - 200);
	else if (id == 300) new_tab();
	else if (id == 1) history_move(-1);
	else if (id == 2) history_move(1);
	else if (id == 3) { network_available = network_online(); apply_location(&tabs[active_tab], tabs[active_tab].url, 0); }
	else if (id == 4) navigate("about:home");
	else if (id == 5) { editing_address = 1; address_selected = 0; }
	else if (id == 6) { editing_address = 0; address_selected = 0; navigate(address); }
	else if (id == 7 || id == 400) {
		tabs[active_tab].bookmarked = !tabs[active_tab].bookmarked;
		copy_text(status_text, sizeof(status_text), tabs[active_tab].bookmarked ?
			"Bookmark saved for this session." : "Bookmark removed.");
	}
	else if (id == 20) tabs[active_tab].view = VIEW_BOOKMARKS;
	else if (id == 21) tabs[active_tab].view = VIEW_HISTORY;
	else if (id == 22) tabs[active_tab].view = VIEW_DOWNLOADS;
	else if (id == 23) tabs[active_tab].view = VIEW_SETTINGS;
	else if (id == 24) { menu_open = !menu_open; focus_id = menu_open ? (tab_count < TAB_MAX ? 500 : 501) : 24; }
	else if (id == 500) { if (tab_count < TAB_MAX) new_tab(); menu_open = 0; focus_id = 5; }
	else if (id == 501) { tabs[active_tab].view = VIEW_BOOKMARKS; menu_open = 0; focus_id = 20; }
	else if (id == 502) { tabs[active_tab].view = VIEW_HISTORY; menu_open = 0; focus_id = 21; }
	else if (id == 503) { tabs[active_tab].view = VIEW_DOWNLOADS; menu_open = 0; focus_id = 22; }
	else if (id == 504) { tabs[active_tab].view = VIEW_SETTINGS; menu_open = 0; focus_id = 23; }
	else if (id == 505) running = 0;
	else if (id == 401) start_ai();
}

static int focusable_controls(int * ids, int capacity) {
	int count = 0;
#define ADD_CONTROL(value) do { if (count < capacity) ids[count++] = (value); } while (0)
	if (menu_open) {
		if (tab_count < TAB_MAX) ADD_CONTROL(500);
		ADD_CONTROL(501); ADD_CONTROL(502); ADD_CONTROL(503); ADD_CONTROL(504); ADD_CONTROL(505);
		return count;
	}
	for (int i = 0; i < tab_count; ++i) { ADD_CONTROL(100 + i); ADD_CONTROL(200 + i); }
	if (tab_count < TAB_MAX) ADD_CONTROL(300);
	if (tabs[active_tab].history_index > 0) ADD_CONTROL(1);
	if (tabs[active_tab].history_index + 1 < tabs[active_tab].history_count) ADD_CONTROL(2);
	ADD_CONTROL(3); ADD_CONTROL(4); ADD_CONTROL(5); ADD_CONTROL(7); ADD_CONTROL(6); ADD_CONTROL(24);
	ADD_CONTROL(20); ADD_CONTROL(21); ADD_CONTROL(22); ADD_CONTROL(23);
	if (tabs[active_tab].view == VIEW_EXTERNAL) ADD_CONTROL(400);
	else if (tabs[active_tab].view == VIEW_SEARCH && !ai_pending) ADD_CONTROL(401);
#undef ADD_CONTROL
	return count;
}

static void move_focus(int direction) {
	int ids[32], count = focusable_controls(ids, 32), current = -1;
	if (!count) return;
	for (int i = 0; i < count; ++i) if (ids[i] == focus_id) { current = i; break; }
	if (current < 0) current = direction > 0 ? count - 1 : 0;
	focus_id = ids[(current + direction + count) % count];
	editing_address = focus_id == 5;
	if (!editing_address) address_selected = 0;
}

int main(void) {
	tab_init(&tabs[0]); tab_count = 1; sync_address();
	yctx = yutani_init();
	if (!yctx) return 1;
	network_available = network_online();
	init_decorations();
	struct decor_bounds bounds;
	decor_get_bounds(NULL, &bounds);
	int width = BROWSER_WIDTH + bounds.width, height = BROWSER_HEIGHT + bounds.height;
	if (width > (int)yctx->display_width - 16) width = yctx->display_width - 16;
	if (height > (int)yctx->display_height - 42) height = yctx->display_height - 42;
	window = yutani_window_create(yctx, width, height);
	yutani_window_move(yctx, window, yctx->display_width / 2 - width / 2,
		28 + (yctx->display_height - 28) / 2 - height / 2);
	yutani_window_advertise_icon(yctx, window, "Ripper — Razion Browser", "razion-ripper");
	ctx = init_graphics_yutani_double_buffer(window);
	font = tt_font_from_shm("sans-serif"); bold = tt_font_from_shm("sans-serif.bold");
	razion_ai_init(&ai_context, "razion-browser"); ai_context.default_flags = RAZION_AI_FLAG_LOCAL_ONLY;
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
				int ctrl = key->event.modifiers & (KEY_MOD_LEFT_CTRL | KEY_MOD_RIGHT_CTRL);
				int alt = key->event.modifiers & (KEY_MOD_LEFT_ALT | KEY_MOD_RIGHT_ALT);
				int shift = key->event.modifiers & (KEY_MOD_LEFT_SHIFT | KEY_MOD_RIGHT_SHIFT);
				if (ctrl && key->event.keycode == 'l') {
					focus_id = 5; editing_address = 1; address_selected = 1;
				} else if (ctrl && key->event.keycode == 't') {
					new_tab(); menu_open = 0; focus_id = 5; editing_address = 1; address_selected = 1;
				} else if (ctrl && key->event.keycode == 'w') {
					close_tab(active_tab); menu_open = 0; focus_id = 5; editing_address = 0; address_selected = 0;
				} else if (ctrl && key->event.keycode == 'd') {
					activate(7);
				} else if (ctrl && key->event.keycode == '\t') {
					active_tab = (active_tab + (shift ? tab_count - 1 : 1)) % tab_count;
					sync_address(); editing_address = 0; address_selected = 0;
				} else if (alt && key->event.keycode == KEY_ARROW_LEFT) history_move(-1);
				else if (alt && key->event.keycode == KEY_ARROW_RIGHT) history_move(1);
				else if (key->event.keycode == '\t') move_focus(shift ? -1 : 1);
				else if (menu_open && (key->event.keycode == KEY_ARROW_UP || key->event.keycode == KEY_ARROW_LEFT)) move_focus(-1);
				else if (menu_open && (key->event.keycode == KEY_ARROW_DOWN || key->event.keycode == KEY_ARROW_RIGHT)) move_focus(1);
				else if (key->event.keycode == KEY_ESCAPE) {
					if (menu_open) { menu_open = 0; focus_id = 24; }
					else if (editing_address) { editing_address = 0; address_selected = 0; sync_address(); }
					else running = 0;
				}
				else if (editing_address && (key->event.key == '\b' || key->event.keycode == KEY_BACKSPACE)) {
					if (address_selected) { address[0] = '\0'; address_length = 0; address_selected = 0; }
					else if (address_length) address[--address_length] = '\0';
				} else if (editing_address && key->event.key == '\n') {
					editing_address = 0; address_selected = 0; navigate(address);
				}
				else if (editing_address && key->event.key >= 0x20 && key->event.key < 0x7f && address_length + 1 < sizeof(address)) {
					if (address_selected) { address[0] = '\0'; address_length = 0; address_selected = 0; }
					address[address_length++] = key->event.key; address[address_length] = '\0';
				} else if (!editing_address && (key->event.key == '\n' || key->event.key == ' ')) activate(focus_id);
				redraw(); break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)event->data;
				if (mouse->wid != window->wid) break;
				int decor = decor_handle_event(yctx, event); if (decor == DECOR_CLOSE) running = 0;
				int old_hover = hover_id, old_pressed = pressed_id, activated = 0;
				int over = control_at(mouse);
				if (mouse->command == YUTANI_MOUSE_EVENT_DOWN) {
					pressed_id = over; if (over >= 0 && over != 498) focus_id = over;
				} else if (mouse->command == YUTANI_MOUSE_EVENT_LEAVE) {
					hover_id = -1; pressed_id = -1;
				} else if (mouse->command == YUTANI_MOUSE_EVENT_RAISE || mouse->command == YUTANI_MOUSE_EVENT_CLICK) {
					if (over >= 0 && over == pressed_id) { activate(over); activated = 1; }
					pressed_id = -1;
				}
				hover_id = over;
				if (decor == DECOR_REDRAW || old_hover != hover_id || old_pressed != pressed_id || activated) redraw();
				break;
			}
			case YUTANI_MSG_RESIZE_OFFER: {
				struct yutani_msg_window_resize * resize = (void *)event->data;
				int w = resize->width < 780 ? 780 : resize->width, h = resize->height < 540 ? 540 : resize->height;
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
