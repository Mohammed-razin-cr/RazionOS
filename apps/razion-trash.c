/**
 * @brief RazionOS per-user Recycle Bin service utility.
 *
 * Files are moved into the user's home filesystem and accompanied by small
 * metadata records. No privileged operation is performed and restore never
 * overwrites an existing path.
 */
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PATH_SIZE 4096

static char trash_root[PATH_SIZE];
static char trash_files[PATH_SIZE];
static char trash_info[PATH_SIZE];

static int join_path(
	char * output,
	size_t output_size,
	const char * directory,
	const char * name,
	const char * suffix) {

	size_t directory_length = strlen(directory);
	size_t name_length = strlen(name);
	size_t suffix_length = suffix ? strlen(suffix) : 0;
	if (directory_length + 1 + name_length + suffix_length >= output_size) {
		errno = ENAMETOOLONG;
		return -1;
	}
	memcpy(output, directory, directory_length);
	output[directory_length] = '/';
	memcpy(output + directory_length + 1, name, name_length);
	if (suffix_length) {
		memcpy(output + directory_length + 1 + name_length,
			suffix, suffix_length);
	}
	output[directory_length + 1 + name_length + suffix_length] = '\0';
	return 0;
}

static int make_directory(const char * path) {
	if (!mkdir(path, 0700) || errno == EEXIST) return 0;
	fprintf(stderr, "razion-trash: %s: %s\n", path, strerror(errno));
	return -1;
}

static int initialize_paths(void) {
	const char * home = getenv("HOME");
	if (!home || !home[0]) {
		errno = ENOENT;
		return -1;
	}
	char local[PATH_SIZE];
	char share[PATH_SIZE];
	if (join_path(local, sizeof(local), home, ".local", NULL) ||
		join_path(share, sizeof(share), local, "share", NULL) ||
		join_path(trash_root, sizeof(trash_root), share, "Trash", NULL) ||
		join_path(trash_files, sizeof(trash_files), trash_root, "files", NULL) ||
		join_path(trash_info, sizeof(trash_info), trash_root, "info", NULL)) {
		return -1;
	}
	return make_directory(local) || make_directory(share) ||
		make_directory(trash_root) || make_directory(trash_files) ||
		make_directory(trash_info);
}

