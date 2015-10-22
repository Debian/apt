#ifndef APT_PRIVATE_CMNDLINE_H
#define APT_PRIVATE_CMNDLINE_H

#include <apt-pkg/cmndline.h>
#include <apt-pkg/macros.h>

#include <vector>

class Configuration;
class pkgSystem;

APT_PUBLIC std::vector<CommandLine::Args> getCommandArgs(char const * const Program, char const * const Cmd);
APT_PUBLIC void ParseCommandLine(CommandLine &CmdL, CommandLine::DispatchWithHelp const * Cmds, CommandLine::Args * const Args,
      Configuration * const * const Cnf, pkgSystem ** const Sys, int const argc, const char * argv[],
      bool(*ShowHelp)(CommandLine &, CommandLine::DispatchWithHelp const *));

#endif
