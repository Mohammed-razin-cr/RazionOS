/**
 * @brief Offline state and persistence engine for Razion Companion.
 *
 * This module has no display, audio, network, or AI dependencies. Desktop and
 * automation clients use this bounded API rather than modifying state files.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <toaru/razion_companion.h>

#define RGB24(r,g,b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))
#define STATE_VERSION 1

/* Shapes describe original procedural silhouettes rendered by the desktop app. */
const razion_companion_species_t razion_companion_species[
	RAZION_COMPANION_SPECIES_COUNT] = {
	{"cat",       "Cat",       "Meow!",   "Independent & playful", RGB24(188,197,215), RGB24(92,157,255), 0, 3, 2},
	{"dog",       "Dog",       "Woof!",   "Friendly & energetic",  RGB24(193,132,72),  RGB24(248,205,117),1, 5, 5},
	{"lion",      "Lion",      "Roar!",   "Calm & confident",      RGB24(217,150,55),  RGB24(116,68,35),  2, 3, 2},
	{"tiger",     "Tiger",     "Rrrrow!", "Bold & curious",        RGB24(226,132,39),  RGB24(46,37,38),   0, 4, 3},
	{"wolf",      "Wolf",      "Awooo!",  "Loyal & observant",     RGB24(126,139,156), RGB24(224,233,240),1, 4, 4},
	{"fox",       "Fox",       "Yip!",    "Clever & lively",       RGB24(224,102,42),  RGB24(247,232,209),1, 5, 3},
	{"rabbit",    "Rabbit",    "Squeak!", "Gentle & alert",        RGB24(220,214,229), RGB24(243,151,182),3, 4, 3},
	{"panda",     "Panda",     "Mrrp!",   "Relaxed & friendly",    RGB24(235,239,238), RGB24(42,48,52),   4, 2, 4},
	{"bear",      "Bear",      "Hrrm!",   "Patient & warm",        RGB24(137,91,58),   RGB24(202,155,99), 4, 2, 3},
	{"penguin",   "Penguin",   "Chirp!",  "Social & determined",  RGB24(37,49,61),    RGB24(244,245,238),5, 3, 4},
	{"eagle",     "Eagle",     "Kree!",   "Focused & proud",       RGB24(105,69,46),   RGB24(235,226,204),6, 4, 2},
	{"owl",       "Owl",       "Hoot!",   "Quiet & thoughtful",    RGB24(139,103,72),  RGB24(224,189,117),6, 2, 2},
	{"parrot",    "Parrot",    "Tweet!",  "Chatty & bright",       RGB24(48,181,124),  RGB24(244,86,74),  6, 4, 5},
	{"snake",     "Snake",     "Hiss!",   "Quiet & calm",          RGB24(81,175,102),  RGB24(219,199,74), 7, 2, 1},
	{"turtle",    "Turtle",    "Prrt!",   "Steady & peaceful",     RGB24(67,151,91),   RGB24(113,95,58),  8, 1, 2},
	{"frog",      "Frog",      "Ribbit!", "Cheerful & springy",    RGB24(79,196,92),   RGB24(213,235,85), 9, 4, 3},
	{"dragon",    "Dragon",    "Grr-roar!","Playful & protective", RGB24(79,125,211),  RGB24(111,225,193),10,5, 3},
	{"dinosaur",  "Dinosaur",  "Rumble!", "Curious & sturdy",      RGB24(93,172,103),  RGB24(230,174,70), 10,3, 3},
	{"unicorn",   "Unicorn",   "Nyee!",   "Gentle & imaginative",  RGB24(229,226,245), RGB24(180,116,234),11,4, 4},
	{"horse",     "Horse",     "Neigh!",  "Calm & adventurous",    RGB24(154,100,65),  RGB24(225,194,145),11,4, 3},
	{"monkey",    "Monkey",    "Ooh-ooh!","Mischievous & social",  RGB24(151,99,58),   RGB24(225,176,119),12,5, 5},
	{"red-panda", "Red Panda", "Mewp!",   "Curious & cozy",        RGB24(184,73,44),   RGB24(244,194,126),4, 3, 3},
};

static int clamp(int value, int minimum, int maximum) {
	if (value < minimum) return minimum;
	if (value > maximum) return maximum;
	return value;
}

