#pragma once

#include "idle-inhibit-unstable-v1-client-protocol.h"

using namespace std;

class Idle {
	struct wl_compositor *compositor = NULL;
	struct zwp_idle_inhibit_manager_v1 *wl_idle_inhibit_manager = NULL;
	struct wl_surface *surface = NULL;
	struct wl_display *display = NULL;
	struct zwp_idle_inhibitor_v1 *idle = NULL;

	static void global_add(void *data, struct wl_registry *registry,
						   uint32_t name, const char *interface, uint32_t);

	static void global_remove(void *, struct wl_registry *, uint32_t);

  public:
	Idle();

	void update(bool isRunning);
};
