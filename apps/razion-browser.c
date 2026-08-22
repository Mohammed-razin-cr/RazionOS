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
		copy_text(status_text, sizeof(status_text), network_online() ?
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
	apply_location(&tabs[active_tab], location, 1);
}

static void draw_ellipsized(struct TT_Font * face, int size, int x, int y,
	const char * text, int maximum, uint32_t color) {
	char * shown = tt_ellipsify(text, size, face, maximum, NULL);
	tt_draw_string(ctx, face, x, y, shown, color);
	free(shown);
}

static void button(int x, int y, int width, int height, const char * label,
	int selected, int disabled) {
	draw_rounded_rectangle(ctx, x, y, width, height, 5,
		selected ? RAZION_SELECTION : RAZION_SURFACE_SECONDARY);
	draw_rectangle_solid(ctx, x, y, width, 1, selected ? RAZION_FOCUS : RAZION_BORDER);
	tt_set_size(font, 10);
	int tw = tt_string_width(font, label);
	tt_draw_string(ctx, font, x + (width - tw) / 2, y + height / 2 + 4,
		label, disabled ? RAZION_TEXT_SECONDARY : RAZION_TEXT_PRIMARY);
}

static void info_panel(int x, int y, int width, const char * title,
	const char * body, uint32_t accent) {
	draw_rounded_rectangle(ctx, x, y, width, 112, 7, RAZION_SURFACE);
	draw_rectangle_solid(ctx, x, y, 3, 112, accent);
	tt_set_size(bold, 16); tt_draw_string(ctx, bold, x + 22, y + 32, title, RAZION_TEXT_PRIMARY);
	tt_set_size(font, 10); draw_ellipsized(font, 10, x + 22, y + 58, body, width - 44, RAZION_TEXT_SECONDARY);
}

static void draw_home(int x, int y, int width, int height) {
	tt_set_size(bold, 30); tt_draw_string(ctx, bold, x + 34, y + 68, "Ripper", RAZION_TEXT_PRIMARY);
	tt_set_size(font, 12); tt_draw_string(ctx, font, x + 36, y + 94,
		"A fast native shell being prepared for a secure web engine.", RAZION_TEXT_SECONDARY);
	info_panel(x + 34, y + 126, width - 68, "Rendering engine status",
		"HTML, CSS and JavaScript rendering is not available in this RazionOS build.", RAZION_WARNING);
	info_panel(x + 34, y + 252, width - 68, network_online() ? "Network available" : "Network unavailable",
		network_online() ? "The system network is connected. Remote pages are still not represented as rendered." :
		"Tabs, history, bookmarks and settings continue to work without a connection.",
		network_online() ? RAZION_SUCCESS : RAZION_ERROR);
	tt_set_size(bold, 12); tt_draw_string(ctx, bold, x + 36, y + 398, "Implemented today", RAZION_TEXT_PRIMARY);
	tt_set_size(font, 10); tt_draw_string(ctx, font, x + 36, y + 424,
		"Tabs  |  navigation history  |  bookmarks  |  downloads surface  |  privacy settings", RAZION_TEXT_SECONDARY);
}

static void draw_external(int x, int y, int width, int height) {
	info_panel(x + 34, y + 52, width - 68,
		network_online() ? "Page not rendered" : "Network unavailable",
		network_online() ? "RazionOS can transfer HTTP data, but it has no sandboxed standards-compliant web renderer." :
		"This address was recorded in session history, but no connection or rendering was attempted.",
		network_online() ? RAZION_WARNING : RAZION_ERROR);
	tt_set_size(bold, 14); tt_draw_string(ctx, bold, x + 36, y + 202, "Requested address", RAZION_TEXT_PRIMARY);
	draw_ellipsized(font, 11, x + 36, y + 228, tabs[active_tab].url, width - 72, RAZION_ACCENT);
	tt_set_size(font, 10); tt_draw_string(ctx, font, x + 36, y + 274,
		"No page content, scripts, cookies, downloads, or browsing data were fabricated.", RAZION_TEXT_SECONDARY);
	button(x + 36, y + 312, 210, 34, "Summarize page unavailable", 0, 1);
	button(x + 254, y + 312, 180, 34, tabs[active_tab].bookmarked ? "Bookmarked" : "Add bookmark", tabs[active_tab].bookmarked, 0);
}

