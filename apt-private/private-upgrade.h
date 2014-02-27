#ifndef APTPRIVATE_PRIVATE_UPGRADE_H
#define APTPRIVATE_PRIVATE_UPGRADE_H

#include <apt-pkg/cmndline.h>


bool DoDistUpgrade(CommandLine &CmdL);
bool DoUpgrade(CommandLine &CmdL);
bool DoUpgradeNoNewPackages(CommandLine &CmdL);
bool DoUpgradeWithAllowNewPackages(CommandLine &CmdL);


#endif
