#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include "idle.hpp"

using namespace std;

// Takes a logind "idle" inhibitor lock and returns a copy of its fd, or -1 if
// the lock could not be taken.
//
// A new system bus connection is opened for every call. sd-bus only runs the
// authentication handshake of a connection when the connection is used, so a
// connection opened at startup and left untouched until the first audio stream
// is dropped by the bus daemon (auth_timeout, 30 s on dbus-daemon's system
// bus) or given up by sd-bus itself (90 s). The call then fails with ENOTCONN.
// The inhibitor is held by the returned fd, so closing the connection
// afterwards does not release it.
static int take_inhibit_lock() {
	sd_bus *bus = nullptr;
	int ret = sd_bus_open_system(&bus);
	if (ret < 0) {
		fprintf(stderr, "Could not connect to the system bus: %s\n",
				strerror(-ret));
		return -1;
	}

	int lock_fd = -1;
	sd_bus_message *message = nullptr;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	ret = sd_bus_call_method(
		bus, "org.freedesktop.login1", "/org/freedesktop/login1",
		"org.freedesktop.login1.Manager", "Inhibit", &error, &message, "ssss",
		"idle", "sway-audio-idle-inhibit", "Audio is playing", "block");
	if (ret < 0) {
		fprintf(stderr, "Could not send inhibit signal! %s: %s\n",
				error.name ? error.name : "(unknown)",
				error.message ? error.message : strerror(-ret));
	} else {
		int fd = -1;
		ret = sd_bus_message_read(message, "h", &fd);
		if (ret < 0) {
			fprintf(stderr, "Could not get DBus response: %s\n",
					strerror(-ret));
		} else {
			// Clone the FD (will be invalid once we unref the message)
			lock_fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
			if (lock_fd < 0) {
				fprintf(stderr, "Could not copy lock fd: %s\n",
						strerror(errno));
			}
		}
	}

	sd_bus_error_free(&error);
	sd_bus_message_unref(message);
	sd_bus_flush_close_unref(bus);
	return lock_fd;
}

void Idle::block() {
	// Skip if already inhibiting
	if (fd >= 0) {
		return;
	}

	// On failure fd stays -1, so the next audio event tries again.
	fd = take_inhibit_lock();
}

void Idle::release_block() {
	if (fd >= 0) {
		close(fd);
		fd = -1;
	}
}

void Idle::update(bool isRunning) {
	if (isRunning) {
		block();
		cout << (fd >= 0 ? "IDLE INHIBITED" : "IDLE INHIBIT FAILED") << endl;
	} else {
		release_block();
		cout << "NOT IDLE INHIBITED" << endl;
	}
}
