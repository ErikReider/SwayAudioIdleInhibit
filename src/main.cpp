#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/pulseaudio.h>
#include <string.h>

#include <iostream>

using namespace std;

enum SubscriptionType {
  SUBSCRIPTION_TYPE_SINK,
  SUBSCRIPTION_TYPE_SOURCE,
  SUBSCRIPTION_TYPE_BOTH
};
enum EventType {
  EVENT_TYPE_SINK,
  EVENT_TYPE_SOURCE,
  EVENT_TYPE_BOTH,
  EVENT_TYPE_NONE
};

struct Data {
  pa_threaded_mainloop *mainloop;
  pa_mainloop_api *mainloop_api;
  pa_context *context;

  EventType eventCalled = EVENT_TYPE_NONE;
  bool activeSource = false;
  bool activeSink = false;

  SubscriptionType subscriptionType;
  pa_subscription_mask_t pa_subscriptionType;
  bool json;

  Data(pa_threaded_mainloop *mainloop, pa_mainloop_api *mainloop_api,
       SubscriptionType subscriptionType,
       pa_subscription_mask_t pa_subscriptionType, bool json) {
    this->mainloop = mainloop;
    this->mainloop_api = mainloop_api;
    this->subscriptionType = subscriptionType;
    this->pa_subscriptionType = pa_subscriptionType;
    this->json = json;
  }

  void quit(int returnValue = 0) {
    mainloop_api->quit(mainloop_api, returnValue);
    pa_threaded_mainloop_stop(mainloop);
    pa_threaded_mainloop_free(mainloop);
  }

  void printResult() {
    bool isRunning = false;
    switch (subscriptionType) {
      case SUBSCRIPTION_TYPE_BOTH:
        isRunning = activeSink || activeSource;
        break;
      case SUBSCRIPTION_TYPE_SINK:
        isRunning = activeSink;
        break;
      case SUBSCRIPTION_TYPE_SOURCE:
        isRunning = activeSource;
        break;
    }
    cout << (isRunning ? "RUNNING" : "NOT RUNNING") << endl;
  }
};

void sink_input_info_callback(pa_context *, const pa_sink_input_info *i, int,
                              void *userdata) {
  auto *data = (Data *)userdata;
  if (i && !i->corked) data->activeSink = true;
  pa_threaded_mainloop_signal(data->mainloop, 0);
}

void source_output_info_callback(pa_context *, const pa_source_output_info *i,
                                 int, void *userdata) {
  auto *data = (Data *)userdata;
  if (i && !i->corked) data->activeSource = true;
  pa_threaded_mainloop_signal(data->mainloop, 0);
}

void getRunning(EventType eventType, Data *data, pa_context *context) {
  pa_threaded_mainloop_lock(data->mainloop);
  pa_operation *op = NULL;
  switch (eventType) {
    case EVENT_TYPE_SINK:
      data->activeSink = false;
      op = pa_context_get_sink_input_info_list(context,
                                               sink_input_info_callback, data);
      break;
    case EVENT_TYPE_SOURCE:
      data->activeSource = false;
      op = pa_context_get_source_output_info_list(
          context, source_output_info_callback, data);
      break;
    default:
      return;
  }
  if (!op) return;
  while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
    pa_threaded_mainloop_wait(data->mainloop);
  }
  pa_operation_unref(op);
  pa_threaded_mainloop_unlock(data->mainloop);
}

void subscribe_callback(pa_context *, pa_subscription_event_type_t type,
                        uint32_t, void *userdata) {
  auto *data = (Data *)userdata;
  bool isBoth = data->subscriptionType == SUBSCRIPTION_TYPE_BOTH;
  switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
    case PA_SUBSCRIPTION_EVENT_SINK:
      data->eventCalled = isBoth ? EVENT_TYPE_BOTH : EVENT_TYPE_SINK;
      break;
    case PA_SUBSCRIPTION_EVENT_SOURCE:
      data->eventCalled = isBoth ? EVENT_TYPE_BOTH : EVENT_TYPE_SOURCE;
      break;
    default:
      return;
  }
}