static const char * path_basename(const char * path) {
	const char * slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

static int safe_stored_name(const char * name) {
	return name && name[0] && strcmp(name, ".") && strcmp(name, "..") &&
		!strchr(name, '/');
}

static int make_unique_name(
	const char * original,
	char * output,
	size_t output_size) {

	const char * base = path_basename(original);
	if (!safe_stored_name(base)) base = "item";
	for (unsigned int suffix = 0; suffix < 100000; ++suffix) {
		char suffix_text[24] = {0};
		if (suffix) snprintf(suffix_text, sizeof(suffix_text), ".%u", suffix);
		size_t suffix_length = strlen(suffix_text);
		size_t base_length = strlen(base);
		if (base_length + suffix_length >= output_size) {
			base_length = output_size - suffix_length - 1;
		}
		memcpy(output, base, base_length);
		memcpy(output + base_length, suffix_text, suffix_length + 1);
		char candidate[PATH_SIZE];
		if (join_path(candidate, sizeof(candidate), trash_files, output, NULL)) return -1;
		struct stat st;
		if (lstat(candidate, &st) && errno == ENOENT) return 0;
	}
	errno = EEXIST;
	return -1;
}

static int write_info(const char * stored_name, const char * original) {
	char path[PATH_SIZE];
	if (join_path(path, sizeof(path), trash_info, stored_name, ".trashinfo")) return -1;
	FILE * file = fopen(path, "w");
	if (!file) return -1;
	fprintf(file, "[Trash Info]\nPath=%s\nDeletionTime=%ld\n",
		original, (long)time(NULL));
	if (fclose(file)) return -1;
	return 0;
}

/**
 * Move an entry without assuming the source and Recycle Bin share a mount.
 * The native rename path remains atomic and fast. RazionOS mv provides the
 * recursive copy-and-remove fallback when the kernel reports EXDEV.
 */
static int move_path(const char * source, const char * destination) {
	if (!rename(source, destination)) return 0;
	if (errno != EXDEV && errno != ENOTSUP) return -1;

	pid_t child = fork();
	if (child < 0) return -1;
	if (!child) {
		char * const arguments[] = {
			"mv", (char *)source, (char *)destination, NULL
		};
		execv("/bin/mv", arguments);
		_exit(127);
	}
	int status = 0;
	while (waitpid(child, &status, 0) < 0) {
		if (errno != EINTR) return -1;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		errno = EIO;
		return -1;
	}
	return 0;
}

static int read_original_path(
	const char * stored_name,
	char * original,
	size_t original_size) {

	char path[PATH_SIZE];
	if (join_path(path, sizeof(path), trash_info, stored_name, ".trashinfo")) return -1;
	FILE * file = fopen(path, "r");
	if (!file) return -1;
	char line[PATH_SIZE + 16];
	original[0] = '\0';
	while (fgets(line, sizeof(line), file)) {
		if (!strncmp(line, "Path=", 5)) {
			char * value = line + 5;
			char * newline = strchr(value, '\n');
			if (newline) *newline = '\0';
			if (value[0] == '/' && strlen(value) < original_size) {
				strcpy(original, value);
			}
			break;
		}
	}
	fclose(file);
	return original[0] ? 0 : -1;
}

static int trash_one(const char * supplied_path) {
	char original[PATH_SIZE];
	if (!realpath(supplied_path, original)) {
		fprintf(stderr, "razion-trash: %s: %s\n", supplied_path, strerror(errno));
		return 1;
	}
	if (!strncmp(original, trash_root, strlen(trash_root)) &&
		(original[strlen(trash_root)] == '/' || !original[strlen(trash_root)])) {
		fprintf(stderr, "razion-trash: item is already in the Recycle Bin: %s\n",
			original);
		return 1;
	}

	char stored_name[512];
	if (make_unique_name(original, stored_name, sizeof(stored_name))) return 1;
	char destination[PATH_SIZE];
	if (join_path(destination, sizeof(destination), trash_files, stored_name, NULL)) return 1;
	if (move_path(original, destination)) {
		fprintf(stderr, "razion-trash: unable to move %s: %s\n",
			original, strerror(errno));
		return 1;
	}
	if (write_info(stored_name, original)) {
		int saved_errno = errno;
		move_path(destination, original);
		fprintf(stderr, "razion-trash: unable to write restore metadata: %s\n",
			strerror(saved_errno));
		return 1;
	}
	printf("%s\n", stored_name);
	return 0;
}

static int restore_one(const char * supplied_name) {
	const char * stored_name = path_basename(supplied_name);
	if (!safe_stored_name(stored_name)) return 1;
	char original[PATH_SIZE];
	if (read_original_path(stored_name, original, sizeof(original))) {
		fprintf(stderr, "razion-trash: restore metadata is missing for %s\n",
			stored_name);
		return 1;
	}
	struct stat st;
	if (!lstat(original, &st)) {
		fprintf(stderr, "razion-trash: restore target already exists: %s\n", original);
		return 1;
	}
	char source[PATH_SIZE];
	char info[PATH_SIZE];
	if (join_path(source, sizeof(source), trash_files, stored_name, NULL) ||
		join_path(info, sizeof(info), trash_info, stored_name, ".trashinfo")) return 1;
	if (move_path(source, original)) {
		fprintf(stderr, "razion-trash: unable to restore %s: %s\n",
			stored_name, strerror(errno));
		return 1;
	}
	unlink(info);
	printf("%s\n", original);
	return 0;
}

static int remove_tree(const char * path) {
	struct stat st;
	if (lstat(path, &st)) return errno == ENOENT ? 0 : 1;
	if (!S_ISDIR(st.st_mode)) return unlink(path) ? 1 : 0;
	DIR * directory = opendir(path);
	if (!directory) return 1;
	int result = 0;
	struct dirent * entry;
	while (!result && (entry = readdir(directory))) {
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
		char child[PATH_SIZE];
		if (join_path(child, sizeof(child), path, entry->d_name, NULL)) {
			result = 1;
			break;
		}
		result = remove_tree(child);
	}
	closedir(directory);
	return result || unlink(path) ? 1 : 0;
}

static unsigned long long tree_size(const char * path) {
	struct stat st;
	if (lstat(path, &st)) return 0;
	if (!S_ISDIR(st.st_mode)) return st.st_size;
	DIR * directory = opendir(path);
	if (!directory) return 0;
	unsigned long long total = 0;
	struct dirent * entry;
	while ((entry = readdir(directory))) {
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
		char child[PATH_SIZE];
		if (join_path(child, sizeof(child), path, entry->d_name, NULL)) continue;
		total += tree_size(child);
	}
	closedir(directory);
	return total;
}

static int empty_trash(void) {
	DIR * directory = opendir(trash_files);
	if (!directory) return 1;
	int result = 0;
	struct dirent * entry;
	while ((entry = readdir(directory))) {
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
		char path[PATH_SIZE];
		if (join_path(path, sizeof(path), trash_files, entry->d_name, NULL)) {
			result = 1;
			continue;
		}
		result |= remove_tree(path);
		if (!join_path(path, sizeof(path), trash_info,
			entry->d_name, ".trashinfo")) unlink(path);
	}
	closedir(directory);
	return result;
}

static int usage(const char * argv0) {
	fprintf(stderr,
		"RazionOS Recycle Bin\n"
		"usage: %s init | put PATH... | restore NAME... | empty | size | path\n",
		argv0);
	return 1;
}

int main(int argc, char * argv[]) {
	if (argc < 2 || initialize_paths()) return usage(argv[0]);
	if (!strcmp(argv[1], "init")) return 0;
	if (!strcmp(argv[1], "path")) {
		printf("%s\n", trash_files);
		return 0;
	}
	if (!strcmp(argv[1], "size")) {
		printf("%llu\n", tree_size(trash_files));
		return 0;
	}
	if (!strcmp(argv[1], "empty")) return empty_trash();
	if (!strcmp(argv[1], "put") && argc > 2) {
		int result = 0;
		for (int i = 2; i < argc; ++i) result |= trash_one(argv[i]);
		return result;
	}
	if (!strcmp(argv[1], "restore") && argc > 2) {
		int result = 0;
		for (int i = 2; i < argc; ++i) result |= restore_one(argv[i]);
		return result;
	}
	return usage(argv[0]);
}
