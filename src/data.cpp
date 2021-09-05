#include "data.hpp"

Data::Data(pa_threaded_mainloop *mainloop, pa_mainloop_api *mainloop_api,
           SubscriptionType subscriptionType,
           pa_subscription_mask_t pa_subscriptionType, EventType eventType) {
  this->mainloop = mainloop;
  this->mainloop_api = mainloop_api;
  this->subscriptionType = subscriptionType;
  this->pa_subscriptionType = pa_subscriptionType;
  this->eventCalled = eventType;

  if (subscriptionType == SUBSCRIPTION_TYPE_IDLE) idle = new Idle();
}

void Data::quit(int returnValue) {
  mainloop_api->quit(mainloop_api, returnValue);
  pa_threaded_mainloop_stop(mainloop);
  pa_threaded_mainloop_free(mainloop);
}

void Data::handleAction() {
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

void Data::print(bool isRunning) {
  cout << (isRunning ? "RUNNING" : "NOT RUNNING") << endl;
}
