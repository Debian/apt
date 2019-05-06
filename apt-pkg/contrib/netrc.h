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

#ifndef APT_8_CLEANER_HEADERS
#include <apt-pkg/strutl.h>
#endif

#ifndef APT_15_CLEANER_HEADERS
#define DOT_CHAR "."
#define DIR_CHAR "/"
#endif

class URI;
class FileFd;

APT_DEPRECATED_MSG("Use FileFd-based MaybeAddAuth instead")
void maybe_add_auth(URI &Uri, std::string NetRCFile);
bool MaybeAddAuth(FileFd &NetRCFile, URI &Uri);
bool IsAuthorized(pkgCache::PkgFileIterator const I, std::vector<std::unique_ptr<FileFd>> &authconfs) APT_HIDDEN;
#endif
