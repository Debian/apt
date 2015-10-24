#ifndef APT_PRIVATE_MAIN_H
#define APT_PRIVATE_MAIN_H

#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC void InitLocale();
APT_PUBLIC void InitSignals();
APT_PUBLIC void CheckIfSimulateMode(CommandLine &CmdL);
APT_PUBLIC void CheckIfCalledByScript(int argc, const char *argv[]);

#endif
