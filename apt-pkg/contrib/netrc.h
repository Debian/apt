// -*- mode: cpp; mode: fold -*-
// SPDX-License-Identifier: GPL-2.0+
// Description								/*{{{*/
/* ######################################################################

   netrc file parser - returns the login and password of a give host in
                       a specified netrc-type file

   This file had this historic note, but now includes further changes
   under the GPL-2.0+:

   Originally written by Daniel Stenberg, <daniel@haxx.se>, et al. and
   placed into the Public Domain, do with it what you will.

   ##################################################################### */
									/*}}}*/
#ifndef NETRC_H
#define NETRC_H

#include <string>

#include <apt-pkg/macros.h>



class URI;
class FileFd;

APT_PUBLIC bool MaybeAddAuth(FileFd &NetRCFile, URI &Uri);
#endif
