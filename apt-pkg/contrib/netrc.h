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

#include <memory>
#include <string>
#include <vector>

#include <apt-pkg/macros.h>
#include <apt-pkg/pkgcache.h>



class URI;
class FileFd;

bool MaybeAddAuth(FileFd &NetRCFile, URI &Uri);
bool IsAuthorized(pkgCache::PkgFileIterator const I, std::vector<std::unique_ptr<FileFd>> &authconfs) APT_HIDDEN;
#endif
