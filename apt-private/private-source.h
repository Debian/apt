#ifndef APT_PRIVATE_SOURCE_H
#define APT_PRIVATE_SOURCE_H

#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC bool DoSource(CommandLine &CmdL);
APT_PUBLIC bool DoBuildDep(CommandLine &CmdL);

#endif
