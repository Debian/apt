// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: init.h,v 1.2 1998/07/12 23:58:27 jgg Exp $
/* ######################################################################

   Init - Initialize the package library

   This function must be called to configure the config class before
   calling many APT library functions.
   
   ##################################################################### */
									/*}}}*/
// Header section: pkglib
#ifndef PKGLIB_INIT_H
#define PKGLIB_INIT_H

#include <apt-pkg/configuration.h>

bool pkglibInitialize(Configuration &Cnf);

#endif
