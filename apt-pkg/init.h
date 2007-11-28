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

#include <apt-pkg/configuration.h>
#include <apt-pkg/pkgsystem.h>

// See the makefile
#define APT_PKG_MAJOR 4
#define APT_PKG_MINOR 6
#define APT_PKG_RELEASE 0
    
extern const char *pkgVersion;
extern const char *pkgLibVersion;

bool pkgInitConfig(Configuration &Cnf);
bool pkgInitSystem(Configuration &Cnf,pkgSystem *&Sys);

#ifdef APT_COMPATIBILITY
#if APT_COMPATIBILITY != 986
#warning "Using APT_COMPATIBILITY"
#endif

inline bool pkgInitialize(Configuration &Cnf) 
{
   return pkgInitConfig(Cnf) && pkgInitSystem(Cnf,_system);
};
#endif

#endif
