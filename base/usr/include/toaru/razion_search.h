#pragma once

#include <_cheader.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

_Begin_C_Header

#define RAZION_SEARCH_NAME_MAX 128
#define RAZION_SEARCH_DETAIL_MAX 192
#define RAZION_SEARCH_TARGET_MAX 512

typedef enum {
	RAZION_SEARCH_KIND_APPLICATION = 1,
	RAZION_SEARCH_KIND_SETTING,
	RAZION_SEARCH_KIND_SYSTEM_TOOL,
	RAZION_SEARCH_KIND_DIRECTORY,
	RAZION_SEARCH_KIND_FILE,
} razion_search_kind_t;

typedef struct {
	uint16_t kind;
	uint16_t reserved;
	char name[RAZION_SEARCH_NAME_MAX];
	char detail[RAZION_SEARCH_DETAIL_MAX];
	char target[RAZION_SEARCH_TARGET_MAX];
	time_t modified;
} razion_search_item_t;

typedef struct {
	size_t visited_entries;
	size_t indexed_items;
	size_t skipped_hidden;
	size_t skipped_symlinks;
	size_t skipped_too_long;
	int truncated;
} razion_search_stats_t;

/**
 * Collect visible regular files and directories below an absolute root.
 * Symbolic links and dot-prefixed entries are never followed or returned.
 * Collection stops when capacity is reached.
 */
extern int razion_search_collect(
	const char * root,
	razion_search_item_t * items,
	size_t capacity,
	unsigned int maximum_depth,
	razion_search_stats_t * stats);

/** Return a positive relevance score when every query token matches. */
extern int razion_search_score(
	const razion_search_item_t * item,
	const char * query);

/** Rank matching item indices by relevance and modification time. */
extern size_t razion_search_rank(
	const razion_search_item_t * items,
	size_t item_count,
	const char * query,
	size_t * output_indices,
	size_t output_capacity);

extern const char * razion_search_kind_string(razion_search_kind_t kind);

_End_C_Header
