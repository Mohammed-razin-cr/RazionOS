/* Premium native management surface for Razion Companion.
 *
 * This file is included by razion-companion.c so it can reuse the existing
 * renderer, engine connection, sound and persistence functions without adding
 * another library or process boundary.
 */

static int hover_x = -1;
static int hover_y = -1;
static int mouse_down = 0;
static int focus_x = -1;
static int focus_y = -1;
static int editing_name = 0;
static int editing_search = 0;
static int active_section = 0;
static int pet_page = 0;
static int control_phase = 0;
static char search_text[24] = "";
static uint64_t status_until = 0;
static char status_text[96] = "Changes are saved locally.";
static int reflex_active = 0;
static int reflex_x = 0;
static int reflex_y = 0;
static uint64_t reflex_until = 0;

typedef struct {
	int ox, oy, width, height;
	int nav_x, nav_y, nav_w;
	int main_x, main_y, main_w, main_h;
	int right_x, right_y, right_w, right_h;
} companion_layout_t;

static int hit(int x, int y, int width, int height, struct yutani_msg_window_mouse_event * mouse) {
	return mouse->new_x >= x && mouse->new_x < x + width && mouse->new_y >= y && mouse->new_y < y + height;
}

static int point_in(int x, int y, int width, int height, int px, int py) {
	return px >= x && px < x + width && py >= y && py < y + height;
}

static void layout_get(companion_layout_t * l) {
	struct decor_bounds bounds;
	decor_get_bounds(window, &bounds);
	l->ox = bounds.left_width;
	l->oy = bounds.top_height;
	l->width = window->width - bounds.width;
	l->height = window->height - bounds.height;
	int gap = l->width < 900 ? 9 : 12;
	l->nav_w = l->width < 900 ? 126 : 164;
	l->right_w = l->width < 900 ? 176 : 226;
	l->nav_x = l->ox;
	l->nav_y = l->oy;
	l->main_x = l->ox + l->nav_w + gap;
	l->main_y = l->oy + 58;
	l->right_x = l->ox + l->width - l->right_w;
	l->right_y = l->main_y;
	l->main_w = l->right_x - gap - l->main_x;
	/* Reserve a persistent reaction/status strip below both content columns. */
	l->main_h = l->height - 96;
	l->right_h = l->main_h;
}

static void panel(int x, int y, int width, int height) {
	draw_rounded_rectangle(ctx, x, y, width, height, 7, C_PANEL);
	draw_rectangle(ctx, x, y, width, 1, C_BORDER);
}

static void ui_icon(int icon, int x, int y, uint32_t color) {
	switch (icon) {
		case 0: circle(ctx, x, y, 6, color); circle(ctx, x, y, 3, C_PANEL); break;
		case 1: circle(ctx, x, y - 3, 4, color); draw_rounded_rectangle(ctx, x - 7, y + 3, 14, 7, 3, color); break;
		case 2: circle(ctx, x - 3, y - 2, 4, color); circle(ctx, x + 4, y + 3, 4, color); break;
		case 3: triangle(ctx, x, y - 7, 6, 13, color); break;
		case 4: draw_rectangle(ctx, x - 7, y - 6, 14, 12, color); circle(ctx, x, y, 3, color); break;
		case 5: circle(ctx, x, y, 7, color); triangle(ctx, x, y - 9, 3, 6, C_PANEL); break;
		default: circle(ctx, x, y, 7, color); circle(ctx, x, y, 3, C_PANEL); break;
	}
}

static void label_button_ex(int x, int y, int width, int height,
	const char * text, int selected, int disabled) {
	int hover = point_in(x, y, width, height, hover_x, hover_y);
	int focused = point_in(x, y, width, height, focus_x, focus_y);
	uint32_t fill = selected ? rgb(28,72,112) : hover && !disabled ? C_PANEL_HOVER : rgb(20,28,38);
	if (mouse_down && hover && !disabled) fill = rgb(25,55,82);
	draw_rounded_rectangle(ctx, x, y, width, height, 5, fill);
	draw_rectangle(ctx, x, y, width, 1, selected ? C_BLUE : focused ? rgb(113,184,255) : C_BORDER);
	tt_set_size(font, height < 30 ? 9 : 10);
	int text_width = tt_string_width(font, text);
	tt_draw_string(ctx, font, x + (width - text_width) / 2, y + height / 2 + 4,
		text, disabled ? rgb(83,96,108) : C_TEXT);
}

static const char * cycle_name(int kind, int value) {
	static const char * colors[] = {"Original", "Sky", "Mint", "Amber", "Violet", "Rose", "Gold", "Slate"};
	static const char * accessories[] = {"None", "Collar", "Scarf", "Charm", "Bow", "Glasses"};
	static const char * personalities[] = {"Species", "Playful", "Calm", "Curious", "Cozy"};
	static const char * sizes[] = {"", "Small", "Medium", "Large"};
	static const char * speeds[] = {"", "Gentle", "Normal", "Lively"};
	static const char * modes[] = {"Free movement", "Corner mode", "Stay nearby"};
	static const char * notices[] = {"Off", "Rare", "Balanced", "Often"};
	static const char * styles[] = {"Soft bounce", "Gentle sway", "Bright bounce"};
	switch (kind) {
		case 0: return colors[value];
		case 1: return accessories[value];
		case 2: return personalities[value];
		case 3: return sizes[value];
		case 4: return speeds[value];
		case 5: return modes[value];
		case 6: return notices[value];
		default: return styles[value];
	}
}

