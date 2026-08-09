#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <toaru/razion_companion.h>

int main(void) {
	assert(RAZION_COMPANION_SPECIES_COUNT >= 22);
	for (size_t i = 0; i < RAZION_COMPANION_SPECIES_COUNT; ++i) {
		assert(razion_companion_species[i].id[0]);
		assert(razion_companion_species[i].name[0]);
		for (size_t j = i + 1; j < RAZION_COMPANION_SPECIES_COUNT; ++j) {
			assert(strcmp(razion_companion_species[i].id,
				razion_companion_species[j].id));
		}
	}

	time_t start = 1000000;
	razion_companion_config_t config;
	razion_companion_state_t state;
	razion_companion_defaults(&config, &state, start);
	assert(!config.enabled);
	assert(!strcmp(config.name, "Nova"));
	assert(state.health == 100 && state.level == 1);

	assert(razion_companion_tick(&state, start + 24 * 60 * 60));
	assert(state.hunger < 76);
	assert(state.energy < 88);
	int hunger = state.hunger;
	const char * reaction = razion_companion_act(&config, &state,
		RAZION_COMPANION_ACTION_FEED, start + 24 * 60 * 60);
	assert(reaction && state.hunger > hunger && state.experience > 0);
	int experience = state.experience;
	razion_companion_act(&config, &state, RAZION_COMPANION_ACTION_TALK,
		start + 2 * 24 * 60 * 60);
	assert(state.experience >= experience + 20); /* Daily interaction bonus. */

	state.hunger = 10;
	state.health = 100;
	state.energy = 100;
	state.happiness = 50;
	state.last_notification = 0;
	config.notification_frequency = 3;
	assert(razion_companion_notice(&config, &state, start) ==
		RAZION_COMPANION_NOTICE_HUNGRY);
	assert(razion_companion_notice(&config, &state, start + 30) ==
		RAZION_COMPANION_NOTICE_NONE);

	char path[128];
	snprintf(path, sizeof(path), "/tmp/razion-companion-%d.conf", getpid());
	config.enabled = 1;
	config.species = 16;
	strcpy(config.name, "Comet");
	assert(!razion_companion_save(path, &config, &state));
	razion_companion_config_t loaded_config;
	razion_companion_state_t loaded_state;
	assert(!razion_companion_load(path, &loaded_config, &loaded_state, start));
	assert(loaded_config.enabled && loaded_config.species == 16);
	assert(!strcmp(loaded_config.name, "Comet"));
	assert(loaded_state.hunger == state.hunger);
	strcpy(config.name, "bad=name");
	assert(!razion_companion_save(path, &config, &state));
	assert(!razion_companion_load(path, &loaded_config, &loaded_state, start));
	assert(!strchr(loaded_config.name, '='));
	assert(!unlink(path));

	puts("razion-companion: all tests passed");
	return 0;
}
