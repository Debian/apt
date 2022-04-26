// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/* ######################################################################
   
   Proxy - Proxy operations
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_PROXY_H
#define PKGLIB_PROXY_H

#include <apt-pkg/macros.h>

class URI;
APT_PUBLIC bool AutoDetectProxy(URI &URL);
APT_HIDDEN bool CanURIBeAccessedViaProxy(URI const &URL);

#endif
