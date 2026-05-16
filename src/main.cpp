#include <cstring>
#include <iostream>
#include <libgen.h>
#include <string>
#include <sys/file.h>

#include "data.hpp"
#include "pulse.hpp"

#define LOCK_FILE "/tmp/sway-audio-idle-inhibit.lock"

void showHelp(char **argv) {
	string name = basename(argv[0]);
	cout << "Usage:\n";
	cout << "\t" << name << " <OPTION>\n";
	cout << "Options:\n";
	cout << "\t " << name
		 << "\t Inhibits idle if either any sink or any source is running\n";
	cout << "\t -h, --help \t\t\t Show help options\n";
	cout << "\t --dry-print-both \t\t Don't inhibit idle and print if either "
			"any "
			"sink or any source is running\n";
	cout
		<< "\t --dry-print-both-waybar \t Same as --dry-print-both but outputs "
		   "in a waybar friendly manner\n";
	cout << "\t --dry-print-sink \t\t Don't inhibit idle and print if any "
			"sink is running\n";
	cout << "\t --dry-print-source \t\t Don't inhibit idle and print if any "
			"source is running\n";
	cout << "\t --ignore-muted-streams \t Don't inhibit idle for muted or "
			"zero-volume streams\n";
	cout << "\t --ignore-sink-inputs \t\t Don't inhibit idle for these "
			"sink inputs\n";
	cout << "\t --ignore-source-outputs \t\t Don't inhibit idle for these "
			"source outputs\n";
}

static void addIgnoredApps(char *arg, char **ignoredApps, int *ignoredAppsCount) {
	char *saveptr;
	char *token = strtok_r(arg, " ", &saveptr);
	while (token != nullptr && *ignoredAppsCount < MAX_IGNORED_APPS) {
		ignoredApps[(*ignoredAppsCount)++] = token;
		token = strtok_r(nullptr, " ", &saveptr);
	}

	ignoredApps[*ignoredAppsCount] = nullptr;
}

static bool is_already_running() {
	FILE *fd = fopen(LOCK_FILE, "w+");
	if (!fd) {
		fprintf(stderr, "Could not open lock file: %s\n", LOCK_FILE);
		return true;
	}
	if (flock(fd->_fileno, LOCK_EX | LOCK_NB) < 0) {
		if (errno == EWOULDBLOCK) {
			fprintf(stderr, "An instance is already running\n");
		} else {
			fprintf(stderr, "Could not lock file: %s\n", LOCK_FILE);
		}
		return true;
	}
	return false;
}

int main(int argc, char *argv[]) {
	bool printBoth = false;
	bool printBothWayBar = false;
	bool printSource = false;
	bool printSink = false;
	bool ignoreMutedStreams = false;

	char *ignoredSinkInputs[MAX_IGNORED_APPS + 1] = {nullptr};
	char *ignoredSourceOutputs[MAX_IGNORED_APPS + 1] = {nullptr};
	int ignoredSinkInputsCount = 0;
	int ignoredSourceOutputsCount = 0;

	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "--dry-print-source") == 0) {
				printSource = true;
			} else if (strcmp(argv[i], "--dry-print-sink") == 0) {
				printSink = true;
			} else if (strcmp(argv[i], "--dry-print-both") == 0) {
				printBoth = true;
			} else if (strcmp(argv[i], "--dry-print-both-waybar") == 0) {
				printBothWayBar = true;
			} else if (strcmp(argv[i], "--ignore-muted-streams") == 0) {
				ignoreMutedStreams = true;
			} else if (strcmp(argv[i], "--ignore-sink-inputs") == 0 &&
					   i + 1 < argc) {
				addIgnoredApps(argv[++i], ignoredSinkInputs,
							   &ignoredSinkInputsCount);
			} else if (strcmp(argv[i], "--ignore-source-outputs") == 0 &&
					   i + 1 < argc) {
				addIgnoredApps(argv[++i], ignoredSourceOutputs,
							   &ignoredSourceOutputsCount);
			} else {
				showHelp(argv);
				return EXIT_SUCCESS;
			}
		}
	}

	pa_subscription_mask_t all_mask =
		(pa_subscription_mask_t)(PA_SUBSCRIPTION_MASK_SINK_INPUT |
								 PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT);
	if (!printSink && !printSource && !printBoth && !printBothWayBar) {
		// Ensure that only one blocking instance is running
		if (is_already_running()) {
			return EXIT_FAILURE;
		}
		return Pulse().init(SUBSCRIPTION_TYPE_IDLE, all_mask, EVENT_TYPE_IDLE,
							ignoredSinkInputs, ignoredSourceOutputs,
							ignoreMutedStreams);
	} else if (printBoth) {
		return Pulse().init(SUBSCRIPTION_TYPE_DRY_BOTH, all_mask,
							EVENT_TYPE_DRY_BOTH, ignoredSinkInputs,
							ignoredSourceOutputs, ignoreMutedStreams);
	} else if (printBothWayBar) {
		return Pulse().init(SUBSCRIPTION_TYPE_DRY_BOTH_WAYBAR, all_mask,
							EVENT_TYPE_DRY_BOTH, ignoredSinkInputs,
							ignoredSourceOutputs, ignoreMutedStreams);
	} else if (printSink) {
		return Pulse().init(SUBSCRIPTION_TYPE_DRY_SINK,
							PA_SUBSCRIPTION_MASK_SINK_INPUT,
							EVENT_TYPE_DRY_SINK, ignoredSinkInputs,
							ignoredSourceOutputs, ignoreMutedStreams);
	} else if (printSource) {
		return Pulse().init(SUBSCRIPTION_TYPE_DRY_SOURCE,
							PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT,
							EVENT_TYPE_DRY_SOURCE, ignoredSinkInputs,
							ignoredSourceOutputs, ignoreMutedStreams);
	}
	return EXIT_SUCCESS;
}