static void draw_pet_variant(int species, int x, int base_y, int scale, int phase, int plain) {
	int old_species = config.species;
	int old_accessory = config.accessory;
	int old_color = config.color;
	int old_pose = render_pose;
	config.species = species;
	if (plain) { config.accessory = 0; config.color = 0; }
	render_pose = 0;
	draw_pet(ctx, x, base_y, scale, phase);
	config.species = old_species;
	config.accessory = old_accessory;
	config.color = old_color;
	render_pose = old_pose;
}

static int contains_casefold(const char * haystack, const char * needle) {
	if (!*needle) return 1;
	for (; *haystack; ++haystack) {
		const char * h = haystack;
		const char * n = needle;
		while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { h++; n++; }
		if (!*n) return 1;
	}
	return 0;
}

static int matched_species(int * output) {
	int count = 0;
	for (int i = 0; i < RAZION_COMPANION_SPECIES_COUNT; ++i) {
		if (contains_casefold(razion_companion_species[i].name, search_text) ||
			contains_casefold(razion_companion_species[i].temperament, search_text)) output[count++] = i;
	}
	return count;
}

static const char * mood_text(void) {
	if (pet_state.health < 45) return "Needs care";
	if (pet_state.hunger < 35) return "Hungry";
	if (pet_state.energy < 30) return "Sleepy";
	if (pet_state.happiness > 78) return "Happy";
	return "Content";
}

static int bond_value(void) {
	int threshold = pet_state.level * 100;
	int value = (pet_state.level - 1) * 10;
	if (threshold) value += pet_state.experience * 10 / threshold;
	return value > 100 ? 100 : value;
}

static void section_heading(int x, int y, const char * title, const char * subtitle) {
	tt_set_size(font, 19);
	tt_draw_string(ctx, font, x, y + 19, title, C_TEXT);
	tt_set_size(font, 9);
	tt_draw_string(ctx, font, x, y + 36, subtitle, C_MUTED);
}

static void draw_navigation(const companion_layout_t * l) {
	draw_rectangle_solid(ctx, l->nav_x, l->nav_y, l->nav_w, l->height, rgb(10,15,22));
	draw_rectangle_solid(ctx, l->nav_x + l->nav_w - 1, l->nav_y, 1, l->height, C_BORDER);
	tt_set_size(font, l->nav_w < 140 ? 14 : 16);
	tt_draw_string(ctx, font, l->nav_x + 14, l->nav_y + 27, "RAZION", C_BLUE);
	tt_set_size(font, 9);
	tt_draw_string(ctx, font, l->nav_x + 14, l->nav_y + 42, "COMPANION", C_MUTED);
	static const char * names[] = {"My Companion", "Profile", "Customization", "Activities",
		"Inventory", "Achievements", "Settings"};
	for (int i = 0; i < 7; ++i) {
		int y = l->nav_y + 72 + i * 40;
		int hover = point_in(l->nav_x + 7, y, l->nav_w - 14, 34, hover_x, hover_y);
		if (i == active_section || hover) draw_rounded_rectangle(ctx, l->nav_x + 7, y, l->nav_w - 14, 34, 4,
			i == active_section ? rgb(23,54,82) : rgb(17,25,35));
		if (i == active_section) draw_rectangle_solid(ctx, l->nav_x + 7, y + 6, 2, 22, C_BLUE);
		ui_icon(i, l->nav_x + 21, y + 17, i == active_section ? C_BLUE : C_MUTED);
		tt_set_size(font, l->nav_w < 140 ? 8 : 9);
		tt_draw_string(ctx, font, l->nav_x + 34, y + 20, names[i], i == active_section ? C_TEXT : C_MUTED);
	}
	tt_set_size(font, 8);
	tt_draw_string(ctx, font, l->nav_x + 14, l->nav_y + l->height - 18, "Offline / private", C_MUTED);
}

static void draw_header(const companion_layout_t * l) {
	tt_set_size(font, 16);
	tt_draw_string(ctx, font, l->main_x, l->oy + 29, config.name, C_TEXT);
	tt_set_size(font, 9);
	char summary[72];
	snprintf(summary, sizeof(summary), "%s  /  %s", razion_companion_species[config.species].name, mood_text());
	tt_draw_string(ctx, font, l->main_x, l->oy + 45, summary, C_MUTED);
	int width = l->width < 900 ? 126 : 150;
	label_button_ex(l->right_x + l->right_w - width, l->oy + 12, width, 30,
		config.enabled ? "Desktop mode on" : "Desktop mode off", config.enabled, 0);
}

