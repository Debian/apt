#ifndef APT_PRIVATE_CMNDLINE_H
#define APT_PRIVATE_CMNDLINE_H

#include <apt-pkg/cmndline.h>
#include <apt-pkg/macros.h>

#include <vector>

APT_PUBLIC std::vector<CommandLine::Args> getCommandArgs(char const * const Program, char const * const Cmd);

#endif
