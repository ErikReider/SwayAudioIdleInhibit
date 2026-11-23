#pragma once

#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/mainloop-api.h>
#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>

#include "data.hpp"

class Pulse {
  public:
	int init(SubscriptionType subscriptionType,
			 pa_subscription_mask_t pa_subscriptionType, EventType eventType,
			 char **ignoredSourceOutputs);

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

	static void connect_timeout_callback(pa_mainloop_api *, pa_time_event *,
										 const struct timeval *,
										 void *userdata);

	static void releaseContext(pa_context *context);

	static pa_context *getContext(Data *data, bool report);

	static pa_context *getContextRetry(Data *data, bool reconnect);

	void connect(pa_threaded_mainloop *mainloop, pa_mainloop_api *mainloop_api,
				 SubscriptionType subscriptionType,
				 pa_subscription_mask_t pa_subscriptionType,
				 EventType eventType, char **ignoredSourceOutputs);

	pa_threaded_mainloop *getMainLoop();

	pa_mainloop_api *getMainLoopApi(pa_threaded_mainloop *mainloop);
};