static void draw_search(int x, int y, int width) {
	draw_rounded_rectangle(ctx, x, y, width, 29, 4, rgb(16,24,33));
	draw_rectangle(ctx, x, y, width, 1, editing_search ? C_BLUE : C_BORDER);
	circle(ctx, x + 13, y + 13, 5, C_MUTED);
	draw_line(ctx, x + 17, x + 21, y + 17, y + 21, C_MUTED);
	tt_set_size(font, 9);
	const char * text = search_text[0] ? search_text : "Search pets";
	tt_draw_string(ctx, font, x + 28, y + 18, text, search_text[0] ? C_TEXT : C_MUTED);
	if (editing_search) {
		int cursor = tt_string_width(font, search_text);
		draw_rectangle_solid(ctx, x + 28 + cursor, y + 7, 1, 15, C_BLUE);
	}
}

static void draw_showcase(int x, int y, int width, int height) {
	panel(x, y, width, height);
	tt_set_size(font, 9);
	tt_draw_string(ctx, font, x + 14, y + 18, "YOUR COMPANION", C_MUTED);
	tt_set_size(font, 17);
	tt_draw_string(ctx, font, x + 14, y + 42, config.name, C_TEXT);
	int name_width = tt_string_width(font, config.name);
	label_button_ex(x + 18 + name_width, y + 24, 30, 22, "Edit", 0, 0);
	tt_set_size(font, 9);
	tt_draw_string(ctx, font, x + 14, y + 59, razion_companion_species[config.species].name, C_BLUE);
	int mood_w = 68;
	draw_rounded_rectangle(ctx, x + width - mood_w - 14, y + 13, mood_w, 23, 11, rgb(19,49,42));
	circle(ctx, x + width - mood_w, y + 24, 4, C_GREEN);
	tt_set_size(font, 8);
	tt_draw_string(ctx, font, x + width - mood_w + 9, y + 27, mood_text(), C_TEXT);
	int base = y + height - 17;
	int scale = height < 190 ? 68 : height < 245 ? 87 : 106;
	ellipse(ctx, x + width / 2, base + 2, 60 * scale / 100, 8 * scale / 100, rgb(8,13,19));
	draw_pet_variant(config.species, x + width / 2, base, scale, control_phase, 0);
}

static const int action_order[] = {RAZION_COMPANION_ACTION_FEED, RAZION_COMPANION_ACTION_PLAY,
	RAZION_COMPANION_ACTION_PET, RAZION_COMPANION_ACTION_SLEEP, RAZION_COMPANION_ACTION_TALK};

static void draw_actions(int x, int y, int width) {
	static const char * names[] = {"Feed", "Play", "Pet", "Sleep", "Talk"};
	int gap = 6;
	int bw = (width - gap * 4) / 5;
	for (int i = 0; i < 5; ++i) label_button_ex(x + i * (bw + gap), y, bw, 36, names[i], 0,
		(action_order[i] == RAZION_COMPANION_ACTION_PLAY && pet_state.energy < 8));
}

static void draw_stat_component(int x, int y, int width, const char * label, int value, uint32_t color) {
	char amount[16];
	snprintf(amount, sizeof(amount), "%d/100", value);
	circle(ctx, x + 7, y + 9, 4, color);
	tt_set_size(font, 9);
	tt_draw_string(ctx, font, x + 18, y + 12, label, C_TEXT);
	int tw = tt_string_width(font, amount);
	tt_draw_string(ctx, font, x + width - tw, y + 12, amount, C_MUTED);
	draw_rounded_rectangle(ctx, x, y + 21, width, 6, 3, rgb(28,39,50));
	if (value) draw_rounded_rectangle(ctx, x, y + 21, width * value / 100, 6, 3, color);
}

static int right_quick_y(const companion_layout_t * l) {
	return l->right_y + 18 + 16 + 41 * 4 + 16 + 76 + 16 + 49 + 18;
}