void context_state_callback(pa_context *c, void *userdata) {
  auto *data = (Data *)userdata;
  switch (pa_context_get_state(c)) {
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      break;
    case PA_CONTEXT_READY:
      pa_threaded_mainloop_signal(data->mainloop, 0);
      break;
    case PA_CONTEXT_TERMINATED:
      pa_threaded_mainloop_signal(data->mainloop, 1);
      data->quit(0);
      fprintf(stderr, "PulseAudio connection terminated.\n");
      break;
    case PA_CONTEXT_FAILED:
    default:
      pa_threaded_mainloop_signal(data->mainloop, 1);
      fprintf(stderr, "Connection failure: %s\n",
              pa_strerror(pa_context_errno(c)));
      data->quit(1);
      break;
  }
}

void connect(pa_threaded_mainloop *mainloop, pa_mainloop_api *mainloop_api,
             SubscriptionType subscriptionType,
             pa_subscription_mask_t pa_subscriptionType, bool json) {
  Data *data = new Data(mainloop, mainloop_api, subscriptionType,
                        pa_subscriptionType, json);

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

  pa_context_set_subscribe_callback(context, subscribe_callback, data);
  pa_context_subscribe(context, data->pa_subscriptionType, NULL, data);

  // print when started
  switch (subscriptionType) {
    case SUBSCRIPTION_TYPE_BOTH:
      data->eventCalled = EVENT_TYPE_BOTH;
      break;
    case SUBSCRIPTION_TYPE_SINK:
      data->eventCalled = EVENT_TYPE_SINK;
      break;
    case SUBSCRIPTION_TYPE_SOURCE:
      data->eventCalled = EVENT_TYPE_SOURCE;
      break;
  }

  for (;;) {
    switch (data->eventCalled) {
      case EVENT_TYPE_BOTH:
        getRunning(EVENT_TYPE_SINK, data, context);
        getRunning(EVENT_TYPE_SOURCE, data, context);
        data->printResult();
        break;
      case EVENT_TYPE_SINK:
        getRunning(data->eventCalled, data, context);
        data->printResult();
        break;
      case EVENT_TYPE_SOURCE:
        getRunning(data->eventCalled, data, context);
        data->printResult();
        break;
      default:
        continue;
    }
    data->eventCalled = EVENT_TYPE_NONE;
  }
}

pa_threaded_mainloop *getMainLoop() {
  pa_threaded_mainloop *mainloop = pa_threaded_mainloop_new();
  if (!mainloop) {
    fprintf(stderr, "pa_mainloop_new() failed.\n");
    exit(1);
  }
  return mainloop;
}

pa_mainloop_api *getMainLoopApi(pa_threaded_mainloop *mainloop) {
  pa_threaded_mainloop_lock(mainloop);
  pa_mainloop_api *mainloop_api = pa_threaded_mainloop_get_api(mainloop);
  pa_threaded_mainloop_unlock(mainloop);

  if (pa_signal_init(mainloop_api) != 0) {
    fprintf(stderr, "pa_signal_init() failed\n");
    exit(1);
  }
  return mainloop_api;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("ERROR: You need at least one argument.\n");
    return 1;
  }

  bool both = false;
  bool source = false;
  bool sink = false;
  bool json = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--both") == 0) {
      both = true;
    } else if (strcmp(argv[i], "--source") == 0) {
      source = true;
    } else if (strcmp(argv[i], "--sink") == 0) {
      sink = true;
    } else if (strcmp(argv[i], "--json") == 0) {
      json = true;
    }
  }

  pa_threaded_mainloop *mainloop = getMainLoop();
  pa_mainloop_api *mainloop_api = getMainLoopApi(mainloop);
  if (pa_threaded_mainloop_start(mainloop)) {
    fprintf(stderr, "pa_threaded_mainloop_start() failed.\n");
    return 1;
  }

  if (both || (sink && source)) {
    connect(mainloop, mainloop_api, SUBSCRIPTION_TYPE_BOTH,
            PA_SUBSCRIPTION_MASK_ALL, json);
  } else if (sink) {
    connect(mainloop, mainloop_api, SUBSCRIPTION_TYPE_SINK,
            PA_SUBSCRIPTION_MASK_SINK, json);
  } else if (source) {
    connect(mainloop, mainloop_api, SUBSCRIPTION_TYPE_SOURCE,
            PA_SUBSCRIPTION_MASK_SOURCE, json);
  }
  return 0;
}