static void copy_name(char * destination, const char * source) {
	if (!source) source = "Nova";
	size_t length = strlen(source);
	if (length >= RAZION_COMPANION_NAME_MAX) {
		length = RAZION_COMPANION_NAME_MAX - 1;
	}
	memcpy(destination, source, length);
	destination[length] = '\0';
}

void razion_companion_defaults(
	razion_companion_config_t * config,
	razion_companion_state_t * state,
	time_t now) {
	memset(config, 0, sizeof(*config));
	memset(state, 0, sizeof(*state));
	config->enabled = 0;
	config->hidden = 0;
	config->species = 0;
	copy_name(config->name, "Nova");
	config->size = 2;
	config->position_x = 820;
	config->position_y = 560;
	config->animation_style = 0;
	config->animation_speed = 2;
	config->desktop_mode = 1;
	config->always_on_top = 0;
	config->sound = 1;
	config->notifications = 1;
	config->notification_frequency = 2;
	state->happiness = 82;
	state->hunger = 76;
	state->energy = 88;
	state->health = 100;
	state->level = 1;
	state->last_update = now;
	state->last_interaction = now;
}

void razion_companion_sanitize(
	razion_companion_config_t * config,
	razion_companion_state_t * state) {
	config->enabled = !!config->enabled;
	config->hidden = !!config->hidden;
	config->species = clamp(config->species, 0, RAZION_COMPANION_SPECIES_COUNT - 1);
	if (!config->name[0]) copy_name(config->name, "Nova");
	config->name[RAZION_COMPANION_NAME_MAX - 1] = '\0';
	for (char * c = config->name; *c; ++c) {
		if ((unsigned char)*c < 0x20 || strchr("=\\\"<>&", *c)) *c = '_';
	}
	config->color = clamp(config->color, 0, 7);
	config->accessory = clamp(config->accessory, 0, 5);
	config->personality = clamp(config->personality, 0, 4);
	config->size = clamp(config->size, 1, 3);
	config->position_x = clamp(config->position_x, 0, 16384);
	config->position_y = clamp(config->position_y, 28, 16384);
	config->animation_style = clamp(config->animation_style, 0, 2);
	config->animation_speed = clamp(config->animation_speed, 1, 3);
	config->desktop_mode = clamp(config->desktop_mode, 0, 2);
	config->always_on_top = !!config->always_on_top;
	config->click_through = !!config->click_through;
	config->sound = !!config->sound;
	config->notifications = !!config->notifications;
	config->notification_frequency = clamp(config->notification_frequency, 0, 3);
	state->happiness = clamp(state->happiness, 0, 100);
	state->hunger = clamp(state->hunger, 0, 100);
	state->energy = clamp(state->energy, 0, 100);
	state->health = clamp(state->health, 0, 100);
	state->level = clamp(state->level, 1, 99);
	state->experience = clamp(state->experience, 0, 999999);
}

static void add_experience(razion_companion_state_t * state, int amount) {
	state->experience += amount;
	while (state->level < 99) {
		int threshold = state->level * 100;
		if (state->experience < threshold) break;
		state->experience -= threshold;
		state->level++;
	}
}

int razion_companion_tick(razion_companion_state_t * state, time_t now) {
	if (!state->last_update || now <= state->last_update) {
		state->last_update = now;
		return 0;
	}
	time_t elapsed = now - state->last_update;
	/* Bound catch-up to one week so a long absence never punishes the user. */
	if (elapsed > 7 * 24 * 60 * 60) elapsed = 7 * 24 * 60 * 60;
	int minutes = (int)(elapsed / 60);
	if (!minutes) return 0;
	int old_happiness = state->happiness;
	int old_hunger = state->hunger;
	int old_energy = state->energy;
	int old_health = state->health;
	state->hunger -= minutes / 12;
	state->energy -= minutes / 15;
	state->happiness -= minutes / 20;
	if (state->hunger < 15 || state->energy < 10) state->health -= minutes / 30;
	else if (state->health < 100 && state->hunger > 45 && state->energy > 35) {
		state->health += minutes / 45;
	}
	razion_companion_config_t dummy;
	memset(&dummy, 0, sizeof(dummy));
	razion_companion_sanitize(&dummy, state);
	state->last_update += (time_t)minutes * 60;
	return old_happiness != state->happiness || old_hunger != state->hunger ||
		old_energy != state->energy || old_health != state->health;
}

