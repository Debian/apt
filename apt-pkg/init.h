// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: init.h,v 1.9.2.2 2004/01/02 18:51:00 mdz Exp $
/* ######################################################################

   Init - Initialize the package library

   This function must be called to configure the config class before
   calling many APT library functions.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_INIT_H
#define PKGLIB_INIT_H

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/configuration.h>
#include <apt-pkg/pkgsystem.h>
#endif

class pkgSystem;
class Configuration;

// These lines are extracted by the makefiles and the buildsystem
// Increasing MAJOR or MINOR results in the need of recompiling all
// reverse-dependencies of libapt-pkg against the new SONAME.
// Non-ABI-Breaks should only increase RELEASE number.
// See also buildlib/libversion.mak
#define APT_PKG_MAJOR 4
#define APT_PKG_MINOR 12
#define APT_PKG_RELEASE 0
    
extern const char *pkgVersion;
extern const char *pkgLibVersion;

bool pkgInitConfig(Configuration &Cnf);
bool pkgInitSystem(Configuration &Cnf,pkgSystem *&Sys);

#endif
