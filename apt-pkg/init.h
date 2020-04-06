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

#include <apt-pkg/macros.h>

class pkgSystem;
class Configuration;

APT_PUBLIC extern const char *pkgVersion;
APT_PUBLIC extern const char *pkgLibVersion;

APT_PUBLIC bool pkgInitConfig(Configuration &Cnf);
APT_PUBLIC bool pkgInitSystem(Configuration &Cnf,pkgSystem *&Sys);

#endif
