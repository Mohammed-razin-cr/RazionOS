/**
 * @brief Fail-closed package metadata parser for the future rzpkg service.
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <toaru/rzpkg.h>

static void copy_text(char * output, size_t size, const char * input) {
	if (!size) return;
	if (!input) input = "";
	size_t length = strlen(input);
	if (length >= size) length = size - 1;
	memcpy(output, input, length);
	output[length] = '\0';
}

static void diagnose(rzpkg_diagnostic_t * diagnostic, rzpkg_status_t status,
	unsigned line, const char * field, const char * message) {
	if (!diagnostic) return;
	diagnostic->status = status;
	diagnostic->line = line;
	copy_text(diagnostic->field, sizeof(diagnostic->field), field);
	copy_text(diagnostic->message, sizeof(diagnostic->message), message);
}

void rzpkg_metadata_init(rzpkg_metadata_t * metadata) {
	if (metadata) memset(metadata, 0, sizeof(*metadata));
}

static int safe_identifier(const char * value, int version) {
	if (!value || !*value) return 0;
	for (const unsigned char * c = (const unsigned char *)value; *c; ++c) {
		if (isalnum(*c) || *c == '-' || *c == '_' || *c == '.' ||
			(version && (*c == '+' || *c == '~'))) continue;
		return 0;
	}
	return 1;
}

static int safe_list(const char * value) {
	if (!value) return 0;
	for (const unsigned char * c = (const unsigned char *)value; *c; ++c) {
		if (isalnum(*c) || strchr("-_.:, /<>=+~", *c)) continue;
		return 0;
	}
	return 1;
}

static int checksum_valid(const char * value) {
	if (strncmp(value, "sha256:", 7)) return 0;
	if (strlen(value + 7) != 64) return 0;
	for (const char * c = value + 7; *c; ++c) if (!isxdigit((unsigned char)*c)) return 0;
	return 1;
}

rzpkg_status_t rzpkg_validate_metadata(const rzpkg_metadata_t * metadata,
	rzpkg_diagnostic_t * diagnostic) {
	if (!metadata) {
		diagnose(diagnostic, RZPKG_INVALID_ARGUMENT, 0, "", "metadata is required");
		return RZPKG_INVALID_ARGUMENT;
	}
#define REQUIRE(check, code, field_name, text) do { if (!(check)) { \
	diagnose(diagnostic, code, 0, field_name, text); return code; } } while (0)
	REQUIRE(safe_identifier(metadata->name, 0), RZPKG_INVALID_NAME,
		"name", "use letters, digits, dot, dash, or underscore");
	REQUIRE(safe_identifier(metadata->version, 1), RZPKG_INVALID_VERSION,
		"version", "version is empty or contains unsafe characters");
	REQUIRE(!strcmp(metadata->architecture, "x86_64") ||
		!strcmp(metadata->architecture, "noarch"), RZPKG_INVALID_ARCHITECTURE,
		"architecture", "supported values are x86_64 and noarch");
	REQUIRE(metadata->description[0], RZPKG_INVALID_DESCRIPTION,
		"description", "description is required");
	REQUIRE(metadata->installed_size > 0, RZPKG_INVALID_SIZE,
		"size", "installed size must be greater than zero");
	REQUIRE(metadata->maintainer[0], RZPKG_INVALID_MAINTAINER,
		"maintainer", "maintainer is required");
	REQUIRE(metadata->license[0], RZPKG_INVALID_LICENSE,
		"license", "license is required");
	REQUIRE(checksum_valid(metadata->checksum), RZPKG_INVALID_CHECKSUM,
		"checksum", "checksum must be sha256 followed by 64 hexadecimal digits");
	REQUIRE(safe_list(metadata->dependencies) && safe_list(metadata->permissions),
		RZPKG_INVALID_LIST, "dependencies/permissions",
		"lists contain unsupported characters");
#undef REQUIRE
	diagnose(diagnostic, RZPKG_VALID, 0, "", "metadata is valid");
	return RZPKG_VALID;
}

static char * trim(char * value) {
	while (isspace((unsigned char)*value)) value++;
	char * end = value + strlen(value);
	while (end > value && isspace((unsigned char)end[-1])) *--end = '\0';
	return value;
}

static int set_field(rzpkg_metadata_t * metadata, const char * key, const char * value) {
#define SET(name, member) if (!strcmp(key, name)) { copy_text(metadata->member, sizeof(metadata->member), value); return 1; }
	SET("name", name)
	SET("version", version)
	SET("architecture", architecture)
	SET("description", description)
	SET("dependencies", dependencies)
	SET("permissions", permissions)
	SET("maintainer", maintainer)
	SET("license", license)
	SET("checksum", checksum)
	SET("signature", signature)
#undef SET
	if (!strcmp(key, "size")) {
		char * end = NULL;
		errno = 0;
		unsigned long long size = strtoull(value, &end, 10);
		if (errno || !end || *trim(end)) return -1;
		metadata->installed_size = size;
		return 1;
	}
	return 0;
}

rzpkg_status_t rzpkg_parse_metadata_file(const char * path,
	rzpkg_metadata_t * metadata, rzpkg_diagnostic_t * diagnostic) {
	if (!path || !metadata) {
		diagnose(diagnostic, RZPKG_INVALID_ARGUMENT, 0, "", "path and output are required");
		return RZPKG_INVALID_ARGUMENT;
	}
	FILE * input = fopen(path, "r");
	if (!input) {
		diagnose(diagnostic, RZPKG_METADATA_IO_ERROR, 0, "", strerror(errno));
		return RZPKG_METADATA_IO_ERROR;
	}
	rzpkg_metadata_init(metadata);
	char line[768];
	unsigned number = 0;
	while (fgets(line, sizeof(line), input)) {
		number++;
		char * text = trim(line);
		if (!*text || *text == '#') continue;
		char * equals = strchr(text, '=');
		if (!equals) {
			fclose(input);
			diagnose(diagnostic, RZPKG_METADATA_SYNTAX_ERROR, number, "", "expected key=value");
			return RZPKG_METADATA_SYNTAX_ERROR;
		}
		*equals = '\0';
		char * key = trim(text);
		char * value = trim(equals + 1);
		int result = set_field(metadata, key, value);
		if (result <= 0) {
			fclose(input);
			diagnose(diagnostic, RZPKG_METADATA_SYNTAX_ERROR, number, key,
				result < 0 ? "invalid numeric value" : "unknown metadata field");
			return RZPKG_METADATA_SYNTAX_ERROR;
		}
	}
	fclose(input);
	rzpkg_status_t status = rzpkg_validate_metadata(metadata, diagnostic);
	if (diagnostic && status != RZPKG_VALID) diagnostic->line = number;
	return status;
}

int rzpkg_repository_available(const char * configuration_path) {
	if (!configuration_path) configuration_path = "/var/rzpkg/repository.conf";
	FILE * input = fopen(configuration_path, "r");
	if (!input) return 0;
	int signed_index = 0, verifier = 0, transaction_service = 0;
	char line[128];
	while (fgets(line, sizeof(line), input)) {
		if (!strncmp(line, "signed-index=enabled", 20)) signed_index = 1;
		else if (!strncmp(line, "signature-verifier=enabled", 26)) verifier = 1;
		else if (!strncmp(line, "transaction-service=enabled", 27)) transaction_service = 1;
	}
	fclose(input);
	return signed_index && verifier && transaction_service;
}

const char * rzpkg_status_string(rzpkg_status_t status) {
	switch (status) {
		case RZPKG_VALID: return "valid";
		case RZPKG_INVALID_ARGUMENT: return "invalid argument";
		case RZPKG_INVALID_NAME: return "invalid package name";
		case RZPKG_INVALID_VERSION: return "invalid version";
		case RZPKG_INVALID_ARCHITECTURE: return "invalid architecture";
		case RZPKG_INVALID_DESCRIPTION: return "missing description";
		case RZPKG_INVALID_SIZE: return "invalid installed size";
		case RZPKG_INVALID_MAINTAINER: return "missing maintainer";
		case RZPKG_INVALID_LICENSE: return "missing license";
		case RZPKG_INVALID_CHECKSUM: return "invalid checksum";
		case RZPKG_INVALID_LIST: return "invalid metadata list";
		case RZPKG_METADATA_IO_ERROR: return "metadata I/O error";
		case RZPKG_METADATA_SYNTAX_ERROR: return "metadata syntax error";
		case RZPKG_BACKEND_UNAVAILABLE: return "signed package backend unavailable";
		default: return "unknown status";
	}
}