static void draw_right_panel(const companion_layout_t * l) {
	panel(l->right_x, l->right_y, l->right_w, l->right_h);
	int x = l->right_x + 14;
	int width = l->right_w - 28;
	int y = l->right_y + 18;
	tt_set_size(font, 11); tt_draw_string(ctx, font, x, y, "Wellbeing", C_TEXT); y += 16;
	draw_stat_component(x, y, width, "Happiness", pet_state.happiness, C_GREEN); y += 41;
	draw_stat_component(x, y, width, "Hunger", pet_state.hunger, C_ORANGE); y += 41;
	draw_stat_component(x, y, width, "Energy", pet_state.energy, C_BLUE); y += 41;
	draw_stat_component(x, y, width, "Health", pet_state.health, C_RED); y += 41;
	draw_rectangle_solid(ctx, x, y, width, 1, C_BORDER); y += 16;
	char level[32], xp[32], bond[32];
	snprintf(level, sizeof(level), "Level %d", pet_state.level);
	snprintf(xp, sizeof(xp), "%d / %d XP", pet_state.experience, pet_state.level * 100);
	snprintf(bond, sizeof(bond), "Bond %d%%", bond_value());
	tt_set_size(font, 11); tt_draw_string(ctx, font, x, y, level, C_TEXT);
	tt_set_size(font, 8); tt_draw_string(ctx, font, x, y + 16, xp, C_MUTED);
	draw_rounded_rectangle(ctx, x, y + 23, width, 6, 3, rgb(28,39,50));
	int threshold = pet_state.level * 100;
	if (pet_state.experience && threshold) draw_rounded_rectangle(ctx, x, y + 23,
		width * pet_state.experience / threshold, 6, 3, C_PURPLE);
	tt_draw_string(ctx, font, x, y + 44, bond, C_MUTED);
	draw_rounded_rectangle(ctx, x, y + 52, width, 5, 2, rgb(28,39,50));
	if (bond_value()) draw_rounded_rectangle(ctx, x, y + 52, width * bond_value() / 100, 5, 2, C_BLUE);
	y += 76;
	draw_rectangle_solid(ctx, x, y, width, 1, C_BORDER); y += 16;
	tt_set_size(font, 10); tt_draw_string(ctx, font, x, y, "Pet profile", C_TEXT);
	tt_set_size(font, 8);
	tt_draw_string(ctx, font, x, y + 16, razion_companion_species[config.species].temperament, C_MUTED);
	char detail[64];
	snprintf(detail, sizeof(detail), "%s / %s", cycle_name(2, config.personality), cycle_name(3, config.size));
	tt_draw_string(ctx, font, x, y + 31, detail, C_MUTED);
	y += 49;
	if (y + 58 < l->right_y + l->right_h) {
		tt_set_size(font, 10); tt_draw_string(ctx, font, x, y, "Quick actions", C_TEXT);
		y += 10;
		int gap = 6, bw = (width - gap) / 2;
		label_button_ex(x, y + 8, bw, 29, config.hidden ? "Show pet" : "Hide pet", !config.hidden, !config.enabled);
		label_button_ex(x + bw + gap, y + 8, bw, 29, config.sound ? "Sound on" : "Sound off", config.sound, 0);
	}
}

static void dashboard_geometry(const companion_layout_t * l, int * grid_y, int * cols,
	int * card_w, int * per_page, int * showcase_y, int * showcase_h, int * action_y) {
	*grid_y = l->main_y + 69;
	*cols = l->main_w / 118;
	if (*cols < 2) *cols = 2;
	if (*cols > 6) *cols = 6;
	*card_w = (l->main_w - (*cols - 1) * 6) / *cols;
	*per_page = *cols * 2;
	*action_y = l->main_y + l->main_h - 43;
	*showcase_y = *grid_y + 122;
	*showcase_h = *action_y - *showcase_y - 9;
}

static void draw_dashboard(const companion_layout_t * l) {
	section_heading(l->main_x, l->main_y, "Choose your companion", "Search and select from 22 original companions");
	int search_w = l->main_w < 470 ? 132 : 166;
	draw_search(l->main_x + l->main_w - search_w, l->main_y + 5, search_w);
	int grid_y, cols, card_w, per_page, showcase_y, showcase_h, action_y;
	dashboard_geometry(l, &grid_y, &cols, &card_w, &per_page, &showcase_y, &showcase_h, &action_y);
	int matches[RAZION_COMPANION_SPECIES_COUNT];
	int count = matched_species(matches);
	int pages = count ? (count + per_page - 1) / per_page : 1;
	if (pet_page >= pages) pet_page = pages - 1;
	if (pet_page < 0) pet_page = 0;
	for (int slot = 0; slot < per_page; ++slot) {
		int at = pet_page * per_page + slot;
		if (at >= count) break;
		int species = matches[at];
		int x = l->main_x + (slot % cols) * (card_w + 6);
		int y = grid_y + (slot / cols) * 58;
		int hover = point_in(x, y, card_w, 52, hover_x, hover_y);
		draw_rounded_rectangle(ctx, x, y, card_w, 52, 5,
			species == config.species ? rgb(24,59,88) : hover ? C_PANEL_HOVER : C_PANEL);
		draw_rectangle(ctx, x, y, card_w, 1, species == config.species ? C_BLUE : C_BORDER);
		if (species == config.species) circle(ctx, x + card_w - 9, y + 9, 3, C_BLUE);
		draw_pet_variant(species, x + 24, y + 45, 29, control_phase, 1);
		tt_set_size(font, 9); tt_draw_string(ctx, font, x + 47, y + 20, razion_companion_species[species].name, C_TEXT);
		tt_set_size(font, 7); tt_draw_string(ctx, font, x + 47, y + 36, razion_companion_species[species].voice, C_MUTED);
	}
	if (!count) {
		tt_set_size(font, 9); tt_draw_string(ctx, font, l->main_x + 8, grid_y + 29, "No companions match your search.", C_MUTED);
	}
	if (pages > 1) {
		label_button_ex(l->main_x, grid_y + 110, 23, 19, "<", 0, pet_page == 0);
		char page[32]; snprintf(page, sizeof(page), "%d of %d", pet_page + 1, pages);
		tt_set_size(font, 8); tt_draw_string(ctx, font, l->main_x + 30, grid_y + 123, page, C_MUTED);
		label_button_ex(l->main_x + 67, grid_y + 110, 23, 19, ">", 0, pet_page == pages - 1);
	}
	if (showcase_h < 130) showcase_h = 130;
	draw_showcase(l->main_x, showcase_y, l->main_w, showcase_h);
	draw_actions(l->main_x, action_y, l->main_w);
}

