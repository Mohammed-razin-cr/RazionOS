#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <toaru/rzpkg.h>

static void valid_metadata(rzpkg_metadata_t * metadata) {
	rzpkg_metadata_init(metadata);
	strcpy(metadata->name, "razion-example");
	strcpy(metadata->version, "1.2.3");
	strcpy(metadata->architecture, "x86_64");
	strcpy(metadata->description, "Example package");
	strcpy(metadata->dependencies, "libc>=1.0, libgraphics");
	strcpy(metadata->permissions, "network, user-files:read");
	strcpy(metadata->maintainer, "RazionOS Project");
	strcpy(metadata->license, "NCSA");
	strcpy(metadata->checksum, "sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
	metadata->installed_size = 4096;
}

int main(void) {
	rzpkg_metadata_t metadata;
	rzpkg_diagnostic_t diagnostic;
	valid_metadata(&metadata);
	assert(rzpkg_validate_metadata(&metadata, &diagnostic) == RZPKG_VALID);
	strcpy(metadata.name, "bad name;rm");
	assert(rzpkg_validate_metadata(&metadata, &diagnostic) == RZPKG_INVALID_NAME);
	valid_metadata(&metadata);
	strcpy(metadata.checksum, "sha256:not-a-digest");
	assert(rzpkg_validate_metadata(&metadata, &diagnostic) == RZPKG_INVALID_CHECKSUM);
	assert(!rzpkg_repository_available("/definitely/not/configured"));
	puts("rzpkg: all tests passed");
	return 0;
}
