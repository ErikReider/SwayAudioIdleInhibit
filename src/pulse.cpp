#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/mainloop-api.h>
#include <pulse/proplist.h>
#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <string.h>

#include "data.hpp"
#include "pulse.hpp"

int Pulse::init(SubscriptionType subscriptionType,
				pa_subscription_mask_t pa_subscriptionType, EventType eventType,
				char **ignoredSourceOutputs) {
	pa_threaded_mainloop *mainloop = getMainLoop();
	pa_mainloop_api *mainloop_api = getMainLoopApi(mainloop);
	if (pa_threaded_mainloop_start(mainloop)) {
		fprintf(stderr, "pa_threaded_mainloop_start() failed.\n");
		return 1;
	}
	connect(mainloop, mainloop_api, subscriptionType, pa_subscriptionType,
			eventType, ignoredSourceOutputs);
	return 0;
}

void Pulse::sink_input_info_callback(pa_context *, const pa_sink_input_info *i,
									 int, void *userdata) {
	Data *data = (Data *)userdata;
	if (i && !i->corked && i->client != PA_INVALID_INDEX)
		data->activeSink = true;
	pa_threaded_mainloop_signal(data->mainloop, 0);
}

void Pulse::source_output_info_callback(pa_context *,
										const pa_source_output_info *i, int,
										void *userdata) {
	Data *data = (Data *)userdata;
	bool ignoreSourceOutput = false;
	if (i && i->proplist) {
		const char *appName = pa_proplist_gets(i->proplist, "application.name");
		if (appName) {
			int ignoredSourceOutputsCount = 0;
			while (data->ignoredSourceOutputs[ignoredSourceOutputsCount] !=
					   nullptr &&
				   ignoreSourceOutput == false &&
				   ignoredSourceOutputsCount < MAX_IGNORED_SOURCE_OUTPUTS) {
				if (strcmp(appName, data->ignoredSourceOutputs
										[ignoredSourceOutputsCount]) == 0) {
					ignoreSourceOutput = true;
					break;
				}
				ignoredSourceOutputsCount++;
			}
		}
	}
	if (i && !i->corked && i->client != PA_INVALID_INDEX && !ignoreSourceOutput)
		data->activeSource = true;
	pa_threaded_mainloop_signal(data->mainloop, 0);
}

void Pulse::getRunning(EventType eventType, Data *data, pa_context *context) {
	pa_threaded_mainloop_lock(data->mainloop);
	pa_operation *op = NULL;
	switch (eventType) {
	case EVENT_TYPE_DRY_SINK:
		data->activeSink = false;
		op = pa_context_get_sink_input_info_list(
			context, sink_input_info_callback, data);
		break;
	case EVENT_TYPE_DRY_SOURCE:
		data->activeSource = false;
		op = pa_context_get_source_output_info_list(
			context, source_output_info_callback, data);
		break;
	default:
		fprintf(stderr, "Operation Default!\n");
		pa_threaded_mainloop_unlock(data->mainloop);
		return;
	}
	if (!op) {
		pa_threaded_mainloop_unlock(data->mainloop);
		fprintf(stderr, "Operation failed!\n");
		return;
	}
	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
		pa_threaded_mainloop_wait(data->mainloop);
	}
	pa_operation_unref(op);
	pa_threaded_mainloop_unlock(data->mainloop);
}

void Pulse::subscribe_callback(pa_context *, pa_subscription_event_type_t type,
							   uint32_t, void *userdata) {
	Data *data = (Data *)userdata;
	EventType eventType;
	switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
	case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
		eventType = EVENT_TYPE_DRY_SINK;
		break;
	case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
		eventType = EVENT_TYPE_DRY_SOURCE;
		break;
	default:
		return;
	}
	if (data->subscriptionType == SUBSCRIPTION_TYPE_IDLE) {
		eventType = EVENT_TYPE_IDLE;
	} else if (data->subscriptionType == SUBSCRIPTION_TYPE_DRY_BOTH ||
			   data->subscriptionType == SUBSCRIPTION_TYPE_DRY_BOTH_WAYBAR) {
		eventType = EVENT_TYPE_DRY_BOTH;
	}
	data->eventCalled = eventType;
	pa_threaded_mainloop_signal(data->mainloop, 0);
}

