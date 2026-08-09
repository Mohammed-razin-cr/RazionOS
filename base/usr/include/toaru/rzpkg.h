/**
 * @brief Razion package metadata and repository safety interface.
 *
 * This API deliberately separates metadata validation from installation.
 * Installation is unavailable until a signed repository and privileged,
 * policy-enforcing transaction service are present.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <_cheader.h>

_Begin_C_Header

#define RZPKG_NAME_MAX 64
#define RZPKG_VERSION_MAX 32
#define RZPKG_ARCH_MAX 16
#define RZPKG_TEXT_MAX 256
#define RZPKG_LIST_MAX 384
#define RZPKG_CHECKSUM_MAX 80
#define RZPKG_SIGNATURE_MAX 160

typedef struct {
	char name[RZPKG_NAME_MAX];
	char version[RZPKG_VERSION_MAX];
	char architecture[RZPKG_ARCH_MAX];
	char description[RZPKG_TEXT_MAX];
	char dependencies[RZPKG_LIST_MAX];
	char permissions[RZPKG_LIST_MAX];
	char maintainer[RZPKG_TEXT_MAX];
	char license[RZPKG_VERSION_MAX];
	char checksum[RZPKG_CHECKSUM_MAX];
	char signature[RZPKG_SIGNATURE_MAX];
	uint64_t installed_size;
} rzpkg_metadata_t;

typedef enum {
	RZPKG_VALID = 0,
	RZPKG_INVALID_ARGUMENT,
	RZPKG_INVALID_NAME,
	RZPKG_INVALID_VERSION,
	RZPKG_INVALID_ARCHITECTURE,
	RZPKG_INVALID_DESCRIPTION,
	RZPKG_INVALID_SIZE,
	RZPKG_INVALID_MAINTAINER,
	RZPKG_INVALID_LICENSE,
	RZPKG_INVALID_CHECKSUM,
	RZPKG_INVALID_LIST,
	RZPKG_METADATA_IO_ERROR,
	RZPKG_METADATA_SYNTAX_ERROR,
	RZPKG_BACKEND_UNAVAILABLE,
} rzpkg_status_t;

typedef struct {
	rzpkg_status_t status;
	unsigned line;
	char field[32];
	char message[128];
} rzpkg_diagnostic_t;

extern void rzpkg_metadata_init(rzpkg_metadata_t * metadata);
extern rzpkg_status_t rzpkg_validate_metadata(
	const rzpkg_metadata_t * metadata,
	rzpkg_diagnostic_t * diagnostic);
extern rzpkg_status_t rzpkg_parse_metadata_file(
	const char * path,
	rzpkg_metadata_t * metadata,
	rzpkg_diagnostic_t * diagnostic);
extern int rzpkg_repository_available(const char * configuration_path);
extern const char * rzpkg_status_string(rzpkg_status_t status);

_End_C_Header
