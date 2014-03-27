#ifndef APTPRIVATE_PRIVATE_UPGRADE_H
#define APTPRIVATE_PRIVATE_UPGRADE_H

#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC bool DoDistUpgrade(CommandLine &CmdL);
APT_PUBLIC bool DoUpgrade(CommandLine &CmdL);
bool DoUpgradeNoNewPackages(CommandLine &CmdL);
bool DoUpgradeWithAllowNewPackages(CommandLine &CmdL);

#endif
