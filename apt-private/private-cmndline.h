#ifndef APT_PRIVATE_CMNDLINE_H
#define APT_PRIVATE_CMNDLINE_H

#include <apt-pkg/cmndline.h>
#include <apt-pkg/macros.h>

#include <vector>

class Configuration;
class pkgSystem;

enum class APT_CMD {
   APT,
   APT_GET,
   APT_CACHE,
   APT_CDROM,
   APT_CONFIG,
   APT_EXTRACTTEMPLATES,
   APT_FTPARCHIVE,
   APT_HELPER,
   APT_INTERNAL_SOLVER,
   APT_MARK,
   APT_SORTPKG,
};

bool ShowHelp(CommandLine &CmdL, CommandLine::DispatchWithHelp const * Cmds);
std::vector<CommandLine::DispatchWithHelp> GetCommands();

APT_PUBLIC std::vector<CommandLine::DispatchWithHelp> ParseCommandLine(CommandLine &CmdL, APT_CMD const Binary,
      Configuration * const * const Cnf, pkgSystem ** const Sys, int const argc, const char * argv[]);
APT_PUBLIC unsigned short DispatchCommandLine(CommandLine &CmdL, std::vector<CommandLine::DispatchWithHelp> const &Cmds);

APT_PUBLIC std::vector<CommandLine::Args> getCommandArgs(APT_CMD const Program, char const * const Cmd);

#endif
