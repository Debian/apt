#ifndef APT_PRIVATE_SHOW_H
#define APT_PRIVATE_SHOW_H

#include <apt-pkg/macros.h>

class CommandLine;

namespace APT {
   namespace Cmd {

      APT_PUBLIC bool ShowPackage(CommandLine &CmdL);
   }
}
#endif