void Pulse::context_state_callback(pa_context *c, void *userdata) {
	Data *data = (Data *)userdata;
	switch (pa_context_get_state(c)) {
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		break;
	case PA_CONTEXT_READY:
		pa_threaded_mainloop_signal(data->mainloop, 0);
		break;
	case PA_CONTEXT_TERMINATED:
	case PA_CONTEXT_FAILED:
		fprintf(stderr, "PulseAudio connection lost. Will retry connection.\n");
		data->failed = true;
		pa_threaded_mainloop_signal(data->mainloop, 0);
		break;
	default:
		fprintf(stderr, "Connection failure: %s\n",
				pa_strerror(pa_context_errno(c)));
		data->quit(1);
		exit(1);
		break;
	}
}

pa_context *Pulse::getContext(pa_threaded_mainloop *mainloop,
							  pa_mainloop_api *mainloop_api, void *userdata) {
	Data *data = (Data *)userdata;

	pa_context *context = pa_context_new(mainloop_api, "PulseAudio Test");
	if (!context) {
		fprintf(stderr, "pa_context_new() failed\n");
		exit(1);
	}

	pa_threaded_mainloop_lock(mainloop);
	if (pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
		fprintf(stderr, "pa_context_connect() failed: %s\n",
				pa_strerror(pa_context_errno(context)));
		exit(1);
	}
	pa_context_set_state_callback(context, context_state_callback, data);
	while (pa_context_get_state(context) != PA_CONTEXT_READY) {
		pa_threaded_mainloop_wait(mainloop);
	}
	pa_threaded_mainloop_unlock(mainloop);

	data->context = context;
	data->failed = false;

	pa_context_set_subscribe_callback(context, subscribe_callback, data);
	pa_context_subscribe(context, data->pa_subscriptionType, NULL, data);
	return context;
}

void Pulse::connect(pa_threaded_mainloop *mainloop,
					pa_mainloop_api *mainloop_api,
					SubscriptionType subscriptionType,
					pa_subscription_mask_t pa_subscriptionType,
					EventType eventType, char **ignoredSourceOutputs) {
	Data *data = new Data(mainloop, mainloop_api, subscriptionType,
						  pa_subscriptionType, eventType, ignoredSourceOutputs);

	pa_context *context = getContext(mainloop, mainloop_api, data);

	for (;;) {
		if (data->failed) {
			context = getContext(data->mainloop, data->mainloop_api, data);
		}
		switch (data->eventCalled) {
		case EVENT_TYPE_IDLE:
		case EVENT_TYPE_DRY_BOTH:
			getRunning(EVENT_TYPE_DRY_SINK, data, context);
			getRunning(EVENT_TYPE_DRY_SOURCE, data, context);
			data->handleAction();
			data->eventCalled = EVENT_TYPE_NONE;
			break;
		case EVENT_TYPE_DRY_SINK:
			getRunning(data->eventCalled, data, context);
			data->handleAction();
			data->eventCalled = EVENT_TYPE_NONE;
			break;
		case EVENT_TYPE_DRY_SOURCE:
			getRunning(data->eventCalled, data, context);
			data->handleAction();
			data->eventCalled = EVENT_TYPE_NONE;
			break;
		case EVENT_TYPE_NONE:
			pa_threaded_mainloop_lock(mainloop);
			pa_threaded_mainloop_wait(mainloop);
			pa_threaded_mainloop_unlock(mainloop);
			break;
		}
	}
}

pa_threaded_mainloop *Pulse::getMainLoop() {
	pa_threaded_mainloop *mainloop = pa_threaded_mainloop_new();
	if (!mainloop) {
		fprintf(stderr, "pa_mainloop_new() failed.\n");
		exit(1);
	}
	return mainloop;
}

pa_mainloop_api *Pulse::getMainLoopApi(pa_threaded_mainloop *mainloop) {
	pa_threaded_mainloop_lock(mainloop);
	pa_mainloop_api *mainloop_api = pa_threaded_mainloop_get_api(mainloop);
	pa_threaded_mainloop_unlock(mainloop);

	if (pa_signal_init(mainloop_api) != 0) {
		fprintf(stderr, "pa_signal_init() failed\n");
		exit(1);
	}
	return mainloop_api;
}
