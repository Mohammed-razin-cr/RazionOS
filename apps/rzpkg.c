/**
 * @brief Safe command-line front end for Razion package metadata.
 */
#include <stdio.h>
#include <string.h>

#include <toaru/rzpkg.h>

static void usage(void) {
	puts("usage: rzpkg status | validate METADATA | info METADATA | install PACKAGE | update PACKAGE | remove PACKAGE");
}

static int load(const char * path, rzpkg_metadata_t * metadata) {
	rzpkg_diagnostic_t diagnostic;
	rzpkg_status_t status = rzpkg_parse_metadata_file(path, metadata, &diagnostic);
	if (status == RZPKG_VALID) return 0;
	fprintf(stderr, "rzpkg: %s", rzpkg_status_string(status));
	if (diagnostic.line) fprintf(stderr, " at line %u", diagnostic.line);
	if (diagnostic.field[0]) fprintf(stderr, " (%s)", diagnostic.field);
	fprintf(stderr, ": %s\n", diagnostic.message);
	return 1;
}

int main(int argc, char * argv[]) {
	if (argc == 2 && !strcmp(argv[1], "status")) {
		if (rzpkg_repository_available(NULL)) {
			puts("rzpkg: signed repository transaction service available");
			return 0;
		}
		puts("rzpkg: metadata validation available; signed repository transaction service is not configured");
		return 2;
	}
	if (argc == 3 && (!strcmp(argv[1], "validate") || !strcmp(argv[1], "info"))) {
		rzpkg_metadata_t metadata;
		if (load(argv[2], &metadata)) return 1;
		if (!strcmp(argv[1], "validate")) {
			puts("valid");
			return 0;
		}
		printf("Application: %s\nVersion: %s\nArchitecture: %s\nSize: %llu bytes\n"
			"License: %s\nMaintainer: %s\nDependencies: %s\nPermissions: %s\nChecksum: %s\nSignature: %s\n",
			metadata.name, metadata.version, metadata.architecture,
			(unsigned long long)metadata.installed_size, metadata.license,
			metadata.maintainer, metadata.dependencies[0] ? metadata.dependencies : "none",
			metadata.permissions[0] ? metadata.permissions : "none", metadata.checksum,
			metadata.signature[0] ? "present (verification service required)" : "not present");
		return 0;
	}
	if (argc == 3 && (!strcmp(argv[1], "install") || !strcmp(argv[1], "update") || !strcmp(argv[1], "remove"))) {
		fprintf(stderr, "rzpkg: refusing %s for '%s': signed repository and privileged transaction service are not configured\n",
			argv[1], argv[2]);
		return 69;
	}
	usage();
	return argc == 2 && !strcmp(argv[1], "--help") ? 0 : 2;
}
