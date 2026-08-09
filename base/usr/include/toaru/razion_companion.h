#pragma once

#include <_cheader.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

_Begin_C_Header

#define RAZION_COMPANION_SPECIES_COUNT 22
#define RAZION_COMPANION_NAME_MAX 32
#define RAZION_COMPANION_PATH_MAX 512

typedef enum {
	RAZION_COMPANION_ACTION_FEED = 0,
	RAZION_COMPANION_ACTION_PET,
	RAZION_COMPANION_ACTION_PLAY,
	RAZION_COMPANION_ACTION_SLEEP,
	RAZION_COMPANION_ACTION_TALK,
	RAZION_COMPANION_ACTION_COUNT,
} razion_companion_action_t;

typedef enum {
	RAZION_COMPANION_NOTICE_NONE = 0,
	RAZION_COMPANION_NOTICE_HUNGRY,
	RAZION_COMPANION_NOTICE_PLAY,
	RAZION_COMPANION_NOTICE_SLEEPY,
	RAZION_COMPANION_NOTICE_HAPPY,
	RAZION_COMPANION_NOTICE_UNWELL,
} razion_companion_notice_t;

typedef struct {
	const char * id;
	const char * name;
	const char * voice;
	const char * temperament;
	uint32_t body_color;
	uint32_t accent_color;
	uint8_t shape;
	uint8_t energy_bias;
	uint8_t social_bias;
} razion_companion_species_t;

typedef struct {
	int enabled;
	int hidden;
	int species;
	char name[RAZION_COMPANION_NAME_MAX];
	int color;
	int accessory;
	int personality;
	int size;
	int position_x;
	int position_y;
	int animation_style;
	int animation_speed;
	int desktop_mode;
	int always_on_top;
	int click_through;
	int sound;
	int notifications;
	int notification_frequency;
} razion_companion_config_t;

typedef struct {
	int happiness;
	int hunger;
	int energy;
	int health;
	int level;
	int experience;
	time_t last_update;
	time_t last_interaction;
	time_t last_notification;
} razion_companion_state_t;

extern const razion_companion_species_t razion_companion_species[
	RAZION_COMPANION_SPECIES_COUNT];

extern void razion_companion_defaults(
	razion_companion_config_t * config,
	razion_companion_state_t * state,
	time_t now);

extern void razion_companion_sanitize(
	razion_companion_config_t * config,
	razion_companion_state_t * state);

/** Advance needs from elapsed wall time; returns non-zero when state changed. */
extern int razion_companion_tick(
	razion_companion_state_t * state,
	time_t now);

/** Apply a local interaction and return the companion's short reaction text. */
extern const char * razion_companion_act(
	razion_companion_config_t * config,
	razion_companion_state_t * state,
	razion_companion_action_t action,
	time_t now);

/** Select a rate-limited notice, updating last_notification when one is due. */
extern razion_companion_notice_t razion_companion_notice(
	const razion_companion_config_t * config,
	razion_companion_state_t * state,
	time_t now);

extern const char * razion_companion_notice_text(
	razion_companion_notice_t notice);

extern int razion_companion_load(
	const char * path,
	razion_companion_config_t * config,
	razion_companion_state_t * state,
	time_t now);

extern int razion_companion_save(
	const char * path,
	const razion_companion_config_t * config,
	const razion_companion_state_t * state);

/** Path reserved for a future controlled AI/automation command endpoint. */
#define RAZION_COMPANION_CONTROL_ENDPOINT "razion-companion"

_End_C_Header
