// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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
#ifndef APT_10_CLEANER_HEADERS
#include <apt-pkg/macros.h>
#endif

class pkgSystem;
class Configuration;

extern const char *pkgVersion;
extern const char *pkgLibVersion;

bool pkgInitConfig(Configuration &Cnf);
bool pkgInitSystem(Configuration &Cnf,pkgSystem *&Sys);

#endif