const char * razion_companion_act(
	razion_companion_config_t * config,
	razion_companion_state_t * state,
	razion_companion_action_t action,
	time_t now) {
	razion_companion_tick(state, now);
	int xp = 0;
	const char * reaction = "Hello!";
	const razion_companion_species_t * species =
		&razion_companion_species[config->species];
	int social = species->social_bias + (config->personality == 1 ? 1 : 0);
	int energy = species->energy_bias + (config->personality == 2 ? -1 : 0);
	int daily_bonus = state->last_interaction &&
		state->last_interaction / (24 * 60 * 60) != now / (24 * 60 * 60);
	switch (action) {
		case RAZION_COMPANION_ACTION_FEED:
			state->hunger += 24; state->health += 3; state->happiness += 3;
			xp = 12; reaction = "That was delicious!"; break;
		case RAZION_COMPANION_ACTION_PET:
			if (social <= 2 && ((now / 60 + config->species) % 5 == 0)) {
				state->happiness += 2; xp = 2; reaction = "Maybe in a moment...";
			} else {
				state->happiness += 9 + social; xp = 8; reaction = "That feels nice!";
			}
			break;
		case RAZION_COMPANION_ACTION_PLAY:
			if (state->energy >= 7 + energy) {
				state->energy -= 7 + energy; state->hunger -= 3 + energy / 2;
				state->happiness += 13 + energy;
				xp = 18; reaction = "Let's play!";
			} else reaction = "I need a little rest.";
			break;
		case RAZION_COMPANION_ACTION_SLEEP:
			state->energy += 36 - energy; state->health += 4; xp = 6;
			reaction = "Good night..."; break;
		case RAZION_COMPANION_ACTION_TALK:
			state->happiness += 2 + social; xp = 3;
			reaction = razion_companion_species[config->species].voice; break;
		default: break;
	}
	state->last_interaction = now;
	add_experience(state, xp + (daily_bonus ? 20 : 0));
	razion_companion_sanitize(config, state);
	return reaction;
}

razion_companion_notice_t razion_companion_notice(
	const razion_companion_config_t * config,
	razion_companion_state_t * state,
	time_t now) {
	if (!config->notifications || !config->notification_frequency) return RAZION_COMPANION_NOTICE_NONE;
	static const time_t intervals[] = {0, 6 * 60 * 60, 3 * 60 * 60, 60 * 60};
	if (state->last_notification &&
		now - state->last_notification < intervals[config->notification_frequency]) {
		return RAZION_COMPANION_NOTICE_NONE;
	}
	razion_companion_notice_t notice = RAZION_COMPANION_NOTICE_NONE;
	if (state->health < 35) notice = RAZION_COMPANION_NOTICE_UNWELL;
	else if (state->hunger < 28) notice = RAZION_COMPANION_NOTICE_HUNGRY;
	else if (state->energy < 22) notice = RAZION_COMPANION_NOTICE_SLEEPY;
	else if (state->happiness < 30) notice = RAZION_COMPANION_NOTICE_PLAY;
	else if (state->happiness > 92 && now - state->last_interaction < 10 * 60) {
		notice = RAZION_COMPANION_NOTICE_HAPPY;
	}
	if (notice != RAZION_COMPANION_NOTICE_NONE) state->last_notification = now;
	return notice;
}

const char * razion_companion_notice_text(razion_companion_notice_t notice) {
	switch (notice) {
		case RAZION_COMPANION_NOTICE_HUNGRY: return "is getting hungry.";
		case RAZION_COMPANION_NOTICE_PLAY: return "would enjoy some company.";
		case RAZION_COMPANION_NOTICE_SLEEPY: return "is ready for a rest.";
		case RAZION_COMPANION_NOTICE_HAPPY: return "is happy to see you.";
		case RAZION_COMPANION_NOTICE_UNWELL: return "needs a little care.";
		default: return "is quietly exploring.";
	}
}