static void draw_field(int x, int y, int width, const char * label, const char * value, int focused) {
	tt_set_size(font, 8); tt_draw_string(ctx, font, x, y + 9, label, C_MUTED);
	draw_rounded_rectangle(ctx, x, y + 16, width, 33, 5, rgb(17,25,34));
	draw_rectangle(ctx, x, y + 16, width, 1, focused ? C_BLUE : C_BORDER);
	tt_set_size(font, 10); tt_draw_string(ctx, font, x + 10, y + 37, value, C_TEXT);
	if (focused) {
		int cursor = tt_string_width(font, value);
		draw_rectangle_solid(ctx, x + 10 + cursor, y + 23, 1, 17, C_BLUE);
	}
}

static void draw_profile_page(const companion_layout_t * l) {
	section_heading(l->main_x, l->main_y, "Pet profile", "Identity and personality");
	int top = l->main_y + 50;
	int preview_w = l->main_w < 500 ? l->main_w * 44 / 100 : l->main_w / 2;
	panel(l->main_x, top, preview_w, l->main_h - 50);
	int base = top + l->main_h - 73;
	ellipse(ctx, l->main_x + preview_w / 2, base + 2, 56, 8, rgb(8,13,19));
	draw_pet_variant(config.species, l->main_x + preview_w / 2, base, l->main_h < 520 ? 82 : 103, control_phase, 0);
	int x = l->main_x + preview_w + 12;
	int width = l->main_w - preview_w - 12;
	draw_field(x, top, width, "PET NAME", config.name, editing_name);
	label_button_ex(x, top + 58, width, 32, "Rename companion", editing_name, 0);
	draw_field(x, top + 102, width, "SPECIES", razion_companion_species[config.species].name, 0);
	draw_field(x, top + 154, width, "PERSONALITY", cycle_name(2, config.personality), 0);
	tt_set_size(font, 8); tt_draw_string(ctx, font, x, top + 220, razion_companion_species[config.species].temperament, C_MUTED);
}

static void option_row(int x, int y, int width, const char * label, const char * value) {
	tt_set_size(font, 9); tt_draw_string(ctx, font, x, y + 20, label, C_TEXT);
	int bw = width < 290 ? 122 : 150;
	label_button_ex(x + width - bw, y, bw, 31, value, 0, 0);
}

static void draw_customization_page(const companion_layout_t * l) {
	section_heading(l->main_x, l->main_y, "Customization", "Make your companion feel personal");
	int top = l->main_y + 50;
	int preview_w = l->main_w < 500 ? 132 : 190;
	panel(l->main_x, top, preview_w, l->main_h - 50);
	int base = top + l->main_h - 82;
	ellipse(ctx, l->main_x + preview_w / 2, base + 2, 53, 7, rgb(8,13,19));
	draw_pet_variant(config.species, l->main_x + preview_w / 2, base, l->main_h < 520 ? 77 : 96, control_phase, 0);
	tt_set_size(font, 11); tt_draw_string(ctx, font, l->main_x + 13, top + 24, config.name, C_TEXT);
	tt_set_size(font, 8); tt_draw_string(ctx, font, l->main_x + 13, top + 40, "Live preview", C_MUTED);
	int x = l->main_x + preview_w + 12;
	int width = l->main_w - preview_w - 12;
	option_row(x, top, width, "Color", cycle_name(0, config.color));
	option_row(x, top + 37, width, "Accessory", cycle_name(1, config.accessory));
	option_row(x, top + 74, width, "Size", cycle_name(3, config.size));
	option_row(x, top + 111, width, "Personality", cycle_name(2, config.personality));
	option_row(x, top + 148, width, "Position", cycle_name(5, config.desktop_mode));
	option_row(x, top + 185, width, "Animation speed", cycle_name(4, config.animation_speed));
	option_row(x, top + 222, width, "Idle motion", cycle_name(7, config.animation_style));
	tt_set_size(font, 8); tt_draw_string(ctx, font, x, top + 276, "More choices unlock as your bond grows.", C_MUTED);
}

static void draw_activities_page(const companion_layout_t * l) {
	section_heading(l->main_x, l->main_y, "Activities", "Care, play and build your bond");
	int top = l->main_y + 50;
	int stage_h = l->main_h - 136;
	panel(l->main_x, top, l->main_w, stage_h);
	if (reflex_active) {
		int dot_x = l->main_x + 24 + reflex_x % (l->main_w - 48);
		int dot_y = top + 28 + reflex_y % (stage_h - 56);
		circle(ctx, dot_x, dot_y, 12, C_GREEN);
		tt_set_size(font, 9); tt_draw_string(ctx, font, l->main_x + 14, top + 22, "Catch the mint dot", C_MUTED);
	} else {
		int base = top + stage_h - 20;
		ellipse(ctx, l->main_x + l->main_w / 2, base + 2, 59, 8, rgb(8,13,19));
		draw_pet_variant(config.species, l->main_x + l->main_w / 2, base, stage_h < 260 ? 81 : 106, control_phase, 0);
	}
	draw_actions(l->main_x, top + stage_h + 8, l->main_w);
	label_button_ex(l->main_x, top + stage_h + 49, l->main_w, 29,
		reflex_active ? "Activity in progress" : "Start reflex activity", reflex_active, 0);
}

