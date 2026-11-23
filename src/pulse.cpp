#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/mainloop-api.h>
#include <pulse/proplist.h>
#include <pulse/pulseaudio.h>
#include <pulse/rtclock.h>
#include <pulse/thread-mainloop.h>
#include <string.h>
#include <unistd.h>

#include "data.hpp"
#include "pulse.hpp"

// How long a new connection may take to become ready
#define CONNECT_TIMEOUT_USEC (10 * PA_USEC_PER_SEC)
// Delay between connection attempts
#define RETRY_DELAY_MIN_MSEC 500
#define RETRY_DELAY_MAX_MSEC 5000

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
	case PA_CONTEXT_READY:
		pa_threaded_mainloop_signal(data->mainloop, 0);
		break;
	case PA_CONTEXT_TERMINATED:
	case PA_CONTEXT_FAILED:
		data->failed = true;
		pa_threaded_mainloop_signal(data->mainloop, 0);
		break;
	default:
		break;
	}
}

void Pulse::connect_timeout_callback(pa_mainloop_api *, pa_time_event *,
									 const struct timeval *, void *userdata) {
	Data *data = (Data *)userdata;
	data->connectTimedOut = true;
	pa_threaded_mainloop_signal(data->mainloop, 0);
}

// Disconnects and frees a context. The caller holds the mainloop lock.
void Pulse::releaseContext(pa_context *context) {
	pa_context_set_state_callback(context, NULL, NULL);
	pa_context_set_subscribe_callback(context, NULL, NULL);
	pa_context_disconnect(context);
	pa_context_unref(context);
}

// Connects a new context and subscribes to its events. Returns NULL if there
// is no server, if the connection fails, or if it isn't ready within
// CONNECT_TIMEOUT_USEC (a server that accepts and never answers would
// otherwise keep it waiting for libpulse's own 30 s).
pa_context *Pulse::getContext(Data *data, bool report) {
	pa_threaded_mainloop_lock(data->mainloop);
	pa_context *context = pa_context_new(data->mainloop_api, "PulseAudio Test");
	if (!context) {
		pa_threaded_mainloop_unlock(data->mainloop);
		if (report)
			fprintf(stderr, "pa_context_new() failed\n");
		return NULL;
	}
	pa_context_set_state_callback(context, context_state_callback, data);
	if (pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
		if (report)
			fprintf(stderr, "pa_context_connect() failed: %s\n",
					pa_strerror(pa_context_errno(context)));
		releaseContext(context);
		pa_threaded_mainloop_unlock(data->mainloop);
		return NULL;
	}

	data->connectTimedOut = false;
	pa_time_event *timeout =
		pa_context_rttime_new(context, pa_rtclock_now() + CONNECT_TIMEOUT_USEC,
							  connect_timeout_callback, data);
	pa_context_state_t state;
	while ((state = pa_context_get_state(context)) != PA_CONTEXT_READY &&
		   PA_CONTEXT_IS_GOOD(state) && !data->connectTimedOut) {
		pa_threaded_mainloop_wait(data->mainloop);
	}
	if (timeout)
		data->mainloop_api->time_free(timeout);
	if (state != PA_CONTEXT_READY) {
		if (report && data->connectTimedOut)
			fprintf(stderr, "PulseAudio connection timed out\n");
		else if (report)
			fprintf(stderr, "PulseAudio connection failed: %s\n",
					pa_strerror(pa_context_errno(context)));
		releaseContext(context);
		pa_threaded_mainloop_unlock(data->mainloop);
		return NULL;
	}

	data->context = context;
	data->failed = false;
	pa_context_set_subscribe_callback(context, subscribe_callback, data);
	pa_operation *op =
		pa_context_subscribe(context, data->pa_subscriptionType, NULL, NULL);
	if (op)
		pa_operation_unref(op);
	// Read the streams again: they may have changed while disconnected
	data->eventCalled = data->eventType;
	pa_threaded_mainloop_unlock(data->mainloop);
	return context;
}

// Connects, retrying until a server answers. The delay between attempts
// doubles from RETRY_DELAY_MIN_MSEC up to RETRY_DELAY_MAX_MSEC. Only the first
// failure of a series is printed.
//
// When reconnecting, the idle inhibitor is kept if the first attempt succeeds
// (the streams are read again right after), so a short drop doesn't release
// it while audio plays. If that attempt fails, the streams of the lost server
// can't be seen: the inhibitor is released (or the inactive state printed)
// until they can.
pa_context *Pulse::getContextRetry(Data *data, bool reconnect) {
	unsigned int delay = RETRY_DELAY_MIN_MSEC;
	bool report = true;
	for (;;) {
		pa_context *context = getContext(data, report);
		if (context) {
			if (reconnect || !report)
				fprintf(stderr, "PulseAudio connection established.\n");
			return context;
		}
		if (report) {
			fprintf(stderr, "Will retry connection.\n");
			if (reconnect) {
				data->activeSink = false;
				data->activeSource = false;
				data->handleAction();
			}
		}
		report = false;
		usleep(delay * 1000);
		delay =
			delay * 2 < RETRY_DELAY_MAX_MSEC ? delay * 2 : RETRY_DELAY_MAX_MSEC;
	}
}

void Pulse::connect(pa_threaded_mainloop *mainloop,
					pa_mainloop_api *mainloop_api,
					SubscriptionType subscriptionType,
					pa_subscription_mask_t pa_subscriptionType,
					EventType eventType, char **ignoredSourceOutputs) {
	Data *data = new Data(mainloop, mainloop_api, subscriptionType,
						  pa_subscriptionType, eventType, ignoredSourceOutputs);

	pa_context *context = getContextRetry(data, false);

	for (;;) {
		pa_threaded_mainloop_lock(mainloop);
		while (data->eventCalled == EVENT_TYPE_NONE && !data->failed) {
			pa_threaded_mainloop_wait(mainloop);
		}
		EventType eventCalled = data->eventCalled;
		data->eventCalled = EVENT_TYPE_NONE;
		bool failed = data->failed;
		pa_threaded_mainloop_unlock(mainloop);

		if (failed) {
			fprintf(stderr,
					"PulseAudio connection lost. Will retry connection.\n");
			pa_threaded_mainloop_lock(mainloop);
			releaseContext(context);
			pa_threaded_mainloop_unlock(mainloop);
			context = getContextRetry(data, true);
			continue;
		}

		switch (eventCalled) {
		case EVENT_TYPE_IDLE:
		case EVENT_TYPE_DRY_BOTH:
			getRunning(EVENT_TYPE_DRY_SINK, data, context);
			getRunning(EVENT_TYPE_DRY_SOURCE, data, context);
			break;
		case EVENT_TYPE_DRY_SINK:
		case EVENT_TYPE_DRY_SOURCE:
			getRunning(eventCalled, data, context);
			break;
		case EVENT_TYPE_NONE:
			break;
		}

		// A connection lost during the queries leaves them incomplete; the
		// next round handles the loss instead
		pa_threaded_mainloop_lock(mainloop);
		failed = data->failed;
		pa_threaded_mainloop_unlock(mainloop);
		if (!failed)
			data->handleAction();
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