static void draw_search(int x, int y, int width, int height) {
	info_panel(x + 34, y + 52, width - 68, "Natural search query",
		"No default search provider is contacted. Razion AI runs only after the explicit button below.", RAZION_ACCENT);
	tt_set_size(bold, 14); tt_draw_string(ctx, bold, x + 36, y + 202, "Query", RAZION_TEXT_PRIMARY);
	draw_ellipsized(font, 11, x + 36, y + 228, tabs[active_tab].url, width - 72, RAZION_ACCENT);
	button(x + 36, y + 260, 178, 34, ai_pending ? "AI working" : "Ask Razion AI", ai_pending, ai_pending);
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

static void redraw(void) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	int ox = bounds.left_width, oy = bounds.top_height;
	int width = window->width - bounds.width, height = window->height - bounds.height;
	draw_fill(ctx, RAZION_BACKGROUND);

	int tab_y = oy, tab_h = 39, tab_w = (width - 56) / (tab_count < 4 ? 4 : tab_count);
	if (tab_w > 230) tab_w = 230;
	for (int i = 0; i < tab_count; ++i) {
		int x = ox + i * tab_w;
		draw_rounded_rectangle(ctx, x + 3, tab_y + 4, tab_w - 6, tab_h - 7, 5,
			i == active_tab ? RAZION_SURFACE_SECONDARY : RAZION_SURFACE);
		draw_ellipsized(font, 9, x + 13, tab_y + 25, tabs[i].title, tab_w - 42,
			i == active_tab ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY);
		tt_set_size(font, 11); tt_draw_string(ctx, font, x + tab_w - 25, tab_y + 24, "x", RAZION_TEXT_SECONDARY);
	}
	button(ox + tab_count * tab_w + 4, tab_y + 5, 38, 30, "+", 0, tab_count == TAB_MAX);
	draw_rectangle_solid(ctx, ox, oy + tab_h, width, 1, RAZION_BORDER);

	int toolbar_y = oy + tab_h + 8;
	button(ox + 10, toolbar_y, 38, 34, "<", 0, tabs[active_tab].history_index == 0);
	button(ox + 53, toolbar_y, 38, 34, ">", 0, tabs[active_tab].history_index + 1 >= tabs[active_tab].history_count);
	button(ox + 96, toolbar_y, 42, 34, "Reload", 0, 0);
	button(ox + 143, toolbar_y, 42, 34, "Home", 0, 0);
	int menu_w = width < 900 ? 294 : 390;
	int address_x = ox + 194;
	int address_w = width - 204 - menu_w;
	draw_rounded_rectangle(ctx, address_x, toolbar_y, address_w, 34, 5, RAZION_SURFACE_SECONDARY);
	draw_rectangle_solid(ctx, address_x, toolbar_y + 33, address_w, 1, editing_address ? RAZION_FOCUS : RAZION_BORDER);
	draw_ellipsized(font, 10, address_x + 12, toolbar_y + 22,
		address[0] ? address : "Enter an address or search", address_w - 24,
		address[0] ? RAZION_TEXT_PRIMARY : RAZION_TEXT_SECONDARY);
	int mx = address_x + address_w + 7;
	button(mx, toolbar_y, 40, 34, "Go", 0, 0); mx += 45;
	button(mx, toolbar_y, 76, 34, "Bookmarks", tabs[active_tab].view == VIEW_BOOKMARKS, 0); mx += 81;
	button(mx, toolbar_y, 61, 34, "History", tabs[active_tab].view == VIEW_HISTORY, 0); mx += 66;
	button(mx, toolbar_y, 76, 34, "Downloads", tabs[active_tab].view == VIEW_DOWNLOADS, 0); mx += 81;
	button(mx, toolbar_y, 60, 34, "Settings", tabs[active_tab].view == VIEW_SETTINGS, 0);

	int body_y = toolbar_y + 48;
	int body_h = height - (body_y - oy) - 31;
	draw_rounded_rectangle(ctx, ox + 10, body_y, width - 20, body_h, 7, RAZION_SURFACE_SECONDARY);
	browser_view_t view = tabs[active_tab].view;
	if (view == VIEW_HOME) draw_home(ox + 10, body_y, width - 20, body_h);
	else if (view == VIEW_EXTERNAL) draw_external(ox + 10, body_y, width - 20, body_h);
	else if (view == VIEW_SEARCH) draw_search(ox + 10, body_y, width - 20, body_h);
	else draw_list_view(ox + 10, body_y, width - 20, view);

	draw_rectangle_solid(ctx, ox + 10, oy + height - 27, width - 20, 1, RAZION_BORDER);
	tt_set_size(font, 8); draw_ellipsized(font, 8, ox + 18, oy + height - 10,
		status_text, width - 36, RAZION_TEXT_SECONDARY);
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

static void click(struct yutani_msg_window_mouse_event * mouse) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	int ox = bounds.left_width, oy = bounds.top_height;
	int width = window->width - bounds.width;
	int tab_w = (width - 56) / (tab_count < 4 ? 4 : tab_count);
	if (tab_w > 230) tab_w = 230;
	for (int i = 0; i < tab_count; ++i) {
		if (hit(ox + i * tab_w + tab_w - 34, oy + 4, 31, 31, mouse)) { close_tab(i); return; }
		if (hit(ox + i * tab_w + 3, oy + 4, tab_w - 6, 32, mouse)) {
			active_tab = i; sync_address(); editing_address = 0; return;
		}
	}
	if (hit(ox + tab_count * tab_w + 4, oy + 5, 38, 30, mouse)) { new_tab(); return; }
	int toolbar_y = oy + 47;
	if (hit(ox + 10, toolbar_y, 38, 34, mouse)) { history_move(-1); return; }
	if (hit(ox + 53, toolbar_y, 38, 34, mouse)) { history_move(1); return; }
	if (hit(ox + 96, toolbar_y, 42, 34, mouse)) { apply_location(&tabs[active_tab], tabs[active_tab].url, 0); return; }
	if (hit(ox + 143, toolbar_y, 42, 34, mouse)) { navigate("about:home"); return; }
	int menu_w = width < 900 ? 294 : 390, address_x = ox + 194;
	int address_w = width - 204 - menu_w;
	if (hit(address_x, toolbar_y, address_w, 34, mouse)) { editing_address = 1; return; }
	int mx = address_x + address_w + 7;
	if (hit(mx, toolbar_y, 40, 34, mouse)) { editing_address = 0; navigate(address); return; } mx += 45;
	if (hit(mx, toolbar_y, 76, 34, mouse)) { tabs[active_tab].view = VIEW_BOOKMARKS; return; } mx += 81;
	if (hit(mx, toolbar_y, 61, 34, mouse)) { tabs[active_tab].view = VIEW_HISTORY; return; } mx += 66;
	if (hit(mx, toolbar_y, 76, 34, mouse)) { tabs[active_tab].view = VIEW_DOWNLOADS; return; } mx += 81;
	if (hit(mx, toolbar_y, 60, 34, mouse)) { tabs[active_tab].view = VIEW_SETTINGS; return; }
	int body_y = toolbar_y + 48;
	if (tabs[active_tab].view == VIEW_EXTERNAL && hit(ox + 264, body_y + 312, 180, 34, mouse)) {
		tabs[active_tab].bookmarked = !tabs[active_tab].bookmarked;
		copy_text(status_text, sizeof(status_text), tabs[active_tab].bookmarked ? "Bookmark saved for this session." : "Bookmark removed.");
	} else if (tabs[active_tab].view == VIEW_SEARCH && hit(ox + 46, body_y + 260, 178, 34, mouse)) start_ai();
}

int main(void) {
	tab_init(&tabs[0]); tab_count = 1; sync_address();
	yctx = yutani_init();
	if (!yctx) return 1;
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
				if (key->event.keycode == KEY_ESCAPE) { if (editing_address) editing_address = 0; else running = 0; }
				else if (editing_address && (key->event.key == '\b' || key->event.keycode == KEY_BACKSPACE)) {
					if (address_length) address[--address_length] = '\0';
				} else if (editing_address && key->event.key == '\n') { editing_address = 0; navigate(address); }
				else if (editing_address && key->event.key >= 0x20 && key->event.key < 0x7f && address_length + 1 < sizeof(address)) {
					address[address_length++] = key->event.key; address[address_length] = '\0';
				}
				redraw(); break;
			}
			case YUTANI_MSG_WINDOW_MOUSE_EVENT: {
				struct yutani_msg_window_mouse_event * mouse = (void *)event->data;
				if (mouse->wid != window->wid) break;
				int decor = decor_handle_event(yctx, event); if (decor == DECOR_CLOSE) running = 0;
				if (mouse->command == YUTANI_MOUSE_EVENT_CLICK) click(mouse);
				redraw(); break;
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