static void draw_inventory_page(const companion_layout_t * l) {
	section_heading(l->main_x, l->main_y, "Inventory", "Accessories unlocked through companion levels");
	static const char * items[] = {"No accessory", "Collar", "Scarf", "Charm", "Bow", "Glasses"};
	int top = l->main_y + 54;
	int cols = l->main_w < 430 ? 2 : 3;
	int gap = 8;
	int card_w = (l->main_w - gap * (cols - 1)) / cols;
	for (int i = 0; i < 6; ++i) {
		int x = l->main_x + (i % cols) * (card_w + gap);
		int y = top + (i / cols) * 100;
		int locked = i > pet_state.level;
		draw_rounded_rectangle(ctx, x, y, card_w, 91, 6, i == config.accessory ? rgb(24,59,88) : C_PANEL);
		draw_rectangle(ctx, x, y, card_w, 1, i == config.accessory ? C_BLUE : C_BORDER);
		circle(ctx, x + card_w / 2, y + 33, 14, locked ? rgb(47,55,63) : i ? C_ORANGE : C_MUTED);
		tt_set_size(font, 9); int tw = tt_string_width(font, items[i]);
		tt_draw_string(ctx, font, x + (card_w - tw) / 2, y + 63, items[i], locked ? C_MUTED : C_TEXT);
		if (locked) { char need[24]; snprintf(need, sizeof(need), "Unlocks at level %d", i);
			tt_set_size(font, 7); tw = tt_string_width(font, need); tt_draw_string(ctx, font, x + (card_w - tw) / 2, y + 79, need, C_MUTED); }
	}
}

static void draw_achievements_page(const companion_layout_t * l) {
	section_heading(l->main_x, l->main_y, "Achievements", "A quiet record of life with your companion");
	static const char * titles[] = {"First hello", "Happy home", "Well rested", "Growing bond", "Best care", "Companion expert"};
	int unlocked[] = {1, pet_state.happiness >= 80, pet_state.energy >= 80,
		pet_state.level >= 2, pet_state.health == 100, pet_state.level >= 5};
	int top = l->main_y + 54;
	int gap = 8, width = (l->main_w - gap) / 2;
	for (int i = 0; i < 6; ++i) {
		int x = l->main_x + (i % 2) * (width + gap);
		int y = top + (i / 2) * 70;
		draw_rounded_rectangle(ctx, x, y, width, 62, 6, C_PANEL);
		circle(ctx, x + 23, y + 30, 12, unlocked[i] ? C_PURPLE : rgb(48,57,66));
		tt_set_size(font, 9); tt_draw_string(ctx, font, x + 44, y + 26, titles[i], unlocked[i] ? C_TEXT : C_MUTED);
		tt_set_size(font, 7); tt_draw_string(ctx, font, x + 44, y + 42, unlocked[i] ? "Unlocked" : "In progress", unlocked[i] ? C_BLUE : C_MUTED);
	}
}

static void setting_row(int x, int y, int width, const char * title, const char * description,
	const char * button, int selected, int disabled) {
	tt_set_size(font, 9); tt_draw_string(ctx, font, x, y + 13, title, disabled ? C_MUTED : C_TEXT);
	tt_set_size(font, 7); tt_draw_string(ctx, font, x, y + 27, description, C_MUTED);
	int bw = width < 380 ? 112 : 142;
	label_button_ex(x + width - bw, y, bw, 31, button, selected, disabled);
}

static void draw_settings_page(const companion_layout_t * l) {
	section_heading(l->main_x, l->main_y, "Settings", "Desktop behavior, sound and notifications");
	int x = l->main_x, y = l->main_y + 55, width = l->main_w;
	setting_row(x, y, width, "Desktop companion", "Run the optional desktop surface", config.enabled ? "Enabled" : "Disabled", config.enabled, 0); y += 45;
	setting_row(x, y, width, "Desktop visibility", "Temporarily hide without disabling", config.hidden ? "Hidden" : "Visible", !config.hidden, !config.enabled); y += 45;
	setting_row(x, y, width, "Window layer", "Keep the companion above applications", config.always_on_top ? "Always on top" : "Normal", config.always_on_top, !config.enabled); y += 45;
	setting_row(x, y, width, "Interaction", "Allow pointer input on the pet", config.click_through ? "Click-through" : "Interactive", !config.click_through, !config.enabled); y += 45;
	setting_row(x, y, width, "Reaction sounds", "Short local procedural sounds", config.sound ? "Sound on" : "Sound off", config.sound, 0); y += 45;
	setting_row(x, y, width, "Notifications", "Rate-limited native reminders", cycle_name(6, config.notification_frequency), config.notifications, 0);
}

