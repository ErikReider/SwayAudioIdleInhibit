#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/mainloop-api.h>
#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <string.h>

#include <cstdlib>
#include <iostream>

#include "./idle.cpp"

enum SubscriptionType {
  SUBSCRIPTION_TYPE_IDLE,
  SUBSCRIPTION_TYPE_DRY_BOTH,
  SUBSCRIPTION_TYPE_DRY_SINK,
  SUBSCRIPTION_TYPE_DRY_SOURCE,
};
enum EventType {
  EVENT_TYPE_IDLE,
  EVENT_TYPE_DRY_BOTH,
  EVENT_TYPE_DRY_SINK,
  EVENT_TYPE_DRY_SOURCE,
  EVENT_TYPE_NONE,
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

  Idle *idle = NULL;

  bool failed = false;

  Data(pa_threaded_mainloop *mainloop, pa_mainloop_api *mainloop_api,
       SubscriptionType subscriptionType,
       pa_subscription_mask_t pa_subscriptionType, EventType eventType) {
    this->mainloop = mainloop;
    this->mainloop_api = mainloop_api;
    this->subscriptionType = subscriptionType;
    this->pa_subscriptionType = pa_subscriptionType;
    this->eventCalled = eventType;

    if (subscriptionType == SUBSCRIPTION_TYPE_IDLE) idle = new Idle();
  }

  void quit(int returnValue = 0) {
    mainloop_api->quit(mainloop_api, returnValue);
    pa_threaded_mainloop_stop(mainloop);
    pa_threaded_mainloop_free(mainloop);
  }

  void handleAction() {
    switch (subscriptionType) {
      case SUBSCRIPTION_TYPE_IDLE:
        idle->update(activeSink || activeSource);
        break;
      case SUBSCRIPTION_TYPE_DRY_BOTH:
        this->print(activeSink || activeSource);
        break;
      case SUBSCRIPTION_TYPE_DRY_SINK:
        this->print(activeSink);
        break;
      case SUBSCRIPTION_TYPE_DRY_SOURCE:
        this->print(activeSource);
        break;
    }
  }

 private:
  void print(bool isRunning) {
    cout << (isRunning ? "RUNNING" : "NOT RUNNING") << endl;
  }
};

class Pulse {
 public:
  int init(SubscriptionType subscriptionType,
           pa_subscription_mask_t pa_subscriptionType, EventType eventType) {
    pa_threaded_mainloop *mainloop = getMainLoop();
    pa_mainloop_api *mainloop_api = getMainLoopApi(mainloop);
    if (pa_threaded_mainloop_start(mainloop)) {
      fprintf(stderr, "pa_threaded_mainloop_start() failed.\n");
      return 1;
    }
    connect(mainloop, mainloop_api, subscriptionType, pa_subscriptionType,
            eventType);
    return 0;
  }

 private:
  static void sink_input_info_callback(pa_context *,
                                       const pa_sink_input_info *i, int,
                                       void *userdata) {
    Data *data = (Data *)userdata;
    if (i && !i->corked) data->activeSink = true;
    pa_threaded_mainloop_signal(data->mainloop, 0);
  }

  static void source_output_info_callback(pa_context *,
                                          const pa_source_output_info *i, int,
                                          void *userdata) {
    Data *data = (Data *)userdata;
    if (i && !i->corked) data->activeSource = true;
    pa_threaded_mainloop_signal(data->mainloop, 0);
  }

  void getRunning(EventType eventType, Data *data, pa_context *context) {
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

  static void subscribe_callback(pa_context *,
                                 pa_subscription_event_type_t type, uint32_t,
                                 void *userdata) {
    Data *data = (Data *)userdata;
    EventType eventType;
    switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
      case PA_SUBSCRIPTION_EVENT_SINK:
        eventType = EVENT_TYPE_DRY_SINK;
        break;
      case PA_SUBSCRIPTION_EVENT_SOURCE:
        eventType = EVENT_TYPE_DRY_SOURCE;
        break;
      default:
        return;
    }
    if (data->subscriptionType == SUBSCRIPTION_TYPE_IDLE) {
      eventType = EVENT_TYPE_IDLE;
    } else if (data->subscriptionType == SUBSCRIPTION_TYPE_DRY_BOTH) {
      eventType = EVENT_TYPE_DRY_BOTH;
    }
    data->eventCalled = eventType;
    pa_threaded_mainloop_signal(data->mainloop, 0);
  }

  static void context_state_callback(pa_context *c, void *userdata) {
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

  static pa_context *getContext(pa_threaded_mainloop *mainloop,
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

  void connect(pa_threaded_mainloop *mainloop, pa_mainloop_api *mainloop_api,
               SubscriptionType subscriptionType,
               pa_subscription_mask_t pa_subscriptionType,
               EventType eventType) {
    Data *data = new Data(mainloop, mainloop_api, subscriptionType,
                          pa_subscriptionType, eventType);

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
};
