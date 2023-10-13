#pragma once

#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/mainloop-api.h>
#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <string.h>

#include <cstdlib>
#include <iostream>

#include "data.hpp"
#include "idle.hpp"

class Pulse {
 public:
  int init(SubscriptionType subscriptionType,
           pa_subscription_mask_t pa_subscriptionType, EventType eventType, char **ignoredSourceOutputs);

 private:
  static void sink_input_info_callback(pa_context *,
                                       const pa_sink_input_info *i, int,
                                       void *userdata);

  static void source_output_info_callback(pa_context *,
                                          const pa_source_output_info *i, int,
                                          void *userdata);

  void getRunning(EventType eventType, Data *data, pa_context *context);

  static void subscribe_callback(pa_context *,
                                 pa_subscription_event_type_t type, uint32_t,
                                 void *userdata);

  static void context_state_callback(pa_context *c, void *userdata);

  static pa_context *getContext(pa_threaded_mainloop *mainloop,
                                pa_mainloop_api *mainloop_api, void *userdata);

  void connect(pa_threaded_mainloop *mainloop, pa_mainloop_api *mainloop_api,
               SubscriptionType subscriptionType,
               pa_subscription_mask_t pa_subscriptionType, EventType eventType, char **ignoredSourceOutputs);

  pa_threaded_mainloop *getMainLoop();

  pa_mainloop_api *getMainLoopApi(pa_threaded_mainloop *mainloop);
};