static void draw_status_area(const companion_layout_t * l) {
	int x = l->main_x;
	int y = l->oy + l->height - 23;
	int width = l->right_x + l->right_w - l->main_x;
	draw_rectangle_solid(ctx, x, y - 6, width, 1, C_BORDER);
	circle(ctx, x + 4, y + 4, 3, status_until ? C_BLUE : C_GREEN);
	tt_set_size(font, 8);
	if (status_until && milliseconds() > status_until) {
		status_until = 0;
		snprintf(status_text, sizeof(status_text), "%s is %s.", config.name,
			pet_state.hunger < 35 ? "hungry" : pet_state.energy < 30 ? "sleepy" : "happy");
	}
	tt_draw_string(ctx, font, x + 14, y + 7, status_text, C_MUTED);
}

static void control_redraw(void) {
	companion_layout_t l;
	layout_get(&l);
	draw_fill(ctx, C_BACKGROUND);
	draw_navigation(&l);
	draw_header(&l);
	if (active_section == 0) draw_dashboard(&l);
	else if (active_section == 1) draw_profile_page(&l);
	else if (active_section == 2) draw_customization_page(&l);
	else if (active_section == 3) draw_activities_page(&l);
	else if (active_section == 4) draw_inventory_page(&l);
	else if (active_section == 5) draw_achievements_page(&l);
	else draw_settings_page(&l);
	draw_right_panel(&l);
	draw_status_area(&l);
	render_decorations(window, ctx, "Razion Companion");
	flip(ctx);
	yutani_flip(yctx, window);
}

static void status(const char * text) {
	snprintf(status_text, sizeof(status_text), "%s", text);
	status_until = milliseconds() + 3500;
}

static void control_action(int action) {
	const char * reaction = razion_companion_act(&config, &pet_state, action, time(NULL));
	char message[96];
	snprintf(message, sizeof(message), "%s: %s", config.name, reaction);
	status(message);
	save_state();
	char command[32];
	snprintf(command, sizeof(command), "react:%d", action);
	if (!send_control(command)) play_reaction_tone(action);
}

static void start_reflex(void) {
	reflex_active = 1;
	reflex_x = 30 + rand() % 300;
	reflex_y = 20 + rand() % 180;
	reflex_until = milliseconds() + 5000;
	status("Catch the mint dot before it moves.");
}

static void settings_changed(const char * message) {
	editing_name = 0;
	notify_overlay();
	status(message);
}

static void cycle_customization_at(int row) {
	if (row == 0) { int maximum = pet_state.level + 1; if (maximum > 7) maximum = 7; config.color = (config.color + 1) % (maximum + 1); }
	else if (row == 1) { int maximum = pet_state.level; if (maximum > 5) maximum = 5; config.accessory = (config.accessory + 1) % (maximum + 1); }
	else if (row == 2) config.size = config.size % 3 + 1;
	else if (row == 3) config.personality = (config.personality + 1) % 5;
	else if (row == 4) config.desktop_mode = (config.desktop_mode + 1) % 3;
	else if (row == 5) config.animation_speed = config.animation_speed % 3 + 1;
	else { int maximum = pet_state.level >= 3 ? 2 : pet_state.level >= 2 ? 1 : 0; config.animation_style = (config.animation_style + 1) % (maximum + 1); }
	settings_changed("Customization saved.");
}

static void click_right_panel(struct yutani_msg_window_mouse_event * mouse, const companion_layout_t * l) {
	int x = l->right_x + 14, width = l->right_w - 28;
	int y = right_quick_y(l);
	int gap = 6, bw = (width - gap) / 2;
	if (hit(x, y, bw, 29, mouse) && config.enabled) {
		config.hidden = !config.hidden;
		send_control(config.hidden ? "hide" : "show");
		settings_changed(config.hidden ? "Companion hidden from the desktop." : "Companion visible on the desktop.");
	} else if (hit(x + bw + gap, y, bw, 29, mouse)) {
		config.sound = !config.sound;
		settings_changed(config.sound ? "Reaction sounds enabled." : "Reaction sounds disabled.");
	}
}

