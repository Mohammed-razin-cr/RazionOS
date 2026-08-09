#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <toaru/razion_search.h>

static void make_file(const char * path) {
	int file = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
	assert(file >= 0);
	assert(write(file, "test", 4) == 4);
	assert(close(file) == 0);
}

int main(void) {
	char root[] = "/tmp/razion-search-XXXXXX";
	assert(mkdtemp(root));
	char notes[512];
	char report[512];
	char hidden[512];
	char link[512];
	snprintf(notes, sizeof(notes), "%s/Notes", root);
	snprintf(report, sizeof(report), "%s/Notes/machine-learning-report.pdf", root);
	snprintf(hidden, sizeof(hidden), "%s/.private", root);
	snprintf(link, sizeof(link), "%s/report-link", root);
	assert(mkdir(notes, 0700) == 0);
	make_file(report);
	make_file(hidden);
	assert(symlink(report, link) == 0);

	razion_search_item_t items[16];
	razion_search_stats_t stats;
	int count = razion_search_collect(root, items, 16, 4, &stats);
	assert(count == 2);
	assert(stats.indexed_items == 2);
	assert(stats.skipped_hidden == 1);
	assert(stats.skipped_symlinks == 1);

	size_t ranked[4];
	size_t matches = razion_search_rank(items, count, "machine pdf", ranked, 4);
	assert(matches == 1);
	assert(!strcmp(items[ranked[0]].name, "machine-learning-report.pdf"));
	assert(razion_search_rank(items, count, "private", ranked, 4) == 0);

	razion_search_item_t limited[1];
	assert(razion_search_collect(root, limited, 1, 4, &stats) == 1);
	assert(stats.truncated == 1);

	assert(unlink(link) == 0);
	assert(unlink(hidden) == 0);
	assert(unlink(report) == 0);
	assert(rmdir(notes) == 0);
	assert(rmdir(root) == 0);
	puts("razion-search: all tests passed");
	return 0;
}
