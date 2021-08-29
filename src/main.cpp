#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/pulseaudio.h>
#include <string.h>

#include <iostream>

#include "./idle.cpp"

using namespace std;

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

  Data(pa_threaded_mainloop *mainloop, pa_mainloop_api *mainloop_api,
       SubscriptionType subscriptionType,
       pa_subscription_mask_t pa_subscriptionType) {
    this->mainloop = mainloop;
    this->mainloop_api = mainloop_api;
    this->subscriptionType = subscriptionType;
    this->pa_subscriptionType = pa_subscriptionType;

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
        if (!idle) idle = new Idle();
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

void sink_input_info_callback(pa_context *, const pa_sink_input_info *i, int,
                              void *userdata) {
  Data *data = (Data *)userdata;
  if (i && !i->corked) data->activeSink = true;
  pa_threaded_mainloop_signal(data->mainloop, 0);
}

void source_output_info_callback(pa_context *, const pa_source_output_info *i,
                                 int, void *userdata) {
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
      op = pa_context_get_sink_input_info_list(context,
                                               sink_input_info_callback, data);
      break;
    case EVENT_TYPE_DRY_SOURCE:
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
  Data *data = (Data *)userdata;
  bool isBoth = data->subscriptionType == SUBSCRIPTION_TYPE_IDLE;
  switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
    case PA_SUBSCRIPTION_EVENT_SINK:
      data->eventCalled = isBoth ? EVENT_TYPE_IDLE : EVENT_TYPE_DRY_SINK;
      break;
    case PA_SUBSCRIPTION_EVENT_SOURCE:
      data->eventCalled = isBoth ? EVENT_TYPE_IDLE : EVENT_TYPE_DRY_SOURCE;
      break;
    default:
      return;
  }
}

void context_state_callback(pa_context *c, void *userdata) {
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
             pa_subscription_mask_t pa_subscriptionType) {
  Data *data =
      new Data(mainloop, mainloop_api, subscriptionType, pa_subscriptionType);

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
    case SUBSCRIPTION_TYPE_IDLE:
      data->eventCalled = EVENT_TYPE_IDLE;
      break;
    case SUBSCRIPTION_TYPE_DRY_BOTH:
      data->eventCalled = EVENT_TYPE_DRY_BOTH;
      break;
    case SUBSCRIPTION_TYPE_DRY_SINK:
      data->eventCalled = EVENT_TYPE_DRY_SINK;
      break;
    case SUBSCRIPTION_TYPE_DRY_SOURCE:
      data->eventCalled = EVENT_TYPE_DRY_SOURCE;
      break;
  }

  for (;;) {
    switch (data->eventCalled) {
      case EVENT_TYPE_IDLE:
      case EVENT_TYPE_DRY_BOTH:
        getRunning(EVENT_TYPE_DRY_SINK, data, context);
        getRunning(EVENT_TYPE_DRY_SOURCE, data, context);
        data->handleAction();
        break;
      case EVENT_TYPE_DRY_SINK:
        getRunning(data->eventCalled, data, context);
        data->handleAction();
        break;
      case EVENT_TYPE_DRY_SOURCE:
        getRunning(data->eventCalled, data, context);
        data->handleAction();
        break;
      default:
        continue;
    }
    data->eventCalled = EVENT_TYPE_NONE;
  }
}

void showHelp(char **argv) {
  cout << "Usage:" << endl;
  cout << "\t" << argv[0] << " <OPTION>" << endl;
  cout << "Options:" << endl;
  cout << "\t-h, --help \t\t Show help options" << endl;
  cout << "\t--dry-print-both \t Don't inhibit idle and print if either any sink or any source is running" << endl;
  cout << "\t--dry-print-sink \t Don't inhibit idle and print if any sink is running" << endl;
  cout << "\t--dry-print-source \t Don't inhibit idle and print if any source is running" << endl;
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
  bool inhibitIdle = true;

  bool printBoth = false;
  bool printSource = false;
  bool printSink = false;

  bool printHelp = false;

  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "--dry-print-source") == 0) {
        printSource = true;
      } else if (strcmp(argv[i], "--dry-print-sink") == 0) {
        printSink = true;
      } else if (strcmp(argv[i], "--dry-print-both") == 0) {
        printBoth = true;
      } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
        printHelp = true;
      }
    }
  }

  if (printHelp) {
    showHelp(argv);
    return 0;
  }

  pa_threaded_mainloop *mainloop = getMainLoop();
  pa_mainloop_api *mainloop_api = getMainLoopApi(mainloop);
  if (pa_threaded_mainloop_start(mainloop)) {
    fprintf(stderr, "pa_threaded_mainloop_start() failed.\n");
    return 1;
  }

  if (inhibitIdle && (!printSink && !printSource && !printBoth)) {
    connect(mainloop, mainloop_api, SUBSCRIPTION_TYPE_IDLE,
            PA_SUBSCRIPTION_MASK_ALL);
  } else if (printBoth) {
    connect(mainloop, mainloop_api, SUBSCRIPTION_TYPE_DRY_BOTH,
            PA_SUBSCRIPTION_MASK_ALL);
  } else if (printSink) {
    connect(mainloop, mainloop_api, SUBSCRIPTION_TYPE_DRY_SINK,
            PA_SUBSCRIPTION_MASK_SINK);
  } else if (printSource) {
    connect(mainloop, mainloop_api, SUBSCRIPTION_TYPE_DRY_SOURCE,
            PA_SUBSCRIPTION_MASK_SOURCE);
  }
  return 0;
}