static void control_click(struct yutani_msg_window_mouse_event * mouse) {
	companion_layout_t l;
	layout_get(&l);
	focus_x = mouse->new_x;
	focus_y = mouse->new_y;
	for (int i = 0; i < 7; ++i) {
		if (hit(l.nav_x + 7, l.nav_y + 72 + i * 40, l.nav_w - 14, 34, mouse)) {
			active_section = i; editing_name = 0; editing_search = 0; status("View changed."); return;
		}
	}
	int toggle_w = l.width < 900 ? 126 : 150;
	if (hit(l.right_x + l.right_w - toggle_w, l.oy + 12, toggle_w, 30, mouse)) {
		config.enabled = !config.enabled;
		if (!config.enabled) config.hidden = 0;
		notify_overlay();
		status(config.enabled ? "Companion enabled." : "Companion disabled; no background process remains.");
		return;
	}
	click_right_panel(mouse, &l);
	if (active_section == 0) {
		int search_w = l.main_w < 470 ? 132 : 166;
		if (hit(l.main_x + l.main_w - search_w, l.main_y + 5, search_w, 29, mouse)) { editing_search = 1; editing_name = 0; return; }
		int grid_y, cols, card_w, per_page, showcase_y, showcase_h, action_y;
		dashboard_geometry(&l, &grid_y, &cols, &card_w, &per_page, &showcase_y, &showcase_h, &action_y);
		int matches[RAZION_COMPANION_SPECIES_COUNT], count = matched_species(matches);
		for (int slot = 0; slot < per_page; ++slot) {
			int at = pet_page * per_page + slot; if (at >= count) break;
			int x = l.main_x + (slot % cols) * (card_w + 6), y = grid_y + (slot / cols) * 58;
			if (hit(x, y, card_w, 52, mouse)) { config.species = matches[at]; settings_changed("Companion species updated."); return; }
		}
		int pages = count ? (count + per_page - 1) / per_page : 1;
		if (hit(l.main_x, grid_y + 110, 23, 19, mouse) && pet_page > 0) { pet_page--; return; }
		if (hit(l.main_x + 67, grid_y + 110, 23, 19, mouse) && pet_page + 1 < pages) { pet_page++; return; }
		if (hit(l.main_x, showcase_y, l.main_w, 62, mouse)) { active_section = 1; editing_name = 1; return; }
		int gap = 6, bw = (l.main_w - gap * 4) / 5;
		for (int i = 0; i < 5; ++i) if (hit(l.main_x + i * (bw + gap), action_y, bw, 36, mouse)) { control_action(action_order[i]); return; }
	} else if (active_section == 1) {
		int top = l.main_y + 50, preview_w = l.main_w < 500 ? l.main_w * 44 / 100 : l.main_w / 2;
		int x = l.main_x + preview_w + 12, width = l.main_w - preview_w - 12;
		if (hit(x, top + 16, width, 74, mouse)) { editing_name = 1; editing_search = 0; return; }
		if (hit(x, top + 170, width, 40, mouse)) { config.personality = (config.personality + 1) % 5; settings_changed("Personality updated."); return; }
	} else if (active_section == 2) {
		int top = l.main_y + 50, preview_w = l.main_w < 500 ? 132 : 190;
		int x = l.main_x + preview_w + 12, width = l.main_w - preview_w - 12;
		int bw = width < 290 ? 122 : 150;
		for (int row = 0; row < 7; ++row) if (hit(x + width - bw, top + row * 37, bw, 31, mouse)) { cycle_customization_at(row); return; }
	} else if (active_section == 3) {
		int top = l.main_y + 50, stage_h = l.main_h - 136;
		if (reflex_active && hit(l.main_x, top, l.main_w, stage_h, mouse)) {
			int dot_x = l.main_x + 24 + reflex_x % (l.main_w - 48), dot_y = top + 28 + reflex_y % (stage_h - 56);
			int dx = mouse->new_x - dot_x, dy = mouse->new_y - dot_y;
			if (dx * dx + dy * dy < 19 * 19 && milliseconds() <= reflex_until) {
				control_action(RAZION_COMPANION_ACTION_PLAY); status("Great reflexes! Bonus play experience earned."); reflex_active = 0;
			} else status("Almost - try the mint dot again.");
			return;
		}
		int gap = 6, bw = (l.main_w - gap * 4) / 5;
		for (int i = 0; i < 5; ++i) if (hit(l.main_x + i * (bw + gap), top + stage_h + 8, bw, 36, mouse)) { control_action(action_order[i]); return; }
		if (hit(l.main_x, top + stage_h + 49, l.main_w, 29, mouse)) { start_reflex(); return; }
	} else if (active_section == 4) {
		int top = l.main_y + 54, cols = l.main_w < 430 ? 2 : 3, gap = 8;
		int card_w = (l.main_w - gap * (cols - 1)) / cols;
		for (int i = 0; i < 6; ++i) {
			int x = l.main_x + (i % cols) * (card_w + gap), y = top + (i / cols) * 100;
			if (hit(x, y, card_w, 91, mouse)) {
				if (i <= pet_state.level) { config.accessory = i; settings_changed("Accessory equipped."); }
				else status("That accessory is still locked.");
				return;
			}
		}
	} else if (active_section == 6) {
		int x = l.main_x, y = l.main_y + 55, width = l.main_w, bw = width < 380 ? 112 : 142;
		for (int row = 0; row < 6; ++row) if (hit(x + width - bw, y + row * 45, bw, 31, mouse)) {
			if (row == 0) { config.enabled = !config.enabled; if (!config.enabled) config.hidden = 0; }
			else if (row == 1 && config.enabled) config.hidden = !config.hidden;
			else if (row == 2 && config.enabled) config.always_on_top = !config.always_on_top;
			else if (row == 3 && config.enabled) config.click_through = !config.click_through;
			else if (row == 4) config.sound = !config.sound;
			else if (row == 5) { config.notification_frequency = (config.notification_frequency + 1) % 4; config.notifications = config.notification_frequency != 0; }
			settings_changed("Companion settings saved."); return;
		}
	}
}
