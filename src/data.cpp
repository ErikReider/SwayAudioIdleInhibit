#include "data.hpp"

Data::Data(pa_threaded_mainloop *mainloop, pa_mainloop_api *mainloop_api,
           SubscriptionType subscriptionType,
           pa_subscription_mask_t pa_subscriptionType, EventType eventType, char** ignoredSourceOutputs) {
  this->mainloop = mainloop;
  this->mainloop_api = mainloop_api;
  this->subscriptionType = subscriptionType;
  this->pa_subscriptionType = pa_subscriptionType;
  this->eventCalled = eventType;
  this->ignoredSourceOutputs = ignoredSourceOutputs;

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
    case SUBSCRIPTION_TYPE_DRY_BOTH_WAYBAR:
      this->printWayBar(activeSink, activeSource);
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

void Data::printWayBar(bool activeSink, bool activeSource) {
  string result[2] = {activeSink ? "output" : "", activeSource ? "input" : ""};
  string text = "";
  for (const auto &str : result) {
    if (!text.empty() && !str.empty()) text += "-";
    text += str;
  }
  if (text.empty()) text = "none";
  cout << "{\"text\": \"\", \"alt\": \"" + text +
              "\", \"tooltip\": \"\", \"class\": "
              "\"" +
              text + "\"}" << endl;
}
