/**
 * @brief Razion Pulse native intent and action broker.
 *
 * Pulse maps a bounded action vocabulary to fixed operating-system APIs.
 * Neither user text nor provider output is ever passed to a shell.
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <toaru/razion_pulse.h>

#define NORMALIZED_REQUEST_MAX 512

static void copy_string(char * destination, size_t size, const char * source) {
	if (!size) return;
	strncpy(destination, source ? source : "", size - 1);
	destination[size - 1] = '\0';
}

static void normalize_request(const char * request, char * output, size_t size) {
	size_t used = 0;
	int pending_space = 0;

	for (const unsigned char * c = (const unsigned char *)request;
		*c && used + 1 < size; ++c) {
		if (isalnum(*c)) {
			if (pending_space && used) output[used++] = ' ';
			output[used++] = tolower(*c);
			pending_space = 0;
		} else {
			pending_space = 1;
		}
	}
	output[used] = '\0';
}

static int has(const char * request, const char * phrase) {
	return strstr(request, phrase) != NULL;
}

static void set_proposal(
	razion_pulse_proposal_t * proposal,
	razion_pulse_action_t action,
	razion_pulse_risk_t risk,
	uint32_t capabilities,
	int confirmation,
	int executable,
	const char * action_id,
	const char * target,
	const char * summary) {

	memset(proposal, 0, sizeof(*proposal));
	proposal->version = RAZION_PULSE_PROTOCOL_VERSION;
	proposal->action = action;
	proposal->risk = risk;
	proposal->required_capabilities = capabilities;
	proposal->requires_confirmation = confirmation;
	proposal->executable = executable;
	copy_string(proposal->action_id, sizeof(proposal->action_id), action_id);
	copy_string(proposal->target, sizeof(proposal->target), target);
	copy_string(proposal->summary, sizeof(proposal->summary), summary);
}

static void home_target(char * output, size_t size, const char * suffix) {
	const char * home = getenv("HOME");
	if (!home || !*home) home = "/home/local";
	snprintf(output, size, "%s%s", home, suffix);
}

razion_pulse_status_t razion_pulse_propose_action(
	const char * action_id,
	razion_pulse_proposal_t * proposal) {

	if (!action_id || !proposal) return RAZION_PULSE_STATUS_INVALID;

	char normalized[RAZION_PULSE_MAX_ACTION_ID];
	normalize_request(action_id, normalized, sizeof(normalized));
	for (char * c = normalized; *c; ++c) {
		if (*c == ' ') *c = '-';
	}

	char target[RAZION_PULSE_MAX_TARGET];
	if (!strcmp(normalized, "open-terminal")) {
		set_proposal(proposal, RAZION_PULSE_ACTION_OPEN_TERMINAL,
			RAZION_PULSE_RISK_READ, RAZION_PULSE_CAP_APP_LAUNCH, 0, 1,
			"open-terminal", "/bin/terminal", "Open Terminal");
	} else if (!strcmp(normalized, "open-calculator")) {
		set_proposal(proposal, RAZION_PULSE_ACTION_OPEN_CALCULATOR,
			RAZION_PULSE_RISK_READ, RAZION_PULSE_CAP_APP_LAUNCH, 0, 1,
			"open-calculator", "/bin/calculator", "Open Calculator");
	} else if (!strcmp(normalized, "open-file-browser")) {
		set_proposal(proposal, RAZION_PULSE_ACTION_OPEN_FILE_BROWSER,
			RAZION_PULSE_RISK_READ, RAZION_PULSE_CAP_APP_LAUNCH, 0, 1,
			"open-file-browser", "/bin/file-browser", "Open File Browser");
	} else if (!strcmp(normalized, "open-system-monitor")) {
		set_proposal(proposal, RAZION_PULSE_ACTION_OPEN_SYSTEM_MONITOR,
			RAZION_PULSE_RISK_READ, RAZION_PULSE_CAP_APP_LAUNCH, 0, 1,
			"open-system-monitor", "/bin/cpuwidget", "Open System Monitor");
	} else if (!strcmp(normalized, "open-settings")) {
		set_proposal(proposal, RAZION_PULSE_ACTION_OPEN_SETTINGS,
			RAZION_PULSE_RISK_READ, RAZION_PULSE_CAP_APP_LAUNCH, 0, 1,
			"open-settings", "/bin/settings", "Open Settings");
	} else if (!strcmp(normalized, "open-wallpaper-settings")) {
		set_proposal(proposal, RAZION_PULSE_ACTION_OPEN_WALLPAPER_SETTINGS,
			RAZION_PULSE_RISK_READ, RAZION_PULSE_CAP_APP_LAUNCH, 0, 1,
			"open-wallpaper-settings", "/bin/wallpaper-picker",
			"Open Wallpaper Settings");
	} else if (!strcmp(normalized, "open-home")) {
		home_target(target, sizeof(target), "");
		set_proposal(proposal, RAZION_PULSE_ACTION_OPEN_HOME,
			RAZION_PULSE_RISK_READ, RAZION_PULSE_CAP_DIRECTORY_OPEN, 0, 1,
			"open-home", target, "Open Home");
	} else if (!strcmp(normalized, "open-downloads")) {
		home_target(target, sizeof(target), "/Downloads");
		set_proposal(proposal, RAZION_PULSE_ACTION_OPEN_DOWNLOADS,
			RAZION_PULSE_RISK_READ, RAZION_PULSE_CAP_DIRECTORY_OPEN, 0, 1,
			"open-downloads", target, "Open Downloads");
	} else if (!strcmp(normalized, "check-memory")) {
		set_proposal(proposal, RAZION_PULSE_ACTION_CHECK_MEMORY,
			RAZION_PULSE_RISK_READ, RAZION_PULSE_CAP_SYSTEM_READ, 0, 1,
			"check-memory", "/proc/meminfo", "Check Memory Usage");
	} else if (!strcmp(normalized, "create-python-project")) {
		home_target(target, sizeof(target), "/Projects/python-project");
		set_proposal(proposal, RAZION_PULSE_ACTION_CREATE_PYTHON_PROJECT,
			RAZION_PULSE_RISK_CHANGE, RAZION_PULSE_CAP_FILE_CREATE, 1, 1,
			"create-python-project", target,
			"Create a Python project without overwriting existing files");
	} else if (!strcmp(normalized, "find-recent-pdf")) {
		set_proposal(proposal, RAZION_PULSE_ACTION_FIND_RECENT_PDF,
			RAZION_PULSE_RISK_READ, RAZION_PULSE_CAP_FILE_SEARCH, 0, 1,
			"find-recent-pdf", "user documents",
			"Find recently modified PDF documents");
	} else if (!strcmp(normalized, "restart-networking")) {
		set_proposal(proposal, RAZION_PULSE_ACTION_RESTART_NETWORKING,
			RAZION_PULSE_RISK_PRIVILEGED, RAZION_PULSE_CAP_NETWORK_ADMIN, 1, 0,
			"restart-networking", "network service",
			"Restart networking through a privileged system broker");
	} else if (!strcmp(normalized, "install-nodejs")) {
		set_proposal(proposal, RAZION_PULSE_ACTION_INSTALL_NODEJS,
			RAZION_PULSE_RISK_PRIVILEGED, RAZION_PULSE_CAP_PACKAGE_MANAGE, 1, 0,
			"install-nodejs", "nodejs",
			"Install Node.js through the package manager");
	} else if (!strcmp(normalized, "summarize-today")) {
		set_proposal(proposal, RAZION_PULSE_ACTION_SUMMARIZE_TODAY,
			RAZION_PULSE_RISK_READ, RAZION_PULSE_CAP_ACTIVITY_READ, 0, 0,
			"summarize-today", "activity index",
			"Summarize today's indexed activity");
	} else {
		memset(proposal, 0, sizeof(*proposal));
		proposal->version = RAZION_PULSE_PROTOCOL_VERSION;
		return RAZION_PULSE_STATUS_UNKNOWN;
	}

	return RAZION_PULSE_STATUS_OK;
}

razion_pulse_status_t razion_pulse_propose(
	const char * request,
	razion_pulse_proposal_t * proposal) {

	if (!request || !*request || !proposal) return RAZION_PULSE_STATUS_INVALID;

	char normalized[NORMALIZED_REQUEST_MAX];
	normalize_request(request, normalized, sizeof(normalized));

	const char * action_id = NULL;
	if (has(normalized, "open") &&
		(has(normalized, "terminal") || has(normalized, "command line"))) {
		action_id = "open-terminal";
	} else if (has(normalized, "open") && has(normalized, "calculator")) {
		action_id = "open-calculator";
	} else if (has(normalized, "open") &&
		(has(normalized, "file browser") || has(normalized, "file manager"))) {
		action_id = "open-file-browser";
	} else if (has(normalized, "open") &&
		(has(normalized, "system monitor") || has(normalized, "task manager"))) {
		action_id = "open-system-monitor";
	} else if (has(normalized, "open") &&
		(has(normalized, "settings") || has(normalized, "preferences"))) {
		action_id = "open-settings";
	} else if ((has(normalized, "open") || has(normalized, "change")) &&
		(has(normalized, "wallpaper") || has(normalized, "background"))) {
		action_id = "open-wallpaper-settings";
	} else if (has(normalized, "open") && has(normalized, "downloads")) {
		action_id = "open-downloads";
	} else if (has(normalized, "open") &&
		(has(normalized, "home") || has(normalized, "home folder"))) {
		action_id = "open-home";
	} else if ((has(normalized, "memory") || has(normalized, "ram")) &&
		(has(normalized, "check") || has(normalized, "usage") ||
		 has(normalized, "status") || has(normalized, "show"))) {
		action_id = "check-memory";
	} else if (has(normalized, "create") && has(normalized, "python") &&
		has(normalized, "project")) {
		action_id = "create-python-project";
	} else if ((has(normalized, "find") || has(normalized, "search")) &&
		has(normalized, "pdf")) {
		action_id = "find-recent-pdf";
	} else if (has(normalized, "restart") &&
		(has(normalized, "network") || has(normalized, "networking"))) {
		action_id = "restart-networking";
	} else if (has(normalized, "install") &&
		(has(normalized, "node js") || has(normalized, "nodejs"))) {
		action_id = "install-nodejs";
	} else if (has(normalized, "summarize") &&
		(has(normalized, "today") || has(normalized, "work"))) {
		action_id = "summarize-today";
	}

	if (!action_id) {
		memset(proposal, 0, sizeof(*proposal));
		proposal->version = RAZION_PULSE_PROTOCOL_VERSION;
		return RAZION_PULSE_STATUS_UNKNOWN;
	}
	return razion_pulse_propose_action(action_id, proposal);
}

int razion_pulse_init(
	razion_pulse_context_t * context,
	const char * app_id,
	uint32_t granted_capabilities) {

	if (!context || !app_id || !*app_id) {
		errno = EINVAL;
		return -1;
	}
	memset(context, 0, sizeof(*context));
	for (size_t i = 0; app_id[i] && i + 1 < sizeof(context->app_id); ++i) {
		unsigned char c = app_id[i];
		context->app_id[i] =
			(isalnum(c) || c == '.' || c == '_' || c == '-') ? c : '_';
	}
	context->granted_capabilities = granted_capabilities;
	return 0;
}

static int spawn_native(const char * executable, char * const arguments[]) {
	if (access(executable, X_OK)) return -1;
	pid_t child = fork();
	if (child < 0) return -1;
	if (!child) {
		execv(executable, arguments);
		_Exit(127);
	}
	return 0;
}

static razion_pulse_status_t open_application(
	razion_pulse_action_t action,
	char * result,
	size_t result_size) {

	const char * executable;
	switch (action) {
		case RAZION_PULSE_ACTION_OPEN_TERMINAL: executable = "/bin/terminal"; break;
		case RAZION_PULSE_ACTION_OPEN_CALCULATOR: executable = "/bin/calculator"; break;
		case RAZION_PULSE_ACTION_OPEN_FILE_BROWSER: executable = "/bin/file-browser"; break;
		case RAZION_PULSE_ACTION_OPEN_SYSTEM_MONITOR: executable = "/bin/cpuwidget"; break;
		case RAZION_PULSE_ACTION_OPEN_SETTINGS: executable = "/bin/settings"; break;
		case RAZION_PULSE_ACTION_OPEN_WALLPAPER_SETTINGS:
			executable = "/bin/wallpaper-picker";
			break;
		default:
			return RAZION_PULSE_STATUS_INVALID;
	}
	char * arguments[] = {(char *)executable, NULL};
	if (spawn_native(executable, arguments)) {
		snprintf(result, result_size, "Unable to launch %s: %s",
			executable, strerror(errno));
		return RAZION_PULSE_STATUS_FAILED;
	}
	snprintf(result, result_size, "Launched %s", executable);
	return RAZION_PULSE_STATUS_OK;
}

static razion_pulse_status_t open_recent_pdf_search(
	char * result,
	size_t result_size) {
	char * arguments[] = {"/bin/universal-search", "--recent-pdf", NULL};
	if (spawn_native(arguments[0], arguments)) {
		snprintf(result, result_size, "Unable to open local PDF search: %s",
			strerror(errno));
		return RAZION_PULSE_STATUS_FAILED;
	}
	copy_string(result, result_size,
		"Opened local PDF search; results are ranked by relevance and modification time.");
	return RAZION_PULSE_STATUS_OK;
}

static razion_pulse_status_t open_directory(
	razion_pulse_action_t action,
	char * result,
	size_t result_size) {

	char target[RAZION_PULSE_MAX_TARGET];
	home_target(target, sizeof(target),
		action == RAZION_PULSE_ACTION_OPEN_DOWNLOADS ? "/Downloads" : "");
	struct stat status;
	if (stat(target, &status) || !S_ISDIR(status.st_mode)) {
		snprintf(result, result_size, "Directory is unavailable: %s", target);
		return RAZION_PULSE_STATUS_UNAVAILABLE;
	}
	char * arguments[] = {"/bin/file-browser", target, NULL};
	if (spawn_native(arguments[0], arguments)) {
		snprintf(result, result_size, "Unable to open %s: %s",
			target, strerror(errno));
		return RAZION_PULSE_STATUS_FAILED;
	}
	snprintf(result, result_size, "Opened %s", target);
	return RAZION_PULSE_STATUS_OK;
}

static razion_pulse_status_t check_memory(char * result, size_t result_size) {
	FILE * meminfo = fopen("/proc/meminfo", "r");
	if (!meminfo) {
		snprintf(result, result_size, "Unable to read memory information: %s",
			strerror(errno));
		return RAZION_PULSE_STATUS_FAILED;
	}

	long total = 0;
	long available = 0;
	char line[160];
	while (fgets(line, sizeof(line), meminfo)) {
		if (sscanf(line, "MemTotal: %ld kB", &total) == 1) continue;
		if (sscanf(line, "MemFree: %ld kB", &available) == 1) continue;
	}
	fclose(meminfo);
	if (total <= 0) {
		copy_string(result, result_size, "Memory information is malformed.");
		return RAZION_PULSE_STATUS_FAILED;
	}
	long used = total - available;
	snprintf(result, result_size,
		"Memory: %ld MB used of %ld MB; %ld MB available.",
		used / 1024, total / 1024, available / 1024);
	return RAZION_PULSE_STATUS_OK;
}

static razion_pulse_status_t create_python_project(
	char * result,
	size_t result_size) {

	char projects[RAZION_PULSE_MAX_TARGET];
	char project[RAZION_PULSE_MAX_TARGET];
	char main_file[RAZION_PULSE_MAX_TARGET];
	home_target(projects, sizeof(projects), "/Projects");
	home_target(project, sizeof(project), "/Projects/python-project");
	home_target(main_file, sizeof(main_file), "/Projects/python-project/main.py");

	struct stat status;
	if (stat(projects, &status) || !S_ISDIR(status.st_mode)) {
		snprintf(result, result_size, "Projects directory is unavailable: %s",
			projects);
		return RAZION_PULSE_STATUS_UNAVAILABLE;
	}
	if (!stat(project, &status)) {
		snprintf(result, result_size,
			"Project already exists; no files were changed: %s", project);
		return RAZION_PULSE_STATUS_UNAVAILABLE;
	}
	if (mkdir(project, 0755)) {
		snprintf(result, result_size, "Unable to create %s: %s",
			project, strerror(errno));
		return RAZION_PULSE_STATUS_FAILED;
	}

	int output = open(main_file, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (output < 0) {
		rmdir(project);
		snprintf(result, result_size, "Unable to create %s: %s",
			main_file, strerror(errno));
		return RAZION_PULSE_STATUS_FAILED;
	}
	const char * starter =
		"def main():\n"
		"    print(\"Hello from RazionOS\")\n"
		"\n"
		"\n"
		"if __name__ == \"__main__\":\n"
		"    main()\n";
	size_t length = strlen(starter);
	if (write(output, starter, length) != (ssize_t)length) {
		close(output);
		unlink(main_file);
		rmdir(project);
		copy_string(result, result_size,
			"Project creation failed; incomplete files were removed.");
		return RAZION_PULSE_STATUS_FAILED;
	}
	close(output);
	snprintf(result, result_size, "Created Python project: %s", project);
	return RAZION_PULSE_STATUS_OK;
}

static void audit_action(
	const razion_pulse_context_t * context,
	const razion_pulse_proposal_t * proposal,
	razion_pulse_status_t status) {

	const char * home = getenv("HOME");
	if (!home || !*home) return;
	char directory[RAZION_PULSE_MAX_TARGET];
	char log_path[RAZION_PULSE_MAX_TARGET];
	snprintf(directory, sizeof(directory), "%s/.razion", home);
	snprintf(log_path, sizeof(log_path), "%s/.razion/pulse-audit.log", home);
	if (mkdir(directory, 0700) && errno != EEXIST) return;

	FILE * log = fopen(log_path, "a");
	if (!log) return;
	fprintf(log,
		"time=%ld app=%s action=%s risk=%s status=%s capabilities=0x%x\n",
		time(NULL),
		context->app_id,
		proposal->action_id,
		razion_pulse_risk_string(proposal->risk),
		razion_pulse_status_string(status),
		proposal->required_capabilities);
	fclose(log);
}

razion_pulse_status_t razion_pulse_execute(
	razion_pulse_context_t * context,
	const razion_pulse_proposal_t * proposal,
	int confirmed,
	char * result,
	size_t result_size) {

	if (!context || !proposal || !result || !result_size ||
		proposal->version != RAZION_PULSE_PROTOCOL_VERSION ||
		proposal->action <= RAZION_PULSE_ACTION_UNKNOWN ||
		proposal->action >= RAZION_PULSE_ACTION_COUNT) {
		return RAZION_PULSE_STATUS_INVALID;
	}
	result[0] = '\0';

	razion_pulse_proposal_t canonical;
	razion_pulse_status_t status;
	if (razion_pulse_propose_action(proposal->action_id, &canonical) !=
		RAZION_PULSE_STATUS_OK || canonical.action != proposal->action) {
		return RAZION_PULSE_STATUS_INVALID;
	}
	proposal = &canonical;

	if ((context->granted_capabilities & proposal->required_capabilities) !=
		proposal->required_capabilities) {
		status = RAZION_PULSE_STATUS_CAPABILITY_DENIED;
		copy_string(result, result_size,
			"The caller does not hold the capability required for this action.");
	} else if (proposal->requires_confirmation && !confirmed) {
		status = RAZION_PULSE_STATUS_CONFIRMATION_REQUIRED;
		copy_string(result, result_size,
			"Explicit user confirmation is required before this action can run.");
	} else if (!proposal->executable ||
		proposal->risk >= RAZION_PULSE_RISK_PRIVILEGED) {
		status = RAZION_PULSE_STATUS_UNAVAILABLE;
		copy_string(result, result_size,
			"No approved native broker is available for this action.");
	} else {
		switch (proposal->action) {
			case RAZION_PULSE_ACTION_OPEN_TERMINAL:
			case RAZION_PULSE_ACTION_OPEN_CALCULATOR:
			case RAZION_PULSE_ACTION_OPEN_FILE_BROWSER:
			case RAZION_PULSE_ACTION_OPEN_SYSTEM_MONITOR:
			case RAZION_PULSE_ACTION_OPEN_SETTINGS:
			case RAZION_PULSE_ACTION_OPEN_WALLPAPER_SETTINGS:
				status = open_application(proposal->action, result, result_size);
				break;
			case RAZION_PULSE_ACTION_OPEN_HOME:
			case RAZION_PULSE_ACTION_OPEN_DOWNLOADS:
				status = open_directory(proposal->action, result, result_size);
				break;
			case RAZION_PULSE_ACTION_CHECK_MEMORY:
				status = check_memory(result, result_size);
				break;
			case RAZION_PULSE_ACTION_CREATE_PYTHON_PROJECT:
				status = create_python_project(result, result_size);
				break;
			case RAZION_PULSE_ACTION_FIND_RECENT_PDF:
				status = open_recent_pdf_search(result, result_size);
				break;
			default:
				status = RAZION_PULSE_STATUS_UNAVAILABLE;
				copy_string(result, result_size,
					"No approved native broker is available for this action.");
				break;
		}
	}
	audit_action(context, proposal, status);
	return status;
}

const char * razion_pulse_risk_string(razion_pulse_risk_t risk) {
	switch (risk) {
		case RAZION_PULSE_RISK_READ: return "read";
		case RAZION_PULSE_RISK_CHANGE: return "change";
		case RAZION_PULSE_RISK_PRIVILEGED: return "privileged";
		case RAZION_PULSE_RISK_DESTRUCTIVE: return "destructive";
		default: return "unknown";
	}
}

const char * razion_pulse_status_string(razion_pulse_status_t status) {
	switch (status) {
		case RAZION_PULSE_STATUS_OK: return "ok";
		case RAZION_PULSE_STATUS_UNKNOWN: return "unknown";
		case RAZION_PULSE_STATUS_INVALID: return "invalid";
		case RAZION_PULSE_STATUS_CAPABILITY_DENIED: return "capability-denied";
		case RAZION_PULSE_STATUS_CONFIRMATION_REQUIRED:
			return "confirmation-required";
		case RAZION_PULSE_STATUS_UNAVAILABLE: return "unavailable";
		case RAZION_PULSE_STATUS_FAILED: return "failed";
		default: return "unknown";
	}
}
