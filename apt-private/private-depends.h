#ifndef APT_PRIVATE_DEPENDS_H
#define APT_PRIVATE_DEPENDS_H

#include <apt-pkg/macros.h>

class CommandLine;

APT_PUBLIC bool Depends(CommandLine &CmdL);
APT_PUBLIC bool RDepends(CommandLine &CmdL);

#endif
