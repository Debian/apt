// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
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



class URI;
class FileFd;

APT_PUBLIC bool MaybeAddAuth(FileFd &NetRCFile, URI &Uri);
#endif
