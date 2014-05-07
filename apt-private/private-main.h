#ifndef APT_PRIVATE_MAIN_H
#define APT_PRIVATE_MAIN_H

#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC void CheckSimulateMode(CommandLine &CmdL);
APT_PUBLIC void InitSignals();

#endif
