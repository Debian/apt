// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: netrc.h,v 1.11 2004/01/07 09:19:35 bagder Exp $
/* ######################################################################

   netrc file parser - returns the login and password of a give host in
                       a specified netrc-type file

   Originally written by Daniel Stenberg, <daniel@haxx.se>, et al. and
   placed into the Public Domain, do with it what you will.

   ##################################################################### */
									/*}}}*/
#ifndef NETRC_H
#define NETRC_H

#include <string>

#include <apt-pkg/macros.h>

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/strutl.h>
#endif

#define DOT_CHAR "."
#define DIR_CHAR "/"

class URI;

void maybe_add_auth (URI &Uri, std::string NetRCFile);
#endif
