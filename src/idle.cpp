#include <cassert>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include "idle.hpp"

using namespace std;

void Idle::block() {
	assert(bus);

	// Skip if already inhibiting
	if (fd >= 0) {
		return;
	}

	sd_bus_message *message = nullptr;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int ret = sd_bus_call_method(
		bus, "org.freedesktop.login1", "/org/freedesktop/login1",
		"org.freedesktop.login1.Manager", "Inhibit", &error, &message, "ssss",
		"idle", "sway-audio-idle-inhibit", "Audio is playing", "block");
	if (ret < 0) {
		fprintf(stderr, "Could not send inhibit signal!\n");
		abort();
	}

	ret = sd_bus_message_read(message, "h", &fd);
	if (ret < 0) {
		errno = -ret;
		fprintf(stderr, "Could not get DBus response\n");
		abort();
	}

	// Clone the FD (will be invalid once we unref the message)
	fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
	if (fd >= 0) {
	} else {
		fprintf(stderr, "Could not copy lock fd\n");
	}

	sd_bus_error_free(&error);
	sd_bus_message_unref(message);
}

void Idle::release_block() {
	if (fd >= 0) {
		close(fd);
		fd = -1;
	}
}

Idle::Idle() {
	// Connect to DBus
	int ret = sd_bus_default_system(&bus);
	if (ret < 0) {
		fprintf(stderr, "Failed to get DBus connection\n");
		abort();
	}
}

void Idle::update(bool isRunning) {
	if (isRunning) {
		block();
		cout << "IDLE INHIBITED" << endl;
	} else {
		release_block();
		cout << "NOT IDLE INHIBITED" << endl;
	}
}
