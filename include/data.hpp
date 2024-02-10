#pragma once

#include <pulse/context.h>
#include <pulse/thread-mainloop.h>

#include <iostream>

#include "idle.hpp"

using namespace std;

#define MAX_IGNORED_SOURCE_OUTPUTS 100

enum SubscriptionType {
  SUBSCRIPTION_TYPE_IDLE,
  SUBSCRIPTION_TYPE_DRY_BOTH,
  SUBSCRIPTION_TYPE_DRY_BOTH_WAYBAR,
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

  char **ignoredSourceOutputs;

  Idle *idle = NULL;

  bool failed = false;

  Data(pa_threaded_mainloop *mainloop, pa_mainloop_api *mainloop_api,
       SubscriptionType subscriptionType,
       pa_subscription_mask_t pa_subscriptionType, EventType eventType, char **ignoredSourceOutputs);

  void quit(int returnValue = 0);

  void handleAction();

 private:
  void print(bool isRunning);
  void printWayBar(bool activeSink, bool activeSource);
};
