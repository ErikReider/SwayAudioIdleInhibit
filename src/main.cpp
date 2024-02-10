#include <libgen.h>
#include <cstring>

#include "data.hpp"
#include "pulse.hpp"

void showHelp(char **argv) {
  string name = basename(argv[0]);
  cout << "Usage:\n";
  cout << "\t" << name << " <OPTION>\n";
  cout << "Options:\n";
  cout << "\t " << name
       << "\t Inhibits idle if either any sink or any source is running\n";
  cout << "\t -h, --help \t\t\t Show help options\n";
  cout << "\t --dry-print-both \t\t Don't inhibit idle and print if either any "
          "sink or any source is running\n";
  cout << "\t --dry-print-both-waybar \t Same as --dry-print-both but outputs "
          "in a waybar friendly manner\n";
  cout << "\t --dry-print-sink \t\t Don't inhibit idle and print if any "
          "sink is running\n";
  cout << "\t --dry-print-source \t\t Don't inhibit idle and print if any "
          "source is running\n";
  cout << "\t --ignore-source-outputs \t\t Don't inhibit idle for these "
          "source outputs\n";
  
}

int main(int argc, char *argv[]) {
  bool printBoth = false;
  bool printBothWayBar = false;
  bool printSource = false;
  bool printSink = false;

  char* ignoredSourceOutputs[MAX_IGNORED_SOURCE_OUTPUTS];
  int ignoredSourceOutputsCount = 0;

  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "--dry-print-source") == 0) {
        printSource = true;
      } else if (strcmp(argv[i], "--dry-print-sink") == 0) {
        printSink = true;
      } else if (strcmp(argv[i], "--dry-print-both") == 0) {
        printBoth = true;
      } else if (strcmp(argv[i], "--dry-print-both-waybar") == 0) {
        printBothWayBar = true;
      } else if (strcmp(argv[i], "--ignore-source-outputs") == 0 && i + 1 < argc) {
        char *saveptr;
        char *token = strtok_r(argv[++i], " ", &saveptr);
        while (token != nullptr && ignoredSourceOutputsCount < MAX_IGNORED_SOURCE_OUTPUTS) {
          ignoredSourceOutputs[ignoredSourceOutputsCount++] = token;
          token = strtok_r(nullptr, " ", &saveptr);
        }

        ignoredSourceOutputs[ignoredSourceOutputsCount] = nullptr;
      } else {
        showHelp(argv);
        return 0;
      }
    }
  }

  pa_subscription_mask_t all_mask =
      (pa_subscription_mask_t)(PA_SUBSCRIPTION_MASK_SINK_INPUT |
                               PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT);
  if (!printSink && !printSource && !printBoth && !printBothWayBar) {
    return Pulse().init(SUBSCRIPTION_TYPE_IDLE, all_mask, EVENT_TYPE_IDLE, ignoredSourceOutputs);
  } else if (printBoth) {
    return Pulse().init(SUBSCRIPTION_TYPE_DRY_BOTH, all_mask,
                        EVENT_TYPE_DRY_BOTH, ignoredSourceOutputs);
  } else if (printBothWayBar) {
    return Pulse().init(SUBSCRIPTION_TYPE_DRY_BOTH_WAYBAR, all_mask,
                        EVENT_TYPE_DRY_BOTH, ignoredSourceOutputs);
  } else if (printSink) {
    return Pulse().init(SUBSCRIPTION_TYPE_DRY_SINK, PA_SUBSCRIPTION_MASK_SINK_INPUT,
                        EVENT_TYPE_DRY_SINK, ignoredSourceOutputs);
  } else if (printSource) {
    return Pulse().init(SUBSCRIPTION_TYPE_DRY_SOURCE,
                        PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT, EVENT_TYPE_DRY_SOURCE, ignoredSourceOutputs);
  }
  return 0;
}