static void parse_value(
	const char * key, const char * value,
	razion_companion_config_t * config,
	razion_companion_state_t * state) {
#define INTEGER_FIELD(name, field) if (!strcmp(key, name)) { field = atoi(value); return; }
	INTEGER_FIELD("enabled", config->enabled)
	INTEGER_FIELD("hidden", config->hidden)
	INTEGER_FIELD("species", config->species)
	if (!strcmp(key, "name")) { copy_name(config->name, value); return; }
	INTEGER_FIELD("color", config->color)
	INTEGER_FIELD("accessory", config->accessory)
	INTEGER_FIELD("personality", config->personality)
	INTEGER_FIELD("size", config->size)
	INTEGER_FIELD("position_x", config->position_x)
	INTEGER_FIELD("position_y", config->position_y)
	INTEGER_FIELD("animation_style", config->animation_style)
	INTEGER_FIELD("animation_speed", config->animation_speed)
	INTEGER_FIELD("desktop_mode", config->desktop_mode)
	INTEGER_FIELD("always_on_top", config->always_on_top)
	INTEGER_FIELD("click_through", config->click_through)
	INTEGER_FIELD("sound", config->sound)
	INTEGER_FIELD("notifications", config->notifications)
	INTEGER_FIELD("notification_frequency", config->notification_frequency)
	INTEGER_FIELD("happiness", state->happiness)
	INTEGER_FIELD("hunger", state->hunger)
	INTEGER_FIELD("energy", state->energy)
	INTEGER_FIELD("health", state->health)
	INTEGER_FIELD("level", state->level)
	INTEGER_FIELD("experience", state->experience)
	if (!strcmp(key, "last_update")) state->last_update = (time_t)strtoll(value, NULL, 10);
	else if (!strcmp(key, "last_interaction")) state->last_interaction = (time_t)strtoll(value, NULL, 10);
	else if (!strcmp(key, "last_notification")) state->last_notification = (time_t)strtoll(value, NULL, 10);
#undef INTEGER_FIELD
}

int razion_companion_load(
	const char * path,
	razion_companion_config_t * config,
	razion_companion_state_t * state,
	time_t now) {
	if (!path || !config || !state) { errno = EINVAL; return -1; }
	razion_companion_defaults(config, state, now);
	FILE * file = fopen(path, "r");
	if (!file) return errno == ENOENT ? 0 : -1;
	char line[256];
	while (fgets(line, sizeof(line), file)) {
		char * end = strchr(line, '\n');
		if (end) *end = '\0';
		char * equals = strchr(line, '=');
		if (!equals) continue;
		*equals = '\0';
		parse_value(line, equals + 1, config, state);
	}
	int result = ferror(file) ? -1 : 0;
	fclose(file);
	razion_companion_sanitize(config, state);
	return result;
}

int razion_companion_save(
	const char * path,
	const razion_companion_config_t * config,
	const razion_companion_state_t * state) {
	if (!path || !config || !state) { errno = EINVAL; return -1; }
	razion_companion_config_t safe_config = *config;
	razion_companion_state_t safe_state = *state;
	razion_companion_sanitize(&safe_config, &safe_state);
	config = &safe_config;
	state = &safe_state;
	FILE * file = fopen(path, "w");
	if (!file) return -1;
	fprintf(file,
		"version=%d\nenabled=%d\nhidden=%d\nspecies=%d\nname=%s\ncolor=%d\n"
		"accessory=%d\npersonality=%d\nsize=%d\nposition_x=%d\nposition_y=%d\n"
		"animation_style=%d\nanimation_speed=%d\ndesktop_mode=%d\n"
		"always_on_top=%d\nclick_through=%d\nsound=%d\nnotifications=%d\n"
		"notification_frequency=%d\nhappiness=%d\nhunger=%d\nenergy=%d\nhealth=%d\n"
		"level=%d\nexperience=%d\nlast_update=%lld\nlast_interaction=%lld\n"
		"last_notification=%lld\n",
		STATE_VERSION, config->enabled, config->hidden, config->species, config->name, config->color,
		config->accessory, config->personality, config->size, config->position_x,
		config->position_y, config->animation_style, config->animation_speed,
		config->desktop_mode, config->always_on_top, config->click_through,
		config->sound, config->notifications, config->notification_frequency,
		state->happiness, state->hunger, state->energy, state->health,
		state->level, state->experience, (long long)state->last_update,
		(long long)state->last_interaction, (long long)state->last_notification);
	int result = ferror(file) ? -1 : 0;
	if (fclose(file)) result = -1;
	return result;
}
