// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Upgrade - Upgrade/DistUpgrade related code

   ##################################################################### */
									/*}}}*/

#ifndef PKGLIB_UPGRADE_H
#define PKGLIB_UPGRADE_H

#include <apt-pkg/macros.h>
#include <cstddef>

class pkgDepCache;
class OpProgress;

namespace APT {
   namespace Upgrade {
      // FIXME: make this "enum class UpgradeMode {" once we enable c++11
      enum UpgradeMode {
         FORBID_REMOVE_PACKAGES = 1,
         FORBID_INSTALL_NEW_PACKAGES = 2,
	 ALLOW_EVERYTHING = 0
      };
      APT_PUBLIC bool Upgrade(pkgDepCache &Cache, int UpgradeMode, OpProgress * const Progress = NULL);
   }
}

APT_PUBLIC bool pkgMinimizeUpgrade(pkgDepCache &Cache);
#endif
