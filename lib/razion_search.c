/**
 * @brief Bounded, offline metadata search for RazionOS.
 *
 * This library indexes names and paths only. It does not read file contents,
 * follow symbolic links, persist user activity, or contact network services.
 */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <toaru/razion_search.h>

#define QUERY_COPY_MAX 256

static void copy_string(char * destination, size_t size, const char * source) {
	if (!size) return;
	if (!source) source = "";
	size_t length = strlen(source);
	if (length >= size) length = size - 1;
	memcpy(destination, source, length);
	destination[length] = '\0';
}

static const char * case_contains(const char * haystack, const char * needle) {
	if (!*needle) return haystack;
	for (; *haystack; ++haystack) {
		const unsigned char * h = (const unsigned char *)haystack;
		const unsigned char * n = (const unsigned char *)needle;
		while (*h && *n && tolower(*h) == tolower(*n)) {
			++h;
			++n;
		}
		if (!*n) return haystack;
	}
	return NULL;
}

static int case_equal(const char * left, const char * right) {
	while (*left && *right) {
		if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
			return 0;
		}
		++left;
		++right;
	}
	return !*left && !*right;
}

static int case_prefix(const char * text, const char * prefix) {
	while (*prefix) {
		if (!*text ||
			tolower((unsigned char)*text) != tolower((unsigned char)*prefix)) {
			return 0;
		}
		++text;
		++prefix;
	}
	return 1;
}

static int collect_directory(
	const char * directory,
	razion_search_item_t * items,
	size_t capacity,
	size_t * count,
	unsigned int depth,
	unsigned int maximum_depth,
	razion_search_stats_t * stats) {

	DIR * dir = opendir(directory);
	if (!dir) return 0;

	struct dirent * entry;
	while ((entry = readdir(dir))) {
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
		stats->visited_entries++;
		if (entry->d_name[0] == '.') {
			stats->skipped_hidden++;
			continue;
		}

		char path[RAZION_SEARCH_TARGET_MAX];
		int written = snprintf(path, sizeof(path), "%s%s%s", directory,
			!strcmp(directory, "/") ? "" : "/", entry->d_name);
		if (written < 0 || (size_t)written >= sizeof(path)) {
			stats->skipped_too_long++;
			continue;
		}

		struct stat status;
		if (lstat(path, &status)) continue;
		if (S_ISLNK(status.st_mode)) {
			stats->skipped_symlinks++;
			continue;
		}
		if (!S_ISDIR(status.st_mode) && !S_ISREG(status.st_mode)) continue;
		if (*count >= capacity) {
			stats->truncated = 1;
			closedir(dir);
			return 1;
		}

		razion_search_item_t * item = &items[(*count)++];
		memset(item, 0, sizeof(*item));
		item->kind = S_ISDIR(status.st_mode) ?
			RAZION_SEARCH_KIND_DIRECTORY : RAZION_SEARCH_KIND_FILE;
		copy_string(item->name, sizeof(item->name), entry->d_name);
		copy_string(item->detail, sizeof(item->detail),
			S_ISDIR(status.st_mode) ? "Folder" : "File");
		copy_string(item->target, sizeof(item->target), path);
		item->modified = status.st_mtime;
		stats->indexed_items++;

		if (S_ISDIR(status.st_mode) && depth < maximum_depth) {
			if (collect_directory(path, items, capacity, count, depth + 1,
				maximum_depth, stats)) {
				closedir(dir);
				return 1;
			}
		}
	}
	closedir(dir);
	return 0;
}

int razion_search_collect(
	const char * root,
	razion_search_item_t * items,
	size_t capacity,
	unsigned int maximum_depth,
	razion_search_stats_t * stats) {

	if (!root || root[0] != '/' || !items || !capacity || !stats) {
		errno = EINVAL;
		return -1;
	}
	memset(stats, 0, sizeof(*stats));
	struct stat status;
	if (lstat(root, &status)) return -1;
	if (!S_ISDIR(status.st_mode) || S_ISLNK(status.st_mode)) {
		errno = ENOTDIR;
		return -1;
	}

	size_t count = 0;
	collect_directory(root, items, capacity, &count, 0, maximum_depth, stats);
	return (int)count;
}

static int kind_weight(uint16_t kind) {
	switch (kind) {
		case RAZION_SEARCH_KIND_APPLICATION: return 24;
		case RAZION_SEARCH_KIND_SETTING: return 20;
		case RAZION_SEARCH_KIND_SYSTEM_TOOL: return 16;
		case RAZION_SEARCH_KIND_DIRECTORY: return 8;
		case RAZION_SEARCH_KIND_FILE: return 4;
		default: return 0;
	}
}

int razion_search_score(const razion_search_item_t * item, const char * query) {
	if (!item || !query) return 0;
	while (isspace((unsigned char)*query)) ++query;
	if (!*query) return 1 + kind_weight(item->kind);

	char copy[QUERY_COPY_MAX];
	copy_string(copy, sizeof(copy), query);
	int score = kind_weight(item->kind);
	char * token = strtok(copy, " \t\r\n");
	while (token) {
		const char * in_name = case_contains(item->name, token);
		const char * in_detail = case_contains(item->detail, token);
		const char * in_target = case_contains(item->target, token);
		if (!in_name && !in_detail && !in_target) return 0;
		if (in_name) score += 60;
		else if (in_detail) score += 30;
		else score += 14;
		if (case_prefix(item->name, token)) score += 28;
		token = strtok(NULL, " \t\r\n");
	}
	if (case_equal(item->name, query)) score += 140;
	else if (case_prefix(item->name, query)) score += 50;
	return score;
}

static int ranks_before(
	const razion_search_item_t * candidate,
	int candidate_score,
	const razion_search_item_t * current,
	int current_score) {
	if (candidate_score != current_score) return candidate_score > current_score;
	if (candidate->modified != current->modified) {
		return candidate->modified > current->modified;
	}
	return strcmp(candidate->name, current->name) < 0;
}

size_t razion_search_rank(
	const razion_search_item_t * items,
	size_t item_count,
	const char * query,
	size_t * output_indices,
	size_t output_capacity) {

	if (!items || !query || !output_indices || !output_capacity) return 0;
	int * scores = calloc(output_capacity, sizeof(int));
	if (!scores) return 0;
	size_t output_count = 0;
	for (size_t index = 0; index < item_count; ++index) {
		int score = razion_search_score(&items[index], query);
		if (!score) continue;
		size_t position = output_count;
		if (position > output_capacity) position = output_capacity;
		while (position > 0 && ranks_before(&items[index], score,
			&items[output_indices[position - 1]], scores[position - 1])) {
			if (position < output_capacity) {
				output_indices[position] = output_indices[position - 1];
				scores[position] = scores[position - 1];
			}
			position--;
		}
		if (position < output_capacity) {
			output_indices[position] = index;
			scores[position] = score;
			if (output_count < output_capacity) output_count++;
		}
	}
	free(scores);
	return output_count;
}

const char * razion_search_kind_string(razion_search_kind_t kind) {
	switch (kind) {
		case RAZION_SEARCH_KIND_APPLICATION: return "Application";
		case RAZION_SEARCH_KIND_SETTING: return "Setting";
		case RAZION_SEARCH_KIND_SYSTEM_TOOL: return "System tool";
		case RAZION_SEARCH_KIND_DIRECTORY: return "Folder";
		case RAZION_SEARCH_KIND_FILE: return "File";
		default: return "Unknown";
	}
}
