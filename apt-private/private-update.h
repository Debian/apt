#ifndef APT_PRIVATE_UPDATE_H
#define APT_PRIVATE_UPDATE_H

#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC bool DoUpdate(CommandLine &CmdL);

#endif
