#include <string.h>
#include <wayland-client-protocol.h>

#include <iostream>

#include "idle-inhibit-unstable-v1-client-protocol.h"

using namespace std;

class Idle {
  struct wl_compositor *compositor = NULL;
  struct zwp_idle_inhibit_manager_v1 *wl_idle_inhibit_manager = NULL;
  struct wl_surface *surface = NULL;
  struct wl_display *display = NULL;
  struct zwp_idle_inhibitor_v1 *idle = NULL;

  static void global_add(void *data, struct wl_registry *registry,
                         uint32_t name, const char *interface, uint32_t) {
    Idle *idle = (Idle *)data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
      idle->compositor = (wl_compositor *)wl_registry_bind(
          registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, zwp_idle_inhibit_manager_v1_interface.name) ==
               0) {
      idle->wl_idle_inhibit_manager =
          (zwp_idle_inhibit_manager_v1 *)wl_registry_bind(
              registry, name, &zwp_idle_inhibit_manager_v1_interface, 1);
    }
  }

  static void global_remove(void *, struct wl_registry *, uint32_t) {
    // Do nothing
  }

 public:
  Idle() {
    display = wl_display_connect(NULL);
    if (display == NULL) {
      fprintf(stderr, "failed to connect to wl_display\n");
      exit(1);
    }

    const struct wl_registry_listener registry_listener = {
        .global = global_add,
        .global_remove = global_remove,
    };

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);

    if (wl_idle_inhibit_manager == NULL) {
      fprintf(stderr, "wl_idle_inhibit_manager is NULL\n");
      exit(1);
    }
    if (compositor == NULL) {
      fprintf(stderr, "compositor is NULL\n");
      exit(1);
    }

    surface = wl_compositor_create_surface(compositor);
  }

  void update(bool isRunning) {
    if (isRunning) {
      if (idle == NULL) {
        idle = zwp_idle_inhibit_manager_v1_create_inhibitor(
            wl_idle_inhibit_manager, surface);
        wl_display_roundtrip(display);
      }
      cout << "IDLE INHIBITED" << endl;
    } else {
      if (idle != NULL) {
        zwp_idle_inhibitor_v1_destroy(idle);
        idle = NULL;
        wl_display_roundtrip(display);
      }
      cout << "NOT IDLE INHIBITED" << endl;
    }
  }
};
