#include <cstring>

#include "pulse.hpp"

void showHelp(char **argv) {
  cout << "Usage:" << endl;
  cout << "\t" << argv[0] << " <OPTION>" << endl;
  cout << "Options:" << endl;
  cout << "\t" << argv[0]
       << "\t\t\t\t Inhibits idle if either any sink or any source is running"
       << endl;
  cout << "\t" << argv[0] << " -h, --help \t\t Show help options" << endl;
  cout << "\t" << argv[0]
       << " --dry-print-both \t Don't inhibit idle and print if either any "
          "sink or any source is running"
       << endl;
  cout << "\t" << argv[0]
       << " --dry-print-sink \t Don't inhibit idle and print if any sink is "
          "running"
       << endl;
  cout << "\t" << argv[0]
       << " --dry-print-source \t Don't inhibit idle and print if any source "
          "is running"
       << endl;
}

int main(int argc, char *argv[]) {
  bool inhibitIdle = true;

  bool printBoth = false;
  bool printSource = false;
  bool printSink = false;

  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "--dry-print-source") == 0) {
        printSource = true;
      } else if (strcmp(argv[i], "--dry-print-sink") == 0) {
        printSink = true;
      } else if (strcmp(argv[i], "--dry-print-both") == 0) {
        printBoth = true;
      } else {
        showHelp(argv);
        return 0;
      }
    }
  }

  if (inhibitIdle && (!printSink && !printSource && !printBoth)) {
    return Pulse().init(SUBSCRIPTION_TYPE_IDLE, PA_SUBSCRIPTION_MASK_ALL,
                        EVENT_TYPE_IDLE);
  } else if (printBoth) {
    return Pulse().init(SUBSCRIPTION_TYPE_DRY_BOTH, PA_SUBSCRIPTION_MASK_ALL,
                        EVENT_TYPE_DRY_BOTH);
  } else if (printSink) {
    return Pulse().init(SUBSCRIPTION_TYPE_DRY_SINK, PA_SUBSCRIPTION_MASK_SINK,
                        EVENT_TYPE_DRY_SINK);
  } else if (printSource) {
    return Pulse().init(SUBSCRIPTION_TYPE_DRY_SOURCE,
                        PA_SUBSCRIPTION_MASK_SOURCE, EVENT_TYPE_DRY_SOURCE);
  }
  return 0;
}
