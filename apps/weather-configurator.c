/**
 * @brief Configure the weather information 'daemon'
 *
 * Messes with /etc/weather.json so it needs root for now...
 *
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2018-2021 K. Lange
 */
#include <stdio.h>
#include <string.h>
#include <toaru/json.h>
#include <toaru/hashmap.h>
#include <toaru/list.h>

typedef struct JSON_Value Value;

int main(int argc, char * argv[]) {

	Value * config = json_parse_file("/etc/weather.json");
	if (config) {
		char * city = JSON_KEY(config, "city")->string;
		char * key = JSON_KEY(config, "key")->string;
		char * __comment = JSON_KEY(config, "--comment")->string;
		char * units = JSON_KEY(config, "units")->string;

		fprintf(stdout, "City? [%s] ", city);
		fflush(stdout);

		char ncity[100];
		fgets(ncity, 100, stdin);

		if (ncity[0] != '\n') {
			char * n = strstr(ncity, "\n");
			if (n) *n = '\0';
			city = ncity;
		}

		fprintf(stdout, "Units? [%s] ", units);
		fflush(stdout);

		char nunits[100];
		fgets(nunits, 100, stdin);

		if (nunits[0] != '\n') {
			char * n = strstr(nunits, "\n");
			if (n) *n = '\0';
			units = nunits;
		}

		fprintf(stdout, "OpenWeatherMap API key? [%s] ",
			key && *key ? "leave blank to keep current" : "required");
		fflush(stdout);

		char nkey[100];
		fgets(nkey, 100, stdin);

		if (nkey[0] != '\n') {
			char * n = strstr(nkey, "\n");
			if (n) *n = '\0';
			key = nkey;
		}

		if (!key || !*key) {
			fprintf(stderr, "No API key provided; weather will remain disabled.\n");
		}

		FILE * f = fopen("/etc/weather.json", "w");
		fprintf(f, "{\n");
		fprintf(f, "    \"city\": \"%s\",\n", city);
		fprintf(f, "    \"units\": \"%s\",\n", units);
		fprintf(f, "\n");
		fprintf(f, "    \"--comment\": \"%s\",\n", __comment);
		fprintf(f, "    \"key\": \"%s\"\n", key);
		fprintf(f, "}\n");
		fclose(f);

	} else {
		fprintf(stderr, "Configuration is not set. Please create /etc/weather.json manually.\n");
		fprintf(stderr, "(Press ENTER to exit.)\n");
		getchar();
		return 0;
	}
}
