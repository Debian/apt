// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################

   Upgrade - Upgrade/DistUpgrade releated code
   
   ##################################################################### */
									/*}}}*/

#ifndef PKGLIB_UPGRADE_H
#define PKGLIB_UPGRADE_H

class pkgDepCache;

namespace APT {
   namespace Upgrade {
      // FIXME: make this "enum class UpgradeMode {" once we enable c++11
      enum UpgradeMode {
         FORBID_REMOVE_PACKAGES = 1,
         FORBID_INSTALL_NEW_PACKAGES = 2
      };
      bool Upgrade(pkgDepCache &Cache, int UpgradeMode);
   }
}

// please use APT::Upgrade::Upgrade() instead
bool pkgDistUpgrade(pkgDepCache &Cache);
bool pkgAllUpgrade(pkgDepCache &Cache);
bool pkgMinimizeUpgrade(pkgDepCache &Cache);


#endif
